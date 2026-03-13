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
    STORAGE = plain, 
    PASSEDBYVALUE
);

-- Функция генерации tsid 
CREATE FUNCTION tsid()
RETURNS tsid
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

-- Функция проверки на пустое значение
CREATE FUNCTION is_empty(tsid)
RETURNS boolean
AS 'MODULE_PATHNAME', 'is_empty'
LANGUAGE C IMMUTABLE STRICT;

-- Функция получения NODE_ID
CREATE FUNCTION node_id(tsid)
RETURNS integer
AS 'MODULE_PATHNAME', 'node_id'
LANGUAGE C IMMUTABLE STRICT;

-- Функция получения времени создания TSID
CREATE FUNCTION datetime(tsid)
RETURNS timestamp with time zone
AS 'MODULE_PATHNAME', 'datetime'
LANGUAGE C IMMUTABLE STRICT;

-- Функции преобразования
CREATE FUNCTION lower(tsid)
RETURNS text
AS 'MODULE_PATHNAME', 'lower'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION upper(tsid)
RETURNS text
AS 'MODULE_PATHNAME', 'upper'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION to_tsid(cstring)
RETURNS tsid
AS 'MODULE_PATHNAME', 'to_tsid'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION to_char(tsid, text)
RETURNS text
AS 'MODULE_PATHNAME', 'to_char'
LANGUAGE C IMMUTABLE STRICT;

 -- Функции сравнения tsid с text
CREATE FUNCTION tsid_eq_text(tsid, text)
RETURNS boolean
AS 'MODULE_PATHNAME', 'tsid_eq_text'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION tsid_ne_text(tsid, text)
RETURNS boolean
AS 'MODULE_PATHNAME', 'tsid_ne_text'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION tsid_lt_text(tsid, text)
RETURNS boolean
AS 'MODULE_PATHNAME', 'tsid_lt_text'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION tsid_le_text(tsid, text)
RETURNS boolean
AS 'MODULE_PATHNAME', 'tsid_le_text'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION tsid_gt_text(tsid, text)
RETURNS boolean
AS 'MODULE_PATHNAME', 'tsid_gt_text'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION tsid_ge_text(tsid, text)
RETURNS boolean
AS 'MODULE_PATHNAME', 'tsid_ge_text'
LANGUAGE C IMMUTABLE STRICT;

-- Операторы tsid с text
CREATE OPERATOR = (
    LEFTARG = tsid,
    RIGHTARG = text,
    PROCEDURE = tsid_eq_text,
    COMMUTATOR = =,
    NEGATOR = <>,
    RESTRICT = eqsel,
    JOIN = eqjoinsel
);

CREATE OPERATOR <> (
    LEFTARG = tsid,
    RIGHTARG = text,
    PROCEDURE = tsid_ne_text,
    COMMUTATOR = <>,
    NEGATOR = =,
    RESTRICT = eqsel,
    JOIN = eqjoinsel
);

CREATE OPERATOR < (
    LEFTARG = tsid,
    RIGHTARG = text,
    PROCEDURE = tsid_lt_text,
    COMMUTATOR = >,
    NEGATOR = >=,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);

CREATE OPERATOR <= (
    LEFTARG = tsid,
    RIGHTARG = text,
    PROCEDURE = tsid_le_text,
    COMMUTATOR = >=,
    NEGATOR = >,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);

CREATE OPERATOR > (
    LEFTARG = tsid,
    RIGHTARG = text,
    PROCEDURE = tsid_gt_text,
    COMMUTATOR = <,
    NEGATOR = <=,
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);

CREATE OPERATOR >= (
    LEFTARG = tsid,
    RIGHTARG = text,
    PROCEDURE = tsid_ge_text,
    COMMUTATOR = <=,
    NEGATOR = <,
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);

-- Симметричные функции text с tsid
CREATE FUNCTION text_eq_tsid(text, tsid)
RETURNS boolean
AS 'MODULE_PATHNAME', 'text_eq_tsid'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION text_ne_tsid(text, tsid)
RETURNS boolean
AS 'MODULE_PATHNAME', 'text_ne_tsid'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION text_lt_tsid(text, tsid)
RETURNS boolean
AS 'MODULE_PATHNAME', 'text_lt_tsid'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION text_le_tsid(text, tsid)
RETURNS boolean
AS 'MODULE_PATHNAME', 'text_le_tsid'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION text_gt_tsid(text, tsid)
RETURNS boolean
AS 'MODULE_PATHNAME', 'text_gt_tsid'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION text_ge_tsid(text, tsid)
RETURNS boolean
AS 'MODULE_PATHNAME', 'text_ge_tsid'
LANGUAGE C IMMUTABLE STRICT;

