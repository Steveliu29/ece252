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

    for (int i = 0; i < 8; i++){
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

// int get_png_data_IHDR(struct data_IHDR *out, RECV_BUF buf_read){
//     long pos = 16;
//
//     memcpy(&(out->width),(buf_read.buf + pos), 4);
//
//     pos = pos + 4;
//     memcpy(&(out->height),(buf_read.buf + pos), 4);
//
//     pos = pos + 4;
//
//
//     out->bit_depth = buf_read.buf[pos];
//     pos++;
//
//     out->color_type = buf_read.buf[pos];
//     pos++;
//
//     out->compression = buf_read.buf[pos];
//     pos++;
//
//     out->filter = buf_read.buf[pos];
//     pos++;
//
//     out->interlace = buf_read.buf[pos];
//     pos++;
//
//
//
//     out -> width = ntohl(out -> width);
//     out -> height = ntohl(out -> height);
//
//     return 0;
// }
