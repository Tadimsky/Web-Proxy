/*
 * proxy.c - Web proxy for COMPSCI 512
 *
 */
#pragma GCC diagnostic ignored "-Wunused-variable"


#include <stdio.h>
#include "csapp.h"
#include <pthread.h>

#define   FILTER_FILE   "proxy.filter"
#define   LOG_FILE      "proxy.log"
#define   DEBUG_FILE    "proxy.debug"

#define MAX_CONNECTION_ATTEMPTS 5


/*============================================================
 * function declarations
 *============================================================*/

int find_target_address(char *uri,
        char *target_address,
        char *path,
        int *port);


void format_log_entry(char *logstring,
        int sock,
        char *uri,
        int size);

void *forwarder(void *args);

void *webTalk(void *args);

void secureTalk(int clientfd, rio_t client, char *inHost, char *version, int serverPort);

void ignore();

void debug_print(char* msg);

int debug;
int proxyPort;
int debugfd;
int logfd;
pthread_mutex_t mutex;

/* main function for the proxy program */

int main(int argc, char *argv[]) {
    int count = 0;
    int listenfd, connfd, clientlen, optval, serverPort, i;
    struct sockaddr_in clientaddr;
    struct hostent *hp;
    char *haddrp;
    sigset_t sig_pipe;
    pthread_t tid;
    int *args;

    if (argc < 2) {
        printf("Usage: ./%s port [debug] [webServerPort]\n", argv[0]);
        exit(1);
    }
    if (argc == 4)
        serverPort = atoi(argv[3]);
    else
        serverPort = 80;

    Signal(SIGPIPE, ignore);

    if (sigemptyset(&sig_pipe) || sigaddset(&sig_pipe, SIGPIPE))
        unix_error("creating sig_pipe set failed");
    if (sigprocmask(SIG_BLOCK, &sig_pipe, NULL) == -1)
        unix_error("sigprocmask failed");

    proxyPort = atoi(argv[1]);

    if (argc > 2)
        debug = atoi(argv[2]);
    else
        debug = 0;


    /* start listening on proxy port */

    listenfd = Open_listenfd(proxyPort);

    optval = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *) &optval, sizeof(int));

    if (debug) debugfd = Open(DEBUG_FILE, O_CREAT | O_TRUNC | O_WRONLY, 0666);

    logfd = Open(LOG_FILE, O_CREAT | O_TRUNC | O_WRONLY, 0666);


    /* if writing to log files, force each thread to grab a lock before writing
       to the files */
    pthread_mutex_init(&mutex, NULL);

    while (1) {

        clientlen = sizeof(clientaddr);

        /* accept a new connection from a client here */
        connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        debug_print("New connection");

        pthread_t clientThread;

        int * threadArgs;
        threadArgs = malloc(sizeof(int) * 2);
        threadArgs[0] = connfd;
        threadArgs[1] = serverPort;

        Pthread_create(&clientThread, NULL, webTalk, (void*)threadArgs);


        /* TODO: new connection has been made from client */
        /* TODO: create a new thread to handle this event so that we can listen for more connections */
    }

    if (debug) Close(debugfd);
    Close(logfd);
    pthread_mutex_destroy(&mutex);

    return 0;
}

/* a possibly handy function that we provide, fully written */

void parseAddress(char *url, char *host, char **file, int *serverPort) {
    char *point1;
    char *point2;
    char *saveptr;

    if (strstr(url, "http://"))
        url = &(url[7]);
    *file = strchr(url, '/');

    strcpy(host, url);

    /* first time strtok_r is called, returns pointer to host */
    /* strtok_r (and strtok) destroy the string that is tokenized */

    /* get rid of everything after the first / */

    strtok_r(host, "/", &saveptr);

    /* now look to see if we have a colon */

    point1 = strchr(host, ':');
    if (!point1) {
        *serverPort = 80;
        return;
    }

    /* we do have a colon, so get the host part out */
    strtok_r(host, ":", &saveptr);

    /* now get the part after the : */
    *serverPort = atoi(strtok_r(NULL, "/", &saveptr));
}



/* this is the function that I spawn as a thread when a new
   connection is accepted */

/* you have to write a lot of it */

