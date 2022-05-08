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
	int sockfd, newsockfd, portno, dest;
	char buffer[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr;
	int n, i, ret;
	socklen_t clilen;
	int clients[MAX_CLIENTS_NO];

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

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	ret = listen(sockfd, MAX_CLIENTS);
	DIE(ret < 0, "listen");

	// se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(sockfd, &read_fds);
	fdmax = sockfd;

	while (1) {
		tmp_fds = read_fds;

		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == sockfd) {
					// a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
					// pe care serverul o accepta
					clilen = sizeof(cli_addr);
					newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
					DIE(newsockfd < 0, "accept");

					// se adauga noul socket intors de accept() la multimea descriptorilor de citire
					FD_SET(newsockfd, &read_fds);
					if (newsockfd > fdmax) {
						fdmax = newsockfd;
					}

					add_client(newsockfd, clients);

					printf("Noua conexiune de la %s, port %d, socket client %d\n",
							inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), newsockfd);

					print_clients(clients);
					send_info_to_clients(clients);
				} else {
					// s-au primit date pe unul din socketii de client,
					// asa ca serverul trebuie sa le receptioneze
					memset(buffer, 0, BUFLEN);
					n = recv(i, buffer, sizeof(buffer), 0);
					DIE(n < 0, "recv");

					if (n == 0) {
						// conexiunea s-a inchis
						printf("Socket-ul client %d a inchis conexiunea\n", i);
						rm_client(i, clients);
						close(i);

						print_clients(clients);
						send_info_to_clients(clients);

						// se scoate din multimea de citire socketul inchis
						FD_CLR(i, &read_fds);
					} else {
						printf("S-a primit de la clientul de pe socketul %d mesajul: %s\n", i, buffer);

						ret = sscanf(buffer, "%d", &dest);
						if (ret < 1 || !is_client_connected(dest, clients)) {
							n = strlen(INVALID_MESSAGE) + 1;
							snprintf(buffer, n, INVALID_MESSAGE);
							dest = i;
						}

						n = send(dest, buffer, n, 0);
						DIE(n < 0, "send");
					}
				}
			}
		}
	}

	close(sockfd);

	return 0;
}
