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
    int             id;
    int             fd;
    char            *str;
    struct s_client *next;
}               t_client;

int         g_socket = -1;
int         g_id = 0;
t_client    *g_clients = NULL;



t_client    *lstlast(void) {

    if(g_clients == NULL)
        return (NULL);
    t_client    *client = g_clients;
    while(client->next)
        client = client->next;
    return(client);    
}

void        del_client(t_client *to_delete) {
    if(to_delete->str)
        free(to_delete->str);
    close(to_delete->fd);
    free(to_delete);
}

void        ft_exit(void) {
    t_client    *next;
    write(2,  "Fatal error\n", 12);
    if(g_socket != -1)
        close(g_socket);
    while(g_clients) {
        next = g_clients->next;
        del_client(g_clients);
        g_clients = next;
    }
    exit(1);
}

void        init_serve(int port) {
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

void        new_client(fd_set   *set){
    struct sockaddr_in cli;
    socklen_t   len = sizeof(cli);

    t_client    *new = malloc(sizeof(t_client));
    bzero(new, sizeof(t_client));
    new->id = g_id;
    g_id++;
    t_client    *last = lstlast();
    if(last == NULL)
        g_clients = new;
    else
        last->next = new;
    new->fd= accept(g_socket, (struct sockaddr *)&cli, &len);
    if(new->fd == -1)
        ft_exit();
    {
        char    msg[50];
        sprintf(msg, "server: client %d just arrived\n", new->id);
        send_to_clients(msg, new->id);
    }
    FD_SET(new->fd, set);
}

void        main_loop(void) {
    fd_set origin, cpy;

    FD_ZERO(&origin);
    FD_ZERO(&cpy);
    FD_SET(g_socket,&origin);
    while(42) {
        cpy = origin;
        if(select(FD_SETSIZE, &cpy, NULL, NULL, NULL) == -1)
            ft_exit();
        if(FD_ISSET(g_socket, &cpy)) {
            new_client(&origin);
        } else {
            t_client *client = g_clients;
            while(client) {
                if(FD_ISSET(client->fd, &cpy))
                    //ft_recv(client, &origin);
                client = client->next;
            }
        }
    }
}

int         main(int argc, char **argv) {
    if(argc != 2){
        write(2,"Wrong number of arguments\n", 27);
        return(1);
    }
    init_serve(atoi(argv[1]));
    main_loop();
}