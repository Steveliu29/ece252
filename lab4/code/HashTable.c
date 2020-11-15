#include <stdio.h>
#include <stdlib.h>
#include <search.h>
#include <string.h>



// Output:
//http://ece252-1.uwaterloo.ca/~yqhuang/lab4/
//http://ece252-1.uwaterloo.ca/~yqhuang/lab4/
//http://ece252-1.uwaterloo.ca/~yqhuang/lab3/index.html
//http://ece252-1.uwaterloo.ca/~yqhuang/lab3/index.html
//http://ece252-1.uwaterloo.ca/~yqhuang/lab4/Disguise.png
//http://ece252-1.uwaterloo.ca/~yqhuang/lab4/Disguise.png
int main()
{
    hcreate(5);

    ENTRY entry1;
    char *URL1 = "http://ece252-1.uwaterloo.ca/~yqhuang/lab4/";
    int length1 = sizeof("http://ece252-1.uwaterloo.ca/~yqhuang/lab4/");
    entry1.key = malloc(sizeof(char) * length1);
    entry1.data = malloc(sizeof(char) * length1);
    memset(entry1.key, 0, length1);
    memset(entry1.data, 0, length1);
    strcpy(entry1.key, URL1);
    strcpy(entry1.data, URL1);

    ENTRY entry2;
    char *URL2 = "http://ece252-1.uwaterloo.ca/~yqhuang/lab3/index.html";
    int length2 = sizeof("http://ece252-1.uwaterloo.ca/~yqhuang/lab3/index.html");
    entry2.key = malloc(sizeof(char) * length2);
    entry2.data = malloc(sizeof(char) * length2);
    memset(entry2.key, 0, length2);
    memset(entry2.data, 0, length2);
    strcpy(entry2.key, URL2);
    strcpy(entry2.data, URL2);

    ENTRY entry3;
    char *URL3 = "http://ece252-1.uwaterloo.ca/~yqhuang/lab4/Disguise.png";
    int length3 = sizeof("http://ece252-1.uwaterloo.ca/~yqhuang/lab4/Disguise.png");
    entry3.key = malloc(sizeof(char) * length3);
    entry3.data = malloc(sizeof(char) * length3);
    memset(entry3.key, 0, length3);
    memset(entry3.data, 0, length3);
    strcpy(entry3.key, URL3);
    strcpy(entry3.data, URL3);

    if (hsearch(entry1, FIND) == NULL)
        hsearch(entry1, ENTER);
    if (hsearch(entry2, FIND) == NULL)
        hsearch(entry2, ENTER);
    if (hsearch(entry3, FIND) == NULL)
        hsearch(entry3, ENTER);

    ENTRY *result1;
    ENTRY *result2;
    ENTRY *result3;

    if (hsearch(entry1, FIND) != NULL)
    {
        result1 = hsearch(entry1, FIND);
        printf("%s\n", result1->key);
        printf("%s\n", result1->data);
    }
    if (hsearch(entry2, FIND) != NULL)
    {
        result2 = hsearch(entry2, FIND);
        printf("%s\n", result2->key);
        printf("%s\n", result2->data);
    }
    if (hsearch(entry3, FIND) != NULL)
    {
        result3 = hsearch(entry3, FIND);
        printf("%s\n", result3->key);
        printf("%s\n", result3->data);
    }

    free(entry1.key);
    free(entry1.data);
    free(entry2.key);
    free(entry2.data);
    free(entry3.key);
    free(entry3.data);

    return 0;
}