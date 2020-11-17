/*
 * The code is derived from cURL example and paster.c base code.
 * The cURL example is at URL:
 * https://curl.haxx.se/libcurl/c/getinmemory.html
 * Copyright (C) 1998 - 2018, Daniel Stenberg, <daniel@haxx.se>, et al..
 *
 * The xml example code is
 * http://www.xmlsoft.org/tutorial/ape.html
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
 * @file main_wirte_read_cb.c
 * @brief cURL write call back to save received data in a user defined memory first
 *        and then write the data to a file for verification purpose.
 *        cURL header call back extracts data sequence number from header if there is a sequence number.
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
#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>

#include <search.h>
#include "Queue.c"


#define SEED_URL "http://ece252-1.uwaterloo.ca/lab4/"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */

#define CT_PNG  "image/png"
#define CT_HTML "text/html"
#define CT_PNG_LEN  9
#define CT_HTML_LEN 9

int URL_counter = 0;

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

typedef struct recv_buf2 {
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} RECV_BUF;

typedef struct png_url2 {
    char url[256];
    int url_size;
} PNG_URL;

int is_png(char *buf);
htmlDocPtr mem_getdoc(char *buf, int size, const char *url);
xmlXPathObjectPtr getnodeset (xmlDocPtr doc, xmlChar *xpath);
int find_http(char *fname, int size, int follow_relative_links, const char *base_url, my_queue* queue);
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
void cleanup(CURL *curl, RECV_BUF *ptr);
int write_file(const char *path, const void *in, size_t len);
CURL *easy_handle_init(RECV_BUF *ptr, const char *url);
int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf, my_queue* queue, PNG_URL* my_png_url);


int is_png(char *buf){

    char *correct_png = malloc ( 8 * sizeof(char) ); /*allocate 8 bytes to check*/

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

htmlDocPtr mem_getdoc(char *buf, int size, const char *url)
{
    int opts = HTML_PARSE_NOBLANKS | HTML_PARSE_NOERROR | \
               HTML_PARSE_NOWARNING | HTML_PARSE_NONET;
    htmlDocPtr doc = htmlReadMemory(buf, size, url, NULL, opts);

    if ( doc == NULL ) {
  //      fprintf(stderr, "Document not parsed successfully.\n");
        return NULL;
    }
    return doc;
}

xmlXPathObjectPtr getnodeset (xmlDocPtr doc, xmlChar *xpath)
{

    xmlXPathContextPtr context;
    xmlXPathObjectPtr result;

    context = xmlXPathNewContext(doc);
    if (context == NULL) {
    //    printf("Error in xmlXPathNewContext\n");
        return NULL;
    }
    result = xmlXPathEvalExpression(xpath, context);
    xmlXPathFreeContext(context);
    if (result == NULL) {
  //      printf("Error in xmlXPathEvalExpression\n");
        return NULL;
    }
    if(xmlXPathNodeSetIsEmpty(result->nodesetval)){
        xmlXPathFreeObject(result);
      //  printf("No result\n");
        return NULL;
    }
    return result;
}

int find_http(char *buf, int size, int follow_relative_links, const char *base_url, my_queue* queue)
{

    int i;
    htmlDocPtr doc;
    xmlChar *xpath = (xmlChar*) "//a/@href";
    xmlNodeSetPtr nodeset;
    xmlXPathObjectPtr result;
    xmlChar *href;
  //  printf("test4\n");
    if (buf == NULL) {
        return 1;
    }

  //  printf("test5\n");

    doc = mem_getdoc(buf, size, base_url);
    result = getnodeset (doc, xpath);
    if (result) {
        nodeset = result->nodesetval;
      //  printf("test6\n");
        for (i=0; i < nodeset->nodeNr; i++) {
            href = xmlNodeListGetString(doc, nodeset->nodeTab[i]->xmlChildrenNode, 1);
            if ( follow_relative_links ) {
                xmlChar *old = href;
                href = xmlBuildURI(href, (xmlChar *) base_url);
                xmlFree(old);
            }
          //  printf("test7\n");
            if ( href != NULL && !strncmp((const char *)href, "http", 4) ) {
                ENTRY pending_entry;
                char * temp_url = (char*) href;
                int url_length = (strlen(temp_url) + 1);
            //    printf("test8\n");
              //  printf("TEST URL: %s\n", temp_url);

                // temp_entry.key = malloc(sizeof(char) * url_length);
                // temp_entry.data = malloc(sizeof(char) * url_length);

                pending_entry.key = temp_url;
                pending_entry.data = temp_url;




                // memset(temp_entry.key, 0, url_length);
                // memset(temp_entry.data, 0, url_length);
                //
                // strcpy(temp_entry.key, temp_url);
                // strcpy(temp_entry.data, temp_url);
            //    printf("test9\n");

                if (hsearch(pending_entry, FIND) == NULL){
                    ENTRY new_entry;

                    new_entry.key = malloc(sizeof(char) * url_length);
                    new_entry.data = malloc(sizeof(char) * url_length);

                    memset(new_entry.key, 0, url_length);
                    memset(new_entry.data, 0, url_length);

                    strcpy(new_entry.key, temp_url);
                    strcpy(new_entry.data, temp_url);

                    char* url_to_add = malloc( url_length* sizeof(char));
                    memset(url_to_add, 0, url_length);
                    strcpy(url_to_add, temp_url);

                    hsearch(new_entry, ENTER);
                    printf("URL_to_add: %s\n", url_to_add);
                    enqueue(queue, url_to_add);
                }
                // else {
                //   //  free_counter1++;
                //     //printf("Freed1: %d\n", free_counter1);
                //     free(temp_entry.key);
                //     free(temp_entry.data);
                // }
          //      printf("href: %s\n", href);
            }
            xmlFree(href);
        }
        xmlXPathFreeObject (result);
    }
    xmlFreeDoc(doc);
    xmlCleanupParser();
    return 0;
}
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

#ifdef DEBUG1_
    printf("%s", p_recv);
#endif /* DEBUG1_ */
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

size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;

    if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */
        /* received data is not 0 terminated, add one byte for terminating 0 */
        size_t new_size = p->max_size + max(BUF_INC, realsize + 1);
        char *q = realloc(p->buf, new_size);
        if (q == NULL) {
            perror("realloc"); /* out of memory */
            return -1;
        }
        p->buf = q;
        p->max_size = new_size;
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}