void *webTalk(void *args) {
    int numBytes, lineNum, serverfd, clientfd, serverPort;
    int tries;
    int byteCount = 0;
    char buf1[MAXLINE], buf2[MAXLINE], buf3[MAXLINE];
    char host[MAXLINE];
    char url[MAXLINE], logString[MAXLINE];
    char *token, *cmd, *version, *file, *saveptr;
    rio_t server, client;
    char slash[10];
    strcpy(slash, "/");


    clientfd = ((int *) args)[0];
    serverPort = ((int *) args)[1];
    free(args);

    Rio_readinitb(&client, clientfd);

    /* Read the Request Header - GET/CONNECT/POST/etc. */
    numBytes = Rio_readlineb(&client, buf1, MAXLINE);

    if (numBytes < 0 || buf1 == NULL) {
    	debug_print("Invalid Request.");
    	return NULL;
    }
    /* Splitting things apart - need to save state */
    char strtokState[MAXLINE];
    char * httpMethod;
    httpMethod = strtok_r(buf1, " ", &strtokState);

    if (httpMethod == NULL) {
    	debug_print("Invalid Request.");
    	return NULL;
    }

    if (strcmp(httpMethod, "GET") == 0) {
    	/* Get the URL of the Request */
    	char * requestParts = strtok_r(NULL, " ", &strtokState);
    	if (requestParts == NULL) {
    		debug_print("Invalid Request.");
    		return NULL;
    	}

    	if (find_target_address(requestParts, host, url, &serverPort) < 0) {
    		debug_print("Could not Parse Request.");
    		return NULL;
    	}
		/* better naming */
    	file = url;

    	/* Get the HTTP Version used */
    	char * httpVersion = NULL;

    	httpVersion = strtok_r(NULL, " ", &strtokState);
    	/* sometimes httpVersion is not specified by the client */
    	if (httpVersion == NULL) {
    		/* just make the httpVersion by \r\n for valid headers */
    		httpVersion = "\r\n";
    	}

    	serverfd = -1;
    	int connectionAttempts = 0;

    	/* connect until we succeed */
    	/* or if we exceed MAX_CONNECTION_ATTEMPTS - then exit */
    	while (serverfd < 0) {
    		if (connectionAttempts > MAX_CONNECTION_ATTEMPTS) {
				fprintf(stderr, "Could not connect to: %s\n", host);
				return NULL;
			}
    		serverfd = Open_clientfd(host, serverPort);
    		connectionAttempts++;
    	}

		Rio_readinitb(&server, serverfd);

		/* reformat the new GET header */
		sprintf(buf2, "%s %s %s", "GET", file, httpVersion);
		Rio_writen(serverfd, buf2, strlen(buf2));

		fprintf(stdout, "Raw Header: %s", buf1);
    	fprintf(stdout, "New Header: %s", buf2);

		/* while we haven't read the last line - the end of the request */
		while (strcmp(buf2, "\r\n") > 0) {
			// TODO: is this necessary?
			//memset((void*)buf2, 0, strlen(buf2));

			/* read new header from client */
			byteCount = Rio_readlineb(&client, buf2, MAXLINE);

			if (byteCount < 0 || buf2 == NULL) {
				debug_print("Did not receive header from client.");
				return NULL;
			}

			/* check for connection headers */
			if (strstr(buf2, "Connection: ") || strstr(buf2, "Proxy-Connection: ")) {
				/* we don't want to send keep-alive, set that to close. */
				/* We also want to send it as Connection to the server. */
				strcpy(buf2, "Connection: close\r\n");
			}
			if (strstr(buf2, "Keep-Alive:")) {
				/* don't send this at all - we don't likes it my precious */
			}
			else {
				/* update length of string in case of modifications to header */
				Rio_writen(serverfd, buf2, strlen(buf2));
			}
		}
		/* client sent last blank line in header requests - shutdown server connection */
		shutdown(serverfd, 1);

		debug_print("Sent Headers - now receiving");

		do {
			/* read the data from the server */
			byteCount = Rio_readnb(&server, buf3, MAXLINE);
			/* send it to the client */
			Rio_writen(clientfd, buf3, byteCount);
		}
		while (byteCount > 0);
		/* Means EOF: shutdown sending to client */
		shutdown(clientfd, 1);
		debug_print("Transferred.");
    }
    else {
    	if (strcmp(httpMethod, "CONNECT") == 0) {
			debug_print("CONNECT");

			/* need to parse this request */
			char * requestServer = strtok_r(NULL, " ", &strtokState);

			/* read the port and hostname */
			char * serverAddress = strtok(requestServer, ":");
			if (serverAddress == NULL) {
				return NULL;
			}
			char * port = strtok(NULL, " ");
			if (port == NULL) {
				port = "443";
			}
			serverPort = atoi(port);

			/* get the HTTP version */
			char * httpVersion = strtok_r(NULL, " ", &strtokState);
			httpVersion[strlen(httpVersion) - 2] = '\0';

			secureTalk(clientfd, client, serverAddress, httpVersion, serverPort);
    	}
    	else {
    		// a different HTTP request - POST, etc
    		debug_print("What just happened?");
    		debug_print(httpMethod);
    	}
    }
    return NULL;
}


