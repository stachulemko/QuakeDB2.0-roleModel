#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <functional>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>



#include "buser.h"

#include "../../memory-mgmt/src/alltable.h"
#include "../../../src/insert.h"
#include "../../bufforing-stm/src/buserCache.h"

// Forward declaration - buserProcess.h will be included after UserProcess definition

struct UserProcess {
    pid_t pid;
    int64_t userId;
    std::string username;
    std::string email;
    bool isActive;
    int pipeToChild[2];    
    int pipeFromChild[2];
    
    UserProcess() : pid(-1), userId(-1), username(""), email(""), isActive(false) {
        pipeToChild[0] = -1;
        pipeToChild[1] = -1;
        pipeFromChild[0] = -1;
        pipeFromChild[1] = -1;
    }
    
    UserProcess(const buser& user) {
        pid = -1;
        userId = user.getId();
        username = user.getUsername();
        email = user.getEmail();
        isActive = false;
        pipeToChild[0] = -1;
        pipeToChild[1] = -1;
        pipeFromChild[0] = -1;
        pipeFromChild[1] = -1;
    }
    
    // Set user data from buser
    void setUserData(const buser& user) {
        userId = user.getId();
        username = user.getUsername();
        email = user.getEmail();
    }
};

// Include buserProcess after UserProcess is defined
#include "../../bufforing-stm/src/buserProcess.h"

inline std::vector<UserProcess*> userProcesses;

class UserProcessManager {
public:
    static std::map<int64_t, UserProcess> processes;
    static std::mutex mtx;

private:
    static std::string executeCommandInProcess(const buser& user, const std::string& command, allTable* table = nullptr) {
        std::stringstream response;
        
        std::clog << "[Process " << getpid() << "] Executing: " << command << std::endl;
        
        if (command.substr(0, 6) == "INSERT") {
            try {
                // Parse INSERT command: "INSERT|value1,value2,value3|null1,null2,null3"
                size_t firstPipe = command.find('|');
                size_t secondPipe = command.find('|', firstPipe + 1);
                
                if (firstPipe != std::string::npos && secondPipe != std::string::npos) {
                    // Get data
                    std::string dataStr = command.substr(firstPipe + 1, secondPipe - firstPipe - 1);
                    std::string bitmapStr = command.substr(secondPipe + 1);
                    
                    // Parse data (example: "100,test,42")
                    std::vector<allVars> data;
                    std::stringstream dataStream(dataStr);
                    std::string item;
                    while (std::getline(dataStream, item, ',')) {
                        // Try to convert to int, if fails use string
                        try {
                            data.push_back(allVars(std::stoi(item)));
                        } catch (...) {
                            data.push_back(allVars(item));
                        }
                    }
                    
                    // Parse bitmap (example: "0,0,0")
                    std::vector<bool> bitmap;
                    std::stringstream bitmapStream(bitmapStr);
                    std::string bitItem;
                    while (std::getline(bitmapStream, bitItem, ',')) {
                        bitmap.push_back(bitItem == "1" || bitItem == "true");
                    }
                    
                    // Call insertRowToTable with provided table reference
                    if (table != nullptr) {
                        insertRowToTable(*table, data, bitmap);
                        response << "INSERT executed for " << user.getUsername() 
                                 << " [" << data.size() << " columns]";
                    } else {
                        response << "ERROR: No table reference provided for INSERT";
                    }
                } else {
                    response << "ERROR: Invalid INSERT format";
                }
            } catch (const std::exception& e) {
                response << "ERROR INSERT: " << e.what();
            }
        }
       
        return response.str();
    }

public:
    static pid_t createUserProcess(const buser& user, allTable* table = nullptr) {
        std::lock_guard<std::mutex> lock(mtx);
        
        UserProcess up;
        
        // Create pipes
        if (pipe(up.pipeToChild) == -1 || pipe(up.pipeFromChild) == -1) {
            std::cerr << "Error creating pipes!" << std::endl;
            return -1;
        }
        
        pid_t pid = fork();
        
        if (pid < 0) {
            std::cerr << "Error fork()!" << std::endl;
            return -1;
        }
        else if (pid == 0) {
            // ===== CHILD PROCESS - LISTENING LOOP =====
            
            close(up.pipeToChild[1]);    // Close write
            close(up.pipeFromChild[0]);  // Close read
            
            std::clog << "[Process " << getpid() << "] User " 
                      << user.getUsername() << " logged in. Waiting for commands..." << std::endl;
            
            char buffer[4096];
            while (true) {
                // Read command from pipe
                memset(buffer, 0, sizeof(buffer));
                ssize_t bytesRead = read(up.pipeToChild[0], buffer, sizeof(buffer) - 1);
                
                if (bytesRead <= 0) {
                    std::clog << "[Process " << getpid() << "] Connection closed" << std::endl;
                    break;
                }
                
                std::string command(buffer);
                
                // Check EXIT
                if (command == "EXIT") {
                    std::clog << "[Process " << getpid() << "] Received EXIT" << std::endl;
                    std::string exitMsg = "Logged out " + user.getUsername();
                    write(up.pipeFromChild[1], exitMsg.c_str(), exitMsg.length());
                    break;
                }
                
                // Execute command with table reference
                std::string response = executeCommandInProcess(user, command, table);
                
                // Send response
                write(up.pipeFromChild[1], response.c_str(), response.length());
            }
            
            close(up.pipeToChild[0]);
            close(up.pipeFromChild[1]);
            
            std::clog << "[Process " << getpid() << "] Ending process" << std::endl;
            _exit(0);
        }
        else {
            // ===== PARENT PROCESS =====
            
            close(up.pipeToChild[0]);    // Close read
            close(up.pipeFromChild[1]);  // Close write
            
            up.pid = pid;
            up.setUserData(user);
            up.isActive = true;
            
            processes[user.getId()] = up;
            
            // Add process to buserProcess buffer
            addProcessToBuffer(&processes[user.getId()]);
            
            std::clog << "[Main] Created process " << pid 
                      << " for user " << user.getUsername() << std::endl;
            
            return pid;
        }
        
        return pid;
    }
    
