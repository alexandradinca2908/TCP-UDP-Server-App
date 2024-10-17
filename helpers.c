#include "helpers.h"

int recv_all(int sockfd, struct packet *buffer) {
    size_t bytes_received = 0;
    size_t res;
    size_t payload_size = 0;

    //  Read size
    while (bytes_received != sizeof(size_t)) {
        res = recv(sockfd, (size_t *)((char *)&payload_size + bytes_received), 
                    sizeof(size_t) - bytes_received, 0);
        DIE(res < 0, "recv");
        bytes_received += res;
    }
    
    //  Read buffer
    bytes_received = 0;
    char *buf = buffer->buf;
    while (bytes_received < payload_size) {
        res = recv(sockfd, buf + bytes_received, payload_size - bytes_received, 0);
        DIE(res < 0, "recv");

        bytes_received += res;
    }
    
    return bytes_received;
}

int send_all(int sockfd, struct packet *buffer) {
    size_t bytes_sent = 0;
    size_t payload_size = buffer->len;
    size_t res;

    //  Send payload size
    int rc = send(sockfd, &buffer->len, sizeof(size_t), 0);
    DIE(rc < 0, "send");

    //  Send payload
    char *buf = buffer->buf;
    while (bytes_sent < payload_size) {
        res = send(sockfd, buf + bytes_sent, payload_size - bytes_sent, 0);
        DIE(res < 0, "send in send_all");

        bytes_sent += res;
    } 

    return bytes_sent;
}

void close_server(struct pollfd *poll_fds, int num_sockets) {
    //  Close client sockets
    for (int i = FIRST_CLIENT; i < num_sockets; i++) {
        //  Send exit message
        struct packet pack;
        strcpy(pack.buf, "exit");
        pack.len = strlen("exit");

        int rc = send_all(poll_fds[i].fd, &pack);
        DIE(rc <= 0, "send_all close_server");

        close(poll_fds[i].fd);
    }
}

int accept_client(struct client **clients, struct pollfd **poll_fds, 
                int nr_clients, char *id, int id_len, int newsockfd,
                int num_sockets) {

    for (int i = 0; i < nr_clients; i++) {
        if (strncmp((*clients)[i].id, id, id_len) == 0) {
            return 0;
        }
    }

    //  ID is unique. Client can be init
    *clients = realloc((*clients), (nr_clients + 1) * sizeof(struct client));
    DIE((*clients) == NULL, "Realloc clients");

    strcpy((*clients)[nr_clients].id, id);
    (*clients)[nr_clients].sockfd = newsockfd;
    (*clients)[nr_clients].nr_topics = 0;
    (*clients)[nr_clients].topics = NULL;

    *poll_fds = realloc((*poll_fds), (num_sockets + 1) * sizeof(struct pollfd));
    DIE((*poll_fds) == NULL, "Realloc poll_fds");

    (*poll_fds)[num_sockets].fd = newsockfd;
    (*poll_fds)[num_sockets].events = POLLIN;

    return 1;
}

void disconnect_client(struct pollfd **poll_fds, struct client **clients, 
                        char *id, int num_sockets, int nr_clients) {
    int i;
    for (i = 0; i < nr_clients; i++) {
        if (strcmp((*clients)[i].id, id) == 0) {
            break;
        }
    }

    //  Close socket
    close((*poll_fds)[FIRST_CLIENT + i].fd);

    //  Remove client from all arrays
    for (int j = i; j < nr_clients - 1; j++) {
        (*clients)[j] = (*clients)[j + 1];
    }
    memset(&(*clients)[i], 0, sizeof(struct client));

    for (int j = FIRST_CLIENT + i; j < num_sockets - 1; j++) {
        (*poll_fds)[j] = (*poll_fds)[j + 1];
    }
    memset(&(*poll_fds)[num_sockets - 1], 0, sizeof(struct pollfd));

}

