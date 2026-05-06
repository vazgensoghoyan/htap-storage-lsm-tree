SSTable ROW

Metadata Header:
magic = "....."
row_count
index_offset
data_offset
layout = ROW/COLUMN
level
min_key
max_key
block_size
----
index ???
1) ofsset minkey maxkey
2) ofsset minkey maxkey
3) ofsset minkey maxkey
----
[minkey maxkey row_count   block] // page size 4KB
[block]
[block]
[block]
[block]

SSTableReader -> cache page, I/O disk
|
| ?


null:
[bitmap][row]
010   value null value
тут стоит битмаску на весь block

[column_bitmap][column]
00010 value value value null value

выравнивать padding-ом до байта

1
2
3
4
5  // SCAN start
6
7
8
9
10
11
12
13
14 // SCAN end
15
16
17
18
19
20



scan

слой Memory: (MemoryCursor ~ MergeCursor(MemCursor, vector(ImmMemCursor)))
memtable (MemCursor) ->
imm_memtable imm_memtable imm_memtable (ImmMemCursor)

SSTableBuilder  imm_memtable -> row SSTable

row SSTable | row SSTable (тут свои Cursor-ы)
column SSTable (а тут как Cursor?)

projection 100, а нужны 0, 2, 5, 99


Где тут появляется page cache?
SSTableRowCursor
SSTableColumnCursor
либо обобщенный с двумя стратегиями (для row, для column) SSTableCursor(std::string file_name)

SSTableRedaer:
И этот курсор будет обращаться к сущности которая работает с page cache и напрямую с IO диска


O_DIRECT - пропускать кеш страниц операционки
Использовать ли?
fsync?
Не паримся


WAL? Write ahead log
Это чтобы данные не утерять
Полная персистентность
Ближе к executor, после успешного ответа пишет в wal, отдельная подпсистема





А давай все таки header сделаем footer

Будет блок [data] где будут блоки данных и без метаданных
Потом блок [block-meta] где будут данный формата (min_key, max_key, row_count, offset_of_block) на каждый блок
Потом footer с информацией

Это для построчного хранения

Ну и для поколоночного определи. Давай додумаем и четко зафиксируем бинарный формат


ROW хранение:

[DATA BLOCKS]
[META MINI INDEX]
[FOOTER WITH INFO]

struct SSTFooter {
    uint32_t magic;        // "SST1"

    uint8_t layout_type;   // 0 = ROW, 1 = COLUMN

    uint32_t num_blocks;

    uint64_t meta_offset;  // start of block-metadata
};

RowBlock состоит просто из какого то колва row. Мета для нее не хранится, по факту часть [DATA BLOCKS] просто много подряд идущих Row. Нужная мета будет в [META MINI INDEX]

Row состоит из (key, null_bitmap, values)

struct RowBlockMeta {
    int64 min_key, max_key;
    uint32 row_count; // может варьироваться для разных блоков, чтобы примерно все выровнять по размеру
    // но фикс размера блока по байтам не будет я думаю
    // просто организуем кеш по таким примерно 4КБ или 8 блокам, примерно
    uint64 offset
}

Про COLUMN

[COL_0 (KEY) PART]
[COL_1 PART] // состоит из меньших блоков очевидно, битмап храним на блок, не на всю колонку
..
[COL_N PART]
[META INFO ABOUT BLOCKS]
[FOOTER WITH INFO]

struct ColumnBlock {
    uint16 num_values;
    bitmap nulls;
    values[];
}

struct ColumnKeyMeta {
    int64 min_key;
    int64 max_key;
    uint64 offset;
}

struct ColumnMeta {
    uint64 offset;
}

Строки храним как [length: uint32] [bytes ...]