int recv_buf_init(RECV_BUF *ptr, size_t max_size)
{
    void *p = NULL;

    if (ptr == NULL) {
        return 1;
    }

    p = malloc(max_size);
    if (p == NULL) {
	return 2;
    }

    ptr->buf = p;
    ptr->size = 0;
    ptr->max_size = max_size;
    ptr->seq = -1;              /* valid seq should be positive */
    return 0;
}

int recv_buf_cleanup(RECV_BUF *ptr)
{
    if (ptr == NULL) {
	return 1;
    }

    free(ptr->buf);
    ptr->size = 0;
    ptr->max_size = 0;
    return 0;
}

void cleanup(CURL *curl, RECV_BUF *ptr)
{
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        recv_buf_cleanup(ptr);
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

void write_url(PNG_URL* png_url, int URL_counter, char* file_name){
    int total_size = 0;
    for (int i = 0; i < URL_counter; i++){
        total_size = total_size + png_url[i].url_size;
    }

    char* file_to_write = malloc(total_size * sizeof(char));

    int read_counter = 0;
    for (int i = 0; i < URL_counter; i++){
        memcpy(file_to_write + read_counter, png_url[i].url, png_url[i].url_size);
        file_to_write[read_counter + png_url[i].url_size - 1] = '\n';
        read_counter = read_counter + png_url[i].url_size;

    }

    write_file(file_name, file_to_write, total_size);

    free(file_to_write);

}

/**
 * @brief create a curl easy handle and set the options.
 * @param RECV_BUF *ptr points to user data needed by the curl write call back function
 * @param const char *url is the target url to fetch resoruce
 * @return a valid CURL * handle upon sucess; NULL otherwise
 * Note: the caller is responsbile for cleaning the returned curl handle
 */

CURL *easy_handle_init(RECV_BUF *ptr, const char *url)
{
    CURL *curl_handle = NULL;

    if ( ptr == NULL || url == NULL) {
        return NULL;
    }

    /* init user defined call back function buffer */
    if ( recv_buf_init(ptr, BUF_SIZE) != 0 ) {
        return NULL;
    }
    /* init a curl session */
    curl_handle = curl_easy_init();

    if (curl_handle == NULL) {
        fprintf(stderr, "curl_easy_init: returned NULL\n");
        return NULL;
    }

    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);

    /* register write call back function to process received data */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3);
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)ptr);

    /* register header call back function to process received header data */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)ptr);

    /* some servers requires a user-agent field */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "ece252 lab4 crawler");

    /* follow HTTP 3XX redirects */
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    /* continue to send authentication credentials when following locations */
    curl_easy_setopt(curl_handle, CURLOPT_UNRESTRICTED_AUTH, 1L);
    /* max numbre of redirects to follow sets to 5 */
    curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 5L);
    /* supports all built-in encodings */
    curl_easy_setopt(curl_handle, CURLOPT_ACCEPT_ENCODING, "");

    /* Max time in seconds that the connection phase to the server to take */
    //curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 5L);
    /* Max time in seconds that libcurl transfer operation is allowed to take */
    //curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 10L);
    /* Time out for Expect: 100-continue response in milliseconds */
    //curl_easy_setopt(curl_handle, CURLOPT_EXPECT_100_TIMEOUT_MS, 0L);

    /* Enable the cookie engine without reading any initial cookies */
    curl_easy_setopt(curl_handle, CURLOPT_COOKIEFILE, "");
    /* allow whatever auth the proxy speaks */
    curl_easy_setopt(curl_handle, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
    /* allow whatever auth the server speaks */
    curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY);

    return curl_handle;
}

