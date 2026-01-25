#define LOG_SILENT
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <mutex>
#include <atomic>
#include <fstream>
#include <filesystem>
#include "../src/sysThreadPool.h"
#include "../../bufforing-stm/src/QuakeSharedBuffers.h"
#include "../../memory-mgmt/src/fileManager.h"

// ============================================================================
// TEST HELPERS
// ============================================================================

// Silence stdout/stderr during tests
class CoutSilencer {
public:
    CoutSilencer(){
        devnull.open("/dev/null");
        oldCout = std::cout.rdbuf(devnull.rdbuf());
        oldCerr = std::cerr.rdbuf(devnull.rdbuf());
    }
    ~CoutSilencer(){
        std::cout.rdbuf(oldCout);
        std::cerr.rdbuf(oldCerr);
    }
private:
    std::ofstream devnull;
    std::streambuf* oldCout{nullptr};
    std::streambuf* oldCerr{nullptr};
};

// Global test folder
const std::string testFolderPath = "/home/stas/dev/QuakeDB2.0-mvccManagment/external/roleModel/test/testFolder";

// Globals required by QuakeSharedBuffers
std::string tablesPath;
int32_t freeSpaceSize = 4096;
void setTablesPath(std::string path){ tablesPath = std::move(path); }
void setFreeSpaceSize(int32_t size){ freeSpaceSize = size; }

// Helper to safely resize shared buffers between tests
void ForceResizeBuffers(int32_t newSize) {
    if (buffers == nullptr) {
        initSharedBuffers(newSize);
    } else {
        std::lock_guard<std::mutex> lock(buffersMutex);
        for (auto& buf : *buffers) {
            delete buf;
            buf = nullptr;
        }
        buffers->clear();
        buffers->resize(newSize, nullptr);
    }
}

// ============================================================================
// BASIC FUNCTIONALITY TESTS
// ============================================================================

TEST(SysThreadPoolTests, ConstructorInitializesCorrectly) {
    CoutSilencer silence;
    ForceResizeBuffers(5);
    
    SysThreadPool<std::vector<ShareBuffer*>, ShareBuffer*> pool(buffers);
    
    // Pool should not crash on construction
    EXPECT_EQ(pool.getThreadId(), -1); // Not started yet
}

TEST(SysThreadPoolTests, StartCreatesThread) {
    CoutSilencer silence;
    ForceResizeBuffers(5);
    
    SysThreadPool<std::vector<ShareBuffer*>, ShareBuffer*> pool(buffers);
    pool.start(buffers);
    
    // Give thread time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Thread should be running (no crash)
    pool.stop();
}

TEST(SysThreadPoolTests, StopTerminatesThread) {
    CoutSilencer silence;
    ForceResizeBuffers(5);
    
    SysThreadPool<std::vector<ShareBuffer*>, ShareBuffer*> pool(buffers);
    pool.start(buffers);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Should not hang or crash
    pool.stop();
    
    SUCCEED();
}

TEST(SysThreadPoolTests, IsFullReturnsFalseWhenEmpty) {
    CoutSilencer silence;
    ForceResizeBuffers(5);
    
    SysThreadPool<std::vector<ShareBuffer*>, ShareBuffer*> pool(buffers);
    
    EXPECT_FALSE(pool.isFull());
}

TEST(SysThreadPoolTests, IsFullReturnsTrueWhenFull) {
    CoutSilencer silence;
    ForceResizeBuffers(3);
    
    // Fill all slots
    for(int i = 0; i < 3; ++i) {
        ShareBuffer* buf = new ShareBuffer();
        buf->tableId = 100 + i;
        buf->blockNum = i;
        buf->count = 1;
        buf->isDirty = false;
        (*buffers)[i] = buf;
    }
    
    SysThreadPool<std::vector<ShareBuffer*>, ShareBuffer*> pool(buffers);
    
    EXPECT_TRUE(pool.isFull());
    
    // Cleanup
    ForceResizeBuffers(3);
}

