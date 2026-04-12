#include "mini_unionfs.h"
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

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

