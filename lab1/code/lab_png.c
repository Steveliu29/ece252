#include "lab_png.h"

int is_png(U8 *buf){

    U8 *correct_png = malloc ( 8 * sizeof(U8) ); /*allocate 8 bytes to check*/

    correct_png[0] = 0x89;
    correct_png[1] = 0x50;
    correct_png[2] = 0x4E;
    correct_png[3] = 0x47;
    correct_png[4] = 0x0D;
    correct_png[5] = 0x0A;
    correct_png[6] = 0x1A;
    correct_png[7] = 0x0A;

    for (int i = 0; i < n; i++){
        if (buf[i] != correct_png[i]){
            /*wrong header for png*/
            free (correct_png);
            return 1;
        }
    }

    free (correct_png);
    return 0;
}

int get_png_height(struct data_IHDR *buf){
    return buf -> width;
}

int get_png_width(struct data_IHDR *buf){
    return buf -> height;
}
int get_png_data_IHDR(struct data_IHDR *out, FILE *fp, long offset, int whence){
    fseek(fp, offset, whence);

    int fd = fileno( fp );


    if (read(fd, &(out->width), 4) != 4)
        return 1;

    if (read(fd, &(out->height), 4 != 4)
        return 1;

    if (read(fd, &(out->bit_depth), 1) != 1)
        return 1;

    if (read(fd, &(out->color_type), 1) != 1)
        return 1;

    if (read(fd, &(out->compression), 1) != 1)
        return 1;

    if (read(fd, &(out->filter), 1) != 1)
        return 1;

    if (read(fd, &(out->interlace), 1) != 1)
        return 1;

    return 0;

}