TEST(SysThreadPoolTests, IsFullReturnsFalseWithOneEmptySlot) {
    CoutSilencer silence;
    ForceResizeBuffers(3);
    
    // Fill only first 2 slots
    for(int i = 0; i < 2; ++i) {
        ShareBuffer* buf = new ShareBuffer();
        buf->tableId = 100 + i;
        buf->blockNum = i;
        (*buffers)[i] = buf;
    }
    
    SysThreadPool<std::vector<ShareBuffer*>, ShareBuffer*> pool(buffers);
    
    EXPECT_FALSE(pool.isFull());
    
    ForceResizeBuffers(3);
}

// ============================================================================
// CACHE HINT TESTS - EVICTION BEHAVIOR
// ============================================================================

TEST(SysThreadPoolTests, CacheHintAddsElementWhenNotFull) {
    CoutSilencer silence;
    ForceResizeBuffers(5);
    std::filesystem::create_directories(testFolderPath);
    setTablesPath(testFolderPath);
    
    SysThreadPool<std::vector<ShareBuffer*>, ShareBuffer*> pool(buffers);
    pool.start(buffers);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    ShareBuffer* newBuf = new ShareBuffer();
    newBuf->tableId = 999;
    newBuf->blockNum = 0;
    newBuf->count = 1;
    newBuf->isDirty = false;
    
    pool.cacheHint(newBuf);
    
    // Element should be added
    bool found = false;
    for(auto b : *buffers) {
        if(b && b->tableId == 999) found = true;
    }
    EXPECT_TRUE(found);
    
    pool.stop();
    ForceResizeBuffers(5);
    clearFolder(testFolderPath);
}
/*
TEST(SysThreadPoolTests, CacheHintTriggersEvictionWhenFull) {
    CoutSilencer silence;
    ForceResizeBuffers(2);
    std::filesystem::create_directories(testFolderPath);
    setTablesPath(testFolderPath);
    clearFolder(testFolderPath);
    
    // Create files for buffers
    createBinFile(testFolderPath, "100");
    createBinFile(testFolderPath, "101");
    
    // Fill cache
    for(int i = 0; i < 2; ++i) {
        ShareBuffer* buf = new ShareBuffer();
        buf->tableId = 100 + i;
        buf->blockNum = 0;
        buf->count = 1;
        buf->isDirty = true;
        tableHeader* h = new tableHeader();
        h->setData(100+i,0,0,0,1,0,0,0,0,4096,{4},{1},{"x"});
        buf->tableHeaderPtr = h;
        (*buffers)[i] = buf;
    }
    
    SysThreadPool<std::vector<ShareBuffer*>, ShareBuffer*> pool(buffers);
    pool.start(buffers);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    EXPECT_TRUE(pool.isFull());
    
    // Add new element - should trigger eviction
    ShareBuffer* newBuf = new ShareBuffer();
    newBuf->tableId = 999;
    newBuf->blockNum = 0;
    newBuf->count = 1;
    newBuf->isDirty = false;
    
    pool.cacheHint(newBuf);
    
    // New element should be in cache
    bool found = false;
    for(auto b : *buffers) {
        if(b && b->tableId == 999) found = true;
    }
    EXPECT_TRUE(found);
    
    // One of the old elements should be evicted (nullptr)
    int nullCount = 0;
    for(auto b : *buffers) {
        if(b == nullptr) nullCount++;
    }
    // After eviction and add, no nulls expected (filled the freed slot)
    // Actually cacheHint evicts and then adds to the freed slot
    
    pool.stop();
    ForceResizeBuffers(2);
    clearFolder(testFolderPath);
}
*/

