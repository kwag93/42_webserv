#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 1004
#define BUF_SIZE 1024

int main(void)
{
	int socket_fd, accepted_fd;
	struct sockaddr_in host_addr, client_addr;
	socklen_t size;
	int recv_length;	   //수신 메세지 길이
	char buffer[BUF_SIZE]; //수신 메세지 저장소

	socket_fd = socket(PF_INET, SOCK_STREAM, 0); //PF_INET=AF_INET 동작은 동일. 명시적 표시
	// https://www.bangseongbeom.com/af-inet-vs-pf-inet.html

	host_addr.sin_family = AF_INET;
	host_addr.sin_port = htons(PORT); //host to network short
	host_addr.sin_addr.s_addr = 0;
	memset(&(host_addr.sin_zero), 0, 8);

	bind(socket_fd, (struct sockaddr *)&host_addr, sizeof(struct sockaddr));
	listen(socket_fd, 5);

	while (1)
	{
		size = sizeof(struct sockaddr_in);
		accepted_fd = accept(socket_fd, (struct sockaddr *)&client_addr, &size); //socket_fd로 들어온 요청이 client_addr에 저장이 된다.
		send(accepted_fd, "Connected", 10, 0);									 //client에 답장?
		printf("Client Info : IP %s, Port %d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
		recv_length = recv(accepted_fd, &buffer, BUF_SIZE, 0); //accept는 들어온 요청에 대한 fd만 처리해주는듯? 여기서 메세지를 실제로 받는 듯하다.
		while (recv_length > 0)
		{
			printf("From Client : %s\n", buffer);
			recv_length = recv(accepted_fd, &buffer, BUF_SIZE, 0);
		}
		close(accepted_fd);
	}
	return 0;
}
