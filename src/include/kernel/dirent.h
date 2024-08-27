#ifndef _KERNEL_DIRENT_H
#define _KERNEL_DIRENT_H

#include <stdint.h>

struct dirent {
        uint32_t inode_n;
        char     name[256];

        struct dirent *next;
};

#endif