int process_html(CURL *curl_handle, RECV_BUF *p_recv_buf, my_queue* queue)
{
    char fname[256];
    int follow_relative_link = 1;
    char *url = NULL;
    pid_t pid = getpid();
  //  printf("test2\n");
    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &url);
  //  printf("test3\n");
    find_http(p_recv_buf->buf, p_recv_buf->size, follow_relative_link, url, queue);

    return 0;
    // sprintf(fname, "./output_%d.html", pid);
    // return write_file(fname, p_recv_buf->buf, p_recv_buf->size);
}

int process_png(CURL *curl_handle, RECV_BUF *p_recv_buf, PNG_URL* my_png_url)
{
    pid_t pid =getpid();
    //char fname[256];
    char buf[8];
    memcpy(buf, p_recv_buf -> buf, 8);
    //printf("check result: %d\n", is_png(buf));
    //printf("SEQ_NUM is: %d\n", p_recv_buf -> seq);
    char *eurl = NULL;          /* effective URL */
    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &eurl);
    if ( eurl != NULL && is_png(buf) == 0) {
      //  printf("The PNG url is: %s\n", eurl);
        strcpy(my_png_url[URL_counter].url, eurl);
        my_png_url[URL_counter].url_size = strlen(eurl) + 1;
        URL_counter = URL_counter + 1;
    }


    return 0;
    // sprintf(fname, "./output_%d_%d.png", p_recv_buf->seq, pid);
    // return write_file(fname, p_recv_buf->buf, p_recv_buf->size);
}
/**
 * @brief process teh download data by curl
 * @param CURL *curl_handle is the curl handler
 * @param RECV_BUF p_recv_buf contains the received data.
 * @return 0 on success; non-zero otherwise
 */

int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf, my_queue* queue, PNG_URL* my_png_url)
{
    CURLcode res;
    char fname[256];
    pid_t pid =getpid();
    long response_code;

    res = curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
    if ( res == CURLE_OK ) {
	    //printf("Response code: %ld\n", response_code);
    }

    if ( response_code >= 400 ) {
    //	fprintf(stderr, "Error.\n");
        return 1;
    }

    char *ct = NULL;
    res = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &ct);
    if ( res == CURLE_OK && ct != NULL ) {
    //	printf("Content-Type: %s, len=%ld\n", ct, strlen(ct));
    } else {
    //    fprintf(stderr, "Failed obtain Content-Type\n");
        return 2;
    }

    if ( strstr(ct, CT_HTML) ) {
      //  printf("test1\n");
        return process_html(curl_handle, p_recv_buf, queue);
    } else if ( strstr(ct, CT_PNG) ) {
        return process_png(curl_handle, p_recv_buf, my_png_url);
    } else {
        // sprintf(fname, "./output_%d", pid);
    }

    return 0;
    // return write_file(fname, p_recv_buf->buf, p_recv_buf->size);
}

