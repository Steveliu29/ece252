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

/**
 * @file main_2proc.c
 * @brief Two processes system. The child process uses cURL to download data to
 *        a shared memory region through cURL call back.
 *        The parent process wait till the child to finish and then
 *        read the data from the shared memory region and output it to a file.
 *        cURL header call back extracts data sequence number from header.
 *        Synchronization is done through waitpid, no semaphores are used.
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
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <semaphore.h>

#define IMG_URL_FIRST "http://ece252-"
#define IMG_URL_SECOND ".uwaterloo.ca:2520/image?img="
#define IMG_URL_THIRD "&part="
#define IMG_URL "http://ece252-1.uwaterloo.ca:2530/image?img=1&part=20"
#define DUM_URL "https://example.com/"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define IMG_SIZE 10000  /* as per lab specification */

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
    sem_t mutex;
    sem_t space;
    sem_t items;
    int produce_counter;
    int consume_counter;
} CTRL_BLK;

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
int write_file(const char *path, const void *in, size_t len);
int shm_recv_buf_init(RECV_BUF *ptr, int buffer_size, size_t nbytes);
void set_URL(char** url, int img_num, int server_num, int part_num);

/**
 * @brief  cURL header call back function to extract image sequence number from
 *         http header data. An example header for image part n (assume n = 2) is:
 *         X-Ece252-Fragment: 2
 * @param  char *p_recv: header data delivered by cURL
 * @param  size_t size size of each memb
 * @param  size_t nmemb number of memb
 * @param  void *userdata user defined data structurea
 * @return size of header data received.
 * @details this routine will be invoked multiple times by the libcurl until the full
 * header data are received.  we are only interested in the ECE252_HEADER line
 * received so that we can extract the image sequence number from it. This
 * explains the if block in the code.
 */
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


/**
 * @brief write callback function to save a copy of received data in RAM.
 *        The received libcurl data are pointed by p_recv,
 *        which is provided by libcurl and is not user allocated memory.
 *        The user allocated memory is at p_userdata. One needs to
 *        cast it to the proper struct to make good use of it.
 *        This function maybe invoked more than once by one invokation of
 *        curl_easy_perform().
 */

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

/**
 * @brief calculate the actual size of RECV_BUF
 * @param size_t nbytes number of bytes that buf in RECV_BUF struct would hold
 * @return the REDV_BUF member fileds size plus the RECV_BUF buf data size
 */
int sizeof_shm_recv_buf(size_t nbytes)
{
    return (sizeof(RECV_BUF) + sizeof(char) * nbytes);
}

/**
 * @brief initialize the RECV_BUF structure.
 * @param RECV_BUF *ptr memory allocated by user to hold RECV_BUF struct
 * @param size_t nbytes the RECV_BUF buf data size in bytes
 * NOTE: caller should call sizeof_shm_recv_buf first and then allocate memory.
 *       caller is also responsible for releasing the memory.
 */

int shm_recv_buf_init(RECV_BUF *ptr, int buffer_size, size_t nbytes)
{
    if ( ptr == NULL ) {
        return 1;
    }

    for (int i = 0; i < buffer_size; i++){
        ptr[i].buf = (char *) &ptr[i] + sizeof(RECV_BUF);
        ptr[i].size = 0;
        ptr[i].max_size = nbytes;
        ptr[i].seq = -1;              /* valid seq should be non-negative */
    }

    return 0;
}


/**
 * @brief output data in memory to a file
 * @param path const char *, output file path
 * @param in  void *, input data to be written to the file
 * @param len size_t, length of the input data in bytes
 */

int write_file(const char *path, const void *in, size_t len)
{
    FILE *fp = NULL;

    if (path == NULL) {
        fprintf(stderr, "write_file: file name is null!\n");
        return -1;
    }

    if (in == NULL) {
        fprintf(stderr, "write_file: input data is null!\n");
        return -1;
    }

    fp = fopen(path, "wb");
    if (fp == NULL) {
        perror("fopen");
        return -2;
    }

    if (fwrite(in, 1, len, fp) != len) {
        fprintf(stderr, "write_file: imcomplete write!\n");
        return -3;
    }
    return fclose(fp);
}

void set_URL(char** url, int img_num, int server_num, int part_num){
    memset(*url, 0, 256);
    strcpy(*url, IMG_URL_FIRST);
    int server_pos = strlen(IMG_URL_FIRST);
    *url[server_pos] = server_num + '0';
    strcat(*url, IMG_URL_SECOND);
    int img_pos = strlen(IMG_URL_FIRST) + strlen(IMG_URL_SECOND) + 1;
    *url[img_pos] = img_num + '0';
    strcat(*url, IMG_URL_THIRD);
    int part_pos = strlen(IMG_URL_FIRST) + strlen(IMG_URL_SECOND) + strlen(IMG_URL_THIRD) + 2;
    int num2 = part_num % 10;
    part_num = part_num / 10;
    int num1 = part_num % 10;
    *url[part_pos] = num1 + '0';
    part_pos++;
    *url[part_pos] = num2 + '0';
}

int shm_CTRL_BLK_init(CTRL_BLK* ptr, int buffer_size)
{
    if ( ptr == NULL ) {
        return 1;
    }

    sem_init (&(ptr -> mutex), 0, 1);
    sem_init (&(ptr -> space), 0, buffer_size);
    sem_init (&(ptr -> items), 0, 0);
    ptr -> produce_counter = 0;
    ptr -> consume_counter = 0;
    }

    return 0;
}

