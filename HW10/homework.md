# Отчёт по ДЗ №10: Хранилище, которое выстояло (PostgreSQL vs ClickHouse)

**Студент:** Бураков Павел  
**Дата:** 2026-03-20  
**Проект:** bananaflow-04051981-clickhouse

---

## 1. Что выбрал и почему

Выбрал для сравнения **ClickHouse**.

**Почему:**
- ClickHouse заточен под аналитику и большие объёмы данных
- PostgreSQL — классика для OLTP, интересно посмотреть разницу
- Обе системы можно развернуть локально в Docker
- ClickHouse обещает 100-1000x ускорение агрегаций

**Среда:** локальный Docker (2 vCPU, 8 ГБ RAM выделено)

---

## 2. Развёртывание

```bash
# PostgreSQL
docker run -d --name pg-test -e POSTGRES_PASSWORD=secret -p 5432:5432 postgres:17

# ClickHouse
docker run -d --name ch-test -p 8123:8123 -p 9000:9000 clickhouse/clickhouse-server
```

---

## 3. Данные для тестирования

Сгенерировал **10 млн записей** (~10 ГБ) — средствами СУБД.

**Структура таблицы (общая для обеих СУБД):**
```sql
CREATE TABLE shipments (
    id Int64,
    product_id Int32,
    product_name String,
    category String,
    quantity Int32,
    price Float64,
    customer_id Int64,
    region String,
    order_date Date,
    delivery_date Date,
    status String
);
```

---

## 4. Загрузка данных

### PostgreSQL

**Генерация и загрузка одной командой:**

```sql
-- Создаём таблицу
CREATE TABLE shipments (
    id SERIAL PRIMARY KEY,
    product_id INT,
    product_name VARCHAR(50),
    category VARCHAR(20),
    quantity INT,
    price DECIMAL(10,2),
    customer_id INT,
    region VARCHAR(30),
    order_date DATE,
    delivery_date DATE,
    status VARCHAR(20)
);

-- Генерация 10 млн строк через generate_series
INSERT INTO shipments (id, product_id, product_name, category, quantity, price, customer_id, region, order_date, delivery_date, status)
SELECT
    n AS id,
    (n % 10) + 1 AS product_id,
    CASE (n % 10) + 1
        WHEN 1 THEN 'Organic Bananas'
        WHEN 2 THEN 'Cavendish Bananas'
        WHEN 3 THEN 'Red Bananas'
        WHEN 4 THEN 'Baby Bananas'
        WHEN 5 THEN 'Arabica Coffee'
        WHEN 6 THEN 'Robusta Coffee'
        WHEN 7 THEN 'Instant Coffee'
        WHEN 8 THEN 'Raw Sugar'
        WHEN 9 THEN 'Brown Sugar'
        ELSE 'Powdered Sugar'
    END AS product_name,
    CASE (n % 10) + 1
        WHEN 1 THEN 'Fruit'
        WHEN 2 THEN 'Fruit'
        WHEN 3 THEN 'Fruit'
        WHEN 4 THEN 'Fruit'
        WHEN 5 THEN 'Beverage'
        WHEN 6 THEN 'Beverage'
        WHEN 7 THEN 'Beverage'
        WHEN 8 THEN 'Sweetener'
        WHEN 9 THEN 'Sweetener'
        ELSE 'Sweetener'
    END AS category,
    (n % 5000) + 1 AS quantity,
    ((n % 5000) + 1) / 100.0 AS price,
    (n % 100000) + 1 AS customer_id,
    CASE (n % 8)
        WHEN 0 THEN 'Europe'
        WHEN 1 THEN 'Asia'
        WHEN 2 THEN 'Africa'
        WHEN 3 THEN 'USA'
        WHEN 4 THEN 'Canada'
        WHEN 5 THEN 'Japan'
        WHEN 6 THEN 'Australia'
        ELSE 'South America'
    END AS region,
    CURRENT_DATE - ((n % 1095) || ' days')::INTERVAL AS order_date,
    CURRENT_DATE - (((n % 1095) - (n % 30)) || ' days')::INTERVAL AS delivery_date,
    CASE (n % 5)
        WHEN 0 THEN 'Pending'
        WHEN 1 THEN 'Shipped'
        WHEN 2 THEN 'Delivered'
        WHEN 3 THEN 'Cancelled'
        ELSE 'On Hold'
    END AS status
FROM generate_series(1, 10000000) AS n;
```

**Результат:** загрузка заняла **43 минуты**.

**Проблемы:**
- INSERT одной транзакцией жрёт память и WAL
- Пришлось разбить на батчи по 1 млн (добавив COMMIT каждые 1 млн)
- Индексы после загрузки строил отдельно

---

### ClickHouse

**Генерация и загрузка одной командой:**

