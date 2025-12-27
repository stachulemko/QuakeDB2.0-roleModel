#include <iostream>
#include "threadPoolRole.h"

Session::Session(int ttl)
{
    this->ttl = ttl;
    //check user credentials
    pthread_mutex_init(&m, nullptr);
    pthread_cond_init(&cv, nullptr);
    
}

Session::~Session() {
    stop();
    pthread_mutex_destroy(&m);
    pthread_cond_destroy(&cv);
}

void Session::start(std::string username, std::string passwd) {
    if(!checkUser(username, passwd)){
        std::cerr << "Session start failed: invalid user credentials\n";
        return;
    }
    else{
        userId = getUserIdFromCache(username,passwd);
        stopping.store(false);
        pthread_create(&thread, nullptr, &Session::thread_entry, this);
    }
    
}

void Session::submit(Task t,int32_t userIp) {
    pthread_mutex_lock(&m);
    t.userIp = userIp;
    q.push(t);
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&m);
}

void Session::stop() {
    if (!stopping.exchange(true)) {
        pthread_mutex_lock(&m);
        pthread_cond_signal(&cv);
        pthread_mutex_unlock(&m);
        pthread_join(thread, nullptr);
    }
}

void* Session::thread_entry(void* arg) {
    static_cast<Session*>(arg)->run();
    return nullptr;
}


bool Session::checkUser(std::string username, std::string passwd){
    pthread_mutex_lock(&m);
    for (int i=0; i<userCache.size(); i++){
        buser* cachedUser = userCache[i];
        if(cachedUser->getUsername() == username && cachedUser->getPasswd() == passwd){
            pthread_mutex_unlock(&m);
            return true;
        }
    }
    std::cerr << "Authentication failed for user: " << username << "\n";
    
    
    pthread_mutex_unlock(&m);
    return false;
}


/*
void Session::run() {
    while (true) {
        pthread_mutex_lock(&m);

        while (q.empty() && !stopping.load()) {
            // timeout = teraz + ttl
            timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            //ts.tv_sec += ttl;

            int rc = pthread_cond_timedwait(&cv, &m, &ts);

            if (rc >= ttl) {
                stopping.store(true);
                break;
            }
        }

        if (stopping.load() && q.empty()) {
            pthread_mutex_unlock(&m);
            break;
        }

        Task t = q.front();
        q.pop();
        pthread_mutex_unlock(&m);
        if(t.user != nullptr){
            addUserToCache(t.user);
            std::cout << "User was created " << t.user->getUsername() << "\n";
            continue;
        }
    }
}
    */


void Session::run() {
    timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);
    end.tv_sec += ttl;

    while (true) {
        pthread_mutex_lock(&m);

        while (q.empty() && !stopping.load()) {
            int rc = pthread_cond_timedwait(&cv, &m, &end);

            if (rc == ETIMEDOUT) {
                std::cout<<"asasdadasddasdsada"<<std::endl;
                stopping.store(true);
                //removeProcessFromBuffer(this);  // Usuń z bufora po upływie TTL
                break;
            }
        }

        if (stopping.load() && q.empty()) {
            pthread_mutex_unlock(&m);
            std::cout<<"asdadsdasdadadad"<<std::endl;
            break;
        }

        Task t = q.front();
        q.pop();
        pthread_mutex_unlock(&m);

        if (t.user != nullptr) {
            addUserToCache(t.user);
            std::cout << "User was created " << t.user->getUsername() << "\n";
        }
        //t.promise.set_value();  // Sygnalizuj zakończenie zadania
    }
}


void Session::addBuser(buser* user){
    Task task;
    task.user=user;
    submit(task, -1);

}




//



