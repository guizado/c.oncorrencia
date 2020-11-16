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

void initLocks() {
    pthread_mutex_init(&bufferLock, NULL);
    pthread_cond_init(&canInsert, NULL);
    pthread_cond_init(&canRemove, NULL);
}

int insertCommand(char* data) {
    printf("insert\n");
    pthread_mutex_lock(&bufferLock);
    while (numberCommands == MAX_COMMANDS) {pthread_cond_wait(&canInsert, &bufferLock);}
    strcpy(inputCommands[insertIndex++], data);
    if (insertIndex == MAX_COMMANDS) insertIndex = 0;
    numberCommands++;
    pthread_cond_signal(&canRemove);
    pthread_mutex_unlock(&bufferLock);
    return 1;
}

char* removeCommand() {
    char * command;
    printf("remove\n");
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


void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
    exit(EXIT_FAILURE);
}

void processInput(FILE * inputfile){
    char line[MAX_INPUT_SIZE];

    /* Start timer */
    gettimeofday(&ti, NULL);

    /* break loop with ^Z or ^D */
    while (fgets(line, sizeof(line)/sizeof(char), inputfile)) {
        char token, type;
        char name[MAX_INPUT_SIZE];

        int numTokens = sscanf(line, "%c %s %c", &token, name, &type);

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
            
            case '#':
                break;
            
            default: { /* error */
                errorParse();
            }
        }
    }
    pthread_mutex_lock(&bufferLock);
    finished = 1;
    pthread_cond_broadcast(&canRemove);
    pthread_mutex_unlock(&bufferLock);
}



void applyCommands(){
    while (1){
        const char* command = removeCommand();
        if (command == NULL){
            return;
        }
        char token, type;
        char name[MAX_INPUT_SIZE];
        int numTokens = sscanf(command, "%c %s %c", &token, name, &type);
        if (numTokens < 2) {
            fprintf(stderr, "Error: invalid command in Queue\n");
            exit(EXIT_FAILURE);
        }

        int searchResult;
        switch (token) {
            case 'c':
                switch (type) {
                    case 'f':
                        printf("Create file: %s\n", name);
                        create(name, T_FILE);
                        break;
                    case 'd':
                        printf("Create directory: %s\n", name);
                        create(name, T_DIRECTORY);
                        break;
                    default:
                        fprintf(stderr, "Error: invalid node type\n");
                        exit(EXIT_FAILURE);
                }
                break;
            case 'l': 
                searchResult = lookup(name);
                if (searchResult >= 0)
                    printf("Search: %s found\n", name);
                else
                    printf("Search: %s not found\n", name);
                break;
            case 'd':
                printf("Delete: %s\n", name);
                delete(name);
                break;
            default: { /* error */
                fprintf(stderr, "Error: command to apply\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    return;
}

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


void createThreadPool() {
    /* Create thread pool */
    tid_arr = (pthread_t*) malloc(sizeof(pthread_t) * (numberThreads));
    for (int i = 0; i < numberThreads; i++){
        if (pthread_create((&tid_arr[i]), NULL, (void*)applyCommands, NULL) != 0) {
            printf("ERROR: unsuccessful thread creation\n");
            exit(EXIT_FAILURE);
        }
    }
}

void joinThreadPool() {
    /* Join thread pool */
    for (int i = 0; i < numberThreads; i++) {
        if (pthread_join(tid_arr[i], NULL) != 0) {
            printf("ERROR: unsuccessful thread join\n");
            exit(EXIT_FAILURE);
        }
    }
    /* Free allocated memory */
    free(tid_arr);
}



int main(int argc, char* argv[]) {
    FILE *inputF, *outputF;
    double final_time;
    initLocks();
    /* init filesystem */
    init_fs();
    /* initialization arguments */
    args(argc, argv, &inputF, &outputF);
    /* FALTAM COMENTÁRIOS AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA */
    createThreadPool();
    processInput(inputF);
    joinThreadPool();
    /* Stop timer and print elapsed time*/
    gettimeofday(&tf, NULL);
    final_time = (tf.tv_sec - ti.tv_sec)*1.0 + (tf.tv_usec - ti.tv_usec)/1000000.0;
    printf("TecnicoFS completed in %.4f seconds\n", final_time);
    /* print tree */
    print_tecnicofs_tree(outputF);
    /* release allocated memory */
    destroy_fs();    
    if (fclose(inputF) != 0) exit(EXIT_FAILURE);
    if (fclose(outputF) != 0) exit(EXIT_FAILURE);
    exit(EXIT_SUCCESS);
}
