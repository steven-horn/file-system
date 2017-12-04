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

    memset(inode_bitmap, 0, INODE_COUNT + DATA_BLOCK_COUNT);

    super_block = (super*)pages_base;
    super_block->imap = (unsigned char*)(pages_base + sizeof(super));
    super_block->imap_size = 254;
    super_block->dmap = (unsigned char*)(pages_base + sizeof(super) + super_block->imap_size));
    super_block->dmap_size = 254;
    super_block->num_inodes = 254;
    super_block->num_blocks = 254;
    super_block->inodes = (inode*)(((void*)dmap) + super_block->num_blocks);
    super_block->blocks = pages_base + 8192;
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
    return bitmap_find_empty(super_block->imap, super_block->imap_size);
}

int
pages_find_empty_data_block()
{
    return bitmap_find_empty(super_block->dmap, super_block->dmap_size);
}

void
print_node(inode* node)
{
    if (node) {
        printf("node{refs: %d, mode: %04o, size: %d, xtra: %d}\n",
               node->refs, node->mode, node->size, node->xtra);
    }
    else {
        printf("node{null}\n");
    }
}

int
pages_get_node_from_path(const char* path)
{
    path = path[1];
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
pages_create(path, mode)
{
    int n = pages_find_empty_inode();
    if (n == -1) {
        return -EDQUOT;
    }

    bitmap_set(super_block->imap, n);

    inode* node = pages_get_node(n);

    const char* p = path[1];
    node->name = p;
    node->refs = 1;
    node->mode = mode;

    if (mode / 1000 == 010) {
        node->num_blocks = 1;
        int b = pages_find_empty_data_block();
        void* block = pages_get_page(b);
        bitmap_set(super_block->dmap, b);
        node->data = block;
        node->size = 4096;
    }
    else {
        node->num_blocks = 0;
        node->data = NULL;
        node->size = 0;
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

    const char* t = to[1];
    node->name = t;

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

int
pages_trunc(const char* path, off_t size)
{
    int n = pages_get_node_from_path(path);
    if (n == -1) {
        return -ENOENT;
    }

    inode* node = pages_get_node(path);

    if (node->size < size) {
        int numBlocks = (size / 4096) + 1;
        int b = (node->block - super_block->blocks) / 4096;
        int n = 0;
        for (int ii = 1; ii < numBlocks; ++ii) {
            if (bitmap_get(super_block->dmap, b + ii) == 1) {
                n = 1;
            }
        }
        if (n = 0) {
            for (int ii = 1; ii < numBlocks; ++ii) {
                bitmap_set(super_block->dmap, b + ii);
            }
            node->size = size;
            node->num_blocks = numBlocks;
        }
        else {
            void* block;
            int rv = pages_get_blocks(numBlocks, block);
            if (rv == -1) {
                return -1;
            }
            node->size = size;
            node->blocks = block;
            node->num_blocks = numBlocks;
        }
    }
    else {
        node->size = size;
    } 
    return 0;
}

int
pages_get_blocks(int num, void* block)
{
    int b = 1;
    int n = 0;
    for (int ii = 0; ii < super_block->num_blocks; ++ii) {
        if (n == num) {
            return 0;
        }
        else if (b == 0) {
            if (bitmap_get(super_block->dmap, ii) == 0) {
                n++;
            }
            else {
                b = 1;
                n = 0;
            }
        }
        else {
            if (bitmap_get(super_block->dmap, ii) == 0) {
                n++;
                b = 0;
                block = pages_get_page(ii);
            }
        }
    }
    return -1;
}
