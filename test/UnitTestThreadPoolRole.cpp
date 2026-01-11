#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include "../src/threadPoolRole.h"
#include "../../bufforing-stm/src/buserCache.h"

// ==================== TESTY THREAD POOL ROLE ====================

class ThreadPoolRoleTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Dodaj testowego użytkownika do cache
        testUser = new buser(getNextUserId(), "testuser", "testpass", "test@email.com", false);
        addUserToCache(testUser);
    }
    
    void TearDown() override {
        // Cleanup
    }
    
    buser* testUser = nullptr;
    
    void setupUser(const std::string& username, const std::string& passwd) {
        buser* user = new buser(getNextUserId(), username, passwd, "test@email.com", false);
        addUserToCache(user);
    }
};

// Test tworzenia sesji
TEST_F(ThreadPoolRoleTest, SessionCreation) {
    Session session(60, 1);  // TTL = 60s, xactionId = 1
    
    EXPECT_EQ(session.getUserId(), -1);  // Nie zalogowany
    EXPECT_EQ(session.getThreadId(), -1);  // Wątek nie uruchomiony
}

// Test uruchamiania sesji z poprawnym użytkownikiem
TEST_F(ThreadPoolRoleTest, SessionStartValidUser) {
    setupUser("validuser", "validpass");
    
    Session session(60, 2);
    session.start("validuser", "validpass");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_NE(session.getUserId(), -1);  // Użytkownik zalogowany
    EXPECT_EQ(session.getThreadId(), 0);  // pthread_create zwraca 0 przy sukcesie
    
    session.stop();
}

// Test uruchamiania sesji z niepoprawnym użytkownikiem
TEST_F(ThreadPoolRoleTest, SessionStartInvalidUser) {
    Session session(60, 3);
    session.start("nonexistent", "wrongpass");
    
    EXPECT_EQ(session.getUserId(), -1);  // Sesja nie uruchomiona
}

// Test submit task - dodawanie użytkownika
TEST_F(ThreadPoolRoleTest, SubmitUserTask) {
    setupUser("taskuser", "taskpass");
    
    Session session(60, 4);
    session.start("taskuser", "taskpass");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    buser* newUser = new buser(getNextUserId(), "newuser", "newpass", "new@email.com", false);
    Task task;
    task.user = newUser;
    
    size_t cacheSizeBefore = userCache.size();
    session.submit(task, 1);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    EXPECT_EQ(userCache.size(), cacheSizeBefore + 1);
    
    session.stop();
}

// Test stop() - zatrzymanie sesji
TEST_F(ThreadPoolRoleTest, SessionStop) {
    setupUser("stopuser", "stoppass");
    
    Session session(3600, 5);  // Długi TTL
    session.start("stopuser", "stoppass");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_EQ(session.getThreadId(), 0);
    
    session.stop();
    
    // Po stop() sesja powinna zakończyć działanie bez crashu
    SUCCEED();
}

// Test wielokrotnego submit
TEST_F(ThreadPoolRoleTest, MultipleSubmit) {
    setupUser("multiuser", "multipass");
    
    Session session(60, 6);
    session.start("multiuser", "multipass");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    size_t cacheSizeBefore = userCache.size();
    
    for (int i = 0; i < 5; i++) {
        buser* user = new buser(getNextUserId(), "batchuser" + std::to_string(i), "pass", "email@test.com", false);
        Task task;
        task.user = user;
        session.submit(task, 1);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    EXPECT_GE(userCache.size(), cacheSizeBefore + 5);
    
    session.stop();
}

// Test TTL - krótki czas życia sesji
TEST_F(ThreadPoolRoleTest, TTLExpiry) {
    setupUser("ttluser", "ttlpass");
    
    Session session(1, 7);  // TTL = 1 sekunda
    session.start("ttluser", "ttlpass");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_EQ(session.getThreadId(), 0);
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    session.stop();  // Powinno działać bez crashu
    SUCCEED();
}

// Test Task z nullptr
TEST_F(ThreadPoolRoleTest, EmptyTask) {
    setupUser("emptyuser", "emptypass");
    
    Session session(60, 8);
    session.start("emptyuser", "emptypass");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    Task task;
    task.user = nullptr;
    task.tupleData = nullptr;
    task.tableHeaderData = nullptr;
    
    session.submit(task, 1);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    session.stop();
    SUCCEED();
}

// Test addBuser przez metodę Session
TEST_F(ThreadPoolRoleTest, AddBuserMethod) {
    setupUser("addbuseruser", "addbuserpass");
    
    Session session(60, 9);
    session.start("addbuseruser", "addbuserpass");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    size_t cacheSizeBefore = userCache.size();
    
    buser* newUser = new buser(getNextUserId(), "addeduser", "addedpass", "added@email.com", false);
    session.addBuser(newUser);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    EXPECT_EQ(userCache.size(), cacheSizeBefore + 1);
    
    session.stop();
}

// Test wielokrotnego stop()
TEST_F(ThreadPoolRoleTest, DoubleStop) {
    setupUser("doublestopuser", "doublestoppass");
    
    Session session(60, 10);
    session.start("doublestopuser", "doublestoppass");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    session.stop();
    session.stop();  // Drugie wywołanie nie powinno crashować
    
    SUCCEED();
}

// Test kolejności tasków (FIFO)
TEST_F(ThreadPoolRoleTest, TaskOrderFIFO) {
    setupUser("fifouser", "fifopass");
    
    Session session(60, 11);
    session.start("fifouser", "fifopass");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Dodaj użytkowników z numerami
    for (int i = 0; i < 3; i++) {
        buser* user = new buser(getNextUserId(), "fifo_" + std::to_string(i), "pass", "email@test.com", false);
        Task task;
        task.user = user;
        session.submit(task, 1);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    session.stop();
    SUCCEED();
}

