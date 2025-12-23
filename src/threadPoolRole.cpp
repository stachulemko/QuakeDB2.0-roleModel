#include "Session.h"
#include <iostream>
#include <errno.h>

Session::Session(buser u, int ttlSeconds)
    : user(u), ttl(ttlSeconds)
{
    pthread_mutex_init(&m, nullptr);
    pthread_cond_init(&cv, nullptr);
}

Session::~Session() {
    stop();
    pthread_mutex_destroy(&m);
    pthread_cond_destroy(&cv);
}

void Session::start(std::string username;, std::string passwd) {
    stopping.store(false);
    pthread_create(&thread, nullptr, &Session::thread_entry, this);
}

void Session::submit(const Task& t) {
    pthread_mutex_lock(&m);
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

void Session::run() {
    while (true) {
        pthread_mutex_lock(&m);

        while (q.empty() && !stopping.load()) {
            // timeout = teraz + ttl
            timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += ttl;

            int rc = pthread_cond_timedwait(&cv, &m, &ts);

            if (rc >= ETIMEDOUT) {
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

        // --- wykonanie zadania ---
        std::cout << "User task: " << t.type << " payload=" << t.payload << "\n";
    }
}
