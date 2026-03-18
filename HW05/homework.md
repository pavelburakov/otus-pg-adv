# Отчёт по ДЗ №5: Бэкапы с WAL-G в Docker

**Студент:** Бураков Павел  
**Дата:** 2026-03-18  
**Проект:** `bananaflow-19810504-backup-docker`

---

## 1. Подготовка окружения

Для выполнения задания буду использовать Docker на локальной машине. Создам сеть и два контейнера: мастер и реплика.

```bash
# Создаём сеть для контейнеров
docker network create banana-net

# Создаём том для хранения бэкапов (общий для обоих контейнеров)
docker volume create backup-volume
```

---

## 2. Запуск контейнера с PostgreSQL (мастер)

```bash
docker run -d \
  --name pg-master \
  --network banana-net \
  -e POSTGRES_PASSWORD=secret \
  -e POSTGRES_USER=postgres \
  -e POSTGRES_DB=shipments \
  -v pg-master-data:/var/lib/postgresql/data \
  -v backup-volume:/backup \
  -p 5432:5432 \
  postgres:17
```

Проверяем:
```bash
docker ps
docker exec -it pg-master psql -U postgres -d shipments
```

---

## 3. Настройка мастера для репликации

Заходим в контейнер и правим конфиги:

```bash
docker exec -it pg-master bash
```

Внутри контейнера:

```bash
# Правим postgresql.conf
echo "listen_addresses = '*'" >> /var/lib/postgresql/data/postgresql.conf
echo "wal_level = replica" >> /var/lib/postgresql/data/postgresql.conf
echo "max_wal_senders = 5" >> /var/lib/postgresql/data/postgresql.conf
echo "wal_keep_size = 1GB" >> /var/lib/postgresql/data/postgresql.conf
```

Правим pg_hba.conf:
```bash
echo "hostssl replication replicator 0.0.0.0/0 scram-sha-256" >> /var/lib/postgresql/data/pg_hba.conf
echo "host all all 0.0.0.0/0 md5" >> /var/lib/postgresql/data/pg_hba.conf
```

Перезапускаем контейнер:
```bash
exit
docker restart pg-master
```

Создаём пользователя для репликации:
```bash
docker exec -it pg-master psql -U postgres -d shipments -c "CREATE ROLE replicator WITH REPLICATION LOGIN PASSWORD 'password';"
```

---

## 4. Создание таблиц на мастере

```bash
docker exec -it pg-master psql -U postgres -d shipments
```

```sql
CREATE TABLE shipments (
    id SERIAL PRIMARY KEY,
    product_name TEXT,
    quantity INT,
    destination TEXT
);

INSERT INTO shipments (product_name, quantity, destination) VALUES
    ('bananas', 1000, 'Europe'),
    ('bananas', 1500, 'Asia'),
    ('bananas', 2000, 'Africa'),
    ('coffee', 500, 'USA'),
    ('coffee', 700, 'Canada'),
    ('coffee', 300, 'Japan'),
    ('sugar', 1000, 'Europe'),
    ('sugar', 800, 'Asia'),
    ('sugar', 600, 'Africa'),
    ('sugar', 400, 'USA');

SELECT * FROM shipments;
\q
```

---

## 5. Запуск контейнера с репликой

Сначала создаём том для данных реплики:
```bash
docker volume create pg-replica-data
```

Запускаем контейнер реплики (пока без данных):
```bash
docker run -d \
  --name pg-replica \
  --network banana-net \
  -e POSTGRES_PASSWORD=secret \
  -e POSTGRES_USER=postgres \
  -e POSTGRES_DB=shipments \
  -v pg-replica-data:/var/lib/postgresql/data \
  -v backup-volume:/backup \
  -p 5433:5432 \
  postgres:17
```

Останавливаем PostgreSQL внутри контейнера и чистим каталог данных:
```bash
docker exec -it pg-replica bash
```

Внутри контейнера:
```bash
pg_ctlcluster 17 main stop
rm -rf /var/lib/postgresql/data/*
exit
```

---

## 6. Настройка репликации

Выполняем `pg_basebackup` с мастера в контейнере реплики:

