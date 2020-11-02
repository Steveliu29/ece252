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
#include <fcntl.h>
#include "helper.c"

#define SEM_ITEMS "/ece252_Charlie.myitems7"
#define SEM_SPACE "/ece252_Charlie.myspace7"
#define SEM_MUTEX "/ece252_Charlie.mymutex7"

// Global variables
sem_t* items;
sem_t* space;
sem_t* mutex;

void produce(RECV_BUF** buf_recv, CTRL_BLK *p_control, int img_num);
void consume(RECV_BUF **p_shm_recv_buf, RECV_BUF **p_shm_output_buf, CTRL_BLK *p_control, int wait_time);
int is_full(CTRL_BLK *p_control);
int is_empty(CTRL_BLK *p_control);
void enqueue(RECV_BUF **p_shm_recv_buf, RECV_BUF *temp, CTRL_BLK *p_control);
RECV_BUF* dequeue(RECV_BUF **p_shm_recv_buf, CTRL_BLK *p_control);
int main( int argc, char** argv );

int main( int argc, char** argv )
{

    int buffer_size = 50;
    int num_producer = 1;
    int num_consumer = 1;
    int wait_time = 1000;
    int img_num = 1;

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

    pid_t pid =getpid();
    pid_t* producer_cpid = malloc(num_producer * sizeof(pid_t));
    pid_t* consumer_cpid = malloc(num_consumer * sizeof(pid_t));
    for (int i = 0; i < num_producer; i++)
        producer_cpid[i] = 0;
    for (int i = 0; i < num_consumer; i++)
        consumer_cpid[i] = 0;


    items = sem_open(SEM_ITEMS, O_CREAT, O_RDWR, 0);
    space = sem_open(SEM_SPACE, O_CREAT, O_RDWR, buffer_size);
    mutex = sem_open(SEM_MUTEX, O_CREAT, O_RDWR, 1);

    if (items == SEM_FAILED || space == SEM_FAILED || mutex == SEM_FAILED){
        perror("sem_open");
        return 1;
    }

    RECV_BUF** p_shm_recv_buf = malloc (sizeof(RECV_BUF*) * buffer_size);
    int* shmid = malloc (sizeof(int) * buffer_size);
    memset(p_shm_recv_buf, 0, sizeof(RECV_BUF*) * buffer_size);
    memset(shmid, 0, sizeof(int) * buffer_size);
    int shm_size = sizeof_shm_recv_buf(IMG_SIZE);

    for (int i = 0; i < buffer_size; i++){
        shmid[i] = shmget(IPC_PRIVATE, shm_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
        if ( shmid[i] == -1 ) {
            perror("shmget");
            abort();
        }
        p_shm_recv_buf[i] = shmat(shmid[i], NULL, 0);
        shm_recv_buf_init(p_shm_recv_buf[i], IMG_SIZE);
    }


    RECV_BUF** p_shm_output_buf = malloc (sizeof(RECV_BUF*) * 50);
    int* shmid_out = malloc (sizeof(int) * 50);
    memset(p_shm_output_buf, 0, sizeof(RECV_BUF*) * 50);
    memset(shmid_out, 0, sizeof(int) * 50);
    int shm_size_out = sizeof_shm_recv_buf(IMG_SIZE);

    for (int i = 0; i < 50; i++){
        shmid_out[i] = shmget(IPC_PRIVATE, shm_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
        if ( shmid_out[i] == -1 ) {
            perror("shmget");
            abort();
        }
        p_shm_output_buf[i] = shmat(shmid_out[i], NULL, 0);
        shm_recv_buf_init(p_shm_output_buf[i], IMG_SIZE);
    }

    CTRL_BLK* p_control;
    int CTRL_shmid;
    int CTRL_shm_size = sizeof(CTRL_BLK);
    CTRL_shmid = shmget(IPC_PRIVATE, CTRL_shm_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if ( CTRL_shmid == -1 ) {
        perror("shmget");
        abort();
    }

    p_control = shmat(CTRL_shmid, NULL, 0);
    shm_CTRL_BLK_init(p_control, buffer_size);


    /*creating producers*/
    for (int i = 0; i < num_producer; i++){
        producer_cpid[i] = fork();

        if ( producer_cpid[i] == 0 ) {
            produce(p_shm_recv_buf, p_control, img_num);
            for (int a = 0; a < buffer_size; a++)
                shmdt(p_shm_recv_buf[a]);
            exit(0);
        } else if ( producer_cpid[i] < 0 ) {
            perror("fork");
            abort();
        }
    }

    // Start the timer
	double times[2];
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0) 
    {
        perror("gettimeofday");
        abort();
    }
    times[0] = tv.tv_sec + (tv.tv_usec / 1000000.0);

    /*creating consumers*/
    for (int i = 0; i < num_consumer; i++){
        consumer_cpid[i] = fork();

        if ( consumer_cpid[i] == 0 ) {
            consume(p_shm_recv_buf, p_shm_output_buf, p_control, wait_time);
            for (int a = 0; a < buffer_size; a++)
                shmdt(p_shm_recv_buf[a]);
            exit(0);
        } else if ( consumer_cpid[i] < 0 ) {
            perror("fork");
            abort();
        }
    }



    int child_status;
    /*wait children for completion*/
    for (int i = 0; i < num_producer; i++)
         wait(&child_status);
    for (int i = 0; i < num_consumer; i++)
         wait(&child_status);


    cat_png(p_shm_output_buf, 50);

    // End the timer
    if (gettimeofday(&tv, NULL) != 0) 
    {
        perror("gettimeofday");
        abort();
    }
    times[1] = tv.tv_sec + (tv.tv_usec / 1000000.0);
    printf("paster2 execution time: %.2lf seconds\n", times[1] - times[0]);

    /* cleanup */
    for (int i = 0; i < buffer_size; i++){
        shmdt(p_shm_recv_buf[i]);
        shmctl(shmid[i], IPC_RMID, NULL);
    }

    for (int i = 0; i < 50; i++){
        shmdt(p_shm_output_buf[i]);
        shmctl(shmid_out[i], IPC_RMID, NULL);
    }

    shmdt(p_control);
    shmctl(CTRL_shmid, IPC_RMID, NULL);

	if (sem_close (items) == -1) {
        perror ("sem_close");
        return 1;
    }
    if (sem_close (space) == -1) {
        perror ("sem_close");
        return 1;
    }
    if (sem_close (mutex) == -1) {
        perror ("sem_close");
        return 1;
    }

    if (sem_unlink (SEM_ITEMS) == -1) {
        perror ("sem_unlink");
        return 1;
    }
    if (sem_unlink (SEM_SPACE) == -1) {
        perror ("sem_unlink");
        return 1;
    }
    if (sem_unlink (SEM_MUTEX) == -1) {
        perror ("sem_unlink");
        return 1;
    }


    free(p_shm_recv_buf);
    free(shmid);
    free(p_shm_output_buf);
    free(shmid_out);
    free(producer_cpid);
    free(consumer_cpid);

    return 0;
}

void produce(RECV_BUF** buf_recv, CTRL_BLK *p_control, int img_num){
    /*setup part_num and server_num*/
    int part_num;
    int server_num;

    sem_wait(mutex);
    part_num = p_control -> produce_counter;
    p_control -> produce_counter = p_control -> produce_counter + 1;
    sem_post(mutex);
    server_num = part_num % 3 + 1;

    /*setup temp buffer*/
    RECV_BUF temp;
    temp.buf = malloc (IMG_SIZE);

    while(part_num < 50){
        /*setup URL*/
        char url[256];
        set_URL(url, img_num, server_num, part_num);

        /*start cURL*/
        CURL *curl_handle;
        CURLcode res;
        curl_global_init(CURL_GLOBAL_DEFAULT);

        /*initialize temp buffer*/
        memset(temp.buf, 0, IMG_SIZE);
        temp.size = 0;
        temp.max_size = IMG_SIZE;
        temp.seq = -1;

        /* init a curl session */
        curl_handle = curl_easy_init();

        if (curl_handle == NULL) {
            fprintf(stderr, "curl_easy_init: returned NULL\n");
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
            sem_wait(space);
            sem_wait(mutex);
            enqueue(buf_recv, &temp, p_control);
            sem_post(mutex);
            sem_post(items);
        }

        /* cleaning up */
        curl_easy_cleanup(curl_handle);
        curl_global_cleanup();


        sem_wait(mutex);
        part_num = p_control -> produce_counter;
        p_control -> produce_counter = p_control -> produce_counter + 1;
        sem_post(mutex);
        server_num = part_num % 3 + 1;
    }

    free(temp.buf);
}

void consume(RECV_BUF **buf_recv, RECV_BUF **output_buf, CTRL_BLK *p_control, int wait_time){
    int consume_terminate;

    sem_wait(mutex);
    consume_terminate = p_control -> consume_counter;
    p_control -> consume_counter = p_control -> consume_counter + 1;
    sem_post(mutex);

    while(consume_terminate < 50){
        RECV_BUF* temp;

        sem_wait(items);
        sem_wait(mutex);
        temp = dequeue(buf_recv, p_control);
        sem_post(mutex);
        sem_post(space);

        usleep(wait_time * 1000);
        move_to_shm_buf(temp, output_buf[temp -> seq]);

        sem_wait(mutex);
        consume_terminate = p_control -> consume_counter;
        p_control -> consume_counter = p_control -> consume_counter + 1;
        sem_post(mutex);
    }
}

// Function definitions
// Check if the queue is full
// @Return 1 - full, 0 - not full
int is_full(CTRL_BLK *p_control)
{
    if ((p_control -> front == (p_control -> rear) + 1) || (p_control -> front == 0 && p_control -> rear == p_control -> queue_size - 1))
    {
        return 1;
    }
    return 0;
}

// Check if the queue is empty
// @Return 1 - empty, 0 - not empty
int is_empty(CTRL_BLK *p_control)
{
    if (p_control -> front == -1)
    {
        return 1;
    }
    return 0;
}

// Enqueue an element
// @Params
// p_shm_recv_buf - shared memory
// temp - element to be added
void enqueue(RECV_BUF **p_shm_recv_buf, RECV_BUF *temp, CTRL_BLK *p_control)
{
    if (is_full(p_control))
    {
        printf("\n Queue is full! \n");
        return;
    }
    else {
        if (p_control -> front == -1)
        {
            p_control -> front = 0;
        }
        p_control -> rear = ((p_control -> rear) + 1) % p_control -> queue_size;

        memcpy(p_shm_recv_buf[p_control -> rear] -> buf, temp -> buf, temp -> size);
        p_shm_recv_buf[p_control -> rear] -> size = temp -> size;
        p_shm_recv_buf[p_control -> rear] -> max_size = temp -> max_size;
        p_shm_recv_buf[p_control -> rear] -> seq = temp -> seq;
    }
}

// Dequeue an element
// @Params
// p_shm_recv_buf - shared memory
// temp - element to be added
RECV_BUF* dequeue(RECV_BUF **p_shm_recv_buf, CTRL_BLK *p_control)
{
    RECV_BUF* temp = malloc (sizeof(RECV_BUF));
    temp -> buf = malloc (IMG_SIZE);
    memset(temp -> buf, 0, IMG_SIZE);
    temp -> size = 0;
    temp -> max_size = 0;
    temp -> seq = -1;

    if (is_empty(p_control))
    {
        printf("\n Queue is empty! \n");
        return temp;
    }
    else {
        // Retrieve the data
        memcpy(temp -> buf, p_shm_recv_buf[p_control -> front] -> buf, p_shm_recv_buf[p_control -> front] -> size);
        temp -> size = p_shm_recv_buf[p_control -> front] -> size;
        temp -> max_size = p_shm_recv_buf[p_control -> front] -> max_size;
        temp -> seq = p_shm_recv_buf[p_control -> front] -> seq;

        // Clean the data
        memset(p_shm_recv_buf[p_control -> front] -> buf, 0, IMG_SIZE);
        p_shm_recv_buf[p_control -> front]->size = 0;
        p_shm_recv_buf[p_control -> front]->max_size = 0;
        p_shm_recv_buf[p_control -> front]->seq = -1;

        if (p_control -> front == p_control -> rear)
        {
            p_control -> front = -1;
            p_control -> rear = -1;
        }
        else{
            p_control -> front = ((p_control -> front) + 1) % p_control -> queue_size;
        }
        return temp;
    }
}