int compare_topics(char *t1, char *t2) {
    //  First check if they are identical
    if (strcmp(t1, t2) == 0) {
        return 1;
    }

    //  Individual levels separated by /
    char *p1, *p2;
    p1 = strtok_r(t1, "/", &t1);
    p2 = strtok_r(t2, "/", &t2);

    int ok = 1;
    while (p1 != NULL && p2 != NULL) {
        if (strcmp(p1, p2) != 0) {
            //  Words dont match
            if (*p1 != '+' && *p1 != '*' && *p2 != '+' && *p2 != '*') {
                ok = 0;
                break;

            //  Word and + wildcard    
            } else if ((*p1 == '+' && *p2 != '*') 
                        || (*p2 == '+' && *p1 != '*')) {
                p1 = strtok_r(t1, "/", &t1);
                p2 = strtok_r(t2, "/", &t2);

            //  Something and * wildcard    
            } else if (*p1 == '*') {
                //  Replace as many levels as needed
                p1 = strtok_r(t1, "/", &t1);
                p2 = strtok_r(t2, "/", &t2);
                
                if (p1 == NULL) {
                    p2 = NULL;
                } else {
                   while (p2 != NULL && (strcmp(p1, p2) != 0 && *p2 != '+' && *p2 != '*')) {
                        p2 = strtok_r(t1, "/", &t1);
                    }

                    //  Didn't find a correspondent for the next level
                    if (p2 == NULL) {
                        ok = 0;
                    } 
                }

            } else if (*p2 == '*') {
                //  Replace as many levels as needed
                p1 = strtok_r(t1, "/", &t1);
                p2 = strtok_r(t2, "/", &t2);
                
                if (p2 == NULL) {
                    p1 = NULL;
                } else {
                   while (p1 != NULL && (strcmp(p1, p2) != 0 && *p1 != '+' && *p1 != '*')) {
                        p1 = strtok_r(t1, "/", &t1);
                    }

                    //  Didn't find a correspondent for the next level
                    if (p1 == NULL) {
                        ok = 0;
                    } 
                }
            }

        //  Separate case for two * on the same level    
        } else if (*p1 == '*' && *(p1 + 1) != '\0' && *(p2 + 1) != '\0') {
            p1 = strtok_r(t1, "/", &t1);
            p2 = strtok_r(t2, "/", &t2);

            //  Find how many levels does a * cover
            if (strcmp(p1, p2) != 0) {
                char split1[MAX_TOPIC_LEN + 1], split2[MAX_TOPIC_LEN + 1];
                strcpy(split1, p1 + strlen(p1) + 1);
                strcpy(split2, p2 + strlen(p2) + 1);

                if (strstr(split1, p2)) {
                    while (strcmp(p1, p2) != 0 && p1 != NULL) {
                        p1 = strtok_r(t1, "/", &t1);
                    }
                } else if (strstr(split2, p1)) {
                    while (strcmp(p1, p2) != 0 && p2 != NULL) {
                        p2 = strtok_r(t2, "/", &t2);
                    }
                } else {
                    //  Mismatch
                    ok = 0;
                    break;
                }
            }

        //  Words match    
        } else {
            p1 = strtok_r(t1, "/", &t1);
            p2 = strtok_r(t2, "/", &t2);
        }
    }

    if (p1 != NULL || p2 != NULL) {
        ok = 0;
    }

    return ok;
}

int subscribe_client(struct client *client, char *topic) {
    //  Avoid identical topics
    for (int i = 0; i < (*client).nr_topics; i++) {
        if (strcmp((*client).topics[i], topic) == 0) {
            return 0;
        }
    }

    int nr_topics = (*client).nr_topics;

    //  Alloc space for topic
    (*client).topics = realloc((*client).topics, 
                                (nr_topics + 1) * sizeof(char *));
    DIE((*client).topics == NULL, "Realloc topics");

    (*client).topics[nr_topics] = malloc(strlen(topic) + 1);
    DIE((*client).topics[nr_topics] == NULL, "Malloc topic");

    //  Update data
    strcpy((*client).topics[nr_topics], topic);
    (*client).nr_topics++;

    return 1;
}

int unsubscribe_client(struct client *client, char *topic) {
    //  Check if we can unsub
    int unsubbed = 0;

    for (int i = 0; i < (*client).nr_topics; i++) {
        //  Remove topic
        if (strcmp((*client).topics[i], topic) == 0) {
            free((*client).topics[i]);

            for (int j = i; j < (*client).nr_topics - 1; j++) {
                (*client).topics[j] = (*client).topics[j + 1];
            }

            (*client).nr_topics--;

            (*client).topics = realloc((*client).topics, 
                                (*client).nr_topics * sizeof(char *));
            DIE((*client).topics == NULL && (*client).nr_topics != 0, "Realloc subbed topics");

            unsubbed = 1;
        }
    }

    return unsubbed;
}

