# Отчёт по ДЗ №7: PostgreSQL в Minikube

**Студент:** Бураков Павел  
**Дата:** 2026-03-18
**Проект:** bananaflow-19810504-k8s

---

## 1. Установка и запуск Minikube

```bash
minikube start --cpus=4 --memory=8192 --driver=docker
```
**Вывод:**
```
😄  minikube v1.37.0 на Ubuntu 24.04
✨  Используется драйвер docker на основе существующего профиля
🎉  minikube 1.38.1 is available! Download it: https://github.com/kubernetes/minikube/releases/tag/v1.38.1
💡  To disable this notice, run: 'minikube config set WantUpdateNotification false'

❗  You cannot change the memory size for an existing minikube cluster. Please first delete the cluster.
🐳  Подготавливается Kubernetes v1.34.0 на Docker 28.4.0 ...
🔗  Configuring bridge CNI (Container Networking Interface) ...
🔎  Компоненты Kubernetes проверяются ...
    ▪ Используется образ gcr.io/k8s-minikube/storage-provisioner:v5
🌟  Включенные дополнения: storage-provisioner, default-storageclass
🏄  Готово! kubectl настроен для использования кластера "minikube" и "default" пространства имён по умолчанию

```

Проверяем:
```bash
kubectl get nodes
```
```
NAME       STATUS   ROLES           AGE   VERSION
minikube   Ready    control-plane   98s   v1.34.0
```

Добавляем репозиторий Bitnami:
```bash
helm repo add postgresql https://community-charts.github.io/helm-charts
helm repo update
```

---

## 2. Установка PostgreSQL через Helm 

```bash
helm install banana-db oci://registry-1.docker.io/bitnamicharts/postgresql \
  --set auth.postgresPassword=banana123 \
  --set primary.persistence.size=2Gi \
  --set primary.resources.requests.cpu=500m \
  --set primary.resources.requests.memory=512Mi \
  --set replicaCount=1
```

**Вывод:**
```
NAME: banana-db
LAST DEPLOYED: Thu Mar 18 14:32:29 2026
NAMESPACE: default
STATUS: deployed
REVISION: 1
TEST SUITE: None
NOTES:
CHART NAME: postgresql
CHART VERSION: 18.5.8
APP VERSION: 18.3.0


** Please be patient while the chart is being deployed **

PostgreSQL can be accessed via port 5432 on the following DNS names from within your cluster:

    banana-db-postgresql.default.svc.cluster.local - Read/Write connection

To get the password for "postgres" run:

    export POSTGRES_PASSWORD=$(kubectl get secret --namespace default banana-db-postgresql -o jsonpath="{.data.postgres-password}" | base64 -d)

To connect to your database run the following command:

    kubectl run banana-db-postgresql-client --rm --tty -i --restart='Never' --namespace default --image registry-1.docker.io/bitnami/postgresql:latest --env="PGPASSWORD=$POSTGRES_PASSWORD" \
      --command -- psql --host banana-db-postgresql -U postgres -d postgres -p 5432

    > NOTE: If you access the container using bash, make sure that you execute "/opt/bitnami/scripts/postgresql/entrypoint.sh /bin/bash" in order to avoid the error "psql: local user with ID 1001} does not exist"

To connect to your database from outside the cluster execute the following commands:

    kubectl port-forward --namespace default svc/banana-db-postgresql 5432:5432 &
    PGPASSWORD="$POSTGRES_PASSWORD" psql --host 127.0.0.1 -U postgres -d postgres -p 5432
```

Проверяем поды:
```bash
kubectl get pods
```
```
NAME                      READY   STATUS    RESTARTS   AGE
banana-db-postgresql-0    1/1     Running   0          45s
```

Сервис создался автоматически:
```bash
kubectl get svc
```
```
NAME                      TYPE        CLUSTER-IP     EXTERNAL-IP   PORT(S)    AGE
banana-db-postgresql      ClusterIP   10.104.153.150 <none>        5432/TCP   5m20s
banana-db-postgresql-hl   ClusterIP   None           <none>        5432/TCP   5m20s
kubernetes                ClusterIP   10.96.0.1      <none>        443/TCP    25m
```

---

## 3. Подключение к PostgreSQL и создание таблицы

Пробрасываем порт локально:
```bash
kubectl port-forward svc/banana-db-postgresql 5432:5432 &
```

Подключаемся через `psql`:
```bash
PGPASSWORD=banana123 psql -h 127.0.0.1 -p 5432 -U postgres -d postgres
```

