/*
 * The code is derived from cURL example and paster.c base code.
 * The cURL example is at URL:
 * https://curl.haxx.se/libcurl/c/getinmemory.html
 * Copyright (C) 1998 - 2018, Daniel Stenberg, <daniel@haxx.se>, et al..
 *
 * The paster.c code is
 * Copyright 2013 Patrick Lam, <p23lam@uwaterloo.ca>.
 *
 * Modifications to the code are
 * Copyright 2018-2019, Yiqing Huang, <yqhuang@uwaterloo.ca>.
 *
 * This software may be freely redistributed under the terms of the X11 license.
 */

/*Copied from the starter code*/

/**
 * @file main_wirte_read_cb.c
 * @brief cURL write call back to save received data in a user defined memory first
 *        and then write the data to a file for verification purpose.
 *        cURL header call back extracts data sequence number from header.
 * @see https://curl.haxx.se/libcurl/c/getinmemory.html
 * @see https://curl.haxx.se/libcurl/using/
 * @see https://ec.haxx.se/callback-write.html
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include "crc.c"      /* for crc()                   */
#include "zutil.c"
#include "lab_png.c"

#define IMG_URL_FIRST "http://ece252-"
#define IMG_URL_SECOND ".uwaterloo.ca:2530/image?img="
#define IMG_URL_THIRD "&part="
#define IMG_URL "http://ece252-1.uwaterloo.ca:2530/image?img=1&part=20"
#define DUM_URL "https://example.com/"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define IMG_SIZE 10240  /* as per lab specification */

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

/* This is a flattened structure, buf points to
  the memory address immediately after
  the last member field (i.e. seq) in the structure.
  Here is the memory layout.
  Note that the memory is a chunk of continuous bytes.

  On a 64-bit machine, the memory layout is as follows:
  +================+
  | buf            | 8 bytes
  +----------------+
  | size           | 8 bytes
  +----------------+
  | max_size       | 8 bytes
  +----------------+
  | seq            | 4 bytes
  +----------------+
  | padding        | 4 bytes
  +----------------+
  | buf[0]         | 1 byte
  +----------------+
  | buf[1]         | 1 byte
  +----------------+
  | ...            | 1 byte
  +----------------+
  | buf[max_size-1]| 1 byte
  +================+
*/
typedef struct recv_buf_flat {
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                          /* <0 indicates an invalid seq number */
} RECV_BUF;

typedef struct control {
    int produce_counter;
    int consume_counter;
    int front;
    int rear;
    int queue_size;
} CTRL_BLK;


size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int shm_recv_buf_init(RECV_BUF *ptr, size_t nbytes);
int shm_CTRL_BLK_init(CTRL_BLK* ptr, int buffer_size);
void set_URL(char* url, int img_num, int server_num, int part_num);
void move_to_shm_buf(RECV_BUF* input_buf, RECV_BUF* output_buf);
void add_png_header(FILE *fp);
void add_IHDR_chunk(FILE *fp, struct data_IHDR *in);
void add_IDAT_chunk(FILE *fp, struct chunk* in);
void add_IEND_chunk(FILE *fp);


size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    RECV_BUF *p = userdata;

    if (realsize > strlen(ECE252_HEADER) &&
	strncmp(p_recv, ECE252_HEADER, strlen(ECE252_HEADER)) == 0) {

        /* extract img sequence number */
	p->seq = atoi(p_recv + strlen(ECE252_HEADER));

    }
    return realsize;
}

size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
     size_t realsize = size * nmemb;
     RECV_BUF *p = (RECV_BUF *)p_userdata;
     if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */
         fprintf(stderr, "User buffer is too small, abort...\n");
         abort();
     }

     memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
     p->size += realsize;
     p->buf[p->size] = 0;

     return realsize;
 }

int sizeof_shm_recv_buf(size_t nbytes)
{
     return (sizeof(RECV_BUF) + sizeof(char) * nbytes);
}

int shm_recv_buf_init(RECV_BUF *ptr, size_t nbytes)
{
    if ( ptr == NULL ) {
        return 1;
    }

    ptr->buf = (char *)ptr + sizeof(RECV_BUF);
    memset(ptr->buf, 0, nbytes);
    ptr->size = 0;
    ptr->max_size = nbytes;
    ptr->seq = -1;              /* valid seq should be non-negative */

    return 0;
}


