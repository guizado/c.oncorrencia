#ifndef FS_H
#define FS_H
#include "state.h"

void addLockedInode(int inumber, int inodeWaitList[], int *len);
void unlockLast(int inodeWaitList[], int *len);
void lock_or_trylock(int inumber, int inodeWaitList[], int *len, lock_mode mode, int try);
void init_fs();
void destroy_fs();
int is_dir_empty(DirEntry *dirEntries);
int create(char *name, type nodeType, int inodeWaitList[], int *len);
int delete(char *name, int inodeWaitList[], int *len);
int lookup(char *name, int inodeWaitList[], int *len, int try);
int move(char *path, char *new_path, int inodeWaitList[], int *len);
void print_tecnicofs_tree(FILE *fp);

#endif /* FS_H */
