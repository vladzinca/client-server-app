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

typedef struct Message {
	int command; // 0 = subscribe, 1 = unsubscribe, 2 = exit
	char topic[50];
	int tip_date;
	char valoare_mesaj[1500];
	int sf;
	int ip[16];
	int port;
} Message;

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	int tcpsockfd, n, ret;
	struct sockaddr_in serv_addr;
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

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(tcpsockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	send(tcpsockfd, argv[1], 10, 0);

	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(tcpsockfd, &read_fds);

	while (1) {
		tmp_fds = read_fds;

		ret = select(tcpsockfd + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
			// se citeste comanda de la tastatura
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN - 1, stdin);

			// trimite mesaje la server pe baza comenzii
			if (strncmp(buffer, "subscribe", 9) == 0) {
				Message msg;
				memset(&msg, 0, sizeof(Message));
				msg.command = 0;

				const char s[1] = " ";
				char* t = strtok(buffer, s);
				t = strtok(NULL, s);
				strcpy(msg.topic, t);
				t = strtok(NULL, s);
				int sf = t[0] - '0';

				if (sf != 0 && sf != 1)
					sf = 0;
				msg.sf = sf;

				printf("Subscribed to topic.\n");

				n = send(tcpsockfd, &msg, sizeof(Message), 0);

			} else if (strncmp(buffer, "unsubscribe", 11) == 0) {
				Message msg;
				memset(&msg, 0, sizeof(Message));
				msg.command = 1;

				const char s[1] = " ";
				char* t = strtok(buffer, s);
				t = strtok(NULL, s);
				strcpy(msg.topic, t);

				printf("Unsubscribed from topic.\n");

				n = send(tcpsockfd, &msg, sizeof(Message), 0);

			} else if (strncmp(buffer, "exit", 4) == 0) {
				Message msg;
				memset(&msg, 0, sizeof(Message));
				msg.command = 2;

				n = send(tcpsockfd, &msg, sizeof(Message), 0);

				break;
			}
		}

		if (FD_ISSET(tcpsockfd, &tmp_fds)) {
			// primeste mesajul de la server

			memset(buffer, 0, BUFLEN);
			n = recv(tcpsockfd, buffer, BUFLEN, 0);
			DIE(n < 0, "recv");

			if (n > 0) {
				Message* msg = (Message*)buffer;
				printf("%d:%u - %s - %s - %s\n", msg->ip, msg->port, msg->topic, msg->tip_date, msg->valoare_mesaj);
			}
			else
				break;

		}
	}

	close(tcpsockfd);

	return 0;
}
