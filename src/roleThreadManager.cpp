#include "roleThreadManager.h"

std::pair<int32_t,Session*> startSession(std::string username, std::string passwd,int32_t ttl,int64_t xactionId,std::string tablePath){
	Session* session = new Session(ttl, xactionId);
	session->start(username, passwd,tablePath);  // start() uruchamia wątek i run()
	addProcessToBuffer(session);
	return {session->getThreadId(), session};
}

void addBuser(std::string newUsername, std::string newPasswd, std::string newEmail, bool useHashing, std::string sessionUsername, std::string sessionPasswd){
	Task t;
	t.user = new buser(getNextUserId(), newUsername, newPasswd, newEmail, useHashing);
	pthread_mutex_lock(&processBufferMutex);
	for (size_t i = 0; i < processBuffer.size(); i++){
		if(checkUserProcess(sessionUsername, sessionPasswd)){
			Session* session = processBuffer[i];
			session->submit(t, 1);
			pthread_mutex_unlock(&processBufferMutex);
			return;
		}
	}
	pthread_mutex_unlock(&processBufferMutex);
}


void addTuple(std::string pathToTablesData, int32_t tableId, std::vector<allVars> data, std::vector<bool> bitmap, std::string sessionUsername, std::string sessionPasswd){
	Task t;
	tupleAdd* tupleData = new tupleAdd();
	tupleData->pathToTablesData = pathToTablesData;
	tupleData->tableId = tableId;
	tupleData->data = data;
	tupleData->bitmap = bitmap;
	t.tupleData = tupleData;
	for (size_t i = 0; i < processBuffer.size(); i++){
		if(checkUserProcess(sessionUsername, sessionPasswd)){
			Session* session = processBuffer[i];
			pthread_mutex_lock(&processBufferMutex);
			session->submit(t, 1);
			pthread_mutex_unlock(&processBufferMutex);
			return;
		}
	}
	LOG_ERROR("No session found for user "<<sessionUsername<<"\n");
}
void waitForAllProcessesToFinish(){
	pthread_mutex_lock(&processBufferMutex);
	for (size_t i = 0; i < processBuffer.size(); i++){
		Session* session = processBuffer[i];
		session->stop();
		//delete session;
	}
	pthread_mutex_unlock(&processBufferMutex);
	//processBuffer.clear();
}


void addTable(std::string sessionUsername, std::string sessionPasswd,std::vector<int8_t> types,std::vector<int8_t> typesWithAllowNull,std::vector<std::string> columnNames,int32_t tableId){
	LOG_DEBUG("Adding table through session for user "<<sessionUsername<<"\n");
	for (size_t i = 0; i < processBuffer.size(); i++){
		if(checkUserProcess(sessionUsername, sessionPasswd)){
			LOG_DEBUG("Found session for user "<<sessionUsername<<"\n");
			Session* session = processBuffer[i];
			tableHeaderAdd* tableAddPtr = new tableHeaderAdd();
			tableHeader* tableHeaderPtr = new tableHeader();
			pthread_mutex_lock(&processBufferMutex);
			int64_t transactionId = getTransactionAndIncrement();
			pthread_mutex_unlock(&processBufferMutex);
			LOG_DEBUG("transactionId for table header: "<<transactionId<<"\n");
			tableHeaderPtr->setData(transactionId,-1,-1,0,static_cast<int32_t>(columnNames.size()),session->getUserId(),0,0,0,0,types,typesWithAllowNull,columnNames);
			tableAddPtr->tableHeaderData = new tableHeader();
			tableAddPtr->tableHeaderData = tableHeaderPtr;
			//tableAddPtr->tableHeaderData->setData(getTransactionAndIncrement(),-1,-1,0,static_cast<int32_t>(columnNames.size()),session->getUserId(),0,0,0,0,types,typesWithAllowNull,columnNames);
			tableAddPtr->tableId = tableId;
			pthread_mutex_lock(&processBufferMutex);
			session->addTable(tableAddPtr);
			pthread_mutex_unlock(&processBufferMutex);
			return;
		}
	}
	LOG_ERROR("No session found for user "<<sessionUsername<<"\n");
}