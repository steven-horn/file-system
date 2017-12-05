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
#include <bsd/string.h>

#include "pages.h"
#include "slist.h"
#include "util.h"
#include "storage.h"
#include "directory.h"

const int NUFS_SIZE  = 1024 * 1024; // 1MB
const int PAGE_COUNT = 256;

static int   pages_fd   = -1;
static void* pages_base =  0;
static super* super_block;
static directory* root;

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

    bitmap_set(super_block->imap, 0);
    bitmap_set(super_block->dmap, 0);
    inode* node = pages_get_node(0);
    node->refs = 1;
    node->mode = 040755;
    node->size = sizeof(directory);
    node->block = pages_get_page(0);
    node->num_blocks = 1;

    root = (directory*) node->block;
    root->node = node;
    root->num_ents = 0;
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
        printf("node{refs: %d, mode: %04o, size: %d, block: %ld, num_blocks: %d, name: %s}\n",
               node->refs, node->mode, node->size, (long) node->block,
               node->num_blocks, node->name);
    }
    else {
        printf("node{null}\n");
    }
}

int
pages_get_node_from_path(const char* path)
{
    if (strcmp(path, "/") == 0) {
        return 0;
    }

    path++;
    inode* n;
    for (int i = 1; i < super_block->num_inodes; i++) {
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

    node->num_blocks = 1;
    int b = pages_find_empty_data_block();
    void* block = pages_get_page(b);
    bitmap_set(super_block->dmap, b);
    node->block = block;
    node->size = 0;
    
    directory_put_ent(root, p, n);
    print_node(node);
    return 0;
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
    
    directory_delete(*root, path + 1);
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

slist*
pages_get_names(const char* path)
{
    //directory dd = directory_from_path(path);
    //print_directory(dd);
    return directory_list(path);
}

int
pages_trunc(const char* path, off_t size)
{
    int n = pages_get_node_from_path(path);
    if (n == -1) {
        return -ENOENT;
    }

    inode* node = pages_get_node(n);

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
            node->block = block;
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

int
pages_write(const char* path, const char* buf, size_t size, off_t offset)
{
    int n = pages_get_node_from_path(path);
    if (n == -1) {
        return -ENOENT;
    }

    inode* node = pages_get_node(n);
    char* dat = (char*)node->block;
    dat += offset + node->size;
    
    strlcpy(dat, buf, size + 1);
    *(dat + size) = '\0';
    node->size += strlen(dat - offset);
    print_node(node);
    printf("Returning %lu\n", size);
    return size;
}