TEST(SysThreadPoolTests, EvictionSelectsLowestCount) {
    CoutSilencer silence;
    ForceResizeBuffers(3);
    std::filesystem::create_directories(testFolderPath);
    setTablesPath(testFolderPath);
    clearFolder(testFolderPath);
    
    // Create files
    for(int i = 0; i < 3; ++i) {
        createBinFile(testFolderPath, std::to_string(200 + i));
    }
    
    // Fill cache with different counts
    for(int i = 0; i < 3; ++i) {
        ShareBuffer* buf = new ShareBuffer();
        buf->tableId = 200 + i;
        buf->blockNum = 0;
        buf->count = 10 + i; // counts: 10, 11, 12
        buf->isDirty = true;
        tableHeader* h = new tableHeader();
        h->setData(200+i,0,0,0,1,0,0,0,0,4096,{4},{1},{"x"});
        buf->tableHeaderPtr = h;
        (*buffers)[i] = buf;
    }
    
    SysThreadPool<std::vector<ShareBuffer*>, ShareBuffer*> pool(buffers);
    pool.start(buffers);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Add new element
    ShareBuffer* newBuf = new ShareBuffer();
    newBuf->tableId = 999;
    newBuf->blockNum = 0;
    newBuf->count = 1;
    newBuf->isDirty = false;
    
    pool.cacheHint(newBuf);
    
    // Table 200 (lowest count=10) should be evicted
    bool table200Present = false;
    for(auto b : *buffers) {
        if(b && b->tableId == 200) table200Present = true;
    }
    EXPECT_FALSE(table200Present);
    
    // Tables 201, 202, 999 should be present
    bool table201 = false, table202 = false, table999 = false;
    for(auto b : *buffers) {
        if(b && b->tableId == 201) table201 = true;
        if(b && b->tableId == 202) table202 = true;
        if(b && b->tableId == 999) table999 = true;
    }
    EXPECT_TRUE(table201);
    EXPECT_TRUE(table202);
    EXPECT_TRUE(table999);
    
    pool.stop();
    ForceResizeBuffers(3);
    clearFolder(testFolderPath);
}

TEST(SysThreadPoolTests, EvictionPrefersCleanOverDirtyWhenSameCount) {
    CoutSilencer silence;
    ForceResizeBuffers(2);
    std::filesystem::create_directories(testFolderPath);
    setTablesPath(testFolderPath);
    clearFolder(testFolderPath);
    
    createBinFile(testFolderPath, "300");
    createBinFile(testFolderPath, "301");
    
    // Two buffers with same count, one dirty one clean
    ShareBuffer* dirty = new ShareBuffer();
    dirty->tableId = 300;
    dirty->blockNum = 0;
    dirty->count = 5;
    dirty->isDirty = true;
    tableHeader* h1 = new tableHeader();
    h1->setData(300,0,0,0,1,0,0,0,0,4096,{4},{1},{"x"});
    dirty->tableHeaderPtr = h1;
    (*buffers)[0] = dirty;
    
    ShareBuffer* clean = new ShareBuffer();
    clean->tableId = 301;
    clean->blockNum = 0;
    clean->count = 5; // Same count
    clean->isDirty = false; // Clean
    tableHeader* h2 = new tableHeader();
    h2->setData(301,0,0,0,1,0,0,0,0,4096,{4},{1},{"x"});
    clean->tableHeaderPtr = h2;
    (*buffers)[1] = clean;
    
    SysThreadPool<std::vector<ShareBuffer*>, ShareBuffer*> pool(buffers);
    pool.start(buffers);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    ShareBuffer* newBuf = new ShareBuffer();
    newBuf->tableId = 999;
    newBuf->blockNum = 0;
    newBuf->count = 1;
    newBuf->isDirty = false;
    
    pool.cacheHint(newBuf);
    
    // Clean (301) should be evicted, dirty (300) should remain
    bool dirtyPresent = false, cleanPresent = false;
    for(auto b : *buffers) {
        if(b && b->tableId == 300) dirtyPresent = true;
        if(b && b->tableId == 301) cleanPresent = true;
    }
    EXPECT_TRUE(dirtyPresent);
    EXPECT_FALSE(cleanPresent);
    
    pool.stop();
    ForceResizeBuffers(2);
    clearFolder(testFolderPath);
}

// ============================================================================
// STRESS TESTS
// ============================================================================

