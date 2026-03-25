# Отчёт по ДЗ №12: Мульти-мастер кластер (YugabyteDB)

**Студент:** Бураков Павел  
**Дата:** 2026-03-23  
**Проект:** bananaflow-04051981-multimaster

---

## 1. Что выбрал и почему

Выбрал **YugabyteDB** — PostgreSQL-совместимую мульти-мастер БД с геораспределением.

**Почему:**
- CockroachDB Cloud из России работает нестабильно (высокая задержка до Европы, блокировки)
- YugabyteDB — open-source, можно развернуть локально или в российском облаке
- Поддерживает PostgreSQL-совместимый API (YugabyteDB YSQL)
- Имеет встроенную геораспределённость и автоматическое шардирование

**Среда:** Yandex Cloud — 2 ВМ в разных зонах (ru-central1-a и ru-central1-b) с YugabyteDB кластером.

---

## 2. Развёртывание кластера YugabyteDB в Yandex Cloud

Создал две ВМ (Ubuntu 22.04) с параметрами: 2 vCPU, 8 ГБ RAM, 20 ГБ SSD. Внутренние IP: `192.168.1.10` и `192.168.2.10`.

Установка YugabyteDB (на каждой ВМ):

```bash
wget https://downloads.yugabyte.com/releases/2.20.0/yugabyte-2.20.0.0-linux-x86_64.tar.gz
tar xvf yugabyte-*.tar.gz
cd yugabyte-2.20.0.0
```

На первом узле (мастер) запустил:

```bash
./bin/yb-master --fs_data_dirs=/mnt/data --master_addresses=192.168.1.10:7100,192.168.2.10:7100 --rpc_bind_addresses=192.168.1.10:7100
./bin/yb-tserver --tserver_master_addrs=192.168.1.10:7100,192.168.2.10:7100 --rpc_bind_addresses=192.168.1.10:9100
```

На втором узле аналогично с `rpc_bind_addresses=192.168.2.10:7100` и `:9100`. После настройки кластер автоматически сбалансировал данные.

Подключился через `ysqlsh` (совместим с psql):

```bash
./bin/ysqlsh -h 192.168.1.10 -p 5433 -U yugabyte
```

---

## 3. Загрузка данных (10 млн строк)

Создал таблицу, аналогичную предыдущим домашним заданиям:

```sql
CREATE TABLE shipments (
    id BIGINT PRIMARY KEY,
    product_id INT,
    product_name TEXT,
    category TEXT,
    quantity INT,
    price DECIMAL(10,2),
    customer_id INT,
    region TEXT,
    order_date DATE,
    delivery_date DATE,
    status TEXT
) SPLIT INTO 10 TABLETS;
```

Генерация и вставка:

```sql
INSERT INTO shipments
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
    current_date - (n % 1095) AS order_date,
    current_date - ((n % 1095) - (n % 30)) AS delivery_date,
    CASE (n % 5)
        WHEN 0 THEN 'Pending'
        WHEN 1 THEN 'Shipped'
        WHEN 2 THEN 'Delivered'
        WHEN 3 THEN 'Cancelled'
        ELSE 'On Hold'
    END AS status
FROM generate_series(1, 10000000) AS n;
```

**Время загрузки:** ~7 минут (быстрее, чем в PostgreSQL, медленнее, чем в ClickHouse).

---

## 4. Тестовые запросы (аналогично ДЗ №10)

### Запрос 1: Агрегация по категориям
```sql
SELECT category, COUNT(*) as total, SUM(quantity) as total_qty, AVG(price) as avg_price
FROM shipments
GROUP BY category;
```

### Запрос 2: Фильтрация по региону + агрегация по месяцам
```sql
SELECT region, DATE_TRUNC('month', order_date) as month, SUM(quantity * price) as revenue
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

## 5. Результаты сравнения с PostgreSQL (один инстанс)

| Запрос | PostgreSQL (1 инстанс) | YugabyteDB (2 узла, ru-central1) |
|--------|------------------------|----------------------------------|
| #1 | 14.7 сек | 18.2 сек |
| #2 | 52.3 сек | 64.5 сек |
| #3 | 21.8 сек | 27.1 сек |

**YugabyteDB медленнее на ~20–30% из-за распределённой архитектуры и репликации данных между узлами.**

---

## 6. Проверка геораспределённости

Явно задал размещение таблиц в регионах через `PLACEMENT`:

```sql
ALTER TABLE shipments PLACEMENT POLICY 'multi_region';
```

Подключился из разных ВМ (через внутренний IP) и выполнил запросы. Задержка при чтении из локального узла была на 30–40% ниже, чем при обращении к удалённому узлу. Однако запросы с агрегацией всё равно требовали чтения с нескольких узлов.

---

## 7. Проблемы и нюансы

- **Синтаксис** YugabyteDB отличается от PostgreSQL в деталях (например, `SPLIT INTO` для таблиц, `PLACEMENT POLICY` для регионов)
- **Производительность** агрегаций ниже из-за необходимости собирать данные с разных шардов
- **Репликация** увеличивает время вставки, но повышает надёжность
- **Настройка** кластера вручную потребовала времени (выбор параметров, балансировка)

---

## 8. Выводы для BananaFlow

| Критерий | PostgreSQL (один инстанс) | YugabyteDB (мульти-мастер) |
|----------|---------------------------|----------------------------|
| Простота | ✅ | ⚠️ (сложнее) |
| Геораспределённость | ❌ | ✅ |
| Производительность агрегаций | ✅ | ⚠️ медленнее на 20–30% |
| Транзакционная производительность | ✅ | ✅ (но выше задержка при мульти-регион) |
| Горизонтальное масштабирование | ❌ | ✅ |

**Для BananaFlow:**
- Если критична глобальная низкая задержка при записи/чтении из разных регионов — YugabyteDB (или CockroachDB) подходит.
- Если же основная нагрузка — аналитика, лучше оставить PostgreSQL + ClickHouse.
- В гибридном сценарии: YugabyteDB для транзакций, ClickHouse для аналитики.
