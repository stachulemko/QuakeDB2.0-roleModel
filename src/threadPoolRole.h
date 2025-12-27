#ifndef THREADPOOLROLE_H
#define THREADPOOLROLE_H

#include <pthread.h>
#include <queue>
#include <atomic>
#include <time.h>
#include "../../bufforing-stm/src/buserCache.h"
#include "buser.h"


struct Task {
    int32_t userIp=-1; // answer to ip adress
    buser *user=nullptr; // adding buser trough the task
    //std::promise<void> promise;  // Dodaj promise do synchronizacji
};

class Session {
public:
    explicit Session(int ttl = 3600);
    ~Session();
    
    void start(std::string username, std::string passwd);
    void submit(Task t,int32_t userIp);
    void stop();
    void addBuser(buser* user);
    //bool checkUserProcess(std::string username,std::string passwd);
    void run();
    int64_t getUserId(){return userId; };

private:
    static void* thread_entry(void* arg);
    bool checkUser(std::string username, std::string passwd);
    void setTtl(int seconds) { ttl = seconds; }
private:
    pthread_t thread{}; 
    pthread_mutex_t m{};
    pthread_cond_t cv{};

    int64_t userId;
    int ttl=3600; //time to live in seconds

    std::queue<Task> q;
    std::atomic<bool> stopping{false};
};

#endif