    // Send command to user process
    static std::string sendCommand(int64_t userId, const std::string& command) {
        std::lock_guard<std::mutex> lock(mtx);

        auto it = processes.find(userId);
        if (it == processes.end() || !it->second.isActive) {
            return "ERROR: User not logged in or process inactive";
        }
        
        UserProcess& up = it->second;
        
        // Send command
        ssize_t written = write(up.pipeToChild[1], command.c_str(), command.length());
        if (written <= 0) {
            return "ERROR: Cannot send command";
        }
        
        // Receive response
        char buffer[4096];
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesRead = read(up.pipeFromChild[0], buffer, sizeof(buffer) - 1);
        
        if (bytesRead > 0) {
            return std::string(buffer);
        }
        
        return "ERROR: No response";
    }
    
    // Terminate user process
    static bool terminateUserProcess(int64_t userId) {
        std::lock_guard<std::mutex> lock(mtx);
        
        auto it = processes.find(userId);
        if (it == processes.end()) {
            return false;
        }
        
        UserProcess& up = it->second;
        
        // Send EXIT command
        std::string exitCmd = "EXIT";
        write(up.pipeToChild[1], exitCmd.c_str(), exitCmd.length());
        
        // Wait for graceful shutdown
        sleep(1);
        
        // Close pipes
        close(up.pipeToChild[1]);
        close(up.pipeFromChild[0]);
        
        // Wait for process
        int status;
        waitpid(up.pid, &status, 0);
        
        std::clog << "[Main] Terminated process " << up.pid 
                  << " for user " << up.username << std::endl;
        
        // Remove process from buserProcess buffer
        removeProcessFromBuffer(&up);
        
        processes.erase(it);
        return true;
    }
    
    static void showAllProcesses() {
        std::lock_guard<std::mutex> lock(mtx);
        
        std::clog << "\n=== Active user processes ===" << std::endl;
        std::clog << "UserID\tPID\tUsername\tStatus" << std::endl;
        std::clog << "------\t---\t--------\t------" << std::endl;
        
        for (const auto& pair : processes) {
            bool alive = kill(pair.second.pid, 0) == 0;
            std::clog << pair.second.userId << "\t" 
                      << pair.second.pid << "\t"
                      << pair.second.username << "\t\t"
                      << (alive ? "Active" : "Terminated") << std::endl;
        }
        std::clog << "=============================\n" << std::endl;
    }
};

inline std::map<int64_t, UserProcess> UserProcessManager::processes;
inline std::mutex UserProcessManager::mtx;

// Globalny vector do przechowywania wszystkich użytkowników (buffer)
inline std::vector<buser*> registeredUsers;
inline std::mutex usersMutex;

// ===== REGISTRATION AND LOGIN =====

// Register new user (add to buffer)
inline bool signUp(const buser& user) {
    std::lock_guard<std::mutex> lock(usersMutex);
    
    // Check if user already exists
    for (const auto& existingUser : registeredUsers) {
        if (existingUser->getUsername() == user.getUsername()) {
            std::clog << "[SignUp] User " << user.getUsername() << " already exists!" << std::endl;
            return false;
        }
    }
    
    // Add user to buffer
    buser* newUser = new buser(user.getId(), user.getUsername(), user.getPasswd(), user.getEmail(), false);
    registeredUsers.push_back(newUser);
    
    // Add user to buserCache
    addUserToCache(newUser);
    
    std::clog << "[SignUp] Registered user: " << user.getUsername() << std::endl;
    return true;
}

// Check if user exists in buffer (for login)
inline buser* findUser(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(usersMutex);
    
    for (auto& user : registeredUsers) {
        if (user->getUsername() == username && user->getPasswd() == password) {
            return user;
        }
    }
    
    return nullptr;
}