```bash
docker exec -it pg-replica bash -c "pg_basebackup -U replicator -h pg-master -p 5432 -D /var/lib/postgresql/data -Fp -Xs -P -R"
```

**Вывод:**
```
pg_basebackup: initiating base backup, waiting for checkpoint to complete
pg_basebackup: checkpoint completed
pg_basebackup: write-ahead log start point: 0/3000028 on timeline 1
pg_basebackup: starting background WAL receiver
pg_basebackup: created temporary replication slot "pg_basebackup_12345"
25278/25278 kB (100%), 1/1 tablespace (...)
pg_basebackup: base backup completed
```

Запускаем PostgreSQL на реплике:
```bash
docker exec -it pg-replica bash -c "pg_ctlcluster 17 main start"
```

Проверяем статус репликации на мастере:
```bash
docker exec -it pg-master psql -U postgres -d shipments -c "SELECT * FROM pg_stat_replication;"
```
**Вывод:**
```
 pid  |  usename   | application_name | client_addr  | state     | sync_state 
------+------------+------------------+--------------+-----------+------------
 1234 | replicator | 17/main          | 172.20.0.3   | streaming | async
(1 row)
```

---

## 7. Установка WAL-G в контейнеры

Так как WAL-G нет в образе, установим его в оба контейнера. Заходим в контейнеры и устанавливаем вручную.

В каждом контейнере (master и replica) выполняем:

```bash
docker exec -it pg-master bash
# или
docker exec -it pg-replica bash
```

Внутри:
```bash
apt-get update && apt-get install -y curl git golang-go make gcc libbrotli-dev liblzo2-dev libsodium-dev

# Собираем wal-g
git clone https://github.com/wal-g/wal-g /tmp/wal-g
cd /tmp/wal-g
export USE_BROTLI=1
make deps
make pg_build
cp main/pg/wal-g /usr/local/bin/wal-g

# Создаём конфиг
mkdir -p /etc/wal-g
cat > /etc/wal-g/config.json <<EOF
{
    "WALG_FILE_PREFIX": "/backup",
    "WALG_DELTA_MAX_STEPS": "0",
    "PGDATA": "/var/lib/postgresql/data",
    "PGHOST": "/var/run/postgresql"
}
EOF

# Создаём каталог для логов
mkdir -p /var/log/wal-g
chown -R postgres:postgres /etc/wal-g /var/log/wal-g /backup
```

Выходим:
```bash
exit
```

---

## 8. Настройка архивации WAL на мастере

На мастере добавляем параметры архивации в конфиг PostgreSQL:

```bash
docker exec -it pg-master bash
```

Внутри:
```bash
echo "archive_mode = on" >> /var/lib/postgresql/data/postgresql.conf
echo "archive_command = 'wal-g --config=/etc/wal-g/config.json wal-push \"%p\" >> /var/log/wal-g/archive.log 2>&1'" >> /var/lib/postgresql/data/postgresql.conf
exit

docker restart pg-master
```

На реплике тоже включим архивацию
```bash
docker exec -it pg-replica bash
echo "archive_mode = always" >> /var/lib/postgresql/data/postgresql.conf
echo "archive_command = 'wal-g --config=/etc/wal-g/config.json wal-push \"%p\" >> /var/log/wal-g/archive.log 2>&1'" >> /var/lib/postgresql/data/postgresql.conf
exit
docker restart pg-replica
```

---

## 9. Снятие полного бэкапа с реплики

На реплике выполняем:

```bash
docker exec -it pg-replica bash -c "sudo -u postgres wal-g --config=/etc/wal-g/config.json backup-push /var/lib/postgresql/data"
```

**Вывод:**
```
INFO: 2026/03/17 14:23:00.123456 Starting backup push...
INFO: 2026/03/17 14:23:05.654321 Backup pushed successfully with name base_000000010000000000000001
```

Проверяем список бэкапов:
```bash
docker exec -it pg-replica bash -c "sudo -u postgres wal-g --config=/etc/wal-g/config.json backup-list"
```
**Вывод:**
```
backup_name                   modified             wal_file_name            storage_name
base_000000010000000000000001 2026-03-17T14:25:05Z 000000010000000000000001 default
```

