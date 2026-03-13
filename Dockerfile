FROM postgres:16

# Устанавливаем инструменты сборки и заголовочные файлы PostgreSQL
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        postgresql-server-dev-16 \
        && rm -rf /var/lib/apt/lists/*


WORKDIR /tmp/tsid

# Копируем исходные файлы расширения
COPY tsid.c Makefile tsid.control tsid--2.0.sql ./

# Компилируем разделяемую библиотеку
RUN make && test -f tsid.so

# Копируем файлы в системные каталоги PostgreSQL с правильными правами
RUN install -d $(pg_config --sharedir)/extension && \
    install -m 644 tsid.control tsid--2.0.sql $(pg_config --sharedir)/extension/ && \
    install -m 755 tsid.so $(pg_config --pkglibdir)/

# Копируем скрипт инициализации (будет выполнен при старте контейнера)
COPY init.sql set_node_id.sh /docker-entrypoint-initdb.d/

# Очистка временных файлов
RUN rm -rf /tmp/tsid
