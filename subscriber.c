#include "helpers.h"

void run_client(int tcp_socket) {
    int rc;
    struct packet send_pack;
    struct packet recv_pack;

    struct pollfd poll_fds[2];
    int num_sockets = 2;

    poll_fds[0].fd = STDIN_FILENO;
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = tcp_socket;
    poll_fds[1].events = POLLIN;

    //  Exit flag
    int exit_flag = 0;
    while (!exit_flag) {
        //  Waiting for a message on any socket
        rc = poll(poll_fds, num_sockets, -1);
        DIE(rc < 0, "poll");

        for (int i = 0; i < num_sockets; i++) {
            if (poll_fds[i].revents & POLLIN) {
                //  STDIN
                if (i == 0) {
                    //  Store buffer
                    char buf[MAX_MSG_LEN + 1];

                    char *verif = memset(buf, 0, sizeof(buf));
                    DIE(verif == NULL, "memset fail");

                    verif = fgets(buf, MAX_MSG_LEN, stdin);
                    DIE(verif == NULL, "fgets fail");

                    if(isspace(buf[0])) {
                        continue;
                    }

                    //  Check command
                    if (strncmp(buf, "subscribe", 9) == 0) {
                        //  Remove newline
                        buf[strlen(buf) - 1] = '\0';

                        strcpy(send_pack.buf, buf);
                        send_pack.len = strlen(buf) + 1;

                        rc = send_all(tcp_socket, &send_pack);
                        DIE(rc <= 0, "send_all subscribe");

                        //  Wait confirmation
                        char *verif = memset((void *)&recv_pack, 0, sizeof(struct packet));
                        DIE(verif == NULL, "memset fail");

                        int rc = recv_all(tcp_socket, &recv_pack);
                        DIE(rc <= 0, "recv_all");

                        if (strncmp(recv_pack.buf, "sub", 3) == 0) {
                            printf("Subscribed to topic %s\n", recv_pack.buf + 3);
                        }    

                    } else if (strncmp(buf, "unsubscribe", 11) == 0) {
                        //  Remove newline
                        buf[strlen(buf) - 1] = '\0';

                        strcpy(send_pack.buf, buf);
                        send_pack.len = strlen(buf) + 1;

                        rc = send_all(tcp_socket, &send_pack);
                        DIE(rc <= 0, "send_all unsubscribe");

                        //  Wait confirmation
                        char *verif = memset((void *)&recv_pack, 0, sizeof(struct packet));
                        DIE(verif == NULL, "memset fail");

                        int rc = recv_all(tcp_socket, &recv_pack);
                        DIE(rc <= 0, "recv_all");

                        if (strncmp(recv_pack.buf, "unsub", 5) == 0) {
                            printf("Unsubscribed from topic %s\n", recv_pack.buf + 5);
                        }

                    } else if (strncmp(buf, "exit", 4) == 0) {
                        //  Send exit signal to server
                        strcpy(send_pack.buf, "exit");
                        send_pack.len = strlen(send_pack.buf) + 1;

                        rc = send_all(tcp_socket, &send_pack);
                        DIE(rc <= 0, "send_all exit");

                        exit_flag = 1;
                        break;
                    } else {
                      continue;
                    }

                //  SERVER         
                } else {
                    char *verif = memset((void *)&recv_pack, 0, sizeof(struct packet));
                    DIE(verif == NULL, "memset fail");

                    int rc = recv_all(tcp_socket, &recv_pack);
                    DIE(rc <= 0, "recv_all");

                    //  Server is closing
                    if (strncmp(recv_pack.buf, "exit", 4) == 0) {
                        exit_flag = 1;
                        break;

                    //  Server sends topic related info
                    } else {
                        printf("%s\n", recv_pack.buf);
                    }
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    DIE (argc != 4, "wrong argument number for subscriber start");

    //  Initalize server data
    struct sockaddr_in server_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    char *verif = memset(&server_addr, 0, socket_len);
    DIE(verif == NULL, "memset fail");

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[3]));
    int rc = inet_pton(AF_INET, argv[2], &server_addr.sin_addr.s_addr);
    DIE(rc <= 0, "inet_pton");

    //  TCP socket
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_socket < 0, "tcp_socket");

    //  Nagle's algorithm variable
    int enable = 1;

    //  Disable Nagle's algorithm
    setsockopt(tcp_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&enable, sizeof(enable));

    //  Connect and send ID
    rc = connect(tcp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    DIE(rc < 0, "connect");

    struct packet pack;
    strcpy(pack.buf, argv[1]);
    pack.len = strlen(pack.buf) + 1; 
    send(tcp_socket, argv[1], MAX_ID_LEN, 0);

    //  If ID is not unique, server won't allow connection
    verif = memset((void *)&pack, 0, sizeof(struct packet));
    DIE(verif == NULL, "memset fail");
                   
    rc = recv_all(tcp_socket, &pack);
    DIE(rc <= 0, "recv_all");

    if (strncmp(pack.buf, "success", 7) == 0) {
        run_client(tcp_socket);
    }

    close(tcp_socket);
    
    return 0;
}
