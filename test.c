// C Program to illustrate the system function
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
        // giving system command and storing return value
        unsigned long long inode = 1317395;
        char field[] = "crtime";
        signed long long pre_epoch = 1743646532;
        unsigned long long pre_extra = 3040029600;
        char dev_path[] = "{DEV PATH HERE}";

        clock_t start, end;
        double cpu_time_used;

        int total_loops;
        double avg_hex_time;
        double avg_str_time;

        for (size_t i = 0; i < 1000; i++) {

                total_loops += 1;
                // hex
                start = clock();

                char bufHex[255];
                sprintf(bufHex, "debugfs -w -R set_inode_field <%llu> %s @%llx %s", inode, field, pre_epoch, dev_path);
                // printf("%s\n", bufHex);

                end = clock();
                cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
                avg_hex_time += cpu_time_used;
                // printf("%f\n", cpu_time_used);

                // date string
                start = clock();

                char epoch_date_string[255];
                signed long long epoch_date = pre_epoch;
                struct tm tm;
                char epochAsstr[10];
                memset(&tm, 0, sizeof(struct tm));
                char *str = epochAsstr;
                int i = 0;
                int sign = epoch_date;
                char bufDat[255];

                if (epoch_date < 0)
                        epoch_date = -epoch_date;

                while (epoch_date > 0) {
                        str[i++] = epoch_date % 10 + '0';
                        epoch_date /= 10;
                }
                if (sign < 0)
                        str[i++] = '-';
                str[i] = '\0';
                for (int j = 0, k = i - 1; j < k; j++, k--) {
                        char temp = str[j];
                        str[j] = str[k];
                        str[k] = temp;
                }

                strptime(epochAsstr, "%s", &tm);
                strftime(epoch_date_string, 255, "%Y%m%d%H%M%S", &tm);
                sprintf(bufDat, "debugfs -w -R set_inode_field <%llu> %s %s %s", inode, field, epoch_date_string, dev_path);
                // printf("%s\n", bufDat);

                end = clock();
                cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
                avg_str_time += cpu_time_used;
                // printf("%f\n", cpu_time_used);
        }
        // printf("debugfs -w -R set_inode_field <%llu> %s_extra %llu %s\n", inode, field, extra, dev_path);

        printf("Hex: %f\n", avg_hex_time);
        printf("Str: %f\n", avg_str_time);
        return 0;
}