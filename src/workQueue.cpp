#include "workQueue.h"

struct concurrentBag 
{
    int connFd;
    int listenFd;
};

class workQueue
{
    private:
        deque<int> jobsQueue;
        pthread_mutex_t jobsMutex;

    public:
        deque<int> getJobsQueue() {return this->jobsQueue;}
        pthread_mutex_t getJobsMutex() {return this->jobsMutex;}

        int addJob(int num) 
        {
            pthread_mutex_lock(&this->jobsMutex);
            this->jobsQueue.push_back(num);
            size_t len = this->jobsQueue.size();
            pthread_mutex_unlock(&this->jobsMutex);
            return len;
        }

        bool removeJob(int *job) 
        {
            pthread_mutex_lock(&this->jobsMutex);
            bool success = !this->jobsQueue.empty();
            if (success) 
            {
                *job = this->jobsQueue.front();
                this->jobsQueue.pop_front();
            }
            pthread_mutex_unlock(&this->jobsMutex);
            return success;
        }

};




