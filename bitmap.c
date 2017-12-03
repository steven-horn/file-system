#include "bitmap.h" 

int
bitmap_get(unsigned char *bitmap, int pos)
{
    unsigned char a = bitmap[pos / BIT];
    pos = pos % BIT;

    return (a >> pos) & 1;
}

void
bitmap_set(unsigned char *bitmap, int pos)
{
    unsigned char *a = &bitmap[pos / BIT];
    pos = pos % BIT;
    
    *a |= 1 << pos;
}

void
bitmap_reset(unsigned char *bitmap, int pos)
{
    unsigned char *a = &bitmap[pos / BIT];
    pos = pos % BIT;

    *a &= ~(1 << pos);
}

void
bitmap_get_empty(unsigned char *bitmap, int size)
{
    for (int i = 0; i < size; i++) {
        if (!bitmap_get(bitmap, i)) {
            return i;
        }
    }
    return -1;
}
