# Protocoale de comunicatii
# Laborator 7 - TCP
# Echo Server
# Makefile

CFLAGS = -Wall -g -Werror -Wno-error=unused-variable

# Portul pe care asculta serverul
PORT = 12345

# Adresa IP a serverului
IP_SERVER = 127.0.0.1

# ID-ul clientului
ID1 = C1
ID2 = C2

all: server subscriber

# Compileaza server.c
server: server.c helpers.o

# Compileaza subscriber.c
subscriber: subscriber.c helpers.o

helpers.o: helpers.c

.PHONY: clean run_server run_client

# Ruleaza serverul
run_server:
	./server ${PORT}

# Ruleaza clientul 	
run_subscriber:
	./subscriber $(ID1) ${IP_SERVER} ${PORT}

run_second_subscriber:
	./subscriber $(ID2) ${IP_SERVER} ${PORT}

clean:
	rm -rf server subscriber *.o *.dSYM
