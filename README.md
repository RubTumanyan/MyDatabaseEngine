# MyDatabaseEngine
From-scratch relational database engine in C++17.
## Features
- SQL lexer and parser
- Query optimizer (logical and physical plans)
- Volcano-model executor
- B-Tree index
- Buffer pool with LRU eviction
- Write-ahead log (WAL)
- Transaction manager with rollback
- Lock manager
- MVCC snapshot visibility
## Tech Stack
- C++17, CMake, Google Test
## Build
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
./tests
```
## License
MIT
