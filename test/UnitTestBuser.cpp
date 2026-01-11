#include <gtest/gtest.h>
#include <pthread.h>
#include <string>
#include "../src/buser.h"

// ==================== TESTY BUSER ====================

class BuserTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup przed każdym testem
    }
    
    void TearDown() override {
        // Cleanup po każdym teście
    }
};

// Test tworzenia użytkownika
TEST_F(BuserTest, Creation) {
    buser user(1, "testuser", "password123", "test@email.com", false);
    
    EXPECT_EQ(user.getId(), 1);
    EXPECT_EQ(user.getUsername(), "testuser");
    EXPECT_EQ(user.getPasswd(), "password123");
    EXPECT_EQ(user.getEmail(), "test@email.com");
}

// Test setterów
TEST_F(BuserTest, Setters) {
    buser user(1, "olduser", "oldpass", "old@email.com", false);
    
    user.setId(42);
    user.setUsername("newuser");
    user.setPasswd("newpass", false);
    user.setEmail("new@email.com");
    
    EXPECT_EQ(user.getId(), 42);
    EXPECT_EQ(user.getUsername(), "newuser");
    EXPECT_EQ(user.getPasswd(), "newpass");
    EXPECT_EQ(user.getEmail(), "new@email.com");
}

// Test getNextUserId - czy generuje unikalne ID
TEST_F(BuserTest, GetNextUserIdUnique) {
    int64_t id1 = getNextUserId();
    int64_t id2 = getNextUserId();
    int64_t id3 = getNextUserId();
    
    EXPECT_EQ(id2, id1 + 1);
    EXPECT_EQ(id3, id2 + 1);
}

// Test thread-safety getNextUserId
TEST_F(BuserTest, GetNextUserIdThreadSafe) {
    const int NUM_THREADS = 10;
    const int IDS_PER_THREAD = 100;
    pthread_t threads[NUM_THREADS];
    
    auto threadFunc = [](void*) -> void* {
        for (int i = 0; i < 100; i++) {
            getNextUserId();
        }
        return nullptr;
    };
    
    int64_t startId = getNextUserId();
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], nullptr, threadFunc, nullptr);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
    }
    
    int64_t endId = getNextUserId();
    
    EXPECT_EQ(endId, startId + NUM_THREADS * IDS_PER_THREAD + 1);
}

// Test pustych stringów
TEST_F(BuserTest, EmptyStrings) {
    buser user(0, "", "", "", false);
    
    EXPECT_EQ(user.getId(), 0);
    EXPECT_EQ(user.getUsername(), "");
    EXPECT_EQ(user.getPasswd(), "");
    EXPECT_EQ(user.getEmail(), "");
}

// Test długich stringów
TEST_F(BuserTest, LongStrings) {
    std::string longString(1000, 'a');
    buser user(1, longString, longString, longString, false);
    
    EXPECT_EQ(user.getUsername().length(), 1000u);
    EXPECT_EQ(user.getPasswd().length(), 1000u);
    EXPECT_EQ(user.getEmail().length(), 1000u);
}

// Test specjalnych znaków
TEST_F(BuserTest, SpecialChars) {
    buser user(1, "user@#$%", "pass!@#$", "test+special@email.com", false);
    
    EXPECT_EQ(user.getUsername(), "user@#$%");
    EXPECT_EQ(user.getPasswd(), "pass!@#$");
    EXPECT_EQ(user.getEmail(), "test+special@email.com");
}

// Test ujemnego ID
TEST_F(BuserTest, NegativeId) {
    buser user(-1, "user", "pass", "email@test.com", false);
    
    EXPECT_EQ(user.getId(), -1);
    
    user.setId(-999);
    EXPECT_EQ(user.getId(), -999);
}

// Test kopiowania danych
TEST_F(BuserTest, DataIntegrity) {
    std::string username = "original";
    std::string passwd = "secret";
    std::string email = "test@test.com";
    
    buser user(100, username, passwd, email, false);
    
    // Zmień oryginalne stringi
    username = "modified";
    passwd = "changed";
    email = "other@test.com";
    
    // User powinien mieć oryginalne wartości
    EXPECT_EQ(user.getUsername(), "original");
    EXPECT_EQ(user.getPasswd(), "secret");
    EXPECT_EQ(user.getEmail(), "test@test.com");
}

// Test max int64
TEST_F(BuserTest, MaxInt64Id) {
    int64_t maxId = INT64_MAX;
    buser user(maxId, "user", "pass", "email@test.com", false);
    
    EXPECT_EQ(user.getId(), maxId);
}

