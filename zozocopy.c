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
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define COUNT(arr) (sizeof(arr) / sizeof(*arr))
#define nullptr NULL

struct ext4_time {
        char field[64];
        struct statx_timestamp statx_time;
        signed long long epoch;
        unsigned long long extra;
        signed long long ns_epoch;
};

// my global variables
char os_sep = '/';

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
        if (s[strlen(s) - 1] != os_sep) {
                gfgAddChar(s, os_sep);
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
        char *lastOsSepIndex = strrchr(copyBaseFolder, os_sep);
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
        printf("%s %s: %s.%u | %lld %llu | %lld", ext4_time->field, ext4_time->field, dateStr, ext4_time->statx_time.tv_nsec, ext4_time->epoch, ext4_time->extra, ext4_time->ns_epoch);
}
void get_stx_btime(struct statx_timestamp *dst_stx_time, struct statx *dst_stxBuf) {
        *dst_stx_time = dst_stxBuf->stx_btime;
}
void get_stx_ctime(struct statx_timestamp *dst_stx_time, struct statx *dst_stxBuf) {
        *dst_stx_time = dst_stxBuf->stx_ctime;
}
void fill_in_time(struct ext4_time *ts_to_fix[], size_t *times_to_fix_index, struct ext4_time *lowest_time, unsigned int stx_mask, struct ext4_time *target_time, struct statx_timestamp stx_time, unsigned int mask_const, char ino_field[]) {
        // put inode field in struct
        strcpy(target_time->field, ino_field);

        // if mask has mask const
        if (stx_mask & mask_const) {
                target_time->statx_time = stx_time; // put stx time into struct

                // put times into ext4_time struct
                long long adjusted = stx_time.tv_sec + 2147483648;                           // epoch adjusted to be a positive number?
                target_time->epoch = (adjusted % (4294967296)) - 2147483648;                 // the epoch adjusted to be a modifer for adjusted?
                target_time->extra = (stx_time.tv_nsec << 2) + floor(adjusted / 4294967296); // shift tv_nsec over by two bits store the epoch multiplier in the lower two bits
                target_time->ns_epoch = (stx_time.tv_sec * 1000000000) + stx_time.tv_nsec;   // the nanosecond unix epoch

                // if the ns timestamp is the lowest ns timestamp, put into lowest variable
                if (target_time->ns_epoch < lowest_time->ns_epoch)
                        *lowest_time = *target_time;

                // print that this timestamp is good
                printf("\033[32m%s\033[0m ", target_time->field);

                return;
        }
        // mark this target_time as one to fix
        ts_to_fix[*times_to_fix_index] = target_time;
        *times_to_fix_index += 1;

        // print that this timestamp is bad
        printf("\033[31m%s\033[0m ", target_time->field);
}
void calc_dir_items(char src_dir[], long *total_files) {
        ensureOsSeperator(src_dir);
        // printf("%s\n", src_dir);

        struct dirent *dp;
        DIR *opened_src_dir = opendir(src_dir); // returns DIR struct upon success, NULL upon failure
        char *file_name;                        // the variable for the current file
        char file_path_name[255];               // the variable to store the file name + file path
        if (!opened_src_dir)                    // if sourcedir is not a directory, exit
                return;
        struct stat forps;
        while ((dp = readdir(opened_src_dir)) != NULL) {
                // get the file name from dp
                file_name = dp->d_name;

                // if we are on a refrence to the current(.) or parent(..)
                // directory, skip this loop
                if (strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0)
                        continue;

                // concatonate the source path and file name together into a new variable
                sprintf(file_path_name, "%s%s", src_dir, file_name);

                // find if the filepath is a directory or not
                if (stat(file_path_name, &forps) == 0) {
                        *total_files += 1;
                        if (forps.st_mode & __S_IFDIR) {
                                // its a directory
                                calc_dir_items(file_path_name, total_files);
                        } else if (forps.st_mode & __S_IFREG) {
                                // its a file
                                // printf("\033[34m%s\033[0m\n", file_path_name);
                        } else {
                                printf("%s | it's a something else \n", file_path_name);
                        }
                } else {
                        printf("%s | unable to get stat \n", file_path_name);
                }
        }
        closedir(opened_src_dir); // closes the sourceDir DIR struct
}

