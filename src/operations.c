#include "mini_unionfs.h"
#include "uthash.h"
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// from path_utils.c
extern int resolve_path(const char *path, char *resolved_path);
// from cow_logic.c
extern int copy_file(const char *src, const char *dst);

static int unionfs_open(const char *path, struct fuse_file_info *fi) {
    char up_path[PATH_MAX], lo_path[PATH_MAX];
    struct mini_unionfs_state *st = UNIONFS_DATA;

    snprintf(up_path, PATH_MAX, "%s%s", st->upper_dir, path);
    snprintf(lo_path, PATH_MAX, "%s%s", st->lower_dir, path);

    // determine if write op (O_WRONLY or O_RDWR)
    int is_write = ((fi->flags & O_ACCMODE) != O_RDONLY);

    // if write and the file only exists in lower_dir, trigger CoW
    if (is_write && access(up_path, F_OK) != 0 && access(lo_path, F_OK) == 0) {
        int res = copy_file(lo_path, up_path);
        if (res != 0) return res;
    }

    // resolve path again to get the (potentially newly copied) file
    char resolved[PATH_MAX];
    int res = resolve_path(path, resolved);
    if (res != 0) return res;

    int fd = open(resolved, fi->flags);
    if (fd == -1) return -errno;

    fi->fh = fd;
    return 0;
}
