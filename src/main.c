#include "mini_unionfs.h"

extern struct fuse_operations unionfs_oper;

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <lower> <upper> <mnt> [options]\n", argv[0]);
        return 1;
    }

    struct mini_unionfs_state *state = malloc(sizeof(struct mini_unionfs_state));
    state->lower_dir = realpath(argv[1], NULL);
    state->upper_dir = realpath(argv[2], NULL);

    // construct a new argv for FUSE: [program_name, mount_point, ...flags]
    char *fuse_argv[argc];
    fuse_argv[0] = argv[0];
    fuse_argv[1] = argv[3]; // mount
    for (int i = 4; i < argc; i++) {
        fuse_argv[i - 2] = argv[i];
    }
    int fuse_argc = argc - 2;

    return fuse_main(fuse_argc, fuse_argv, &unionfs_oper, state);
}

