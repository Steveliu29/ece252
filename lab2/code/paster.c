#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "helper.c"
#include "lab_png.c"

int main(int argc, char **argv);
void get_all_frags(RECV_BUF* buf_arr, int* png_left, int img_num);

int main(int argc, char **argv)
{
    /*Copied from starter file: main_getopt.c*/
    int c;
    int t = 1;
    int n = 1;
    char *str = "option requires an argument";

    while ((c = getopt (argc, argv, "t:n:")) != -1) {
        switch (c) {
        case 't':
	    t = strtoul(optarg, NULL, 10);
	    /* printf("option -t specifies a value of %d.\n", t); */
	    if (t <= 0) {
                fprintf(stderr, "%s: %s > 0 -- 't'\n", argv[0], str);
                return -1;
            }
            break;
        case 'n':
            n = strtoul(optarg, NULL, 10);
	   /* printf("option -n specifies a value of %d.\n", n); */
            if (n <= 0 || n > 3) {
                fprintf(stderr, "%s: %s 1, 2, or 3 -- 'n'\n", argv[0], str);
                return -1;
            }
            break;
        default:
            return -1;
        }
    }

    /*end of the starter file*/

    RECV_BUF buf_arr[50];

    for (int i = 0; i < 50; i++){
        buf_arr[i].buf = NULL;
        buf_arr[i].size = 0;
        buf_arr[i].max_size = 0;
        buf_arr[i].seq = -1;
    }

    int png_left = 50;

    get_all_frags(buf_arr, &png_left, n);



    /*Add changes here*/



    for (int i = 0; i < 50; i++)
         recv_buf_cleanup(&buf_arr[i]);

    return 0;

}

void get_all_frags(RECV_BUF* buf_arr, int* png_left, int img_num){
    while(*png_left != 0){
        RECV_BUF temp;
        if ( get_png_frag(img_num, &temp) == 0 ){
            if (buf_arr[temp.seq].buf == NULL){
                buf_arr[temp.seq].buf = temp.buf;
                buf_arr[temp.seq].size = temp.size;
                buf_arr[temp.seq].max_size = temp.max_size;
                buf_arr[temp.seq].seq = temp.seq;

                printf("png left: %d\n",*png_left);
                *png_left = *png_left - 1;
            }
            else{
                recv_buf_cleanup(&temp);
            }
        }
    }
}