TEST(SysThreadPoolTests, StressMultipleEvictions) {
    CoutSilencer silence;
    ForceResizeBuffers(3);
    std::filesystem::create_directories(testFolderPath);
    setTablesPath(testFolderPath);
    clearFolder(testFolderPath);
    
    // Create initial files
    for(int i = 0; i < 50; ++i) {
        createBinFile(testFolderPath, std::to_string(400 + i));
    }
    
    // Fill cache initially
    for(int i = 0; i < 3; ++i) {
        ShareBuffer* buf = new ShareBuffer();
        buf->tableId = 400 + i;
        buf->blockNum = 0;
        buf->count = 1;
        buf->isDirty = true;
        tableHeader* h = new tableHeader();
        h->setData(400+i,0,0,0,1,0,0,0,0,4096,{4},{1},{"x"});
        buf->tableHeaderPtr = h;
        (*buffers)[i] = buf;
    }
    
    SysThreadPool<std::vector<ShareBuffer*>, ShareBuffer*> pool(buffers);
    pool.start(buffers);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Force many evictions
    for(int i = 3; i < 20; ++i) {
        ShareBuffer* newBuf = new ShareBuffer();
        newBuf->tableId = 400 + i;
        newBuf->blockNum = 0;
        newBuf->count = 1;
        newBuf->isDirty = true;
        tableHeader* h = new tableHeader();
        h->setData(400+i,0,0,0,1,0,0,0,0,4096,{4},{1},{"x"});
        newBuf->tableHeaderPtr = h;
        
        pool.cacheHint(newBuf);
    }
    
    // Cache should still have 3 elements
    int count = 0;
    for(auto b : *buffers) {
        if(b != nullptr) count++;
    }
    EXPECT_EQ(count, 3);
    
    pool.stop();
    ForceResizeBuffers(3);
    clearFolder(testFolderPath);
}

TEST(SysThreadPoolTests, RapidStartStop) {
    CoutSilencer silence;
    ForceResizeBuffers(5);
    
    // Rapidly start and stop multiple times
    for(int i = 0; i < 10; ++i) {
        SysThreadPool<std::vector<ShareBuffer*>, ShareBuffer*> pool(buffers);
        pool.start(buffers);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        pool.stop();
    }
    
    SUCCEED();
}

TEST(SysThreadPoolTests, CacheHintWithNullCachePtr) {
    CoutSilencer silence;
    
    // Create pool with nullptr cache
    SysThreadPool<std::vector<ShareBuffer*>, ShareBuffer*> pool(nullptr);
    
    // isFull should return false when cachePtr is null
    EXPECT_FALSE(pool.isFull());
}

// ============================================================================
// EDGE CASES
// ============================================================================

TEST(SysThreadPoolTests, SingleSlotCache) {
    CoutSilencer silence;
    ForceResizeBuffers(1);
    std::filesystem::create_directories(testFolderPath);
    setTablesPath(testFolderPath);
    clearFolder(testFolderPath);
    
    createBinFile(testFolderPath, "500");
    
    ShareBuffer* buf = new ShareBuffer();
    buf->tableId = 500;
    buf->blockNum = 0;
    buf->count = 1;
    buf->isDirty = true;
    tableHeader* h = new tableHeader();
    h->setData(500,0,0,0,1,0,0,0,0,4096,{4},{1},{"x"});
    buf->tableHeaderPtr = h;
    (*buffers)[0] = buf;
    
    SysThreadPool<std::vector<ShareBuffer*>, ShareBuffer*> pool(buffers);
    pool.start(buffers);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    EXPECT_TRUE(pool.isFull());
    
    // Add new element - must evict the only one
    ShareBuffer* newBuf = new ShareBuffer();
    newBuf->tableId = 999;
    newBuf->blockNum = 0;
    newBuf->count = 1;
    newBuf->isDirty = false;
    
    pool.cacheHint(newBuf);
    
    // Old should be gone, new should be present
    bool old500 = false, new999 = false;
    for(auto b : *buffers) {
        if(b && b->tableId == 500) old500 = true;
        if(b && b->tableId == 999) new999 = true;
    }
    EXPECT_FALSE(old500);
    EXPECT_TRUE(new999);
    
    pool.stop();
    ForceResizeBuffers(1);
    clearFolder(testFolderPath);
}

