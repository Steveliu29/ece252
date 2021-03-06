#include <stdio.h>    /* for printf(), perror()...   */
#include <stdlib.h>   /* for malloc()                */
#include "crc.c"      /* for crc()                   */
#include "zutil.c"
#include "lab_png.c"  /* simple PNG data structures  */

int main(int argc, char **argv);

int main(int argc, char **argv){
    if (argc != 2){
        printf("Usage: ./pnginfo filename\n");
        return 1;
    }

    char* file_name = argv[1];

    FILE* fp = fopen(file_name, "r");

    if (fp == NULL){
        printf("Target file doesn't exist.\n");
        return 1;
    }

    int fd = fileno( fp );

    /* check if the file is png */
    U8 buf[8];
    fread(buf, 1, 8, fp);

    if (is_png(buf) == 1){
        printf("%s: Not a PNG file\n",file_name);
        fclose(fp);
        return 0;
    }

    /* check if the file is corrupted */
    int is_corrupted = 0;
    U32 length = 0;
    U32 type_data_length = 0;
    U32 crc_val = 0;
    U32 computed_crc = 0;
    int i;

    for (i = 0; i < 3 && is_corrupted == 0; i++)
    {
        fread(&length, 1, 4, fp);
        length = ntohl(length);
        type_data_length = length + 4;

        U8* type_and_data = malloc ( (type_data_length) * sizeof(U8) );

        fread(type_and_data, 1, type_data_length, fp);

        fread(&crc_val, 1, 4, fp);
        crc_val = ntohl(crc_val);

        computed_crc = crc(type_and_data, type_data_length);

        if (crc_val != computed_crc)
            is_corrupted = 1;

        free(type_and_data);
    }

    /* get the dimension of the file */
    data_IHDR_p png_attributes = malloc(DATA_IHDR_SIZE * sizeof(U8));

    long current_pos = ftell(fp);

    if(get_png_data_IHDR(png_attributes, fp, current_pos) == 1){
        printf("Cannot read the data from IHDR chunk.\n");
        return 1;
    }

    /* print out the result */
    printf("%s: %d x %d\n", file_name, png_attributes -> width, png_attributes -> height);

    if (is_corrupted == 1){
        if (i == 1)
            printf("IHDR chunk CRC error: computed %x, expected %x\n", computed_crc, crc_val);
        else if (i == 2)
            printf("IDAT chunk CRC error: computed %x, expected %x\n", computed_crc, crc_val);
        else if (i == 3)
            printf("IEND chunk CRC error: computed %x, expected %x\n", computed_crc, crc_val);
    }
    free (png_attributes);
    fclose(fp);

    return 0;
}