-- Операторы text с tsid
CREATE OPERATOR = (
    LEFTARG = text,
    RIGHTARG = tsid,
    PROCEDURE = text_eq_tsid,
    COMMUTATOR = =,
    NEGATOR = <>,
    RESTRICT = eqsel,
    JOIN = eqjoinsel
);

CREATE OPERATOR <> (
    LEFTARG = text,
    RIGHTARG = tsid,
    PROCEDURE = text_ne_tsid,
    COMMUTATOR = <>,
    NEGATOR = =,
    RESTRICT = eqsel,
    JOIN = eqjoinsel
);

CREATE OPERATOR < (
    LEFTARG = text,
    RIGHTARG = tsid,
    PROCEDURE = text_lt_tsid,
    COMMUTATOR = >,
    NEGATOR = >=,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);

CREATE OPERATOR <= (
    LEFTARG = text,
    RIGHTARG = tsid,
    PROCEDURE = text_le_tsid,
    COMMUTATOR = >=,
    NEGATOR = >,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);

CREATE OPERATOR > (
    LEFTARG = text,
    RIGHTARG = tsid,
    PROCEDURE = text_gt_tsid,
    COMMUTATOR = <,
    NEGATOR = <=,
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);

CREATE OPERATOR >= (
    LEFTARG = text,
    RIGHTARG = tsid,
    PROCEDURE = text_ge_tsid,
    COMMUTATOR = <=,
    NEGATOR = <,
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);

-- Сравнение tsid с bigint
CREATE FUNCTION tsid_eq_int8(tsid, bigint)
RETURNS boolean
AS 'MODULE_PATHNAME', 'tsid_eq_int8'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION tsid_ne_int8(tsid, bigint)
RETURNS boolean
AS 'MODULE_PATHNAME', 'tsid_ne_int8'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION tsid_lt_int8(tsid, bigint)
RETURNS boolean
AS 'MODULE_PATHNAME', 'tsid_lt_int8'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION tsid_le_int8(tsid, bigint)
RETURNS boolean
AS 'MODULE_PATHNAME', 'tsid_le_int8'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION tsid_gt_int8(tsid, bigint)
RETURNS boolean
AS 'MODULE_PATHNAME', 'tsid_gt_int8'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION tsid_ge_int8(tsid, bigint)
RETURNS boolean
AS 'MODULE_PATHNAME', 'tsid_ge_int8'
LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR = (
    LEFTARG = tsid,
    RIGHTARG = bigint,
    PROCEDURE = tsid_eq_int8,
    COMMUTATOR = =,
    NEGATOR = <>,
    RESTRICT = eqsel,
    JOIN = eqjoinsel
);

CREATE OPERATOR <> (
    LEFTARG = tsid,
    RIGHTARG = bigint,
    PROCEDURE = tsid_ne_int8,
    COMMUTATOR = <>,
    NEGATOR = =,
    RESTRICT = eqsel,
    JOIN = eqjoinsel
);

CREATE OPERATOR < (
    LEFTARG = tsid,
    RIGHTARG = bigint,
    PROCEDURE = tsid_lt_int8,
    COMMUTATOR = >,
    NEGATOR = >=,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);

CREATE OPERATOR <= (
    LEFTARG = tsid,
    RIGHTARG = bigint,
    PROCEDURE = tsid_le_int8,
    COMMUTATOR = >=,
    NEGATOR = >,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);

CREATE OPERATOR > (
    LEFTARG = tsid,
    RIGHTARG = bigint,
    PROCEDURE = tsid_gt_int8,
    COMMUTATOR = <,
    NEGATOR = <=,
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);

CREATE OPERATOR >= (
    LEFTARG = tsid,
    RIGHTARG = bigint,
    PROCEDURE = tsid_ge_int8,
    COMMUTATOR = <=,
    NEGATOR = <,
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);