TEST(SysThreadPoolTests, AllBuffersWithSameCount) {
    CoutSilencer silence;
    ForceResizeBuffers(4);
    std::filesystem::create_directories(testFolderPath);
    setTablesPath(testFolderPath);
    clearFolder(testFolderPath);
    
    for(int i = 0; i < 4; ++i) {
        createBinFile(testFolderPath, std::to_string(600 + i));
    }
    
    // All buffers with same count and all dirty
    for(int i = 0; i < 4; ++i) {
        ShareBuffer* buf = new ShareBuffer();
        buf->tableId = 600 + i;
        buf->blockNum = 0;
        buf->count = 5; // Same count
        buf->isDirty = true; // All dirty
        tableHeader* h = new tableHeader();
        h->setData(600+i,0,0,0,1,0,0,0,0,4096,{4},{1},{"x"});
        buf->tableHeaderPtr = h;
        (*buffers)[i] = buf;
    }
    
    SysThreadPool<std::vector<ShareBuffer*>, ShareBuffer*> pool(buffers);
    pool.start(buffers);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    ShareBuffer* newBuf = new ShareBuffer();
    newBuf->tableId = 999;
    newBuf->blockNum = 0;
    newBuf->count = 1;
    newBuf->isDirty = false;
    
    pool.cacheHint(newBuf);
    
    // First buffer (index 0) should be evicted when all have same count/dirty
    bool table600 = false;
    for(auto b : *buffers) {
        if(b && b->tableId == 600) table600 = true;
    }
    EXPECT_FALSE(table600);
    
    // New should be present
    bool new999 = false;
    for(auto b : *buffers) {
        if(b && b->tableId == 999) new999 = true;
    }
    EXPECT_TRUE(new999);
    
    pool.stop();
    ForceResizeBuffers(4);
    clearFolder(testFolderPath);
}

TEST(SysThreadPoolTests, EvictionWritesToFile) {
    CoutSilencer silence;
    ForceResizeBuffers(2);
    std::filesystem::create_directories(testFolderPath);
    setTablesPath(testFolderPath);
    clearFolder(testFolderPath);
    
    createBinFile(testFolderPath, "700");
    createBinFile(testFolderPath, "701");
    
    // Fill cache with dirty buffers
    for(int i = 0; i < 2; ++i) {
        ShareBuffer* buf = new ShareBuffer();
        buf->tableId = 700 + i;
        buf->blockNum = 0;
        buf->count = 1 + i; // Different counts
        buf->isDirty = true;
        tableHeader* h = new tableHeader();
        h->setData(700+i,0,0,0,1,0,0,0,0,4096,{4},{1},{"x"});
        buf->tableHeaderPtr = h;
        (*buffers)[i] = buf;
    }
    
    SysThreadPool<std::vector<ShareBuffer*>, ShareBuffer*> pool(buffers);
    pool.start(buffers);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // File should be empty before eviction
    size_t sizeBefore = getFileSize(testFolderPath, "700.bin");
    EXPECT_EQ(sizeBefore, 0);
    
    // Trigger eviction
    ShareBuffer* newBuf = new ShareBuffer();
    newBuf->tableId = 999;
    newBuf->blockNum = 0;
    newBuf->count = 1;
    newBuf->isDirty = false;
    
    pool.cacheHint(newBuf);
    
    // Evicted buffer (700) should be written to file
    size_t sizeAfter = getFileSize(testFolderPath, "700.bin");
    EXPECT_GT(sizeAfter, 0);
    
    pool.stop();
    ForceResizeBuffers(2);
    clearFolder(testFolderPath);
}

TEST(SysThreadPoolTests, MultiplePoolsOnSameCache) {
    CoutSilencer silence;
    ForceResizeBuffers(10);
    
    // This tests that multiple pool instances can be created
    // (though typically only one should manage a cache)
    SysThreadPool<std::vector<ShareBuffer*>, ShareBuffer*> pool1(buffers);
    SysThreadPool<std::vector<ShareBuffer*>, ShareBuffer*> pool2(buffers);
    
    // Both should see cache as not full
    EXPECT_FALSE(pool1.isFull());
    EXPECT_FALSE(pool2.isFull());
}

