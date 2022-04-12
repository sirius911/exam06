/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   serveur.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: clorin <clorin@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2022/04/11 13:18:40 by clorin            #+#    #+#             */
/*   Updated: 2022/04/12 12:40:40 by clorin           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>

#define		NBCLIENT	10
#define		BUFFER		1024

int G_nbClients = 0, G_fd_max = 0;
int G_idClients[NBCLIENT];
char *G_msg[NBCLIENT];
char G_readBufffer[BUFFER + 1], G_writeBuffer[BUFFER + 1];
fd_set readFds, writeFds, fds;

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	printf("extract_message(%s, %s)\n", *buf, *msg);
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void fatal(void)
{
	write(2, "Fatal error\n", 12);
	exit(1);
}

void notify(int from, char *s)
{
	for(int fd = 0; fd <= G_fd_max; fd++)
	{
		if (FD_ISSET(fd, &writeFds) && from != fd)
			send(fd, s, strlen(s), 0);
	}
}

void deliver(int fd)
{
	char *s;

	while(extract_message(&(G_msg[fd]), &s))
	{
		sprintf(G_writeBuffer, "client %d: ", G_idClients[fd]);
		notify(fd, G_writeBuffer);
		notify(fd, s);
		free(s);
		s = NULL;
	}
}

void add_client(int fd)
{
	G_fd_max = fd > G_fd_max ? fd : G_fd_max;
	G_idClients[fd] = G_nbClients++;
	G_msg[fd] = NULL;
	FD_SET(fd, &fds);
	sprintf(G_writeBuffer, "server: client %d just arrived\n", G_idClients[fd]);
	notify(fd, G_writeBuffer);
}

void remove_client(int fd)
{
	sprintf(G_writeBuffer, "server: client %d just left\n", G_idClients[fd]);
	notify(fd, G_writeBuffer);
	free(G_msg[fd]);
	G_msg[fd] = NULL;
	FD_CLR(fd, &fds);
	close(fd);
}

// int create_client(void)
// {
// 	fd_max = socket(AF_INET, SOCK_STREAM, 0);
// 	if (fd_max < 0)
// 		fatal();
// 	FD_SET(fd_max, &fds);
// 	return (fd_max);
// }

int main(int ac, char **av) {
	
	if (ac != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}

	FD_ZERO(&fds);
	//int sockfd = create_client();
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		fatal();
	G_fd_max = sockfd;
	FD_SET(sockfd, &fds);
	struct sockaddr_in servaddr; 
	bzero(&servaddr, sizeof(servaddr)); 
 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1 
	servaddr.sin_port = htons(atoi(av[1]));
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		fatal();
	if (listen(sockfd, 10) != 0)
		fatal();
	printf("server at 127.0.0.1:%d (fd = %d)\n", atoi(av[1]), sockfd);
	while(42)
	{
		readFds = writeFds = fds;
		//printf("Waiting in select()...");
		if (select(G_fd_max + 1, &readFds, &writeFds, NULL, NULL) < 0)
			fatal();
		
		for(int fd = 0; fd <= G_fd_max; fd++)
		{
			if (!FD_ISSET(fd, &readFds))
				continue;
			printf(" connexion fd = %d\n", fd);
			if (fd == sockfd)
			{
				printf("\t\t(%d) => serveur\n", sockfd);
				int len = sizeof(servaddr);
				int client_fd = accept(sockfd, (struct sockaddr *)&servaddr, &len);
				if (client_fd >= 0)
				{
					add_client(client_fd);
					break;
				}	
			}
			else
			{
				printf("\t\tread >>> ");
				int readed = recv(fd, G_readBufffer, BUFFER, 0);
				printf("(%d) octects ", readed);
				if (readed <= 0)
				{
					printf("\n");
					remove_client(fd);
					break;
				}
				G_readBufffer[readed] = '\0';
				printf("%s", G_readBufffer);
				G_msg[fd] = str_join(G_msg[fd], G_readBufffer);
				deliver(fd);
			}
		}
	}
        return (0);
}