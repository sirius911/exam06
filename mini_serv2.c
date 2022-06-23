#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct s_client {
        int                 id;
        int                 fd;
        char                *str;
        struct s_client    *next;
}       t_client;

int         g_socket = -1;
int         g_id = 0;

t_client    *g_clients = NULL;

void    delete_client(t_client  *to_delete) {
    if(to_delete->str)
        free(to_delete->str);
    close (to_delete->fd);
    free(to_delete);
}

void    ft_exit(void) {
    t_client    *next;

    write(2, "Fatal error\n", 12);
    if(g_socket != -1)
        close(g_socket);
    while(g_clients) {
        next = g_clients->next;
        delete_client(g_clients);
        g_clients = next;
    }
    exit(1);
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

t_client    *lstlast(void) {
    if(g_clients == NULL)
        return NULL;
    t_client    *curr = g_clients;
    while(curr->next)
        curr = curr->next;
    return curr;
}

// t_client    **lstnext(void) {
//     t_client    *client;
//     if(g_clients ==NULL)
//         return (&g_clients);
//     client = g_clients;
//     while(client->next != NULL)
//         client = client->next;
//     return (&client->next);
// }

void    send_one_client(t_client *client, const char *msg) {
    int sent = 0;
    int ret;
    int len = strlen(msg);
    while(sent < len) {
        ret = send(client->fd, msg + sent, len - sent, MSG_DONTWAIT);
        if(ret <= 0)
            return;
        sent += ret;
    }
}

void    send_to_clients(const char *msg, int id) {
    t_client    *client;
    client = g_clients;
    while(client) {
        if(client->id != id)
            send_one_client(client, msg);
        client = client->next;
    }
}

void    new_client(fd_set *set) {
    struct sockaddr_in cli;
    socklen_t   len;

    t_client    *new = malloc(sizeof(t_client));
    bzero(new, sizeof(t_client));

    len = sizeof(cli);
	new->fd = accept(g_socket, (struct sockaddr *)&cli, &len);
    if(new->fd ==-1)
        ft_exit();
    new->id = g_id;
    g_id++;
    FD_SET(new->fd, set);
    t_client    *last = lstlast();
    if(last == NULL)
        g_clients = new;
    else {
        last->next = new;
    }
    {
        char    msg[50];
        sprintf(msg, "server: client %d just arrived\n", new->id);
        send_to_clients(msg, new->id);
    }
}

void    remove_client(t_client *to_delete, fd_set *origin) {
    t_client    *curr = g_clients;
    FD_CLR(to_delete->fd, origin);
    if(curr == to_delete) {
        g_clients = g_clients->next;
        delete_client(to_delete);
        return;
    }
    while(curr->next != to_delete)
        curr = curr->next;
    curr->next = to_delete->next;
    delete_client(to_delete);
}

void    ft_recv(t_client *client, fd_set *origin) {
    int     ret;
    char    buff[125001];
    char    *msg;

    ret = recv(client->fd, &buff, 125000, MSG_DONTWAIT);
    if(ret == 0) {
        //client quitte
        sprintf(buff, "server: client %d just left\n", client->id);
        send_to_clients(buff, client->id);
        remove_client(client, origin);
        return;
    } else if (ret < 0)
        return;
    buff[ret] = '\0';
    client->str = str_join(client->str, buff);
    while((ret = extract_message(&client->str, &msg)) == 1){
        sprintf(buff,"client %d: %s", client->id, msg);
        send_to_clients(buff, client->id);
        free(msg);
    }
    if(ret == -1)
        ft_exit();
}

void    init_serve(int port) {
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

void    main_loop(void) {
    fd_set  origin, cpy;

    FD_ZERO(&origin);
    FD_ZERO(&cpy);
    FD_SET(g_socket,&origin);
    while(42) {
        cpy = origin;
        if((select(FD_SETSIZE,&cpy,NULL,NULL,NULL)) == -1)
            ft_exit();
        if(FD_ISSET(g_socket, &cpy)) {
            // new client
            new_client(&origin);
        } else {
            t_client *client = g_clients;
            while(client) {
                if(FD_ISSET(client->fd, &cpy))
                    ft_recv(client, &origin);
                client = client->next;
            }
        }
    }
}

int     main(int argc, char **argv) {
    if(argc != 2) {
        write(2, "Wrong number of arguments\n", 26);
        return (1);
    }
    init_serve(atoi(argv[1]));
    main_loop();
}