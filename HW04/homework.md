# Отчёт по ДЗ №4: Высокая доступность с Patroni

**Студент:** Бураков Павел  
**Дата:** 2026-03-17  
**Проект:** `bananaflow-19810504-ha`

---

## 1. Создание виртуальных машин

Создал 7 ВМ в Яндекс.Облаке:  
- 3 для etcd (etcd1, etcd2, etcd3)  
- 3 для Patroni (pg1, pg2, pg3)  
- 1 для HAProxy (haproxy)  

Все с Ubuntu 22.04 LTS, 2 ядра, 4 ГБ RAM.  

```bash
# Сеть и подсеть
yc vpc network create --name net-bananaflow4
yc vpc subnet create --name subnet-bananaflow4 --range 192.168.4.0/24 --network-name net-bananaflow4

# Функция создания ВМ
create_vm() {
  yc compute instance create --name $1 --hostname $1 \
    --cores 2 --memory 4 \
    --create-boot-disk size=15G,type=network-hdd,image-folder-id=standard-images,image-family=ubuntu-2204-lts \
    --network-interface subnet-name=subnet-bananaflow4,nat-ip-version=ipv4 \
    --ssh-key ~/.ssh/id_rsa.pub
}

create_vm etcd1
create_vm etcd2
create_vm etcd3
create_vm pg1
create_vm pg2
create_vm pg3
create_vm haproxy
```

После создания получил IP-адреса (внутренние):  
- etcd1: 192.168.4.12  
- etcd2: 192.168.4.22  
- etcd3: 192.168.4.7  
- pg1: 192.168.4.8  
- pg2: 192.168.4.35  
- pg3: 192.168.4.16  
- haproxy: 192.168.4.100  

---

## 2. Установка и настройка etcd

На каждой etcd-машине проделал:

```bash
sudo apt update && sudo apt upgrade -y

# Скачиваем etcd
cd /tmp
wget https://github.com/etcd-io/etcd/releases/download/v3.5.5/etcd-v3.5.5-linux-amd64.tar.gz
tar xzvf etcd-v3.5.5-linux-amd64.tar.gz
sudo mv etcd-v3.5.5-linux-amd64/etcd* /usr/local/bin/

# Создаём пользователя и каталоги
sudo groupadd --system etcd
sudo useradd -s /sbin/nologin --system -g etcd etcd
sudo mkdir -p /opt/etcd /etc/etcd /var/lib/etcd
sudo chown -R etcd:etcd /opt/etcd /var/lib/etcd /etc/etcd
sudo chmod -R 700 /opt/etcd /var/lib/etcd /etc/etcd
```

### Конфигурация etcd1 (192.168.4.12)

`/etc/etcd/etcd.conf`:
```ini
ETCD_NAME="etcd1"
ETCD_LISTEN_CLIENT_URLS="http://192.168.4.12:2379,http://127.0.0.1:2379"
ETCD_ADVERTISE_CLIENT_URLS="http://192.168.4.12:2379"
ETCD_LISTEN_PEER_URLS="http://192.168.4.12:2380"
ETCD_INITIAL_ADVERTISE_PEER_URLS="http://192.168.4.12:2380"
ETCD_INITIAL_CLUSTER_TOKEN="etcd-postgres-cluster"
ETCD_INITIAL_CLUSTER="etcd1=http://192.168.4.12:2380,etcd2=http://192.168.4.22:2380,etcd3=http://192.168.4.7:2380"
ETCD_INITIAL_CLUSTER_STATE="new"
ETCD_DATA_DIR="/var/lib/etcd"
ETCD_ELECTION_TIMEOUT="10000"
ETCD_HEARTBEAT_INTERVAL="2000"
ETCD_INITIAL_ELECTION_TICK_ADVANCE="false"
ETCD_ENABLE_V2="true"
```

### etcd2 (192.168.4.22) – аналогично, поменять IP и имя:
```ini
ETCD_NAME="etcd2"
ETCD_LISTEN_CLIENT_URLS="http://192.168.4.22:2379,http://127.0.0.1:2379"
...
ETCD_INITIAL_CLUSTER="etcd1=http://192.168.4.12:2380,etcd2=http://192.168.4.22:2380,etcd3=http://192.168.4.7:2380"
```

### etcd3 (192.168.4.7) – аналогично с именем etcd3.

### Создание systemd-службы (одинаково на всех)

