# Отчёт по ДЗ №8: Managed Service for PostgreSQL

**Студент:** Бураков Павел  
**Дата:** 2026-03-19
**Проект:** bananaflow-19810504-yc


---

## 1. Создание кластера через YC CLI

```bash
# Установка и инициализация CLI 
curl -sSL https://storage.yandexcloud.net/yandexcloud-yc/install.sh | bash
yc init

# Создание кластера
yc managed-postgresql cluster create \
  --name bananaflow-mdb \
  --environment production \
  --network-id enp6bhqu3vs78358789p \
  --resource-preset s2.micro \
  --host zone-id=ru-central1-a,subnet-id=e9b6add2d1qm9ksdo9oa,assign-public-ip \
  --disk-type network-ssd \
  --disk-size 10 \
  --user name=banana,password=banana123 \
  --database name=shipments,owner=banana \
  --postgresql-version 17 --async
```

---

## 2. Подключение к кластеру

```bash
# SSL-сертификат
mkdir -p ~/.postgresql && \
wget "https://storage.yandexcloud.net/cloud-certs/CA.pem" \
     --output-document ~/.postgresql/root.crt && \
chmod 0600 ~/.postgresql/root.crt

# Подключение
psql "host=rc1a-lsdf3lot6k76j8la8.mdb.yandexcloud.net port=6432 sslmode=verify-full dbname=shipments user=banana"
```

---

## 3. Проверка

```sql
SELECT version();
```

**Вывод:**
```
PostgreSQL 17.5 (Ubuntu 17.5-201-yandex.59510.7fea32f73d) on x86_64-pc-linux-gnu
```

---

## 4. Параметры кластера

| Параметр | Значение |
|----------|----------|
| Версия | 17 |
| Класс | s2.micro (1 vCPU, 2 GB RAM) |
| Диск | 10 GB SSD |
| База | shipments |
| Пользователь | banana |
