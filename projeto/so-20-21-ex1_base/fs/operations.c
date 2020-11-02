#include "operations.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>


pthread_mutex_t fsMutex;
pthread_rwlock_t fsRWLock;
int strat = 0;


void lock(int strat, int op) {
    switch (strat) {
        case 0:
            break;
        case 1:
            if (pthread_mutex_lock(&fsMutex) != 0) exit(EXIT_FAILURE);
            break;
        case 2:
            if (op) {
                if (pthread_rwlock_rdlock(&fsRWLock) != 0) exit(EXIT_FAILURE);
                }
            else if (pthread_rwlock_wrlock(&fsRWLock) != 0) exit(EXIT_FAILURE);
            break;
        default:
            break;          
    }
}

void unlock(int strat) {
    switch (strat) {
        case 0:
            break;
        case 1:
            if (pthread_mutex_unlock(&fsMutex) != 0) exit(EXIT_FAILURE);
            break;
        case 2:
            if (pthread_rwlock_unlock(&fsRWLock) != 0) exit(EXIT_FAILURE);
            break;
        default:
            break;
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
    if (pthread_mutex_init(&fsMutex, NULL) != 0) exit(EXIT_FAILURE);
    if (pthread_rwlock_init(&fsRWLock, NULL) != 0) exit(EXIT_FAILURE);
}


/*
 * Destroy tecnicofs and inode table.
 */
void destroy_fs() {
	inode_table_destroy();
    if (pthread_mutex_destroy(&fsMutex) != 0) exit(EXIT_FAILURE);
    if (pthread_rwlock_destroy(&fsRWLock) != 0) exit(EXIT_FAILURE);
}


/*
 * Checks if content of directory is not empty.
 * Input:
 *  - entries: entries of directory
 * Returns: SUCCESS or FAIL
 */

int is_dir_empty(DirEntry *dirEntries, int strat) {
	if (dirEntries == NULL) {
		return FAIL;
	}
    lock(strat,1);
	for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
    
		if (dirEntries[i].inumber != FREE_INODE) {
            unlock(strat);
			return FAIL;
            
		}
	}
    unlock(strat);
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
int lookup_sub_node(char *name, DirEntry *entries, int strat) {
	if (entries == NULL) {
		return FAIL;
	}
    lock(strat,1);
	for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (entries[i].inumber != FREE_INODE && strcmp(entries[i].name, name) == 0) {
            unlock(strat);
            return entries[i].inumber;
        }
    }
    unlock(strat);
	return FAIL;
}


/*
 * Creates a new node given a path.
 * Input:
 *  - name: path of node
 *  - nodeType: type of node
 * Returns: SUCCESS or FAIL
 */
int create(char *name, type nodeType, int strat){

	int parent_inumber, child_inumber;
	char *parent_name, *child_name, name_copy[MAX_FILE_NAME];
	/* use for copy */
	type pType;
	union Data pdata;

	strcpy(name_copy, name);
	split_parent_child_from_path(name_copy, &parent_name, &child_name);

	parent_inumber = lookup(parent_name, strat);

	if (parent_inumber == FAIL) {
		printf("failed to create %s, invalid parent dir %s\n",
		        name, parent_name);
		return FAIL;
	}
    lock(strat,1);
	inode_get(parent_inumber, &pType, &pdata);
    unlock(strat);

	if (pType != T_DIRECTORY) {
		printf("failed to create %s, parent %s is not a dir\n",
		        name, parent_name);
		return FAIL;
	}

	if (lookup_sub_node(child_name, pdata.dirEntries, strat) != FAIL) {
		printf("failed to create %s, already exists in dir %s\n",
		       child_name, parent_name);
		return FAIL;
	}

	/* create node and add entry to folder that contains new node */
    lock(strat,0);
	child_inumber = inode_create(nodeType);
    unlock(strat);
	if (child_inumber == FAIL) {
		printf("failed to create %s in  %s, couldn't allocate inode\n",
		        child_name, parent_name);
		return FAIL;
	}
    lock(strat, 0);
	if (dir_add_entry(parent_inumber, child_inumber, child_name) == FAIL) {
		printf("could not add entry %s in dir %s\n",
		       child_name, parent_name);
        unlock(strat);
		return FAIL;
	}
    unlock(strat);

	return SUCCESS;
}


/*
 * Deletes a node given a path.
 * Input:
 *  - name: path of node
 * Returns: SUCCESS or FAIL
 */
int delete(char *name, int strat){

	int parent_inumber, child_inumber;
	char *parent_name, *child_name, name_copy[MAX_FILE_NAME];
	/* use for copy */
	type pType, cType;
	union Data pdata, cdata;

	strcpy(name_copy, name);
	split_parent_child_from_path(name_copy, &parent_name, &child_name);

	parent_inumber = lookup(parent_name, strat);

	if (parent_inumber == FAIL) {
		printf("failed to delete %s, invalid parent dir %s\n",
		        child_name, parent_name);
		return FAIL;
	}
    lock(strat,1);
	inode_get(parent_inumber, &pType, &pdata);
    unlock(strat);

	if(pType != T_DIRECTORY) {
		printf("failed to delete %s, parent %s is not a dir\n",
		        child_name, parent_name);
		return FAIL;
	}

	child_inumber = lookup_sub_node(child_name, pdata.dirEntries, strat);

	if (child_inumber == FAIL) {
		printf("could not delete %s, does not exist in dir %s\n",
		       name, parent_name);
		return FAIL;
	}
    lock(strat, 1);
	inode_get(child_inumber, &cType, &cdata);
    unlock(strat);

	if (cType == T_DIRECTORY && is_dir_empty(cdata.dirEntries, strat) == FAIL) {
		printf("could not delete %s: is a directory and not empty\n",
		       name);
		return FAIL;
	}

	/* remove entry from folder that contained deleted node */
    lock(strat,0);
	if (dir_reset_entry(parent_inumber, child_inumber) == FAIL) {
		printf("failed to delete %s from dir %s\n",
		       child_name, parent_name);
        unlock(strat);
		return FAIL;
	}
    unlock(strat);
    
    lock(strat,0);
	if (inode_delete(child_inumber) == FAIL) {
		printf("could not delete inode number %d from dir %s\n",
		       child_inumber, parent_name);
        unlock(strat);
		return FAIL;
    }
	unlock(strat);

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
int lookup(char *name, int strat) {
	char full_path[MAX_FILE_NAME];
	char delim[] = "/";

	strcpy(full_path, name);

	/* start at root node */
	int current_inumber = FS_ROOT;

	/* use for copy */
	type nType;
	union Data data;

	/* get root inode data */
    lock(strat,1);
	inode_get(current_inumber, &nType, &data);
    unlock(strat);

	char *path = strtok(full_path, delim);

	/* search for all sub nodes */
	while (path != NULL && (current_inumber = lookup_sub_node(path, data.dirEntries, strat)) != FAIL) {
        lock(strat,1);
		inode_get(current_inumber, &nType, &data);
        unlock(strat);
		path = strtok(NULL, delim);
	}

	return current_inumber;
}


/*
 * Prints tecnicofs tree.
 * Input:
 *  - fp: pointer to output file
 */
void print_tecnicofs_tree(FILE *fp){
	inode_print_tree(fp, FS_ROOT, "");
}