void debugfs_copy_time(struct ext4_time target_time, char *dst_path, char *dev_path, void (*get_current_target_time_in_stx)(struct statx_timestamp *, struct statx *)) {
        // get stats of src time for calling debugfs
        // get inode of dst file
        // setup fields needed for calling debugfs
        char epoch_buf[255];
        char extra_buf[255];
        char *field;
        signed long long epoch;
        unsigned long long extra;
        struct statx_timestamp target_stx_time;
        struct statx dst_stxBuf;

        // TODO: we only need the inode from this stxbuf, is there a faster way to get this?
        if ((statx(AT_FDCWD, dst_path, AT_EMPTY_PATH | AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW | AT_STATX_FORCE_SYNC, STATX_BASIC_STATS | STATX_BTIME | STATX_MNT_ID | STATX_DIOALIGN | STATX_MNT_ID_UNIQUE, &dst_stxBuf)) == -1)
                printf("Error getting statx of source file: \033[31m%s\033[0m\n", strerror(errno));
        unsigned long long dst_inode = dst_stxBuf.stx_ino;
        field = &target_time.field;
        epoch = target_time.epoch;
        extra = target_time.extra;
        target_stx_time = target_time.statx_time;
        // void (*get_current_target_time_in_stx)(struct statx_timestamp *, struct statx *) = get_stx_ctime;

        // generate the debugfs calls
        sprintf(epoch_buf, "sudo debugfs -w -R 'set_inode_field <%llu> %stime @0x%llx' %s >nul 2>nul", dst_inode, field, epoch, dev_path);      // debugfs -w -R set_inode_field <DST_INODE> FIELD @EPOCH DEV_PATH
        sprintf(extra_buf, "sudo debugfs -w -R 'set_inode_field <%llu> %stime_extra 0x%llx' %s >nul 2>nul", dst_inode, field, extra, dev_path); // debugfs -w -R set_inode_field <DST_INODE> FIELD_extra EXTRA DEV_PATH

        // TODO: remove this bodge where i just need to loop until it has been successful
        // TODO: MAKE THIS NOT USE A SYTSTEM CALL TO DEBUGFS. DEBUGFS IS SLOW AND DOES NOT WORK ON NTFS STYLE DRIVES
        // loop until the stats have been correctly coppied
        while (true) {
                // update dst stx buf
                if ((statx(AT_FDCWD, dst_path, AT_EMPTY_PATH | AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW | AT_STATX_FORCE_SYNC, STATX_BASIC_STATS | STATX_BTIME | STATX_MNT_ID | STATX_DIOALIGN | STATX_MNT_ID_UNIQUE, &dst_stxBuf)) == -1)
                        printf("Error getting statx of source file: \033[31m%s\033[0m\n", strerror(errno));

                // get what the target time field in the dst stx buf currently is
                struct statx_timestamp dst_current_target_time;
                get_current_target_time_in_stx(&dst_current_target_time, &dst_stxBuf);

                // check if the times match eachother
                if (target_stx_time.tv_sec == dst_current_target_time.tv_sec && target_stx_time.tv_nsec == dst_current_target_time.tv_nsec) {
                        printf("\033[92m%s\033[0m ", field);
                        break;
                } else
                        printf("\033[91m%s\033[0m ", field);

                // put source epoch in destination inode
                system(epoch_buf);

                // put source epoch_extra in destination inode
                system(extra_buf);

                // drop the caches to refresh the times stat gets
                system("sudo sync && sudo sysctl -w vm.drop_caches=3 >nul 2>nul");
        }
}

