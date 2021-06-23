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
#include <pthread.h>
#include <thread>
#include <mutex>
#include <chrono>
#include "workQueue.cpp"
extern "C"
{
    #include "parse.h"
    #include "pcsa_net.h"
}

#define MAXBUF 8192


using namespace std;

string port; 
string rootDir;
typedef struct sockaddr SA;
int num_threads;
int timeout;
mutex serverMtx;
workQueue workQ;

// struct concurrentBag 
// {
//     struct sockaddr_storage clientAddr;
//     int connFd;
//     char *rootFolder;
// };

string getMIME(string string)
{
    if (string == "html") return "text/html";
    if (string == "css") return "text/css";
    if (string == "plain") return "text/plain";
    if ((string == "javascript") || (string == "js")) return "text/javascript";
    if (string == "png") return "image/png";
    if ((string == "jpg") || (string == "jpeg")) return "image/jpg";
    if (string == "gif") return "image/gif";
    if (string == "mp4") return "video/mp4";
    if (string == "mpeg") return "audio/mpeg";
    return "";
}

static const char* DAY_NAMES[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char* MONTH_NAMES[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

string currentDate()
{
    time_t now = chrono::system_clock::to_time_t(chrono::system_clock::now());

    char buf[100] = {0};
    strftime(buf, sizeof(buf), "%Y-%m-%d", localtime(&now));
    return buf;
}


char* reponseRequest(char* buf, int numberStatus, char* status, unsigned long packetSize, char* mime)
{
    string time = currentDate();
    sprintf(buf,
            "HTTP/1.1 %d %s\r\n"
            "Server: ICWS\r\n"
            "Date: %s\r\n"
            "Connection: connected\r\n"
            "Content-length: %lu\r\n"
            "Content-type: %s\r\n\r\n", // This Line White Screen Problem
            numberStatus, status, time.c_str() ,packetSize, mime);
    return buf;
}

char* errorRequest(char *buf, int numberStatus, char* status)
{
    sprintf(buf,
        "HTTP/1.1 %d %s\r\n"
        "Server: ICWS\r\n"
        "Connection: close\r\n\r\n",
        numberStatus, status);
    return buf;
}


void serve_http(int connFd, char *rootFolder)
{
    char buf[MAXBUF];

    int readRequest = read(connFd, buf, MAXBUF);

    int defout = dup(1);
    freopen("/dev/null", "w", stdout);

    serverMtx.lock();
    Request *request = parse(buf,readRequest,connFd);
    serverMtx.unlock();

    fflush(stdout);
    dup2(defout, 1);
    close(defout);
    if (request==NULL) {
        printf("NULL Request!\n"); 
        return;
    }

    char url[255];
    strcpy(url, rootFolder);
    strcat(url, request->http_uri);
    struct stat stats;

    int inputFd = open(url, O_RDONLY);
    if (inputFd < 0)
    {
        printf("input failed\n");
        char* msg = strdup("Not Found");
        errorRequest(buf, 404, msg);
        write_all(connFd, buf, strlen(buf));
        return;
    }
    string mimeType = "";
    char* mimeFlag = strrchr(url, '.');
    mimeFlag++;
    if (strcasecmp(request->http_method, "GET") == 0)
    {
        if(stat(url, &stats) >= 0)
        {
            mimeType = getMIME(mimeFlag);
            char* msg = strdup("OK");
            reponseRequest(buf, 200, msg, stats.st_size,  (char*) mimeType.c_str());
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
            mimeType = getMIME(mimeFlag);
            char* msg = strdup("OK");
            reponseRequest(buf, 200, msg, stats.st_size, (char*) mimeType.c_str());
            write_all(connFd, buf, strlen(buf));
        }
        close(inputFd);
    }
    else 
    {
        char* msg = strdup("Unknown Method");
        errorRequest(buf, 501, msg);
        write_all(connFd, buf, strlen(buf));
        close(inputFd);
    }
    free(request->headers);
    free(request);
    return;
}

// void* conn_handler(void *args) 
// {
//     struct concurrentBag* context = (struct concurrentBag*) args;
    
//     pthread_detach(pthread_self());
//     serve_http(context->connFd, context->rootFolder);
//     close(context->connFd);
    
//     free(context);
//     return NULL;
// }

void do_Work() {
    for (;;)
    {
        int w;
        if (workQ.removeJob(&w)) 
        {
            if (w < 0) break;
            serve_http(w, (char*) rootDir.c_str());
            close(w);
        }
        else 
        {
            //continue;
            //sleep(0);
            this_thread::yield();
            //usleep(250000);
        }
        
    }
}

int runServer() 
{
    thread worker[num_threads];
    for (int i = 0; i < num_threads; i++)
    {
        worker[i] = thread(do_Work);
    }
    int listenFd = open_listenfd((char*) port.c_str());
    while (true) 
    {
        struct sockaddr_storage clientAddr;
        socklen_t clientLen = sizeof(struct sockaddr_storage);
        //pthread_t threadInfo;

        int connFd = accept(listenFd, (SA *) &clientAddr, &clientLen);
        if (connFd < 0) { fprintf(stderr, "Failed to accept\n"); continue; }

        // struct concurrentBag *context = (struct concurrentBag *) malloc(sizeof(struct concurrentBag));

        // context->connFd = connFd;
        // context->rootFolder = (char*) rootDir.c_str();

        char hostBuf[MAXBUF], svcBuf[MAXBUF];
        if (getnameinfo((SA *) &clientAddr, clientLen, hostBuf, MAXBUF, svcBuf, MAXBUF, 0)==0) 
            printf("Connection from %s:%s\n", hostBuf, svcBuf);
        else
            printf("Connection from ?UNKNOWN?\n");
                
        // memcpy(&context->clientAddr, &clientAddr, sizeof(struct sockaddr_storage));
        // pthread_create(&threadInfo, NULL, conn_handler, (void *) context);
        workQ.addJob(connFd);
    }
}

int main(int argc, char **argv)
{
    if (argc != 9)
    {
        cout << "Not Enought Argument!" << endl;
        return EXIT_FAILURE;
    }
    if (string(argv[1]) != "--port")
    {
        cout << "invalid port command" << endl;
        return EXIT_FAILURE;
    }
    if (string(argv[3]) != "--root")
    {
        cout << "invalid root command" << endl;
        return EXIT_FAILURE;
    }
    if (string(argv[5]) != "--numThreads")
    {
        cout << "invalid thread number command" << endl;
        return EXIT_FAILURE;
    }
    if (string(argv[7]) != "--timeout")
    {
        cout << "invalid timeout command" << endl;
        return EXIT_FAILURE;
    }
    port = string(argv[2]);
    rootDir = string(argv[4]);
    num_threads = atoi(argv[6]);
    timeout = atoi(argv[8]);
    runServer();
}