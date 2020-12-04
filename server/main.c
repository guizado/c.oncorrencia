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
 * Execute a command and store i-numbers corresponding to
 * locked nodes to unlock after command execution.
 * Input:
 *  - command: string representation of a command
 * Returns: SUCCESS or FAIL
 */ 
int applyCommands(char *command){
    int inodeWaitList[MAX_DEPTH], res, len = 0;

    if (command == NULL){
        return FAIL;
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
        case 'p':
            printf("Print: %s", name);
            res = printFS(name);
            return res;
        default: { /* error */
            fprintf(stderr, "Error: command to apply\n");
            exit(EXIT_FAILURE);
        }
    }
    return FAIL;
}


/*
 * Parses arguments from stdin: number of threads to be used and socket name for server socket
 * Input:
 *  - argc: number of arguments
 *  - argv: the arguments
 *  - socketname: name for the server's socket
 */
void args(int argc, char *argv[], char *socketname) {
    if (argc != 3) {
        fprintf(stderr, "ERROR: invalid argument number\n");
        exit(EXIT_FAILURE);
    }
    if ((numberThreads = atoi(argv[1])) <= 0) {
        fprintf(stderr,"ERROR: number of threads must be a positive integer\n");
        exit(EXIT_FAILURE);
    }
    strcpy(socketname, argv[2]);
}


/*
 * Initializes a given socket address
 * Input:
 *  - path: path for the socket address
 *  - addr: pointer to an uninitialized socket address structure
 * Returns:
 *  - SUN_LEN(addr): length of socket address structure
 *  - FAIL: otherwise
 */
int setSockAddrUn(char *path, struct sockaddr_un *addr) {

  if (addr == NULL)
    return FAIL;

  bzero((char *)addr, sizeof(struct sockaddr_un));
  addr->sun_family = AF_UNIX;
  strcpy(addr->sun_path, path);

  return SUN_LEN(addr);
}


/*
 * Creates a socket for the server, initializing its address
 * Input:
 *  - path: path for the socket address
 * Exit: EXIT_FAILURE on error
 */
void createSocket(char * path) {
    if ((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr,"server: can't open socket");
        exit(EXIT_FAILURE);
    }
    unlink(path);
    addrlen = setSockAddrUn(path, &server_addr);
    if (bind(sockfd, (struct sockaddr *) &server_addr, addrlen) < 0) {
        fprintf(stderr,"server: bind error");
        exit(EXIT_FAILURE);
    }
}


/*
 * Infinite loop that waits for a client request corresponding to a command
 * Protocol:
 *  - Receives: string representing a command to be executed
 *  - Responds: SUCCESS or FAIL
 */
void socketOn() {
    struct sockaddr_un client_addr;
    char command[MAX_INPUT_SIZE], response[3];
    int n, res;

    while (1) {
        addrlen=sizeof(struct sockaddr_un);
        n = recvfrom(sockfd, command, sizeof(command)-1, 0, (struct sockaddr *)&client_addr, &addrlen);
        if (n <= 0) {
            fprintf(stderr,"socketOn: recvfrom error\n");
            continue;
        }
        command[n] = '\0';
        res = applyCommands(command);
        sprintf(response, "%d", res);
        response[2] = '\0';
        if (sendto(sockfd, response, strlen(response), 0, (struct sockaddr *)&client_addr, addrlen) < 0) {
            fprintf(stderr,"socketOn: sendto error\n");
            continue;
        }
    }
}


/*
 * Allocates memory and initializes the threads that will execute the commands.
 */
void createThreadPool() {
    tid_arr = (pthread_t*) malloc(sizeof(pthread_t) * (numberThreads));
    for (int i = 0; i < numberThreads; i++){
        if (pthread_create((&tid_arr[i]), NULL, (void*)socketOn, NULL) != 0) {
            fprintf(stderr,"ERROR: unsuccessful thread creation\n");
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
            fprintf(stderr,"ERROR: unsuccessful thread join\n");
            exit(EXIT_FAILURE);
        }
    }
    free(tid_arr);
}


/*
 * Main
 * Steps:
 *  - 247: initialize file system
 *  - 248: parse input arguments
 *  - 250-252: Create socket and initialize thread pool, waits for client request
 *  - The rest of the program will never run because it continuously waits for client requests, it's just preventive
 *  - 255-256: Wait for all the threads to end and free allocated memory
 */ 
int main(int argc, char* argv[]) {
    char *socketname = malloc(sizeof(char) * MAX_SOCKET_PATH);
    init_fs();
    args(argc, argv, socketname);
    createSocket(socketname);
    createThreadPool();
    printf("[SERVER ON]\n");
    joinThreadPool();
    destroy_fs();    
    exit(EXIT_SUCCESS);
}
