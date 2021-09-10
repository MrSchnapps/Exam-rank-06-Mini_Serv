#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

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

void fatal(t_serv *serv)
{
	write(2, "Fatal error\n", 12);
	if (serv->sockfd > 0)
		close(serv->sockfd);
	if (serv->clients)
	{
		t_clt *tmp = serv->clients;
		while (serv->clients)
		{
			close(serv->clients->fd);
			tmp = serv->clients->next;
			free(serv->clients->buffer);
			free(serv->clients);
			serv->clients = tmp;
		}
	}
	exit (1);
}

int get_fd_max(t_serv *serv)
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

t_clt *add_client(t_serv *serv, int fd, fd_set *set)
{
	t_clt *tmp = serv->clients;
	t_clt *new;

	if (!(new = (t_clt *)malloc(sizeof(t_clt))))
		fatal(serv);
	new->id = serv->total++;
	new->fd = fd;
	new->buffer = NULL;
	new->next = NULL;
	if (!serv->clients)
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

t_clt *remove_clt(t_serv *serv, t_clt *to_remove)
{
	t_clt *tmp = serv->clients;

	if (!tmp)
		return (NULL);
	if (tmp == to_remove)
	{
		serv->clients = to_remove->next;
		free(tmp->buffer);
		free(tmp);
		tmp = serv->clients;
		return (tmp);
	}
	while (tmp && tmp->next != to_remove)
		tmp = tmp->next;
	tmp->next = to_remove->next;
	free(to_remove->buffer);
	free(to_remove);
	return (tmp->next);
}

char *str_join(char *s1, char *s2, int code)
{
	size_t len1 = 0;
	size_t len2 = strlen(s2);
	char *new;

	if (s1)
		len1 = strlen(s1);
	if (!(new = (char *)malloc(len1 + len2 + 1)))
		return (NULL);
	if (s1)
		strcpy(new, s1);
	strcpy(new + len1, s2);
	new[len1 + len2] = 0;
	if (code == 1 && s1)
		free(s1);
	return (new);
}

void	parsing(t_serv *serv, t_clt *curr_clt, fd_set *writes, char *content)
{
	int i = 0;
	int j = 0;
	char buff[65535];
	char buff2[200];
	char *to_send = NULL;

	bzero(&buff, sizeof(buff));
	bzero(&buff2, sizeof(buff2));

	while (content[i])
	{
		buff[j] = content[i];
		j++;
		if (content[i] == '\n')
		{
			sprintf(buff2, "client %d : ", curr_clt->id);
			if (curr_clt->buffer)
			{
				if (!(to_send = str_join(buff2, curr_clt->buffer, 0)))
					fatal(serv);
				if (!(to_send = str_join(to_send, buff, 1)))
					fatal(serv);
				free(curr_clt->buffer);
				curr_clt->buffer = NULL;
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
	if (argc != 2)
	{
		write (2, "Wrong numbers of arguments\n", 27);
		exit (1);
	}
	
	t_serv serv;
	serv.total = 0;
	serv.sockfd = 0;
	serv.clients = NULL;

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr)); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(argv[1]));

	serv.sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (serv.sockfd == -1)
		fatal(&serv);
	if ((bind(serv.sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		fatal(&serv);
	if (listen(serv.sockfd, 10) != 0)
		fatal(&serv);

	fd_set reads;
	fd_set writes;
	fd_set curr_sock;
	FD_ZERO(&curr_sock);
	FD_SET(serv.sockfd, &curr_sock);

	char buffer[200];
	char recv_buffer[65535];

	while (19)
	{
		reads = writes = curr_sock;

		int activity = select(get_fd_max(&serv) + 1, &reads, &writes, NULL, NULL);
		if (activity < 0)
			fatal(&serv);
		else if (activity > 0)
		{
			if (FD_ISSET(serv.sockfd, &reads))
			{
				int new_clt_fd = accept(serv.sockfd, NULL, NULL);
				if (new_clt_fd > 0)
				{
					t_clt *new_clt = add_client(&serv, new_clt_fd, &curr_sock);
					bzero(&buffer, sizeof(buffer));
					sprintf(buffer, "server: client %d just arrived\n", new_clt->id);
					send_all(&serv, buffer, new_clt_fd, &writes);
					continue ;
				}
			}

			t_clt *tmp = serv.clients;
			while (tmp)
			{
				if (FD_ISSET(tmp->fd, &reads))
				{
					bzero(&recv_buffer, sizeof(recv_buffer));
					ssize_t received = recv(tmp->fd, recv_buffer, 65535 - 1, MSG_DONTWAIT);
					if (received == 0)
					{
						bzero(&buffer, sizeof(buffer));
						sprintf(buffer, "server: client %d just left\n", tmp->id);
						send_all(&serv, buffer, tmp->fd, &writes);
						tmp = remove_clt(&serv, tmp);
						continue ;
					}
					else if (received > 0)
					{
						recv_buffer[received] = 0;
						parsing(&serv, tmp, &writes, recv_buffer);
					}
				}
				tmp = tmp->next;
			}
		}
	}
	exit (0);
}