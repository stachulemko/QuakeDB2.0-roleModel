#ifndef BUSER_H
#define BUSER_H

#include <iostream>
#include <pthread.h>
//#include <openssl/sha.h>
#include <iomanip>
#include <sstream>
#include <string>
//#include "../../bufforing-stm/src/buserCache.h"


/*
inline std::string sha256(const std::string& password) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)password.c_str(), password.size(), hash);
    
    std::stringstream ss;
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}
*/


inline int64_t id = 0;
inline pthread_mutex_t idMutex = PTHREAD_MUTEX_INITIALIZER;

inline int64_t getNextUserId(){
    pthread_mutex_lock(&idMutex);
    id++;
    int64_t result = id;
    pthread_mutex_unlock(&idMutex);
    return result;
}

class buser{
    private:
        int64_t id;
        std::string username;
        std::string passwd;
        std::string email;
    public:
        buser(int64_t id, std::string username,std::string passwd,std::string email,bool useHashing = true){
            this->id = id;
            this->username = username;
            if(useHashing){
                this->passwd =passwd; //sha256(passwd);
            } else {
                this->passwd = passwd;
            }
            this->email = email;
        };
        void setId(int64_t id){
            this->id = id;
        }
        void setUsername(std::string username){
            this->username = username;
        }
        void setPasswd(std::string passwd,bool useHashing = true){
            if(useHashing){
                this->passwd = passwd ;//sha256(passwd);
            } else {
                this->passwd = passwd;
            }
        }
        void setEmail(std::string email){
            this->email = email;
        }
        int64_t getId() const { return this->id; }
        std::string getUsername() const { return this->username; }
        std::string getPasswd() const { return this->passwd; }
        std::string getEmail() const { return this->email; }
};








#endif