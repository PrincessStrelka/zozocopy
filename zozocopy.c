// https://iq.opengenus.org/traversing-folders-in-c/

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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <time.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

// my global variables
#define nullptr NULL
char osSep = '/';
long targetFileCount = 1347375; // the aim is to process 1347375 files in a reasonable speed

// https://www.geeksforgeeks.org/how-to-append-a-character-to-a-string-in-c/
void addChar(char *s, char c) {
        while (*s++)
                ;     // Move pointer to the end
        *(s - 1) = c; // Append the new character
        *s = '\0';    // Add null terminator to mark new end
}
// https:www.geeksforgeeks.org/how-to-convert-an-integer-to-a-string-in-c/
void intToStr(int N, char *str) {
        int i = 0;

        // Save the copy of the number for sign
        int sign = N;

        // If the number is negative, make it positive
        if (N < 0)
                N = -N;

        // Extract digits from the number and add them to the
        // string
        while (N > 0) {

                // Convert integer digit to character and store
                // it in the str
                str[i++] = N % 10 + '0';
                N /= 10;
        }

        // If the number was negative, add a minus sign to the
        // string
        if (sign < 0) {
                str[i++] = '-';
        }

        // Null-terminate the string
        str[i] = '\0';

        // Reverse the string to get the correct order
        for (int j = 0, k = i - 1; j < k; j++, k--) {
                char temp = str[j];
                str[j] = str[k];
                str[k] = temp;
        }
}
void bitwisePrint(unsigned int var) {
        unsigned int temp = var;
        unsigned int maskSize = sizeof(var) * 8;
        for (unsigned int bit = 0; bit < maskSize; bit++) {
                printf("%u", (temp & 1 << (maskSize - 1) ? 1 : 0));
                temp = temp << 1;
        }
}
void ensureOsSeperator(char *s) {
        if (s[strlen(s) - 1] != osSep) {
                addChar(s, osSep);
        }
}
void printTime(char label[], unsigned int mask, unsigned int maskConst, struct statx_timestamp timestamp) {
        if (mask & maskConst) {
                // get times from timestamp
                signed long long tv_epoch = timestamp.tv_sec;
                unsigned int tv_nsec = timestamp.tv_nsec;

                // get times for set inode field
                long long adjusted = tv_epoch + 2147483648;
                signed long long sif_time = (adjusted % 4294967296) - 2147483648;

                // shift it over by two bits
                // store the multiplier in the lower two bits
                unsigned int sif_extra = (tv_nsec << 2) + floor(adjusted / 4294967296);

                // get epoch as a formatted string
                struct tm tm;
                char dateStr[255];
                char epochAsstr[10];
                intToStr(timestamp.tv_sec, epochAsstr);
                memset(&tm, 0, sizeof(struct tm));
                strptime(epochAsstr, "%s", &tm);
                strftime(dateStr, sizeof(dateStr), "%Y-%m-%d %H:%M:%S", &tm);

                // print time data
                printf("%s: %s.%u | %lld %u\n", label, dateStr, tv_nsec, sif_time, sif_extra);

        } else
                printf("%s: -\n", label);
}

void replaceFirstInstance(char *str, char *match, char *replacement) {
        char const *pos = strstr(str, match);
        if (pos) {
                // find the index of the substring match
                int matchindex = pos - str;

                // get everything from before the substring
                char modifiedString[255] = "";
                strncpy(modifiedString, str, matchindex);

                // get everything from after the substring
                char lastPart[255] = "";
                strncpy(lastPart, str + matchindex + strlen(match), strlen(str));

                // concatinate first part of string, replacement, and last part of string
                strcat(modifiedString, replacement);
                strcat(modifiedString, lastPart);

                // put the modified string into string
                strcpy(str, modifiedString);
        }
}