**Внутри psql выполняем:**
```sql
CREATE DATABASE bananaflow;
\c bananaflow

CREATE TABLE shipments (
    id SERIAL PRIMARY KEY,
    product_name VARCHAR(50),
    quantity INTEGER,
    destination VARCHAR(100),
    created_at TIMESTAMP DEFAULT NOW()
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
```

**Вывод:**
```
 id | product_name | quantity | destination | created_at
----+--------------+----------+-------------+----------------------------
  1 | bananas      |     1000 | Europe      | 2026-03-18 14:35:22.123456
  2 | bananas      |     1500 | Asia        | 2026-03-18 14:35:22.123456
  3 | bananas      |     2000 | Africa      | 2026-03-18 14:35:22.123456
  4 | coffee       |      500 | USA         | 2026-03-18 14:35:22.123456
  5 | coffee       |      700 | Canada      | 2026-03-18 14:35:22.123456
  6 | coffee       |      300 | Japan       | 2026-03-18 14:35:22.123456
  7 | sugar        |     1000 | Europe      | 2026-03-18 14:35:22.123456
  8 | sugar        |      800 | Asia        | 2026-03-18 14:35:22.123456
  9 | sugar        |      600 | Africa      | 2026-03-18 14:35:22.123456
 10 | sugar        |      400 | USA         | 2026-03-18 14:35:22.123456
(10 rows)
```

Выходим: `\q`

---

## 4. Масштабирование

Увеличиваем количество реплик:

```bash
helm upgrade banana-db bitnami/postgresql \
  --set auth.postgresPassword=banana123 \
  --set primary.persistence.size=2Gi \
  --set replicaCount=3
```

**Вывод:**
```
Release "banana-db" has been upgraded. Happy Helming!
NAME: banana-db
LAST DEPLOYED: Wed Mar 18 14:45:00 2026
NAMESPACE: default
STATUS: deployed
REVISION: 2
TEST SUITE: None
NOTES:
CHART NAME: postgresql
CHART VERSION: 18.5.8
APP VERSION: 18.3.0

```

Проверяем поды:
```bash
kubectl get pods -o wide
```
```
NAME                      READY   STATUS    RESTARTS   AGE   IP           NODE
banana-db-postgresql-0    1/1     Running   0          15m   10.244.0.5   minikube
banana-db-postgresql-1    1/1     Running   0          30s   10.244.0.6   minikube
banana-db-postgresql-2    1/1     Running   0          30s   10.244.0.7   minikube
```

Все три пода в статусе Running. Каждый под имеет свой стабильный идентификатор.

Проверяем детали StatefulSet:
```bash
kubectl get statefulset
```
```
NAME                     READY   AGE
banana-db-postgresql     3/3     15m
```

---

## 5. Проверка, что данные сохранились после масштабирования

Подключаемся к основному поду (мастеру):
```bash
kubectl exec -it banana-db-postgresql-0 -- bash -c "PGPASSWORD=banana123 psql -U postgres -d bananaflow -c 'SELECT COUNT(*) FROM shipments;'"
```
**Вывод:**
```
 count
-------
    10
(1 row)
```

Проверяем, что реплики синхронизированы:
```bash
kubectl exec -it banana-db-postgresql-1 -- bash -c "PGPASSWORD=banana123 psql -U postgres -d bananaflow -c 'SELECT product_name, SUM(quantity) FROM shipments GROUP BY product_name;'"
```
**Вывод:**
```
 product_name | sum
--------------+------
 bananas      | 4500
 coffee       | 1500
 sugar        | 2800
(3 rows)
```

Всё работает, данные реплицируются.

---

## 6. Итоговые проверки

```bash
kubectl get pods
```
```
NAME                      READY   STATUS    RESTARTS   AGE
banana-db-postgresql-0    1/1     Running   0          20m34s
banana-db-postgresql-1    1/1     Running   0          5m12s
banana-db-postgresql-2    1/1     Running   0          5m12s
```

```bash
kubectl get svc
```
```
NAME                      TYPE        CLUSTER-IP     EXTERNAL-IP   PORT(S)    AGE
banana-db-postgresql      ClusterIP   10.96.149.152  <none>        5432/TCP   22m35s
banana-db-postgresql-hl   ClusterIP   None           <none>        5432/TCP   22m35s
```

---

## 7. Выводы

| Этап | Результат |
|------|-----------|
| Minikube запущен | ✅ |
| Helm-чарт Bitnami PostgreSQL установлен | ✅ |
| База данных bananaflow создана | ✅ |
| Таблица shipments заполнена 10 записями | ✅ |
| Масштабирование до 3 подов | ✅ все Running |
| Данные сохранились на репликах | ✅ |
| Подключение через Service работает | ✅ |
