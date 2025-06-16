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

struct profile {
        char label[64];
        long total_usec;
        int count;
};

struct profile stat_profile = {.label = "stat"};
struct profile fstat_profile = {.label = "fstat"};
struct profile fstat2_profile = {.label = "fstat with a prexisting file descriptor"};
struct profile statx_profile = {.label = "statx"};

void add_profiling(struct profile *prof, struct timeval *stop, struct timeval *start) {
        gettimeofday(stop, NULL);
        prof->total_usec += (stop->tv_sec * 1000000 + stop->tv_usec) - (start->tv_sec * 1000000 + start->tv_usec);
        prof->count += 1;
        gettimeofday(start, NULL);
};

int main(int argc, char const *argv[]) {
        char *path = "/home/zoey/Desktop/dest/SOURCE/test.png";
        unsigned long long dst_inode;
        // start profiling
        struct timeval prof_stop, prof_start;
        gettimeofday(&prof_start, NULL);

        // profile stat
        add_profiling(&stat_profile, &prof_stop, &prof_start);

        // profile fstat
        add_profiling(&fstat_profile, &prof_stop, &prof_start);

        // profile fstat 2
        add_profiling(&fstat2_profile, &prof_stop, &prof_start);

        // profile statx
        struct statx dst_stxBuf;
        if ((statx(AT_FDCWD, path, AT_EMPTY_PATH | AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW | AT_STATX_FORCE_SYNC, STATX_BASIC_STATS | STATX_BTIME | STATX_MNT_ID | STATX_DIOALIGN | STATX_MNT_ID_UNIQUE, &dst_stxBuf)) == -1)
                printf("Error getting statx of source file: \033[31m%s\033[0m\n", strerror(errno));
        dst_inode = dst_stxBuf.stx_ino;
        add_profiling(&statx_profile, &prof_stop, &prof_start);

        printf("%s: %fs\n", stat_profile.label, ((double)stat_profile.total_usec / stat_profile.count) / 1000000);
        printf("%s: %fs\n", fstat_profile.label, ((double)fstat_profile.total_usec / fstat_profile.count) / 1000000);
        printf("%s: %fs\n", fstat2_profile.label, ((double)fstat2_profile.total_usec / fstat2_profile.count) / 1000000);
        printf("%s: %fs\n", statx_profile.label, ((double)statx_profile.total_usec / statx_profile.count) / 1000000);
        return 0;
}
