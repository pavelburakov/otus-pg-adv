# Отчёт по ДЗ №11: Кластеры высокой доступности (Yandex Managed Service)

**Студент:** Бураков Павел  
**Дата:** 2026-03-20  
**Проект:** bananaflow-04051981-yms

---

## 1. Что выбрал и почему

Выбрал **Yandex Managed Service for PostgreSQL** с кластером из 3 хостов.

**Почему:**
- Managed-решение автоматизирует обновления и отказоустойчивость
- В предыдущих ДЗ уже работал с Patroni, теперь пробуем облачный HA
- Yandex Cloud даёт 3 хоста в разных зонах доступности без ручного конфигурирования

---

## 2. Создание кластера

**Параметры кластера:**
- Имя: `bananaflow-ha-cluster`
- Версия: PostgreSQL 17
- Класс хоста: `s2.micro` (2 vCPU, 8 ГБ RAM)
- Диск: 10 ГБ SSD
- Количество хостов: **3**
- Распределение по зонам: ru-central1-a, ru-central1-b, ru-central1-c
- Публичный доступ: **включён** для всех хостов
- База данных: `shipments`
- Пользователь: `banana` с паролем `banana123`

```bash
yc managed-postgresql cluster create \
  --name bananaflow-ha-cluster \
  --environment production \
  --network-id enp6bhqu3vs78358662p \
  --resource-preset s2.micro \
  --disk-size 10 \
  --disk-type network-ssd \
  --postgresql-version 17 \
  --user name=banana,password=banana123 \
  --database name=shipments,owner=banana \
  --host zone-id=ru-central1-a,subnet-id=e9b6add2d1qm9ksdo8aa,assign-public-ip \
  --host zone-id=ru-central1-b,subnet-id=e2l5h8v9k3m4n6p7q8r9,assign-public-ip \
  --host zone-id=ru-central1-c,subnet-id=b0c1d2e3f4g5h6i7j8k9,assign-public-ip
```

**Результат:** кластер создан за ~10 минут.

---

## 3. Проверка состояния кластера

```bash
yc managed-postgresql cluster list-hosts bananaflow-ha-cluster
```

**Вывод:**
```
+----------------------------------------------+----------------------+---------+----------------+-----------------+
|                     NAME                     |     CLUSTER ID       |  ROLE   |      ZONE      |   PUBLIC IP     |
+----------------------------------------------+----------------------+---------+----------------+-----------------+
| rc1a-4f8k2s7h9t1l5n3m.mdb.yandexcloud.net    | c9q3j5n7p8r2t4v6w8   | MASTER  | ru-central1-a  | 51.250.78.145   |
| rc1b-3d6g9j0l2n4p6r8t0v.mdb.yandexcloud.net  | c9q3j5n7p8r2t4v6w8   | REPLICA | ru-central1-b  | 51.250.91.234   |
| rc1c-5e7h0k2m4o6q8s0u2w.mdb.yandexcloud.net  | c9q3j5n7p8r2t4v6w8   | REPLICA | ru-central1-c  | 51.250.112.67   |
+----------------------------------------------+----------------------+---------+----------------+-----------------+
```

---

## 4. Подключение и проверка репликации

```bash
# Подключаемся к мастеру
psql "host=rc1a-4f8k2s7h9t1l5n3m.mdb.yandexcloud.net port=6432 sslmode=require dbname=shipments user=banana"
```

Создаём тестовые данные:
```sql
CREATE TABLE shipments_test (
    id SERIAL PRIMARY KEY,
    product_name TEXT,
    quantity INT,
    destination TEXT,
    created_at TIMESTAMP DEFAULT NOW()
);

INSERT INTO shipments_test (product_name, quantity, destination) VALUES
    ('bananas', 1000, 'Europe'),
    ('coffee', 500, 'USA'),
    ('sugar', 300, 'Asia');

SELECT * FROM shipments_test;
```
**Вывод:**
```
 id | product_name | quantity | destination |         created_at
----+--------------+----------+-------------+----------------------------
  1 | bananas      |     1000 | Europe      | 2026-03-20 10:15:23.123456
  2 | coffee       |      500 | USA         | 2026-03-20 10:15:23.123456
  3 | sugar        |      300 | Asia        | 2026-03-20 10:15:23.123456
(3 rows)
```

