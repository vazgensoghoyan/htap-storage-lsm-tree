# HTAP Storage

**Русский** · [English](README-eng.md)

## Содержание

- [О проекте](#о-проекте)
- [Архитектура](#архитектура)
- [Сборка и тесты](#сборка-и-тесты)
- [Использование](#использование)
- [Ограничения](#ограничения)
- [Навигация](#навигация)

## О проекте

Экспериментальное HTAP-хранилище на C++23: LSM-дерево сочетает построчный формат для точечных чтений и записи с колоночным форматом для аналитических сканирований. HTAP — Hybrid Transactional/Analytical Processing.

Проект собирается в библиотеку `htap_storage`, а не в сервер или SQL-консоль. Внутри — собственные SSTable, SQL-парсер, семантический анализатор и исполнитель запросов; зависимости от RocksDB нет.

- Точечный поиск, диапазоны `[from, to)` и чтение выбранных колонок через курсоры.
- `CREATE TABLE`, `INSERT`, `SELECT`, `WHERE`, `GROUP BY`, `ORDER BY`, `LIMIT`.
- `COUNT(expr)` и числовые `SUM`, `AVG`, `MIN`, `MAX`; типы `INT64`, `DOUBLE`, `STRING` и nullable-колонки.
- Фоновый или синхронный flush/compaction, разреженные индексы, кеши метаданных и блоков, пропуск блоков по числовым min/max-статистикам.

Это исследовательский прототип, не СУБД с гарантиями ACID: см. [ограничения](#ограничения).

## Архитектура

```text
SQL → Lexer / Parser → Binder → Executor → IStorageEngine
                                              ├─ LSMStorageEngine
                                              └─ MockStorageEngine

Запись → MemTable → ImmutableMemTable → L0 (ROW)
                                         ↓ compaction
                                       L1 (ROW)
                                         ↓ compaction
                                       L2+ (COLUMN)
```

Показан формат уровней по умолчанию. При заполнении MemTable становится неизменяемой и сбрасывается в SSTable на L0. Compaction объединяет SSTable исходного уровня и записывает результат на следующий, меняя формат по настройке `row_to_column_level`. Чтение объединяет данные из памяти и с диска; для колоночных SSTable загружаются нужные колонки.

Основные настройки [`StorageConfig`](include/storage/api/config.hpp):

| Параметр | По умолчанию | Назначение |
| --- | --- | --- |
| `root_path` | `./htap_data` | Каталог данных |
| `memtable_threshold` | `10 000` | Порог заморозки MemTable по числу ключей |
| `level0_compaction_trigger` | `4` | Число SSTable на L0 для запуска compaction |
| `row_to_column_level` | `2` | Первый уровень с колоночным форматом при compaction |
| `sparse_index_step` | `0` | Адаптивный шаг индекса; положительное значение фиксирует шаг |
| `is_compaction_background` | `true`* | Flush/compaction в отдельном потоке |

\* Значение по умолчанию задаётся при сборке; его можно переопределить перед созданием хранилища.

## Сборка и тесты

Нужны **CMake 3.20+**, компилятор C++23 и стандартная библиотека с поддержкой `std::format`. При включённых тестах CMake скачивает GoogleTest **1.14.0**, поэтому первая конфигурация требует доступа к сети.

Из корня репозитория:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Опции CMake:

| Опция | По умолчанию | Назначение |
| --- | --- | --- |
| `BUILD_TESTS` | `ON` | Собирать тесты; `OFF` — только библиотеку |
| `ENABLE_WARNINGS` | `ON` | Предупреждения компилятора |
| `HTAP_ENABLE_LOGGING` | `OFF` | Логирование во время выполнения |
| `HTAP_BACKGROUND_COMPACTION` | `ON` | Режим flush/compaction по умолчанию |

Для синхронного режима добавьте `-DHTAP_BACKGROUND_COMPACTION=OFF` к команде конфигурации. [CI](.github/workflows/testing.yml) запускает тесты на Ubuntu в обоих режимах. Набор покрывает парсер, executor, память, SSTable, курсоры, кеши и сквозные сценарии flush/compaction/повторного открытия.

## Использование

Подключите репозиторий к CMake-проекту приложения с уже объявленной целью `your_app`:

```cmake
add_subdirectory(htap-storage-lsm-tree)
target_link_libraries(your_app PRIVATE htap_storage)
target_compile_features(your_app PRIVATE cxx_std_23)
```

Минимальный пример полного пути от SQL до результата:

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

Результат: `North: 30`. Каждый вызов обрабатывает один SQL-оператор. Пример читает данные из памяти и не демонстрирует их сохранение после завершения процесса.

Для прямого доступа используйте `LSMStorageEngine::insert`, `get` и `scan`. Результат чтения — `ICursor`; индексы колонок в `value()` относятся к исходной схеме, а не к позиции в проекции. Подробный синтаксис: [язык запросов](docs/query_language.md).

## Ограничения

- **Схема:** ровно один ключ `INT64 KEY`, обязательно первая колонка, без `NULL`.
- **Повторные ключи:** запись заменяется только внутри активной MemTable. Между MemTable и SSTable нет разрешения версий или общей дедупликации; используйте уникальные ключи.
- **Сохранность:** нет WAL и транзакций. Активная MemTable не сбрасывается при завершении работы. `wait_for_compaction(table)` ждёт обработки уже неизменяемых таблиц и compaction, но не сбрасывает активную MemTable и не является барьером надёжного сохранения.
- **Повторное открытие:** SSTable восстанавливаются из manifest после `create_table` с прежними именем и схемой в том же каталоге. Схемы автоматически не загружаются.
- **SQL:** нет `UPDATE`, `DELETE`, JOIN, подзапросов, `DISTINCT`, `HAVING`, алиасов и `COUNT(*)`; вместо последнего используйте `COUNT(id)`.

## Навигация

| Путь | Содержимое |
| --- | --- |
| [`include/`](include/) | Публичные заголовки и интерфейсы |
| [`src/lsmtree/`](src/lsmtree/) | MemTable, построение SSTable, manifest и compaction |
| [`src/storage/`](src/storage/) | Движки, курсоры, чтение, кеши и пропуск блоков |
| [`src/parser/`](src/parser/), [`src/executor/`](src/executor/) | Разбор, связывание и выполнение SQL |
| [`tests/`](tests/) | Модульные, интеграционные и сквозные тесты |
| [`docs/query_language.md`](docs/query_language.md) | Справочник синтаксиса SQL |
| [`docs/LSM-HTAP-article.pdf`](docs/LSM-HTAP-article.pdf) | Статья *Real-Time LSM-Trees for HTAP Workloads*: контекст гибридного хранения, не спецификация этой реализации |
