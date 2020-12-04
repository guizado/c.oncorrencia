#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <stdio.h>
#include "tecnicofs-api-constants.h"
#include "tecnicofs-client-api.h"

int sockfd, clilen, servlen;
struct sockaddr_un cli_addr, serv_addr;


/*
 * Sends a request to a server socket. Prints the response to stdout.
 * Protocol:
 *  - sends: string representing a command
 *  - receives: SUCCESS or FAIL
 * Input:
 *  - command: string representing the command
 * Returns: SUCCESS or FAIL 
 */
int datagram_send(char *command) {
    char *rec_buffer = malloc(sizeof(char)*SERVER_RESPONSE_SIZE);
    int n = strlen(command);

    command[n] = '\0';
    if (sendto(sockfd, command, n, 0, (struct sockaddr*)&serv_addr, servlen) != n) {
        fprintf(stderr,"datagram_send: sendto error\n");
        return -1;
    }

    if ((n = recvfrom(sockfd, rec_buffer, SERVER_RESPONSE_SIZE, 0, 0, 0)) < 0) {
        fprintf(stderr,"datagram_send: recvfrom error\n");
        return -1;
    }
    rec_buffer[n] = '\0';
    puts(rec_buffer);
    return 0;
}

/*
 * tfs functions use datagram_send to communicate with the server. 
 * Each function corresponds to a possible operation on tecnicofs.
 * Each function returns 0 in case of success, -1 otherwise.
 */

int tfsCreate(char *filename, char nodeType) {
  char *command = malloc(sizeof(char)*MAX_INPUT_SIZE);
  sprintf(command, "c %s %c", filename, nodeType);
  if (datagram_send(command) < 0) return -1;
  return 0;
}

int tfsDelete(char *path) {
  char *command = malloc(sizeof(char)*MAX_INPUT_SIZE);
  sprintf(command, "d %s", path);
  if (datagram_send(command) < 0) return -1;
  return 0;
}

int tfsMove(char *from, char *to) {
  char *command = malloc(sizeof(char)*MAX_INPUT_SIZE);
  sprintf(command, "m %s %s", from, to);
  if (datagram_send(command) < 0) return -1;
  return 0;
}

int tfsLookup(char *path) {
  char *command = malloc(sizeof(char)*MAX_INPUT_SIZE);
  sprintf(command, "l %s", path);
  if (datagram_send(command) < 0) return -1;
  return 0;
}

int tfsPrint(char *path){
    char *command = malloc(sizeof(char)*MAX_INPUT_SIZE);
    sprintf(command, "p %s", path);
    if (datagram_send(command) < 0) return -1;
    return 0;
}

/*
 * Creates and initializes the client's socket and server's address
 * Input:
 *  - sockPath: path for the server's address
 * Returns:
 *  - 0: Success
 *  - -1: Fail
 */
int tfsMount(char * sockPath) {
  char str_pid[20], cl_path[MAX_FILE_NAME];
  sprintf(str_pid, "%d", getpid());
  sprintf(cl_path, "/tmp/client-");
  strcat(cl_path, str_pid);

  /* Client socket and address */
  if ((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0) ) < 0) {
      fprintf(stderr,"tfsMount: client socket error\n");
      return -1;
  }
  bzero((char*) &cli_addr, sizeof(cli_addr));
  cli_addr.sun_family = AF_UNIX;
  strcpy(cli_addr.sun_path, cl_path);
  clilen = sizeof(cli_addr.sun_family) + strlen(cli_addr.sun_path);
  if (bind(sockfd, (struct sockaddr*) &cli_addr, clilen) < 0) {
      fprintf(stderr,"tfsMount: client bind error\n");
      return -1;
  }

  /* Server address */
  bzero((char*) &serv_addr, sizeof(serv_addr));
  serv_addr.sun_family = AF_UNIX;
  strcpy(serv_addr.sun_path, sockPath);
  servlen = sizeof(serv_addr.sun_family) + strlen(serv_addr.sun_path);
  return 0;
}

/*
 * Close client socket and unlink its path.
 * Returns:
 *  - 0: Success
 *  - -1: Fail
 */
int tfsUnmount() {
    if (close(sockfd) != 0) {
        fprintf(stderr,"tfsUnmount: close error\n");
        return -1;
    }
    if (unlink(cli_addr.sun_path) != 0) {
        fprintf(stderr,"tfsUnmount: unlink error\n");
        return -1;
    }
    return 0;
}
