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

struct fuse_operations unionfs_oper = {};
