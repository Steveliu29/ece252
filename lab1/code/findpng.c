#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <zconf.h>
#include "lab_png.c"



// Function declarations
void findpng(char *path);
int main(int argc, char **argv);


// Function specifications
void findpng(char *path)
{
    // Variable
    DIR *dir = opendir(path);
    struct dirent *dirent;
    struct stat stat;


    // Recursive function
    while ((dirent = readdir(dir)) != NULL)
    {
        // Store updated path in pathStorage
        unsigned long length = strlen(path) + 2;
        char path_storage[1024 + length];
        snprintf(path_storage, sizeof(path_storage), "%s/%s", path, dirent->d_name);

        // Write information about the file into stat
        if (lstat(path_storage, &stat) < 0)
        {
            continue;
        }

        // Regular file
        if (S_ISREG(stat.st_mode))
        {
            FILE *file = fopen(path_storage, "r");
            if (file == NULL)
            {
                fclose(file);
                continue;
            }
            U8 buffer[8];
            fread(buffer, 1, 8, file);

            // Check if it a png file
            if (is_png(buffer) == 0)
            {
                printf("%s/%s\n", path, dirent->d_name);
            }

            fclose(file);
        }
        // Directory
        else if (S_ISDIR(stat.st_mode))
        {
            // Ignore current and previous directories
            if (strcmp(dirent->d_name, ".") == 0)
            {
                continue;
            }
            if (strcmp(dirent->d_name, "..") == 0)
            {
                continue;
            }

            findpng(path_storage);
        }
        // Symbolic link
        else {
            continue;
        }
    }

    // Close directory stream
    closedir(dir);
}


int main(int argc, char **argv)
{
    // Variables
    DIR *dir;


    // Error detections

    // No directory
    if (argc == 1)
    {
        printf("Usage: %s <directory name>\n", argv[0]);
        exit(1);
    }

    // Directory does not exist
    if ((dir = opendir(argv[1])) == NULL)
    {
        printf("Error opening directory: %s\n", argv[1]);
        exit(2);
    }


    // There should be error when calling the function
    findpng(argv[1]);


    // Close directory stream
    if (closedir(dir) != 0)
    {
        printf("closedir");
        exit(3);
    }

    return 0;
}