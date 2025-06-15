#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
        /* Open the command for reading. */
        const char *command = "echo Hello";
        FILE *stream = popen(command, "r");
        if (stream == NULL)
                printf("Failed to run command\n");
        pclose(stream);

        return 0;
}