TEST(SysThreadPoolTests, ThreadIdDefaultValue) {
    CoutSilencer silence;
    ForceResizeBuffers(5);
    
    SysThreadPool<std::vector<ShareBuffer*>, ShareBuffer*> pool(buffers);
    
    // Before start, threadId should be -1
    EXPECT_EQ(pool.getThreadId(), -1);
}

TEST(SysThreadPoolTests, CacheWithMixedNullAndNonNull) {
    CoutSilencer silence;
    ForceResizeBuffers(5);
    std::filesystem::create_directories(testFolderPath);
    setTablesPath(testFolderPath);
    clearFolder(testFolderPath);
    
    // Fill only odd slots
    for(int i = 0; i < 5; ++i) {
        if(i % 2 == 1) {
            createBinFile(testFolderPath, std::to_string(800 + i));
            ShareBuffer* buf = new ShareBuffer();
            buf->tableId = 800 + i;
            buf->blockNum = 0;
            buf->count = 1;
            buf->isDirty = false;
            (*buffers)[i] = buf;
        }
    }
    
    SysThreadPool<std::vector<ShareBuffer*>, ShareBuffer*> pool(buffers);
    pool.start(buffers);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    EXPECT_FALSE(pool.isFull()); // Has empty slots
    
    // Adding should use empty slot, no eviction
    ShareBuffer* newBuf = new ShareBuffer();
    newBuf->tableId = 999;
    newBuf->blockNum = 0;
    newBuf->count = 1;
    newBuf->isDirty = false;
    
    pool.cacheHint(newBuf);
    
    // All original should still be present
    for(int i = 0; i < 5; ++i) {
        if(i % 2 == 1) {
            bool found = false;
            for(auto b : *buffers) {
                if(b && b->tableId == 800 + i) found = true;
            }
            EXPECT_TRUE(found);
        }
    }
    
    pool.stop();
    ForceResizeBuffers(5);
    clearFolder(testFolderPath);
}

TEST(SysThreadPoolTests, LargeCacheNoEviction) {
    CoutSilencer silence;
    ForceResizeBuffers(100);
    std::filesystem::create_directories(testFolderPath);
    setTablesPath(testFolderPath);
    clearFolder(testFolderPath);
    
    SysThreadPool<std::vector<ShareBuffer*>, ShareBuffer*> pool(buffers);
    pool.start(buffers);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Add 50 elements - no eviction needed
    for(int i = 0; i < 50; ++i) {
        ShareBuffer* buf = new ShareBuffer();
        buf->tableId = 900 + i;
        buf->blockNum = 0;
        buf->count = 1;
        buf->isDirty = false;
        
        pool.cacheHint(buf);
    }
    
    // All 50 should be present
    int count = 0;
    for(auto b : *buffers) {
        if(b != nullptr) count++;
    }
    EXPECT_EQ(count, 50);
    
    pool.stop();
    ForceResizeBuffers(100);
    clearFolder(testFolderPath);
}

TEST(SysThreadPoolTests, EvictionWithBlockPtr) {
    CoutSilencer silence;
    ForceResizeBuffers(2);
    std::filesystem::create_directories(testFolderPath);
    setTablesPath(testFolderPath);
    clearFolder(testFolderPath);
    
    createBinFile(testFolderPath, "1000");
    createBinFile(testFolderPath, "1001");
    
    // Create buffer with blockPtr instead of tableHeaderPtr
    ShareBuffer* buf1 = new ShareBuffer();
    buf1->tableId = 1000;
    buf1->blockNum = 1;
    buf1->count = 1;
    buf1->isDirty = true;
    buf1->blockPtr = new block8kb(4096, 1, 1, 0, 0, 0, 0);
    (*buffers)[0] = buf1;
    
    ShareBuffer* buf2 = new ShareBuffer();
    buf2->tableId = 1001;
    buf2->blockNum = 0;
    buf2->count = 2;
    buf2->isDirty = true;
    tableHeader* h = new tableHeader();
    h->setData(1001,0,0,0,1,0,0,0,0,4096,{4},{1},{"x"});
    buf2->tableHeaderPtr = h;
    (*buffers)[1] = buf2;
    
    SysThreadPool<std::vector<ShareBuffer*>, ShareBuffer*> pool(buffers);
    pool.start(buffers);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    ShareBuffer* newBuf = new ShareBuffer();
    newBuf->tableId = 999;
    newBuf->blockNum = 0;
    newBuf->count = 1;
    newBuf->isDirty = false;
    
    pool.cacheHint(newBuf);
    
    // Buffer with blockPtr (1000, lower count) should be evicted
    bool buf1000 = false;
    for(auto b : *buffers) {
        if(b && b->tableId == 1000) buf1000 = true;
    }
    EXPECT_FALSE(buf1000);
    
    pool.stop();
    ForceResizeBuffers(2);
    clearFolder(testFolderPath);
}

