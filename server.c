#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helpers.h"

#define INVALID_MESSAGE ("Format invalid pentru mesaj!")
#define MAX_CLIENTS_NO 100
#define FD_START 4

typedef struct Message {
	char command;
	char topic[50];
	int tip_date;
	char valoare_mesaj[1500];
	int sf;
	int ip[16];
	int port;
} Message;

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

void init_clients(int* clients) {
	int i;

	for (i = 0; i < MAX_CLIENTS_NO; i++) {
		clients[i] = 0;
	}
}

void print_clients(int* clients) {
	int i;

	printf("Clienti conectat: ");
	for (i = 0; i < MAX_CLIENTS_NO; i++) {
		if (clients[i]) {
			printf("%d ", i + FD_START);
		}
	}
	printf("\n");
}

void write_clients(char* buffer, int* clients) {
	int i;
	int len;

	snprintf(buffer, BUFLEN - 1, "Clienti conectati: ");
	for (i = 0; i < MAX_CLIENTS_NO; i++) {
		if (clients[i]) {
			len = strlen(buffer);
			snprintf(buffer + len, BUFLEN - len - 1, "%d ", i + FD_START);
		}
	}
	len = strlen(buffer);
	snprintf(buffer + len, BUFLEN - len - 1, "\n");
}

void add_client(int id, int* clients) {
	clients[id - FD_START] = 1;
	printf("%d\n", id - FD_START);
}

void rm_client(int id, int* clients) {
	clients[id - FD_START] = 0;
}

int is_client_connected(int id, int* clients) {
	return id >= FD_START && clients[id - FD_START];
}

void send_info_to_clients(int* clients) {
	int i, n;
	char buffer[BUFLEN];

	write_clients(buffer, clients);

	for (i = 0; i < MAX_CLIENTS_NO; i++) {
		if (clients[i]) {
			n = send(i + FD_START, buffer, strlen(buffer) + 1, 0);
			DIE(n < 0, "send");
		}
	}
}

int main(int argc, char *argv[])
{
	int tcpsockfd, udpsockfd, newsockfd, portno, dest;
	char buffer[sizeof(Message)];
	struct sockaddr_in serv_addr, tcp_addr, udp_addr;
	int n, i, ret1, ret3, ret4, ret5;
	socklen_t clilen;
	int clients[MAX_CLIENTS_NO];

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	if (argc < 2) {
		usage(argv[0]);
	}

	init_clients(clients);

	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	tcpsockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(tcpsockfd < 0, "socket");

	udpsockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(udpsockfd < 0, "socket");

	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	ret1 = bind(tcpsockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret1 < 0, "bind");

	memset((char *) &udp_addr, 0, sizeof(udp_addr));
	udp_addr.sin_family = AF_INET;
	udp_addr.sin_port = htons(portno);
	udp_addr.sin_addr.s_addr = INADDR_ANY;

	// ret2 = bind(udpsockfd, (struct sockaddr *) &udp_addr, sizeof(struct sockaddr));
	// DIE(ret2 < 0, "bind");

	ret3 = listen(tcpsockfd, MAX_CLIENTS);
	DIE(ret3 < 0, "listen");

	// se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(tcpsockfd, &read_fds);
	FD_SET(udpsockfd, &read_fds);
	if (tcpsockfd > udpsockfd)
		fdmax = tcpsockfd;
	else fdmax = udpsockfd;

	int flag = 1;
	while (1) {
		tmp_fds = read_fds;

		ret4 = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret4 < 0, "select");

		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == tcpsockfd) {
					// a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
					// pe care serverul o accepta
					clilen = sizeof(udp_addr);
					newsockfd = accept(tcpsockfd, (struct sockaddr *) &tcp_addr, &clilen);
					DIE(newsockfd < 0, "accept");

					// se adauga noul socket intors de accept() la multimea descriptorilor de citire
					FD_SET(newsockfd, &read_fds);
					if (newsockfd > fdmax) {
						fdmax = newsockfd;
					}

					recv(newsockfd, buffer, BUFLEN, 0);
					// add_client(newsockfd, clients);

					printf("New client %s connected from %s:%d.\n",
							buffer, inet_ntoa(tcp_addr.sin_addr), ntohs(tcp_addr.sin_port));

					// print_clients(clients);
					send_info_to_clients(clients);
				} else if (i == udpsockfd) {
				} else if (i == STDIN_FILENO) {
					fgets(buffer, BUFLEN, stdin);
					if (strncmp(buffer, "exit", 4) == 0) { // cu strcmp nu merge, wtf
						flag = 0;
						break;
					}
				} else {
					// s-au primit date pe unul din socketii de client,
					// asa ca serverul trebuie sa le receptioneze
					memset(buffer, 0, sizeof(Message));
					n = recv(i, buffer, sizeof(Message), 0);
					DIE(n < 0, "recv");

					Message* msg = (Message*) buffer;

					if (n == 0) {
						// conexiunea s-a inchis
						// printf("Socket-ul client %d a inchis conexiunea\n", i);
						// rm_client(i, clients);
						close(i);

						// print_clients(clients);
						// send_info_to_clients(clients);

						// se scoate din multimea de citire socketul inchis
						FD_CLR(i, &read_fds);
					} else if (msg->command == 'e') {
						printf("Client C1 disconnected.\n");
						close(i);
						FD_CLR(i, &read_fds);
						break;

					} else {
						// printf("S-a primit de la clientul de pe socketul %d mesajul: %s\n", i, buffer);

						ret5 = sscanf(buffer, "%d", &dest);
						if (ret5 < 1 || !is_client_connected(dest, clients)) {
							n = strlen(INVALID_MESSAGE) + 1;
							// snprintf(buffer, n, INVALID_MESSAGE);
							dest = i;
						}

						n = send(dest, buffer, n, 0);
						DIE(n < 0, "send");
					}
				}
			}
		}
		if (flag == 0)
			break;
	}

	for (int i = 3; i <= fdmax; i++) {
		if (FD_ISSET(i, &read_fds))
			close(i);
	}

	return 0;
}
