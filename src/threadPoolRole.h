#ifndef THREADPOOLROLE_H
#define THREADPOOLROLE_H

#include <pthread.h>
#include <queue>
#include <atomic>
#include <time.h>
#include "../../bufforing-stm/src/buserCache.h"
#include "buser.h"
#include "../../bufforing-stm/src/QuakeSharedBuffers.h"
#include "../../bufforing-stm/src/log.h"


struct tupleAdd {
        std::string pathToTablesData;
        int32_t tableId;
        std::vector<allVars> data;
        std::vector<bool> bitmap;
};

struct tableHeaderAdd {
    tableHeader* tableHeaderData;
    int32_t tableId;
};

struct Task {
    int32_t userIp=-1; // answer to ip adress
    buser *user=nullptr; // adding buser trough the task
    tupleAdd *tupleData=nullptr; // adding tuple task
    tableHeaderAdd *tableHeaderData=nullptr; // adding table task
    

};

class Session {
public:
    explicit Session(int ttl,int64_t xactionId);
    ~Session();
    
    void start(std::string username, std::string passwd,std::string tablePath);
    void submit(Task t,int32_t userIp);
    void stop();
    void addBuser(buser* user);
    void addTable(tableHeaderAdd* tableHeaderData);
    //bool checkUserProcess(std::string username,std::string passwd);
    void run();
    int64_t getUserId(){return userId; };
    int32_t getThreadId(){return threadId; };

private:
    static void* thread_entry(void* arg);
    bool checkUser(std::string username, std::string passwd);
    void setTtl(int seconds) { ttl = seconds; }
private:
    pthread_t thread{}; 
    pthread_mutex_t m{};
    pthread_cond_t cv{};

    int32_t threadId=-1;
    int64_t userId=-1;
    int32_t ttl=3600; //time to live in seconds
    int64_t xactionId=-1;
    std::string tablePath="";

    std::queue<Task> q;
    std::atomic<bool> stopping{false};
};

#endif
