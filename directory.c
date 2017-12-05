#include <stdlib.h>
#include <bsd/string.h>
#include <string.h>

#include "directory.h"

void
directory_init()
{
    
}

directory
directory_from_pnum(int pnum)
{
    
}

int
directory_lookup_pnum(directory dd, const char* name)
{

}

int
tree_lookup_pnum(const char* path)
{

}

directory
directory_from_path(const char* path)
{
    return *((directory*) pages_get_page(0));
}

int
directory_put_ent(directory* dd, const char* name, int pnum)
{
    dirent* ent = malloc(sizeof(dirent));
    strlcpy(ent->name, name, 50);
    ent->node = pages_get_node(pnum);
    dd->ents[dd->num_ents] = *ent;
    dd->num_ents++;
}

int
directory_delete(directory dd, const char* name)
{
    for (int ii = 0; ii < dd.num_ents; ii++) {
        if (strcmp(dd.ents[ii].name, name) == 0) {
            for (; ii < dd.num_ents - 1; ii++) {
                dd.ents[ii] = dd.ents[ii + 1];
            }
            dd.num_ents--;
            return 0;
        }
    }
    return -1;
}

slist*
directory_list(const char* path)
{
    directory dd = directory_from_path(path);
    slist* result = malloc(sizeof(slist));
    result->data = 0;
    for (int ii = 0; ii < dd.num_ents; ii++) {
        result = s_cons(dd.ents[ii].name, result);
    }
    return result;
}

void
print_directory(directory dd)
{
    for (int ii = 0; ii < dd.num_ents; ii++) {
        printf("%s\n", dd.ents[ii].name);
    }
}