void copyPath(char path[], char *baseFolder, char *destFolder) {
        // fill in the parameters for statx call, and call it
        struct statx stxBuf;

        // print the file we are statting, and the return code from calling statx
        printf("\033[34m%s\033[0m\n", path);
        int stxFlags = AT_EMPTY_PATH | AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW | AT_STATX_FORCE_SYNC;
        unsigned int stxMask = STATX_BASIC_STATS | STATX_BTIME | STATX_MNT_ID | STATX_DIOALIGN | STATX_MNT_ID_UNIQUE;
        int ret = statx(AT_FDCWD, path, stxFlags, stxMask, &stxBuf);
        printf("statx result: %i\n", ret);
        if (ret < 0)
                return;

        // print the stx mask result
        printf("stx_mask: ");
        bitwisePrint(stxBuf.stx_mask);
        printf("\n");

        // if the stx buffer contains a time, print the time, else dont
        printTime("aTime        (Access)", stxBuf.stx_mask, STATX_ATIME, stxBuf.stx_atime);
        printTime("mTime        (Modify)", stxBuf.stx_mask, STATX_MTIME, stxBuf.stx_mtime);
        printTime("cTime        (Change)", stxBuf.stx_mask, STATX_CTIME, stxBuf.stx_ctime);
        printTime("crTime/bTime (Birth )", stxBuf.stx_mask, STATX_BTIME, stxBuf.stx_btime);

        // ensure basefolder does not end with os sep
        char copyBaseFolder[1000];
        strcpy(copyBaseFolder, baseFolder);
        if ((copyBaseFolder[strlen(copyBaseFolder) - 1]) == '/')
                copyBaseFolder[strlen(copyBaseFolder) - 1] = '\0';

        // get the source path base
        char *lastOsSepIndex = strrchr(copyBaseFolder, osSep);
        char *sourcePathBase = lastOsSepIndex ? lastOsSepIndex + 1 : copyBaseFolder;

        // create a modified base path
        ensureOsSeperator(destFolder);
        char modifiedBaseFolder[1000];
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        sprintf(modifiedBaseFolder, "%s%s-%d-%02d-%02d_%02d.%02d.%02d", destFolder, sourcePathBase, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

        // swap out the base path in the filepath for the modified base path
        char copyFilePath[1000] = "";
        strcpy(copyFilePath, path);
        printf("%s\n", copyFilePath);
        replaceFirstInstance(copyFilePath, copyBaseFolder, modifiedBaseFolder);
        printf("%s\n", copyFilePath);
}

void travelDirectory(char sourceDirectory[], char *baseFolder, char *destFolder) {
        ensureOsSeperator(sourceDirectory);

        copyPath(sourceDirectory, baseFolder, destFolder);

        DIR *sourceDir = opendir(sourceDirectory); // returns DIR struct upon success, NULL upon failure
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
                strcpy(filePath, sourceDirectory);
                strcat(filePath, file_name);

                // find if the filepath is a directory or not
                stat(filePath, &filePathStatBuf);
                if (S_ISDIR(filePathStatBuf.st_mode)) {
                        // if it is a directory
                        // getFileInfo(sourcePath);
                        travelDirectory(filePath, baseFolder, destFolder);
                } else {
                        // if it is a filepath
                        copyPath(filePath, baseFolder, destFolder);
                }
        }
        closedir(sourceDir); // closes the sourceDir DIR struct
}

int main() {
        // ensure last character of source folder is the os seperator
        char filename[1000] = "/home/zoey/Desktop/source";
        //  char filename[] = "/media/zoey/DATA/BACKUP/Pictures/this user/";
        ensureOsSeperator(filename);

        // ensure the last character of dest folder is the os seperator
        char destFolder[1000] = "/home/zoey/Desktop/test";
        ensureOsSeperator(destFolder);

        // recursively itterate through every file and folder in source
        // directory, printing out the names of all directories and files,
        // including source directory
        printf("Copying from \"%s\" to \"%s\"\n", filename, destFolder);

        travelDirectory(filename, filename, destFolder);

        return 0;
}
