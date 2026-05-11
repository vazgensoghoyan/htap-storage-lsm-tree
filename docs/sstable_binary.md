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
    uint64_t min_key;
    uint64_t max_key;
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

```cpp
struct RowBlockMeta {
    int64_t  min_key;
    int64_t  max_key;
    uint32_t row_count;
    uint64_t offset;     // начало блока
    uint64_t size_bytes;
    uint32_t block_id;
};
```

### RMK:

Пока не вводим массив row_ofsets для бин поиска, предполагается, что тут и линейный скан пока что норм. Потом возможно захотим ввести. Стоит поначалу попробовать 64-256 строк в блоке.

Row block header-а нет, он не нужен. В части [DATA BLOCKS] просто хранятся подряд идущие много Row. Вся информация о том, где блок начинается и где заканчивается есть в части [META ABOUT BLOCKS], там все offset-ы и даже доп инфа, чтобы пропускать блоки или делать бин поиск если нужно.

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

Key не дублируется в values. И key у нас по всему хранилищу даже на уровне пользователя по схеме всегда обязан быть первой колонкой и всегда not null int64. Это наши требования.
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
[data]          // только значения не null, то есть их может быть меньше чем values_count
```

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
    uint64_t size_bytes;
    uint32_t block_id;
};
```

RMK: чтобы найти конкретную колонку, нужно видимо бин поиск делать по column_id этой части файла. Есть вариант расширить эту часть файла.

## COLUMN DATA BLOCK STRUCTURE

```text
[ColumnBlock(key)]
[ColumnBlock(key)]
[ColumnBlock(key)]
[ColumnBlock(col1)]
[ColumnBlock(col1)]
[ColumnBlock(col1)]
[ColumnBlock(col1)]
[ColumnBlock(col2)]
[ColumnBlock(col2)]
[ColumnBlock(col2)]
...
```

Все column blocks идут параллельно внутри одного logical row block.

# Endiannes

Думаю, четко зафиксируем little-endian.
