-- Сначала подключаемся к шаблонной базе данных template1,
-- чтобы расширение было доступно во всех новых базах
\connect template1
CREATE EXTENSION IF NOT EXISTS tsid;

-- Затем подключаемся к шаблонной базе данных postgres
\connect postgres
CREATE EXTENSION IF NOT EXISTS tsid;

-- Затем подключаемся к целевой базе данных mydb
\connect mydb 
CREATE EXTENSION IF NOT EXISTS tsid;

\echo 'Extension tsid installed successfully.'

-- Создаем тестовую таблицу с использованием tsid
CREATE TABLE IF NOT EXISTS test_table (
    id tsid DEFAULT tsid(),
    data text,
    created_at timestamp DEFAULT now()
);

-- Создаем индекс
CREATE INDEX IF NOT EXISTS idx_test_table_id ON test_table(id);
