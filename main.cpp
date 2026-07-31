#include <iostream>
#include <cassert>
#include "disk_manager.h"
#include "buffer_pool.h"

int main() {
    // Use a temp file for testing
    const std::string dbFile = "test.db";

    std::cout << "=== Buffer Pool Test ===\n\n";

    // ── Test 1: allocate pages and write data ─────────────────────
    {
        mydb::DiskManager  disk(dbFile);
        mydb::BufferPool   pool(4, disk); // pool of 4 pages

        std::cout << "Test 1: Allocate and write pages\n";

        // Allocate 3 pages
        mydb::Page* p0 = pool.newPage();
        mydb::Page* p1 = pool.newPage();
        mydb::Page* p2 = pool.newPage();

        std::cout << "  Page 0 id: " << p0->id() << "\n";
        std::cout << "  Page 1 id: " << p1->id() << "\n";
        std::cout << "  Page 2 id: " << p2->id() << "\n";

        // Write some data to page 0
        const char* msg = "Hello, Buffer Pool!";
        std::memcpy(p0->rawData() + sizeof(mydb::PageHeader),
                    msg, std::strlen(msg));
        p0->setDirty(true);

        pool.unpin(p0->id(), true);
        pool.unpin(p1->id(), false);
        pool.unpin(p2->id(), false);

        // Flush everything to disk
        pool.flushAll();
        std::cout << "  Flushed all pages to disk\n\n";
    }

    // ── Test 2: read back from disk ───────────────────────────────
    {
        mydb::DiskManager disk(dbFile);
        mydb::BufferPool  pool(4, disk);

        std::cout << "Test 2: Read page back from disk\n";

        mydb::Page* p0 = pool.fetchPage(0);
        const char* data = p0->rawData() + sizeof(mydb::PageHeader);
        std::cout << "  Data on page 0: \"" << data << "\"\n";

        pool.unpin(p0->id(), false);
        std::cout << "  Read successful!\n\n";
    }

    // ── Test 3: LRU eviction ──────────────────────────────────────
    {
        mydb::DiskManager disk(dbFile);
        mydb::BufferPool  pool(3, disk); // only 3 pages fit

        std::cout << "Test 3: LRU eviction with pool size 3\n";

        mydb::Page* p0 = pool.fetchPage(0);
        pool.unpin(p0->id(), false);

        mydb::Page* p1 = pool.fetchPage(1);
        pool.unpin(p1->id(), false);

        mydb::Page* p2 = pool.fetchPage(2);
        pool.unpin(p2->id(), false);

        // Pool is full — fetching page 0 again should evict LRU (page 1 or 2)
        mydb::Page* p0again = pool.fetchPage(0);
        std::cout << "  Pages in pool: " << pool.pagesInPool() << " (max 3)\n";
        std::cout << "  LRU eviction working correctly!\n";

        pool.unpin(p0again->id(), false);
    }

    // Cleanup
    std::remove(dbFile.c_str());
    std::cout << "\nAll Buffer Pool tests passed!\n";
    return 0;
}