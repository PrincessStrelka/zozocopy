#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char const *argv[]) {

        const char *source = "/home/zoey/Desktop/source/sourcefile.txt";
        const char *destination = "/home/zoey/Desktop/test/source-2025-03-28_19.51.52/sourcefile.txt";
        int result;
        int input;
        int output;

        if ((input = open(source, O_RDONLY)) == -1)
                printf("Error creating source File Descriptor\n");
        else
                printf("Source FD: %d\n", input);

        if ((output = creat(destination, 0660)) == -1)
                printf("Error creating destination File Descriptor: %s\n", strerror(errno));
        else
                printf("Dest FD: %d\n", output);

        close(input);
        close(output);
        return 0;
}
