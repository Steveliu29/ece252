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
        printf("Target file doesn't exist.");
        return 1;
    }

    int fd = fileno( fp );
    U8 buf[8];
    read(fd,buf,8);

    if (is_png(buf) == 1)
        printf("%s: Not a PNG file\n",file_name);



    return 0;
}