/* this function handles the two-way encrypted data transferred in
   an HTTPS connection */

void secureTalk(int clientfd, rio_t client, char *inHost, char *version, int serverPort) {
    int serverfd, numBytes1, numBytes2;
    int tries;
    rio_t server;
    char buf1[MAXLINE], buf2[MAXLINE];
    pthread_t tid;
    int *args;

    if (serverPort == proxyPort)
        serverPort = 443;

    /* connect to the server */
    tries = 0;
    while (tries < MAX_CONNECTION_ATTEMPTS) {
    	serverfd = Open_clientfd(inHost, serverPort);
    	if (serverfd >= 0) {
    		break;
    	}
    }
    /* clientfd is browser */
    /* serverfd is server */

    //Rio_readinitb(&server, server);
    //buf1 = "";
    //Rio_writen(clientfd )
    /* let the client know we've connected to the server */

    /* spawn a thread to pass bytes from origin server through to client */

    /* now pass bytes from client to server */

}

/* this function is for passing bytes from origin server to client */

void *forwarder(void *args) {
    int numBytes, lineNum, serverfd, clientfd;
    int byteCount = 0;
    char buf1[MAXLINE];
    clientfd = ((int *) args)[0];
    serverfd = ((int *) args)[1];
    free(args);

    while (1) {

        /* serverfd is for talking to the web server */
        /* clientfd is for talking to the browser */

    }
}


void ignore() {
    ;
}


/*============================================================
 * url parser:
 *    find_target_address()
 *        Given a url, copy the target web server address to
 *        target_address and the following path to path.
 *        target_address and path have to be allocated before they 
 *        are passed in and should be long enough (use MAXLINE to be 
 *        safe)
 *
 *        Return the port number. 0 is returned if there is
 *        any error in parsing the url.
 *
 *============================================================*/

/*find_target_address - find the host name from the uri */
int find_target_address(char *uri, char *target_address, char *path,
        int *port) {
    //  printf("uri: %s\n",uri);


    if (strncasecmp(uri, "http://", 7) == 0) {
        char *hostbegin, *hostend, *pathbegin;
        int len;

        /* find the target address */
        hostbegin = uri + 7;
        hostend = strpbrk(hostbegin, " :/\r\n");
        if (hostend == NULL) {
            hostend = hostbegin + strlen(hostbegin);
        }

        len = hostend - hostbegin;

        strncpy(target_address, hostbegin, len);
        target_address[len] = '\0';

        /* find the port number */
        if (*hostend == ':') *port = atoi(hostend + 1);

        /* find the path */

        pathbegin = strchr(hostbegin, '/');

        if (pathbegin == NULL) {
            path[0] = '\0';

        }
        else {
        	// TODO: Removed this, why was it here? are things going to explode?!
            //pathbegin++;
            strcpy(path, pathbegin);
        }
        return 0;
    }
    target_address[0] = '\0';
    return -1;
}


/*============================================================
 * log utility
 *    format_log_entry
 *       Copy the formatted log entry to logstring
 *============================================================*/

void format_log_entry(char *logstring, int sock, char *uri, int size) {
    time_t now;
    char buffer[MAXLINE];
    struct sockaddr_in addr;
    unsigned long host;
    unsigned char a, b, c, d;
    socklen_t len = sizeof(addr);

    now = time(NULL);
    strftime(buffer, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    if (getpeername(sock, (struct sockaddr *) &addr, &len)) {
        /* something went wrong writing log entry */
        printf("getpeername failed\n");
        return;
    }

    host = ntohl(addr.sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;

    sprintf(logstring, "%s: %d.%d.%d.%d %s %d\n", buffer, a, b, c, d, uri, size);
}

void debug_print(char* msg) {
	fprintf(stdout, "%s\n", msg);
}
