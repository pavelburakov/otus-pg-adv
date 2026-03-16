# Отчёт по ДЗ №2: PostgreSQL в Docker 

**Студент:** Бураков Павел  
**Дата:** 2026-03-16  
**Проект:** `bananaflow-19810504-docker`

---

## 1. Создание ВМ в Яндекс.Облаке
```bash
yc vpc network create --name net-bananaflow2 --description "For bananaflow docker"
yc vpc subnet create --name subnet-bananaflow2 --range 192.168.2.0/24 --network-name net-bananaflow2
yc compute instance create --name bananaflow-docker --hostname docker-vm \
  --cores 2 --memory 4 \
  --create-boot-disk size=15G,type=network-hdd,image-folder-id=standard-images,image-family=ubuntu-2004-lts \
  --network-interface subnet-name=subnet-bananaflow2,nat-ip-version=ipv4 \
  --ssh-key ~/.ssh/id_rsa.pub
```

## 2. Подключение по SSH
```bash
IP=$(yc compute instance show --name bananaflow-docker | grep -E ' +address' | tail -1 | awk '{print $2}')
ssh yc-user@$IP
```

## 3. Установка Docker Engine
```bash
sudo apt update && sudo apt install -y apt-transport-https ca-certificates curl software-properties-common
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /usr/share/keyrings/docker-archive-keyring.gpg
echo "deb [arch=amd64 signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
sudo apt update && sudo apt install -y docker-ce
sudo usermod -aG docker $USER
newgrp docker
```

## 4. Создание каталога для данных PostgreSQL
```bash
sudo mkdir -p /var/lib/postgres
sudo chown -R $USER:$USER /var/lib/postgres
```

## 5. Запуск контейнера с PostgreSQL 16
```bash
docker run -d \
  --name pg-server \
  -e POSTGRES_PASSWORD=secret \
  -e POSTGRES_USER=banana \
  -e POSTGRES_DB=shipments \
  -v /var/lib/postgres:/var/lib/postgresql/data \
  -p 5432:5432 \
  postgres:16
```
Проверка: `docker ps`

## 6. Запуск контейнера с клиентом PostgreSQL
```bash
docker network create banana-net
docker network connect banana-net pg-server
docker run -it --rm --name pg-client --network banana-net postgres:16 psql -h pg-server -U banana -d shipments
```
Пароль: `secret`

## 7. Создание таблицы и вставка данных
В консоли psql (клиент) выполняем:
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

## 8. Подключение к серверу с ноутбука
На хосте ВМ установим клиент PostgreSQL:
```bash
sudo apt install -y postgresql-client
```
Подключаемся к localhost:
```bash
psql -h localhost -p 5432 -U banana -d shipments
```
Проверяем данные:
```sql
SELECT * FROM shipments;
\q
```

## 9. Удаление контейнера сервера и создание заново
```bash
docker stop pg-server
docker rm pg-server
# Проверяем, что данные в /var/lib/postgres остались
ls /var/lib/postgres
```
Создаём контейнер заново с тем же томом (версия 16):
```bash
docker run -d \
  --name pg-server-new \
  -e POSTGRES_PASSWORD=secret \
  -e POSTGRES_USER=banana \
  -e POSTGRES_DB=shipments \
  -v /var/lib/postgres:/var/lib/postgresql/data \
  -p 5432:5432 \
  postgres:16
```

## 10. Проверка сохранения данных
Подключаемся через клиент с хоста:
```bash
psql -h localhost -p 5432 -U banana -d shipments
```
```sql
SELECT * FROM shipments;   -- все 10 записей на месте
\q
```
Либо через контейнер-клиент:
```bash
docker run -it --rm --network host postgres:16 psql -h localhost -U banana -d shipments
```

## 11. Удаление ресурсов
```bash
docker stop pg-server-new && docker rm pg-server-new
yc compute instance delete bananaflow-docker
yc vpc subnet delete subnet-bananaflow2
yc vpc network delete net-bananaflow2
```

---

**Выводы:**  
- Данные сохранились после пересоздания контейнера благодаря тому.  
- Подключение с хоста работает через опубликованный порт.  
- Docker удобен для быстрого развёртывания тестовых БД.
