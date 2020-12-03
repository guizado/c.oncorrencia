#ifndef API_H
#define API_H

#include "tecnicofs-api-constants.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

int sockfd, clilen, servlen;
struct sockaddr_un cli_addr, serv_addr;

int datagram_send(char *command);
int tfsCreate(char *path, char nodeType);
int tfsDelete(char *path);
int tfsLookup(char *path);
int tfsMove(char *from, char *to);
int tfsMount(char* serverName);
int tfsUnmount();

#endif /* CLIENT_H */