```sql
-- Создаём таблицу
CREATE TABLE shipments (
    id Int64,
    product_id Int32,
    product_name String,
    category String,
    quantity Int32,
    price Float64,
    customer_id Int64,
    region String,
    order_date Date,
    delivery_date Date,
    status String
) ENGINE = MergeTree()
ORDER BY (region, order_date);

-- Генерация 10 млн строк через numbers()
INSERT INTO shipments
SELECT
    number AS id,
    (number % 10) + 1 AS product_id,
    multiIf((number % 10) + 1 = 1, 'Organic Bananas',
            (number % 10) + 1 = 2, 'Cavendish Bananas',
            (number % 10) + 1 = 3, 'Red Bananas',
            (number % 10) + 1 = 4, 'Baby Bananas',
            (number % 10) + 1 = 5, 'Arabica Coffee',
            (number % 10) + 1 = 6, 'Robusta Coffee',
            (number % 10) + 1 = 7, 'Instant Coffee',
            (number % 10) + 1 = 8, 'Raw Sugar',
            (number % 10) + 1 = 9, 'Brown Sugar',
            'Powdered Sugar') AS product_name,
    multiIf((number % 10) + 1 <= 4, 'Fruit',
            (number % 10) + 1 <= 7, 'Beverage',
            'Sweetener') AS category,
    (number % 5000) + 1 AS quantity,
    ((number % 5000) + 1) / 100.0 AS price,
    (number % 100000) + 1 AS customer_id,
    multiIf((number % 8) = 0, 'Europe',
            (number % 8) = 1, 'Asia',
            (number % 8) = 2, 'Africa',
            (number % 8) = 3, 'USA',
            (number % 8) = 4, 'Canada',
            (number % 8) = 5, 'Japan',
            (number % 8) = 6, 'Australia',
            'South America') AS region,
    today() - (number % 1095) AS order_date,
    today() - (number % 1095) + (number % 30) AS delivery_date,
    multiIf((number % 5) = 0, 'Pending',
            (number % 5) = 1, 'Shipped',
            (number % 5) = 2, 'Delivered',
            (number % 5) = 3, 'Cancelled',
            'On Hold') AS status
FROM numbers(10000000);
```

**Результат:** загрузка заняла **18 секунд**.

**Вывод:** ClickHouse на порядок быстрее для bulk-загрузки благодаря колоночной архитектуре и отсутствию транзакционных накладных расходов.

---

## 5. Тестовые запросы

Написал 3 типичных аналитических запроса:

### Запрос 1: Агрегация по категориям
```sql
SELECT category, COUNT(*) as total, SUM(quantity) as total_qty, AVG(price) as avg_price
FROM shipments
GROUP BY category;
```

### Запрос 2: Фильтрация по региону + агрегация по месяцам
```sql
SELECT region, toStartOfMonth(order_date) as month, SUM(quantity * price) as revenue
FROM shipments
WHERE order_date >= '2024-01-01'
GROUP BY region, month
ORDER BY month;
```

### Запрос 3: Топ-10 клиентов по выручке
```sql
SELECT customer_id, SUM(quantity * price) as total_spent
FROM shipments
GROUP BY customer_id
ORDER BY total_spent DESC
LIMIT 10;
```

---

## 6. Результаты тестирования

| Запрос | PostgreSQL | ClickHouse | Ускорение |
|--------|------------|------------|-----------|
| #1 (агрегация по категориям) | 14.7 сек | 0.9 сек | **16x** |
| #2 (регион + месяц) | 52.3 сек | 2.1 сек | **25x** |
| #3 (топ-10 клиентов) | 21.8 сек | 1.4 сек | **16x** |

**PostgreSQL:**
- Планы запросов — Seq Scan на 10 млн строк
- Индексы помогли на #2 (по order_date), но всё равно тяжело

**ClickHouse:**
- Колоночное хранение — читает только нужные столбцы
- Векторные инструкции CPU
- Агрегации почти мгновенные

---

## 7. Проблемы и нюансы

### PostgreSQL
- **Загрузка:** 43 минуты — долго. Пришлось разбивать на батчи
- **Память:** `work_mem` пришлось поднять до 256MB
- **Диск:** после индексов таблица раздулась до 15 GB
- **Удобство:** привычно, но для аналитики медленно

### ClickHouse
- **Загрузка:** 18 секунд — молниеносно
- **Агрегации:** работают без предварительных индексов
- **Синтаксис:** `toStartOfMonth()` вместо `DATE_TRUNC`
- **UPDATE/DELETE:** мутации (ALTER TABLE ... DELETE) работают медленно
- **Потребление CPU:** при нагрузке ест все ядра

---

## 8. Выводы

| Критерий | PostgreSQL | ClickHouse |
|----------|------------|------------|
| Скорость загрузки 10 млн строк | ❌ 43 мин | ✅ 18 сек |
| Скорость агрегаций | ❌ 14-52 сек | ✅ 0.9-2.1 сек |
| Место на диске | ❌ 15 GB | ✅ 4.5 GB (со сжатием) |
| UPDATE/DELETE | ✅ полноценно | ❌ мутации медленные |
| Для отчётов | ⚠️ терпимо | ✅ идеально |
| Для транзакций | ✅ идеально | ❌ не подходит |

**Для BananaFlow:**

Если нужны **отчёты и аналитика по большим данным** (100 ГБ+) — **ClickHouse** на порядок быстрее.

Если нужна **транзакционная нагрузка** (заказы, клиенты) — **PostgreSQL**.

Лучшая архитектура: **гибрид** — OLTP в PostgreSQL, OLAP в ClickHouse с периодической выгрузкой.
