#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <linux/fcntl.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/stat.h>
#include <selinux/label.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define COUNT(arr) (sizeof(arr) / sizeof(*arr))
#define nullptr NULL

struct ext4_time {
        char label[64];
        char field[64];
        struct statx_timestamp statx_time;
        signed long long epoch;
        unsigned long long extra;
        signed long long ns_epoch;
};

// █ ▓ ░
// my global variables
char osSep = '/';
// global variables
struct ext4_time lowest_time = {.ns_epoch = INT64_MAX};
struct ext4_time *timestamps_to_fix[4];
size_t fix_ts_index = 0;
unsigned int stx_mask = 0;

void gfgAddChar(char *s, char c) {
        // https://www.geeksforgeeks.org/how-to-append-a-character-to-a-string-in-c/
        while (*s++)
                ;     // Move pointer to the end
        *(s - 1) = c; // Append the new character
        *s = '\0';    // Add null terminator to mark new end
}
void gfgIntToStr(int N, char *str) {
        // https:www.geeksforgeeks.org/how-to-convert-an-integer-to-a-string-in-c/
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
void bitwisePrint(int var) {
        int var_copy = var;                                            // copy the input into a modifyable variable
        int mask_size = sizeof(var) * 8;                               // get the amount of bits in the input variable
        for (size_t bit = 0; bit < mask_size; bit++) {                 // itterate through every bit in the input variable
                bool is_one = var_copy & 1 << (mask_size - 1);         // check if the highest bit is one
                printf("\033[%im%u\033[0m", is_one ? 97 : 30, is_one); // print the bit
                var_copy = var_copy << 1;                              // put the next bit into the highest bit
        }
}
void ensureOsSeperator(char *s) {
        if (s[strlen(s) - 1] != osSep) {
                gfgAddChar(s, osSep);
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
                gfgIntToStr(timestamp.tv_sec, epochAsstr);
                memset(&tm, 0, sizeof(struct tm));
                strptime(epochAsstr, "%s", &tm);
                strftime(dateStr, sizeof(dateStr), "%Y-%m-%d %H:%M:%S", &tm);

                // print time data
                printf("%s: %s.%u | %lld %u\n", label, dateStr, tv_nsec, sif_time, sif_extra);

        } else
                printf("%s: -\n", label);
}
void replaceFirstInstance(char *src_string, char *substring, char *replacement) {
        char const *substring_position = strstr(src_string, substring);
        if (substring_position) {
                // find the index of the substring match
                int matchindex = substring_position - src_string;

                // get everything from before the substring
                char modifiedString[255] = "";
                strncpy(modifiedString, src_string, matchindex);

                // get everything from after the substring
                char lastPart[255] = "";
                strncpy(lastPart, src_string + matchindex + strlen(substring), strlen(src_string));

                // concatinate first part of string, replacement, and last part of string
                strcat(modifiedString, replacement);
                strcat(modifiedString, lastPart);

                // put the modified string into string
                strcpy(src_string, modifiedString);
        }
}
void copyPath(char src_path[], char *baseFolder, char *destFolder) {
        printf("\033[34m%s\033[0m\n", src_path);
        /* ---- CREATE NEW FILE PATH ---- */
        // ensure basefolder does not end with os sep
        char copyBaseFolder[1000];
        strcpy(copyBaseFolder, baseFolder);
        if ((copyBaseFolder[strlen(copyBaseFolder) - 1]) == '/')
                copyBaseFolder[strlen(copyBaseFolder) - 1] = '\0';

        // get the source path base
        char *lastOsSepIndex = strrchr(copyBaseFolder, osSep);
        char *sourcePathBase = lastOsSepIndex ? lastOsSepIndex + 1 : copyBaseFolder;

        // create a modified base path
        char modifiedBaseFolder[1000];
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        sprintf(modifiedBaseFolder, "%s%s-%d-%02d-%02d_%02d.%02d.%02d", destFolder, sourcePathBase, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

        // swap out the base path in the filepath for the modified base path
        char dst_path[1000] = "";
        strcpy(dst_path, src_path);
        replaceFirstInstance(dst_path, copyBaseFolder, modifiedBaseFolder);
        printf("\033[35m%s\033[0m\n", dst_path);

        /* ---- GET BASE FILE STATS ---- */
        // fill in the parameters for statx call, and call it
        struct statx src_stxBuf;

        // print the file we are statting, and the return code from calling statx
        int src_statx_err = statx(AT_FDCWD,
                                  src_path,
                                  AT_EMPTY_PATH | AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW | AT_STATX_FORCE_SYNC,
                                  STATX_BASIC_STATS | STATX_BTIME | STATX_MNT_ID | STATX_DIOALIGN | STATX_MNT_ID_UNIQUE,
                                  &src_stxBuf);
        printf("statx result: %i\n", src_statx_err);
        if (src_statx_err < 0)
                return;

        // print the stx mask result
        printf("stx_mask: ");
        bitwisePrint(src_stxBuf.stx_mask);
        printf("\n");

        // if the stx buffer contains a time, print the time, else dont
        printTime("aTime        (Access)", src_stxBuf.stx_mask, STATX_ATIME, src_stxBuf.stx_atime);
        printTime("mTime        (Modify)", src_stxBuf.stx_mask, STATX_MTIME, src_stxBuf.stx_mtime);
        printTime("cTime        (Change)", src_stxBuf.stx_mask, STATX_CTIME, src_stxBuf.stx_ctime);
        printTime("crTime/bTime (Birth )", src_stxBuf.stx_mask, STATX_BTIME, src_stxBuf.stx_btime);

        /* ---- COPY FILE ---- */

        /*
        int byte_count, src_fd, dst_fd, copy_ret;
        byte_count = 4096; // how big does this need to be?
        unsigned char copy_buf[byte_count];

        // get file descriptors
        src_fd = open(src_path, O_RDONLY);                                         // get file descriptor from src path as read only
        dst_fd = open(dst_path, O_WRONLY | O_CREAT | O_EXCL | O_NOATIME, S_IRWXU); // get file descriptor from src path as write only, possibly create dir
        if (src_fd == -1 || dst_fd == -1)
                printf("Error getting File Descriptor.\n");

        close(src_fd);
        close(dst_fd);*/
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
void epochToString(char *dateStr, signed long long epoch) {
        struct tm tm;
        char epochAsstr[10];
        memset(&tm, 0, sizeof(struct tm));
        gfgIntToStr(epoch, epochAsstr);
        strptime(epochAsstr, "%s", &tm);
        strftime(dateStr, 255, "%Y-%m-%d %H:%M:%S", &tm);
}
void printStxTime(char *label, struct statx_timestamp stx_time) {
        char dateStr[255];
        char buffer[254] = "-";
        epochToString(&dateStr, stx_time.tv_sec);
        if (stx_time.tv_sec != 0)
                sprintf(buffer, "%s.%u", dateStr, stx_time.tv_nsec);
        printf("%s: %s \n", label, buffer);
}
void printExt4Time(struct ext4_time *ext4_time) {
        // get epoch as a formatted string
        char dateStr[255];
        epochToString(&dateStr, ext4_time->statx_time.tv_sec);
        printf("%s %s: %s.%u | %lld %llu | %lld", ext4_time->field, ext4_time->label, dateStr, ext4_time->statx_time.tv_nsec, ext4_time->epoch, ext4_time->extra, ext4_time->ns_epoch);
}
void fillInTime(struct ext4_time *ext4_time, char field[], char label[], struct statx_timestamp stx_time, unsigned int mask_const) {
        strcpy(ext4_time->label, label);
        strcpy(ext4_time->field, field);

        if (stx_mask & mask_const) {
                // put times into ext4_time struct
                long long adjusted = stx_time.tv_sec + 2147483648;                         // epoch adjusted to be a positive number?
                ext4_time->epoch = (adjusted % (4294967296)) - 2147483648;                 // the epoch adjusted to be a modifer for adjusted?
                ext4_time->extra = (stx_time.tv_nsec << 2) + floor(adjusted / 4294967296); // shift tv_nsec over by two bits store the epoch multiplier in the lower two bits
                ext4_time->ns_epoch = (stx_time.tv_sec * 1000000000) + stx_time.tv_nsec;   // the nanosecond unix epoch
                ext4_time->statx_time = stx_time;                                          // put the time from the result of stx into our struct

                // print ext4 time
                printExt4Time(ext4_time);

                // if the ns timestamp is the lowest ns timestamp, put into lowest variable
                if (ext4_time->ns_epoch < lowest_time.ns_epoch) {
                        printf(" | NEW LOWEST: %lld -> %lld", lowest_time.ns_epoch, ext4_time->ns_epoch);
                        lowest_time.statx_time = ext4_time->statx_time;
                        lowest_time.epoch = ext4_time->epoch;
                        lowest_time.extra = ext4_time->extra;
                        lowest_time.ns_epoch = ext4_time->ns_epoch;
                }
                printf("\n");
        } else {

                // mark this timestamp as one to be filled in with the lowest timestamp
                printf("%s: -\n", ext4_time->label);
                timestamps_to_fix[fix_ts_index] = ext4_time;
                fix_ts_index += 1;
        }
}
void get_b_time(struct statx_timestamp *dst_stx_time, struct statx *dst_stxBuf) {
        *dst_stx_time = dst_stxBuf->stx_btime;
}
void compare_times(char *label, struct ext4_time src, struct statx_timestamp dst_tv) {
        char src_sec_str[255] = "-";
        char dst_sec_str[255] = "-";
        epochToString(src_sec_str, src.epoch);
        epochToString(dst_sec_str, dst_tv.tv_sec);
        printf("%s src %s.%u | dst \033[%dm%s\033[0m.\033[%dm%u\033[0m\n", label, src_sec_str, src.statx_time.tv_nsec, src.epoch == dst_tv.tv_sec ? 92 : 91, dst_sec_str, src.statx_time.tv_nsec == dst_tv.tv_nsec ? 92 : 91, dst_tv.tv_nsec);
}

int main() {
        clock_t clock_start, clock_end;
        double cpu_time_used_seconds;
        long targetFileCount = 1347375;
        clock_start = clock();

        // ---- COPY FILE AND DATA ----
        // TODO: figure out the_bit_to_swap_out
        // if we are given a source directory, then the bit to swap out is just the source directory
        // if we are given a source file, then the bit to swap out is just everything up until before the source file name
        // char *src_path = "/home/zoey/Desktop/source/sourcefile.txt";
        // char *the_bit_to_swap_out = "/home/zoey/Desktop/source";
        char *src_path = "/media/zoey/DATA/SOURCE/sourcefile.txt";
        char *the_bit_to_swap_out = "/media/zoey/DATA/SOURCE";

        // df -T /media/zoey/DATA/SOURCE/sourcefile.txt | tail -n1 | cut -d' ' -f1
        char dev_path[] = "/dev/nvme0n1p2";

        /* ---- ENSURE DIRECTORIES EXIST ---- */
        // ENSURE dst_folder AND ALL PARENT DIRECTORIES EXIST
        char dst_folder[] = "/home/zoey/Desktop/test";
        char temp_path[COUNT(dst_folder)];
        // bitwisePrint(temp_path);
        // printf("\n");
        for (size_t i = 0; i < (COUNT(dst_folder)); i++) {
                // if the current character is not os seperator,  or not the end of the loop, skip
                if (dst_folder[i] != osSep && i != COUNT(dst_folder) - 1)
                        continue;
                // copy everything up to this character into a temporary variable
                strncpy(temp_path, dst_folder, i + 1);
                // set last bit of temp_path to null terminator to avoid reading junk data as part of the path
                temp_path[i + 1] = '\0';
                // create a directory using the string in temp path
                // if (mkdir(temp_path, S_IRWXU | S_IRWXG | S_IRWXO) == -1)
                //        printf("Error creating [%s]: \033[31m%s\033[0m\n", temp_path, strerror(errno));
                // else
                //        printf("Created path [\033[32m%s\033[0m]\n", temp_path);

                if (mkdir(temp_path, S_IRWXU | S_IRWXG | S_IRWXO) != -1)
                        printf("Created path [\033[32m%s\033[0m]\n", temp_path);
        }

        /* ---- GENERATE NEW PATH FOR DESTINATION FILE ----*/
        // GENERATE PATH TO COPY FILE TO
        char dst_path[1000] = "";
        strcpy(dst_path, src_path);

        // swap out the_bit_to_swap_out in src_path with dst_folder
        char string_before_substring[255] = "";
        char string_after_substring[255] = "";
        int match_position = strstr(dst_path, the_bit_to_swap_out) - dst_path;
        strncpy(string_before_substring, dst_path, match_position);
        strncpy(string_after_substring, dst_path + match_position + strlen(the_bit_to_swap_out), strlen(dst_path));
        sprintf(dst_path, "%s%s%s", string_before_substring, dst_folder, string_after_substring);

        // append current time to source directory

        // print source and destination path
        printf("Src path: \033[34m%s\033[0m\n", src_path);
        printf("Dst path: \033[35m%s\033[0m\n", dst_path);

        /* ---- COPY BYTES FROM SOURCE TO DESTINATION FILE
                the following only works on non directory files. if path was a directory, it would have been created by prior steps ----*/
        // get the file directories of the source and destination paths
        int src_fd, dst_fd;
        if ((src_fd = open(src_path, O_RDONLY)) == -1)
                printf("Error creating source File Descriptor: \033[31m%s\033[0m\n", strerror(errno));
        if ((dst_fd = open(dst_path, O_CREAT | O_WRONLY | O_TRUNC, 0644)) == -1)
                printf("Error creating destination File Descriptor: \033[31m%s\033[0m\n", strerror(errno));

        // get the filesize of src_fd
        // TODO: is it better to use stat or fstat here?
        struct stat src_stat;
        if ((fstat(src_fd, &src_stat)) == -1)
                printf("Error getting source file stat: \033[31m%s\033[0m\n", strerror(errno));

        // copy all the bytes from the source file to the destination file
        // TODO: figure out how to stop from updating the last data access timestamp of the file
        size_t byte_buf_size = LONG_MAX; // TODO: how big should this be?
        off_t copy_start_point = 0;
        printf("Copy %lu bytes from FD %i to FD %i, %lu bytes at a time\n", src_stat.st_size, src_fd, dst_fd, byte_buf_size);
        size_t loopcount = 0;
        while (copy_start_point < src_stat.st_size) {
                printf("Loop #%lu, copy start [%ld]\n", loopcount += 1, copy_start_point);
                if (sendfile(dst_fd, src_fd, &copy_start_point, byte_buf_size) == -1)
                        break;
        }
        close(src_fd);

        /* ---- COPY STATS FROM SOURCE TO DESTINATION ----*/
        // get stats of source file from statx
        struct statx src_stxBuf;
        int result = 0;
        result = statx(AT_FDCWD,
                       src_path,
                       AT_EMPTY_PATH | AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW | AT_STATX_FORCE_SYNC,
                       STATX_BASIC_STATS | STATX_BTIME | STATX_MNT_ID | STATX_DIOALIGN | STATX_MNT_ID_UNIQUE,
                       &src_stxBuf);
        if (result == -1)
                printf("Error getting statx of source file: \033[31m%s\033[0m\n", strerror(errno));

        // print the stx mask result
        stx_mask = src_stxBuf.stx_mask;
        printf("stx_mask of \033[34m%s\033[0m: ", src_path);
        bitwisePrint(stx_mask);
        printf("\n\n");

        // get all the times of the source file in our custom struct. mark how many need to be fixed and what timestamp is the lowest
        lowest_time.ns_epoch = INT64_MAX;
        fix_ts_index = 0;
        struct ext4_time target_a_time, target_m_time, target_c_time, target_b_time;
        fillInTime(&target_a_time, "atime", " (Access)", src_stxBuf.stx_atime, STATX_ATIME);
        fillInTime(&target_m_time, "mtime", " (Modify)", src_stxBuf.stx_mtime, STATX_MTIME);
        fillInTime(&target_c_time, "ctime", " (Change)", src_stxBuf.stx_ctime, STATX_CTIME);
        fillInTime(&target_b_time, "crtime", "(Birth )", src_stxBuf.stx_btime, STATX_BTIME);

        // if a timestamp is empty, give it the timestamp of the lowest filled timestamp
        for (size_t i = 0; i < fix_ts_index; i++) {
                struct ext4_time *timestamp_to_fix = timestamps_to_fix[i];
                printf("\033[32m");
                timestamp_to_fix->statx_time = lowest_time.statx_time;
                timestamp_to_fix->epoch = lowest_time.epoch;
                timestamp_to_fix->extra = lowest_time.extra;
                timestamp_to_fix->ns_epoch = lowest_time.ns_epoch;
                printExt4Time(timestamp_to_fix);
                printf("\033[0m\n");
        }

        /* ---- COPY OVER STATS ---- */
        // COPY ATIME and COPY MTIME
        // TODO: is utimensat or futimens faster?
        struct timespec times[2];
        // set up atime
        times[0].tv_sec = src_stxBuf.stx_atime.tv_sec;
        times[0].tv_nsec = src_stxBuf.stx_atime.tv_nsec;
        // set up mtime
        times[1].tv_sec = src_stxBuf.stx_mtime.tv_sec;
        times[1].tv_nsec = src_stxBuf.stx_mtime.tv_nsec;
        // put the new times in the file directed to be dst fd
        futimens(dst_fd, times);
        close(dst_fd);

        // COPY CTIME and COPY CRTIME
        // TODO: MAKE THIS NOT USE A SYTSTEM CALL TO DEBUGFS. DEBUGFS IS SLOW AND DOES NOT WORK ON NTFS STYLE DRIVES
        // get stats of destination file from statx
        struct statx dst_stxBuf;
        if ((statx(AT_FDCWD, dst_path, AT_EMPTY_PATH | AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW | AT_STATX_FORCE_SYNC, STATX_BASIC_STATS | STATX_BTIME | STATX_MNT_ID | STATX_DIOALIGN | STATX_MNT_ID_UNIQUE, &dst_stxBuf)) == -1)
                printf("Error getting statx of source file: \033[31m%s\033[0m\n", strerror(errno));
        unsigned long long inode = dst_stxBuf.stx_ino; // TODO: we only need the inode from this stxbuf, is there a faster way to get this?
        // set ctime

        // set crtime
        struct ext4_time target_ext4_time = target_b_time;

        char systembuf[255];
        char *field = &target_ext4_time.field;
        signed long long epoch = target_ext4_time.epoch;
        unsigned long long extra = target_ext4_time.extra;
        struct statx_timestamp target_stx_time = target_ext4_time.statx_time;
        // loop until the stats have been correctly coppied
        while (true) {
                // get the updated stx for destination to check it has been correctly changed
                if (statx(AT_FDCWD, dst_path, AT_EMPTY_PATH | AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW | AT_STATX_FORCE_SYNC, STATX_BASIC_STATS | STATX_BTIME | STATX_MNT_ID | STATX_DIOALIGN | STATX_MNT_ID_UNIQUE, &dst_stxBuf) == -1)
                        printf("Error getting statx of source file: \033[31m%s\033[0m\n", strerror(errno));

                // set values for this loop
                struct statx_timestamp current_stx_time = dst_stxBuf.stx_btime;

                // if the stat of source is same as dest, break
                if (target_stx_time.tv_sec == current_stx_time.tv_sec && target_stx_time.tv_nsec == current_stx_time.tv_nsec)
                        break;

                // print error message
                printf("\033[91mWUH OH. sif %s\033[0m\n", field);

                // put source epoch in destination inode
                sprintf(systembuf, "sudo debugfs -w -R 'set_inode_field <%llu> %s @0x%llx' %s", inode, field, epoch, dev_path); // debugfs -w -R set_inode_field <DST_INODE> FIELD @EPOCH DEV_PATH
                system(systembuf);
                // put source epoch_extra in destination inode
                sprintf(systembuf, "sudo debugfs -w -R 'set_inode_field <%llu> %s_extra 0x%llx' %s", inode, field, extra, dev_path); // debugfs -w -R set_inode_field <DST_INODE> FIELD_extra EXTRA DEV_PATH
                system(systembuf);
                // drop the caches to refresh the times stat gets
                system("sudo sync && sudo sysctl -w vm.drop_caches=3");
        }

        // DISPLAY THE source vs DST TIMES
        compare_times(" a_time:", target_a_time, dst_stxBuf.stx_atime);
        compare_times(" m_time:", target_m_time, dst_stxBuf.stx_mtime);
        compare_times(" c_time:", target_c_time, dst_stxBuf.stx_ctime);
        compare_times("cr_time:", target_b_time, dst_stxBuf.stx_btime);

        clock_end = clock();
        cpu_time_used_seconds = ((double)(clock_end - clock_start)) / CLOCKS_PER_SEC;
        double timePerAllFiles = (cpu_time_used_seconds * targetFileCount);
        printf("%f | An estimated %f min to process %li files\n", cpu_time_used_seconds, timePerAllFiles / 60, targetFileCount);

        return 0;
}
