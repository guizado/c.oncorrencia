#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include "fs/operations.h"

#define MAX_INPUT_SIZE 100
#define MAX_DEPTH 10
#define MAX_SOCKET_PATH 100

#define OUTDIM 512

/* Global variables */
int numberThreads = 0;
pthread_t *tid_arr;

int sockfd;
socklen_t addrlen;
struct sockaddr_un server_addr;




/*
 * For invalid commands.
 */
void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
    exit(EXIT_FAILURE);
}



/* 
 * Given an array of locked i-numbers, unlocks the corresponding i-nodes and resets the array
 * Input:
 *  - inodeWaitList: array of i-numbers
 *  - len: number of i-numbers in the array
 */
void unlockAll(int inodeWaitList[], int *len) {
    for (int i = 0; i < *len; i++) {
        unlock(inodeWaitList[i]);
        inodeWaitList[i] = 0;
    }
    *len = 0;
}

/*
 * Executes commands from the buffer and stores i-numbers corresponding to
 * locked nodes to unlock after each command.
 */ 
int applyCommands(char *command){
    int inodeWaitList[MAX_DEPTH], res, len = 0;

    if (command == NULL){
        return -2;
    }
    char token, type;
    char name[MAX_INPUT_SIZE], name2[MAX_INPUT_SIZE];
    int numTokens = sscanf(command, "%c %s %s", &token, name, name2);
    if (numTokens < 2) {
        fprintf(stderr, "Error: invalid command in Queue\n");
        exit(EXIT_FAILURE);
    }

    int searchResult;
    switch (token) {
        case 'c':
            type = name2[0];
            switch (type) {
                case 'f':
                    printf("Create file: %s\n", name);
                    res = create(name, T_FILE, inodeWaitList, &len);
                    unlockAll(inodeWaitList, &len);
                    return res;
                case 'd':
                    printf("Create directory: %s\n", name);
                    res = create(name, T_DIRECTORY, inodeWaitList, &len);
                    unlockAll(inodeWaitList, &len);
                    return res;
                default:
                    fprintf(stderr, "Error: invalid node type\n");
                    exit(EXIT_FAILURE);
            }
            break;
        case 'l': 
            searchResult = lookup(name, inodeWaitList, &len, 0);
            unlockAll(inodeWaitList, &len);
            if (searchResult >= 0)
                printf("Search: %s found\n", name);
            else
                printf("Search: %s not found\n", name);
            return searchResult;
        case 'd':
            printf("Delete: %s\n", name);
            res = delete(name, inodeWaitList, &len);
            unlockAll(inodeWaitList, &len);
            return res;
        case 'm':
            printf("Move: %s %s\n", name, name2);
            res = move(name, name2, inodeWaitList, &len);
            unlockAll(inodeWaitList, &len);
            return res;
        default: { /* error */
            fprintf(stderr, "Error: command to apply\n");
            exit(EXIT_FAILURE);
        }
    }
    return -2;
}


/*
 * Parses arguments: input and output files and the numbers of threads to be used.
 * Input:
 *  - argc: number of arguments
 *  - argv: the arguments
 *  - in: input file
 *  - out: output file
 */
void args(int argc, char *argv[], char *socketname) {
    if (argc != 3) {
        printf("ERROR: invalid argument number\n");
        exit(EXIT_FAILURE);
    }
    if ((numberThreads = atoi(argv[1])) <= 0) {
        printf("ERROR: number of threads must be a positive integer\n");
        exit(EXIT_FAILURE);
    }
    strcpy(socketname, argv[2]);
}

int setSockAddrUn(char *path, struct sockaddr_un *addr) {

  if (addr == NULL)
    return 0;

  bzero((char *)addr, sizeof(struct sockaddr_un));
  addr->sun_family = AF_UNIX;
  strcpy(addr->sun_path, path);

  return SUN_LEN(addr);
}

void createSocket(char * path) {
    if ((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
        perror("server: can't open socket");
        exit(EXIT_FAILURE);
    }
    unlink(path);
    addrlen = setSockAddrUn(path, &server_addr);
    if (bind(sockfd, (struct sockaddr *) &server_addr, addrlen) < 0) {
        perror("server: bind error");
        exit(EXIT_FAILURE);
    }
}

void socketOn() {
    while (1) {
        struct sockaddr_un client_addr;
        char command[MAX_INPUT_SIZE], response[10];
        int n, res, err;
        addrlen=sizeof(struct sockaddr_un);
        n = recvfrom(sockfd, command, sizeof(command)-1, 0, (struct sockaddr *)&client_addr, &addrlen);
        if (n <= 0) {
            printf("socketOn: recvfrom error\n");
            continue;
        }
        res = applyCommands(command);
        sprintf(response, "%d", res);
        response[1] = '\0';
        puts(response);
        err = sendto(sockfd, response, strlen(response), 0, (struct sockaddr *)&client_addr, addrlen);
        if (err < 0) {
            printf("socketOn: sendto error %d %s\n", err, strerror(err));
            continue;
        }
        print_tecnicofs_tree(stdout);
    }
}

/*
 * Allocates memory and initializes the threads that will execute the commands.
 */
void createThreadPool() {
    tid_arr = (pthread_t*) malloc(sizeof(pthread_t) * (numberThreads));
    for (int i = 0; i < numberThreads; i++){
        if (pthread_create((&tid_arr[i]), NULL, (void*)socketOn, NULL) != 0) {
            printf("ERROR: unsuccessful thread creation\n");
            exit(EXIT_FAILURE);
        }
    }
}

/*
 * Waits for all the threads to finish and frees the memory allocated-
 */
void joinThreadPool() {
    for (int i = 0; i < numberThreads; i++) {
        if (pthread_join(tid_arr[i], NULL) != 0) {
            printf("ERROR: unsuccessful thread join\n");
            exit(EXIT_FAILURE);
        }
    }
    free(tid_arr);
}


int main(int argc, char* argv[]) {
    char *socketname = malloc(sizeof(char) * MAX_SOCKET_PATH);
    /* init filesystem */
    init_fs();
    /* parse arguments */
    args(argc, argv, socketname);
    /* create socket */
    createSocket(socketname);
    /* create the threads */
    createThreadPool();
    printf("[SERVER ON]\n");
    /* join the threads */
    joinThreadPool();
    /* print tree */
    print_tecnicofs_tree(stdout);
    /* release allocated memory and close the files */
    destroy_fs();    
    exit(EXIT_SUCCESS);
}