void travel_dir(char src_dir[], char src_dir_parent[], char base_dst_dir[], long *items_progress, long total_items, char *dev_path, long *time_taken) {
        clock_t start;
        clock_t end;
        ensureOsSeperator(src_dir);
        // swap out src_dir_parent in src_dir with base_dst_dir /media/zoey/DATA
        char dst_dir[100];
        sprintf(dst_dir, "%s%s", base_dst_dir, src_dir + (strlen(src_dir_parent) - 1));

        // ensure the dst directory exists
        // itterate over every letter of dst_dir
        char dst_dir_chunk[COUNT(dst_dir)];
        bool dir_been_made = false;
        for (size_t letter_index = 0; letter_index < (COUNT(dst_dir)); letter_index++) {
                // if we arent at an os seperator or the end of dst_dir, skip this loop
                if (dst_dir[letter_index] != os_sep && letter_index != COUNT(dst_dir) - 1)
                        continue;
                // copy everything before and including character into the temporary path
                strncpy(dst_dir_chunk, dst_dir, letter_index + 1);
                // and then add a null terminator to avoid reading junk data as part of the path
                dst_dir_chunk[letter_index + 1] = '\0';
                // try to create a directory using temp_path, and set if a dir has been made
                if (mkdir(dst_dir_chunk, S_IRWXU | S_IRWXG | S_IRWXO) != -1)
                        dir_been_made = true;
        }

        // TODO: copy over src dir times to dst dir
        struct dirent *dp;
        DIR *opened_src_dir = opendir(src_dir); // returns DIR struct upon success, NULL upon failure
        char *file_name;                        // the variable for the current file
        char src_file_path[255];                // the variable to store the file name + file path
        if (!opened_src_dir)                    // if sourcedir is not a directory, exit
                return;
        struct stat forps;
        while ((dp = readdir(opened_src_dir)) != NULL) {

                // get the file name from dp
                file_name = dp->d_name;

                // if we are on a refrence to the current(.) or parent(..)
                // directory, skip this loop
                if (strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0)
                        continue;

                // concatonate the source path and file name together into a new variable
                sprintf(src_file_path, "%s%s", src_dir, file_name);

                // start timer for the processing time of the loop
                struct timeval stop, start;
                gettimeofday(&start, NULL);

                // find if the filepath is a directory or not
                if (stat(src_file_path, &forps) == 0) {
                        // update and output item progress
                        *items_progress += 1;
                        double percent_done = (double)*items_progress / (double)total_items;
                        int progress_bar_total = 20;
                        int bar_filled = progress_bar_total * percent_done;
                        int bar_unfilled = progress_bar_total - bar_filled;

                        // print progress bar
                        printf("[\033[33m%.*s\033[0m%.*s] ", bar_filled, "####################", bar_unfilled, "--------------------");
                        // print percentage done
                        printf("[%6.2f%] ", percent_done * 100.0);
                        // print amount done
                        printf("[%ld/%ld] ", *items_progress, total_items);

                        // OUTPUT IF A DIR HAS BEEN MADE
                        if (dir_been_made) {
                                printf("[\033[32mD\033[0m] ");
                                dir_been_made = false;
                        } else
                                printf("[\033[30mD\033[0m] ");

                        // determine what the item is
                        if (forps.st_mode & __S_IFDIR) {
                                // its a directory
                                printf("[Dir] ");

                                // print eta
                                gettimeofday(&stop, NULL);
                                *time_taken += (stop.tv_sec * 1000000 + stop.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec);
                                double seconds = (((*time_taken / (double)*items_progress) * total_items) - *time_taken) / 1000000;
                                int hour = seconds / 3600;
                                seconds -= hour * 3600;
                                int mins = seconds / 60;
                                printf("[\033[94meta: %d:%d:%.2f\033[0m] ", hour, mins, seconds - mins * 60);
                                gettimeofday(&start, NULL);

                                // travel the directory
                                printf("\n");
                                travel_dir(src_file_path, src_dir_parent, base_dst_dir, items_progress, total_items, dev_path, time_taken);
                        } else if (forps.st_mode & __S_IFREG) {
                                // its a file
                                printf("[Fil] ");

                                // generate dst_file_path
                                char dst_path[255];
                                sprintf(dst_path, "%s%s", base_dst_dir, src_file_path + (strlen(src_dir_parent) - 1));

                                /* ----- copy file ----- */
                                // get the file directories of the source and destination paths
                                int src_fd, dst_fd;
                                if ((src_fd = open(src_file_path, O_RDONLY)) == -1)
                                        printf("Error creating source File Descriptor: \033[31m%s\033[0m\n", strerror(errno));
                                if ((dst_fd = open(dst_path, O_CREAT | O_WRONLY | O_TRUNC, 0644)) == -1)
                                        printf("Error creating destination File Descriptor: \033[31m%s\033[0m\n", strerror(errno));

                                // get the filesize of src_fd
                                struct stat src_stat;
                                if ((fstat(src_fd, &src_stat)) == -1) // TODO: is it better to use stat or fstat here?
                                        printf("Error getting source file stat: \033[31m%s\033[0m\n", strerror(errno));

                                // copy all the bytes from the source file to the destination file
                                size_t byte_buf_size = LONG_MAX;
                                off_t copy_start_point = 0;
                                while (copy_start_point < src_stat.st_size) {
                                        if (sendfile(dst_fd, src_fd, &copy_start_point, byte_buf_size) == -1)
                                                break;
                                }
                                close(src_fd);

                                /* ----- get the file stats ----- */
                                // get stats of source file from statx
                                struct statx src_stxBuf;
                                if (statx(AT_FDCWD, src_file_path, AT_EMPTY_PATH | AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW | AT_STATX_FORCE_SYNC, STATX_BASIC_STATS | STATX_BTIME | STATX_MNT_ID | STATX_DIOALIGN | STATX_MNT_ID_UNIQUE, &src_stxBuf))
                                        printf("Error getting statx of source file: \033[31m%s\033[0m\n", strerror(errno));
                                unsigned int stx_mask = src_stxBuf.stx_mask;

                                // get all the times of the source file in our custom struct. mark how many need to be fixed and what timestamp is the lowest
                                printf("[Fix: ");
                                struct ext4_time target_a_time, target_m_time, target_c_time, target_b_time;
                                struct ext4_time lowest_time = {.ns_epoch = INT64_MAX};
                                size_t ts_fix_index = 0;
                                struct ext4_time *ts_to_fix[4] = {};
                                fill_in_time(&ts_to_fix, &ts_fix_index, &lowest_time, stx_mask, &target_a_time, src_stxBuf.stx_atime, STATX_ATIME, "a");
                                fill_in_time(&ts_to_fix, &ts_fix_index, &lowest_time, stx_mask, &target_m_time, src_stxBuf.stx_mtime, STATX_MTIME, "m");
                                fill_in_time(&ts_to_fix, &ts_fix_index, &lowest_time, stx_mask, &target_c_time, src_stxBuf.stx_ctime, STATX_CTIME, "c");
                                fill_in_time(&ts_to_fix, &ts_fix_index, &lowest_time, stx_mask, &target_b_time, src_stxBuf.stx_btime, STATX_BTIME, "cr");
                                printf("]");

                                // if a timestamp is empty, give it the timestamp of the lowest filled timestamp
                                for (size_t i = 0; i < ts_fix_index; i++) {
                                        struct ext4_time *timestamp_to_fix = ts_to_fix[i];
                                        timestamp_to_fix->statx_time = lowest_time.statx_time;
                                        timestamp_to_fix->epoch = lowest_time.epoch;
                                        timestamp_to_fix->extra = lowest_time.extra;
                                        timestamp_to_fix->ns_epoch = lowest_time.ns_epoch;
                                }

                                /* ---- write the file stats ---- */
                                printf(" [Set: ");
                                // COPY ATIME and COPY MTIME
                                struct timespec atime_mtime[2];
                                // set up atime
                                atime_mtime[0].tv_sec = src_stxBuf.stx_atime.tv_sec;
                                atime_mtime[0].tv_nsec = src_stxBuf.stx_atime.tv_nsec;
                                // set up mtime
                                atime_mtime[1].tv_sec = src_stxBuf.stx_mtime.tv_sec;
                                atime_mtime[1].tv_nsec = src_stxBuf.stx_mtime.tv_nsec;
                                //  put the new times in the file directed to be dst fd
                                printf("\033[92m%s\033[0m ", target_a_time.field);
                                printf("\033[92m%s\033[0m ", target_m_time.field);
                                // TODO: is utimensat or futimens faster?
                                futimens(dst_fd, atime_mtime);
                                close(dst_fd);

                                // COPY CTIME
                                debugfs_copy_time(target_c_time, dst_path, dev_path, get_stx_ctime);

                                // COPY CRTIME
                                debugfs_copy_time(target_b_time, dst_path, dev_path, get_stx_btime);
                                printf("] ");

                                // print eta
                                gettimeofday(&stop, NULL);
                                *time_taken += (stop.tv_sec * 1000000 + stop.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec);
                                double seconds = (((*time_taken / (double)*items_progress) * total_items) - *time_taken) / 1000000;
                                int hour = seconds / 3600;
                                seconds -= hour * 3600;
                                int mins = seconds / 60;
                                printf("[\033[94meta: %d:%d:%.2f\033[0m] ", hour, mins, seconds - mins * 60);
                                gettimeofday(&start, NULL);

                                printf("\n");
                        } else {
                                printf("%s | it's a something else \n", src_file_path);
                        }

                } else {
                        printf("%s | unable to get stat \n", src_file_path);
                }
        }
        closedir(opened_src_dir); // closes the sourceDir DIR struct
}

