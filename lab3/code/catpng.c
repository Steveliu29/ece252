#include <stdio.h>    /* for printf(), perror()...   */
#include <stdlib.h>   /* for malloc()                */
#include <errno.h>    /* for errno                   */
#include "crc.c"      /* for crc()                   */
#include "zutil.c"

int cat_png(RECV_BUF* buf_arr, int arr_size);
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
    for (int i = 0; i < arr_size; i++){
        data_IHDR_p temp = malloc(DATA_IHDR_SIZE * sizeof(U8));
        memset(temp, 0, DATA_IHDR_SIZE);

        get_png_data_IHDR(temp, *buf_arr[i]);

        output_IHDR -> width = temp -> width; /*width does not get added since the width will stay the same*/
        output_IHDR -> height = output_IHDR -> height + (temp -> height);
        output_IHDR -> bit_depth = temp -> bit_depth;
        output_IHDR -> color_type = temp -> color_type;
        output_IHDR -> compression = temp -> compression;
        output_IHDR -> filter = temp -> filter;
        output_IHDR -> interlace = temp -> interlace;

        free(temp);
    }
    add_IHDR_chunk(new_fp, output_IHDR);

    chunk_p sum_IDAT = malloc (sizeof (struct chunk));
    U8* inflated_buffer = malloc ( (output_IHDR -> height) * ((output_IHDR -> width) * 4 + 1) );
    long total_read = 0;
    U64 len_def = 0;      /* compressed data length                        */
    U64 len_inf = 0;      /* uncompressed data length                      */

    sum_IDAT -> length = 0;
//    sum_IDAT -> type[0] = 0x49;
//    sum_IDAT -> type[1] = 0x44;
//    sum_IDAT -> type[2] = 0x41;
//    sum_IDAT -> type[3] = 0x54;
    sum_IDAT -> p_data = NULL;
    sum_IDAT -> crc = 0;
    for (int i = 0; i < arr_size; i++){
        U32 temp_length = 0;
        U32 temp_type = 0;

        long pos = 33;

        memcpy(&(temp_length),(*buf_arr[i].buf + pos), 4);

        pos = pos + 4;

        // fseek(fp, 33, SEEK_SET);
        // fread(&temp_length, 1, 4, fp);
        //printf("%d\n",temp_length);
        temp_length = ntohl(temp_length);
        sum_IDAT -> length = sum_IDAT -> length + temp_length;

        memcpy(&(temp_type),(*buf_arr[i].buf + pos), 4);

        pos = pos + 4;

        //fread(&temp_type, 1, 4, fp);

        U8* temp_data = malloc ( temp_length * sizeof(U8) );

        memcpy(temp_data,(*buf_arr[i].buf + pos), temp_length);

        pos = pos + temp_length;
            // printf("%d\n",pos);
            // printf("%d\n",c);

      //  fread(temp_data, 1, temp_length, fp);

        int ret = mem_inf(inflated_buffer + total_read, &len_inf, temp_data, temp_length);
//        if (ret != 0) {
//            /* failure */
//            fprintf(stderr,"mem_def failed. ret = %d.\n", ret);
//            return ret;
//        }
        total_read = total_read + len_inf;

        free(temp_data);
    //    fclose(fp);
    }
    sum_IDAT -> p_data  = malloc(total_read);

    int ret_def = mem_def(sum_IDAT -> p_data, &len_def, inflated_buffer, total_read, Z_DEFAULT_COMPRESSION);
//    if (ret_def != 0) {
//        /* failure */
//        fprintf(stderr,"mem_def failed. ret = %d.\n", ret_def);
//        return ret_def;
//    }

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
