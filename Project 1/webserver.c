/*
Author: Nathan Tung
Notes: Using skeleton code from Beej's Guide to Network Programming
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#define PORT "2080" // the port users will be connecting to
#define BACKLOG 10 // how many pending connections queue will hold

#define HTML "Content-Type: text/html\r\n"
#define TXT "Content-Type: text/plain\r\n"
#define JPEG "Content-Type: image/jpeg\r\n"
#define JPG "Content-Type: image/jpg\r\n"
#define GIF "Content-Type: image/gif\r\n"

#define STATUS_200 "HTTP/1.1 200 OK\r\n"
#define STATUS_404 "HTTP/1.1 404 Not Found\r\n\r\n"
#define ERROR_404_HTML "<h1>Error 404: File Not Found!</h1> <br><br> <h3>File requested must be in same directory as server.</h3>"

typedef enum
{
	html, txt, jpeg, jpg, gif
}
extension;

char *requestDump(int);
void serveFile (int, char *);
void generateResponseMessage(int, char *, size_t);
extension getFileType(char *);
char *replace(const char *, const char *, const char *);
char *replace2(const char *, const char *, const char *);

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
int main(int argc, char *argv[])
{
	int sockfd, new_fd; // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP
	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}
	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
		p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
		sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
		close(sockfd);
			perror("server: bind");
			continue;
		}
		break;
	}
	if (p == NULL) {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}
	freeaddrinfo(servinfo); // all done with this structure
	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}
	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}
	printf("server: waiting for connections...\n");
	while(1) { // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}
		inet_ntop(their_addr.ss_family,
		get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
		printf("server: got connection from %s\n", s);
		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener

			char *filename;
			filename = requestDump(new_fd); // dump request message to console, fetch filename
			serveFile(new_fd, filename); // serve requested file to client

			close(new_fd);
			exit(0);
		}
		close(new_fd); // parent doesn't need this
	}
	return 0;
}

// displays socket request message in console
// returns c string containing name of requested file
char *requestDump (int sock)
{
	int n;
	char buffer[512];
	bzero(buffer,512);
	n = read(sock,buffer,511); // read socket request message into buffer

	if (n < 0) perror("Socket: error reading!");
	
	// make a copy of buffer to tokenize
	char fname[512];
	memcpy(fname, buffer, 512);

	// tokenize request message based on space; grab 2nd token (file name)
	char *token;
	const char space[2] = " ";
	token = strtok(fname, space);
	token = strtok(NULL, space);

	// increment the token to get rid of leading '/'
	token++;

	//however, if token is empty (no file selected), set token to '\0'
	if(strlen(token)<=0) token = "\0";

	// print buffer contents into console
	printf("HTTP REQUEST MESSAGE:\n%s\n", buffer);

	// print file name into console
	//printf("TOKEN:\n%s\n", token);

	// Writing more messages to browser
	// n = write(sock,"I got your message",18);
	// if (n < 0) perror("Socket: error writing!");

	return token;
}

// given socket and filename, serve the file to client browser
// if file is not requested or cannot be found, give error
void serveFile (int sock, char *filename)
{
	if(filename=="\0")
	{
		// send 404 error status to client browser, then display error page
		send(sock, STATUS_404, strlen(STATUS_404), 0);
		send(sock, ERROR_404_HTML, strlen(ERROR_404_HTML), 0);
		printf("Error: no file specified!\n");
		return;
	}

	// create source buffer to fopen and fread designated file
	char *source = NULL;
	FILE *fp = fopen(filename, "r");

	if (fp==NULL)
	{
		// send 404 error status to client browser, then display error page
		send(sock, STATUS_404, strlen(STATUS_404), 0);
		send(sock, ERROR_404_HTML, strlen(ERROR_404_HTML), 0);
		printf("Error: file not found!\n");
		return;
	}

	if (fseek(fp, 0L, SEEK_END) == 0)
	{
		// set fsize to file size
	        long fsize = ftell(fp);
	        if (fsize == -1)
		{
			// send 404 error status to client browser, then display error page
			send(sock, STATUS_404, strlen(STATUS_404), 0);
			send(sock, ERROR_404_HTML, strlen(ERROR_404_HTML), 0);
			printf("File size error!\n");
			return;
		}

		// allocate source buffer to filesize
		source = malloc(sizeof(char) * (fsize + 1));

		// return to front of file
		if (fseek(fp, 0L, SEEK_SET) != 0)
		{
			// send 404 error status to client browser, then display error page
			send(sock, STATUS_404, strlen(STATUS_404), 0);
			send(sock, ERROR_404_HTML, strlen(ERROR_404_HTML), 0);
			printf("File size error!\n");
			return;
		}

		// set source to file data
		size_t sourceLength = fread(source, sizeof(char), fsize, fp);

		// check file source for fread errors
		if (sourceLength == 0)
		{
			// send 404 error status to client browser, then display error page
			send(sock, STATUS_404, strlen(STATUS_404), 0);
			send(sock, ERROR_404_HTML, strlen(ERROR_404_HTML), 0);
			printf("File reading error!\n");
			return;
		}

		// NULL-terminate the source
		source[sourceLength] = '\0';
		
		// send HTTP response header to client browser
		generateResponseMessage(sock, filename, sourceLength);

		// send file to client browser
		send(sock, source, sourceLength, 0);

		printf("File \"%s\" served to client!\n\n", filename);
	}

	// close file and free dynamically allocated file source
	fclose(fp);
	free(source);
}

// generates HTTP response message only if file is successfully served (200 OK)
// message holds the HTML-formatted response message (using <br> tags)
// newMessage holds the console-formatted response message (using \r\n characters)
void generateResponseMessage(int sock, char *filename, size_t fileLength)
{
	char message[512];

	// header status
	char *status; 
	status = STATUS_200;
	
	// header connection
	char *connection = "Connection: close\r\n";

	// header date
	struct tm* dateclock;
	time_t now;
	time(&now);
	dateclock = gmtime(&now);
	char nowtime[35];
	strftime(nowtime, 35, "%a, %d %b %Y %T %Z", dateclock);
	//printf("Date time: %s\n", nowtime);
	char date[50] = "Date: ";
	strcat(date, nowtime);
	strcat(date, "\r\n");

	// header server
	char *server = "Server: NathanTung/1.0\r\n";

	// header last-modified
	struct tm* lmclock;
	struct stat attrib;
	stat(filename, &attrib);
	lmclock = gmtime(&(attrib.st_mtime));
	char lmtime[35];
	strftime(lmtime, 35, "%a, %d %b %Y %T %Z", lmclock);
	//printf("Last modified time: %s\n", lmtime);
	char lastModified[50] = "Last-Modified: ";
	strcat(lastModified, lmtime);
	strcat(lastModified, "\r\n");

	// header content-length	
	char contentLength[50] = "Content-Length: ";	
	char len[10];
	sprintf (len, "%d", (unsigned int)fileLength);
	strcat(contentLength, len);
	strcat(contentLength, "\r\n");

	// header content-type
	extension ext = getFileType(filename);
	char *contentType;
	contentType = TXT;
	if(ext==html) contentType = HTML;
	if(ext==jpeg) contentType = JPEG;
	if(ext==jpg) contentType = JPG;
	if(ext==gif) contentType = GIF;

	int offset = strlen(status);
	memcpy(message, status, offset);

	memcpy(message+offset, connection, strlen(connection));
	offset+=strlen(connection);

	memcpy(message+offset, date, strlen(date));
	offset+=strlen(date);

	memcpy(message+offset, server, strlen(server));
	offset+=strlen(server);

	memcpy(message+offset, lastModified, strlen(lastModified));
	offset+=strlen(lastModified);

	memcpy(message+offset, contentLength, strlen(contentLength));
	offset+=strlen(contentLength);

	memcpy(message+offset, contentType, strlen(contentType));
	offset+=strlen(contentType);

	memcpy(message+offset, "\r\n\0", 3);

	// send response to client browser as header lines
	send(sock, message, strlen(message), 0);

	// send copy of response to console
	printf("HTTP RESPONSE MESSAGE:\n%s\n", message);
}


// return file extension
extension getFileType(char *filename)
{
	if (strstr(filename, ".html") != NULL) return html;
	if (strstr(filename, ".txt") != NULL) return txt;
	if (strstr(filename, ".jpeg") != NULL) return jpeg;
	if (strstr(filename, ".jpg") != NULL) return jpg;
	if (strstr(filename, ".gif") != NULL) return gif;

	// in case of client error, assume plaintext
	return txt;
}

