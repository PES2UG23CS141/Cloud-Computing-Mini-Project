#include "mini_unionfs.h"
#include "uthash.h"
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// uthash to track seen/hidden files
struct file_entry {
    char name[256];
    UT_hash_handle hh;
};

// from path_utils.c
extern int resolve_path(const char *path, char *resolved_path);
// from cow_logic.c
extern int copy_file(const char *src, const char *dst);

static int unionfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    char resolved[PATH_MAX];
    int res = resolve_path(path, resolved);
    if (res != 0) return res;
    if (lstat(resolved, stbuf) == -1) return -errno;
    return 0;
}

static int unionfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                           off_t offset, struct fuse_file_info *fi,
                           enum fuse_readdir_flags flags) {
    struct mini_unionfs_state *st = UNIONFS_DATA;
    struct file_entry *registry = NULL, *e;
    char full_path[PATH_MAX];
    DIR *dp;
    struct dirent *de;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    snprintf(full_path, PATH_MAX, "%s%s", st->upper_dir, path);
    dp = opendir(full_path);
    if (dp != NULL) {
        while ((de = readdir(dp)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
            if (strncmp(de->d_name, ".wh.", 4) == 0) {
                e = malloc(sizeof(struct file_entry));
                strncpy(e->name, de->d_name + 4, 256);
                HASH_ADD_STR(registry, name, e);
                continue;
            }
            e = malloc(sizeof(struct file_entry));
            strncpy(e->name, de->d_name, 256);
            HASH_ADD_STR(registry, name, e);
            filler(buf, de->d_name, NULL, 0, 0);
        }
        closedir(dp);
    }

    snprintf(full_path, PATH_MAX, "%s%s", st->lower_dir, path);
    dp = opendir(full_path);
    if (dp != NULL) {
        while ((de = readdir(dp)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
            HASH_FIND_STR(registry, de->d_name, e);
            if (e == NULL) filler(buf, de->d_name, NULL, 0, 0);
        }
        closedir(dp);
    }

    struct file_entry *tmp;
    HASH_ITER(hh, registry, e, tmp) { HASH_DEL(registry, e); free(e); }
    return 0;
}

static int unionfs_read(const char *path, char *buf, size_t size, off_t offset,
                         struct fuse_file_info *fi) {
    int res = pread(fi->fh, buf, size, offset);
    if (res == -1) res = -errno;
    return res;
}

static int unionfs_open(const char *path, struct fuse_file_info *fi) {
    char up_path[PATH_MAX], lo_path[PATH_MAX];
    struct mini_unionfs_state *st = UNIONFS_DATA;

    snprintf(up_path, PATH_MAX, "%s%s", st->upper_dir, path);
    snprintf(lo_path, PATH_MAX, "%s%s", st->lower_dir, path);

    int is_write = ((fi->flags & O_ACCMODE) != O_RDONLY);
    if (is_write && access(up_path, F_OK) != 0 && access(lo_path, F_OK) == 0) {
        int res = copy_file(lo_path, up_path);
        if (res != 0) return res;
    }

    char resolved[PATH_MAX];
    int res = resolve_path(path, resolved);
    if (res != 0) return res;

    int fd = open(resolved, fi->flags);
    if (fd == -1) return -errno;
    fi->fh = fd;
    return 0;
}

static int unionfs_write(const char *path, const char *buf, size_t size,
                          off_t offset, struct fuse_file_info *fi) {
    int res = pwrite(fi->fh, buf, size, offset);
    if (res == -1) res = -errno;
    return res;
}

// construct whiteout path: /folder/file -> upper/folder/.wh.file
void get_whiteout_path(const char *path, char *wh_path) {
    struct mini_unionfs_state *st = UNIONFS_DATA;
    char *slash = strrchr(path, '/');
    if (slash == path) {
        sprintf(wh_path, "%s/.wh.%s", st->upper_dir, path + 1);
    } else {
        int dir_len = slash - path;
        sprintf(wh_path, "%s%.*s/.wh.%s", st->upper_dir, dir_len, path, slash + 1);
    }
}

static int unionfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    struct mini_unionfs_state *st = UNIONFS_DATA;
    char up_path[PATH_MAX], wh_path[PATH_MAX];

    // now that get_whiteout_path exists, remove whiteout if present
    get_whiteout_path(path, wh_path);
    unlink(wh_path);

    snprintf(up_path, PATH_MAX, "%s%s", st->upper_dir, path);
    int fd = open(up_path, fi->flags | O_CREAT, mode);
    if (fd == -1) return -errno;
    fi->fh = fd;
    return 0;
}

static int unionfs_unlink(const char *path) {
    struct mini_unionfs_state *st = UNIONFS_DATA;
    char up_path[PATH_MAX], lo_path[PATH_MAX], wh_path[PATH_MAX];

    snprintf(up_path, PATH_MAX, "%s%s", st->upper_dir, path);
    snprintf(lo_path, PATH_MAX, "%s%s", st->lower_dir, path);

    // If it exists in upper_dir, perform physical unlink
    if (access(up_path, F_OK) == 0) {
        if (unlink(up_path) == -1) return -errno;
    }

    // If it exists in lower_dir, create a whiteout file in upper_dir
    if (access(lo_path, F_OK) == 0) {
        get_whiteout_path(path, wh_path);
        int fd = creat(wh_path, 0666);
        if (fd == -1) return -errno;
        close(fd);
    }

    return 0;
}

static int unionfs_rmdir(const char *path) {
    struct mini_unionfs_state *st = UNIONFS_DATA;
    char up_path[PATH_MAX], lo_path[PATH_MAX], wh_path[PATH_MAX];

    snprintf(up_path, PATH_MAX, "%s%s", st->upper_dir, path);
    snprintf(lo_path, PATH_MAX, "%s%s", st->lower_dir, path);

    // If it exists in upper_dir, use rmdir()
    if (access(up_path, F_OK) == 0) {
        if (rmdir(up_path) == -1) return -errno;
    }

    // If it exists in lower_dir, mask it with a whiteout
    if (access(lo_path, F_OK) == 0) {
        get_whiteout_path(path, wh_path);
        int fd = creat(wh_path, 0666);
        if (fd == -1) return -errno;
        close(fd);
    }

    return 0;
}

struct fuse_operations unionfs_oper = {
    .getattr = unionfs_getattr,
    .readdir = unionfs_readdir,
    .open    = unionfs_open,
    .read    = unionfs_read,
    .write   = unionfs_write,
    .create  = unionfs_create,
    .unlink  = unionfs_unlink,
    .rmdir   = unionfs_rmdir,
};
