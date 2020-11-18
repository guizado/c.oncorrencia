#include "operations.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>


void addLockedInode(int inumber, int inodeWaitList[], int *len) {
    inodeWaitList[*len] = inumber;
    *len = *len + 1;
}

void unlockLast(int inodeWaitList[], int *len) {
    unlock(inodeWaitList[*len - 1]);
    inodeWaitList[*len - 1] = 0;
    *len = *len - 1;
}


void lock_or_trylock(int inumber, int inodeWaitList[], int *len, lock_mode mode, int try) {
    if (try) {
        if (trylock(inumber, mode)) addLockedInode(inumber, inodeWaitList, len);
    }
    else {
        if (lock(inumber, mode)) addLockedInode(inumber, inodeWaitList, len);
    }
}

/* Given a path, fills pointers with strings for the parent path and child
 * file name
 * Input:
 *  - path: the path to split. ATENTION: the function may alter this parameter
 *  - parent: reference to a char*, to store parent path
 *  - child: reference to a char*, to store child file name
 */
void split_parent_child_from_path(char * path, char ** parent, char ** child) {

	int n_slashes = 0, last_slash_location = 0;
	int len = strlen(path);

	// deal with trailing slash ( a/x vs a/x/ )
	if (path[len-1] == '/') {
		path[len-1] = '\0';
	}

	for (int i=0; i < len; ++i) {
		if (path[i] == '/' && path[i+1] != '\0') {
			last_slash_location = i;
			n_slashes++;
		}
	}

	if (n_slashes == 0) { // root directory
		*parent = "";
		*child = path;
		return;
	}

	path[last_slash_location] = '\0';
	*parent = path;
	*child = path + last_slash_location + 1;

}


/*
 * Initializes tecnicofs and creates root node.
 */
void init_fs() {
	inode_table_init();
	/* create root inode */
	int root = inode_create(T_DIRECTORY);
	if (root != FS_ROOT) {
		printf("failed to create node for tecnicofs root\n");
		exit(EXIT_FAILURE);
	}
}


/*
 * Destroy tecnicofs and inode table.
 */
void destroy_fs() {
	inode_table_destroy();
}


/*
 * Checks if content of directory is not empty.
 * Input:
 *  - entries: entries of directory
 * Returns: SUCCESS or FAIL
 */

int is_dir_empty(DirEntry *dirEntries) {
	if (dirEntries == NULL) {
		return FAIL;
	}
	for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
		if (dirEntries[i].inumber != FREE_INODE) {
			return FAIL; 
		}
	}
	return SUCCESS;
}


/*
 * Looks for node in directory entry from name.
 * Input:
 *  - name: path of node
 *  - entries: entries of directory
 * Returns:
 *  - inumber: found node's inumber
 *  - FAIL: if not found
 */
int lookup_sub_node(char *name, DirEntry *entries) {

	if (entries == NULL) {
		return FAIL;
	}
	for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (entries[i].inumber != FREE_INODE && strcmp(entries[i].name, name) == 0) {
            return entries[i].inumber;
        }
    }
	return FAIL;
}


/*
 * Creates a new node given a path.
 * Input:
 *  - name: path of node
 *  - nodeType: type of node
 * Returns: SUCCESS or FAIL
 */