int shm_CTRL_BLK_init(CTRL_BLK* ptr, int buffer_size)
{
    if ( ptr == NULL ) {
        return 1;
    }

    ptr -> produce_counter = 0;
    ptr -> consume_counter = 0;
    ptr -> front = -1;
    ptr -> rear = -1;
    ptr -> queue_size = buffer_size;

    return 0;
}


void set_URL(char* url, int img_num, int server_num, int part_num)
{
    memset(url, 0, 256);
    strcpy(url, IMG_URL_FIRST);
    int server_pos = strlen(IMG_URL_FIRST);
    url[server_pos] = server_num + '0';
    strcat(url, IMG_URL_SECOND);
    int img_pos = strlen(IMG_URL_FIRST) + strlen(IMG_URL_SECOND) + 1;
    url[img_pos] = img_num + '0';
    strcat(url, IMG_URL_THIRD);
    int part_pos = strlen(IMG_URL_FIRST) + strlen(IMG_URL_SECOND) + strlen(IMG_URL_THIRD) + 2;
    if (part_num >= 10){
        int num2 = part_num % 10;
        part_num = part_num / 10;
        int num1 = part_num % 10;
        url[part_pos] = num1 + '0';
        part_pos++;
        url[part_pos] = num2 + '0';
    } else{
        url[part_pos] = part_num + '0';
    }
}

void move_to_shm_buf(RECV_BUF* input, RECV_BUF* output){
    chunk_p sum_IDAT = malloc (sizeof (struct chunk));
    U64 len_inf = 0;      /* uncompressed data length                      */

    sum_IDAT -> length = 0;
    sum_IDAT -> p_data = NULL;
    sum_IDAT -> crc = 0;

    U32 temp_length = 0;
    U32 temp_type = 0;

    long pos = 33;

    memcpy(&(temp_length),(input -> buf + pos), 4);

    pos = pos + 4;

    temp_length = ntohl(temp_length);

    sum_IDAT -> length = sum_IDAT -> length + temp_length;
    memcpy(&(temp_type),(input -> buf + pos), 4);
    pos = pos + 4;

    U8* temp_data = malloc ( temp_length * sizeof(U8) );

    memcpy(temp_data,(input -> buf + pos), temp_length);

    pos = pos + temp_length;

    int ret = mem_inf(output -> buf, &len_inf, temp_data, temp_length);
    output -> size = len_inf;
    output -> seq = input -> seq;

    free(temp_data);

}

int cat_png(RECV_BUF** buf_arr, int arr_size){

    FILE* new_fp = fopen("all.png", "wb+");
    if (new_fp == NULL){
        printf("Unable to create all.png");
        return 1;
    }

    int new_fd = fileno( new_fp );

    /*initializing the header for the PNG*/
    add_png_header(new_fp);

    /*collecting all the IHDR chuncks*/
    data_IHDR_p output_IHDR = malloc(DATA_IHDR_SIZE * sizeof(U8));
    memset(output_IHDR, 0, DATA_IHDR_SIZE);

    output_IHDR -> width = 400; /*width does not get added since the width will stay the same*/
    output_IHDR -> height = 300;
    output_IHDR -> bit_depth = 8;
    output_IHDR -> color_type = 6;
    output_IHDR -> compression = 0;
    output_IHDR -> filter = 0;
    output_IHDR -> interlace = 0;

    add_IHDR_chunk(new_fp, output_IHDR);

    chunk_p sum_IDAT = malloc (sizeof (struct chunk));
    U8* inflated_buffer = malloc ( (output_IHDR -> height) * ((output_IHDR -> width) * 4 + 1) );
    long total_read = 0;
    U64 len_def = 0;      /* compressed data length                        */
    U64 len_inf = 0;      /* uncompressed data length                      */

    sum_IDAT -> length = 0;
    sum_IDAT -> p_data = NULL;
    sum_IDAT -> crc = 0;

    for (int i = 0; i < arr_size; i++){
        U32 temp_type = 0;

        sum_IDAT -> length = sum_IDAT -> length + buf_arr[i] -> size;

        memcpy(inflated_buffer + total_read, buf_arr[i] -> buf, buf_arr[i] -> size);

        total_read = total_read + buf_arr[i] -> size;
    }
    sum_IDAT -> p_data  = malloc(total_read);

    int ret_def = mem_def(sum_IDAT -> p_data, &len_def, inflated_buffer, total_read, Z_DEFAULT_COMPRESSION);

    sum_IDAT -> length = len_def;
    add_IDAT_chunk(new_fp, sum_IDAT);
    add_IEND_chunk(new_fp);

    free(output_IHDR);
    free(sum_IDAT->p_data);
    free(sum_IDAT);
    free(inflated_buffer);

    fclose (new_fp);
    return 0;
}

