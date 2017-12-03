#define _GNU_SOURCE
#include <string.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

#include "pages.h"
#include "slist.h"
#include "util.h"
#include "storage.h"

const int NUFS_SIZE  = 1024 * 1024; // 1MB
const int PAGE_COUNT = 256;

static int   pages_fd   = -1;
static void* pages_base =  0;
static super* super_block;

void
pages_init(const char* path)
{
    pages_fd = open(path, O_CREAT | O_RDWR, 0644);
    assert(pages_fd != -1);

    int rv = ftruncate(pages_fd, NUFS_SIZE);
    assert(rv == 0);

    pages_base = mmap(0, NUFS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, pages_fd, 0);
    assert(pages_base != MAP_FAILED);

    super_block = (super*)pages_base;
    super_block->imap = (unsigned char*)(pages_base + sizeof(super));
    super_block->imap_size = 254;
    super_block->dmap = (unsigned char*)(pages_base + sizeof(super) + super_block->imap_size);
    super_block->dmap_size = 254;
    super_block->num_inodes = 254;
    super_block->num_blocks = 254;
    super_block->inodes = (inode*) (super_block->dmap + super_block->num_blocks);
    super_block->blocks = pages_base + 8192;

    memset(super_block->imap, 0, super_block->num_inodes + super_block->num_blocks);
}

void
pages_free()
{
    int rv = munmap(pages_base, NUFS_SIZE);
    assert(rv == 0);
}

void*
pages_get_page(int pnum)
{
    return super_block->blocks + 4096 * pnum;
}

inode*
pages_get_node(int node_id)
{
    return super_block->inodes + node_id;
}

int
pages_find_empty_inode()
{
    return bitmap_get_empty(super_block->imap, super_block->imap_size);
}

int
pages_find_empty_data_block()
{
    return bitmap_get_empty(super_block->dmap, super_block->dmap_size);
}

void
print_node(inode* node)
{
    if (node) {
        printf("node{refs: %d, mode: %04o, size: %d}\n",
               node->refs, node->mode, node->size);
    }
    else {
        printf("node{null}\n");
    }
}

int
pages_get_node_from_path(const char* path)
{
    path++;
    inode* n;
    for (int i = 0; i < super_block->num_inodes; i++) {
	    n = pages_get_node(i);
	    if (strcmp(path, n->name) == 0) {
	        return i;
	    }
    }
    return -1;
}

int
pages_create(const char* path, int mode)
{
    int n = pages_find_empty_inode();
    if (n == -1) {
        return -EDQUOT;
    }

    bitmap_set(super_block->imap, n);

    inode* node = pages_get_node(n);

    const char* p = path + 1;
    strncpy(node->name, p, 48);
    node->refs = 1;
    node->mode = mode;
    node->size = 0;

    if (mode / 1000 == 010) {
        node->num_blocks = 1;
        int b = pages_find_empty_data_block();
        void* block = pages_get_page(b);
        bitmap_set(super_block->dmap, b);
        node->block = block;
    }
    else {
        node->num_blocks = 0;
        node->block = NULL;
    }
}

int
pages_delete(const char* path)
{
    int n = pages_get_node_from_path(path);
    if (n == -1) {
        return -ENOENT;
    }

    inode* node = pages_get_node(n);

    if (node->num_blocks > 0) {
        int b = (node->block - super_block->blocks) / 4096;
        bitmap_set(super_block->dmap, b);
    }
    bitmap_set(super_block->imap, n);

    return 0;
}

int
pages_rename(const char* from, const char* to)
{
    int n = pages_get_node_from_path(from);
    if (n == -1) {
        return -ENOENT;
    }

    int nn = pages_get_node_from_path(to);
    if (n != -1) {
        pages_delete(to);
    }
    
    inode* node = pages_get_node(n);

    const char* t = to + 1;
    strncpy(node->name, t, 48);

    return 0;
}

int
pages_readdir(void* buf, fuse_fill_dir_t filler)
{
    struct stat st;
    inode* node;

    for (int ii = 0; ii < super_block->num_inodes; ++ii) {
        if (bitmap_get(super_block->imap, ii) == 1) {
            node = pages_get_node(ii);
            char* n = "/";
            strncat(n, node->name, 40);
            get_stat(n, &st);
            filler(buf, ".", &st, 0);
        }
    }
}
