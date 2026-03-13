#!/bin/bash
set -e

psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "$POSTGRES_DB" <<-EOSQL
    -- Загружаем библиотеку, чтобы параметр стал известен
    LOAD 'tsid';
    -- Теперь можно менять параметр
    ALTER SYSTEM SET tsid.node_id = ${NODE_ID:-0};
    SELECT pg_reload_conf();
EOSQL
