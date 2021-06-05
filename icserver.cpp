#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "pcsa_net.h"

using namespace std;

/* Rather arbitrary. In real life, be careful with buffer overflow */
#define MAXBUF 1024  

typedef struct sockaddr SA;

void writeToPage(int connFd, int outputFd)
{
    ssize_t bytesRead;
    char buf[MAXBUF];

    while ((bytesRead = read(connFd, buf, MAXBUF)) > 0)
    {
        ssize_t numToWrite = bytesRead;
        char *writeBuf = buf;
        while (numToWrite > 0)
        {
            ssize_t numWritten = write(outputFd, writeBuf, numToWrite);
            if (numWritten < 0)
            {
                fprintf(stderr, "ERROR writing, meh\n");
                break;
            }
            numToWrite -= numWritten;
            writeBuf += numWritten;
        }
    }
    printf("DEBUG: Connection closed\n");
}

void respond(int connFd, string url, string mime)
{
    char buf[MAXBUF];
    int urlFd = open(url.c_str(), O_RDONLY);
    char *msg = "404 Not Found";
    if (urlFd < 0)
    {
        sprintf(buf,
                "HTTP/1.1 404 Not Found\r\n"
                "Server: Micro\r\n"
                "Connection: close\r\n\r\n");
        write_all(connFd, buf, strlen(buf));
        write_all(connFd, msg, strlen(msg));
        return;
    }
    struct stat fstatbuf;
    fstat(urlFd, &fstatbuf);
    sprintf(buf,
            "HTTP/1.1 200 OK\r\n"
            "Server: Micro\r\n"
            "Connection: close\r\n"
            "Content-length: %lu\r\n"
            "Content-type: %s\r\n\r\n",
            fstatbuf.st_size, mime);
    write_all(connFd, buf, strlen(buf));
    writeToPage(urlFd, connFd);
}


void serve_http(int connFd, string rootFolder)
{
    char buf[MAXBUF];
    
	while (read_line(connFd, buf, MAXBUF) > 0)
	{
		printf("LOG: %s\n", buf);
		/* [METHOD] [URI] [HTTPVER] */
		char method[MAXBUF], url[MAXBUF], httpVer[MAXBUF];
		sscanf(buf, "%s %s %s", method, url, httpVer);

		char newPath[80];
		if (strcasecmp(method, "GET") == 0)
		{
			if (url[0] == '/')
			{
				sprintf(newPath, "%s%s", rootFolder, url);
				if (strstr(url, "html") != NULL)
				{
					respond(connFd, newPath, "text/html");
				}
				else if (strstr(url, "jpg") != NULL || strstr(url, "jpeg") != NULL)
				{
					respond(connFd, newPath, "image/jpeg");
				}
				else
				{
					respond(connFd, newPath, NULL);
				}
			}
		}
		else
		{
			printf("LOG: Unknown request\n");
		}
	}
        
}

int main(int argc, char* argv[]) {
    if (argc < 3)
    {
        printf("Not Enought Argument!\n");
        return EXIT_FAILURE;
    }

    int listenFd = open_listenfd(argv[1]);
    char *rootFolder = argv[2];

    while (true) 
    {
        struct sockaddr_storage clientAddr;
        socklen_t clientLen = sizeof(struct sockaddr_storage);

        int connFd = accept(listenFd, (SA *) &clientAddr, &clientLen);
        if (connFd < 0) { fprintf(stderr, "Failed to accept\n"); continue; }

        char hostBuf[MAXBUF], svcBuf[MAXBUF];
        if (getnameinfo((SA *) &clientAddr, clientLen, 
                        hostBuf, MAXBUF, svcBuf, MAXBUF, 0)==0) 
            printf("Connection from %s:%s\n", hostBuf, svcBuf);
        else
            printf("Connection from ?UNKNOWN?\n");
                
        serve_http(connFd, rootFolder);
        close(connFd);
    }

    return 0;
}