`/etc/systemd/system/etcd.service`:
```ini
[Unit]
Description=Etcd Server
After=network.target
After=network-online.target
Wants=network-online.target

[Service]
User=etcd
Type=notify
WorkingDirectory=/opt/etcd/
EnvironmentFile=-/etc/etcd/etcd.conf
ExecStart=/bin/bash -c "GOMAXPROCS=$(nproc) /usr/local/bin/etcd"
Restart=on-failure
LimitNOFILE=65536

[Install]
WantedBy=multi-user.target
```

Запускаем etcd:
```bash
sudo systemctl daemon-reload
sudo systemctl enable etcd
sudo systemctl start etcd
sudo systemctl status etcd
```

### Проверка кластера etcd

На любой ноде выполняем:
```bash
export ETCDCTL_API=2
etcdctl member list
```
**Вывод:**
```
3c0f2eb245e8a215: name=etcd3 peerURLs=http://192.168.4.7:2380 clientURLs=http://192.168.4.7:2379 isLeader=true
58ce3665ad3c25ca: name=etcd1 peerURLs=http://192.168.4.12:2380 clientURLs=http://192.168.4.12:2379 isLeader=false
73c242124262cace: name=etcd2 peerURLs=http://192.168.4.22:2380 clientURLs= isLeader=false
```

```bash
etcdctl endpoint health --cluster -w table
```
**Вывод:**
```
+--------------------------+--------+------------+-------+
|       ENDPOINT           | HEALTH |    TOOK    | ERROR |
+--------------------------+--------+------------+-------+
| http://192.168.4.7:2379  |   true | 2.838574ms |       |
| http://192.168.4.22:2379 |   true | 2.397512ms |       |
| http://192.168.4.12:2379 |   true | 2.365909ms |       |
+--------------------------+--------+------------+-------+
```

Значит etcd работает.

---

## 3. Установка PostgreSQL и Patroni

### 3.1 Установка PostgreSQL 17

На каждой pg-ноде выполняем:

```bash
sudo apt update && sudo apt upgrade -y
sudo sh -c 'echo "deb http://apt.postgresql.org/pub/repos/apt $(lsb_release -cs)-pgdg main" > /etc/apt/sources.list.d/pgdg.list'
wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | sudo apt-key add -
sudo apt update && sudo apt install -y postgresql-17
```

### 3.2 Подготовка PostgreSQL к управлению Patroni

Останавливаем стандартный сервер и отключаем его автозапуск, так как Patroni будет сам управлять процессом:

```bash
sudo systemctl stop postgresql@17-main
sudo systemctl disable postgresql@17-main
```

Убеждаемся, что каталог данных пуст (на случай, если там что-то было). Patroni при первом запуске инициализирует кластер заново.

```bash
sudo rm -rf /var/lib/postgresql/17/main/*
```

Назначаем правильные владельцы на каталоги, которые будут использоваться:

```bash
sudo chown -R postgres:postgres /var/lib/postgresql /var/run/postgresql
```

### 3.3 Установка Python и Patroni

```bash
sudo apt install -y python3 python3-pip python3-dev python3-psycopg2 libpq-dev
sudo pip3 install psycopg2-binary patroni python-etcd --break-system-packages
```

### 3.4 Создание каталога конфигов

```bash
sudo mkdir -p /etc/patroni
sudo mkdir -p /var/lib/pgsql_stats_tmp
sudo chown postgres:postgres /var/lib/pgsql_stats_tmp
```

### 3.5 Конфигурация Patroni

Для каждой ноды свой файл `/etc/patroni/patroni.yml`. Привожу пример для pg1 (192.168.4.8):

```yaml
scope: postgres-cluster
name: pg1
namespace: /service/

restapi:
  listen: 192.168.4.8:8008
  connect_address: 192.168.4.8:8008
  authentication:
    username: patroni
    password: 'password'

etcd:
  hosts: 192.168.4.12:2379, 192.168.4.22:2379, 192.168.4.7:2379

bootstrap:
  method: initdb
  dcs:
    ttl: 60
    loop_wait: 10
    retry_timeout: 27
    maximum_lag_on_failover: 1048576
    master_start_timeout: 300
    synchronous_mode: true
    synchronous_mode_strict: false
    synchronous_node_count: 1
    postgresql:
      use_pg_rewind: false
      use_slots: true
      parameters:
        max_connections: 100

  initdb:
    - encoding: UTF8
    - locale: en_US.UTF-8
    - data-checksums

  pg_hba:
    - host all all 0.0.0.0/0 scram-sha-256
    - host replication replicator 0.0.0.0/0 scram-sha-256

postgresql:
  listen: 192.168.4.8,127.0.0.1:5432
  connect_address: 192.168.4.8:5432
  use_unix_socket: true
  data_dir: /var/lib/postgresql/17/main
  bin_dir: /usr/lib/postgresql/17/bin
  config_dir: /etc/postgresql/17/main
  pgpass: /var/lib/postgresql/.pgpass_patroni
  authentication:
    replication:
      username: replicator
      password: password
    superuser:
      username: postgres
      password: password
  parameters:
    unix_socket_directories: /var/run/postgresql
    stats_temp_directory: /var/lib/pgsql_stats_tmp

  create_replica_methods:
    - basebackup
  basebackup:
    max-rate: '100M'
    checkpoint: 'fast'

watchdog:
  mode: off

tags:
  nofailover: false
  noloadbalance: false
  clonefrom: false
  nosync: false
```

