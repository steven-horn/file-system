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

const int NUFS_SIZE  = 1024 * 1024; // 1MB
const int PAGE_COUNT = 256;

// 254 data blocks (starts 2 pages in)
// 400 inodes (start 702 bytes in)
// inode bitmap starts 48 bytes in
// data block bitmap starts 448 bytes in

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
    return pages_base + 4096 * pnum;
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

inode*
pages_get_node_from_path(const char* path)
{
    path = path[1];
    inode* n;
    for (int i = 0; i < super_block->num_inodes; i++) {
	n = pages_get_node(i);
	if (strcmp(path, n->name) == 0) {
	    return n;
	}
     }
     return -1;
}
}
