#ifndef ROLETHREADMANAGER_H
#define ROLETHREADMANAGER_H

#include "threadPoolRole.h"
#include "../../bufforing-stm/src/buserProcess.h"

void startSession(std::string username, std::string passwd,int32_t ttl);

void addBuser(std::string newUsername, std::string newPasswd, std::string newEmail, bool useHashing, std::string sessionUsername, std::string sessionPasswd);

void waitForAllProcessesToFinish();

#endif