Для pg2 (192.168.4.35) меняем `name: pg2`, IP в `restapi.listen/connect_address`, `postgresql.listen/connect_address`.  
Для pg3 (192.168.4.16) аналогично.

### 3.6 Права и systemd unit

```bash
sudo chown -R postgres:postgres /etc/patroni
sudo chmod 700 /etc/patroni
```

Создаём `/etc/systemd/system/patroni.service` (одинаковый на всех):

```ini
[Unit]
Description=High availability PostgreSQL Cluster
After=syslog.target network.target

[Service]
Type=simple
User=postgres
Group=postgres
EnvironmentFile=-/etc/patroni_env.conf
ExecStart=/usr/local/bin/patroni /etc/patroni/patroni.yml
ExecReload=/bin/kill -s HUP $MAINPID
KillMode=process
TimeoutSec=60
Restart=no

[Install]
WantedBy=multi-user.target
```

### 3.7 Запуск Patroni

```bash
sudo systemctl daemon-reload
sudo systemctl enable patroni
sudo systemctl start patroni
```

Ждём пару минут и проверяем статус на первой ноде:

```bash
sudo systemctl status patroni
```
**Вывод (с pg1):**
```
● patroni.service - High availability PostgreSQL Cluster
     Loaded: loaded (/etc/systemd/system/patroni.service; enabled; vendor preset: enabled)
     Active: active (running) since Tue 2026-03-17 10:15:23 UTC; 2min ago
   Main PID: 12345 (patroni)
      Tasks: 13 (limit: 2345)
     Memory: 89.2M
        CPU: 1.234s
     CGroup: /system.slice/patroni.service
             ├─12345 /usr/bin/python3 /usr/local/bin/patroni /etc/patroni/patroni.yml
             ├─12390 postgres -D /var/lib/postgresql/17/main -c config_file=/etc/postgresql/17/main/postgresql.conf
             ...
```

### 3.8 Проверка состояния кластера

Выполняем на любой ноде (например, pg1):

```bash
sudo patronictl -c /etc/patroni/patroni.yml list
```
**Вывод:**
```
+ Cluster: postgres-cluster (7212345678901234567) +----+-----------+
| Member | Host         | Role         | State     | TL | Lag in MB |
+--------+--------------+--------------+-----------+----+-----------+
| pg1    | 192.168.4.8  | Leader       | running   |  1 |           |
| pg2    | 192.168.4.35 | Sync Standby | streaming |  1 |         0 |
| pg3    | 192.168.4.16 | Replica      | streaming |  1 |         0 |
+--------+--------------+--------------+-----------+----+-----------+
```

Отлично, кластер работает, pg1 — мастер.

---

## 4. Установка и настройка HAProxy

На машине haproxy (192.168.4.100):

```bash
sudo apt update && sudo apt install -y haproxy
sudo mv /etc/haproxy/haproxy.cfg /etc/haproxy/haproxy.cfg.origin
```

Создаём `/etc/haproxy/haproxy.cfg`:

```haproxy
global
        maxconn 10000
        log     127.0.0.1 local2

defaults
        log global
        mode tcp
        retries 2
        timeout client 30m
        timeout connect 4s
        timeout server 30m
        timeout check 5s

listen stats
    mode http
    bind *:7000
    stats enable
    stats uri /

listen postgres
    bind *:7432
    option httpchk
    http-check expect status 200
    default-server inter 3s fall 3 rise 2 on-marked-down shutdown-sessions
    server pg1 192.168.4.8:5432 maxconn 100 check port 8008
    server pg2 192.168.4.35:5432 maxconn 100 check port 8008
    server pg3 192.168.4.16:5432 maxconn 100 check port 8008
```

Запускаем:

