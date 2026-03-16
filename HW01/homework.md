# Отчёт по ДЗ: Настройка PostgreSQL и транзакции

**Студент:** Бураков Павел  
**Дата:** 2026-03-16  
**Проект:** `bananaflow-19810504`

---

## 1. Создание ВМ в Яндекс.Облаке
```bash
yc vpc network create --name net-bananaflow --description "For bananaflow"
yc vpc subnet create --name subnet-bananaflow --range 192.168.1.0/24 --network-name net-bananaflow
yc compute instance create --name bananaflow-19810504 --hostname bananaflow-vm \
  --cores 2 --memory 4 \
  --create-boot-disk size=15G,type=network-hdd,image-folder-id=standard-images,image-family=ubuntu-2404-lts \
  --network-interface subnet-name=subnet-bananaflow,nat-ip-version=ipv4 \
  --ssh-key ~/.ssh/id_rsa.pub
```

## 2. Подключение по SSH (две сессии)
```bash
# Получаем IP
IP=$(yc compute instance show --name bananaflow-19810504 | grep -E ' +address' | tail -1 | awk '{print $2}')
# Сессия 1
ssh yc-user@$IP
# Сессия 2 (в другом окне)
ssh yc-user@$IP
```

## 3. Установка PostgreSQL
```bash
sudo apt update && sudo apt upgrade -y
# Добавляем репозиторий PostgreSQL
sudo sh -c 'echo "deb http://apt.postgresql.org/pub/repos/apt $(lsb_release -cs)-pgdg main" > /etc/apt/sources.list.d/pgdg.list'
wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | sudo apt-key add -
sudo apt update && sudo apt install -y postgresql
```

## 4. Запуск psql и отключение autocommit
В обеих сессиях:
```bash
sudo -u postgres psql
```
```sql
\set AUTOCOMMIT off
\echo :AUTOCOMMIT   -- должно показать 0
```

## 5. Создание таблицы shipments (сессия 1)
```sql
CREATE TABLE shipments (
    id SERIAL PRIMARY KEY,
    product_name TEXT,
    quantity INTEGER,
    destination TEXT
);

INSERT INTO shipments (product_name, quantity, destination) VALUES
    ('bananas', 1000, 'Europe'),
    ('coffee', 500, 'USA');

COMMIT;
```
Проверка в сессии 2:
```sql
SELECT * FROM shipments;
-- Видны обе записи.
```

## 6. Эксперимент с Read Committed (уровень по умолчанию)
```sql
-- Сессия 1 и 2
BEGIN;  -- начало транзакции
```

**Сессия 1:**
```sql
INSERT INTO shipments (product_name, quantity, destination) VALUES ('sugar', 300, 'Asia');
```

**Сессия 2:**
```sql
SELECT * FROM shipments;
-- Новая запись не видна (грязное чтение запрещено).
```

**Сессия 1:** `COMMIT;`

**Сессия 2:** снова `SELECT * FROM shipments;`
```sql
-- Запись видна (фантомное чтение возможно в read committed).
COMMIT;
```

## 7. Эксперимент с Repeatable Read
```sql
-- Сессия 1 и 2
BEGIN;
SET TRANSACTION ISOLATION LEVEL REPEATABLE READ;
```

**Сессия 1:**
```sql
INSERT INTO shipments (product_name, quantity, destination) VALUES ('bananas', 2000, 'Africa');
```

**Сессия 2:**
```sql
SELECT * FROM shipments;
-- Не видно (грязное чтение запрещено).
```

**Сессия 1:** `COMMIT;`

**Сессия 2:** снова `SELECT * FROM shipments;`
```sql
-- Всё ещё не видно (repeatable read не видит изменений после начала транзакции).
```

**Сессия 2:** `COMMIT;` и затем `SELECT * FROM shipments;`
```sql
-- Теперь видно (новая транзакция видит все закоммиченные данные).
```

## 8. Дополнительно: неповторяемое чтение в Repeatable Read
```sql
-- Сессия 1
BEGIN;
SELECT * FROM shipments WHERE id=1;  -- допустим, amount=1000

-- Сессия 2
BEGIN;
SET TRANSACTION ISOLATION LEVEL REPEATABLE READ;
SELECT * FROM shipments WHERE id=1;  -- тоже 1000

-- Сессия 1
UPDATE shipments SET quantity=1200 WHERE id=1;
COMMIT;

-- Сессия 2
SELECT * FROM shipments WHERE id=1;  -- всё ещё 1000 (неповторяемое чтение отсутствует)
UPDATE shipments SET quantity=1200 WHERE id=1;  -- ошибка: could not serialize access
ROLLBACK;
```

## 9. Удаление ресурсов
```bash
yc compute instance delete bananaflow-19810504
yc vpc subnet delete subnet-bananaflow
yc vpc network delete net-bananaflow
```

---

**Выводы:**  
- В `read committed` видим новые данные сразу после коммита другой транзакции (фантомы возможны).  
- В `repeatable read` транзакция видит снимок на момент начала, даже после чужих коммитов (фантомов нет).  
- Конфликты обновления в `repeatable read` приводят к ошибке сериализации.
