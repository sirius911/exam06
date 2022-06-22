#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct s_client
{
    int               id;
    int               fd;
    char              *str;
    struct s_client    *next;
}               t_client;

//globals
int                 g_id = 0;
int                 g_socket = -1;
t_client            *g_clients = NULL;

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
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

void        del_client(t_client *client) {
    if(client->str)
        free(client->str);
    close(client->fd);
    free(client);
}

void        ft_exit(void) {
    write(2, "Fatal error\n", 12);
    //ferme socket d'écoute
    if(g_socket != -1)
        close(g_socket);
    //ferme tous les clients
    t_client    *next;
    while(g_clients) {
        next = g_clients->next;
        del_client(g_clients);
        g_clients = next;
    }
    exit(1);
}

t_client   **lstnext(void) {
    t_client *last = g_clients;
    if(g_clients == NULL)
        return (&g_clients);
    while(last->next != NULL)
        last = last->next;
    return (&last->next);
}

void        send_one_client(t_client *client, const char *msg) {
    int ret;
    int sent = 0;
    int len = strlen(msg);
    while(sent < len)
    {
        ret = send(client->fd, msg + sent, len - sent, MSG_DONTWAIT);
        if (ret <= 0)
            return;
        sent += ret;
    }
}

void        send_to_clients(const char *msg, int except_id) {
    t_client    *client = g_clients;
    while(client) {
        if(client->id != except_id)
            send_one_client(client, msg);
        client = client->next;
    }
}

void       init_serv(int port) {
    
    struct sockaddr_in servaddr;

    // socket create and verification 
	g_socket = socket(AF_INET, SOCK_STREAM, 0); 
	if (g_socket == -1) 
		ft_exit();
    bzero(&servaddr, sizeof(servaddr));
    // assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port);

    if ((bind(g_socket, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		ft_exit();
    if (listen(g_socket, 10) != 0)
        ft_exit();
}

void    new_client(fd_set   *set) {
    struct sockaddr_in  cli;
    socklen_t           len = sizeof(cli);
    t_client            *client = malloc(sizeof(t_client));
    bzero(client, sizeof(t_client));
    t_client    **ptrlast = lstnext();
    *ptrlast = client;
    client->fd = accept(g_socket, (struct sockaddr *) &cli, &len);
    if (client->fd < 0)
        ft_exit();
    client->id = g_id;
    g_id++;
    {
        char msg[50];
        sprintf(msg, "server: client %d just arrived\n", client->id);
        send_to_clients(msg, client->id);
    }
    FD_SET(client->fd, set);
}

void    remove_client(t_client *client_to_delete, fd_set *origin) {
    t_client    *curr = g_clients;
    FD_CLR(client_to_delete->fd, origin);
    if(curr == client_to_delete) {
        g_clients = g_clients->next;
        del_client(client_to_delete);
        return;
    }
    while(curr->next != client_to_delete)
        curr = curr->next;
    curr->next = client_to_delete->next;
    del_client(client_to_delete);
}

void    ft_recv(t_client *client, fd_set *origin) {
    char    buff[125001];
    char    *msg;
    int     ret;
    ret = recv(client->fd, buff, 125000, MSG_DONTWAIT);
    if(ret == 0) {
        //client a quitté
        sprintf(buff, "server: client %d just left\n", client->id);
        send_to_clients(buff, client->id);
        remove_client(client, origin);
        return;
    }
    if (ret < 0)
        return;
    buff[ret] = '\0';
    client->str = str_join(client->str, buff);
    while((ret = extract_message(&client->str, &msg)) == 1) {
        sprintf(buff, "client %d: %s", client->id, msg);
        send_to_clients(buff, client->id);
        free(msg);
    }
    if(ret == -1)
        ft_exit();
}

void    main_loop() {
   fd_set   origin, cpy;
   FD_ZERO(&origin);
   FD_SET(g_socket, &origin);
   while(42) {
        cpy = origin;
        if (select(FD_SETSIZE, &cpy, NULL, NULL, NULL) == -1)
            ft_exit();
        if (FD_ISSET(g_socket, &cpy))
            new_client(&origin);
        else {
            t_client *curr, *next;
            curr = g_clients;
            while(curr) {
                next = curr->next;
                if(FD_ISSET(curr->fd, &cpy))
                    ft_recv(curr, &origin);
                curr = next;
            }
        }
   }
}

int main(int argc, char ** argv) {
    if(argc != 2)
    {
        write(2, "Wrong number of arguments\n", 26);
        exit(1);
    }
    init_serv(atoi(argv[1]));
    main_loop();
	// int sockfd, connfd, len;
	// struct sockaddr_in servaddr, cli; 

	// socket create and verification 
	// sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	// if (sockfd == -1) { 
	// 	printf("socket creation failed...\n"); 
	// 	exit(0); 
	// } 
	// else
	// 	printf("Socket successfully created..\n"); 
	// bzero(&servaddr, sizeof(servaddr)); 

	// // assign IP, PORT 
	// servaddr.sin_family = AF_INET; 
	// servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	// servaddr.sin_port = htons(8081); 
  
	// Binding newly created socket to given IP and verification 
	// if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) { 
	// 	printf("socket bind failed...\n"); 
	// 	exit(0); 
	// } 
	// else
	// 	printf("Socket successfully binded..\n");
	// if (listen(sockfd, 10) != 0) {
	// 	printf("cannot listen\n"); 
	// 	exit(0); 
	// }
	// len = sizeof(cli);
	// connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
	// if (connfd < 0) { 
    //     printf("server acccept failed...\n"); 
    //     exit(0); 
    // } 
    // else
    //     printf("server acccept the client...\n");

    // close(sockfd);
}