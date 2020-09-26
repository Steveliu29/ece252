#include <stdio.h>    /* for printf(), perror()...   */
#include <stdlib.h>   /* for malloc()                */
#include "crc.c"      /* for crc()                   */
#include "zutil.c"
#include "lab_png.c"  /* simple PNG data structures  */

int main(int argc, char **argv);
void add_png_header(int fd);
int add_IHDR_chunk(int fd, struct data_IHDR *in);

int main(int argc, char **argv){
    if (argc == 1){
        printf("Usage: catpng ./img1.png ./img2.png ...\n");
        return 1;
    }

    FILE* new_fp = fopen("all.png", "wb+");
    if (new_fp == NULL){
        printf("Unable to create all.png");
        return 1;
    }

    int new_fd = fileno( new_fp );
    add_png_header(new_fd);

    data_IHDR_p output_IHDR = malloc(DATA_IHDR_SIZE * sizeof(U8));

    for (int i = 1; i < argc; i++){
        data_IHDR_p temp = malloc(DATA_IHDR_SIZE * sizeof(U8));

        char* file_name = argv[i];

        FILE* fp = fopen(file_name, "r");
        if (fp == NULL){
            printf("Cannot open %s.", file_name);
            fclose(new_fp);
            return 1;
        }

        long current_pos = ftell(fp);
        if(get_png_data_IHDR(temp, fp, current_pos) == 1){
            printf("Cannot read the data from IHDR chunk.");
            return 1;
        }

        output_IHDR -> width = temp -> width; /*width does not get added since the width will stay the same*/
        output_IHDR -> height = output_IHDR -> height + (temp -> height);
        output_IHDR -> bit_depth = temp -> bit_depth;
        output_IHDR -> color_type = temp -> color_type;
        output_IHDR -> compression = temp -> compression;
        output_IHDR -> filter = temp -> filter;
        output_IHDR -> interlace = temp -> interlace;

        fclose(fp);
        free(temp);
    }

    add_IHDR_chunk(new_fd, output_IHDR);


    
    // fseek(new_fp, 0, SEEK_SET);
    // U8 *buf = malloc ( 12 * sizeof(U8) );
    // read(new_fd, buf, 12);
    //
    // for (int i = 0; i < 12; i++){
    //     printf("%x\n", buf[i]);
    // }








    // for (int i = 1; i < argc; i++){
    //
    //     char* file_name = argv[i];
    //
    //     FILE* fp = fopen(file_name, "r");
    //     if (fp == NULL){
    //         printf("Cannot open %s.", file_name);
    //         fclose(new_fp);
    //         return 1;
    //     }
    //
    //
    // }

    fclose (new_fp);
    return 0;
}

void add_png_header(int fd){
    U8 *png_header = malloc ( 8 * sizeof(U8) ); /*allocate 8 bytes to check*/

    png_header[0] = 0x89;
    png_header[1] = 0x50;
    png_header[2] = 0x4E;
    png_header[3] = 0x47;
    png_header[4] = 0x0D;
    png_header[5] = 0x0A;
    png_header[6] = 0x1A;
    png_header[7] = 0x0A;

    write(fd, png_header, 8);
    free (png_header);
}

int add_IHDR_chunk(int fd, struct data_IHDR *in){
    U8 *length_buf = malloc ( 4 * sizeof(U8) );

    length_buf[0] = 0x00;
    length_buf[1] = 0x00;
    length_buf[2] = 0x00;
    length_buf[3] = 0x0d;

    write(fd, length_buf, 4);

    U32 net_width = htonl(in -> width);
    U32 net_height = htonl(in -> height);

    // write(fd, &net_width, 4);
    // write(fd, &net_height, 4);
    // write(fd, &(in -> bit_depth), 1);
    // write(fd, &(in -> color_type), 1);
    // write(fd, &(in -> compression), 1);
    // write(fd, &(in -> filter), 1);
    // write(fd, &(in -> interlace), 1);

    U8 *type_and_data_buf = malloc ( 17 * sizeof(U8) );

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
    write(fd, type_and_data_buf, 17);

    int computed_crc = crc(type_and_data_buf, 17);
    // printf("%d\n", computed_crc);

    computed_crc = htonl (computed_crc);

    write(fd, &computed_crc, 4);

    free (length_buf);
    free (type_and_data_buf);

    return 0;
}
