#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "helper.c"
#include "lab_png.c"
#include "catpng.c"


int main(int argc, char **argv);
void* get_single_frag(void *argument);
void get_all_frags(RECV_BUF* buf_arr, int* png_left, int threads_num, int img_num);


typedef struct server_img_num{
    int server_num;
    int img_num;
} frag_info;


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

    get_all_frags(buf_arr, &png_left, t, n);

    cat_png(buf_arr,50);

    for (int i = 0; i < 50; i++)
    {
        recv_buf_cleanup(&buf_arr[i]);
    }

    return 0;
}

void* get_single_frag(void *argument)
{
    frag_info *fragInfo = (frag_info *) argument;

    RECV_BUF *recvBuf = malloc(sizeof(RECV_BUF));
    recvBuf->buf = NULL;
    recvBuf->size = 0;
    recvBuf->max_size = 0;
    recvBuf->seq = -1;

    get_png_frag(fragInfo->server_num, fragInfo->img_num, recvBuf);

    free(fragInfo);

    return recvBuf;
}

void get_all_frags(RECV_BUF* buf_arr, int* png_left, int threads_num, int img_num)
{
    pthread_t *tid;

    while (*png_left != 0)
    {
        tid = malloc(threads_num * sizeof(pthread_t));
        memset(tid, 0, threads_num);

        for (int i = 1; i <= threads_num; ++i)
        {
            int arr_index = i - 1;
            int server_num = i % 3;
            if (server_num == 0)
            {
                server_num = 3;
            }

            frag_info *fragInfo = malloc(sizeof(frag_info));
            fragInfo->server_num = server_num;
            fragInfo->img_num = img_num;

            pthread_create(&(tid[arr_index]), NULL, get_single_frag, fragInfo);
        }

        for (int i = 0; i < threads_num; ++i)
        {
            void *variable;
            pthread_join(tid[i], &variable);
            RECV_BUF *recvBuf = (RECV_BUF *) variable;

            int seq = recvBuf->seq;
            if (buf_arr[seq].buf == NULL)
            {
                buf_arr[seq].buf = recvBuf->buf;
                buf_arr[seq].size = recvBuf->size;
                buf_arr[seq].max_size = recvBuf->max_size;
                buf_arr[seq].seq = recvBuf->seq;

                printf("png left: %d\n",*png_left);
                *png_left = *png_left - 1;

                free(variable);
            }
            else{
                recv_buf_cleanup(variable);
                free(variable);
            }
        }

        free(tid);
    }
}