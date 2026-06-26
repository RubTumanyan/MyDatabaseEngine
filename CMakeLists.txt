cmake_minimum_required(VERSION 3.20)

project(MyDatabaseEngine VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Prevent FetchContent warning (IMPORTANT) ──
cmake_policy(SET CMP0135 NEW)

# ── GoogleTest ─────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(googletest)

# ── Core engine library ─────────────────────────────────────
add_library(mydb_engine
        src/lexer.cpp
        src/parser.cpp
        src/optimizer.cpp
        src/executor.cpp
        src/transaction.cpp
        src/lock_manager.cpp
        src/buffer_pool.cpp
        src/table_heap.cpp
        src/btree.cpp
        src/wal.cpp
        src/disk_manager.cpp
)

target_include_directories(mydb_engine PUBLIC include)

# ── Main executable ─────────────────────────────────────────
add_executable(mydb main.cpp)
target_link_libraries(mydb PRIVATE mydb_engine)

# ── Tests ────────────────────────────────────────────────────
enable_testing()

add_executable(tests
        tests/test_lexer.cpp
        tests/test_parser.cpp
        tests/test_optimizer.cpp
        tests/test_executor.cpp
        tests/test_transaction.cpp
        tests/test_btree.cpp
)

target_link_libraries(tests PRIVATE mydb_engine GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(tests)