#include "helpers.h"

void run_chat_multi_server(int udp_socket, int tcp_socket) {
    //  Server poll
    struct pollfd *poll_fds = malloc(sizeof(struct pollfd) * 3);
    int num_sockets = 3;
    int rc;

    //  Packets
    struct packet send_pack;
    struct packet recv_pack;
    struct message *udp_buf;

    //  UDP
    poll_fds[0].fd = udp_socket;
    poll_fds[0].events = POLLIN;
    //  TCP
    poll_fds[1].fd = tcp_socket;
    poll_fds[1].events = POLLIN;
    //  STDIN
    poll_fds[2].fd = STDIN_FILENO;
    poll_fds[2].events = POLLIN;


    //  Client data
    struct client *clients = NULL;
    int nr_clients = 0;
    //  Client auxiliary data
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    //  Exit flag
    int exit_flag = 0;

    //  Server execution
    while (!exit_flag) {
        //  Waiting for a message on any socket
        rc = poll(poll_fds, num_sockets, -1);
        DIE(rc < 0, "poll");

        //  Check sockets
        for (int i = 0; i < num_sockets; i++) {
            if (poll_fds[i].revents & POLLIN) {
                //  STDIN
                if (i == 2) {
                    char buffer[5];
                    char *verif = fgets(buffer, 5, stdin);
                    DIE(verif == NULL, "fgets failed");

                    if (strncmp(buffer, "exit", 4) == 0) {
                        close_server(poll_fds, num_sockets);
                        exit_flag = 1;
                        break;
                    }

                //  TCP socket
                } else if (i == 1) {
                    //  Connection request
                    client_len = sizeof(client_addr);
                    const int newsockfd =
                        accept(tcp_socket, (struct sockaddr *)&client_addr, &client_len);
                    DIE(newsockfd < 0, "accept");

                    //  Disable Nagle's algorithm
                    int enable = 1;
                    setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, (char*)&enable, sizeof(int));

                    //  Read client ID
                    char id[MAX_ID_LEN + 1];
                    int id_len = recv(newsockfd, id, MAX_ID_LEN, 0);
                    DIE(id_len == 0, "Couldn't read ID");

                    //  Check if ID is unique
                    int ack = accept_client(&clients, &poll_fds, nr_clients, id, id_len, 
                                            newsockfd, num_sockets);

                    if (ack == 0) {
                        //  Deny client
                        printf("Client %s already connected.\n", id);

                        strcpy(send_pack.buf, "fail");
                        send_pack.len = strlen(send_pack.buf) + 1;

                        rc = send_all(newsockfd, &send_pack);
                        DIE(rc <= 0, "send_all fail");

                        //  Close socket
                        close(newsockfd);
                    } else {
                        //  Accept client
                        strcpy(send_pack.buf, "success");
                        send_pack.len = strlen(send_pack.buf) + 1;

                        rc = send_all(newsockfd, &send_pack);
                        DIE(rc <= 0, "send_all success");

                        //  Add new socket to poll
                        poll_fds[num_sockets].fd = newsockfd;
                        poll_fds[num_sockets].events = POLLIN;
                        num_sockets++;
                        nr_clients++;

                        printf("New client %s connected from %s:%u.\n", id,
                                inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
                    }
                    
                //  UDP socket
                } else if (i == 0) {
                    //  Alloc struct
                    udp_buf = malloc(sizeof(struct message));

                    char *verif = memset(udp_buf, 0, sizeof(struct message));
                    DIE(verif == NULL, "memset fail");

                    client_len = sizeof(client_addr);

                    char buf_cast[MAX_UDP_BUF_LEN];

                    //  Read data from udp
                    int rc = recvfrom(poll_fds[i].fd, buf_cast, MAX_UDP_BUF_LEN,
                                        0, (struct sockaddr*)&client_addr, &client_len);
                    DIE(rc < 0, "recvfrom");

                    //  Copy data in message struct
                    verif = memcpy(udp_buf, buf_cast, sizeof(struct message));
                    DIE(verif == NULL, "memcpy fail");
                    
                    udp_buf->source_ip = client_addr.sin_addr;
                    udp_buf->source_port = client_addr.sin_port;

                    //  Prep message to be sent to all eligible clients
                    send_message(udp_buf, clients, poll_fds, num_sockets, nr_clients, rc);

                    free(udp_buf);

                //  Client
                } else {
                    char *verif = memset((void *)&recv_pack, 0, sizeof(struct packet));
                    DIE(verif == NULL, "memset fail");
                    
                    int rc = recv_all(poll_fds[i].fd, &recv_pack);
                    DIE(rc <= 0, "recv_all");
                    
                    //  Extract client
                    int client_index = i;
                    struct client *clt = &clients[client_index - FIRST_CLIENT];

                    //  Client is disconnecting
                    if (strncmp(recv_pack.buf, "exit", 4) == 0) {
                        char id[MAX_ID_LEN + 1];
                        strcpy(id, (*clt).id);

                        //  Disconnect client
                        disconnect_client(&poll_fds, &clients, id, num_sockets, nr_clients);
                        
                        printf("Client %s disconnected.\n", id);

                        //  Update vars
                        num_sockets--;
                        nr_clients--;

                        clients = realloc(clients, nr_clients * sizeof(struct client));
                        DIE(clients == NULL && nr_clients != 0, "Realloc clients");

                        poll_fds = realloc(poll_fds, num_sockets * sizeof(struct pollfd));
                        DIE(poll_fds == NULL && num_sockets != 0, "Realloc poll_fds");

                    //  Client is subscribing
                    } else if (strncmp(recv_pack.buf, "subscribe", 9) == 0) {
                        //  Extract topic
                        char *topic = strchr(recv_pack.buf, ' ') + 1;
                        DIE(topic == NULL, "null topic");
                        subscribe_client(clt, topic);

                        //  Send confirmation text
                        //  Confimation payload is "sub" + topic
                        char conf[MAX_TOPIC_LEN + 4] = "sub";
                        strcat(conf, topic);
                        strcpy(send_pack.buf, conf);
                        send_pack.len = strlen(conf) + 1;

                        rc = send_all(poll_fds[client_index].fd, &send_pack);
                        DIE(rc <= 0, "send_all subscribe");
                                              
                    } else if (strncmp(recv_pack.buf, "unsubscribe", 11) == 0) {
                        //  Extract topic
                        char *topic = strchr(recv_pack.buf, ' ') + 1;
                        DIE(topic == NULL, "null topic");
                        int unsubbed = unsubscribe_client(clt, topic);

                        //  Send confirmation text
                        //  Confimation payload is "unsub" + topic
                        if (unsubbed) {
                            char conf[MAX_TOPIC_LEN + 6] = "unsub";
                            strcat(conf, topic);
                            strcpy(send_pack.buf, conf);
                            send_pack.len = strlen(conf) + 1;

                            rc = send_all(poll_fds[client_index].fd, &send_pack);
                            DIE(rc <= 0, "send_all unsubscribe");
                        } else {
                            strcpy(send_pack.buf, "fail unsub");
                            send_pack.len = strlen(send_pack.buf) + 1;

                            rc = send_all(poll_fds[client_index].fd, &send_pack);
                            DIE(rc <= 0, "send_all unsubscribe");
                        }

                    } else {
                        continue;
                    }
                }   
            }
        }  
    }
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    DIE (argc != 2, "wrong argument number for server start");

    //  Initalize server data
    struct sockaddr_in server_addr;

    char *verif = memset(&server_addr, 0, sizeof(server_addr));
    DIE(verif == NULL, "memset fail");

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(atoi(argv[1]));

    //  UDP socket
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udp_socket < 0, "udp_socket");

    //  Bind UDP socket to server address
    int rc = bind(udp_socket, (const struct sockaddr *)&server_addr, sizeof(server_addr));
    DIE(rc < 0, "udp_socket bind");

    //  TCP socket
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_socket < 0, "tcp_socket");
    
    //  Nagle's algorithm variable
    int enable = 1;

    //  Disable Nagle's algorithm
    setsockopt(tcp_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&enable, sizeof(enable));

    rc = bind(tcp_socket, (struct sockaddr *)&server_addr,
              sizeof(struct sockaddr));
    DIE(rc < 0, "bind");

    rc = listen(tcp_socket, MAX_CONNECTIONS);
    DIE(rc < 0, "listen");

    run_chat_multi_server(udp_socket, tcp_socket);

    close(tcp_socket);
    close(udp_socket);

    return 0;
}