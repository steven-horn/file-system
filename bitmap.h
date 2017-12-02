#ifndef BITMAP_H
#define BITMAP_H

#define BIT (CHAR_BIT * sizeof(unsigned char))

int  bitmap_get(unsigned char *bitmap, int pos);
void bitmap_set(unsigned char *bitmap, int pos);
void bitmap_reset(unsigned char *bitmap, int pos);
void bitmap_get_empty(unsigned char *bitmap, int size);

#endif
