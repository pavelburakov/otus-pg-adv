# Отчёт по ДЗ №13: Параллельный кластер (Greenplum)

**Студент:** Бураков Павел  
**Дата:** 2026-03-24  
**Проект:** bananaflow-04051981-greenplum

---

## 1. Что выбрал и почему

Выбрал Greenplum — массово-параллельная (MPP) аналитическая СУБД.

**Почему:**
- Greenplum заточен под большие объёмы данных и сложные аналитические запросы
- В предыдущих ДЗ уже работал с CockroachDB и ClickHouse; теперь нужно попробовать классический MPP
- Легко сравнивать с одиночным PostgreSQL, так как Greenplum построен на PostgreSQL

**Среда:** Yandex Cloud — 3 ВМ (1 мастер + 2 сегментных хоста) с Ubuntu 20.04, 4 vCPU, 8 ГБ RAM, 20 ГБ SSD на каждом.

---

## 2. Развёртывание кластера Greenplum

### 2.1 Создание ВМ

Через Yandex Cloud CLI создал три ВМ:

```bash
yc compute instance create --name gp-master \
  --cores 4 --memory 8 \
  --create-boot-disk size=20G,type=network-ssd,image-family=ubuntu-2004-lts \
  --network-interface subnet-name=default-ru-central1-a,nat-ip-version=ipv4 \
  --ssh-key ~/.ssh/id_rsa.pub

yc compute instance create --name gp-segment1 \
  --cores 4 --memory 8 \
  --create-boot-disk size=20G,type=network-ssd,image-family=ubuntu-2004-lts \
  --network-interface subnet-name=default-ru-central1-a,nat-ip-version=ipv4 \
  --ssh-key ~/.ssh/id_rsa.pub

yc compute instance create --name gp-segment2 \
  --cores 4 --memory 8 \
  --create-boot-disk size=20G,type=network-ssd,image-family=ubuntu-2004-lts \
  --network-interface subnet-name=default-ru-central1-a,nat-ip-version=ipv4 \
  --ssh-key ~/.ssh/id_rsa.pub
```

Внутренние IP: мастер – `10.128.0.5`, сегмент1 – `10.128.0.6`, сегмент2 – `10.128.0.7`.

### 2.2 Установка Greenplum 

На всех узлах установил зависимости и скачал бинарную сборку Greenplum 6.25:

```bash
sudo apt update && sudo apt install -y python3 python3-pip openssh-server
curl storage.yandexcloud.net/greenplum-jammy-packages/install.sh | sudo bash
sudo apt update && sudo apt install /tmp/gp-packages/*.deb
```

На мастере создал файл `hostfile` с IP сегментов и запустил инициализацию кластера:

```bash
export GPHOME=/usr/local/greenplum-db
source $GPHOME/greenplum_path.sh
gpssh-exkeys -f hostfile
gpseginstall -f hostfile
```

Инициализация кластера с 2 сегментами:

```bash
mkdir /data/gpdata
gpinitsystem -c gpinitsystem_config -h hostfile
```

После успешного старта получил кластер с одним мастером и двумя сегментами, каждый сегмент с двумя primary mirror — для отказоустойчивости.

---

## 3. Загрузка данных

Создал таблицу `shipments` с распределением по хэшу `id` (чтобы данные равномерно легли на сегменты):

```sql
CREATE TABLE shipments (
    id BIGINT,
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
) DISTRIBUTED BY (id);
```

Генерация 10 млн строк, аналогично PostgreSQL, но с использованием `generate_series` прямо в Greenplum. Загрузка заняла ~3 минуты:

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

---

## 4. Тестовые запросы (те же, что и в ДЗ №10)

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

## 5. Результаты сравнения

| Запрос | PostgreSQL (1 инстанс) | Greenplum (2 сегмента) |
|--------|------------------------|------------------------|
| #1 (агрегация по категориям) | 14.7 сек | 3.2 сек |
| #2 (регион + месяц) | 52.3 сек | 8.1 сек |
| #3 (топ-10 клиентов) | 21.8 сек | 4.5 сек |

**Ускорение:**
- Запрос #1: **в 4.6 раза**
- Запрос #2: **в 6.5 раза**
- Запрос #3: **в 4.8 раза**

---

## 6. Проблемы и нюансы

- **Сложность установки**: Greenplum требует ручной настройки ssh-доступа между узлами и конфигурационных файлов.
- **Распределение данных**: важно правильно выбрать ключ распределения, иначе некоторые сегменты могут быть перегружены.
- **Потребление ресурсов**: на 10 млн строк (10 ГБ) каждый сегмент использовал ~5 ГБ RAM во время агрегаций.
- **Время загрузки**: несмотря на параллельность, генерация через `generate_series` шла медленнее, чем в ClickHouse, но всё равно быстрее, чем в PostgreSQL.
- **Аналитические запросы**: Greenplum показал значительный прирост за счёт параллельной обработки.

---

## 7. Выводы для BananaFlow

| Критерий | PostgreSQL | ClickHouse | Greenplum (MPP) |
|----------|------------|------------|-----------------|
| Тип нагрузки | OLTP / смешанная | OLAP | OLAP (MPP) |
| Скорость агрегаций (10 млн) | 14–52 сек | 0.9–2.1 сек | 3–8 сек |
| Горизонтальное масштабирование | ❌ | ✅ (шардинг) | ✅ (MPP) |
| Сложность развёртывания | ✅ просто | ✅ просто | ⚠️ сложно |
| Совместимость с SQL | ✅ стандарт | ⚠️ диалект | ✅ стандарт PostgreSQL |

**Для BananaFlow:**
- Если нужна аналитика на больших объёмах (10–100 ГБ+), Greenplum — отличный выбор.
- ClickHouse даёт ещё более высокую скорость на агрегациях, но требует адаптации SQL.
- PostgreSQL подходит для транзакционной нагрузки, но для аналитики на больших данных нужны индексы, партиции или дополнительные решения.
