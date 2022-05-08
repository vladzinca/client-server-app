#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"

typedef struct mesaj_udp {
	char topic[50];
	uint8_t tip_date;
	char continut[1500];
} mesaj_udp;

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	int tcpsockfd, n, ret;
	struct sockaddr_in tcp_addr;
	char buffer[BUFLEN];
	fd_set read_fds, tmp_fds;

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	if (argc < 4) {
		usage(argv[0]);
	}

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	tcpsockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(tcpsockfd < 0, "socket");

	tcp_addr.sin_family = AF_INET;
	tcp_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &tcp_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(tcpsockfd, (struct sockaddr*) &tcp_addr, sizeof(tcp_addr));
	DIE(ret < 0, "connect");

	send(tcpsockfd, argv[1], 10, 0);

	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(tcpsockfd, &read_fds);

	while (1) {
		tmp_fds = read_fds;

		ret = select(tcpsockfd + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
			// se citeste de la tastatura
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN - 1, stdin);

			if (strncmp(buffer, "exit", 4) == 0) {
				break;
			}

			// se trimite mesaj la server
			n = send(tcpsockfd, buffer, strlen(buffer), 0);
			DIE(n < 0, "send");
		}

		if (FD_ISSET(tcpsockfd, &tmp_fds)) {
			memset(buffer, 0, BUFLEN);
			n = recv(tcpsockfd, buffer, BUFLEN, 0);
			DIE(n < 0, "recv");

			// printf("Msg: %s\n", buffer);
		}
	}

	close(tcpsockfd);

	return 0;
}