void produce(char** url, RECV_BUF *p_shm_recv_buf, CTRL_BLK *p_control ,int img_num){
    RECV_BUF temp;
    int part_num;
    int server_num;

    sem_wait(&(p_control -> mutex));
    part_num = p_control -> produce_counter;
    p_control -> produce_counter = p_control -> produce_counter + 1;
    sem_post(&(p_control -> mutex));
    server_num = part_num % 3 + 1;

    while (part_num <= 50){

        set_URL(url, img_num, server_num, part_num);

        curl_global_init(CURL_GLOBAL_DEFAULT);

        /* init a curl session */
        curl_handle = curl_easy_init();

        if (curl_handle == NULL) {
            fprintf(stderr, "curl_easy_init: returned NULL\n");
            return 1;
        }

        /* specify URL to get */
        curl_easy_setopt(curl_handle, CURLOPT_URL, url);

        /* register write call back function to process received data */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl);
        /* user defined data structure passed to the call back function */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) &temp);

        /* register header call back function to process received header data */
        curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);
        /* user defined data structure passed to the call back function */
        curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *) &temp);

        /* some servers requires a user-agent field */
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        /* get it! */
        res = curl_easy_perform(curl_handle);


        if( res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            // printf("%lu bytes received in memory %p, seq=%d.\n",  \
            //       p_shm_recv_buf->size, p_shm_recv_buf->buf, p_shm_recv_buf->seq);
            sem_wait(&(p_control -> space));
            sem_wait(&(p_control -> mutex));
            enqeue(temp);
            sem_post(&(p_control -> mutex));
            sem_post(&(p_control -> items));
        }

        /* cleaning up */
        curl_easy_cleanup(curl_handle);
        curl_global_cleanup();

        /* getting another piece */
        sem_wait(&(p_control -> mutex));
        part_num = p_control -> produce_counter;
        p_control -> produce_counter = p_control -> produce_counter + 1;
        sem_post(&(p_control -> mutex));
        server_num = part_num % 3;
    }
}

int main( int argc, char** argv )
{
    CURL *curl_handle;
    CURLcode res;
    char url[256];
    RECV_BUF *p_shm_recv_buf;
    char fname[256];
    pid_t pid =getpid();
    pid_t cpid = 0;

    int buffer_size;
    int num_producer;
    int num_consumer;
    int wait_time;
    int img_num;
    CTRL_BLK *p_control;
    RECV_BUF *p_shm_output_buf;

    if (argc != 6) {
        printf("Usage: ./paster2 <B> <P> <C> <X> <N>\n");
        return 1;
    } else {
        buffer_size   = atoi(argv[1]);
        num_producer  = atoi(argv[2]);
        num_consumer  = atoi(argv[3]);
        wait_time     = atoi(argv[4]);
        img_num       = atoi(argv[5]);
    }

    int shmid;
    int shm_size = buffer_size * sizeof_shm_recv_buf(IMG_SIZE);
    printf("shm_size = %d.\n", shm_size);
    shmid = shmget(IPC_PRIVATE, shm_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if ( shmid == -1 ) {
        perror("shmget");
        abort();
    }

    p_shm_recv_buf = shmat(shmid, NULL, 0);
    shm_recv_buf_init(p_shm_recv_buf, buffer_size, IMG_SIZE);

  //  printf("%s: URL is %s\n", argv[0], url);

    int CTRL_shmid;
    int CTRL_shm_size = buffer_size * sizeof(CTRL_BLK);
    printf("CTRL_shm_size = %d.\n", CTRL_shm_size);
    CTRL_shmid = shmget(IPC_PRIVATE, CTRL_shm_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if ( CTRL_shmid == -1 ) {
        perror("shmget");
        abort();
    }

    p_control = shmat(CTRL_shmid, NULL, 0);
    shm_CTRL_BLK_init(p_control, buffer_size);

    int output_shmid;
    int output_shm_size = 50 * sizeof_shm_recv_buf(IMG_SIZE);
    printf("output_shm_size = %d.\n", output_shm_size);
    output_shmid = shmget(IPC_PRIVATE, output_shm_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if ( output_shmid == -1 ) {
        perror("shmget");
        abort();
    }

    p_shm_output_buf = shmat(output_shmid, NULL, 0);
    shm_recv_buf_init(p_shm_output_buf, 50, IMG_SIZE);

    for (int i = 0; i < num_producer; i++)
        cpid = fork();

    if ( cpid == 0 ) {          /* child proc download */
        produce(&url, p_shm_recv_buf, p_control, img_num);
        shmdt(p_shm_recv_buf);
        shmdt(p_control);
        exit(0);
    } else if ( cpid > 0 ) {    /* parent proc */
        int state;
        waitpid(cpid, &state, 0);
        sprintf(fname, "./output_%d_%d.png", p_shm_recv_buf->seq, pid);
        write_file(fname, p_shm_recv_buf->buf, p_shm_recv_buf->size);
        shmdt(p_shm_recv_buf);
        shmctl(shmid, IPC_RMID, NULL);
    } else {
        perror("fork");
        abort();
    }


    return 0;
}