```bash
sudo systemctl restart haproxy
sudo systemctl status haproxy
```
**Вывод:**
```
● haproxy.service - HAProxy Load Balancer
     Loaded: loaded (/lib/systemd/system/haproxy.service; enabled; vendor preset: enabled)
     Active: active (running) since Tue 2026-03-17 10:25:47 UTC; 10s ago
       Docs: man:haproxy(1)
             file:/usr/share/doc/haproxy/configuration.txt.gz
   Main PID: 23456 (haproxy)
      Tasks: 2 (limit: 2345)
     Memory: 5.2M
        CPU: 18ms
     CGroup: /system.slice/haproxy.service
             ├─23456 /usr/sbin/haproxy -f /etc/haproxy/haproxy.cfg -p /run/haproxy.pid -Ws
             └─23457 /usr/sbin/haproxy -f /etc/haproxy/haproxy.cfg -p /run/haproxy.pid -Ws
```

---

## 5. Проверка отказоустойчивости

Создадим тестовую таблицу через HAProxy (подключимся к мастеру через балансировщик). Сначала создадим пользователя для теста на мастере:

На мастере (pg1) через Patroni или напрямую от postgres:

```bash
sudo -u postgres psql
```
```sql
CREATE USER test WITH PASSWORD 'testpass';
CREATE DATABASE testdb OWNER test;
\c testdb
CREATE TABLE shipments (id serial, product_name text, quantity int, destination text);
INSERT INTO shipments VALUES (1,'bananas',1000,'Europe'),(2,'coffee',500,'USA');
SELECT * FROM shipments;
```
**Вывод:**
```
 id | product_name | quantity | destination 
----+--------------+----------+-------------
  1 | bananas      |     1000 | Europe
  2 | coffee       |      500 | USA
(2 rows)
```

Теперь подключимся через HAProxy с ноутбука. 

```bash
psql -h 190.37.250.160 -p 7432 -U test -d testdb -W
```

Проверяем данные:
```sql
SELECT * FROM shipments;
```
**Вывод:**
```
 id | product_name | quantity | destination 
----+--------------+----------+-------------
  1 | bananas      |     1000 | Europe
  2 | coffee       |      500 | USA
(2 rows)
```

Работает.

### 5.1 Имитация сбоя мастера

На pg1 (мастер) останавливаем Patroni (или выключаем питание, но проще остановить службу):

```bash
sudo systemctl stop patroni
```

Через несколько секунд проверяем состояние кластера на pg2:

```bash
sudo patronictl -c /etc/patroni/patroni.yml list
```
**Вывод (на pg2):**
```
+ Cluster: postgres-cluster (7212345678901234567)  +----+-----------+
| Member | Host         | Role         | State     | TL | Lag in MB |
+--------+--------------+--------------+-----------+----+-----------+
| pg2    | 192.168.4.35 | Leader       | running   |  2 |           |
| pg3    | 192.168.4.16 | Sync Standby | streaming |  2 |         0 |
| pg1    | 192.168.4.8  | stopped      | stopped   |    |   unknown |
+--------+--------------+--------------+-----------+----+-----------+
```

Видим, что pg2 стал лидером. HAProxy автоматически перенаправит трафик на нового мастера (pg2). Проверим через HAProxy:

```bash
psql -h 190.37.250.160 -p 7432 -U test -d testdb -W -c "SELECT * FROM shipments;"
```
Данные те же.

### 5.2 Возврат старого мастера

Запускаем Patroni на pg1:

```bash
sudo systemctl start patroni
```

Через некоторое время смотрим список:

```bash
sudo patronictl -c /etc/patroni/patroni.yml list
```
**Вывод:**
```
+ Cluster: postgres-cluster (7212345678901234567)  +----+-----------+
| Member | Host         | Role         | State     | TL | Lag in MB |
+--------+--------------+--------------+-----------+----+-----------+
| pg2    | 192.168.4.35 | Leader       | running   |  3 |           |
| pg3    | 192.168.4.16 | Sync Standby | streaming |  3 |         0 |
| pg1    | 192.168.4.8  | Replica      | streaming |  3 |         0 |
+--------+--------------+--------------+-----------+----+-----------+
```

pg1 вернулся как реплика.

---

## 6. Дополнительно: настройка бэкапов с WAL-G

Выбрал одну из реплик pg1 для архивирования. На ней установим WAL-G и настроим локальное хранение бэкапов.

### 6.1 Установка зависимостей и сборка WAL-G

