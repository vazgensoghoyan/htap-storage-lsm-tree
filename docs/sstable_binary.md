# SSTable Binary Format (HTAP LSM Engine)

## Общая идея

SSTable — immutable файл, содержащий отсортированные по key данные, разбитые на блоки.

Каждый SSTable состоит из:

```text
[DATA BLOCKS]
[META ABOOUT BLOCKS] // mini index
[FOOTER]
```

---

# FOOTER (фиксированная точка входа)

Footer всегда находится в конце файла. Это точка входа в чтение файла.

```cpp
struct SSTFooter {
    uint32_t magic;        // "SST1", например
    uint32_t num_blocks;
    uint64_t meta_offset;  // offset начала META INDEX
    uint8_t layout_type;   // 0 = ROW, 1 = COLUMN
};
```

# META INDEX (block metadata array)

Расположен перед footer, читается через meta_offset.

## Общий формат:

```text
[Block_1_meta]
[Block_2_meta]
...
[Block_N_meta]
```
    int64_t max_key;
    int16_t column_id;
    uint32_t values_count;
    uint64_t offset;
};
```

RMK: чтобы найти конкретную 

---

## Row Block Meta

```cpp
struct RowBlockMeta {
    int64_t  min_key;
    int64_t  max_key;
    uint32_t row_count;
    uint64_t offset;     // начало блока
};
```

### Назначение:

* быстрый skip блоков при scan
* определение диапазона ключей
* навигация по файлу через offset

---

### RMK:

Row block header-а нет, он не нужен. В части [DATA BLOCKS] просто хранятся подряд идущие много Row. Вся информация о том, где блок начинается и где заканчивается есть в части [META ABOUT BLOCKS], там все offset-ы и даже доп инфа, чтобы пропускать блоки или делать бин поиск если нужно.

Block_id тоже явно не вводится, потому что пока что этот простой "индекс", если так вообще можно это назвать, имеет фиксированные размеры мета инфы на каждый блок. Нумерация просто идет 0, 1, ...

---

## Column Block Meta

(если COLUMN layout)

```cpp
struct ColumnBlockMeta {
    int64_t min_key;
    int64_t max_key;
    int16_t column_id;
    uint32_t values_count;
    uint64_t offset;
};
```

RMK: чтобы найти конкретную колонку, нужно видимо бин поиск делать по column_id этой части файла. Есть вариант расширить эту часть файла.

```
struct ColumnMeta {
    int16_t block_id; // первый блок который принадлежит этой колонке
}
```

Это пусть будет в начале части [META INDEX].

---

# DATA BLOCKS

Блок — минимальная единица:

* чтения с диска
* кеширования
* декодирования

## Row encoding

```text
[key: int64]
[null_bitmap]   // size = ceil(num_columns / 8)
[values...]
```

На каждое значение кроме string уходит 8 байт. Формат хранения строк: uint32 length, bytes[ length ].

---

# COLUMN Layout

## Общий вид SST:

```text
[DATA BLOCKS]
[META INDEX]
[FOOTER]
```

---

## Column Block (внутри data block)

```text
[null_bitmap]   // сразу на весь блок, size = ceil(block_rows / 8)
[data]          // values_count (из meta index) значений
```

## COLUMN DATA BLOCK STRUCTURE

```text
[ColumnBlock(key)]
[ColumnBlock(col1)]
[ColumnBlock(col2)]
...
```

Все column blocks идут параллельно внутри одного logical row block.

---

## Block boundaries

Определяются через META INDEX:

```text
size(block[i]) = meta[i+1].offset - meta[i].offset
```

(или footer boundary для последнего)

---
---
---

# Чтение из SSTable (по шагам)

## 1. Открытие SSTable

* если файл ещё не открыт → открываем file descriptor
* читаем footer с конца файла

## 2. Разбор footer

* проверяем `magic`
* определяем `layout_type` (ROW / COLUMN)
* читаем `meta_offset`
* читаем `num_blocks`

## 3. Загрузка META INDEX

* читаем блок `RowBlockMeta[]` (или ColumnBlockMeta[])
* кладём в память (или кешируем отдельно как index-cache)

## 4. Поиск подходящих блоков (pruning)

* для key или range:
  * идём по meta index
  * выбираем блоки где:

```text
min_key <= key < max_key
```

или пересечение диапазона

## 5. Проверка block cache

для каждого выбранного блока:

* формируем `cache_key = (sst_id, block_id)`
* проверяем LRU cache:
  * если есть → используем готовый блок
  * если нет → читаем с диска
  * тут появляются тактики кеширования

## 6. Чтение блока с диска (cache miss)

* делаем `pread(offset, size)`
* получаем бинарный block
* кладём в cache
* продолжаем

## 7. Декодирование блока

### ROW:

* читаем RowBlockHeader
* читаем строки последовательно:
  * key
  * null_bitmap
  * values

### COLUMN:

* читаем ColumnBlockHeader
* читаем столбцы
* собираем row по projection

## 8. Фильтрация по key

* если точечный get:
  * внутри блока ищем key (бинарный поиск)
* если scan:
  * идём по всем строкам блока (начало и конец можно бинарным поиском, если будет давать улучшение)

## 9. Возврат результата через Cursor

* строки не копируются сразу
* Cursor отдаёт:
  * key()
  * value(col)
  * next()

## 10. Merge на уровне LSM (если несколько SST)

* результаты нескольких SST cursors сливаются через MergeCursor
* порядок гарантируется по key
