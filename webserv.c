#include <netdb.h> //struct addrinfo 구조체를 가진 헤더
#include <stdio.h>
#include <errno.h>
#define LISTENQ 10
void op_transaction(int fd);
int read_requesthdrs(rio_t *rp, int log, char *method);
int parse_uri(char *uri, char *filename, char *cgiargs);
void static_serve(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void dynamic_serve(int fd, char *filename, char *cgiargs);
void client_error(int fd, char *cause, char *errnum,
				  char *shortmsg, char *longmsg);

//addrinfo 구조체는 네트워크 주소정보(인터넷 주소)와 호스트이름을 표현하는데 사용되며, 이 정보는 bind( ), connect( )호출 시 입력 파라미터에 사용될 수 있다.
//또한 getaddrinfo( ) 함수 호출 시, hint 정보를 알리는 입력 파라미터로 사용할 수 있으며, getaddrinfo( ) 함수의 결과값을 전달하는 출력 파라미터로도 사용된다.

// struct addrinfo{
// 	int     ai_flags;               /* 입력 플래그 (AI_* 상수) */
//     int     ai_family;              /* 주소 패밀리 : AF_INET, AF_INET6 */
//     int     ai_socktype;            /* 종류 : SOCK_STREAM, SOCK_DGRAM */
//     int     ai_protocol;            /* 소켓 프로토콜 */
//     size_t  ai_addrlen;             /* ai_addr 이 가르키는 구조체 크기 */
//     char *  ai_canonname;           /* 공식 호스트 명 */
//     struct  sockaddr *ai_addr;      /* 소켓 주소 구조체를 가르키는 포인터 */
//     struct  addrinfo *ai_next;      /* 연결 리스트에서 다음 구조체 */
// };

int open_clientfd(char *hostname, char *port)
{
	int clientfd, addr_info_error;
	struct addrinfo hints, *net_info_list, *net_info;
	// 함수에게 말그대로 힌트를 준다. 희망하는 유형을 알려주는 힌트를 제공한다.
	// addrinfo 구조체에 hint로 줄 정보를 채운 뒤, 그것의 주소값을 넘기면 된다.
	// 이 힌트는 반환받을 result를 제한하는 기준을 지정하는 것이다.
	// 예를들면, IPv4주소만 받고 싶거나, IPv6주소만 받고 싶을 수도 있고, 둘다 받고 싶을수도 있다.
	// 이럴땐hints의 ai_family의 값을 조작하면 된다. 별도의 hint를 제공하지 않을 경우, NULL을 넣는다.

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICSERV; //numeric
	hints.ai_flags |= AI_ADDRCONFIG;
	if ((addr_info_error = getaddrinfo(hostname, port, &hints, &net_info_list)) != 0)
	{ //getaddrinfo() 정상일시 0, 아닐시 숫자 반환(에러 처리 필요)
		fprintf(stderr, "getaddrinfo failed (%s:%s): %s\n", hostname, port, gai_strerror(addr_info_error));
		//gai_strerror : getaddrinfo() and getnameinfo() function에서 나오는 에러값을 문자열로 바꿔주는 함수
		return -2;
	}

	for (net_info = net_info_list; net_info; net_info = net_info->ai_next)
	{
		if ((clientfd = socket(net_info->ai_family, net_info->ai_socktype, net_info->ai_protocol)) < 0)
			continue;														  // 만들기 실패하면 다음꺼로 넘어감
		if (connect(clientfd, net_info->ai_addr, net_info->ai_addrlen) != -1) // 성공0 실패 -1
			break;															  //성공하면 그 시점에서 이 반복문을 넘어간다. 소켓은 통신을 위해서 닫지 않는다.
		if (close(clientfd) < 0)
		{
			fprintf(stderr, "open_clientfd: close failed: %s\n", strerror(errno));
			return -1;
		}
	}

	freeaddrinfo(net_info_list); //addrinfo free()
	if (!net_info)
		return -1;
	else
		return clientfd;
	// getaddrinfo 를 위해 addrinfo 세팅을 하고, 함수를 호출해 addrinfo 구조체 리스트를 반환한다.
	// 이 리스트를 탐색하며, client socket을 만들고 connect를 시도한다. 만약 connect가 실패하면, socket을 닫는다.
	// connect가 성공하면 메모리를 반환하고, clientfd를 리턴한다.
}

int open_listenfd(char *port)
{
	struct addrinfo hints, *net_info_list, *net_info;
	int listenfd, addr_info_error, optval = 1;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; //여기가 clientfd하고 순서가 조금 다른데, 차이가 무엇인가?
	hints.ai_flags |= AI_NUMERICSERV;

	if ((addr_info_error = getaddrinfo(NULL, port, &hints, &net_info_list)) != 0) //자기가 호스트이므로 hostname에 null이 들어감
	{
		fprintf(stderr, "getaddrinfo failed (port %s): %s\n", port, gai_strerror(addr_info_error));
		return -2;
	}

	for (net_info = net_info_list; net_info; net_info = net_info->ai_next)
	{
		if ((listenfd = socket(net_info->ai_family, net_info->ai_socktype, net_info->ai_protocol)) < 0)
			continue; // 만들기 실패하면 다음꺼로 넘어감
		setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));
		//SO_REUSEADDR옵션 : 이미 사용중인 주소나 포트에 대해서도 바인드를 허용하게 해준다.

		if (bind(listenfd, net_info->ai_addr, net_info->ai_addrlen) == 0)
			break;
		if (close(listenfd) < 0)
		{ /* Bind failed, try the next */
			fprintf(stderr, "open_listenfd close failed: %s\n", strerror(errno));
			return -1;
		}
	}
	freeaddrinfo(net_info_list);
	if (!net_info)
		return -1;
	if (listen(listenfd, LISTENQ) < 0) //서버가 클라이언트 요청을 받을수있게 한다.
	{
		close(listenfd);
		return -1;
	}
	return listenfd;
}

#define MAXLINE 8192
typedef struct sockaddr SA;

int main(int argc, char **argv)
{
	int listenfd, connfd; //connfd : connect된 클라이언트의 fd?
	char hostname[MAXLINE], port[MAXLINE];
	socklen_t clientlen;
	struct sockaddr_storage clientaddr; // 소켓 주소 정보를 저장한다. IPv4 or IPv6를 고려하지 않아도 되는 storage 구조체

	if (argc != 2) //프로그램명 + 포트가 없으면 오류
	{
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}
	listenfd = open_listenfd(argv[1]); //argv[1]이 포트이므로 해당포트번호를 열어준다.
	while (1)						   //listen 이후 요청 대기
	{
		clientlen = sizeof(clientaddr);
		connfd = accept(listenfd, (SA *)&clientaddr, &clientlen); //클라이언트 요청 수락
		getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
		op_transaction(connfd);
		close(connfd);
	}
}

#define RIO_BUFSIZE 8192
typedef struct
{
	int rio_fd;				   /* Descriptor for this internal buf */
	int rio_cnt;			   /* Unread bytes in internal buf */
	char *rio_bufptr;		   /* Next unread byte in internal buf */
	char rio_buf[RIO_BUFSIZE]; /* Internal buffer */
} rio_t;
#include <sys/stat.h>

void op_transaction(int fd)
{
	int is_static;
	struct stat sbuf;
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	char filename[MAXLINE], cgiargs[MAXLINE];
	rio_t rio;
	int log;
	size_t n;

	rio_readinitb(&rio, fd);
	rio_readlineb(&rio, buf, MAXLINE);
	printf("Request headers:\n");
	printf("%s", buf);

	sscanf(buf, "%s %s %s", method, uri, version);
}