int create(char *name, type nodeType, int inodeWaitList[], int *len){

	int parent_inumber, child_inumber;
	char *parent_name, *child_name, name_copy[MAX_FILE_NAME];
	/* use for copy */
	type pType;
	union Data pdata;

	strcpy(name_copy, name);
	split_parent_child_from_path(name_copy, &parent_name, &child_name);

	parent_inumber = lookup(parent_name, inodeWaitList, len, 0);

	if (parent_inumber == FAIL) {
		printf("failed to create %s, invalid parent dir %s\n",
		        name, parent_name);
		return FAIL;
	}

	inode_get(parent_inumber, &pType, &pdata);

    unlockLast(inodeWaitList, len);
    lock(parent_inumber, LWRITE);
    addLockedInode(parent_inumber, inodeWaitList, len);

	if (pType != T_DIRECTORY) {
		printf("failed to create %s, parent %s is not a dir\n",
		        name, parent_name);
		return FAIL;
	}
	if (lookup_sub_node(child_name, pdata.dirEntries) != FAIL) {
		printf("failed to create %s, already exists in dir %s\n",
		       child_name, parent_name);
		return FAIL;
	}

	/* create node and add entry to folder that contains new node */
	child_inumber = inode_create(nodeType);
    lock(child_inumber, LWRITE);
    addLockedInode(child_inumber, inodeWaitList, len);

	if (child_inumber == FAIL) {
		printf("failed to create %s in  %s, couldn't allocate inode\n",
		        child_name, parent_name);
		return FAIL;
	}
	if (dir_add_entry(parent_inumber, child_inumber, child_name) == FAIL) {
		printf("could not add entry %s in dir %s\n",
		       child_name, parent_name);
		return FAIL;
	}

	return SUCCESS;
}


/*
 * Deletes a node given a path.
 * Input:
 *  - name: path of node
 * Returns: SUCCESS or FAIL
 */
int delete(char *name, int inodeWaitList[], int *len){

	int parent_inumber, child_inumber;
	char *parent_name, *child_name, name_copy[MAX_FILE_NAME];
	/* use for copy */
	type pType, cType;
	union Data pdata, cdata;

	strcpy(name_copy, name);
	split_parent_child_from_path(name_copy, &parent_name, &child_name);

	parent_inumber = lookup(parent_name, inodeWaitList, len, 0);

	if (parent_inumber == FAIL) {
		printf("failed to delete %s, invalid parent dir %s\n",
		        child_name, parent_name);
		return FAIL;
	}

	inode_get(parent_inumber, &pType, &pdata);

    unlockLast(inodeWaitList, len);
    lock(parent_inumber, LWRITE);
    addLockedInode(parent_inumber, inodeWaitList, len);

	if(pType != T_DIRECTORY) {
		printf("failed to delete %s, parent %s is not a dir\n",
		        child_name, parent_name);
		return FAIL;
	}

	child_inumber = lookup_sub_node(child_name, pdata.dirEntries);

	if (child_inumber == FAIL) {
		printf("could not delete %s, does not exist in dir %s\n",
		       name, parent_name);
		return FAIL;
	}
	inode_get(child_inumber, &cType, &cdata);

	if (cType == T_DIRECTORY && is_dir_empty(cdata.dirEntries) == FAIL) {
		printf("could not delete %s: is a directory and not empty\n",
		       name);
		return FAIL;
	}

	/* remove entry from folder that contained deleted node */
	if (dir_reset_entry(parent_inumber, child_inumber) == FAIL) {
		printf("failed to delete %s from dir %s\n",
		       child_name, parent_name);
		return FAIL;
	}
	if (inode_delete(child_inumber) == FAIL) {
		printf("could not delete inode number %d from dir %s\n",
		       child_inumber, parent_name);
		return FAIL;
    }

	return SUCCESS;
}


/*
 * Lookup for a given path.
 * Input:
 *  - name: path of node
 * Returns:
 *  inumber: identifier of the i-node, if found
 *     FAIL: otherwise
 */
int lookup(char *name, int inodeWaitList[], int *len, int try) {
	char full_path[MAX_FILE_NAME];
	char delim[] = "/";

	strcpy(full_path, name);

	/* start at root node */
    lock_or_trylock(FS_ROOT, inodeWaitList, len, LREAD, try);
	int current_inumber = FS_ROOT;

	/* use for copy */
	type nType;
	union Data data;

	/* get root inode data */
	inode_get(current_inumber, &nType, &data);

	char *path = strtok(full_path, delim);

	/* search for all sub nodes */
	while (path != NULL && (current_inumber = lookup_sub_node(path, data.dirEntries)) != FAIL) {
        lock_or_trylock(FS_ROOT, inodeWaitList, len, LREAD, try);
		inode_get(current_inumber, &nType, &data);
		path = strtok(NULL, delim);
	}

	return current_inumber;
}

