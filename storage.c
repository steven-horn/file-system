#include <stdio.h>
#include <string.h>

#include "storage.h"
#include "pages.h"

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
    inode* node = pages_get_node_from_path(path);
    file_data* fdata = malloc(sizeof(file_data));
    fdata->path = path;
    fdata->mode = node->mode;
    fdata->data = (const char*)block;
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

