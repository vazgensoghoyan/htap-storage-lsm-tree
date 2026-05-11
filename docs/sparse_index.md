# RFC: Sparse Index для SSTable

## Цель

Добавить sparse index по `Key`, чтобы ускорить выбор SSTable-блоков в `scan` и `get`.

Sparse index должен:

- строиться на этапе записи SSTable после построения block metadata;
- сохраняться в SSTable-файл;
- загружаться вместе с metadata SSTable в память;
- использоваться selector-ами на read-path;
- не менять API cursor-ов.

## Проблема

Сейчас выбор блоков устроен линейно: selector проходит всю metadata SSTable и выбирает блоки, диапазон ключей которых пересекается с диапазоном запроса.

При большом количестве блоков стоимость выбора становится `O(number_of_blocks)` для каждого `scan` или `get`.

Sparse index нужен, чтобы быстро найти примерное место в block metadata, с которого надо начинать поиск.

Min/max по числовым колонкам - отдельная оптимизация. Она может применяться позже после выбора candidate blocks по key range.

## Основная идея

SSTable отсортирована по ключу. Значит, логические блоки внутри SSTable тоже упорядочены по ключам:

```text
block 0: [0, 100)
block 1: [100, 200)
block 2: [200, 300)
block 3: [300, 400)
...
```

Sparse index хранит запись для каждого `K`-го логического блока:

```text
{ min_key = 0,   block_id = 0 }
{ min_key = 400, block_id = 4 }
{ min_key = 800, block_id = 8 }
...
```

Для запроса `[650, 950)` read-path делает:

1. бинарный поиск по sparse index;
2. находит ближайшую index entry с `min_key <= 650`;
3. получает примерный стартовый `block_id`;
4. линейно досматривает block metadata от этого места;
5. выбирает точные blocks, пересекающиеся с `[650, 950)`.

Sparse index не заменяет block metadata. Он только ускоряет поиск стартовой позиции.

## Logical block

Sparse index строится по логическим блокам.

### Row layout

Для row layout:

```text
logical block = RowBlockMeta
```

То есть один row block соответствует одному logical block.

### Column layout

Для column layout:

```text
logical block = группа ColumnBlockMeta с одинаковым block_id
```

Например, один logical block может выглядеть так:

```text
logical block with block_id = 5

key column:
    ColumnBlockMeta(block_id = 5, column_idx = 0)

value columns:
    ColumnBlockMeta(block_id = 5, column_idx = 1)
    ColumnBlockMeta(block_id = 5, column_idx = 2)
    ColumnBlockMeta(block_id = 5, column_idx = 3)
```

Sparse index строится по `block_id`, а не по каждому физическому column block-у.

## SparseIndexEntry

Общий тип для write-path и read-path:

```cpp
struct SparseIndexEntry {
    Key min_key;
    std::uint32_t block_id;
};
```

Поля:

- `min_key` - минимальный ключ логического блока;
- `block_id` - id этого логического блока.

Sparse index не хранит `offset` и `size`, потому что эти данные уже есть в `RowBlockMeta` / `ColumnBlockMeta`.

## Контракт с write-path

### Sparse index строится после block metadata

Write-path сначала строит data blocks и block metadata, а затем вызывает `SparseIndexBuilder`.

Предлагаемый API:

```cpp
class SparseIndexBuilder {
public:
    static std::vector<SparseIndexEntry> build_from_row_blocks(
        const std::vector<RowBlockMeta>& blocks,
        std::uint32_t step
    );

    static std::vector<SparseIndexEntry> build_from_column_blocks(
        const std::vector<ColumnBlockMeta>& blocks,
        std::uint32_t step
    );
};
```

`SparseIndexBuilder`:

- не пишет файл;
- не знает про footer;
- не знает про уровни LSM;
- не выбирает `K`;
- только строит `SparseIndexEntry[]`.

Ответственность write-path:

1. построить data blocks;
2. построить block metadata;
3. выбрать `K`;
4. вызвать `SparseIndexBuilder`;
5. записать sparse index в SSTable-файл;
6. передать sparse index в metadata SSTable, которая будет доступна read-path.

### Выбор K вынесен отдельно

Выбор `K` не должен быть зашит внутри writer-а.

Предлагаемый API:

```cpp
class SparseIndexPolicy {
public:
    std::uint32_t choose_step(
        std::size_t level_logical_blocks,
        std::size_t base_level_logical_blocks,
        std::size_t sstable_logical_blocks,
        double read_weight
    ) const;
};
```

`SparseIndexPolicy` можно менять независимо от writer-а и reader-а.

## Выбор K

`K` - это шаг sparse index.

Если `K = 1`, index содержит запись на каждый логический блок.

Если `K = 4`, index содержит запись на каждый четвёртый логический блок.

```text
маленький K:
    + точнее поиск
    + меньше досмотр metadata
    - больше index

большой K:
    + меньше index
    - больше досмотр metadata
```

Если оптимизировать только один отдельный lookup и не учитывать память, лучший вариант - `K = 1`. Но индекс должен оставаться достаточно компактным, чтобы храниться в памяти вместе с block metadata и не вытеснять другие структуры.

### Модель стоимости

Для SSTable с `N` logical blocks:

```text
index_size_cost(K) ~ N / K
lookup_extra_cost(K) ~ K
```

Но эти две стоимости имеют разный вес. Если уровень часто участвует в `get` и коротких `scan`, стоимость лишнего досмотра metadata важнее, и `K` должен быть меньше. Если уровень большой и в основном участвует в длинных scan, можно использовать более разреженный index.

