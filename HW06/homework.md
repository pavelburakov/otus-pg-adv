# Отчёт по ДЗ №6: Гонка за производительностью в Docker

**Студент:** Бураков Павел  
**Дата:** 2026-03-18  
**Проект:** `bananaflow-19810504-performance`

---

## 1. Подготовка окружения

Создаём сеть и том для данных:
```bash
docker network create banana-perf-net
docker volume create pg-perf-data
```

Запускаем контейнер с PostgreSQL 17 на лимитированных ресурсах (2 ядра, 2 ГБ RAM) для чистоты эксперимента:
```bash
docker run -d \
  --name pg-perf \
  --network banana-perf-net \
  --cpus=2 \
  --memory=2g \
  -e POSTGRES_PASSWORD=secret \
  -e POSTGRES_USER=postgres \
  -e POSTGRES_DB=test \
  -v pg-perf-data:/var/lib/postgresql/data \
  -p 5432:5432 \
  postgres:17
```

Проверяем:
```bash
docker ps
docker exec -it pg-perf psql -U postgres -d test -c "SELECT version();"
```

---

## 2. Установка pgbench и подготовка тестовых данных

pgbench уже встроен в образ, но нужно убедиться, что он доступен:
```bash
docker exec -it pg-perf pgbench --version
# pgbench (PostgreSQL) 17.x
```

Инициализируем тестовые данные с масштабным коэффициентом 40 (примерно 400 МБ):
```bash
docker exec -it pg-perf pgbench -i --scale=40 --foreign-keys -U postgres test
```
**Вывод:**
```
dropping old tables...
NOTICE:  table "pgbench_accounts" does not exist, skipping
NOTICE:  table "pgbench_branches" does not exist, skipping
NOTICE:  table "pgbench_history" does not exist, skipping
NOTICE:  table "pgbench_tellers" does not exist, skipping
creating tables...
generating data (client-side)...
vacuuming...
creating primary keys...
creating foreign keys...
done in 85.47 s (drop tables 0.00 s, create tables 0.02 s, client-side generate 52.18 s, vacuum 0.18 s, primary keys 25.04 s, foreign keys 8.05 s).
```

Создаём внутри контейнера директорию для результатов:
```bash
docker exec -it pg-perf mkdir -p /var/lib/postgresql/test_result
```

Создаём скрипт для тестирования внутри контейнера:
```bash
docker exec -it pg-perf bash -c "cat > /var/lib/postgresql/pgbench.sh << 'EOF'
#!/bin/bash
clients=\"1 10 20 50 100\"
t=60
dir=/var/lib/postgresql/test_result

for c in \$clients; do
    echo \"Testing with \$c clients...\"
    echo \"start test: \$(date +\"%Y.%m.%d_%H:%M:%S\")\" > \"\${dir}/pgbench_\${c}.txt\"
    pgbench -h localhost -p 5432 -U postgres test -c \$c -j \$c -T \$t >> \"\${dir}/pgbench_\${c}.txt\"
    echo \"stop test: \$(date +\"%Y.%m.%d_%H:%M:%S\")\" >> \"\${dir}/pgbench_\${c}.txt\"
done
EOF"

docker exec -it pg-perf chmod +x /var/lib/postgresql/pgbench.sh
```

---

## 3. Тест 1: Настройки по умолчанию

Запускаем тест:
```bash
docker exec -it pg-perf /var/lib/postgresql/pgbench.sh
```

Смотрим результаты:
```bash
docker exec -it pg-perf cat /var/lib/postgresql/test_result/pgbench_1.txt
```
**Вывод:**
```
start test: 2026.03.17_18:15:23
pgbench (17.2 (Debian 17.2-1.pgdg120+1))
transaction type: <builtin: TPC-B (sort of)>
scaling factor: 40
query mode: simple
number of clients: 1
number of threads: 1
maximum number of tries: 1
duration: 60 s
number of transactions actually processed: 9325
number of failed transactions: 0 (0.000%)
latency average = 6.434 ms
initial connection time = 5.948 ms
tps = 155.42
stop test: 2026.03.17_18:16:23
```

```bash
docker exec -it pg-perf cat /var/lib/postgresql/test_result/pgbench_10.txt
```
```
...
tps = 481.01
```

```bash
docker exec -it pg-perf cat /var/lib/postgresql/test_result/pgbench_50.txt
```
```
...
tps = 691.62
```

Собираем все результаты:

| Клиенты | TPS (средний) | Latency avg (ms) |
|---------|---------------|------------------|
| 1       | 155.42        | 6.43             |
| 10      | 481.01        | 20.79            |
| 20      | 620.62        | 32.23            |
| 50      | 691.62        | 72.29            |
| 100     | 666.73        | 149.99           |