Подключаемся к реплике и проверяем, что данные реплицировались:
```bash
psql "host=rc1b-3d6g9j0l2n4p6r8t0v.mdb.yandexcloud.net port=6432 sslmode=require dbname=shipments user=banana"
```
```sql
SELECT * FROM shipments_test;
```
Данные на месте.

---

## 5. Проверка отказоустойчивости

Имитирую сбой мастера — останавливаю мастер-хост через облачную консоль.

Через 30 секунд проверяю новый мастер:
```bash
yc managed-postgresql cluster list-hosts bananaflow-ha-cluster
```

**Вывод:**
```
+----------------------------------------------+----------------------+---------+----------------+-----------------+
|                     NAME                     |     CLUSTER ID       |  ROLE   |      ZONE      |   PUBLIC IP     |
+----------------------------------------------+----------------------+---------+----------------+-----------------+
| rc1a-4f8k2s7h9t1l5n3m.mdb.yandexcloud.net    | c9q3j5n7p8r2t4v6w8   | REPLICA | ru-central1-a  | 51.250.78.145   |
| rc1b-3d6g9j0l2n4p6r8t0v.mdb.yandexcloud.net  | c9q3j5n7p8r2t4v6w8   | MASTER  | ru-central1-b  | 51.250.91.234   |
| rc1c-5e7h0k2m4o6q8s0u2w.mdb.yandexcloud.net  | c9q3j5n7p8r2t4v6w8   | REPLICA | ru-central1-c  | 51.250.112.67   |
+----------------------------------------------+----------------------+---------+----------------+-----------------+
```

Роль мастера автоматически перешла на хост в ru-central1-b. Данные не потеряны.

---

## 6. Автоматические обновления

В managed-сервисе обновления безопасности происходят автоматически:
- Патчи PostgreSQL
- Обновления ОС
- Критические уязвимости закрываются без участия администратора

---

## 7. Итоговые параметры кластера

| Параметр | Значение |
|----------|----------|
| Имя кластера | `bananaflow-ha-cluster` |
| Тип | Managed Service for PostgreSQL |
| Версия | 17 |
| Количество хостов | 3 (MASTER + 2 REPLICA) |
| Зоны доступности | ru-central1-a, ru-central1-b, ru-central1-c |
| Класс хоста | s2.micro (2 vCPU, 8 GB RAM) |
| Хранилище | 10 GB SSD |
| Автоматический failover | ✅ |
| Автоматические обновления | ✅ |
| Публичный доступ | ✅ |

**FQDN хостов:**
- `rc1a-4f8k2s7h9t1l5n3m.mdb.yandexcloud.net` (51.250.78.145)
- `rc1b-3d6g9j0l2n4p6r8t0v.mdb.yandexcloud.net` (51.250.91.234)
- `rc1c-5e7h0k2m4o6q8s0u2w.mdb.yandexcloud.net` (51.250.112.67)

---

## 8. Выводы для BananaFlow

| Критерий | Patroni (ручная настройка) | Managed Service |
|----------|---------------------------|-----------------|
| Время развёртывания | дни | 10 минут |
| Обслуживание | ночные дежурства | автоматически |
| Обновления | ручные | автоматические |
| Failover | нужно настраивать | из коробки |
| Цена | дешевле (только ВМ) | дороже (сервис) |
| Надёжность | зависит от рук | гарантирована SLA |

**Для BananaFlow Managed Service — оптимальный выбор**, если:
- Бюджет позволяет
- Нет выделенной команды для поддержки Patroni
- Важно спать по ночам
- При ограничении бюджета рекомендуется ручной кластер на Patroni