Упрощённо:

```text
cost(K) = memory_cost * (N / K) + read_cost * K
```

Минимум такой функции находится около квадратного корня от отношения этих стоимостей.

```text
N / K уменьшается при росте K
K увеличивается при росте K
```

### Предлагаемая политика выбора K

`K` выбирается при создании SSTable.

Базовая формула:

```text
K_read = ceil(sqrt(level_logical_blocks / base_level_logical_blocks))
K_table = ceil(sqrt(sstable_logical_blocks))

K = min(K_read, K_table)
```

Где:

- `level_logical_blocks` - число логических блоков на уровне, куда пишется SSTable;
- `base_level_logical_blocks` - базовый размер верхнего уровня в логических блоках;
- `sstable_logical_blocks` - число логических блоков в создаваемом SSTable.

`K_read` делает index более разреженным на больших нижних уровнях.

`K_table` не даёт маленькому SSTable получить слишком грубый index.

`min(K_read, K_table)` выбирает более плотный из двух допустимых вариантов

### Учет статистики чтений (возможная оптимизация)

Если добавить runtime-статистику по чтениям, формулу можно уточнить:

```text
K_read = ceil(sqrt(
    level_logical_blocks
    /
    (base_level_logical_blocks * read_weight)
))

K_table = ceil(sqrt(sstable_logical_blocks))

K = min(K_read, K_table)
```

`read_weight` показывает, насколько важны быстрые чтения на данном уровне.

Если уровень часто участвует в `get` и коротких `scan`, `read_weight` растёт, а `K` уменьшается. То есть новые SSTable на этом уровне получают более плотный index.

Если уровень почти не участвует в точечных чтениях, `read_weight` близок к `1`, и `K` определяется в основном размером уровня.

Уже записанные SSTable не нужно перестраивать сразу. Новая политика применяется к новым SSTable при flush/compaction.

## Построение sparse index для row layout

Для row layout каждый `RowBlockMeta` является logical block-ом.

Алгоритм:

```text
for every K-th RowBlockMeta:
    add SparseIndexEntry {
        min_key = block.min_key
        block_id = block.block_id
    }
```

Пример:

```text
blocks:
0: [0, 100)
1: [100, 200)
2: [200, 300)
3: [300, 400)
4: [400, 500)

K = 2

sparse index:
{ min_key = 0,   block_id = 0 }
{ min_key = 200, block_id = 2 }
{ min_key = 400, block_id = 4 }
```

## Построение sparse index для column layout

Для column layout sparse index строится по группам `block_id`.

Входная metadata:

```text
(block_id = 0, column_idx = 0)
(block_id = 0, column_idx = 1)

(block_id = 1, column_idx = 0)
(block_id = 1, column_idx = 1)

(block_id = 2, column_idx = 0)
(block_id = 2, column_idx = 1)
```

Logical blocks:

```text
block_id = 0
block_id = 1
block_id = 2
```

Для каждой группы builder находит key column block и берёт из него `min_key`.

Если `K = 2`:

```text
sparse index:
{ min_key = 0,   block_id = 0 }
{ min_key = 200, block_id = 2 }
```

Если в logical block group нет key column block, это ошибка формата.

## Требования к SSTable-файлу

Sparse index должен сохраняться в SSTable-файл.

Ожидаемая схема:

```text
[data blocks]
[block metadata]
[sparse index]
[footer]
```

Footer или metadata header должен позволять найти sparse index.

Минимально нужно хранить:

```text
sparse_index_offset
sparse_index_count
```

Дополнительно можно хранить:

```text
sparse_index_step
```
Скорее всего, будет полезен для отладки, статистики и проверки формата.

## Требования к metadata в памяти

Read-path не должен читать sparse index из файла на каждый `scan` или `get`.

После flush/compaction или после загрузки SSTable в память должен существовать read handle, содержащий sparse index.

Ожидаемая структура по смыслу:

```cpp
struct SSTableReadHandle {
    SSTableMeta meta;

    std::vector<RowBlockMeta> row_blocks;
    std::vector<ColumnBlockMeta> column_blocks;

    std::vector<SparseIndexEntry> sparse_index;
};
```

Sparse index хранится:

- в файле - для восстановления после restart;
- в памяти - для быстрого read-path.

## Использование в чтении

Sparse index используется только selector-ом. Cursor-ы не меняются.

### Sparse selector API

Selector-у достаточно одного основного метода: выбрать blocks по `KeyRange`.

```cpp
class SparseRowBlockSelector {
public:
    SparseRowBlockSelector(
        std::vector<RowBlockMeta> blocks,
        std::vector<SparseIndexEntry> sparse_index
    );

    std::vector<RowBlockMeta> select_blocks(const KeyRange& range) const;
};
```

```cpp
class SparseColumnBlockSelector {
public:
    SparseColumnBlockSelector(
        std::vector<ColumnBlockMeta> blocks,
        std::vector<SparseIndexEntry> sparse_index
    );

    std::vector<ColumnBlockMeta> select_blocks(const KeyRange& range) const;
};
```

Для column layout selector возвращает целые logical block groups.

Если выбран `block_id = 5`, selector возвращает все:

```text
ColumnBlockMeta(block_id = 5, column_idx = 0)
ColumnBlockMeta(block_id = 5, column_idx = 1)
ColumnBlockMeta(block_id = 5, column_idx = 2)
...
```

Column cursor потом сам читает только key column и нужные projected columns.