---

## 4. Тест 2: Первый этап тюнинга (базовые настройки)

Заходим в контейнер и правим конфиг:
```bash
docker exec -it pg-perf bash
```

Внутри контейнера:
```bash
cat >> /var/lib/postgresql/data/postgresql.conf << EOF

# Базовые оптимизации
shared_buffers = 256MB
effective_cache_size = 1512MB
work_mem = 10MB
effective_io_concurrency = 200
random_page_cost = 1.1
EOF

exit
```

Перезапускаем контейнер:
```bash
docker restart pg-perf
```

Ждём 10 секунд и запускаем тест снова:
```bash
docker exec -it pg-perf /var/lib/postgresql/pgbench.sh
```

Смотрим результаты (для 50 клиентов):
```bash
docker exec -it pg-perf cat /var/lib/postgresql/test_result/pgbench_50.txt
```
```
...
tps = 725.24
```

Сводка после базового тюнинга:

| Клиенты | TPS (средний) | Latency avg (ms) | Прирост |
|---------|---------------|------------------|---------|
| 1       | 164.07        | 6.10             | +5.6%   |
| 10      | 593.16        | 16.86            | +23.3%  |
| 20      | 678.39        | 29.48            | +9.3%   |
| 50      | 725.24        | 68.94            | +4.9%   |
| 100     | 716.04        | 139.66           | +7.4%   |

---

## 5. Тест 3: Экстремальный тюнинг

Снова заходим в контейнер и добавляем настройки, жертвующие надёжностью:
```bash
docker exec -it pg-perf bash
```

```bash
cat >> /var/lib/postgresql/data/postgresql.conf << EOF

# Экстремальные настройки 
synchronous_commit = off
wal_level = minimal
max_wal_senders = 0
fsync = off
full_page_writes = off

# Дополнительные оптимизации
effective_io_concurrency = 300
seq_page_cost = 0.5
random_page_cost = 0.6
parallel_setup_cost = 10.0
default_statistics_target = 1000
EOF

exit
```

Перезапускаем:
```bash
docker restart pg-perf
```

Запускаем тест:
```bash
docker exec -it pg-perf /var/lib/postgresql/pgbench.sh
```

Смотрим результаты для 10 клиентов:
```bash
docker exec -it pg-perf cat /var/lib/postgresql/test_result/pgbench_10.txt
```
```
start test: 2026.03.17_19:45:12
pgbench (17.2 (Debian 17.2-1.pgdg120+1))
transaction type: <builtin: TPC-B (sort of)>
scaling factor: 40
query mode: simple
number of clients: 10
number of threads: 10
duration: 60 s
number of transactions actually processed: 70219
number of failed transactions: 0 (0.000%)
latency average = 8.550 ms
tps = 1169.56
stop test: 2026.03.17_19:46:12
```

Сводка после экстремального тюнинга:

| Клиенты | TPS (средний) | Latency avg (ms) | Прирост от базового |
|---------|---------------|------------------|---------------------|
| 1       | 1110.00       | 0.90             | +614%               |
| 10      | 1169.56       | 8.55             | +143%               |
| 20      | 1062.66       | 18.82            | +71%                |
| 50      | 977.12        | 51.17            | +41%                |
| 100     | 922.68        | 108.38           | +38%                |

---

## 6. Сравнительная таблица (все результаты)

| Конфигурация | 1 client | 10 clients | 20 clients | 50 clients | 100 clients |
|--------------|----------|------------|------------|------------|-------------|
| **Default**  | 155 TPS  | 481 TPS    | 621 TPS    | 692 TPS    | 667 TPS     |
| **Basic tune** | 164 TPS | 593 TPS    | 678 TPS    | 725 TPS    | 716 TPS     |
| **Extreme**  | 1110 TPS | 1170 TPS   | 1063 TPS   | 977 TPS    | 923 TPS     |


---

## 7. Выводы

- **Базовые настройки** дают умеренный прирост (5-23%) и безопасны для продакшена.
- **Экстремальные настройки** дают колоссальный прирост на малом количестве клиентов:
  - `fsync = off` — риск потери данных при сбое питания
  - `synchronous_commit = off` — риск потери последних транзакций
  - `wal_level = minimal` — невозможность репликации и PITR
  - `full_page_writes = off` — риск повреждения страниц при сбое
- Оптимальное количество клиентов для данной конфигурации — **10-20**, дальше TPS падает из-за конкуренции.
- Максимальный TPS достигнут при **10 клиентах** с экстремальными настройками — **1170 TPS**.
- Docker отлично подходит для таких экспериментов: легко менять конфиги, перезапускать, не бояться сломать систему.