int main( int argc, char** argv )
{
    CURL *curl_handle;
    CURLcode res;
    char* url = malloc(256 * sizeof(char));

    int c;
    int t = 1;
    int m = 1;
    char v[256];
    char *str = "option requires an argument";

    memset(url, 0, 256);
    memset(v, 0, 256);
    while ((c = getopt (argc, argv, "t:m:v:")) != -1) {
        switch (c) {

        case 't':
	    t = strtoul(optarg, NULL, 10);
	  //  printf("option -t specifies a value of %d.\n", t);
	    if (t <= 0) {
                fprintf(stderr, "%s: %s > 0 -- 't'\n", argv[0], str);
                return -1;
            }
            break;

        case 'm':
            m = strtoul(optarg, NULL, 10);
	 //   printf("option -m specifies a value of %d.\n", m);
            break;

        case 'v':
            strcpy(v, optarg);
    //  printf("option -v specifies a value of %s.\n", v);
            break;

        default:
            return -1;

        }
    }

    strcpy(url, argv[argc-1]);
    //printf("The URL is: %s.\n", url);

    /*Start of processing url*/

    hcreate(500);
    my_queue* url_queue = queue_init(500);
    PNG_URL* my_png_url = malloc (m * sizeof(PNG_URL));
    PNG_URL* log_url = malloc (500 * sizeof(PNG_URL));

    for (int i = 0; i < m; i++){
        memset(my_png_url[i].url, 0, 256);
        my_png_url[i].url_size = -1;
    }

    for (int i = 0; i < 500; i++){
        memset(log_url[i].url, 0, 256);
        log_url[i].url_size = -1;
    }

    URL_counter = 0;
    int log_counter = 0;

    ENTRY first_entry;
    int url_length = (strlen(url) + 1);
    first_entry.key = malloc(sizeof(char) * url_length);
    first_entry.data = malloc(sizeof(char) * url_length);
    memset(first_entry.key, 0, url_length);
    memset(first_entry.data, 0, url_length);

    strcpy(first_entry.key, url);
    strcpy(first_entry.data, url);

    if (hsearch(first_entry, FIND) == NULL)
    {
        hsearch(first_entry, ENTER);
    //    printf("URL_to_add: %s\n", url_to_add);
        enqueue(url_queue, url);
    }

    while (!is_empty(url_queue) && URL_counter != m){
        RECV_BUF recv_buf;
        char* next_url = dequeue(url_queue);
        printf("DEQUEUE URL: %s\n", next_url);

        strcpy(log_url[log_counter].url, next_url);
        log_url[log_counter].url_size = strlen(next_url) + 1;
        log_counter = log_counter + 1;

        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl_handle = easy_handle_init(&recv_buf, next_url);

        if ( curl_handle == NULL ) {
            fprintf(stderr, "Curl initialization failed. Exiting...\n");
            curl_global_cleanup();
            abort();
        }

        /* get it! */
        res = curl_easy_perform(curl_handle);

        if( res != CURLE_OK) {
            // fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            // cleanup(curl_handle, &recv_buf);
            // exit(1);
        } else {
	         //  printf("%lu bytes received in memory %p, seq=%d.\n", recv_buf.size, recv_buf.buf, recv_buf.seq);
             process_data(curl_handle, &recv_buf, url_queue, my_png_url);
        }

        /* process the download data */

        cleanup(curl_handle, &recv_buf);

    }

    write_url(my_png_url, URL_counter, "png_urls.txt");

    if (v != "")
        write_url(log_url, log_counter, v);


    // Clean-up

    for (int i = url_queue -> rear; i >= 0; i--){
        ENTRY input;

        char* remaining_url = url_queue -> ptr[i];
        input.key = remaining_url;
      	input.data = remaining_url;

        if (hsearch(input, FIND) != NULL)
      	{
      		ENTRY *result = hsearch(input, FIND);
      		free(result->key);
      		free(result->data);
      	}

    }
    printf("queue size: %d\n", url_queue -> rear);
    free(my_png_url);
    free(log_url);
    queue_destory(url_queue);
    hdestroy();

    return 0;
}
