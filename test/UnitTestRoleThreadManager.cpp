// Google Test suite for roleThreadManager
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "../src/roleThreadManager.h"
#include "../../bufforing-stm/src/buserCache.h"
#include "../../bufforing-stm/src/buserProcess.h"


/*
class RoleThreadManagerTest : public ::testing::Test {
protected:
	void setupUser(const std::string &username, const std::string &passwd) {
		buser *user = new buser(getNextUserId(), username, passwd, "test@email.com", false);
		addUserToCache(user);
	}
};

TEST_F(RoleThreadManagerTest, StartSession) {
	setupUser("sessionuser1", "sessionpass1");

	auto [threadId, session] = startSession("sessionuser1", "sessionpass1", 1, 100);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	EXPECT_NE(session, nullptr);
	EXPECT_EQ(threadId, 0);
	EXPECT_NE(session->getUserId(), -1);
	EXPECT_GT(processBuffer.size(), 0u);

	session->stop();
}

TEST_F(RoleThreadManagerTest, StartSessionInvalidUser) {
	auto [threadId, session] = startSession("nonexistent", "wrongpass", 1, 101);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	EXPECT_NE(session, nullptr);
	EXPECT_EQ(session->getUserId(), -1);
}

TEST_F(RoleThreadManagerTest, AddBuserThroughManager) {
	setupUser("manageruser1", "managerpass1");

	auto [threadId, session] = startSession("manageruser1", "managerpass1", 1, 102);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	size_t cacheSizeBefore = userCache.size();
	addBuser("newmanageruser", "newmanagerpass", "new@email.com", false, "manageruser1", "managerpass1");
	std::this_thread::sleep_for(std::chrono::milliseconds(300));

	EXPECT_EQ(userCache.size(), cacheSizeBefore + 1);

	session->stop();
}

TEST_F(RoleThreadManagerTest, MultipleSessions) {
	setupUser("multi1", "pass1");
	setupUser("multi2", "pass2");
	setupUser("multi3", "pass3");

	size_t bufferSizeBefore = processBuffer.size();

	auto [id1, session1] = startSession("multi1", "pass1", 1, 200);
	auto [id2, session2] = startSession("multi2", "pass2", 1, 201);
	auto [id3, session3] = startSession("multi3", "pass3", 1, 202);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	EXPECT_GE(processBuffer.size(), bufferSizeBefore + 3);

	session1->stop();
	session2->stop();
	session3->stop();
}

TEST_F(RoleThreadManagerTest, WaitForAllProcessesToFinish) {
	setupUser("waituser1", "waitpass1");
	setupUser("waituser2", "waitpass2");

	auto [id1, session1] = startSession("waituser1", "waitpass1", 1, 300);
	auto [id2, session2] = startSession("waituser2", "waitpass2", 1, 301);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	addBuser("waitnew1", "pass", "email@test.com", false, "waituser1", "waitpass1");
	addBuser("waitnew2", "pass", "email@test.com", false, "waituser2", "waitpass2");

	waitForAllProcessesToFinish();
	SUCCEED();
}

TEST_F(RoleThreadManagerTest, AddTupleThroughManager) {
	setupUser("tupleuser", "tuplepass");

	auto [threadId, session] = startSession("tupleuser", "tuplepass", 1, 400);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	std::vector<allVars> data;
	int32_t value = 42;
	data.push_back(value);
	std::vector<bool> bitmap = {true};

	addTuple("data/tablesData/", 1, data, bitmap, "tupleuser", "tuplepass");
	std::this_thread::sleep_for(std::chrono::milliseconds(200));

	session->stop();
	SUCCEED();
}

TEST_F(RoleThreadManagerTest, AddTableThroughManager) {
	setupUser("tableuser", "tablepass");

	auto [threadId, session] = startSession("tableuser", "tablepass", 60, 500);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	std::vector<int8_t> types = {1, 2};
	std::vector<int8_t> typesWithNull = {1, 1};
	std::vector<std::string> columnNames = {"col1", "col2"};

	addTable("tableuser", "tablepass", types, typesWithNull, columnNames, 1);
	std::this_thread::sleep_for(std::chrono::milliseconds(300));

	session->stop();
	SUCCEED();
}

TEST_F(RoleThreadManagerTest, CheckUserProcess) {
	setupUser("checkuser", "checkpass");

	auto [threadId, session] = startSession("checkuser", "checkpass", 1, 600);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	EXPECT_TRUE(checkUserProcess("checkuser", "checkpass"));
	EXPECT_FALSE(checkUserProcess("nonexistent", "wrongpass"));

	session->stop();
}

TEST_F(RoleThreadManagerTest, ConcurrentUserAddition) {
	setupUser("concuser", "concpass");

	auto [threadId, session] = startSession("concuser", "concpass", 1, 700);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	size_t cacheSizeBefore = userCache.size();
	const int NUM_USERS = 10;

	std::vector<std::thread> threads;
	for (int i = 0; i < NUM_USERS; i++) {
		threads.emplace_back([i]() {
			addBuser("concnewuser" + std::to_string(i), "pass", "email@test.com", false, "concuser", "concpass");
		});
	}
	for (auto &t : threads) {
		t.join();
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	EXPECT_GE(userCache.size(), cacheSizeBefore + NUM_USERS);

	session->stop();
}

TEST_F(RoleThreadManagerTest, SessionsWithDifferentTTL) {
	setupUser("ttl1user", "ttl1pass");
	setupUser("ttl2user", "ttl2pass");

	auto [id1, session1] = startSession("ttl1user", "ttl1pass", 1, 800);
	auto [id2, session2] = startSession("ttl2user", "ttl2pass", 1, 801);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	EXPECT_EQ(session1->getThreadId(), 0);
	EXPECT_EQ(session2->getThreadId(), 0);

	std::this_thread::sleep_for(std::chrono::seconds(2));
	session1->stop();
	session2->stop();
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
*/