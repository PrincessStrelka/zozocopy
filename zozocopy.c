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

char os_sep;

void ensureOsSeperator(char *s) {
        if (s[strlen(s) - 1] != os_sep) {
                // https://www.geeksforgeeks.org/how-to-append-a-character-to-a-string-in-c/
                while (*s++)
                        ;          // Move pointer to the end
                *(s - 1) = os_sep; // Append the new character
                *s = '\0';         // Add null terminator to mark new end
        }
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

void get_stx_btime(struct statx_timestamp *dst_stx_time, struct statx *dst_stxBuf) {
        *dst_stx_time = dst_stxBuf->stx_btime;
}
void get_stx_ctime(struct statx_timestamp *dst_stx_time, struct statx *dst_stxBuf) {
        *dst_stx_time = dst_stxBuf->stx_ctime;
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

void print_seconds(double seconds) {
        int hour = seconds / 3600;
        seconds -= hour * 3600;
        int mins = seconds / 60;
        printf("%dh %dm %fs", hour, mins, seconds - mins * 60);
}

struct profile {
        char label[64];
        long total_usec;
        int count;
};

struct profile get_stats_profile = {.label = "get stats from source file"};
struct profile timecopy_profile = {.label = "a and m time"};
struct profile debugfs_profile = {.label = "c and b/cr time"};
void add_profiling(struct profile *prof, struct timeval *stop, struct timeval *start) {
        gettimeofday(stop, NULL);
        prof->total_usec += (stop->tv_sec * 1000000 + stop->tv_usec) - (start->tv_sec * 1000000 + start->tv_usec);
        prof->count += 1;
        gettimeofday(start, NULL);
};

void travel_directory_and_clone(char src_dir[], char src_dir_parent[], char base_dst_dir[], long *items_progress, long total_items, char *dev_path, long *time_taken) {
        // ensure that src_dir has an os seperator on the end
        ensureOsSeperator(src_dir);

        // generate dst_dir by swapping src_dir_parent in src_dir with base_dst_dir
        char dst_dir[100];
        sprintf(dst_dir, "%s%s", base_dst_dir, src_dir + (strlen(src_dir_parent) - 1));

        // ensure the dst directory exists
        mkdir(dst_dir, S_IRWXU | S_IRWXG | S_IRWXO);

        // itterate through src dir directory
        DIR *opened_src_dir = opendir(src_dir); // returns DIR struct upon success, NULL upon failure
        if (!opened_src_dir)                    // if sourcedir is not a directory, exit
                return;
        struct dirent *src_dirent;
        while ((src_dirent = readdir(opened_src_dir)) != NULL) {
                // get the file name from dp // the variable for the current file
                char *file_name = src_dirent->d_name;

                // concatonate the source path and file name together into a new variable
                char src_path[255];
                sprintf(src_path, "%s%s", src_dir, file_name);

                // if we are on a refrence to the current(.) or parent(..) directory, or if we are unable to get stat from the filepath. skip this loop
                struct stat src_path_stat;
                if (strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0 || stat(src_path, &src_path_stat) != 0)
                        continue;

                // update and output item progress
                *items_progress += 1;

                // print percentage done
                double percent_done = (double)*items_progress / (double)total_items;
                printf("[%6.2f%] ", percent_done * 100.0);

                // print progress bar
                int bar_filled = 30 * percent_done;
                printf("[\033[33m%.*s\033[0m\033[3m%.*s\033[0m] ", bar_filled, "########################################", 30 - bar_filled, "----------------------------------------");

                // determine what the item is
                if (src_path_stat.st_mode & __S_IFDIR) {
                        // its a directory

                        // TODO: copy over src dir times to dst dir

                        // print where we are reletively
                        printf("[\033[36m%s\033[0m]\n", src_dir + (strlen(src_dir_parent) - 1));

                        // go into the directory in source path
                        travel_directory_and_clone(src_path, src_dir_parent, base_dst_dir, items_progress, total_items, dev_path, time_taken);
                } else if (src_path_stat.st_mode & __S_IFREG) {
                        // its a file
                        // generate dst_file_path
                        char dst_path[255];
                        sprintf(dst_path, "%s%s", base_dst_dir, src_path + (strlen(src_dir_parent) - 1));

                        /* ----- copy file ----- */
                        // get the file directories of the source and destination paths
                        int src_fd, dst_fd;
                        if ((src_fd = open(src_path, O_RDONLY)) == -1)
                                printf("Error creating source File Descriptor: \033[31m%s\033[0m\n", strerror(errno));
                        if ((dst_fd = open(dst_path, O_CREAT | O_WRONLY | O_TRUNC, 0644)) == -1)
                                printf("Error creating destination File Descriptor: \033[31m%s\033[0m\n", strerror(errno));

                        // copy all the bytes from the source file to the destination file
                        off_t copy_start_point = 0;
                        while (copy_start_point < src_path_stat.st_size)
                                if (sendfile(dst_fd, src_fd, &copy_start_point, LONG_MAX) == -1)
                                        break;
                        close(src_fd);

                        /* ----- get the file stats ----- */
                        struct timeval prof_stop, prof_start;
                        gettimeofday(&prof_start, NULL);

                        // get stats of source file from statx
                        struct statx src_stxBuf;
                        if (statx(AT_FDCWD, src_path, AT_EMPTY_PATH | AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW | AT_STATX_FORCE_SYNC, STATX_BASIC_STATS | STATX_BTIME | STATX_MNT_ID | STATX_DIOALIGN | STATX_MNT_ID_UNIQUE, &src_stxBuf))
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

                        // ---- fix timestamps ----
                        // we are copying from an ntfs drive so we know crtime will always be the empty one
                        // if a timestamp is empty, give it the timestamp of the lowest filled timestamp
                        for (size_t i = 0; i < ts_fix_index; i++) {
                                struct ext4_time *timestamp_to_fix = ts_to_fix[i];
                                timestamp_to_fix->statx_time = lowest_time.statx_time;
                                timestamp_to_fix->epoch = lowest_time.epoch;
                                timestamp_to_fix->extra = lowest_time.extra;
                                timestamp_to_fix->ns_epoch = lowest_time.ns_epoch;
                        }
                        printf("]");
                        add_profiling(&get_stats_profile, &prof_stop, &prof_start);

                        // ---- write the file stats ----

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
                        add_profiling(&timecopy_profile, &prof_stop, &prof_start);

                        // ---- DEBUGFS STAT COPY ----
                        // sudo debugfs -w -R 'set_inode_field <DESTINATION_INODE> crtime @0xEPOCH' DEV_PATH
                        // sudo debugfs -w -R 'set_inode_field <DESTINATION_INODE> crtime_extra 0x%EXTRA' DEV_PATH

                        // COPY CTIME
                        debugfs_copy_time(target_c_time, dst_path, dev_path, get_stx_ctime);

                        // COPY CRTIME
                        debugfs_copy_time(target_b_time, dst_path, dev_path, get_stx_btime);

                        add_profiling(&debugfs_profile, &prof_stop, &prof_start);
                        printf("]\n");
                } else {
                        printf("%s | it's a something else \n", src_path);
                }
        }
        closedir(opened_src_dir); // closes the sourceDir DIR struct
}

int main() {
        struct timeval total_stop, total_start;
        gettimeofday(&total_start, NULL);

        /* ---- ARGS ---- */
        char base_src_dir[] = "/media/zoey/DATA/SOURCE";
        char *base_dst_dir = "/home/zoey/Desktop/dest";
        char dev_path[] = "/dev/nvme0n1p2"; // TODO: can we get dev_path programatically? or figure out how to not need it at all?
        os_sep = '/';                       // TODO: can we get os seperator programatically?
        long target_time_mins = 30;
        long target_item_count = 1347375;

        /* ---- figure out the deepest directory in the source dir path ---- */
        size_t last_char_index = 0;
        for (size_t letter_index = 0; letter_index < strlen(base_src_dir); letter_index++) {
                //  if we are an os seperator, mark our position
                if (base_src_dir[letter_index] == os_sep)
                        last_char_index = letter_index;
        }
        char src_dir_parent[1000];
        strncpy(src_dir_parent, base_src_dir, last_char_index);

        // create the dest dir inside the dest dir base
        char dst_dir[100];
        sprintf(dst_dir, "%s%s", base_dst_dir, base_src_dir + (strlen(src_dir_parent) - 1));

        // ensure that dest dir and all its parent directories exist
        char dst_dir_chunk[COUNT(dst_dir)];
        for (size_t letter_index = 0; letter_index < (COUNT(dst_dir)); letter_index++) {
                // if we arent at an os seperator or the end of dst_dir, skip this loop
                if (dst_dir[letter_index] != os_sep && letter_index != COUNT(dst_dir) - 1)
                        continue;
                // copy everything before and including character into the temporary path
                strncpy(dst_dir_chunk, dst_dir, letter_index + 1);
                // and then add a null terminator to avoid reading junk data as part of the path
                dst_dir_chunk[letter_index + 1] = '\0';
                // try to create a directory using temp_path, and set if a dir has been made
                if (mkdir(dst_dir_chunk, S_IRWXU | S_IRWXG | S_IRWXO) != -1) {
                        printf("DIRECTORY HAS BEEN MADE %s\n", dst_dir_chunk);
                }
        }

        // figure out how many items we will be processing
        long total_items = 0;
        calc_dir_items(base_src_dir, &total_items);
        printf("Copying %ld Items from \033[34m%s\033[0m to \033[35m%s\033[0m\n", total_items, base_src_dir, base_dst_dir);

        // itterate through source dir and its subdirectories, and copy any item you find
        long items_progress = 0;
        long time_taken = 0;
        travel_directory_and_clone(base_src_dir, src_dir_parent, base_dst_dir, &items_progress, total_items, dev_path, &time_taken);
        gettimeofday(&total_stop, NULL);

        // print stuff on completion
        printf("\n");
        double total_time_usec = (double)((total_stop.tv_sec * 1000000 + total_stop.tv_usec) - (total_start.tv_sec * 1000000 + total_start.tv_usec));
        double usec_per_item = total_time_usec / (double)total_items;
        printf("Completed in \033[94m%f\033[0ms at \033[94m%f\033[0ms/i\n", total_time_usec / 1000000, usec_per_item / 1000000);

        double usec_per_item_target = ((double)target_time_mins * 60 * 1000000) / (double)target_item_count;
        double sec_per_item_dif = (usec_per_item - usec_per_item_target) / 1000000;
        printf("The program is running \033[%im%f\033[0ms %s of the target speed [\033[94m%f\033[0ms/i] \n", sec_per_item_dif >= 0 ? 31 : 32, sec_per_item_dif, sec_per_item_dif >= 0 ? "behind" : "ahead", usec_per_item_target / 1000000);
        printf("\033[93m%ld\033[0m items would be completed in \033[31m", target_item_count);
        print_seconds(((total_time_usec / (double)total_items) * (double)target_item_count) / 1000000);
        printf("\033[0m. The target time to complete is \033[93m");
        print_seconds(target_time_mins * 60);
        printf("\033[0m\n");

        printf("Done! 🎉\n\n");
        char system_buf[255];
        sprintf(system_buf, "stat /home/zoey/Desktop/dest/SOURCE/test.png", base_src_dir);
        system("stat /media/zoey/DATA/SOURCE/test.png");
        printf("\n");
        system("stat /home/zoey/Desktop/dest/SOURCE/test.png");
        printf("\n");

        printf("%s: %f\n", get_stats_profile.label, ((double)get_stats_profile.total_usec / get_stats_profile.count) / 1000000);
        printf("%s: %f\n", timecopy_profile.label, ((double)timecopy_profile.total_usec / timecopy_profile.count) / 1000000);
        printf("%s: %f\n", debugfs_profile.label, ((double)debugfs_profile.total_usec / debugfs_profile.count) / 1000000);
        return 0;
}
