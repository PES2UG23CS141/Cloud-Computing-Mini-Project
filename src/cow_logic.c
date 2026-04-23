#include "mini_unionfs.h"
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#include <string.h>
#include <limits.h>

/* Recursively creates directories in the upper layer */
void ensure_dir_path(const char *upper_dir, const char *path) {
    char path_copy[PATH_MAX];
    char full_path[PATH_MAX];
    strncpy(path_copy, path, PATH_MAX);
    
    char *dir = dirname(path_copy);
    if (strcmp(dir, "/") == 0 || strcmp(dir, ".") == 0) return;

    /* Recursively ensure parent exists */
    ensure_dir_path(upper_dir, dir);

    /* Create the current directory in the upper layer */
    snprintf(full_path, PATH_MAX, "%s%s", upper_dir, dir);
    mkdir(full_path, 0777); 
}

int copy_file(const char *src, const char *dst) {
    int sfd = open(src, O_RDONLY);
    if (sfd < 0) return -errno;

    // create new file in upper_dir with the same permissions
    struct stat st;
    fstat(sfd, &st);
    int dfd = open(dst, O_WRONLY | O_CREAT | O_EXCL, st.st_mode);
    if (dfd < 0) {
        close(sfd);
        return -errno;
    }

    char buf[8192];
    ssize_t n;
    while ((n = read(sfd, buf, sizeof(buf))) > 0) {
        if (write(dfd, buf, n) == n) continue;
        close(sfd);
        close(dfd);
        return -EIO;
    }

    close(sfd);
    close(dfd);
    return 0;
}
