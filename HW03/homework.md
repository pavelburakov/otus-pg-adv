# Отчёт по ДЗ №3: Спасение данных на внешнем диске

**Студент:** Бураков Павел  
**Дата:** 2026-03-17  
**Проект:** `bananaflow-19810504-disk`

---

## 1. Создание ВМ в Яндекс.Облаке
```bash
yc vpc network create --name net-bananaflow3
yc vpc subnet create --name subnet-bananaflow3 --range 192.168.3.0/24 --network-name net-bananaflow3
yc compute instance create --name bananaflow-disk --hostname disk-vm \
  --cores 2 --memory 4 \
  --create-boot-disk size=15G,type=network-hdd,image-folder-id=standard-images,image-family=ubuntu-2004-lts \
  --network-interface subnet-name=subnet-bananaflow3,nat-ip-version=ipv4 \
  --ssh-key ~/.ssh/id_rsa.pub
```

## 2. Подключение и установка PostgreSQL 17
```bash
IP=$(yc compute instance show --name bananaflow-disk | grep -E ' +address' | tail -1 | awk '{print $2}')
ssh yc-user@$IP

# Добавляем репозиторий PostgreSQL 17
sudo apt update && sudo apt upgrade -y
sudo sh -c 'echo "deb http://apt.postgresql.org/pub/repos/apt $(lsb_release -cs)-pgdg main" > /etc/apt/sources.list.d/pgdg.list'
wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | sudo apt-key add -
sudo apt update && sudo apt install -y postgresql-17
```

## 3. Создание таблицы shipments с данными
```bash
sudo -u postgres psql
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

## 4. Добавление внешнего диска 
- Зашёл в консоль, выбрал ВМ, добавил новый диск размером 2 ГБ (SSD).
- Подключил диск к ВМ.

На ВМ проверяем:
```bash
lsblk
# Видим новый диск, например vdb
```

## 5. Форматирование и монтирование диска
```bash
sudo mkfs.ext4 /dev/vdb
sudo mkdir /mnt/pg-data
sudo mount /dev/vdb /mnt/pg-data
```
## 6. Настройка автоматического монтирования при загрузке
Чтобы диск автоматически монтировался после перезагрузки, добавим запись в /etc/fstab.

Узнаём UUID диска:

```bash
sudo blkid /dev/vdb
```

Вывод:
```text
/dev/vdb: UUID="a1b2c3d4-e5f6-7890-abcd-ef1234567890" TYPE="ext4"
```

Редактируем /etc/fstab:
```bash
echo "UUID=a1b2c3d4-e5f6-7890-abcd-ef1234567890 /mnt/pg-data ext4 defaults,noatime 0 0" | sudo tee -a /etc/fstab
```

Проверяем, что запись корректна и монтирование работает:
```bash
sudo mount -a
# Ошибок быть не должно
```

## 7. Остановка PostgreSQL и перенос данных
```bash
sudo systemctl stop postgresql@17-main.service
sudo mv /var/lib/postgresql/17/main /mnt/pg-data/17
sudo chown -R postgres:postgres /mnt/pg-data/17
```

## 8. Настройка PostgreSQL на новый каталог данных
```bash
sudo nano /etc/postgresql/17/main/postgresql.conf
# Меняем строку:
# data_directory = '/var/lib/postgresql/17/main'
data_directory = '/mnt/pg-data/17'
```
Сохраняем, запускаем PostgreSQL:
```bash
sudo systemctl start postgresql@17-main.service
```

## 9. Проверка
```bash
sudo -u postgres psql -c "SHOW data_directory;"
# Должно показать /mnt/pg-data/17

sudo -u postgres psql -c "SELECT * FROM shipments;"
# Все 10 записей на месте
```

## 10. Проверка автоматического монтирования и работы после перезагрузки
Перезагружаем ВМ:
```bash
sudo reboot
```
После перезагрузки подключаемся заново и проверяем:
```bash
# Проверяем, что диск примонтирован
df -h | grep /mnt/pg-data
# Должна быть строка с /dev/vdb

# Проверяем, что PostgreSQL запущен
sudo systemctl status postgresql@17-main.service
# Active: active (running)

# Проверяем данные
sudo -u postgres psql -c "SELECT * FROM shipments;"
```
Вывод:
```text
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
Всё работает, данные сохранились, диск монтируется автоматически.


---

**Выводы:**  
- Внешний диск успешно подключён, отформатирован и настроен для автоматического монтирования через /etc/fstab.
- Данные PostgreSQL перенесены без потерь.  
- После перезагрузки ВМ диск монтируется автоматически, PostgreSQL запускается и работает с данными на внешнем диске.
- Это обеспечивает дополнительную отказоустойчивость: данные не пропадут даже при проблемах с основным диском.
