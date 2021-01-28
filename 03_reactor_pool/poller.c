#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <error.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <limits.h>
#include <poll.h>
#include <features.h>
#include <stdint.h>

#define INFTIM -1
#define OPEN_MAX (1024)
#define BUF_MAX (1024)

struct fd_connection_s {
	int fd;
	char conn_buf[BUF_MAX];
} ;

typedef struct fd_connection_s fd_connection_t;

int main(int argc, char **argv)
{
	int					i, maxi, listenfd, connfd, sockfd;
	int					nready;
	ssize_t				n;
	char				buf[BUF_MAX];
	char str[100];
	socklen_t			clilen;
	struct pollfd		client_pfs[OPEN_MAX];
	fd_connection_t connection[OPEN_MAX];

	size_t w_len = 0, rw_len = 0;

	struct sockaddr_in	cliaddr, servaddr;

	listenfd = socket(AF_INET, SOCK_STREAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(8888);

	bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
	listen(listenfd, 5);

	client_pfs[0].fd = listenfd;
	client_pfs[0].events = POLLIN; 
	for (i = 1; i < OPEN_MAX; i++) {
		client_pfs[i].fd = -1;	 
		connection[i].fd = -1;
		
		memset(connection[i].conn_buf, 0x0, BUF_MAX * sizeof(char));
	}

	maxi = 0;

	//printf("EVENT VALUE, POLLIN(%d), POLLOUT(%d)\n", POLLIN, POLLOUT);

	while (1) {

		nready = poll(client_pfs, maxi+1, INFTIM);

		//printf("BEGIN == 1. new events[%d], maxi[%d] \n", nready, maxi);

		if (client_pfs[0].revents & POLLIN) {	/* 如果套接字有读事件则调用accept */
			clilen = sizeof(cliaddr);
			connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);

			for (i = 1; i < OPEN_MAX; i++)	
				if (client_pfs[i].fd < 0) {
					client_pfs[i].fd = connfd;			//accept的返回的描述符
					client_pfs[i].events = POLLIN;	//监听读事件

					connection[i].fd = connfd;
					memset(connection[i].conn_buf, 0x0, BUF_MAX * sizeof(char));
					break;
				}

				if (i == OPEN_MAX) {
					perror("too many client_pfss");
					exit(0);
				}


				if (i > maxi)//数组当前下标以及poll监听数量
					maxi = i;				

				printf("get a new connection[%d] ... \n", connfd);

				if (--nready <= 0) {
					continue;	
				}
		}

		//printf("2. new events[%d], maxi[%d] \n", nready, maxi);

		for (i = 1; i <= maxi; i++) {
			
			printf("	1. fd[%d]event: %d\n", client_pfs[i].fd, client_pfs[i].revents);
			
			if (client_pfs[i].revents & POLLIN) {
				if ((n = read(client_pfs[i].fd, buf, BUF_MAX)) < 0) {
					if (errno == ECONNRESET) {
						/*客户端重置连接 */
						close(client_pfs[i].fd);
						client_pfs[i].fd = -1;
						connection[i].fd = -1;

					} else {
						perror("read error");
					}

				} else if (n == 0) {
					close(client_pfs[i].fd);
					client_pfs[i].fd = -1;
					connection[i].fd = -1;

				} else {

					// 将buf保存起来
					memcpy(connection[i].conn_buf, buf, n);
					connection[i].conn_buf[n] = 0;

					// 并注册发送事件
					client_pfs[i].events = POLLOUT;	//监听写事件
					connection[i].fd = connfd;

					printf("get buf(%s) on (%d) ...\n", connection[i].conn_buf, connection[i].fd);
				}
				
				if (--nready <= 0) {
					break;	
				}

			} 
			
			//printf("3. new events[%d], maxi[%d] \n", nready, maxi);

			if (client_pfs[i].revents & POLLOUT) {
				//printf("	2. fd[%d]event: %d\n", client_pfs[i].fd, client_pfs[i].revents);

				printf("write buf(%s) on (%d) ...\n", connection[i].conn_buf, connection[i].fd);

				w_len = strlen(connection[i].conn_buf);
				if (w_len > 0) {
					rw_len = write(client_pfs[i].fd, connection[i].conn_buf, w_len);

					if (rw_len >= w_len) {
						client_pfs[i].events = POLLIN;	//监听读事件
						connection[i].fd = connfd;

					} else {
						memmove(connection[i].conn_buf, connection[i].conn_buf + rw_len, w_len - rw_len);
						// 事件状态不更新
					}
				}

				if (--nready <= 0) {
					break;	
				}

			}

			//printf("END == 4. new events[%d], maxi[%d] \n", nready, maxi);

		}

	}
}