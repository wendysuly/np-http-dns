#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "networking.hh"

#define LISTENQLEN 5

int create_and_listen(unsigned short port)
{
	int listenfd;

	// create socket
	if ((listenfd = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
	{
		perror("socket");
		return -1;
	}

	// bind server address to socket
	struct sockaddr_in6	servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin6_family = AF_INET6;
	servaddr.sin6_addr = in6addr_any; // any interface
	servaddr.sin6_port = htons(port);
	if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
	{
		perror("bind");
		return -1;
	}

	// set socket to passive mode
	if (listen(listenfd, LISTENQLEN) < 0)
	{
		perror("listen");
		return -1;
	}

	return listenfd;
}

void print_address(const char *prefix, const struct addrinfo *res)
{
	/* slightly modified from lecture example */

	char outbuf[80];
	struct sockaddr_in *sin = (struct sockaddr_in *)res->ai_addr;
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)res->ai_addr;
	void *address;

	if (res->ai_family == AF_INET)
		address = &(sin->sin_addr);
	else if (res->ai_family == AF_INET6)
		address = &(sin6->sin6_addr);
	else
	{
		std::cerr << "Unknown address" << std::endl;
		return;
	}

	const char *ret = inet_ntop(res->ai_family, address, outbuf, sizeof(outbuf));
	std::cout << prefix << " " << ret << std::endl;
}

int send_message(int sockfd, std::string message)
{
	std::cout << std::endl << "Sending message:" << std::endl << message << std::endl;
	const char* msg = message.c_str();
	int n;
	if ((n = write(sockfd, msg, strlen(msg)+1)) < 0)
		std::cerr << "Error in writing to socket" << std::endl;
	return n;
}

int send_message(int sockfd, std::string header, const char* payload, size_t pllength)
{
	std::cout << std::endl << "Sending header:" << std::endl << header << std::endl;
	const char* msg = header.c_str();
	int hsent, psent;
	if ((hsent = write(sockfd, msg, strlen(msg))) < 0)
		std::cerr << "Error in writing to socket" << std::endl;
	if ((psent = write(sockfd, payload, pllength+1)) < 0)
		std::cerr << "Error in writing to socket" << std::endl;

	std::cout << psent << " bytes of payload sent" << std::endl;
	return hsent + psent;
}

int tcp_connect(const char *hostname, const char *service)
{
	/* slightly modified from lecture example */

	int	sockfd, addrret;
	struct addrinfo	hints, *res, *ressave;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((addrret = getaddrinfo(hostname, service, &hints, &res)) != 0)
	{
		std::cerr << "Failed to get address info for " << hostname << ", "
				<< service << ": " << gai_strerror(addrret) << std::endl;
		return -1;
	}
	ressave = res;

	do
	{
		sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sockfd < 0)
		{
			perror("Failed to create socket");
			continue; // ignore this one
		}

		print_address("Trying to connect", res);
		if (connect(sockfd, res->ai_addr, res->ai_addrlen) == 0)
			break; // success
		perror("Failed to connect");
		close(sockfd);	// ignore this one
	}
	while ((res = res->ai_next) != NULL);

	if (res == NULL)
	{
		std::cerr << "Failed to connect " << hostname << ", " << service << std::endl,
		sockfd = -1;
	}
	else
		print_address("Using address", res);

	freeaddrinfo(ressave);

	return sockfd;
}
