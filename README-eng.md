# HTAP Storage

[Русский](README.md) · **English**

## Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Build and test](#build-and-test)
- [Usage](#usage)
- [Limitations](#limitations)
- [Project map](#project-map)

## Overview

An experimental C++23 HTAP storage engine: an LSM tree combines row-oriented storage for point reads and writes with column-oriented storage for analytical scans. HTAP stands for Hybrid Transactional/Analytical Processing.

The project builds the `htap_storage` library, not a server or SQL shell. It implements its own SSTables, SQL parser, semantic binder, and query executor; it does not depend on RocksDB.

- Point lookups, `[from, to)` range scans, and column projections through cursors.
- `CREATE TABLE`, `INSERT`, `SELECT`, `WHERE`, `GROUP BY`, `ORDER BY`, `LIMIT`.
- `COUNT(expr)` and numeric `SUM`, `AVG`, `MIN`, `MAX`; `INT64`, `DOUBLE`, `STRING`, and nullable columns.
- Background or synchronous flush/compaction, sparse indexes, metadata and block caches, and numeric min/max block pruning.

This is a research prototype, not an ACID database: see [limitations](#limitations).

## Architecture

```text
SQL → Lexer / Parser → Binder → Executor → IStorageEngine
                                              ├─ LSMStorageEngine
                                              └─ MockStorageEngine

Write → MemTable → ImmutableMemTable → L0 (ROW)
                                        ↓ compaction
                                      L1 (ROW)
                                        ↓ compaction
                                      L2+ (COLUMN)
```

The diagram shows the default layouts. Once full, a MemTable becomes immutable and is flushed into an L0 SSTable. Compaction merges the source level's SSTables into the next level, choosing its layout via `row_to_column_level`. Reads combine in-memory and on-disk data; column SSTables load the required columns.

Key [`StorageConfig`](include/storage/api/config.hpp) settings:

| Setting | Default | Purpose |
| --- | --- | --- |
| `root_path` | `./htap_data` | Data directory |
| `memtable_threshold` | `10,000` | Key count that triggers MemTable freezing |
| `level0_compaction_trigger` | `4` | L0 SSTable count that triggers compaction |
| `row_to_column_level` | `2` | First column-oriented compaction output level |
| `sparse_index_step` | `0` | Adaptive index stride; a positive value fixes the stride |
| `is_compaction_background` | `true`* | Flush/compaction on a worker thread |

\* The default is set at build time and can be overridden before constructing the engine.

## Build and test

Requires **CMake 3.20+**, a C++23 compiler, and a standard library supporting `std::format`. With tests enabled, CMake downloads GoogleTest **1.14.0**, so the first configuration needs network access.

From the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

CMake options:

| Option | Default | Purpose |
| --- | --- | --- |
| `BUILD_TESTS` | `ON` | Build tests; use `OFF` for the library only |
| `ENABLE_WARNINGS` | `ON` | Compiler warnings |
| `HTAP_ENABLE_LOGGING` | `OFF` | Runtime logging |
| `HTAP_BACKGROUND_COMPACTION` | `ON` | Default flush/compaction mode |

For synchronous operation, add `-DHTAP_BACKGROUND_COMPACTION=OFF` to the configure command. [CI](.github/workflows/testing.yml) tests both modes on Ubuntu. Coverage includes the parser, executor, memory layer, SSTables, cursors, caches, and end-to-end flush/compaction/reopen scenarios.

## Usage

Add the repository to an application's CMake project with an existing `your_app` target:

```cmake
add_subdirectory(htap-storage-lsm-tree)
target_link_libraries(your_app PRIVATE htap_storage)
target_compile_features(your_app PRIVATE cxx_std_23)
```

A minimal example of the complete SQL-to-result path:

```cpp
#include <cstdint>
#include <iostream>
#include <string>
#include <variant>

#include "executor/binder.hpp"
#include "executor/executor.hpp"
#include "parser/parser.hpp"
#include "storage/lsm/lsm_storage_engine.hpp"

int main() {
    htap::storage::StorageConfig config;
    config.root_path = "./example_data";
    htap::storage::LSMStorageEngine storage(config);
    htap::executor::Binder binder(storage);
    htap::executor::Executor executor(storage);

    auto run = [&](const std::string& sql) {
        htap::parser::Parser parser{htap::parser::Lexer{sql}};
        auto statement = parser.ParseStatement();
        auto bound = binder.Bind(*statement);
        return executor.Execute(*bound);
    };

    run("CREATE TABLE orders (id INT64 KEY, region STRING, amount DOUBLE);");
    run("INSERT INTO orders (id, region, amount) VALUES (1, 'North', 10.0);");
    run("INSERT INTO orders (id, region, amount) VALUES (2, 'North', 20.0);");
    auto result = std::get<htap::executor::SelectResult>(run(
        "SELECT region, SUM(amount) FROM orders GROUP BY region;"));

    std::cout << std::get<std::string>(*result.rows.at(0).at(0)) << ": "
              << std::get<double>(*result.rows.at(0).at(1)) << '\n';
}
```

Output: `North: 30`. Each call handles one SQL statement. This example reads in-memory data; it does not demonstrate persistence after process exit.

For direct access, use `LSMStorageEngine::insert`, `get`, and `scan`. Reads return an `ICursor`; column indexes passed to `value()` refer to the original schema, not positions within the projection. See the [query language reference](docs/query_language.md) for detailed syntax.

## Limitations

- **Schema:** exactly one non-null `INT64 KEY`, which must be the first column.
- **Duplicate keys:** replacement occurs only within the active MemTable. There is no cross-MemTable/SSTable version resolution or global deduplication; use unique keys.
- **Durability:** no WAL or transactions. The active MemTable is not flushed on shutdown. `wait_for_compaction(table)` drains existing immutable tables and compaction work; it neither flushes the active MemTable nor acts as a durability barrier.
- **Reopening:** SSTables are recovered from the manifest after calling `create_table` with the original name and schema under the same data root. Schemas are not loaded automatically.
- **SQL:** no `UPDATE`, `DELETE`, joins, subqueries, `DISTINCT`, `HAVING`, aliases, or `COUNT(*)`; use `COUNT(id)` instead of the latter.

## Project map

| Path | Contents |
| --- | --- |
| [`include/`](include/) | Public headers and interfaces |
| [`src/lsmtree/`](src/lsmtree/) | MemTables, SSTable builders, manifests, and compaction |
| [`src/storage/`](src/storage/) | Engines, cursors, reads, caches, and block pruning |
| [`src/parser/`](src/parser/), [`src/executor/`](src/executor/) | SQL parsing, binding, and execution |
| [`tests/`](tests/) | Unit, integration, and end-to-end tests |
| [`docs/query_language.md`](docs/query_language.md) | SQL syntax reference |
| [`docs/LSM-HTAP-article.pdf`](docs/LSM-HTAP-article.pdf) | *Real-Time LSM-Trees for HTAP Workloads*: background on hybrid layouts, not a specification of this implementation |