-- Симметричные операторы bigint с tsid
CREATE FUNCTION int8_eq_tsid(bigint, tsid)
RETURNS boolean
AS 'MODULE_PATHNAME', 'int8_eq_tsid'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION int8_ne_tsid(bigint, tsid)
RETURNS boolean
AS 'MODULE_PATHNAME', 'int8_ne_tsid'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION int8_lt_tsid(bigint, tsid)
RETURNS boolean
AS 'MODULE_PATHNAME', 'int8_lt_tsid'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION int8_le_tsid(bigint, tsid)
RETURNS boolean
AS 'MODULE_PATHNAME', 'int8_le_tsid'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION int8_gt_tsid(bigint, tsid)
RETURNS boolean
AS 'MODULE_PATHNAME', 'int8_gt_tsid'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION int8_ge_tsid(bigint, tsid)
RETURNS boolean
AS 'MODULE_PATHNAME', 'int8_ge_tsid'
LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR = (
    LEFTARG = bigint,
    RIGHTARG = tsid,
    PROCEDURE = int8_eq_tsid,
    COMMUTATOR = =,
    NEGATOR = <>,
    RESTRICT = eqsel,
    JOIN = eqjoinsel
);

CREATE OPERATOR <> (
    LEFTARG = bigint,
    RIGHTARG = tsid,
    PROCEDURE = int8_ne_tsid,
    COMMUTATOR = <>,
    NEGATOR = =,
    RESTRICT = eqsel,
    JOIN = eqjoinsel
);

CREATE OPERATOR < (
    LEFTARG = bigint,
    RIGHTARG = tsid,
    PROCEDURE = int8_lt_tsid,
    COMMUTATOR = >,
    NEGATOR = >=,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);

CREATE OPERATOR <= (
    LEFTARG = bigint,
    RIGHTARG = tsid,
    PROCEDURE = int8_le_tsid,
    COMMUTATOR = >=,
    NEGATOR = >,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);

CREATE OPERATOR > (
    LEFTARG = bigint,
    RIGHTARG = tsid,
    PROCEDURE = int8_gt_tsid,
    COMMUTATOR = <,
    NEGATOR = <=,
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);

CREATE OPERATOR >= (
    LEFTARG = bigint,
    RIGHTARG = tsid,
    PROCEDURE = int8_ge_tsid,
    COMMUTATOR = <=,
    NEGATOR = <,
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);

-- Функции сравнения tsid с tsid
CREATE FUNCTION tsid_eq(tsid, tsid)
RETURNS boolean
AS 'MODULE_PATHNAME', 'tsid_eq'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION tsid_lt(tsid, tsid)
RETURNS boolean
AS 'MODULE_PATHNAME', 'tsid_lt'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION tsid_le(tsid, tsid)
RETURNS boolean
AS 'MODULE_PATHNAME', 'tsid_le'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION tsid_ge(tsid, tsid)
RETURNS boolean
AS 'MODULE_PATHNAME', 'tsid_ge'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION tsid_gt(tsid, tsid)
RETURNS boolean
AS 'MODULE_PATHNAME', 'tsid_gt'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION tsid_cmp(tsid, tsid)
RETURNS integer
AS 'MODULE_PATHNAME', 'tsid_cmp'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION tsid_hash(tsid)
RETURNS integer
AS 'MODULE_PATHNAME', 'tsid_hash'
LANGUAGE C IMMUTABLE STRICT;

-- Операторы сравнения
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

CREATE OPERATOR <= (
    LEFTARG = tsid,
    RIGHTARG = tsid,
    PROCEDURE = tsid_le,
    COMMUTATOR = '>=',
    NEGATOR = '>',
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);

CREATE OPERATOR >= (
    LEFTARG = tsid,
    RIGHTARG = tsid,
    PROCEDURE = tsid_ge,
    COMMUTATOR = '<=',
    NEGATOR = '<',
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);

CREATE OPERATOR > (
    LEFTARG = tsid,
    RIGHTARG = tsid,
    PROCEDURE = tsid_gt,
    COMMUTATOR = '<',
    NEGATOR = '<=',
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);

-- Класс операторов для индексов B-tree
CREATE OPERATOR CLASS tsid_ops
    DEFAULT FOR TYPE tsid USING btree AS
    OPERATOR 1 <,
    OPERATOR 2 <=,
    OPERATOR 3 =,
    OPERATOR 4 >=,
    OPERATOR 5 >,
    FUNCTION 1 tsid_cmp(tsid, tsid);

-- Хэш-класс операторов
CREATE OPERATOR CLASS tsid_hash_ops
    DEFAULT FOR TYPE tsid USING hash AS
    OPERATOR 1 =,
    FUNCTION 1 tsid_hash(tsid);
