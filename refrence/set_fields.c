#include "e2p/e2p.h"
#include "uuid/uuid.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

// #include <debugfs/util.h>
// #include "config.h"
// #include "debugfs.h"
// #include "support/quotaio.h"

typedef char *const *ss_argv_t;

/*
 * This routine returns 1 if the filesystem is not open, and prints an
 * error message to that effect.
 */
int check_fs_open(char *name) {
        if (!current_fs) {
                com_err(name, 0, "Filesystem not open");
                return 1;
        }
        return 0;
}

/*
 * This routine returns 1 if a filesystem is not opened read/write,
 * and prints an error message to that effect.
 */
int check_fs_read_write(char *name) {
        if (!(current_fs->flags & EXT2_FLAG_RW)) {
                com_err(name, 0, "Filesystem opened read/only");
                return 1;
        }
        return 0;
}

/*
 * This is a common helper function used by the command processing
 * routines
 */
int common_args_process(int argc, ss_argv_t argv, int min_argc, int max_argc, const char *cmd, const char *usage, int flags) {
        if (check_fs_open(argv[0]))
                return 1;
        if (check_fs_read_write(argv[0]))
                return 1;
        return 0;
}

int main(void) {
        int argc = 4;
        ss_argv_t argv = {
            "set_inode_field", // 0 = set_inode_field
            "",                // 1 = <DESTINATION_INODE>
            "",                // 2 = crtime / crtime_extra / ctime / ctime_extra
            ""                 // 3 = @0xEPOCH / 0xEXTRA
        };

        const char *usage = "<inode> <field> <value>\n"
                            "\t\"set_inode_field -l\" will list the names of "
                            "the fields in an ext2 inode\n\twhich can be set.";
        static struct field_set_info *ss;

        if (common_args_process(argc, argv, 4, 4, "set_inode", usage, CHECK_FS_RW))
                return;

        if ((ss = find_field(inode_fields, argv[2])) == 0) {
                com_err(argv[0], 0, "invalid field specifier: %s", argv[2]);
                return;
        }

        set_ino = string_to_inode(argv[1]);
        if (!set_ino)
                return;

        if (debugfs_read_inode2(set_ino, (struct ext2_inode *)&set_inode, argv[1], sizeof(set_inode), (ss->flags & FLAG_CSUM) ? READ_INODE_NOCSUM : 0))
                return;

        if (ss->func(ss, argv[2], argv[3]) == 0) {
                debugfs_write_inode2(set_ino, (struct ext2_inode *)&set_inode, argv[1], sizeof(set_inode), (ss->flags & FLAG_CSUM) ? WRITE_INODE_NOCSUM : 0);

                return 0;
        }
