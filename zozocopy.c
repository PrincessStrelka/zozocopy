#include <dirent.h>
// #include <fcntl.h>
#include <linux/stat.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>

#include <ctype.h>
#include <errno.h>
#include <linux/fcntl.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <time.h>

// my global variables
#define nullptr NULL
char osSep = '/';
long targetFileCount = 1347375; // the aim is to process 1347375 files in a reasonable speed

void bitwisePrint(unsigned int var) {
        unsigned int temp = var;
        unsigned int maskSize = sizeof(var) * 8;
        for (unsigned int bit = 0; bit < maskSize; bit++) {
                printf("%u", (temp & 1 << (maskSize - 1) ? 1 : 0));
                temp = temp << 1;
        }
}

void getFileInfo(char path[]) {
        printf("\033[34m%s\033[0m\n", path);
        // fill in the parameters for statx call, and call it
        struct statx stxBuf;

        // print the file we are statting, and the return code from calling statx
        printf("statx result: %i\n",
               statx(AT_FDCWD,
                     *path,
                     AT_EMPTY_PATH | AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW | AT_STATX_FORCE_SYNC,
                     STATX_BASIC_STATS | STATX_BTIME | STATX_MNT_ID | STATX_DIOALIGN | STATX_MNT_ID_UNIQUE,
                     &stxBuf));

        // print the stx mask result
        printf("stx_mask: ");
        bitwisePrint(stxBuf.stx_mask);
        printf("\n");

        // get stat struct for path
        struct stat statBuf;
        stat(path, &statBuf);

        // print stats
        printf(" aTime (Access): %ld, %lu\n", statBuf.st_atime, statBuf.st_atim.tv_nsec);
        printf(" mTime (Modify): %ld, %lu\n", statBuf.st_mtime, statBuf.st_mtim.tv_nsec);
        printf(" cTime (Change): %ld, %lu\n", statBuf.st_ctime, statBuf.st_ctim.tv_nsec);
        // if the stx buffer contains btime, print btime, else dont
        printf("crTime (Birth ): ");
        if (stxBuf.stx_mask & STATX_BTIME)
                printf("%lld, %u", stxBuf.stx_btime.tv_sec, stxBuf.stx_btime.tv_nsec);
        else
                printf("-");
        printf("\n");
}

void addChar(char *s, char c) {
        // https://www.geeksforgeeks.org/how-to-append-a-character-to-a-string-in-c/
        while (*s++)
                ;     // Move pointer to the end
        *(s - 1) = c; // Append the new character
        *s = '\0';    // Add null terminator to mark new end
}

void ensureOsSep(char *s) {
        if (s[strlen(s) - 1] != osSep) {
                addChar(s, osSep);
        }
}

void travelDirectory(char sourcePath[]) {
        // ensure source path ends with the os seperator
        ensureOsSep(sourcePath);

        // https://iq.opengenus.org/traversing-folders-in-c/
        DIR *sourceDir = opendir(sourcePath); // get sourceDir as a pointer to a DIR
                                              // struct from filename string. returns
                                              // DIR upon success, NULL upon failure
        struct dirent *dp;
        char *file_name; // define the filename variable
        struct stat filePathStatBuf;

        // declare and initialise a variable that will store the sourcepath +
        // filename should implement hannahs suggestion for dynamic allocation
        char filePath[1000];

        // if sourcedir is not a directory, exit
        if (!sourceDir)
                return;

        // loop through every file under sourcePath
        while ((dp = readdir(sourceDir)) != NULL) {
                // get the file name from dp
                file_name = dp->d_name;

                // if we are on a refrence to the current(.) or parent(..)
                // directory, skip this loop
                if (strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0)
                        continue;

                // concatonate the source path and file name together into a new
                // variable
                strcpy(filePath, sourcePath);
                strcat(filePath, file_name);

                // find if the filepath is a directory or not
                stat(filePath, &filePathStatBuf);
                if (S_ISDIR(filePathStatBuf.st_mode)) {
                        // if it is a directory
                        getFileInfo(sourcePath);
                        travelDirectory(filePath);
                } else {
                        // if it is a filepath
                        getFileInfo(filePath);
                }
        }
        closedir(sourceDir); // closes the sourceDir DIR struct
}

int main() {
        // ensure last character of source folder is the os seperator
        char filename[] = "/home/zoey/Desktop/source";
        // char filename[] = "/media/zoey/DATA/BACKUP/Pictures/this user/";
        ensureOsSep(filename);

        // ensure the last character of dest folder is the os seperator
        char destFolder[] = "/home/zoey/Desktop/test";
        ensureOsSep(destFolder);

        // recursively itterate through every file and folder in source
        // directory, printing out the names of all directories and files,
        // including source directory
        printf("Copying from \"%s\" to \"%s\"\n", filename, destFolder);
        travelDirectory(filename);
        getFileInfo("/home/zoey/Desktop/source/sourcefile.txt");
        getFileInfo("/home/zoey/Desktop/source/sourcefile.txt");

        return 0;
}
