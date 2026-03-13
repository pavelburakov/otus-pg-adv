-- Функции ввода/вывода для типа tsid
CREATE FUNCTION tsid_in(cstring)
RETURNS tsid
AS 'MODULE_PATHNAME', 'tsid_in'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION tsid_out(tsid)
RETURNS cstring
AS 'MODULE_PATHNAME', 'tsid_out'
LANGUAGE C IMMUTABLE STRICT;

-- Тип данных tsid
CREATE TYPE tsid (
    INPUT   = tsid_in,
    OUTPUT  = tsid_out,
    INTERNALLENGTH = 8,
    ALIGNMENT = double,
    STORAGE = plain
);

-- Функция генерации tsid (возвращает bigint)
CREATE FUNCTION tsid()
RETURNS bigint
AS 'MODULE_PATHNAME', 'tsid'
LANGUAGE C VOLATILE;

-- Функции генерации в разных форматах
CREATE FUNCTION tsid_upr()
RETURNS text
AS 'MODULE_PATHNAME', 'tsid_upr'
LANGUAGE C VOLATILE;

CREATE FUNCTION tsid_lwr()
RETURNS text
AS 'MODULE_PATHNAME', 'tsid_lwr'
LANGUAGE C VOLATILE;

CREATE FUNCTION tsid_dec()
RETURNS text
AS 'MODULE_PATHNAME', 'tsid_dec'
LANGUAGE C VOLATILE;

CREATE FUNCTION tsid_hex()
RETURNS text
AS 'MODULE_PATHNAME', 'tsid_hex'
LANGUAGE C VOLATILE;

-- Функция генерации типа tsid
CREATE FUNCTION tsid_generate()
RETURNS tsid
AS 'MODULE_PATHNAME', 'tsid_generate'
LANGUAGE C VOLATILE;

-- Функции сравнения (опционально, для индексов)
CREATE FUNCTION tsid_eq(tsid, tsid)
RETURNS boolean
AS 'MODULE_PATHNAME', 'tsid_eq'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION tsid_lt(tsid, tsid)
RETURNS boolean
AS 'MODULE_PATHNAME', 'tsid_lt'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION tsid_hash(tsid)
RETURNS integer
AS 'MODULE_PATHNAME', 'tsid_hash'
LANGUAGE C IMMUTABLE STRICT;

-- Операторы
CREATE OPERATOR = (
    LEFTARG = tsid,
    RIGHTARG = tsid,
    PROCEDURE = tsid_eq,
    COMMUTATOR = '=',
    NEGATOR = '<>',
    RESTRICT = eqsel,
    JOIN = eqjoinsel
);

CREATE OPERATOR < (
    LEFTARG = tsid,
    RIGHTARG = tsid,
    PROCEDURE = tsid_lt,
    COMMUTATOR = >,
    NEGATOR = >=,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);

-- Класс операторов для индексов B-tree
CREATE OPERATOR CLASS tsid_ops
    DEFAULT FOR TYPE tsid USING btree AS
    OPERATOR 1 <,
    OPERATOR 2 <=,
    OPERATOR 3 =,
    OPERATOR 4 >=,
    OPERATOR 5 >,
    FUNCTION 1 tsid_lt(tsid, tsid),
    FUNCTION 2 tsid_eq(tsid, tsid);

-- Хэш-класс операторов
CREATE OPERATOR CLASS tsid_hash_ops
    DEFAULT FOR TYPE tsid USING hash AS
    OPERATOR 1 =,
    FUNCTION 1 tsid_hash(tsid);