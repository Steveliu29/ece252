#include <stdio.h>    /* for printf(), perror()...   */
#include <stdlib.h>   /* for malloc()                */
#include <errno.h>    /* for errno                   */
#include "lab_png.c"
#include "crc.c"      /* for crc()                   */
#include "zutil.c"

int cat_png(RECV_BUF** buf_arr, int arr_size);
void add_png_header(FILE *fp);
void add_IHDR_chunk(FILE *fp, struct data_IHDR *in);
void add_IDAT_chunk(FILE *fp, struct chunk* in);
void add_IEND_chunk(FILE *fp);

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
      //  printf("size: %d\n", buf_arr[i] -> size);

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

    // for (int i = 0; i < 17; i++)
    // printf("%x\n", type_and_data_buf[i]);
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

    // printf("%x\n", computed_crc);

    fwrite(&computed_crc, 1, 4, fp);
}
