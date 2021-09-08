#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>

# define BUFFER_SIZE 10

typedef struct s_clt
{
	int id;
	int fd;
	char *buffer;
	struct s_clt *next;
}	t_clt;

typedef struct s_serv
{
	int total;
	int sockfd;
	t_clt *clients;
}	t_serv;

void	fatal(t_serv *serv)
{
	write (2, "Fatal Error\n", strlen("Fatal Error\n"));
	if (serv->sockfd > 0)
		close(serv->sockfd);
	exit (1);
}

int get_max_fd(t_serv *serv)
{
	t_clt *tmp = serv->clients;
	int max = serv->sockfd;

	while (tmp)
	{
		if (tmp->fd > max)
			max = tmp->fd;
		tmp = tmp->next;
	}
	return (max);
}

t_clt *add_client(t_serv *serv, int new_clt_fd, fd_set *set)
{
	t_clt *tmp = serv->clients;
	t_clt *new;

	if (!(new = (t_clt *)malloc(sizeof(t_clt))))
		fatal(serv);
	new->fd = new_clt_fd;
	new->id = serv->total++;
	new->buffer = NULL;
	new->next = NULL;
	if (!tmp)
		serv->clients = new;
	else
	{
		while (tmp && tmp->next)
			tmp = tmp->next;
		tmp->next = new;
	}
	FD_SET(new->fd, set);
	return (new);
}

void	send_all(t_serv *serv, char *content, int fd, fd_set *writes)
{
	t_clt *tmp = serv->clients;

	while (tmp)
	{
		if (tmp->fd != fd && FD_ISSET(tmp->fd, writes))
			send(tmp->fd, content, strlen(content), MSG_DONTWAIT);
		tmp = tmp->next;
	}
}

t_clt *remove_client(t_serv *serv, t_clt *to_remove)
{
	t_clt *tmp = serv->clients;

	if (!tmp)
		return (NULL);
	if (tmp == to_remove)
	{
		free(tmp->buffer);
		serv->clients = tmp->next;
		free(tmp);
		tmp = serv->clients;
		return (tmp);
	}
	while (tmp->next != to_remove && tmp->next)
		tmp = tmp->next;
	tmp->next = to_remove->next;
	free(to_remove->buffer);
	free(to_remove);
	return (tmp->next);
}

char *str_join(char *s1, char *s2, int code)
{
	int len1;
	int len2;
	char *new;

	len1 = 0;
	if (s1)
		len1 = strlen(s1);
	len2 = strlen(s2);
	if (!(new = (char *)malloc(len1 + len2 + 1)))
		return (NULL);
	if (s1)
		memcpy(new, s1, len1);
	memcpy(new + len1, s2, len2);
	new[len1 + len2] = 0;
	if (code == 1 && s1)
		free(s1);
	return (new);

}

void	parsing(t_serv *serv, t_clt *curr_clt, fd_set *writes, char *str, int len)
{
	size_t i = 0;
	size_t j = 0;
	char buff[BUFFER_SIZE];
	char buff2[100];
	char *to_send = NULL;

	bzero(&buff, sizeof(buff));
	bzero(&buff2, sizeof(buff2));
	while (str[i])
	{
		buff[j] = str[i];
		j++;
		if (str[i] == '\n')
		{
			sprintf(buff2, "client %d: ", curr_clt->id);
			if (curr_clt->buffer)
			{
				if (!(to_send = str_join(buff2, curr_clt->buffer, 0)))
					fatal(serv);
				if (!(to_send = str_join(to_send, buff, 1)))
					fatal(serv);
				free(curr_clt->buffer);
				curr_clt->buffer = 0;
			}
			else
			{
				if (!(to_send = str_join(buff2, buff, 0)))
					fatal(serv);
			}
			send_all(serv, to_send, curr_clt->fd, writes);
			free(to_send);
			to_send = NULL;
			bzero(&buff, sizeof(buff));
			bzero(&buff2, sizeof(buff2));
			j = 0;
		}
		i++;
	}
	if (buff[0] != 0)
	{
		if (!(curr_clt->buffer = str_join(curr_clt->buffer, buff, 1)))
			fatal(serv);
	}
}

int main(int argc, char **argv)
{
	//int sockfd;
	if (argc != 2)
	{
		write (STDERR_FILENO, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
		exit(1);
	}
	struct sockaddr_in servaddr;

	t_serv server;
	server.sockfd = 0;
	server.total = 0;
	server.clients = NULL;

	bzero(&servaddr, sizeof(servaddr)); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(argv[1]));

	server.sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (server.sockfd == -1)
		fatal(&server);
	if ((bind(server.sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		fatal(&server);
	if (listen(server.sockfd, 10) != 0)
		fatal(&server);


	fd_set reads;
	fd_set writes;
	fd_set curr_sock;
	FD_ZERO(&curr_sock);
	FD_SET(server.sockfd, &curr_sock);
	char buffer[100];
	char recv_buffer[BUFFER_SIZE];

	while (1)
	{
		reads = writes = curr_sock;
		int activity = select(get_max_fd(&server) + 1, &reads, &writes, NULL, NULL);
		if (activity < 0)
			fatal(&server);
		else if (activity > 0)
		{
			if (FD_ISSET(server.sockfd, &reads)) //new clt
			{
				int new_clt_fd = accept(server.sockfd, NULL, NULL);
				if (new_clt_fd >= 0)
				{
					t_clt *new_clt = add_client(&server, new_clt_fd, &curr_sock);
					bzero(&buffer, sizeof(buffer));
					sprintf(buffer, "server: client %d just arrived\n", new_clt->id);
					send_all(&server, buffer, new_clt_fd, &writes);
					continue ;
				}
			}
			
			t_clt *tmp = server.clients;
			while (tmp)
			{
				if (FD_ISSET(tmp->fd, &reads))
				{
					ssize_t received = recv(tmp->fd, recv_buffer, BUFFER_SIZE - 1, MSG_DONTWAIT);
					if (received == 0) //clt disconnected*
					{
						bzero(&buffer, sizeof(buffer));
						sprintf(buffer, "server: client %d just left.\n", tmp->id);
						send_all(&server, buffer, tmp->fd, &writes);
						FD_CLR(tmp->fd, &curr_sock);
						close(tmp->fd);
						tmp = remove_client(&server, tmp);
						continue ;
					}
					else if (received > 0)
					{
						recv_buffer[received] = 0;
						parsing(&server, tmp, &writes, recv_buffer, received);
					}
				}
				tmp = tmp->next;
			}
		}

	}
	exit(0);
}