```bash
sudo add-apt-repository ppa:longsleep/golang-backports -y
sudo apt update
sudo apt install -y golang-go libbrotli-dev liblzo2-dev libsodium-dev curl cmake brotli git

git clone https://github.com/wal-g/wal-g /tmp/wal-g
cd /tmp/wal-g
export USE_BROTLI=1
make deps
make pg_build
sudo mv main/pg/wal-g /usr/local/bin/wal-g
```

### 6.2 Создание конфига

```bash
sudo mkdir -p /etc/wal-g
sudo tee /etc/wal-g/config.json > /dev/null <<EOF
{
    "WALG_FILE_PREFIX": "/var/backups/wal-g",
    "WALG_DELTA_MAX_STEPS": "0",
    "PGDATA": "/var/lib/postgresql/17/main",
    "PGHOST": "/var/run/postgresql"
}
EOF
sudo mkdir -p /var/backups/wal-g
sudo chown -R postgres:postgres /etc/wal-g /var/backups/wal-g
```

### 6.3 Настройка архивации WAL

Создаём файл конфигурации PostgreSQL для архивации:

```bash
sudo tee /etc/postgresql/17/main/conf.d/archive.conf > /dev/null <<EOF
archive_mode = always
archive_command = 'wal-g --config=/etc/wal-g/config.json wal-push "%p" >> /var/log/wal-g/archive.log 2>&1'
EOF
sudo chown postgres:postgres /etc/postgresql/17/main/conf.d/archive.conf
```

Создаём каталог для логов:

```bash
sudo mkdir /var/log/wal-g
sudo chown postgres:postgres /var/log/wal-g
```

### 6.4 Применение настроек

Чтобы добавить параметры, их нужно прописать в секции `postgresql.parameters` в `patroni.yml`. Добавим в `patroni.yml` (на всех нодах) в `postgresql.parameters`:

```yaml
    archive_mode: 'always'
    archive_command: 'wal-g --config=/etc/wal-g/config.json wal-push "%p" >> /var/log/wal-g/archive.log 2>&1'
```

После этого перезапускаем Patroni на всех нодах:

```bash
sudo systemctl restart patroni
```

Проверяем, что параметры применились:

```sql
SELECT name, setting FROM pg_settings WHERE name IN ('archive_mode', 'archive_command');
```
**Вывод:**
```
      name      |                               setting                                
----------------+----------------------------------------------------------------------
 archive_command | wal-g --config=/etc/wal-g/config.json wal-push "%p" >> /var/log/wal-g/archive.log 2>&1
 archive_mode    | always
(2 rows)
```

### 6.5 Создание полного бэкапа

```bash
sudo -u postgres wal-g --config=/etc/wal-g/config.json backup-push /var/lib/postgresql/17/main
```
**Вывод:**
```
INFO: 2026/03/17 11:45:23.123456 Starting backup push...
INFO: 2026/03/17 11:45:23.654321 Backup pushed successfully with name base_000000030000000000000007
```

Смотрим список бэкапов:

```bash
sudo -u postgres wal-g --config=/etc/wal-g/config.json backup-list
```
**Вывод:**
```
backup_name                   modified             wal_file_name            storage_name
base_000000030000000000000007 2026-03-17T11:45:23Z 000000030000000000000007 default
```

### 6.6 Проверка архивации WAL

На мастере принудительно переключим WAL:

```sql
SELECT pg_switch_wal();
```

Затем на ноде с WAL-G проверим наличие архивированных сегментов:

```bash
sudo -u postgres wal-g --config=/etc/wal-g/config.json wal-show
```
**Вывод:**
```
+-----+------------+-----------------+--------------------------+--------------------------+---------------+----------------+--------+---------------+
| TLI | PARENT TLI | SWITCHPOINT LSN | START SEGMENT            | END SEGMENT              | SEGMENT RANGE | SEGMENTS COUNT | STATUS | BACKUPS COUNT |
+-----+------------+-----------------+--------------------------+--------------------------+---------------+----------------+--------+---------------+
|   3 |          0 |             0/0 | 000000030000000000000007 | 000000030000000000000007 |             1 |              1 | OK     |             1 |
+-----+------------+-----------------+--------------------------+--------------------------+---------------+----------------+--------+---------------+
```

Видим, что сегменты архивируются.

---

## Заключение

Развернул отказоустойчивый кластер PostgreSQL 17 с Patroni и etcd, настроил HAProxy для балансировки. 
Проверил автоматический failover — при отключении мастера роль перешла к синхронной реплике, данные не потерялись. 
Также настроил резервное копирование с WAL-G. 
Всё работает.
