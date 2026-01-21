#ifndef SYSTHREADPOOL_H
#define SYSTHREADPOOL_H
#include <pthread.h>
#include <queue>
#include <iostream>
#include <vector>
#include "../../bufforing-stm/src/log.h"
#include "../../bufforing-stm/src/QuakeSharedBuffers.h"


extern std::vector<int32_t> threadPoolIds;

template<typename Cache, typename VectorType>
class SysThreadPool {
    private:
        pthread_t thread{}; 
        pthread_mutex_t m{};
        pthread_cond_t cv{};
        int32_t threadId=-1;
        bool cacheHintFlag = false;
        bool stopping = false;
        int32_t blockIndex = -1;
        Cache* cachePtr=nullptr;
        VectorType cacheEl = nullptr;
        VectorType elementToAdd = nullptr; 
        static void* thread_entry(void* arg) {
            static_cast<SysThreadPool*>(arg)->run();
            return nullptr;
        }

    public:
        SysThreadPool(Cache *cache){
            cachePtr = cache;
            pthread_mutex_init(&m, nullptr);
            pthread_cond_init(&cv, nullptr);
        }
        ~SysThreadPool(){
            stop();
            pthread_mutex_destroy(&m);
            pthread_cond_destroy(&cv);
        }
        
        void start(Cache *cache) {
            cachePtr = cache;
            int rc = pthread_create(&thread, nullptr, &SysThreadPool::thread_entry, this);
            if (rc != 0) {
                LOG_ERROR("pthread_create failed rc=" << rc);
            } else {
                LOG_DEBUG("Started thread, pthread_t=" << thread);
                threadPoolIds.push_back(thread);
            }
            //threadPoolIds.push_back(threadId);
            //LOG_DEBUG("Starting SysThreadPool for thread ID "<<threadId);
        }
        void stop() {
            pthread_mutex_lock(&m);
            stopping = true;
            pthread_cond_broadcast(&cv);
            pthread_mutex_unlock(&m);
            pthread_join(thread, nullptr);
        }
        /*
        void run(){
            pthread_mutex_lock(&m);  // Zablokuj mutex na początku
            while(!stopping){
                while(!cacheHintFlag && !stopping){
                    pthread_cond_wait(&cv, &m);
                }
                if(cacheHintFlag==true){
                    LOG_DEBUG("Cache hint received in SysThreadPool for thread ID "<<threadId);
                    if (stopping) {
                        break;
                    }
                    int32_t indexToDelete = 0;
                    int32_t minCount = (*cachePtr)[0] ? (*cachePtr)[0]->count : INT32_MAX;
                    bool isDirtyMin = (*cachePtr)[0] ? (*cachePtr)[0]->isDirty : false;
                    for (size_t i = 1; i < cachePtr->size(); ++i) {
                        cacheEl = (*cachePtr)[i]; 
                        if (!cacheEl) continue;
                        if(cacheEl->count < minCount){
                            minCount = cacheEl->count;
                            isDirtyMin = cacheEl->isDirty;
                            indexToDelete = i;
                        }
                        else if(cacheEl->count == minCount){
                            if(!cacheEl->isDirty && isDirtyMin){
                                minCount = cacheEl->count;
                                isDirtyMin = cacheEl->isDirty;
                                indexToDelete = i;
                            }
                        }
                    }
                    LOG_DEBUG("Evicting buffer at index " << indexToDelete);
                    cachePtr->erase(cachePtr->begin() + indexToDelete);
                    cacheHintFlag = false;
                }
                pthread_cond_wait(&cv, &m);
            }
            pthread_mutex_unlock(&m);  
        }
        */
        



        void run() {
            pthread_mutex_lock(&m);
            while (!stopping) {
                while (!cacheHintFlag && !stopping) {
                    pthread_cond_wait(&cv, &m);   // czeka, nie blokuje innych wątków/CLI
                }
                if (stopping || !cachePtr) break;

                cacheHintFlag = false;            // zużywamy sygnał
                int32_t indexToDelete = 0;
                int32_t minCount = (*cachePtr)[0] ? (*cachePtr)[0]->count : INT32_MAX;
                bool isDirtyMin = (*cachePtr)[0] ? (*cachePtr)[0]->isDirty : false;

                for (size_t i = 1; i < cachePtr->size(); ++i) {
                    cacheEl = (*cachePtr)[i];
                    if (!cacheEl) continue;
                    if (cacheEl->count < minCount ||
                    (cacheEl->count == minCount && !cacheEl->isDirty && isDirtyMin)) {
                        minCount = cacheEl->count;
                        isDirtyMin = cacheEl->isDirty;
                        indexToDelete = i;
                    }
                }
                LOG_DEBUG("Evicting buffer at index " << indexToDelete);
                addBufferDataToFile((*cachePtr)[indexToDelete]);
                (*cachePtr)[indexToDelete] = nullptr;
                addToFreeSlot(elementToAdd);
            }
            pthread_mutex_unlock(&m);
        }
        int32_t getThreadId(){return threadId; };
        bool isFull(){
            if (!cachePtr){
                LOG_DEBUG("cachePtr is null in SysThreadPool for thread ID "<<threadId);
                return false;   
            } 
            for (size_t i = 0; i < cachePtr->size(); ++i) {
                if((*cachePtr)[i] == nullptr){
                    LOG_DEBUG("Cache slot ["<<i<<"] is empty (nullptr) - cache not full");
                    return false;
                }
            }
            LOG_DEBUG("Cache is full in SysThreadPool for thread ID "<<threadId);
            return true;
        }
        void cacheHint(VectorType elementToAdd){
            if(isFull()){
                this->elementToAdd = elementToAdd;
                cacheHintFlag = true;
                pthread_cond_signal(&cv);
            }
            else{
                LOG_DEBUG("Cache not full, adding element directly without eviction");
                addToFreeSlot(elementToAdd);
            }
        }

};

#endif