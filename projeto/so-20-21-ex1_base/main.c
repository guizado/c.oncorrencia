#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include "fs/operations.h"

#define MAX_COMMANDS 150000
#define MAX_INPUT_SIZE 100
#define MAX_DEPTH 10

/* Global variables */
int numberThreads = 0;
char inputCommands[MAX_COMMANDS][MAX_INPUT_SIZE];
int numberCommands = 0; // totalCommands = 0; numberCommandsInserted = 0;
int insertIndex = 0, removeIndex = 0;

pthread_t *tid_arr;
int finished = 0;

pthread_mutex_t bufferLock;
pthread_cond_t canInsert, canRemove;

struct timeval ti, tf;

/*
 * Initialize locks and conditions for the circular buffer.
 */
void initLocks() {
    pthread_mutex_init(&bufferLock, NULL);
    pthread_cond_init(&canInsert, NULL);
    pthread_cond_init(&canRemove, NULL);
}

/*
 * Inserts a command on the buffer, waits if it is full.
 * Input:
 *  - data: string representing a command
 * Returns: 1
 */
int insertCommand(char* data) {
    pthread_mutex_lock(&bufferLock);
    while (numberCommands == MAX_COMMANDS) {pthread_cond_wait(&canInsert, &bufferLock);}
    strcpy(inputCommands[insertIndex++], data);
    if (insertIndex == MAX_COMMANDS) insertIndex = 0;
    numberCommands++;
    pthread_cond_signal(&canRemove);
    pthread_mutex_unlock(&bufferLock);
    return 1;
}

/*
 * Removes a command from the buffer, waits if it is empty.
 */
char* removeCommand() {
    char * command;
    pthread_mutex_lock(&bufferLock);
    while (numberCommands == 0 && !finished) {pthread_cond_wait(&canRemove, &bufferLock);}
    if (finished && numberCommands == 0) {
        pthread_mutex_unlock(&bufferLock);
        return NULL;
    } 
    command = inputCommands[removeIndex++];
    if (removeIndex == MAX_COMMANDS) removeIndex = 0;
    numberCommands--;
    pthread_cond_signal(&canInsert);
    pthread_mutex_unlock(&bufferLock);
    return command;
}

/*
 * For invalid commands.
 */
void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
    exit(EXIT_FAILURE);
}

/*
 * Opens the inputfile, checks if commands have the correct number of tokens and inserts them.
 * Input:
 *  - inputfile: input file, source of commands
 */
void processInput(FILE * inputfile){
    char line[MAX_INPUT_SIZE];

    /* Start timer */
    gettimeofday(&ti, NULL);

    /* break loop with ^Z or ^D */
    while (fgets(line, sizeof(line)/sizeof(char), inputfile)) {
        char token;
        char name[MAX_INPUT_SIZE], name2[MAX_INPUT_SIZE];

        int numTokens = sscanf(line, "%c %s %s", &token, name, name2);
        /* perform minimal validation */
        if (numTokens < 1) {
            continue;
        }
        switch (token) {
            case 'c':
                if(numTokens != 3)
                    errorParse();
                if(insertCommand(line))
                    break;
                return;
            
            case 'l':
                if(numTokens != 2)
                    errorParse();
                if(insertCommand(line))
                    break;
                return;
            
            case 'd':
                if(numTokens != 2)
                    errorParse();
                if(insertCommand(line))
                    break;
                return;

            case 'm':
                if(numTokens != 3)
                    errorParse();
                if(insertCommand(line))
                    break;
                return;
            
            case '#':
                break;
            
            default: { /* error */
                errorParse();
            }
        }
    }
    /* Treat EOF so threads know when to stop trying to remove commands */
    pthread_mutex_lock(&bufferLock);
    finished = 1;
    pthread_cond_broadcast(&canRemove);
    pthread_mutex_unlock(&bufferLock);
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
void applyCommands(){
    int inodeWaitList[MAX_DEPTH];
    int len = 0;

    while (1){
        const char* command = removeCommand();
        if (command == NULL){
            return;
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
                        create(name, T_FILE, inodeWaitList, &len);
                        unlockAll(inodeWaitList, &len);
                        break;
                    case 'd':
                        printf("Create directory: %s\n", name);
                        create(name, T_DIRECTORY, inodeWaitList, &len);
                        unlockAll(inodeWaitList, &len);
                        break;
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
                break;
            case 'd':
                printf("Delete: %s\n", name);
                delete(name, inodeWaitList, &len);
                unlockAll(inodeWaitList, &len);
                break;
            case 'm':
                printf("Move: %s %s\n", name, name2);
                move(name, name2, inodeWaitList, &len);
                unlockAll(inodeWaitList, &len);
                break;
            default: { /* error */
                fprintf(stderr, "Error: command to apply\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    return;
}

/*
 * Parses arguments: input and output files and the numbers of threads to be used.
 * Input:
 *  - argc: number of arguments
 *  - argv: the arguments
 *  - in: input file
 *  - out: output file
 */
void args(int argc, char *argv[], FILE **in, FILE **out) {
    if (argc != 4) {
        printf("ERROR: invalid argument number\n");
        exit(EXIT_FAILURE);
    }
    *in = fopen(argv[1], "r");
    if (*in == NULL) {
        printf("ERROR: invalid input file\n");
        exit(EXIT_FAILURE);
    }
    *out = fopen(argv[2], "w");
    if (*out == NULL) {
        printf("ERROR: invalid output file\n");
        exit(EXIT_FAILURE);
    }
    if ((numberThreads = atoi(argv[3])) <= 0) {
        printf("ERROR: number of threads must be a positive integer\n");
        exit(EXIT_FAILURE);
    }
}

/*
 * Allocates memory and initializes the threads that will execute the commands.
 */
void createThreadPool() {
    tid_arr = (pthread_t*) malloc(sizeof(pthread_t) * (numberThreads));
    for (int i = 0; i < numberThreads; i++){
        if (pthread_create((&tid_arr[i]), NULL, (void*)applyCommands, NULL) != 0) {
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
    FILE *inputF, *outputF;
    double final_time;
    /* initialize locks */
    initLocks();
    /* init filesystem */
    init_fs();
    /* parse arguments */
    args(argc, argv, &inputF, &outputF);
    /* create the threads */
    createThreadPool();
    /* main tread feeds the buffer and the other threads can start removing */
    /* timer starts here */
    processInput(inputF);
    /* join the threads */
    joinThreadPool();
    /* stop timer and print elapsed time*/
    gettimeofday(&tf, NULL);
    final_time = (tf.tv_sec - ti.tv_sec)*1.0 + (tf.tv_usec - ti.tv_usec)/1000000.0;
    printf("TecnicoFS completed in %.4f seconds\n", final_time);
    /* print tree */
    print_tecnicofs_tree(outputF);
    /* release allocated memory and close the files */
    destroy_fs();    
    if (fclose(inputF) != 0) exit(EXIT_FAILURE);
    if (fclose(outputF) != 0) exit(EXIT_FAILURE);
    exit(EXIT_SUCCESS);
}