void remove_trailing_zeroes(char *str, int decimals) {
    for (int i = strlen(str) - 1; i >= 0; i--) {
        if (str[i] != '0') {
            if (str[i] == '.') {
                str[i] = '\0';
            } else {
                if (decimals == 4) {
                    str[i + 1] = '\0';
                }
            }
            break;
        }
    }
}

void send_message(struct message *udp_buf, struct client *clients,
                struct pollfd *poll_fds, int num_sockets, int nr_clients, int rc) {
    //  Process message
    char message[MAX_MSG_LEN + 50];
    char conversion[32];

    //  Clear message array
    memset(&message, 0, MAX_MSG_LEN + 50);

    //  IP
    inet_ntop(AF_INET, &udp_buf->source_ip, conversion, INET_ADDRSTRLEN);
    strcpy(message, conversion);
    strcat(message, ":");

    //  Port
    sprintf(conversion, "%hu", udp_buf->source_port);
    strcat(message, conversion);
    strcat(message, " - ");

    //  Topic
    int found_null = 0;
    for (int i = 0; i <= MAX_TOPIC_LEN; i++) {
        if (*(udp_buf->topic + i) == '\0') {
            found_null = 1;
            break;
        }
    }
    if (found_null) {
        strcat(message, udp_buf->topic);
    } else {
        strncat(message, udp_buf->topic, MAX_TOPIC_LEN);
    }
    strcat(message, " - ");

    //  Message type and message
    switch(udp_buf->type) {
        case 0:
            strcat(message, "INT - ");
            
            unsigned int int_nr; 
            char *verif = memcpy(&int_nr, udp_buf->payload + 1, sizeof(uint32_t));
            DIE(verif == NULL, "memcpy fail");

            int_nr = ntohl(int_nr);

            //  Check sign
            char sign = udp_buf->payload[0];
            if (sign & 1 && int_nr != 0) {
                strcat(message, "-");
            }

            sprintf(conversion, "%u", int_nr);
            strcat(message, conversion);

            break;

        case 1:
            strcat(message, "SHORT_REAL - ");

            uint16_t short_nr;
            verif = memcpy(&short_nr, udp_buf->payload, sizeof(uint16_t));
            DIE(verif == NULL, "memcpy fail");

            short_nr = ntohs(short_nr);

            sprintf(conversion, "%.2f", short_nr * 1.0 / 100);
            remove_trailing_zeroes(conversion, 2);
            strcat(message, conversion);

            break;

        case 2:
            strcat(message, "FLOAT - ");

            //  Check sign
            //  It's the first byte
            sign = udp_buf->payload[0];

            //  Take number
            //  It's stored in the next 4 bytes after sign
            verif = memcpy(&int_nr, udp_buf->payload + 1, sizeof(uint32_t));
            DIE(verif == NULL, "memcpy fail");

            int_nr = ntohl(int_nr);

            if (sign & 1 && int_nr != 0) {
                strcat(message, "-");
            }
            
            //  Take power
            //  Power is in the 5th byte
            uint8_t pow_ten;
            verif = memcpy(&pow_ten, udp_buf->payload + 1 + 4, sizeof(uint8_t));
            DIE(verif == NULL, "memset fail");
            
            //  Calculate division
            float div = 1;
            while(pow_ten) {
                div *= 10;
                pow_ten--;
            }

            //  Truncate trailing 0's
            sprintf(conversion, "%.4f", int_nr / div);
            remove_trailing_zeroes(conversion, 4);
            strcat(message, conversion);

            break;

        case 3:
            strcat(message, "STRING - ");
            if (strchr(udp_buf->payload, '\0')) {
                strcat(message, udp_buf->payload);
            } else {
                //  Calculate how many chars the payload has
                //  We received rc bytes
                //  From those, substract topic and type
                int size = rc - MAX_TOPIC_LEN - 1;
                strncat(message, udp_buf->payload, size);
            }

            break;
    }

    struct packet send_pack;
    strcpy(send_pack.buf, message);
    send_pack.len = strlen(send_pack.buf) + 1;
    
    char copy1[51];
    char copy2[51];

    for (int i = 0; i < nr_clients; i++) {
        int subbed = 0;

        //  Check subbed array
        for (int j = 0; j < clients[i].nr_topics; j++) {
            strcpy(copy1, udp_buf->topic);
            strcpy(copy2, clients[i].topics[j]);

            if (compare_topics(copy1, copy2)) {
                subbed = 1;
                break;
            }
        }
        
        //  Send message
        if (subbed == 1) {
            send_all(poll_fds[FIRST_CLIENT + i].fd, &send_pack);
        }
    }
}