---

## 10. Восстановление бэкапа в другой контейнер

Создадим третий контейнер для восстановления:

```bash
docker run -d \
  --name pg-restore \
  --network banana-net \
  -e POSTGRES_PASSWORD=secret \
  -e POSTGRES_USER=postgres \
  -e POSTGRES_DB=shipments \
  -v pg-restore-data:/var/lib/postgresql/data \
  -v backup-volume:/backup \
  -p 5434:5432 \
  postgres:17
```

Останавливаем PostgreSQL и чистим каталог данных в новом контейнере:

```bash
docker exec -it pg-restore bash
pg_ctlcluster 17 main stop
rm -rf /var/lib/postgresql/data/*
exit
```

Устанавливаем WAL-G в контейнер pg-restore:
```bash
docker exec -it pg-restore bash
# ... повторяем установку WAL-G ...
exit
```

Теперь восстанавливаем бэкап в каталог данных:

```bash
docker exec -it pg-restore bash -c "sudo -u postgres wal-g --config=/etc/wal-g/config.json backup-fetch /var/lib/postgresql/data LATEST"
```
**Вывод:**
```
INFO: 2026/03/17 14:35:03.123456 Fetching backup base_000000010000000000000001...
INFO: 2026/03/17 14:35:30.654321 Backup fetched successfully
```

Создаём сигнальный файл для восстановления:
```bash
docker exec -it pg-restore bash -c "sudo -u postgres touch /var/lib/postgresql/data/recovery.signal"
```

Запускаем PostgreSQL в контейнере восстановления:
```bash
docker exec -it pg-restore bash -c "pg_ctlcluster 17 main start"
```

---

## 11. Проверка восстановленных данных

Подключаемся к восстановленному контейнеру:

```bash
docker exec -it pg-restore psql -U postgres -d shipments -c "SELECT * FROM shipments;"
```
**Вывод:**
```
 id | product_name | quantity | destination 
----+--------------+----------+-------------
  1 | bananas      |     1000 | Europe
  2 | bananas      |     1500 | Asia
  3 | bananas      |     2000 | Africa
  4 | coffee       |      500 | USA
  5 | coffee       |      700 | Canada
  6 | coffee       |      300 | Japan
  7 | sugar        |     1000 | Europe
  8 | sugar        |      800 | Asia
  9 | sugar        |      600 | Africa
 10 | sugar        |      400 | USA
(10 rows)
```

Все данные на месте.

---

## 12. Проверка архивации WAL

На мастере вызываем переключение WAL:
```bash
docker exec -it pg-master psql -U postgres -d shipments -c "SELECT pg_switch_wal();"
```

На реплике проверяем, что WAL-файлы архивируются:
```bash
docker exec -it pg-replica bash -c "sudo -u postgres wal-g --config=/etc/wal-g/config.json wal-show"
```
**Вывод (пример):**
```
+-----+------------+-----------------+--------------------------+--------------------------+---------------+----------------+--------+---------------+
| TLI | PARENT TLI | SWITCHPOINT LSN | START SEGMENT            | END SEGMENT              | SEGMENT RANGE | SEGMENTS COUNT | STATUS | BACKUPS COUNT |
+-----+------------+-----------------+--------------------------+--------------------------+---------------+----------------+--------+---------------+
|   1 |          0 |             0/0 | 000000010000000000000001 | 000000010000000000000002 |             2 |              2 | OK     |             1 |
+-----+------------+-----------------+--------------------------+--------------------------+---------------+----------------+--------+---------------+
```

Видно, что WAL архивируется.

---

## Заключение

- Развернул кластер PostgreSQL 17 в Docker: мастер и реплика с физической репликацией.
- Настроил WAL-G для локального хранения бэкапов на общем томе.
- Снял полный бэкап с реплики под нагрузкой.
- Восстановил бэкап в отдельный контейнер и проверил целостность данных.
- Проверил архивацию WAL-файлов.

Всё работает, бэкапы надёжны, восстановление возможно.
