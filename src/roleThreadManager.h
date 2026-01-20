#ifndef ROLETHREADMANAGER_H
#define ROLETHREADMANAGER_H

#include "threadPoolRole.h"
#include "../../bufforing-stm/src/buserProcess.h"
#include "../../bufforing-stm/src/mvcc.h"

std::pair<int32_t,Session*> startSession(std::string username, std::string passwd,int32_t ttl,int64_t xactionId,std::string tablePath);

void addBuser(std::string newUsername, std::string newPasswd, std::string newEmail, bool useHashing, std::string sessionUsername, std::string sessionPasswd);

void waitForAllProcessesToFinish();

void addTuple(std::string pathToTablesData, int32_t tableId, std::vector<allVars> data, std::vector<bool> bitmap, std::string sessionUsername, std::string sessionPasswd);

void addTable(std::string sessionUsername, std::string sessionPasswd,std::vector<int8_t> types,std::vector<int8_t> typesWithAllowNull,std::vector<std::string> columnNames,int32_t tableId);

#endif