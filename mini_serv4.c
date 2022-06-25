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
    int             fd;
    int             id;
    char            *str;
    struct s_client *next;
}              t_client;

int                 g_id = 0;
int                 g_socket = -1;
t_client            *g_clients = NULL;

t_client            *lastClient(void) {
    if(g_clients == NULL)
        return NULL;
    t_client    *client = g_clients;
    while(client->next)
        client = client->next;
    return client;
}

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

void            del_client(t_client *deleted_client) {
    if(deleted_client)
        free (deleted_client->str);
    close (deleted_client->fd);
    free(deleted_client);
}

void            ft_exit(void) {
    t_client    *next;
    write(2, "Fatal error\n", 12);
    if(g_socket != -1)
        close (g_socket);
    while(g_clients) {
        next = g_clients->next;
        del_client(g_clients);
        g_clients = next;
    }
    exit(1);
}

void            send_one_client(t_client *client, const char *msg) {
    int ret;
    int sent = 0;
    int len = strlen(msg);
    while(sent < len) {
        ret = send(client->fd, msg + sent, len - sent, MSG_DONTWAIT);
        if(ret < 0)
            return;
        sent += ret;
    }
}

void            send_to_clients(const char *msg, int id) {
    t_client    *client;

    client = g_clients;
    while(client) {
        if(client->id != id)
            send_one_client(client, msg);
        client = client->next;
    }
}

void            new_client(fd_set *origin) {
    t_client    *new;

    new = malloc(sizeof(t_client));
    if(!new)
        ft_exit();
    bzero(new, sizeof(t_client));
    t_client    *last;
    last = lastClient();
    if(last == NULL)
        g_clients = new;
    else
        last->next = new;

    struct sockaddr_in cli;
    socklen_t len = sizeof(cli);
    new->fd = accept(g_socket, (struct sockaddr *)&cli, &len);
    if(new->fd < 0)
        ft_exit();
    new->id = g_id;
    g_id++;
    FD_SET(new->fd, origin);
    {
        char    msg[50];
        sprintf(msg, "server: client %d just arrived\n", new->id);
        send_to_clients(msg, new->id);
    }
}

void            init_serv(int port) {
    struct sockaddr_in servaddr;

    g_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(g_socket == -1)
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

void            remove_client(t_client *client_removed, fd_set  *origin) {
    t_client    *client = g_clients;
    
    FD_CLR(client_removed->fd, origin);
    if(client_removed == g_clients){
        //first
        g_clients = client_removed->next;
    } else {
        while(client->next != client_removed)
            client = client->next;
        client->next = client_removed->next;
    }
    del_client(client_removed);
}

void            ft_recv(t_client *client, fd_set *origin) {
    char    buff[125001];
    int     ret;
    char    *msg;

    ret = recv(client->fd,buff, 125000, MSG_DONTWAIT);
    if(ret == 0) {
        //client quitte
        sprintf(buff, "server: client %d just left\n", client->id);
        send_to_clients(buff, client->id);
        remove_client(client, origin);
        return;
    }
    if(ret < 0)
       ft_exit();
    buff[ret] = '\0';
    client->str = str_join(client->str, buff);
    while((ret = extract_message(&client->str, &msg)) == 1) {
        sprintf(buff,"client %d: %s", client->id, msg);
        send_to_clients(buff, client->id);
        free(msg);
    }
    if(ret == -1)
        ft_exit();
    
}

void            main_loop() {
    fd_set  origin, cpy;

    FD_ZERO(&origin);
    FD_ZERO(&cpy);
    FD_SET(g_socket, &origin);
    while(42) {
        cpy = origin;
        if(select(FD_SETSIZE, &cpy, NULL, NULL, NULL) == -1){
            printf("Error ici\n");
            ft_exit();
        }
        if(FD_ISSET(g_socket, &cpy)){
            new_client(&origin);
        } else {
            t_client    *client = g_clients;
            t_client    *next;
            while(client) {
                next = client->next;
                if(FD_ISSET(client->fd, &cpy))
                    ft_recv(client, &origin);
                client = next;
            }
        }
    }
}

int             main(int argc, char **argv){
    if(argc != 2) {
        write(2,"Wrong number of arguments\n", 26);
        return(1);
    }
    init_serv(atoi(argv[1]));
    main_loop();
}