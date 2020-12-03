#include "tecnicofs-client-api.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>

#define SERVER_RESPONSE_SIZE 2

int datagram_send(char *command) {
    int err;
    char *rec_buffer = malloc(sizeof(char)*SERVER_RESPONSE_SIZE);
    int n = strlen(command);
    err = sendto(sockfd, command, n, 0, (struct sockaddr*)&serv_addr, servlen);
    if (err != n) {
        printf("datagram_send: sendto error %s\n",strerror(err));
        return -1;
    }
    n = recvfrom(sockfd, rec_buffer, SERVER_RESPONSE_SIZE, 0, 0, 0);
    if (n < 0) {
        printf("datagram_send: recvfrom error\n");
        return -1;
    }
    rec_buffer[n] = '\0';
    puts(rec_buffer);
    return 0;
}

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
  sprintf(command, "d %s", path);
  if (datagram_send(command) < 0) return -1;
  return 0;
}

int tfsMount(char * sockPath) {
  char *str_pid, *cl_path;
  cl_path = malloc(sizeof(char)*MAX_FILE_NAME);
  str_pid = malloc(sizeof(char)* 20);
  sprintf(str_pid, "%d", getpid());
  sprintf(cl_path, "/tmp/client-");
  strcat(cl_path, str_pid);

  if ((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0) ) < 0) {
      printf("tfsMount: client socket error\n");
      return -1;
  }
  bzero((char*) &cli_addr, sizeof(cli_addr));
  cli_addr.sun_family = AF_UNIX;
  strcpy(cli_addr.sun_path, cl_path);
  clilen = sizeof(cli_addr.sun_family) + strlen(cli_addr.sun_path);
  if (bind(sockfd, (struct sockaddr*) &cli_addr, clilen) < 0) {
      printf("tfsMount: client bind error\n");
      return -1;
  }

  bzero((char*) &serv_addr, sizeof(serv_addr));
  serv_addr.sun_family = AF_UNIX;
  strcpy(serv_addr.sun_path, sockPath);
  servlen = sizeof(serv_addr.sun_family) + strlen(serv_addr.sun_path);
  return 0;
}

int tfsUnmount() {
    if (close(sockfd) != 0) {
        printf("tfsUnmount: close error\n");
        return -1;
    }
    if (unlink(cli_addr.sun_path) != 0) {
        printf("tfsUnmount: unlink error\n");
        return -1;
    }
    return 0;
}
