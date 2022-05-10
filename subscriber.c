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

/* un mesaj de la server sau de la subscriber */
typedef struct Mesaj
{
	uint8_t comanda;
	char topic[51];
	char valoare_mesaj[1501];
	uint8_t sf;
	uint8_t ip[16];
	uint16_t port;
} Mesaj;

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	int tcpsockfd, n, ret, flag = 1;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];
	fd_set read_fds, tmp_fds;

	if (argc < 4)
		usage(argv[0]);

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	tcpsockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(tcpsockfd < 0, "socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(tcpsockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	send(tcpsockfd, argv[1], 10, 0);

	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(tcpsockfd, &read_fds);

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	while (1)
	{
		tmp_fds = read_fds;

		ret = select(tcpsockfd + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		/* subscriber-ul da o comanda */
		if (FD_ISSET(STDIN_FILENO, &tmp_fds))
		{
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN - 1, stdin);

			Mesaj msg;
			memset(&msg, 0, sizeof(Mesaj));

			/* subscribe */
			if (strncmp(buffer, "subscribe ", 10) == 0)
			{
				msg.comanda = 0;

				const char s[1] = " ";
				char *t = strtok(buffer, s);
				t = strtok(NULL, s);
				strcpy(msg.topic, t);
				t = strtok(NULL, s);
				int sf = t[0] - '0';

				if (sf != 0 && sf != 1)
					sf = 0;
				msg.sf = sf;

				printf("Subscribed to topic.\n");
			}
			/* unsubscribe */
			else if (strncmp(buffer, "unsubscribe ", 12) == 0)
			{
				msg.comanda = 1;

				const char s[1] = " ";
				char *t = strtok(buffer, s);
				t = strtok(NULL, s);
				strcpy(msg.topic, t);

				printf("Unsubscribed from topic.\n");
			}
			/* exit */
			else if (strncmp(buffer, "exit", 4) == 0)
			{
				msg.comanda = 2;
				flag = 1;
			}

			n = send(tcpsockfd, &msg, sizeof(Mesaj), 0);
			DIE(n < 0, "send");

			if (flag == 0)
				break;
		}
		/* subscriber-ul primeste un mesaj de la server */
		if (FD_ISSET(tcpsockfd, &tmp_fds))
		{
			char got_buffer[sizeof(Mesaj)];
			memset(got_buffer, 0, sizeof(Mesaj));
			n = recv(tcpsockfd, got_buffer, sizeof(Mesaj), 0);
			DIE(n < 0, "recv");

			if (n > 0)
			{
				Mesaj *msg = (Mesaj *)got_buffer;
				if (msg->comanda == 0)
					printf("%s:%u - %s - INT - %s\n", msg->ip, msg->port,
											msg->topic, msg->valoare_mesaj);
				else if (msg->comanda == 1)
					printf("%s:%u - %s - SHORT_REAL - %s\n", msg->ip,
								msg->port, msg->topic, msg->valoare_mesaj);
				else if (msg->comanda == 2)
					printf("%s:%u - %s - FLOAT - %s\n", msg->ip, msg->port,
											msg->topic, msg->valoare_mesaj);
				else if (msg->comanda == 3)
					printf("%s:%u - %s - STRING - %s\n", msg->ip, msg->port,
											msg->topic, msg->valoare_mesaj);
			}
			else
				break;
		}
	}

	close(tcpsockfd);

	return 0;
}
