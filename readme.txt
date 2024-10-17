Tema 2 -  Aplicatie client-server TCP si UDP pentru gestionarea mesajelor

Note: for this homework, I have started from Lab 7 by copying the basic structure of the algorithms and the starting points of run_chat_multi_server and run_client, as well as the Makefile. All of these have been updated with the requirements for this homework.

server.c: 
    In main(), the server begins by binding a TCP and a UDP socket. After socket management is done, the server enters its core function, run_chat_multi_server.

    In run_chat_multi_server(), the server simply waits for messages on any of its three main sockets: tcp, udp and stdin, as well as additional sockets that shall connect later.

        For stdin, there can be only one correct command: exit. It shuts down the server after safely removing all clients and sending them exit signals as well.

        For TCP as well, only one thing can happen: we receive connection requests from other clients. After accepting them in order to receive their ID's, the client is checked for eligibility: if the ID is already taken, connection is automatically closed on both ends; else, we store the client in the two main arrays: clients (which contains all information regarding a client) and poll_fds (which manages all active sockets);

        For UDP, the steps are straightforward: receive (max) 1500 bytes, sort them out by copying them in a preset structure, to which we add the UDP client's data. Afterwards, we process the message and send it to any client that is subscribed to the topic.

        For clients, there are 3 possibilities:
            exit, which removes the client, its data and closes connection
            subscribe, which adds a topic in the subscription array of the client
            unsubscribe, which removes a topic from the subscription array of the client

subscriber.c:
    In main(), the subscriber establishes a TCP connection with the server, then waits for an ID check. If the ID is unique, server allows connection and the subscriber starts running.

    In run_client(), the subscriber waits for any signal of communication comming from the poll (stdin and server).

        For stdin, subscriber can receive 3 commands:
            subscribe/unsubscribe topic, where the subscriber sends a buffer with that exact command to the server and then waits for confirmation from the server
            exit, which sends a signal to the server and automatically disconnects the client

        For server, the subscriber can either receive an exit signal (server is closing) or an already processed buffer, containing topic-related information.

helpers.c/.h:
    - struct message: represents the datagrams sent from UDP-clients to TCP-clients via server; contains the topic, type, and payload as well as UDP-client IP and port which are added after reception
    - struct client: contains the ID, socket and subscribed topic of a client, as well as the number of subbed topics
    - struct packet: represents the packets sent through TCP sockets; a packet contains a buffer and its length, to ensure that all bytes are received on the other end

    recv_all(): reads size_t bytes, containing the size of the buffer that is read next, then reads the buffer itself; this function ensures that all bytes are received, since recv() alone may encounter issues for bigger payloads

    send_all(): sends size_t bytes, representing the size of the buffer that is sent next, then sends the buffer itself; same as recv_all(), this function ensures that everything is sent properly

    close_server(): closes all client sockets, after sending each one of them a closing signal

    accept_client(): checks for ID uniqueness, and if everything is ok, space is alloc'd for the new client

    disconnect_client(): disconnects a client upon receiving an exit signal from them by freeing any related memory and closing the socket

    compare_topics(): this function is the equivalent of a mini regex function. It compares two topics by level, treating all cases:
        - for two exact words, compare by words
        - for word and +, treat as equal
        - for word and *, skip as many levels as needed until both strings reach the same level(word)
        - for two *, check the * that covers more levels (by searching each string for the level that comes up right after *) and then continue from that identical level on both the strings
        - if by the end of the parsing any string still has levels, the topics don't match

    subscribe_client(): checks for topic uniqueness(only exact matches, not wildcard matches) and if everything is ok, space is alloc'd for the new topic

    unsubscribe_client(): searches for an identical topic to remove, and if it's found, space is freed

    remove_trailing_zeroes(): processes the real numbers from the message that will be sent to the TCP client; it adds a '\0' between the number and unnecessary zeroes for a cleaner output on the other end

    send_message(): this function takes a message structure and processes the information within, forming a string that contains the final message for the clients; depending on the message type, the algorithm processes the data itself according to the requirements. Lastly, the final message is sent to all subscribed clients




