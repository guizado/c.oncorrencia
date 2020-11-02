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

int numberThreads = 0;

pthread_mutex_t globalMutex;

char inputCommands[MAX_COMMANDS][MAX_INPUT_SIZE];
int numberCommands = 0;
int headQueue = 0;


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



void applyCommands(void *arg){
    int strat = *((int*)arg);
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
        printf("%s\n", command);
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
                        create(name, T_FILE, strat);
                        break;
                    case 'd':
                        printf("Create directory: %s\n", name);
                        create(name, T_DIRECTORY, strat);
                        break;
                    default:
                        fprintf(stderr, "Error: invalid node type\n");
                        exit(EXIT_FAILURE);
                }
                break;
            case 'l': 
                searchResult = lookup(name, strat);
                if (searchResult >= 0)
                    printf("Search: %s found\n", name);
                else
                    printf("Search: %s not found\n", name);
                break;
            case 'd':
                printf("Delete: %s\n", name);
                delete(name, strat);
                break;
            default: { /* error */
                fprintf(stderr, "Error: command to apply\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

void args(int argc, char *argv[], FILE **in, FILE **out, int *ss) {
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
    if (strcmp(argv[4], "mutex") == 0) {*ss = MUTEX;}
    else if (strcmp(argv[4], "rwlock") == 0) {*ss = RWLOCK;}
    else if (strcmp(argv[4], "nosync") == 0) {*ss = NOSYNC;}
    else {
        printf("ERROR: invalid synchronization strategy\n");
        exit(EXIT_FAILURE);
    }
    if (*ss == NOSYNC && numberThreads != 1) {
        printf("WARNING: number of threads has been set to one to accomplish non-synchronization");
        numberThreads = 1;
    }
}


pthread_t * createThreadPool(int strat) {
    int i;
    pthread_t *tid_arr;
    if (numberThreads > 1) {
        if (pthread_mutex_init(&globalMutex, NULL) != 0) {
            printf("ERROR: unsuccessful mutex initialization\n");
            exit(EXIT_FAILURE);
        }
    }
    tid_arr = (pthread_t*) malloc(sizeof(pthread_t) * (numberThreads));
    for (i = 0; i < numberThreads; i++){
        if (pthread_create((&tid_arr[i]), NULL, (void*)applyCommands, (void*)&strat) != 0) {
            printf("ERROR: unsuccessful thread creation");
            exit(EXIT_FAILURE);
        }
    }
    return tid_arr;
}

void joinThreadPool(pthread_t *tid_arr) {
    int i;
    for (i = 0; i < numberThreads; i++) {
        if (pthread_join(tid_arr[i], NULL) != 0) {
            printf("ERROR: unsuccessful thread join");
            exit(EXIT_FAILURE);
        }
    }    
}


int main(int argc, char* argv[]) {
    int strat;
    pthread_t *tid_arr;
    FILE *inputF, *outputF;
    struct timeval ti, tf;
    double final_time;
    /* init filesystem */
    init_fs();
    /* initialization arguments */
    args(argc, argv, &inputF, &outputF, &strat);
    /* Process input from the input file */
    processInput(inputF);
    /* Start timer */
    gettimeofday(&ti, NULL);
    /* Initialize global mutex, create thread pool and apply commands with those threads */
    tid_arr = createThreadPool(strat); 
    /* join threads and release allocated memory */
    joinThreadPool(tid_arr);
    free(tid_arr);
    if (numberThreads > 1) {
        if (pthread_mutex_destroy(&globalMutex) != 0) {
            printf("ERROR: unsuccessful mutex destruction");
            exit(EXIT_FAILURE);
        }
    }
    /* Stop timer and print elapsed time to output file*/
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
