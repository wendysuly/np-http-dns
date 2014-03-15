#include <algorithm>
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <istream>
#include <syslog.h>
#include <sys/socket.h>
#include <unistd.h>

#include "daemon.hh"
#include "helpers.hh"
#include "http.hh"
#include "networking.hh"

#define RECVBUFSIZE 1024

int main(int argc, char *argv[])
{
	unsigned short port;
	bool debug = false; // becomes a daemon by default
	std::string username;
	if (get_server_opts(argc, argv, port, debug, username) < 0)
		return -1;

	if (!debug)
	{
		std::cout << "starting daemon..." << std::endl;
		if (daemon_init("httpserver", LOG_WARNING) < 0)
			return -1;
		syslog(LOG_NOTICE, "started");
		closelog();
	}

	int listenfd;
	if ((listenfd = create_and_listen(port)) < 0)
		return -1;

	fd_set readset; // set to watch to see if read won't block
	int connfd = -1;
	struct timeval timeout;
	int numfds;
	int maxfd = listenfd + 1;
	socklen_t len;
	struct sockaddr_in6	cliaddr;
	char buff[80];
	char buffer[RECVBUFSIZE];

	while (1)
	{
		FD_ZERO(&readset); // clear the set
		FD_SET(listenfd, &readset); // add listen fd to the set
		if (connfd >= 0)
			FD_SET(connfd, &readset); // add client connection fd to the set

		// on Linux, select modifies timeout -> reinitialize it
		timeout.tv_sec = 3;
		timeout.tv_usec = 0;

		if ((numfds = select(maxfd, &readset, NULL, NULL, &timeout)) < 0)
		{
			perror("select");
			return -1;
		}

		len = sizeof(cliaddr);
		if (FD_ISSET(listenfd, &readset))
		{
			if ((connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &len)) < 0)
			{
				perror("accept");
				return -1;
			}
			std::cout << "connection from " << inet_ntop(AF_INET6, &cliaddr.sin6_addr, buff, sizeof(buff))
					  << ", port " << ntohs(cliaddr.sin6_port) << std::endl;

			if (connfd >= maxfd)
				maxfd = connfd + 1;
		}

		if (connfd >= 0 && FD_ISSET(connfd, &readset)) // handle client's request
		{
			// parse request header
			bool emptylinefound = false;
			int recvd;
			std::string readtotal;
			size_t found; // index of start of delimiter
			std::string delimiter("\r\n\r\n");
			std::string header;
			memset(buffer, 0, RECVBUFSIZE);
			while (!emptylinefound && (recvd = read(connfd, buffer, RECVBUFSIZE)) > 0)
			{
				std::string chunk(buffer, recvd);
				readtotal += chunk;
				found = readtotal.find(delimiter);
				if (found != std::string::npos)
				{
					emptylinefound = true;
					header = readtotal.substr(0, found + delimiter.length()); // include empty line to header
					std::cout << std::endl << "request header:" << std::endl << header << std::endl;
				}
			}
			if (recvd < 0)
			{
				perror("read");
				return -1;
			}
			if (!emptylinefound)
			{
				std::cerr << "empty line not found" << std::endl;
				return -1;
			}
			http_message msg(header);
			msg.parse_req_header();
			std::string resp_header = msg.create_resp_header(username);
			if (msg.method == http_method::GET)
			{
				if (msg.status_code == http_status_code::_200_OK_) // send header + payload
				{
					char* payload = new char[msg.content_length]; // allocate memory for payload
					std::ifstream fs(msg.filename);
					fs.read(payload, msg.content_length);
					if ((size_t)fs.gcount() != msg.content_length)
					{
						std::cerr << "failed to read file into memory" << std::endl;
						return -1;
					}
					std::cout << "successfully read " << fs.gcount() << " characters into memory" << std::endl;
					fs.close();

					std::cout << "Content length: " << msg.content_length << std::endl;
					if (send_message(connfd, resp_header, payload, msg.content_length) < 0)
						return -1;
					delete[] payload; // free memory
				}
				else if (send_message(connfd, resp_header) < 0) // send only header
					return -1;
			}
			else if (msg.method == http_method::PUT)
			{
				if (msg.status_code == http_status_code::_200_OK_ || msg.status_code == http_status_code::_201_CREATED_) // read payload
				{
					std::string payload_so_far = readtotal.substr(found + delimiter.length());
					size_t payload_read = payload_so_far.length();
					std::ofstream file;
					file.open(msg.filename.c_str());
					file << payload_so_far;

					// read rest of payload from socket and write to a file
					while (payload_read < msg.content_length && (recvd = read(connfd, buffer, RECVBUFSIZE)) > 0)
					{
						std::string chunk(buffer, recvd);
						payload_read += recvd;
						file << chunk;
					}
					file.close();
					if (recvd == 0)
					{
						std::cerr << "client closed connection" << std::endl;
						return -1;
					}
					if (recvd < 0)
					{
						perror("read");
						return -1;
					}
				}
				if (send_message(connfd, resp_header) < 0) // send response
					return -1;
			}

			// close client socket and start waiting for new client
			close(connfd);
			connfd = -1;
			std::cout << "closed connection" << std::endl;
		}

		if (numfds == 0)
			std::cout << "timer expired" << std::endl;
	}
}
