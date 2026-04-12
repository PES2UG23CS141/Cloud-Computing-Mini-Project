#include "mini_unionfs.h"
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>

int resolve_path(const char *path, char *resolved_path) {
    struct mini_unionfs_state *st = UNIONFS_DATA;
    char wh_path[PATH_MAX];
    char up_path[PATH_MAX];
    char lo_path[PATH_MAX];

    // construct whiteout path
    char *slash = strrchr(path, '/');
    if (slash == path) { // root file: /file.txt -> upper/.wh.file.txt
        snprintf(wh_path, PATH_MAX, "%s/.wh.%s", st->upper_dir, path + 1);
    } else { // subfolder file: /dir/file.txt -> upper/dir/.wh.file.txt
        int dir_len = slash - path;
        snprintf(wh_path, PATH_MAX, "%s%.*s/.wh.%s", st->upper_dir, dir_len, path, slash + 1);
    }

    // If whiteout exists, the file is considered to be deleted
    if (access(wh_path, F_OK) == 0) {
        return -ENOENT;
    }

    // Check upper directory
    snprintf(up_path, PATH_MAX, "%s%s", st->upper_dir, path);
    if (access(up_path, F_OK) == 0) {
        strcpy(resolved_path, up_path);
        return 0;
    }

    // check lower directory
    snprintf(lo_path, PATH_MAX, "%s%s", st->lower_dir, path);
    if (access(lo_path, F_OK) == 0) {
        strcpy(resolved_path, lo_path);
        return 0;
    }

    return -ENOENT;
}
