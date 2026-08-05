# MyDatabaseEngine

A from-scratch relational database engine written in modern C++17. It implements the entire SQL query pipeline — lexer, parser, optimizer, and a Volcano-style executor — plus the storage and transaction machinery that sits underneath: paged file storage, a buffer pool with LRU eviction, a B-Tree index, a write-ahead log with crash recovery, a lock manager, and MVCC-style snapshot visibility.

Built as a learning project to understand how databases actually work under the hood, and as a portfolio showpiece of systems programming in C++.

## Table of contents

- [Features](#features)
- [How it works](#how-it-works)
  - [The big picture](#the-big-picture)
  - [Step by step: running a SELECT](#step-by-step-running-a-select)
  - [The lexer](#1-the-lexer)
  - [The parser and AST](#2-the-parser-and-ast)
  - [The optimizer: logical and physical plans](#3-the-optimizer-logical-and-physical-plans)
  - [The Volcano executor](#4-the-volcano-executor)
  - [Expression evaluation](#5-expression-evaluation)
  - [The write path (DML)](#6-the-write-path-dml)
  - [Storage engine: pages, disk manager, buffer pool](#7-storage-engine-pages-disk-manager-buffer-pool)
  - [The B-Tree index](#8-the-b-tree-index)
  - [Transactions: commit and rollback](#9-transactions-commit-and-rollback)
  - [The lock manager](#10-the-lock-manager)
  - [MVCC snapshot isolation](#11-mvcc-snapshot-isolation)
  - [Write-ahead logging and crash recovery](#12-write-ahead-logging-and-crash-recovery)
- [Maturity level](#maturity-level)
- [Repository layout](#repository-layout)
- [Getting started](#getting-started)
- [Roadmap](#roadmap)
- [License](#license)

## Features

**Query pipeline**
- Hand-written SQL lexer (tokenizer) with line/column tracking and comment support
- Recursive-descent parser with a precedence-climbing expression grammar, covering `SELECT`, `INSERT`, `UPDATE`, `DELETE`, `CREATE TABLE`, and `DROP TABLE`
- Two-pass query optimizer producing logical and physical plans (filter pushdown, projection, sort, limit, hash join)
- Volcano-model iterator executor (`open` / `next` / `close`)

**Storage & indexing**
- Row-oriented heap tables (`TableStorage`), with a `TableHeap` API for page-organized tables
- Fixed-size 4 KB pages with headers (free space, record count, checksum)
- Disk manager for paged file I/O
- Buffer pool with LRU eviction, pinning, and dirty-page flushing
- B-Tree index (order 4) with exact lookup, range scans, and leaf-node linked lists

**Transactions & concurrency**
- Transaction manager with `begin` / `commit` / `rollback`
- Per-transaction write log used to undo changes on rollback
- Write-ahead log (WAL) with LSN chaining, commit flushing, and crash recovery (redo committed / undo aborted)
- Lock manager with shared and exclusive modes
- MVCC-style snapshot visibility (`createdBy` / `deletedBy` version metadata)

## How it works

### The big picture

A query does not go directly to the data. It passes through five layers, each transforming the query into a more concrete representation:

```
SQL text
   │
   ▼
┌──────────┐   ┌──────────┐   ┌──────────────┐   ┌───────────────┐   ┌────────────────┐
│  Lexer   │──▶│  Parser  │──▶│  Optimizer   │──▶│   Executor    │──▶│  Storage /     │
│  tokens  │   │   AST    │   │ logical plan │   │ physical plan │   │  Index / WAL   │
└──────────┘   └──────────┘   └──────────────┘   └───────────────┘   └────────────────┘
                                                      │                    ▲
                                                      └────▶ operators ────┘
                                                      (Volcano iteration)

Storage and concurrency subsystems (buffer pool, B-Tree,
transaction manager, lock manager, MVCC, WAL) sit below
the executor and back every operator.
```

1. **Lexer** — converts the raw SQL *string* into a flat list of *tokens*.
2. **Parser** — converts *tokens* into an *abstract syntax tree* (AST) that represents the query's structure.
3. **Optimizer** — converts the *AST* into a *logical plan* (relational algebra: *what* to do), then into a *physical plan* (concrete algorithms: *how* to do it).
4. **Executor** — builds a tree of *operators* from the physical plan and *pulls* rows through it using the Volcano iteration model.
5. **Storage** — operators read and write rows through the storage engine, while the transaction subsystem guarantees isolation and durability.

### Step by step: running a SELECT

Consider the query:

```sql
SELECT id, name FROM users WHERE age >= 30 LIMIT 3;
```

Here is what happens inside the engine:

**Step 1 — Lexing.** The query string is scanned character by character. Identifiers (`users`, `age`), keywords (`SELECT`, `FROM`, `WHERE`, `LIMIT`), numbers (`30`, `3`), and punctuation (`,`, `>=`) become tokens. The output is roughly:

```text
SELECT   IDENTIFIER "id"   COMMA   IDENTIFIER "name"
FROM     IDENTIFIER "users"
WHERE    IDENTIFIER "age"   GTE   NUMBER "30"
LIMIT    NUMBER "3"   END_OF_FILE
```

**Step 2 — Parsing.** The parser walks the token list with a recursive-descent grammar. It recognizes the `SELECT` keyword and then parses each clause: the projection list, the `FROM` table, the `WHERE` expression (using an operator-precedence ladder), and the `LIMIT` count. The result is a `SelectStatement` AST node:

```text
SelectStatement
├── columns = ["id", "name"]
├── table   = "users"
├── where   = BinaryExpr(GTE, ColumnRef("age"), Literal(30))
└── limit   = 3
```

**Step 3 — Optimization.** The optimizer runs two passes. *Pass 1* lowers the AST into a logical plan, built bottom-up so cheap operations run first:

```text
LogicalLimit (count = 3)
└── LogicalProjection (columns = id, name)
    └── LogicalFilter  (predicate = age >= 30)
        └── LogicalScan (table = users)
```

Notice that the **filter is placed below the projection** — rows that fail `WHERE` are discarded before any columns are projected, so no work is wasted projecting rows nobody wants.

*Pass 2* lowers the logical plan into a physical plan by choosing a concrete algorithm for each logical node. The engine currently has exactly one algorithm per node type, so this pass is mostly a 1:1 mapping:

```text
PhysicalLimit
└── PhysicalProjection
    └── PhysicalFilter
        └── PhysicalSeqScan (table = users)
```

**Step 4 — Execution.** The executor turns the physical plan into a tree of operator objects and *pulls* rows through it. `SeqScan` emits every row of `users`; `Filter` keeps only rows where `age >= 30`; `Projection` keeps only the `id` and `name` columns; `Limit` stops pulling after 3 rows. The executor collects the output into a `std::vector<Row>` and returns it.

**Step 5 — Result.** The caller receives 3 rows with exactly two columns each.

### 1. The lexer

`Lexer` (`include/lexer.h`) is a classic hand-written scanner. It holds the source string plus `pos_`, `line_`, and `col_` cursors and exposes low-level navigation helpers:

- `peek()` — look at the current character without consuming it
- `peekNext()` — look one character ahead (needed for two-character operators like `>=` and `!=`)
- `advance()` — consume the current character and return it

It skips whitespace and `--` line comments, then recognizes four kinds of lexemes:

| Reader | Produces | Examples |
|---|---|---|
| `readString()` | `STRING` tokens | `'hello world'` |
| `readNumber()` | `NUMBER` tokens | `42`, `3.14` |
| `readIdentifierOrKeyword()` | `IDENTIFIER` or a keyword | `users`, `SELECT`, `FROM` |
| symbol handling | operators + punctuation | `=`, `!=`, `<`, `>=`, `,`, `(` |

`classifyWord()` uppercases each identifier and maps it to a keyword `TokenType` when it matches one (e.g. `select` → `SELECT`); otherwise it stays an `IDENTIFIER`. Every token carries its `line` and `col`, so later errors can point back at the exact spot in the query.

### 2. The parser and AST

`Parser` (`include/parser.h`) is a **recursive-descent parser** over the token stream. The grammar is split into layers:

- **Statements** — `SELECT`, `INSERT`, `UPDATE`, `DELETE`, `CREATE TABLE`, `DROP TABLE`
- **Clauses** — projection list, `WHERE`, `ORDER BY`, `LIMIT`, column definitions
- **Expressions** — a precedence ladder: `parseExpression` (AND/OR) → `parseComparison` (`= != < > <= >=`) → `parseUnary` (`NOT`, unary `-`) → `parsePrimary` (literal, column reference, parenthesized expression)

Two helpers drive the parser:

- `expect(type, msg)` — consume the token if it matches, otherwise throw a `ParseError` with the message. This is how syntax errors get reported.
- `match(type)` — consume the token if it matches and return `true`; otherwise return `false` without consuming. This makes optional clauses like `LIMIT` easy to parse.

The AST (`include/ast.h`) is a set of plain structs glued together with `std::variant`. Expressions are one of:

```cpp
using Expr = std::variant<
    std::shared_ptr<LiteralExpr>,    // 42, 3.14, 'hello', true/false
    std::shared_ptr<ColumnRefExpr>,  // "age" or "users.age"
    std::shared_ptr<BinaryExpr>,     // age >= 30,  x AND y
    std::shared_ptr<UnaryExpr>>;     // NOT x,  -x
```

Statements mirror the grammar one-to-one — `SelectStatement`, `InsertStatement`, `UpdateStatement`, `DeleteStatement`, `CreateTableStatement`, `DropTableStatement` — each holding only the fields that clause actually uses (e.g. `SelectStatement` has `columns`, `table`, optional `where`, `orderBy`, and optional `limit`).

### 3. The optimizer: logical and physical plans

`Optimizer` (`include/optimizer.h`) separates *what* to compute from *how* to compute it — the same idea used by real DBMSes.

**Pass 1 — AST → logical plan.** A logical plan is a tree of relational-algebra nodes (`Scan`, `Filter`, `Projection`, `Sort`, `Limit`, `Join`). For `SELECT`, the plan is assembled bottom-up so the cheapest operations execute first and the plan is built to a fixed shape:

```
Scan → Filter → Projection → Sort → Limit
```

Two deliberate optimizations are visible in `buildSelect`:

- **Filter before projection** — predicates are applied while rows are still wide, so columns are only projected for rows that survive.
- **Limit at the top** — `Limit` sits at the root of the tree and stops pulling rows from its child once its count is reached. Because the executor is pull-based, this means the engine *never scans the whole table* for a `LIMIT` query — it scans just enough rows to satisfy the limit.

For `DELETE` and `UPDATE`, the filter is pushed down directly above the scan so non-matching rows are skipped as early as possible.

**Pass 2 — logical → physical plan.** Each logical node is mapped to a concrete algorithm:

| Logical node | Physical node | Algorithm |
|---|---|---|
| `Scan` | `SeqScan` | Full table scan, O(n) |
| `Filter` | `Filter` | Row-by-row predicate evaluation |
| `Projection` | `Projection` | Column subsetting |
| `Sort` | `Sort` | In-memory sort, O(n log n) |
| `Limit` | `Limit` | Stops pulling after n rows |
| `Join` | `HashJoin` | Hash table on build side, O(n + m) |

In a production database, pass 2 consults table statistics and a cost model to choose between e.g. `SeqScan` and an index scan, or between hash join and merge join. This engine keeps it deterministic — one physical algorithm per logical node — and the code comments mark exactly where a cost-based decision would plug in.

### 4. The Volcano executor

`Executor` (`include/executor.h`, `include/operator.h`) implements the **Volcano iteration model**. Every operator inherits from a single abstract interface:

```cpp
class Operator {
public:
    virtual void open()  = 0;                                   // initialize
    virtual std::optional<Row> next() = 0;                      // next row, or nullopt
    virtual void close() = 0;                                   // release resources
};
```

`next()` returns one row at a time as a `std::optional<Row>` — `std::nullopt` signals "no more rows". The caller (usually another operator) *pulls* rows, so nothing is computed until it is actually needed. This is what makes `LIMIT` cheap: the limit operator simply stops calling `next()` on its child.

The built-in operators are deliberately small, each doing one thing:

| Operator | Behavior |
|---|---|
| `SeqScan` | Iterates a table, returning each row in turn; when given an `MvccSnapshot`, only returns rows that are *visible* to it |
| `Filter` | Pulls from its child, evaluates the `WHERE` predicate on each row, and silently drops non-matching rows |
| `Projection` | Pulls a row, keeps only the requested columns (`*` passes the row through untouched) |
| `Sort` | On `open()`, pulls and buffers *all* rows from its child, sorts them by the `ORDER BY` clauses, then serves them one at a time |
| `Limit` | Counts rows pulled from its child and returns `nullopt` once the limit is reached |
| `HashJoin` | On `open()`, drains its *build* side into a hash table keyed by the join key; then for each *probe* row, looks up matching rows and emits the concatenated result |

`Executor::execute()` ties it together: it builds the operator tree from the physical plan, calls `open()`, then repeatedly calls `next()` until the pipeline is exhausted, collecting the rows, and finally calls `close()`.

### 5. Expression evaluation

Predicates are evaluated by three cooperating functions in `src/executor.cpp`:

- **`resolveExpr`** — reduces an expression node to a concrete string given a row. Literals return their stored text; column references look up the value in the row; compound expressions return empty.
- **`compareValues`** — compares two string values under a SQL operator. It first tries to parse both sides as numbers with `std::stod`; if that works it compares numerically, otherwise it falls back to plain string comparison. This is why `WHERE age >= 30` works numerically even though everything is stored as text.
- **`evaluateExpr`** — recursively evaluates a predicate to `true`/`false`. `AND` and `OR` short-circuit; `NOT` inverts its operand; comparison operators delegate to `compareValues`.

### 6. The write path (DML)

Writes are handled directly by the executor rather than as operator trees:

- **`executeInsert`** — builds a `Row` from the column list and literal values, appends it to the table, and (if inside a transaction) logs a `WriteRecord`.
- **`executeUpdate`** — scans the table, evaluates the `WHERE` predicate per row, applies the `SET` assignments to matching rows, and logs an update `WriteRecord` carrying the *old* and *new* row.
- **`executeDelete`** — uses `std::remove_if` to erase rows matching the `WHERE` predicate, logging each deletion with the old row.
- **`executeCreateTable`** — registers an empty table, throwing if it already exists.

Crucially, **every DML operation records enough information to undo itself**. `WriteRecord` stores `oldRow` and `newRow`, so a rollback can restore the exact previous state:

```cpp
struct WriteRecord {
    enum class Type { Insert, Update, Delete };
    Type        type;
    std::string table;
    std::string key;      // primary key, typically "id"
    Row         oldRow;
    Row         newRow;
};
```

### 7. Storage engine: pages, disk manager, buffer pool

The storage layer is built on the classic **page → buffer pool → disk** hierarchy.

**Pages.** The fundamental unit of storage is a fixed-size 4 KB block (`include/page.h`). Every page starts with a `PageHeader` holding its id, free space, record count, and a checksum. A page tracks two runtime flags: `dirty` (modified in memory, must be written back) and `pinCount` (how many components are currently using it).

**Disk manager.** `DiskManager` (`include/disk_manager.h`) is a thin layer over raw file I/O. A database file is just a sequence of `PAGE_SIZE`-byte blocks; the disk manager translates a `PageId` into a byte offset (`id * PAGE_SIZE`) and reads or writes whole pages. `allocatePage()` appends a new block to the end of the file.

**Buffer pool.** `BufferPool` (`include/buffer_pool.h`) caches frequently used pages in memory and is the engine's answer to the age-old problem of disk being orders of magnitude slower than RAM. Its eviction policy is **LRU (least recently used)**, implemented with two data structures:

- a `std::unordered_map<PageId, std::unique_ptr<Page>>` — the page cache
- a doubly-linked list (`std::list<PageId>`) where the **front is the most recently used** page and the **back is the least recently used**

Every access calls `touchPage()`, which moves the page to the front of the list in O(1) using a cached iterator. When the pool is full and a new page must be loaded, `evict()` takes from the back of the list — but only if the page is *unpinned*; a pinned page (one an operator is actively using) can never be evicted. Dirty pages are written back to disk on eviction, and `flushAll()` forces all dirty pages out, which is what a checkpoint would do in a real system.

A `TableHeap` (`include/table_heap.h`) builds on pages: it inserts rows into pages as serialized byte strings, returning a `RowId` (page id + slot index), and supports point reads, updates, deletes, and scans with optional MVCC filtering.

### 8. The B-Tree index

`BTree` (`include/btree.h`) implements a classic B-Tree with order 4 (max 4 keys per node). Each node is stored on its own page and serialized to/from raw page bytes via `serialize`/`deserialize`:

```cpp
struct BTreeNode {
    bool     isLeaf;                        // leaf or internal?
    int      keyCount;                      // how many keys are stored
    std::string keys[ORDER + 1];            // the keys
    PageId   children[ORDER + 2];           // child page ids (internal nodes)
    std::string values[ORDER + 1];          // values at the leaf (row ids)
    PageId   nextLeaf;                      // pointer to the next leaf page
};
```

The leaf nodes form a **linked list** (`nextLeaf`), which makes range scans trivial: find the first leaf page that could hold the lower bound, then walk the leaf chain. The three public operations are:

- **`insert(key, value)`** — descends to the correct leaf (`findLeaf`), inserts, and if the node is full calls `splitNode`. Splitting climbs back up recursively (`insertRec`), returning a `SplitResult` (the split key and the new page) so the parent can be adjusted — the standard B-Tree insertion algorithm.
- **`search(key)`** — descends the tree following the key comparisons and returns the value stored at the leaf, or `std::nullopt`.
- **`rangeScan(from, to)`** — collects all `(key, value)` pairs in `[from, to]` by walking leaves.

The index is fully functional and wired into the storage layer, but the optimizer currently always chooses a `SeqScan` — hooking the index into `WHERE` planning is the natural next step.

### 9. Transactions: commit and rollback

`TransactionManager` (`include/txn_manager.h`) is the heart of atomicity. A `Transaction` is just an id, a status (`Active` / `Committed` / `Aborted`), and a growing write log.

- **`begin()`** — allocates the next transaction id, registers the transaction, and if a WAL is attached appends a `Begin` record.
- **`commit()`** — marks the transaction committed and releases all its locks. With a WAL attached, it first appends every buffered write to the log (as `Insert`/`Update`/`Delete` records), then a `Commit` record, then calls `wal->flush()` to force everything to durable storage *before* acknowledging the commit.
- **`rollback()`** — undoes the transaction by replaying its write log **in reverse order**:
  - an `Insert` is undone by removing the row with the matching key
  - an `Update` or `Delete` is undone by restoring the recorded `oldRow`
  - then it appends an `Abort` record to the WAL and releases all locks

Undoing in reverse order is essential: it guarantees that earlier writes are not clobbered by later ones during the unwind, mirroring how real undo logs work.

### 10. The lock manager

`LockManager` (`include/lock_manager.h`) provides **shared** and **exclusive** locks on arbitrary named resources (in practice, string keys like `"users:1"`).

The rules are the standard two-mode table:

- **Shared** locks are compatible with other **shared** locks — many readers can hold a resource at once.
- **Exclusive** locks conflict with *everything* — a writer gets the resource all to itself.

Internally, each resource maps to a `std::vector<LockEntry>` (holder id + mode), guarded by a `std::mutex`. `acquireLock` grants a shared lock only if every existing holder also holds shared; it grants an exclusive lock only if the resource is completely free. `releaseAll(txnId)` is called on commit and rollback to drop every lock a transaction holds — deadlock handling via waiting/timeouts is a future extension.

This is the isolation layer beneath transactions: locking controls *which transactions may touch the same data concurrently*, and it combines with MVCC (below) to provide the engine's concurrency guarantees.

### 11. MVCC snapshot isolation

The engine's **multi-version concurrency control** (`include/mvcc.h`) gives every transaction a consistent view of the database as of the moment it began — the classic *snapshot isolation* guarantee. Readers never block writers and writers never block readers.

Each version of a row carries two transaction ids:

```cpp
struct VersionedRow {
    Row   data;
    TxnId createdBy;   // which transaction created this version
    TxnId deletedBy;   // 0 = not deleted
};
```

A snapshot holds the id of the transaction that took it, and a version is **visible** to that snapshot only if:

```cpp
bool isVisible(const VersionedRow& v) const {
    bool created   = v.createdBy <= snapshotId_;
    bool notDeleted = (v.deletedBy == 0) || (v.deletedBy > snapshotId_);
    return created && notDeleted;
}
```

That is: the version was created by a transaction that committed *before* the snapshot (or is the snapshot itself), and either has not been deleted, or was deleted by a transaction that started *after* the snapshot. `SeqScan` already honors snapshots — when an executor is given an `MvccSnapshot`, the scan silently skips rows that are not visible to it. True multi-versioned storage (retaining all row versions on disk) is the remaining piece.

### 12. Write-ahead logging and crash recovery

`WAL` (`include/wal.h`) implements the engine's **durability** guarantee: *never write data pages before you have logged the change that produced them.*

Every change is described by a `LogRecord`:

```cpp
struct LogRecord {
    enum class Type : uint8_t {
        Begin, Commit, Abort,   // transaction boundaries
        Update, Insert, Delete   // data changes
    };
    LSN         lsn;       // unique log sequence number
    LSN         prevLsn;   // previous record of the SAME transaction (chain)
    TxnId       txnId;
    std::string table, key;
    std::string oldValue;  // serialized old row (for undo)
    std::string newValue;  // serialized new row (for redo)
};
```

Two details are worth calling out:

- **LSN chaining.** Each record stores `prevLsn`, the LSN of the previous record written by the same transaction. This lets recovery walk each transaction's log chain forward or backward.
- **Both old and new values.** Every data change is logged with its before-image (`oldValue`) and after-image (`newValue`), so recovery can **redo** committed work and **undo** in-flight work.

On commit, the transaction manager appends all of its write records followed by a `Commit` record, then calls `flush()` to force the log to disk *before* the commit is acknowledged. If the process crashes, `recover()` scans the entire log and applies the standard algorithm:

1. **Redo** — replay the changes of every transaction that reached `Commit`.
2. **Undo** — revert the changes of every transaction that never committed (only `Begin`/data records, no `Commit`).

Rows are serialized to plain text (`"col1=val1;col2=val2"`) so log records are human-inspectable.

## Maturity level

| Subsystem | Status |
|---|---|
| Lexer / Parser / AST | Working |
| Optimizer (logical + physical plans) | Working (single algorithm per node) |
| Volcano executor + operators | Working |
| DML (insert/update/delete) | Working |
| In-memory tables | Working |
| Buffer pool + disk manager + pages | Working |
| B-Tree index | Working (not yet wired into the optimizer) |
| Table heap on pages | Working |
| Transactions (begin/commit/rollback) | Working |
| Lock manager | Working |
| MVCC snapshot visibility | Partially wired (scan filtering present; version retention pending) |
| WAL + crash recovery | Working |

The engine is a teaching and portfolio project: each subsystem is real and functional, but several production concerns — cost-based optimization, on-disk versioned rows, full `ORDER BY`/`JOIN` through the SQL front end, and a client protocol — are deliberately left for the roadmap.

## Repository layout

```
include/       Public headers (AST, plans, operators, storage, txn)
src/           Implementation (lexer, parser, optimizer, executor, storage…)
TESTS/         Google Test suite
main.cpp       Demo driver exercising the full pipeline
CMakeLists.txt Build configuration
```

## Getting started

### Prerequisites

- CMake 3.20+
- A C++17 compiler (MSVC, GCC, or Clang)
- Internet access on first configure (Google Test is fetched automatically via `FetchContent`)

### Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

This produces two targets:

- `mydb` — the demo executable
- `tests` — the Google Test binary

### Run the demo

```bash
./build/mydb
```

The demo drives the full pipeline end to end. It evaluates SQL against an in-memory `users` table, exercises transaction commit and rollback, demonstrates B-Tree-style point and range lookups, and shows the lock manager granting and denying shared/exclusive locks:

```text
=== Full SQL Pipeline ===

SQL: SELECT id, name FROM users WHERE age >= 30;
Results (3 rows):
  id=1 name=Alice
  id=3 name=Carol
  id=5 name=Eve
```

### Run the tests

```bash
ctest --test-dir build --output-on-failure
```

Or run the test binary directly:

```bash
./build/tests
```

The suite covers the lexer, parser, optimizer, executor, transactions, and B-Tree.

## Roadmap

- [ ] Wire the B-Tree index into the optimizer so `WHERE key = …` becomes an index scan
- [ ] Durable heap-file persistence for `TableStorage` backed by the buffer pool
- [ ] Recovery driven by WAL for on-disk tables (not just in-memory)
- [ ] `INSERT` / `UPDATE` / `DELETE` through the SQL front end (AST → executor)
- [ ] Cost model and join-order selection in the optimizer
- [ ] Full MVCC: retain all row versions and garbage-collect old ones
- [ ] Lock wait queues and deadlock detection
- [ ] Columnar access paths and vectorized execution
- [ ] Network protocol and a client driver

## License

[MIT](LICENSE)
