#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <linux/fcntl.h>
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

void bitwisePrintOld(int var) {
        int var_copy = var;                                            // copy the input into a modifyable variable
        int mask_size = sizeof(var) * 8;                               // get the amount of bits in the input variable
        for (size_t bit = 0; bit < mask_size; bit++) {                 // itterate through every bit in the input variable
                bool is_one = var_copy & 1 << (mask_size - 1);         // check if the highest bit is one
                printf("\033[%im%u\033[0m", is_one ? 97 : 30, is_one); // print the bit
                var_copy = var_copy << 1;                              // put the next bit into the highest bit
        }
}

void bitwisePrint(void *var) {
        printf("%lu", sizeof var);
        printf("\n");
}

int main(int argc, char const *argv[]) {
        // 1
        bool tbool = true;
        bitwisePrint(tbool);

        // 8
        char tchar = 69;
        bitwisePrint(tchar);

        // 16
        int tint = 420;
        bitwisePrint(tint);

        // 32
        long tlong = 42069;
        bitwisePrint(tlong);

        // 64
        long long tllong = 6942069;
        bitwisePrint(tllong);

        // ???
        float tfloat;
        bitwisePrint(tfloat);
        double tdouble;
        bitwisePrint(tdouble);
        long double tldouble;
        bitwisePrint(tldouble);

        return 0;
}
