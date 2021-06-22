#pragma once
#include <deque>
#include <pthread.h>
#include <sys/socket.h>

using namespace std;

struct concurrentBag;

class workQueue
{
    private:
        deque<int> jobsQueue;
        pthread_mutex_t jobsMutex;

    public:
        deque<int> getJobsQueue();
        pthread_mutex_t getJobsMutex();

        int addJob(int num);
        bool removeJob(int *job);
};