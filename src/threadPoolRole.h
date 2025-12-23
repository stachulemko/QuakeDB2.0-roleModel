#ifndef THREADPOOLROLE_H
#define THREADPOOLROLE_H

#include <pthread.h>
#include <queue>
#include <atomic>
#include <time.h>
#include "buser.h"

struct Task {
    int type;
    int payload;
};

class Session {
public:
    explicit Session(buser u, int ttlSeconds = 3600);
    ~Session();
    
    void start();
    void submit(const Task& t);
    void stop();

private:
    static void* thread_entry(void* arg);
    void run();

private:
    pthread_t thread{};
    pthread_mutex_t m{};
    pthread_cond_t cv{};

    buser user;
    int ttl; //time to live in seconds

    std::queue<Task> q;
    std::atomic<bool> stopping{false};
};

#endif
