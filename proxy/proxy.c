/*
 * proxy.c - CS:APP Web proxy
 *
 * TEAM MEMBERS:
 *     Andrew Carnegie, ac00@cs.cmu.edu
 *     Harry Q. Bovik, bovik@cs.cmu.edu
 *
 * IMPORTANT: Give a high level description of your code here. You
 * must also provide a header comment at the beginning of each
 * function that describes what that function does.
 */

#include "csapp.h"

/*
 * Function prototypes
 */
void redirect_packet(int sockfd);
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);

/*
 * main - Main routine for the proxy program
 */
int main(int argc, char **argv)
{

    /* Check arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        exit(0);
    }

    puts("Start opening listening fd");
    int listenfd = Open_listenfd(atoi(argv[1]));
    puts("End opening listening fd");

    SA connect_addr;
    socklen_t connect_addr_len;

    for (;;) {
        puts("Start listening");
        int connectfd =
            Accept(listenfd, (SA *)&connect_addr, &connect_addr_len);
        puts("End listening");

        redirect_packet(connectfd);
    }

    Close(listenfd);

    exit(0);
}

char *extract_uri(char *header);

void redirect_packet(int sockfd)
{
    /* Prepare buffer */
    rio_t send_buf;
    Rio_readinitb(&send_buf, sockfd);

    ssize_t size;

    char user_buf[MAXBUF];
    char hostname[MAXBUF];
    char pathname[MAXBUF];
    int port;

    puts("Start reading");

    /* Get uri */
    size = Rio_readlineb(&send_buf, user_buf, sizeof(user_buf));
    char *uri = extract_uri(user_buf);
    parse_uri(uri, hostname, pathname, &port);

    printf("hostname: %s\n", hostname);
    printf("pathname: %s\n", pathname);
    printf("port: %d\n", port);

    /* Connect to server */
    int server_fd = open_clientfd(hostname, port);

    Rio_writen(server_fd, user_buf, size);
    do {
        size = Rio_readlineb(&send_buf, user_buf, sizeof(user_buf));
        printf("%s", user_buf);
        Rio_writen(server_fd, user_buf, size);
    } while (size > 2);  // strcmp(user_buf, "\r\n") != 0

    shutdown(server_fd, SHUT_WR);
    shutdown(sockfd, SHUT_RD);

    puts("End reading");
    puts("Start writing");

    /* Receive packet from server */
    rio_t recv_buf;
    Rio_readinitb(&recv_buf, server_fd);

    do {
        size = Rio_readlineb(&recv_buf, user_buf, sizeof(user_buf));
        printf("%s", user_buf);
        Rio_writen(sockfd, user_buf, size);
        /* Temp */
        break;
    } while (size > 2);

    /* Temp */
    char msg[] = "<html>Hello World</html>";
    Rio_writen(sockfd, "\r\n", 2);
    Rio_writen(sockfd, msg, strlen(msg) + 1);

    puts("End writing");

    shutdown(server_fd, SHUT_RD);
    Close(server_fd);

    shutdown(sockfd, SHUT_WR);
    Close(sockfd);
}

/*
 * extract_uri - Extract uri from http header
 *
 * Prepare the well-formed uri, stripping out
 * http method and version information.
 *
 * [IN] header:
 *      The first line of http message,
 *      containing method, uri and http
 *      version.
 * [RETURN]
 *      A duplication of the uri, the
 *      caller should remember to free
 *      the space.
 */
char *extract_uri(char *header)
{
    char needle[] = " ";

    /* uri's content is between [uri_beg, uri_end) */
    char *uri_beg = strstr(header, needle) + strlen(needle);
    char *uri_end = strstr(uri_beg, needle);

    size_t uri_len = uri_end - uri_beg;
    char *uri_buf = Malloc((uri_len + 1) * sizeof(*uri_buf));
    strncpy(uri_buf, uri_beg, uri_len);
    uri_buf[uri_len] = '\0';

    return uri_buf;
}

/*
 * parse_uri - URI parser
 *
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0) {
        hostname[0] = '\0';
        return -1;
    }

    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';

    /* Extract the port number */
    *port = 80; /* default */
    if (*hostend == ':')
        *port = atoi(hostend + 1);

    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
        pathname[0] = '\0';
    }
    else {
        pathbegin++;
        strcpy(pathname, pathbegin);
    }

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring.
 *
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr,
                     char *uri, int size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /*
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 13, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;


    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s", time_str, a, b, c, d, uri);
}


