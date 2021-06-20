#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <ctime>
#include <pthread.h>
extern "C"
{
    #include "parse.h"
    #include "pcsa_net.h"
}

#define MAX_HEADER_BUF 8192
#define MAXBUF 4096

using namespace std;

string port; 
string rootDir;
typedef struct sockaddr SA;

string getMIME(string string)
{
    if (string == "html") return "text/html";
    if (string == "plain") return "text/plain";
    if (string == "css") return "text/css";
    if ((string == "javascript") || (string == "js")) return "text/javascript";
    if (string == "mp4") return "video/mp4";
    if (string == "png") return "image/png";
    if ((string == "jpg") || (string == "jpeg")) return "image/jpg";
    if (string == "gif") return "image/gif";
    if (string == "mpeg") return "audio/mpeg";
    return "";
}

char* reponseRequest(char* buf, int numberStatus, char* status, unsigned long packetSize, char* mime)
{
    sprintf(buf,
            "HTTP/1.1 %d %s\r\n"
            "Server: ICWS\r\n"
            "Connection: close\r\n"
            "Content-length: %lu\r\n"
            "Content-type: %s\r\n\r\n",
            numberStatus, status, packetSize, mime);
    
    return buf;
}

char* errorRequest(char *buf, int numberStatus, char* status)
{
    sprintf(buf,
        "HTTP/1.1 %d %s\r\n"
        "Server: ICWS\r\n"
        "Connection: close\r\n",
        numberStatus, status);
    return buf;
}


void serve_http(int connFd, char *rootFolder)
{
    char buf[MAXBUF];

    int readRequest = read(connFd, buf, MAXBUF);

    Request *request = parse(buf,readRequest,connFd);

    char url[255];
    strcpy(url, rootFolder);
    strcat(url, request->http_uri);
    string mimeType;
    char* mimeFlag;
    struct stat stats;

    int inputFd = open(url, O_RDONLY);
    if (inputFd < 0)
    {
        printf("input failed\n");
        errorRequest(buf, 404, "Not Found");
        write_all(connFd, buf, strlen(buf));
        return;
    }
    mimeFlag = strrchr(url, '.');
    mimeFlag++;
    if (strcasecmp(request->http_method, "GET") == 0)
    {
        if(stat(url, &stats) >= 0)
        {
            mimeType = "";
            mimeType = getMIME(mimeFlag);
            reponseRequest(buf, 200, "OK", stats.st_size, (char*) mimeType.c_str());
            printf("buf = %s\n",buf);
            write_all(connFd, buf, strlen(buf));
            ssize_t numRead;
            while ((numRead = read(inputFd, buf, MAXBUF)) > 0)
            {
                write_all(connFd, buf, numRead);
            }
        }
        close(inputFd);
    }
    else if(strcasecmp(request->http_method, "HEAD") == 0)
    {
        if(stat(url, &stats) >= 0)
        {
            mimeType = "";
            mimeType = getMIME(mimeFlag);
            reponseRequest(buf, 200, "OK", stats.st_size, (char*) mimeType.c_str());
            write_all(connFd, buf, strlen(buf));
        }
        close(inputFd);
    }
    else 
    {
        errorRequest(buf, 501, "Unknown Method");
        write_all(connFd, buf, strlen(buf));
        close(inputFd);
    }
    free(request->headers);
    free(request);
}

int runServer() 
{
    int listenFd = open_listenfd((char*) port.c_str());
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
                
        serve_http(connFd, (char*) rootDir.c_str());
        close(connFd);
    }
}

int main(int argc, char **argv)
{
    if (argc != 5)
    {
        cout << "Not Enought Argument!" << endl;
        return EXIT_FAILURE;
    }
    if (string(argv[1]) != "--port")
    {
        cout << "invalid port command: try to use --port" << endl;
        return EXIT_FAILURE;
    }
    if (string(argv[3]) != "--root")
    {
        cout << "invalid port command: try to use --port" << endl;
        return EXIT_FAILURE;
    }
    else
    {
        port = string(argv[2]);
        rootDir = string(argv[4]);
    }
    runServer();
}