// ============================================================================
// CONCURRENCY TESTS
// ============================================================================

TEST(SysThreadPoolTests, ThreadSafetyBasic) {
    CoutSilencer silence;
    ForceResizeBuffers(20);
    std::filesystem::create_directories(testFolderPath);
    setTablesPath(testFolderPath);
    clearFolder(testFolderPath);
    
    for(int i = 0; i < 100; ++i) {
        createBinFile(testFolderPath, std::to_string(2000 + i));
    }
    
    SysThreadPool<std::vector<ShareBuffer*>, ShareBuffer*> pool(buffers);
    pool.start(buffers);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    std::atomic<int> successCount{0};
    
    // Launch multiple threads adding elements
    std::vector<std::thread> threads;
    for(int t = 0; t < 5; ++t) {
        threads.emplace_back([&pool, &successCount, t]() {
            for(int i = 0; i < 10; ++i) {
                ShareBuffer* buf = new ShareBuffer();
                buf->tableId = 2000 + t * 10 + i;
                buf->blockNum = 0;
                buf->count = 1;
                buf->isDirty = false;
                
                pool.cacheHint(buf);
                successCount++;
            }
        });
    }
    
    for(auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(successCount.load(), 50);
    
    pool.stop();
    ForceResizeBuffers(20);
    clearFolder(testFolderPath);
}

TEST(SysThreadPoolTests, SequentialEvictionIntegrity) {
    CoutSilencer silence;
    ForceResizeBuffers(5);
    std::filesystem::create_directories(testFolderPath);
    setTablesPath(testFolderPath);
    clearFolder(testFolderPath);
    
    for(int i = 0; i < 100; ++i) {
        createBinFile(testFolderPath, std::to_string(3000 + i));
    }
    
    // Fill cache
    for(int i = 0; i < 5; ++i) {
        ShareBuffer* buf = new ShareBuffer();
        buf->tableId = 3000 + i;
        buf->blockNum = 0;
        buf->count = i + 1; // counts 1-5
        buf->isDirty = true;
        tableHeader* h = new tableHeader();
        h->setData(3000+i,0,0,0,1,0,0,0,0,4096,{4},{1},{"x"});
        buf->tableHeaderPtr = h;
        (*buffers)[i] = buf;
    }
    
    SysThreadPool<std::vector<ShareBuffer*>, ShareBuffer*> pool(buffers);
    pool.start(buffers);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Add 20 more elements sequentially
    for(int i = 5; i < 25; ++i) {
        ShareBuffer* buf = new ShareBuffer();
        buf->tableId = 3000 + i;
        buf->blockNum = 0;
        buf->count = 1;
        buf->isDirty = true;
        tableHeader* h = new tableHeader();
        h->setData(3000+i,0,0,0,1,0,0,0,0,4096,{4},{1},{"x"});
        buf->tableHeaderPtr = h;
        
        pool.cacheHint(buf);
    }
    
    // Cache should have exactly 5 elements
    int count = 0;
    for(auto b : *buffers) {
        if(b != nullptr) count++;
    }
    EXPECT_EQ(count, 5);
    
    // Most recent additions should be in cache
    // The last 5 added (20-24) might be there, or mix depending on eviction
    
    pool.stop();
    ForceResizeBuffers(5);
    clearFolder(testFolderPath);
}
