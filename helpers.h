#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <netinet/in.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>

#define MAX_ID_LEN 10
#define MAX_CONNECTIONS 32
#define MAX_MSG_LEN 1500
#define MAX_TOPIC_LEN 50
#define FIRST_CLIENT 3
#define MAX_UDP_BUF_LEN 1553

/*
 * Macro de verificare a erorilor
 * Exemplu:
 * 		int fd = open (file_name , O_RDONLY);
 * 		DIE( fd == -1, "open failed");
 */

#define DIE(assertion, call_description)                                       \
    do {                                                                         \
        if (assertion) {                                                           \
          fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);                       \
          perror(call_description);                                                \
          exit(EXIT_FAILURE);                                                      \
        }                                                                          \
    } while (0)
#endif

struct message {
    char topic[MAX_TOPIC_LEN];
    uint8_t type;
    char payload[MAX_MSG_LEN];
    struct in_addr source_ip;
	in_port_t source_port;
};

struct client {
    char id[11];
    int sockfd;
    int nr_topics;
    char **topics;
};

struct packet {
    size_t len;
    char buf[MAX_MSG_LEN + 50];
};

int recv_all(int sockfd, struct packet *buffer);
int send_all(int sockfd, struct packet *buffer);
void close_server(struct pollfd *poll_fds, int num_sockets);
int accept_client(struct client **clients, struct pollfd **poll_fds, 
                int nr_clients, char *id, int id_len, int newsockfd,
                int num_sockets);
void disconnect_client(struct pollfd **poll_fds, struct client **clients, 
                        char *id, int num_sockets, int nr_clients);
int compare_topics(char *t1, char *t2);
int subscribe_client(struct client *client, char *topic);
int unsubscribe_client(struct client *client, char *topic);
void remove_trailing_zeroes(char *str, int decimals);
void send_message(struct message *udp_buf, struct client *clients,
                struct pollfd *poll_fds, int num_sockets, int nr_clients, int rc);