/*
 * Move an entry to a new path
 * Input:
 *  - path: path of the existing entry
 *  - new_path: path which the moving entry will occupy
 * Returns: SUCCESS or FAIL
 */
int move(char *path, char *new_path, int inodeWaitList[], int *len) {
    int parent_inumber, child_inumber, new_parent_inumber, counter;
	char *parent_name, *child_name, path_copy[MAX_FILE_NAME];
    char *new_parent_name, *new_child_name, new_path_copy[MAX_FILE_NAME], new_path_copy2[MAX_FILE_NAME];
    char *token;

    type pType, npType;
	union Data pdata, npdata;

    strcpy(path_copy, path);
    strcpy(new_path_copy, new_path);
    strcpy(new_path_copy2, new_path);
	split_parent_child_from_path(path_copy, &parent_name, &child_name);
    split_parent_child_from_path(new_path_copy, &new_parent_name, &new_child_name);

    /* Test for infinite loops */
    counter = 0;
    token = strtok(new_path_copy2, "/");
    while (token != NULL) {
        if (!strcmp(token, child_name)) counter++;
        token = strtok(NULL, "/");
    }
    if (counter > 1) {
        printf("failed to move %s, infinite loop detected\n", child_name);
        return FAIL;
    }




    parent_inumber = lookup(parent_name, inodeWaitList, len, 0);

    if (parent_inumber == FAIL) {
		printf("failed to move %s, invalid parent dir %s\n",
		        path, parent_name);
		return FAIL;
	}
    inode_get(parent_inumber, &pType, &pdata);

    unlockLast(inodeWaitList, len);
    lock(parent_inumber, LWRITE);
    addLockedInode(parent_inumber, inodeWaitList, len);


    if (pType != T_DIRECTORY) {
		printf("failed to move %s, parent %s is not a dir\n",
		        path, parent_name);
		return FAIL;
	}

    child_inumber = lookup_sub_node(child_name, pdata.dirEntries);
	if (child_inumber == FAIL) {
		printf("failed to move %s, does not exist in dir %s\n",
		       child_name, parent_name);
		return FAIL;
	}

    lock(child_inumber, LWRITE);
    addLockedInode(child_inumber, inodeWaitList, len);

    new_parent_inumber = lookup(new_parent_name, inodeWaitList, len, 1);

    if (new_parent_inumber == FAIL) {
		printf("failed to move %s, invalid parent dir %s\n",
		        new_path, new_parent_name);
		return FAIL;
	}
    inode_get(new_parent_inumber, &npType, &npdata);

    if (inodeWaitList[*len - 1] == new_parent_inumber) unlockLast(inodeWaitList, len);
    if (trylock(new_parent_inumber, LWRITE)) addLockedInode(new_parent_inumber, inodeWaitList, len);

    if (npType != T_DIRECTORY) {
		printf("failed to move %s, parent %s is not a dir\n",
		        new_path, new_parent_name);
		return FAIL;
	}

	if (lookup_sub_node(child_name, npdata.dirEntries) != FAIL) {
		printf("failed to move %s, already exists in dir %s\n",
		       child_name, new_parent_name);
		return FAIL;
	}

    if (dir_reset_entry(parent_inumber, child_inumber) == FAIL) {
		printf("failed to delete %s from dir %s\n",
		       child_name, parent_name);
		return FAIL;
	}

    if (dir_add_entry(new_parent_inumber, child_inumber, new_child_name) == FAIL) {
		printf("could not add entry %s in dir %s\n",
		       new_child_name, new_parent_name);
		return FAIL;
	}
    
    return SUCCESS;
}

/*
 * Prints tecnicofs tree.
 * Input:
 *  - fp: pointer to output file
 */
void print_tecnicofs_tree(FILE *fp){
	inode_print_tree(fp, FS_ROOT, "");
}
