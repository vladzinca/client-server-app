#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helpers.h"
#include <netinet/tcp.h>

#define INVALID_MESSAGE ("Format invalid pentru mesaj!")
#define MAX_CLIENTS_NO 100
#define ID_LEN 10
#define TOPICS_AMOUNT 32

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

/* o topica si flag-ul de store-and-forward */
typedef struct Topica
{
	char topic[51];
	uint8_t sf;
} Topica;

/* un payload de la client UDP */
typedef struct Payload
{
	char topic[50];
	uint8_t tip_date;
	char valoare_mesaj[1501];
} Payload;

int max(int a, int b)
{
	if (a > b)
		return a;
	else
		return b;
}

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

void init_clients(int *clients)
{
	int i;

	for (i = 0; i < MAX_CLIENTS_NO; i++)
	{
		clients[i] = 0;
	}
}

int main(int argc, char *argv[])
{
	int tcpsockfd, newsockfd, udpsockfd, portno;
	char buffer[sizeof(Mesaj)];
	struct sockaddr_in serv_addr, new_addr, udp_addr;
	int n, i, j, k, ret, poz, online, topica, nagle = 1, flag = 1;
	socklen_t client_size = sizeof(struct sockaddr);
	int clients[MAX_CLIENTS_NO], online_status[MAX_CLIENTS_NO];
	int topics_counter[MAX_CLIENTS_NO], stored_topics_counter[MAX_CLIENTS_NO];
	char *ids[MAX_CLIENTS_NO];
	Topica *topics[MAX_CLIENTS_NO];
	Mesaj *stored_topics[MAX_CLIENTS_NO];

	fd_set read_fds;
	fd_set tmp_fds;
	int fdmax;

	if (argc < 2)
	{
		usage(argv[0]);
	}

	init_clients(clients);
	init_clients(online_status);
	init_clients(topics_counter);
	init_clients(stored_topics_counter);

	for (i = 0; i < MAX_CLIENTS_NO; i++)
	{
		ids[i] = calloc(ID_LEN, sizeof(char));
		topics[i] = calloc(TOPICS_AMOUNT, sizeof(Topica));
		stored_topics[i] = calloc(TOPICS_AMOUNT, sizeof(Mesaj));
	}

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	tcpsockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(tcpsockfd < 0, "socket");

	udpsockfd = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(udpsockfd < 0, "socket");

	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	memset((char *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(tcpsockfd, (struct sockaddr *)&serv_addr,
				sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	ret = listen(tcpsockfd, MAX_CLIENTS_NO);
	DIE(ret < 0, "listen");

	ret = setsockopt(tcpsockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&nagle,
						sizeof(int));
	DIE(ret < 0, "nagle");

	memset((char *)&udp_addr, 0, sizeof(udp_addr));
	udp_addr.sin_family = AF_INET;
	udp_addr.sin_port = htons(portno);
	udp_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(udpsockfd, (struct sockaddr *)&udp_addr,
				sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(tcpsockfd, &read_fds);
	FD_SET(udpsockfd, &read_fds);
	fdmax = max(tcpsockfd, udpsockfd);

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	while (1)
	{
		tmp_fds = read_fds;

		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		for (i = 0; i < fdmax + 1; i++)
		{
			if (FD_ISSET(i, &tmp_fds))
			{
				if (i == STDIN_FILENO)
				{
					fgets(buffer, BUFLEN, stdin);
					if (strncmp(buffer, "exit", 4) == 0)
					{
						flag = 0;
						break;
					}
				}
				else if (i == tcpsockfd)
				{
					newsockfd = accept(tcpsockfd, (struct sockaddr *)&new_addr,
										&client_size);
					DIE(newsockfd < 0, "accept");

					recv(newsockfd, buffer, ID_LEN, 0);

					poz = 0, online = 0;
					for (j = 5; j < fdmax + 1; j++)
						if (strcmp(buffer, ids[j]) == 0)
						{
							poz = j;
							online = online_status[j];
							break;
						}
					/* subscriber-ul este vechi */
					if (poz)
					{
						if (online)
						/* subscriber-ul e online si incearca
							sa se conecteze din nou */
						{
							close(newsockfd);
							printf("Client %s already connected.\n", buffer);
						}
						/* subscriber-ul e offline, caz in care
							revine online */
						else
						{
							FD_SET(newsockfd, &read_fds);

							clients[poz] = newsockfd;
							online_status[poz] = 1;

							printf("New client %s connected from %s:%u.\n",
									buffer, inet_ntoa(new_addr.sin_addr),
									ntohs(new_addr.sin_port));

							for (k = 0; k < stored_topics_counter[poz]; k++)
								send(clients[poz], &stored_topics[poz][k],
										sizeof(Mesaj), 0);
							stored_topics_counter[poz] = 0;
						}
					}
					/* subscriber-ul este nou */
					else
					{
						FD_SET(newsockfd, &read_fds);
						fdmax = newsockfd;

						strcpy(ids[newsockfd], buffer);
						clients[newsockfd] = newsockfd;
						online_status[newsockfd] = 1;

						printf("New client %s connected from %s:%u.\n", buffer,
								inet_ntoa(new_addr.sin_addr),
								ntohs(new_addr.sin_port));
					}
				}
				else if (i == udpsockfd)
				{
					/* primim un mesaj de tip payload UDP */
					Payload *got_message = (Payload *)buffer;

					memset(buffer, 0, sizeof(Mesaj));
					ret = recv(udpsockfd, buffer, sizeof(got_message->topic) +
								sizeof(got_message->valoare_mesaj), 0);

					/* construim pe baza lui un mesaj pe care sa il trimitem
						subscriberilor */
					Mesaj msg;
					memset(&msg, 0, sizeof(Mesaj));
					msg.comanda = got_message->tip_date;
					strcpy(msg.topic, got_message->topic);
					strcpy((char *)msg.ip, inet_ntoa(udp_addr.sin_addr));
					msg.port = htons(udp_addr.sin_port);

					/* INT */
					if (got_message->tip_date == 0)
					{
						int valoare_mesaj = ntohl(*(int *)
											(got_message->valoare_mesaj + 1));

						if (got_message->valoare_mesaj[0] == 1)
							valoare_mesaj = (-1) * valoare_mesaj;

						sprintf(msg.valoare_mesaj, "%d", valoare_mesaj);
					}
					/* SHORT_REAL */
					else if (got_message->tip_date == 1)
					{
						float valoare_mesaj = ntohs(*(int *)
												(got_message->valoare_mesaj));

						valoare_mesaj = valoare_mesaj / 100;

						sprintf(msg.valoare_mesaj, "%.2f", valoare_mesaj);
					}
					/* FLOAT */
					else if (got_message->tip_date == 2)
					{
						float valoare_mesaj = ntohl(*(int *)
											(got_message->valoare_mesaj + 1));

						if (got_message->valoare_mesaj[0] == 1)
							valoare_mesaj = (-1) * valoare_mesaj;
						int p = 1;
						for (j = 0; j < got_message->valoare_mesaj[5]; j++)
							p = p * 10;
						valoare_mesaj = valoare_mesaj / p;

						sprintf(msg.valoare_mesaj, "%f", valoare_mesaj);
					}
					/* STRING */
					else if (got_message->tip_date == 3)
					{
						char valoare_mesaj[1501];
						strcpy(valoare_mesaj, got_message->valoare_mesaj);

						sprintf(msg.valoare_mesaj, "%s", valoare_mesaj);
					}

					/* trimitem mesajul clientilor online si il salvam
						in stored_topics pentru abonatii offline */
					for (j = 5; j < fdmax + 1; j++)
						for (k = 0; k < topics_counter[j]; k++)
							if (strcmp(msg.topic, topics[j][k].topic) == 0)
							{
								if (online_status[j])
									send(clients[j], &msg, sizeof(Mesaj), 0);
								else if (topics[j][k].sf == 1)
									stored_topics[j]
										[stored_topics_counter[j]++] = msg;
								else
									break;
							}
				}
				/* primim date de la un subscriber */
				else
				{
					memset(buffer, 0, sizeof(Mesaj));
					n = recv(i, buffer, sizeof(Mesaj), 0);
					DIE(n < 0, "recv");

					Mesaj *msg = (Mesaj *)buffer;
					poz = 0;
					for (int j = 5; j < fdmax + 1; j++)
						if (i == clients[j])
						{
							poz = j;
							break;
						}

					/* subscribe */
					if (msg->comanda == 0)
					{
						strcpy(topics[poz][topics_counter[poz]].topic,
								msg->topic);
						topics[poz][topics_counter[poz]].sf = msg->sf;
						topics_counter[poz] += 1;
					}
					/* unsubscribe */
					else if (msg->comanda == 1)
					{
						topica = -1;
						for (j = 0; j < topics_counter[poz]; j++)
							if (strcmp(msg->topic,
										topics[poz][j].topic) == 0)
							{
								topica = j;
								break;
							}
						if (topica != -1)
						{
							for (j = topica; j < topics_counter[poz]; j++)
								topics[poz][j] = topics[poz][j + 1];
							topics_counter[poz] -= 1;
						}
					}
					/* exit */
					else if (msg->comanda == 2) // exit
					{
						for (j = 5; j < fdmax + 1; j++)
							if (clients[j] == i)
							{
								FD_CLR(i, &read_fds);
								close(i);

								printf("Client %s disconnected.\n", ids[j]);

								clients[j] = -1;
								online_status[j] = 0;

								break;
							}
					}
				}
			}
		}
		if (flag == 0)
			break;
	}

	/* inchidem toti socketii */
	for (i = 3; i < fdmax + 1; i++)
	{
		if (FD_ISSET(i, &read_fds))
		{
			FD_CLR(i, &read_fds);
			close(i);
		}
	}
	close(tcpsockfd);
	close(udpsockfd);

	return 0;
}
