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

    // process upper directory (precedence & whiteouts)
    snprintf(full_path, PATH_MAX, "%s%s", st->upper_dir, path);
    dp = opendir(full_path);
    if (dp != NULL) {
        while ((de = readdir(dp)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;

            // handle whiteouts: .wh.filename masks filename
            if (strncmp(de->d_name, ".wh.", 4) == 0) {
                e = malloc(sizeof(struct file_entry));
                strncpy(e->name, de->d_name + 4, 256);
                HASH_ADD_STR(registry, name, e);
                continue;
            }

            // add to registry to prevent duplicates from lower_dir
            e = malloc(sizeof(struct file_entry));
            strncpy(e->name, de->d_name, 256);
            HASH_ADD_STR(registry, name, e);
            filler(buf, de->d_name, NULL, 0, 0);
        }
        closedir(dp);
    }

    // process lower directory
    snprintf(full_path, PATH_MAX, "%s%s", st->lower_dir, path);
    dp = opendir(full_path);
    if (dp != NULL) {
        while ((de = readdir(dp)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;

            // only show if not masked by upper_dir or a whiteout
            HASH_FIND_STR(registry, de->d_name, e);
            if (e == NULL) {
                filler(buf, de->d_name, NULL, 0, 0);
            }
        }
        closedir(dp);
    }

    // cleanup uthash
    struct file_entry *tmp;
    HASH_ITER(hh, registry, e, tmp) {
        HASH_DEL(registry, e);
        free(e);
    }

    return 0;
}

static int unionfs_read(const char *path, char *buf, size_t size, off_t offset,
                         struct fuse_file_info *fi) {
    // pread: allows reading at a specific offset without moving the file pointer
    int res = pread(fi->fh, buf, size, offset);
    if (res == -1) res = -errno;
    
    return res;
}

struct fuse_operations unionfs_oper = {
    .getattr = unionfs_getattr,
    .readdir = unionfs_readdir,
    .read    = unionfs_read,
};
