//ygeslin is the original coder of this code, the bogoss, that's how he is called

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
// reprendre header depuis main.c
#include <stdio.h> // sprintf
#include <stdlib.h> // malloc
// header ajoute
int extract_message(char **buf, char **msg) // copy from main
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

char *str_join(char *buf, char *add) // copy form main
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

typedef struct 			s_client 
{
	int 				id;
	int 				fd;
	char 				*str;
	struct s_client 	*next;
}						t_client;

int 					g_id = 0;
int 					g_socket = -1; // socket d'ecoute
t_client				*g_clients = NULL;

// ! -----------------------------------
void del_client(t_client *client)
{
	if (client)
		free(client->str);
	close(client->fd);
	free(client);
}

// ! -----------------------------------
void ft_exit(char *msg)
{
	t_client *next;
	write(2, msg, strlen(msg));
	write(2, "\n", 1); // ?
	if (g_socket != -1)
		close(g_socket); // ferme socket ecoute
	while(g_clients != NULL) // je ferme tout les clients
	{
		next = g_clients->next;
		del_client(g_clients);
		g_clients = next;
	}
	exit(1);
}

// ! -----------------------------------
t_client **lstnext(void)
{
	t_client *last = g_clients;
	if (g_clients == NULL) // si g_clients pas init => je renvoie ptr sur lui
		return (&g_clients);
	while(last->next != NULL) // si non je vais cherche le dernier et je renvoie ptr sur lui
		last = last->next;
	return (&last->next);
}

// ! -----------------------------------
void send_one_client(t_client *client, const char *const msg)
{
	int ret;
	int sent = 0;
	int len = strlen(msg);
	while(sent < len) // j'envoie jusqu'a avoir atteint len demandee
	{
		if ((ret = send(client->fd, msg + sent, len - sent, MSG_DONTWAIT)) <= 0) // en non bloquant avec MSG_DONTWAIT
			return ; // si client a quitte ou plus dispo
		sent += ret;
	}
}

// ! -----------------------------------
void send_to_clients(const char *const msg, int except_id)
{
	t_client *client = g_clients;
	while (client)
	{
		if (client->id != except_id)
			send_one_client(client, msg);
		client = client->next;
	}
}

// ! -----------------------------------
void new_client(fd_set *set)
{
	struct sockaddr_in cli;
	socklen_t len = sizeof(cli);

	t_client *client = malloc(sizeof(t_client));
	bzero(client, sizeof(t_client));

	*lstnext() = client;// ?
	if ((client->fd = accept(g_socket, (struct sockaddr *)&cli, &len)) < 0) // creation de la socket client
		ft_exit("Fatal error");
	client->id = g_id++; 
	{
		char msg[50];
		sprintf(msg, "server: client %d just arrived\n", client->id);
		send_to_clients(msg, client->id);
	}
	FD_SET(client->fd, set);
}

// ! -----------------------------------
void remove_client(t_client *to_delete, fd_set *origin)
{
	t_client *curr = g_clients;
	FD_CLR(to_delete->fd, origin); // je retire le fd du fd_set origin
	if (curr == to_delete) // si je dois delete la 1ere valeur de la liste
	{
		g_clients = g_clients->next; // j'actualise sur la valeur suivante
		del_client(to_delete); // je free la valeur actuelle
		return ;
	}
	while (curr->next != to_delete) // sinon je cherche la prochaine valeur avant celle a supprimer
		curr = curr->next;
	curr->next = to_delete->next; // j'actualise la valeur avant celle a suppr sur celle next de celle a suppr
	del_client(to_delete); // je suppr la valeur
}

// ! -----------------------------------
void ft_recv(t_client *client, fd_set *origin)
{
	//	char buff[125000 + 20 + 1]; // ?
	char buff[125001];
	char *msg;
	int ret;
	if ((ret = recv(client->fd, buff, 125000, MSG_DONTWAIT)) == 0) // je recv en mode non bloquant // si 0 le client a quitte
	{
		sprintf(buff, "server: client %d just left\n", client->id);
		send_to_clients(buff, client->id); // j'envoie un message indiquant depart client
		remove_client(client, origin); // je retire le client
		return ;
	}
	else if (ret < 0)
		return ; // si erreur => retour
	buff[ret] = '\0'; // sinon j'ai bien recu et met un 0 en fin de buffer
	client->str = str_join(client->str, buff) ;// je join le buff car je ne peux pas le faire passer direct a extract
	while ((ret = extract_message(&client->str, &msg)) == 1) // je lis jusqu'au prochain \n
	{
		sprintf(buff, "client %d: %s", client->id, msg);
		send_to_clients(buff, client->id); // j'envoie phrase par phrase
		free(msg); // je libere msg a chaque tour !
	}
	if (ret == -1)
		ft_exit("Fatal error");
}

// ! -----------------------------------
void mini_serv(void)
{
	fd_set origin, cpy; // origin = fd_set global contenant tous les clients que j'ajoute / retire
	// cpy == fd_set temporaire contenant les events / fd  pret pour read ou write
	FD_ZERO(&origin);
	FD_SET(g_socket, &origin);// j'ajoute le set de fd a la socket d'ecoute
	while(1)
	{
		cpy = origin;
		if (select(FD_SETSIZE, &cpy, NULL, NULL, NULL) == -1)// check si un event est dispo en read ou write dans cpy et le declenche
			ft_exit("Fatal error");
		if (FD_ISSET(g_socket, &cpy)) // si le client se connecte pour la premiere fois
			new_client(&origin); // je l'ajoute a origin
		else                          // sinon je parcoure tous les clients existants
		{
			t_client *curr, *next;
			curr = g_clients;
			while (curr)
			{
				next = curr->next;
				if (FD_ISSET(curr->fd, &cpy)) // si le client existe dans cpy, alors il est pret en read ou write
					ft_recv(curr, &origin); // donc je recois son contenu // ! j'envoie origin uniquement pour pouvoir delete des clients si besoin directement dedans
				curr = next;
			}
		}
	}
}

// ! -----------------------------------
void init_serv(int port)
{
	// a prendre main
	struct sockaddr_in servaddr;
	if ((g_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		ft_exit("Fatal error");
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port);
	if ((bind(g_socket, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		ft_exit("Fatal error");
	if (listen(g_socket, 10) != 0)
		ft_exit("Fatal error");}

// ! -----------------------------------
int main(int ac, char **av)
{
	if (ac != 2)
		ft_exit("Wrong number of arguments");
	init_serv(atoi(av[1]));
	mini_serv();
	return 0;
}