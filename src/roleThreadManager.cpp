#include "roleThreadManager.h"

void startSession(std::string username, std::string passwd,int32_t ttl){
	Session* session = new Session(ttl);
	session->start(username, passwd);  // start() uruchamia wątek i run()
	addProcessToBuffer(session);
}

void addBuser(std::string newUsername, std::string newPasswd, std::string newEmail, bool useHashing, std::string sessionUsername, std::string sessionPasswd){
	Task t;
	t.user = new buser(getNextUserId(), newUsername, newPasswd, newEmail, useHashing);
	for (size_t i = 0; i < processBuffer.size(); i++){
		if(checkUserProcess(sessionUsername, sessionPasswd)){
			Session* session = processBuffer[i];
			session->submit(t, 1);
			break;
		}
	}
}

void waitForAllProcessesToFinish(){
	for (size_t i = 0; i < processBuffer.size(); i++){
		Session* session = processBuffer[i];
		session->stop();
		delete session;
	}
	processBuffer.clear();
}
