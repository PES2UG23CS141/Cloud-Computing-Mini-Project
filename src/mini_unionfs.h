#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global state to keep track of the directories
struct mini_unionfs_state {
    char* lower_dir; /* read only base layer */
    char* upper_dir; /* read write layer */
};

// macro for easy access to the state
#define UNIONFS_DATA ((struct mini_unionfs_state *) fuse_get_context()->private_data)
