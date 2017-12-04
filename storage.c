#include <stdio.h>
#include <string.h>
#include <fuse.h>
#include <stdlib.h>
#include <errno.h>

#include "storage.h"
#include "hints/pages.h"

typedef struct file_data {
    const char* path;
    int         mode;
    const char* data;
} file_data;

void
storage_init(const char* path)
{
    printf("TODO: Store file system data in: %s\n", path);
    pages_init(path);
}

static int
streq(const char* aa, const char* bb)
{
    return strcmp(aa, bb) == 0;
}

static file_data*
get_file_data(const char* path) 
{
    int n = pages_get_node_from_path(path);
    inode* node = pages_get_node(n);
    if (n == -1) {
        return NULL;
    }
    file_data* fdata = malloc(sizeof(file_data));
    fdata->path = path;
    fdata->mode = node->mode;
    fdata->data = (const char*)node->block;
    return fdata;
}

int
get_stat(const char* path, struct stat* st)
{
    file_data* dat = get_file_data(path);
    if (!dat) {
        return -1;
    }

    memset(st, 0, sizeof(struct stat));
    st->st_uid  = getuid();
    st->st_mode = dat->mode;
    if (dat->data) {
        st->st_size = strlen(dat->data);
    }
    else {
        st->st_size = 0;
    }
    free(dat);
    return 0;
}

const char*
get_data(const char* path)
{
    file_data* dat = get_file_data(path);
    if (!dat) {
        return 0;
    }
    return dat->data;
}

int
storage_create(const char* path, int mode)
{
    if (strlen(path) > 48) {
        return -ENAMETOOLONG;
    }

    return pages_create(path, mode);
}

int
storage_delete(const char* path)
{
    if (strlen(path) > 48) {
        return -ENAMETOOLONG;
    }

    return pages_delete(path);
}

int
storage_rename(const char* from, cost char* to)
{
    if (strlen(from) > 48 || strlen(to) > 48) {
        return -ENAMETOOLONG;
    }

    return pages_rename(from, to);
}

int
storage_readdir(void* buf, fuse_fill_dir_t filler)
{
    return pages_readdir(buf, filler);
}