// Login user (create process)
inline pid_t logIn(const std::string& username, const std::string& password, allTable* table = nullptr) {
    buser* user = findUser(username, password);
    
    if (user == nullptr) {
        std::clog << "[LogIn] Error: Invalid username or password!" << std::endl;
        return -1;
    }
    
    // Check if user is already logged in (separate scope for mutex)
    {
        std::lock_guard<std::mutex> lock(UserProcessManager::mtx);
        auto it = UserProcessManager::processes.find(user->getId());
        if (it != UserProcessManager::processes.end() && it->second.isActive) {
            std::clog << "[LogIn] User " << username << " is already logged in!" << std::endl;
            return it->second.pid;
        }
    }
    
    // Create process for user (createUserProcess has its own lock)
    std::clog << "[LogIn] Logging in user: " << username << std::endl;
    return UserProcessManager::createUserProcess(*user, table);
}

// Logout user (terminate process)
inline bool logOut(int64_t userId) {
    std::clog << "[LogOut] Logging out user ID: " << userId << std::endl;
    return UserProcessManager::terminateUserProcess(userId);
}

// Logout user by name
inline bool logOut(const std::string& username) {
    std::lock_guard<std::mutex> lock(UserProcessManager::mtx);
    
    for (auto& pair : UserProcessManager::processes) {
        if (pair.second.username == username) {
            int64_t userId = pair.first;
            UserProcessManager::mtx.unlock();
            return logOut(userId);
        }
    }
    
    std::clog << "[LogOut] User not found: " << username << std::endl;
    return false;
}

// Show all registered users
inline void showRegisteredUsers() {
    std::lock_guard<std::mutex> lock(usersMutex);
    
    std::clog << "\n=== Registered users ===" << std::endl;
    std::clog << "ID\tUsername\tEmail" << std::endl;
    std::clog << "--\t--------\t-----" << std::endl;
    
    for (const auto& user : registeredUsers) {
        std::clog << user->getId() << "\t" 
                  << user->getUsername() << "\t\t"
                  << user->getEmail() << std::endl;
    }
    
    std::clog << "Total: " << registeredUsers.size() << " users" << std::endl;
    std::clog << "========================\n" << std::endl;
}

// ===== OLD API (to be removed or renamed) =====

// Deprecated: Use signUp() instead
inline pid_t singUpUser(const buser& user) {
    std::clog << "[DEPRECATED] Use signUp() for registration and logIn() for login!" << std::endl;
    return UserProcessManager::createUserProcess(user);
}

inline std::string executeCommand(int64_t userId, const std::string& command) {
    return UserProcessManager::sendCommand(userId, command);
}

// Helper: Execute INSERT with table reference - directly calls insertRowToTable
inline std::string executeInsert(int64_t userId, allTable& table, const std::vector<allVars>& data, const std::vector<bool>& bitmap) {
    // Check if user is logged in
    {
        std::lock_guard<std::mutex> lock(UserProcessManager::mtx);
        auto it = UserProcessManager::processes.find(userId);
        if (it == UserProcessManager::processes.end() || !it->second.isActive) {
            return "ERROR: User not logged in or process inactive";
        }
    }
    
    try {
        // Directly call insertRowToTable with the provided table
        insertRowToTable(table, const_cast<std::vector<allVars>&>(data), const_cast<std::vector<bool>&>(bitmap));
        return "INSERT executed for userId " + std::to_string(userId) + " [" + std::to_string(data.size()) + " columns]";
    } catch (const std::exception& e) {
        return "ERROR INSERT: " + std::string(e.what());
    }
}

// Helper: Execute INSERT with MVCC - directly calls insertRowToTableWithMvcc
inline std::string executeInsertWithMvcc(int64_t userId, allTable& table, 
                                          int64_t xmin, int64_t xmax, int32_t cid, 
                                          int32_t infomask, int16_t hoff, bool bitmapFlag,
                                          int64_t oid, std::vector<bool> bitMap, 
                                          std::vector<allVars> data,
                                          int32_t nextblock, int32_t blockIdentification,
                                          int32_t pd_lsn, int16_t pd_checksum, 
                                          int16_t pd_flags, int8_t contain_toast) {
    // Check if user is logged in
    {
        std::lock_guard<std::mutex> lock(UserProcessManager::mtx);
        auto it = UserProcessManager::processes.find(userId);
        if (it == UserProcessManager::processes.end() || !it->second.isActive) {
            return "ERROR: User not logged in or process inactive";
        }
    }
    
    try {
        // Directly call insertRowToTableWithMvcc
        insertRowToTableWithMvcc(table, xmin, xmax, cid, infomask, hoff, bitmapFlag, 
                                  oid, bitMap, data, nextblock, blockIdentification,
                                  pd_lsn, pd_checksum, pd_flags, contain_toast);
        return "INSERT WITH MVCC executed for userId " + std::to_string(userId) + " [" + std::to_string(data.size()) + " columns]";
    } catch (const std::exception& e) {
        return "ERROR INSERT MVCC: " + std::string(e.what());
    }
}

// Deprecated: Use logOut() instead
inline void SingOutUser(int64_t userId) {
    std::clog << "[DEPRECATED] Use logOut() instead of SingOutUser()!" << std::endl;
    UserProcessManager::terminateUserProcess(userId);
}

inline void showActiveUsers() {
    UserProcessManager::showAllProcesses();
}

#endif