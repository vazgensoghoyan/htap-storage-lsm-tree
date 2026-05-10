# Query Language

## Overview

This document describes the currently supported query language for the HTAP storage prototype.

The parser currently supports the following statement types:

* `CREATE TABLE`
* `INSERT INTO ... VALUES`
* `SELECT`

Only **one statement per query** is supported.

The parser performs **syntactic validation only**. Semantic checks and execution checks are handled by later stages.

---

## Lexical Rules

### Keywords

Keywords are case-insensitive.

Supported keywords:

* `CREATE`
* `TABLE`
* `INSERT`
* `INTO`
* `VALUES`
* `SELECT`
* `FROM`
* `WHERE`
* `KEY`
* `NULL`
* `NOT`
* `AND`
* `OR`
* `IS`
* `GROUP`
* `BY`
* `ORDER`
* `LIMIT`
* `ASC`
* `DESC`
* `COUNT`
* `SUM`
* `AVG`
* `MIN`
* `MAX`
* `STRING`
* `INT64`
* `DOUBLE`

### Identifiers

Identifiers are used for:

* table names
* column names

Identifiers:

* may contain letters, digits and `_`
* must start with a letter or `_`

### Literals

Supported literal kinds:

* string literals
* integer literals
* double literals
* `NULL`

#### String literals

String literals use PostgreSQL-style single quotes.

Examples:

```sql
'hello'
''
'I''m fine'
```

A single quote inside a string is written as `''`.

#### Integer literals

Examples:

```sql
0
42
-7
```

#### Double literals

Examples:

```sql
3.14
-0.5
12.0
```

### Operators

Supported comparison operators:

* `=`
* `!=`
* `<`
* `<=`
* `>`
* `>=`

### Punctuation

Supported punctuation:

* `(`
* `)`
* `,`
* `;`
* `*`

---

## Data Types

Supported column types:

* `STRING`
* `INT64`
* `DOUBLE`

---

## CREATE TABLE

### Syntax

```sql
CREATE TABLE table_name (
    column_name column_type [KEY | NULL | NOT NULL],
    ...
);
```

### Notes

* a table definition must contain at least one column
* currently supported column modifiers:

  * `KEY`
  * `NULL`
  * `NOT NULL`

### Example

```sql
CREATE TABLE users (
    key STRING KEY,
    name STRING NULL,
    age INT64,
    salary DOUBLE NULL
);
```

---

## INSERT

### Syntax

```sql
INSERT INTO table_name (column_name, ...)
VALUES (literal, ...);
```

### Notes

* an explicit column list is required
* inserted values are currently restricted to literals
* syntax is validated by the parser
* checks such as matching the number of columns and values belong to later semantic validation

### Example

```sql
INSERT INTO users (key, name, age, salary)
VALUES ('u1', 'Dasha', 19, 1000000.5);
```

---

## SELECT

### Syntax

```sql
SELECT select_item, ...
FROM table_name
[WHERE expression]
[GROUP BY expression, ...]
[ORDER BY expression [ASC | DESC], ...]
[LIMIT int_literal];
```

### Supported Select Items

A select item may be:

* `*`
* a regular expression
* an aggregate expression

Examples:

```sql
SELECT *
SELECT name
SELECT age
SELECT AVG(score)
SELECT city, COUNT(key)
```

---

## Aggregate Functions

Supported aggregate functions:

* `COUNT(expr)`
* `SUM(expr)`
* `AVG(expr)`
* `MIN(expr)`
* `MAX(expr)`

### Important Limitation

`COUNT(*)` is **not supported**.

Use `COUNT(key_column)` instead.

---

## Expressions

Expressions are currently supported in:

* `WHERE`
* `GROUP BY`
* `ORDER BY`
* aggregate arguments
* `SELECT` items
* `INSERT` values (literals only)

### Supported Expression Forms

* column reference
* literal
* parenthesized expression
* unary `NOT`
* comparison expression
* `AND`
* `OR`
* `IS NULL`
* `IS NOT NULL`

### Examples

```sql
age >= 18
city = 'SPB'
(a = 1 AND b = 2) OR c = 3
NOT active
city IS NULL
city IS NOT NULL
```

### Operator Precedence

Expressions are parsed with the following precedence:

1. atomic expressions

   * identifiers
   * literals
   * parenthesized expressions
2. predicate expressions

   * comparison operators
   * `IS NULL`
   * `IS NOT NULL`
3. unary `NOT`
4. `AND`
5. `OR`

For example:

```sql
a = 1 AND b = 2 OR c = 3
```

is parsed as:

```text
((a = 1) AND (b = 2)) OR (c = 3)
```

---

## WHERE

### Syntax

```sql
WHERE expression
```

### Example

```sql
SELECT name
FROM users
WHERE age >= 18 AND city IS NOT NULL;
```

---

## GROUP BY

### Syntax

```sql
GROUP BY expression, ...
```

### Example

```sql
SELECT city, COUNT(key)
FROM users
GROUP BY city;
```

---

## ORDER BY

### Syntax

```sql
ORDER BY expression [ASC | DESC], ...
```

### Notes

* if direction is omitted, ascending order is used by default

### Examples

```sql
ORDER BY city
ORDER BY city ASC
ORDER BY score DESC
ORDER BY city ASC, country DESC
```

---

## LIMIT

### Syntax

```sql
LIMIT int_literal
```

### Example

```sql
SELECT name
FROM users
LIMIT 10;
```

---

## Unsupported Features

The following features are currently not supported:

* `COUNT(*)`
* `DISTINCT`
* `HAVING`
* aliases (`AS`)
* arithmetic expressions such as `a + b`
* joins
* `UPDATE`
* `DELETE`
* multiple statements in one query
* quoted identifiers
* comments
* PostgreSQL-specific extended string syntaxes such as `E'...'`
* dollar-quoted strings

---

## Notes on Validation

The parser performs **syntactic validation only**.

Examples of checks that belong to later semantic or execution stages:

* whether a table exists
* whether a column exists
* whether exactly one key column is defined
* whether aggregate usage is semantically valid
* whether `GROUP BY` is compatible with the select list
* whether the number of inserted values matches the number of specified columns
* whether `LIMIT` is semantically valid for execution
