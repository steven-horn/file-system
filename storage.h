#ifndef NUFS_STORAGE_H
#define NUFS_STORAGE_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "slist.h"

void        storage_init(const char* path);
int         get_stat(const char* path, struct stat* st);
int         get_data(const char* path, const char* data);
int         storage_create(const char* path, int mode);
int         storage_delete(const char* path);
int         storage_rename(const char* from, const char* to);
int         storage_readdir(void* buf, fuse_fill_dir_t filler);
int         storage_trunc(const char* path, off_t size);
slist*      storage_get_names(const char* path);
int         storage_write(const char* path, const char* buf, size_t size, off_t offset);

#endif
