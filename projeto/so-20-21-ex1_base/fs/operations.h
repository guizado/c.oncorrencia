#ifndef FS_H
#define FS_H
#include "state.h"


void init_fs();
void destroy_fs();
int is_dir_empty(DirEntry *dirEntries, int strat);
int create(char *name, type nodeType, int strat);
int delete(char *name, int strat);
int lookup(char *name, int strat);
void print_tecnicofs_tree(FILE *fp);
void lock(int strat, int op);
void unlock(int strat);

#endif /* FS_H */
