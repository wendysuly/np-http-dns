/* HTTP protocol */

#ifndef NETPROG_HTTP_HH
#define NETPROG_HTTP_HH

#include <string>

/* supported methods */
enum http_method
{
	NOT_SET, // default value
	GET,
	PUT,
	UNSUPPORTED
};

/* supported status codes */
enum http_status_code
{
	_NOT_SET_, // default value
	_200_OK_,
	_201_CREATED_,
	_400_BAD_REQUEST_,
	_403_FORBIDDEN_,
	_404_NOT_FOUND_,
	_501_NOT_IMPLEMENTED_,
	_UNSUPPORTED_
};

class http_message
{
public:


	/* delimiter between header and payload */
	static const std::string delimiter;

	/* constructors */
	http_message();
	http_message(std::string header);
	http_message(http_method method, std::string filename, std::string host, std::string username);
	http_message(http_method method, std::string filename, size_t filesize, const char* file, std::string host, std::string username);

	/* parse request header and fill object member values */
	void parse_req_header();

	/* parse response header and return success*/
	bool parse_resp_header(std::string header);

	/* create and return header */
	std::string create_header() const;

	std::string create_resp_header(std::string username) const;

	/* dump object member values */
	void dump_values() const;

	std::string header; // received header
	http_method method;
	const char* payload;
	http_status_code status_code;
	std::string filename;
	size_t content_length;

private:

	const std::string method_to_str(http_method m) const;
	static const char* status_code_strings[];

	std::string host;
	std::string content_type;
	std::string username;
	std::string server;
};

class http_request
{
public:

	/* Constructor */
	http_request();

	/* Return request object by reading and parsing it from socket
	 * Caller must open and close the socket descriptor
	 *
	 * param sockfd: socket descriptor with receive timeout set
	 * returns: request object */
	static http_request from_socket(int sockfd);

	/* Return request object by constructing it from parameters
	 *
	 * param method:
	 * param filename:
	 * param hostname:
	 * param username:
	 * returns: request object */
	static http_request from_params(std::string method, std::string filename, std::string hostname, std::string username);

	void print() const;

	std::string header; // whole header

	http_method method;
	std::string filename;
	std::string protocol;
	std::string hostname;
	std::string username;
	std::string content_type;
	size_t content_length;

private:

	void create_header();
	void parse_header();
};

class http_response
{
public:

	/* Constructor */
	http_response();

	static http_response from_request(http_request request, std::string username);

	void print() const;

	std::string header; // whole header

	std::string protocol;
	http_status_code status_code;
	std::string username;
	std::string content_type;
	size_t content_length;

	http_method request_method;

private:

	void create_header();
};

#endif