int main() {
        clock_t program_start = clock();
        long target_file_count = 1347375;

        /* ---- ARGS ---- */
        // TODO: can we get dev_path programatically? or figure out how to not need it at all?
        char dev_path[] = "/dev/nvme0n1p2";
        char src_dir[] = "/media/zoey/DATA/SOURCE";
        char *base_dst_dir = "/home/zoey/Desktop/dest";

        /* ---- figure out the source dir parent ---- */
        char src_dir_parent[1000];
        size_t last_char_index = 0;
        for (size_t letter_index = 0; letter_index < strlen(src_dir); letter_index++) {
                //  if we are an os seperator, mark our position
                if (src_dir[letter_index] == os_sep)
                        last_char_index = letter_index;
        }
        strncpy(src_dir_parent, src_dir, last_char_index);

        /* ---- ensure src dir top in dst dir exists ---- */
        printf("\033[34m%s\033[0m\n", src_dir);
        printf("%s\n", src_dir_parent);
        printf("\033[35m%s\033[0m\n", base_dst_dir);

        /* ---- Figure out how many items we will be processing ---- */
        long total_items = 0;
        calc_dir_items(src_dir, &total_items);

        /* ---- itterate through every file in src dir and subdirs ---- */
        printf("%ld Items\n", total_items);
        long items_progress = 0;
        long time_taken = 0;
        travel_dir(src_dir, src_dir_parent, base_dst_dir, &items_progress, total_items, dev_path, &time_taken);

        printf("\n");
        // printf("Total program run time: %fs\n", (double)(clock() - program_start) / CLOCKS_PER_SEC);
        double seconds = ((time_taken / 1000000) / (double)total_items) * target_file_count;
        int hour = seconds / 3600;
        seconds -= hour * 3600;
        int mins = seconds / 60;
        printf("Estimated time to complete target [%ld] Files: %d:%d:%f\n", target_file_count, hour, mins, seconds - mins * 60);
        printf("Done! 🎉\n\n");
        // system("stat /home/zoey/Desktop/dest/SOURCE/test.png");
        return 0;
}