void add_png_header(FILE *fp){
    U8 png_header[8];

    png_header[0] = 0x89;
    png_header[1] = 0x50;
    png_header[2] = 0x4E;
    png_header[3] = 0x47;
    png_header[4] = 0x0D;
    png_header[5] = 0x0A;
    png_header[6] = 0x1A;
    png_header[7] = 0x0A;

    fwrite(png_header, 1, 8, fp);
}

void add_IHDR_chunk(FILE *fp, struct data_IHDR *in){
    U8 length_buf[4];

    length_buf[0] = 0x00;
    length_buf[1] = 0x00;
    length_buf[2] = 0x00;
    length_buf[3] = 0x0d;

    fwrite(length_buf, 1 , 4, fp);

    U32 net_width = htonl(in -> width);
    U32 net_height = htonl(in -> height);

    U8 type_and_data_buf[17];

    type_and_data_buf[0] = 0x49;
    type_and_data_buf[1] = 0x48;
    type_and_data_buf[2] = 0x44;
    type_and_data_buf[3] = 0x52;

    memcpy(type_and_data_buf + 4, &net_width, 4);
    memcpy(type_and_data_buf + 8, &net_height, 4);
    memcpy(type_and_data_buf + 12, &(in -> bit_depth), 1);
    memcpy(type_and_data_buf + 13, &(in -> color_type), 1);
    memcpy(type_and_data_buf + 14, &(in -> compression), 1);
    memcpy(type_and_data_buf + 15, &(in -> filter), 1);
    memcpy(type_and_data_buf + 16, &(in -> interlace), 1);

    fwrite(type_and_data_buf, 1, 17, fp);

    int computed_crc = crc(type_and_data_buf, 17);

    computed_crc = htonl (computed_crc);

    fwrite(&computed_crc, 1, 4, fp);
}

void add_IDAT_chunk(FILE *fp, struct chunk* in){


    U32 net_length = htonl(in -> length);
    fwrite(&net_length, 1, 4, fp);

    U8 type_buf[4];
    type_buf[0] = 0x49;
    type_buf[1] = 0x44;
    type_buf[2] = 0x41;
    type_buf[3] = 0x54;
    fwrite(type_buf, 1, 4, fp);

    fwrite(in->p_data, 1, in->length, fp);


    U8 *type_and_data_buf = malloc ( (in -> length + 4) * sizeof(U8) );
    memcpy(type_and_data_buf, type_buf, 4);
    memcpy(type_and_data_buf + 4, in -> p_data, in -> length);

    int computed_crc = crc(type_and_data_buf, (in -> length + 4));

    computed_crc = htonl (computed_crc);

    fwrite(&computed_crc, 1, 4, fp);

    free(type_and_data_buf);
}

void add_IEND_chunk(FILE *fp){
    U8 length_buf[4];

    length_buf[0] = 0x00;
    length_buf[1] = 0x00;
    length_buf[2] = 0x00;
    length_buf[3] = 0x00;

    fwrite(length_buf, 1, 4, fp);

    U8 type_buf[4];

    type_buf[0] = 0x49;
    type_buf[1] = 0x45;
    type_buf[2] = 0x4e;
    type_buf[3] = 0x44;

    fwrite(type_buf, 1, 4, fp);

    int computed_crc = crc(type_buf, 4);

    computed_crc = htonl (computed_crc);

    fwrite(&computed_crc, 1, 4, fp);
}
