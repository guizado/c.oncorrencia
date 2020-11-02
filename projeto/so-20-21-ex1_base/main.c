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

typedef enum synchStrategy {NOSYNC, MUTEX, RWLOCK} synchStrategy;
int strat;

int numberThreads = 0;

pthread_mutex_t globalMutex;
pthread_mutex_t fsMutex;
pthread_rwlock_t fsRWLock;

char inputCommands[MAX_COMMANDS][MAX_INPUT_SIZE];
int numberCommands = 0;
int headQueue = 0;

void lock() {
    switch (strat) {
        case 0:
            break;
        case 1:
            if (pthread_mutex_lock(&fsMutex) != 0) exit(EXIT_FAILURE);
            break;
        case 2:
            if (pthread_rwlock_wrlock(&fsRWLock) != 0) exit(EXIT_FAILURE);
            break;
        default:
            /* ERROR */
            break;          
    }
}

void unlock() {
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
            /*ERROR*/
            break;
    }
}


int insertCommand(char* data) {
    if(numberCommands != MAX_COMMANDS) {
        strcpy(inputCommands[numberCommands++], data);
        return 1;
    }
    return 0;
}

char* removeCommand() {
    if(numberCommands > 0){
        numberCommands--;
        return inputCommands[headQueue++];  
    }
    return NULL;
}


void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
    exit(EXIT_FAILURE);
}

void processInput(FILE * inputfile){
    char line[MAX_INPUT_SIZE];

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
}



void applyCommands(){
    while (1){
        if (numberThreads > 1) {
            if (pthread_mutex_lock(&globalMutex) != 0) {
                printf("ERROR: mutex lock\n");
                exit(EXIT_FAILURE);
            }
        }
        if (numberCommands < 1) break;
        const char* command = removeCommand();
        if (numberThreads > 1) {
            if (pthread_mutex_unlock(&globalMutex) != 0) {
                printf("ERROR: mutex unlock");
                exit(EXIT_FAILURE);
            }
        }
        if (command == NULL){
            break;
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
                        lock();
                        create(name, T_FILE);
                        unlock();
                        break;
                    case 'd':
                        printf("Create directory: %s\n", name);
                        lock();
                        create(name, T_DIRECTORY);
                        unlock();
                        break;
                    default:
                        fprintf(stderr, "Error: invalid node type\n");
                        exit(EXIT_FAILURE);
                }
                break;
            case 'l': 
                lock();
                searchResult = lookup(name);
                unlock();
                if (searchResult >= 0)
                    printf("Search: %s found\n", name);
                else
                    printf("Search: %s not found\n", name);
                break;
            case 'd':
                printf("Delete: %s\n", name);
                lock();
                delete(name);
                unlock();
                break;
            default: { /* error */
                fprintf(stderr, "Error: command to apply\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

void args(int argc, char *argv[], FILE **in, FILE **out) {
    if (argc != 5) {
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
    if (strcmp(argv[4], "mutex") == 0) {strat = MUTEX;}
    else if (strcmp(argv[4], "rwlock") == 0) {strat = RWLOCK;}
    else if (strcmp(argv[4], "nosync") == 0) {strat = NOSYNC;}
    else {
        printf("ERROR: invalid synchronization strategy\n");
        exit(EXIT_FAILURE);
    }
    if (strat == NOSYNC && numberThreads != 1) {
        printf("WARNING: number of threads has been set to one to accomplish non-synchronization\n");
        numberThreads = 1;
    }
}


void threadPool() {
    int i;
    pthread_t *tid_arr;
    /* Initialize locks */
    if (numberThreads > 1) {
        if (pthread_mutex_init(&globalMutex, NULL) || pthread_mutex_init(&fsMutex, NULL)) {
            printf("ERROR: unsuccessful mutex initialization\n");
            exit(EXIT_FAILURE);
        }
        if (pthread_rwlock_init(&fsRWLock, NULL)) {
            printf("ERROR: unsuccessful rw-lock initialization\n");
            exit(EXIT_FAILURE);
        }

    }
    /* Create thread pool */
    tid_arr = (pthread_t*) malloc(sizeof(pthread_t) * (numberThreads));
    for (i = 0; i < numberThreads; i++){
        if (pthread_create((&tid_arr[i]), NULL, (void*)applyCommands, NULL) != 0) {
            printf("ERROR: unsuccessful thread creation\n");
            exit(EXIT_FAILURE);
        }
    }
    /* Join thread pool */
    for (i = 0; i < numberThreads; i++) {
        if (pthread_join(tid_arr[i], NULL) != 0) {
            printf("ERROR: unsuccessful thread join\n");
            exit(EXIT_FAILURE);
        }
    }
    /* free allocated memory and destroy locks */
    free(tid_arr);
    if (numberThreads > 1) {
        if (pthread_mutex_destroy(&globalMutex) || pthread_mutex_destroy(&fsMutex)) {
            printf("ERROR: unsuccessful mutex destructio\n");
            exit(EXIT_FAILURE);
        }
        if (pthread_rwlock_destroy(&fsRWLock)) {
            printf("ERROR: rw-lock destruction\n");
            exit(EXIT_FAILURE);
        }
    }
}


int main(int argc, char* argv[]) {
    FILE *inputF, *outputF;
    struct timeval ti, tf;
    double final_time;
    /* init filesystem */
    init_fs();
    /* initialization arguments */
    args(argc, argv, &inputF, &outputF);
    /* Process input from the input file */
    processInput(inputF);
    /* Start timer */
    gettimeofday(&ti, NULL);
    /* Initialize global mutex, create thread pool and apply commands with those threads */
    /* join threads and release allocated memory */
    threadPool(); 
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
