#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include "utils/builtins.h"
#include "utils/timestamp.h"
#include "utils/guc.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

#define TSID_BYTES 8
#define TSID_CHARS 13
#define TSID_EPOCH 631152000000L                    //EPOCH from 2020-01-01T00:00:00.000
#define RANDOM_BITS 22                              // 10 (нода) + 12 (счётчик)
#define NODE_BITS   10
#define COUNTER_BITS 12
#define NODE_SHIFT   COUNTER_BITS                   // 12
#define TIMESTAMP_SHIFT (NODE_BITS + COUNTER_BITS)  // 22
#define COUNTER_MASK ((1 << COUNTER_BITS) - 1)      // 0xFFF
#define NODE_MASK ((1 << NODE_BITS) - 1)            // 
#define TSID_EMPTY 0LL

static volatile uint32 counter = 0;
static int node_id_guc = 0;

static char ALPHABET_UPPERCASE[32] =
{
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'J', 'K',
    'M', 'N', 'P', 'Q', 'R', 'S', 'T', 'V', 'W', 'X', 'Y', 'Z'
};

static char ALPHABET_LOWERCASE[32] =
{
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'j', 'k',
    'm', 'n', 'p', 'q', 'r', 's', 't', 'v', 'w', 'x', 'y', 'z'
};

static short ALPHABET_VALUES[128] =
{
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 00-23 не используются
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 23-47 не используются
    0,  // 0
    1,  // 1
    2,  // 2
    3,  // 3
    4,  // 4
    5,  // 5
    6,  // 6
    7,  // 7
    8,  // 8
    9,  // 9
    -1, -1, -1, -1, -1, -1, -1, // 58-64 не используются
    10, // A
    11, // B
    12, // C
    13, // D
    14, // E
    15, // F
    16, // G
    17, // H
    1,  // I (выглядит как 1)
    18, // J
    19, // K
    1,  // L (выглядит как 1)
    20, // M
    21, // N
    0,  // O (выглядит как 0)
    22, // P
    23, // Q
    24, // R
    25, // S
    26, // T
    -1, // U (можно спутать с V)
    27, // V
    28, // W
    29, // X
    30, // Y
    31, // Z
    -1, -1, -1, -1, -1, -1,  // 91-96 не используются
    10, // a
    11, // b
    12, // c
    13, // d
    14, // e
    15, // f
    16, // g
    17, // h
    1,  // i (выглядит как 1)
    18, // j
    19, // k
    1,  // l (выглядит как 1)
    20, // m
    21, // n
    0,  // o (выглядит как 0)
    22, // p
    23, // q
    24, // r
    25, // s
    26, // t
    -1, // u (можно спутать с v)
    27, // v
    28, // w
    29, // x
    30, // y
    31, // z
    -1, -1, -1, -1, -1   // 123-127 не используются
};

void _PG_init(void)
{
    DefineCustomIntVariable("tsid.node_id", "Node ID for TSID generation (0-1023)", NULL, &node_id_guc, 0, 0, 1023, PGC_SUSET, 0, NULL, NULL, NULL);
}

long long tsid_num()
{
    // Используем timestamp PostgreSQL (микросекунды с 2000-01-01)
    TimestampTz now = GetCurrentTimestamp();
    long long time_part = (now / 1000 - TSID_EPOCH) << TIMESTAMP_SHIFT;
    long long node_part = node_id_guc << NODE_SHIFT;
    long long rand_part = __sync_fetch_and_add(&counter, 1) & COUNTER_MASK;
    return time_part | node_part | rand_part;
}

long long tsid_parse(char * chars)
{
    int len = strlen(chars);
    long long id = 0;    

    if (len == 0)
    {
        return TSID_EMPTY;
    }

    if (len != TSID_CHARS)
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type tsid: \"%s\"", chars)));
    }
    
    for (int i = 0; i < TSID_CHARS; i++)
    {
        char c = chars[i];
    
        if (c < 0 || c > 127)
        {
            ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid character in tsid: \"%c\"", c)));
        }
        
        short val = ALPHABET_VALUES[(int)c];
        
        if (val < 0)
        {
            ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid character in tsid: \"%c\"", c)));
        }
        
        id = (id << 5) | val;
    }
    
    return id;
}

int tsid_str(long long id, char * chars, char * alphabet) 
{
    if (id == TSID_EMPTY)
    {
        chars[0x00] = '\0';
    }
    else
    {
        chars[0x00] = alphabet[(id >> 60) & 0b11111];
        chars[0x01] = alphabet[(id >> 55) & 0b11111];
        chars[0x02] = alphabet[(id >> 50) & 0b11111];
        chars[0x03] = alphabet[(id >> 45) & 0b11111];
        chars[0x04] = alphabet[(id >> 40) & 0b11111];
        chars[0x05] = alphabet[(id >> 35) & 0b11111];
        chars[0x06] = alphabet[(id >> 30) & 0b11111];
        chars[0x07] = alphabet[(id >> 25) & 0b11111];
        chars[0x08] = alphabet[(id >> 20) & 0b11111];
        chars[0x09] = alphabet[(id >> 15) & 0b11111];
        chars[0x0a] = alphabet[(id >> 10) & 0b11111];
        chars[0x0b] = alphabet[(id >> 5) & 0b11111];
        chars[0x0c] = alphabet[id & 0b11111];
        chars[0x0d] = '\0';
    }
    
    return strlen(chars);
}

int tsid_format(long long id, char * chars, char * format) 
{
    int result = -1;
    
    if (id == TSID_EMPTY)
    {
        chars[0] = '\0';
        result = 0;
    }
    else if (strlen(format) != 1)
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid format: \"%s\"", format)));
    }
    else if (format[0x00] == 'U' || format[0x00] == 'u') 
    { 
        result = tsid_str(id, chars, ALPHABET_UPPERCASE);
    }
    else if (format[0x00] == 'L' || format[0x00] == 'l') 
    { 
        result = tsid_str(id, chars, ALPHABET_LOWERCASE);
    }
    else if (format[0x00] == 'D' || format[0x00] == 'd') 
    { 
        pg_sprintf(chars, "%0lld", id);
        result = strlen(chars);
    }
    else if (format[0x00] == 'H' || format[0x00] == 'h')
    {
        pg_sprintf(chars, "%0llX", id);
        result = strlen(chars);
    }
    else
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid format: \"%s\"", format)));
    }
    
    return result;
}

int tsid_node_id(long long id) 
{
    return (int)((id >> COUNTER_BITS) & NODE_MASK);
}

long long tsid_time(long long id) 
{
    return ((id >> TIMESTAMP_SHIFT) + TSID_EPOCH) * 1000;
}


/* Функции для генерации TSID в разных форматах */ 
PG_FUNCTION_INFO_V1(tsid);
Datum tsid(PG_FUNCTION_ARGS)
{
    PG_RETURN_INT64(tsid_num());
}

PG_FUNCTION_INFO_V1(tsid_upr);
Datum tsid_upr(PG_FUNCTION_ARGS) 
{
    text * chars = (text *)palloc(17);
    SET_VARSIZE(chars, 17);
    tsid_str(tsid_num(), VARDATA(chars), ALPHABET_UPPERCASE);
    PG_RETURN_TEXT_P(chars);
}

PG_FUNCTION_INFO_V1(tsid_lwr);
Datum tsid_lwr(PG_FUNCTION_ARGS) 
{
    text * chars = (text *)palloc(17);
    SET_VARSIZE(chars, 17);
    tsid_str(tsid_num(), VARDATA(chars), ALPHABET_LOWERCASE);
    PG_RETURN_TEXT_P(chars);
}

PG_FUNCTION_INFO_V1(tsid_dec);
Datum tsid_dec(PG_FUNCTION_ARGS) 
{
    text * chars = (text *)palloc(22);
    SET_VARSIZE(chars, 22);
    tsid_format(tsid_num(), VARDATA(chars), "D");
    PG_RETURN_TEXT_P(chars);
}

PG_FUNCTION_INFO_V1(tsid_hex);
Datum tsid_hex(PG_FUNCTION_ARGS) 
{
    text * chars = (text *)palloc(20);
    SET_VARSIZE(chars, 20);
    tsid_format(tsid_num(), VARDATA(chars), "H");
    PG_RETURN_TEXT_P(chars);
}

// Функция проверки на EMPTY
PG_FUNCTION_INFO_V1(is_empty);
Datum is_empty(PG_FUNCTION_ARGS)
{
    int64 id = PG_GETARG_INT64(0);
    PG_RETURN_BOOL(id == TSID_EMPTY);
}

/* Функции определения node_id */ 
PG_FUNCTION_INFO_V1(node_id);
Datum node_id(PG_FUNCTION_ARGS)
{
    int64 id = PG_GETARG_INT64(0);
    
    if (id == TSID_EMPTY)
    {
        PG_RETURN_NULL();
    }
    else
    {
        PG_RETURN_INT32(tsid_node_id(id));
    }
}

/* Функции определения время создания */ 
PG_FUNCTION_INFO_V1(datetime);
Datum datetime(PG_FUNCTION_ARGS)
{
    int64 id = PG_GETARG_INT64(0);
    
    if (id == TSID_EMPTY)
    {
        PG_RETURN_NULL();
    }
    else
    {
        PG_RETURN_TIMESTAMPTZ(tsid_time(id));
    }
}

/* Функции для преобразований TSID в строку и обратно */
PG_FUNCTION_INFO_V1(lower);
Datum lower(PG_FUNCTION_ARGS)
{
    text * chars = (text *)palloc(17);
    SET_VARSIZE(chars, 17);
    tsid_str(PG_GETARG_INT64(0), VARDATA(chars), ALPHABET_LOWERCASE);
    PG_RETURN_TEXT_P(chars);
}

PG_FUNCTION_INFO_V1(upper);
Datum upper(PG_FUNCTION_ARGS)
{
    text * chars = (text *)palloc(17);
    SET_VARSIZE(chars, 17);
    tsid_str(PG_GETARG_INT64(0), VARDATA(chars), ALPHABET_UPPERCASE);
    PG_RETURN_TEXT_P(chars);
}

PG_FUNCTION_INFO_V1(to_char);
Datum to_char(PG_FUNCTION_ARGS)
{
    int64 id = PG_GETARG_INT64(0);
    text * format_text = PG_GETARG_TEXT_PP(1); 
    char * format = text_to_cstring(format_text);
    char buf[32];
    int len = tsid_format(id, buf, format);
    text * result = (text *) palloc(len + VARHDRSZ);
    SET_VARSIZE(result, len + VARHDRSZ);
    memcpy(VARDATA(result), buf, len);
    PG_RETURN_TEXT_P(result);
}

PG_FUNCTION_INFO_V1(to_tsid);
Datum to_tsid(PG_FUNCTION_ARGS)
{
    PG_RETURN_INT64(tsid_parse(PG_GETARG_CSTRING(0)));
}

/* Функции для ввода/вывода TSID */
PG_FUNCTION_INFO_V1(tsid_in);
Datum tsid_in(PG_FUNCTION_ARGS)
{
    PG_RETURN_INT64(tsid_parse(PG_GETARG_CSTRING(0)));
}

PG_FUNCTION_INFO_V1(tsid_out);
Datum tsid_out(PG_FUNCTION_ARGS)
{
    int64 id = PG_GETARG_INT64(0);
    char *chars;
    
    if (id == TSID_EMPTY)
    {
        chars = (char *) palloc(1);
        chars[0] = '\0';
    }
    else
    {
        chars = (char *) palloc(TSID_CHARS + 1);
        tsid_format(id, chars, "U");
    }

    PG_RETURN_CSTRING(chars);
}

/* Функции сравнения TSID с TSID */
PG_FUNCTION_INFO_V1(tsid_eq);
Datum tsid_eq(PG_FUNCTION_ARGS)
{
    int64 a = PG_GETARG_INT64(0);
    int64 b = PG_GETARG_INT64(1);
    PG_RETURN_BOOL(a == b);
}

PG_FUNCTION_INFO_V1(tsid_lt);
Datum tsid_lt(PG_FUNCTION_ARGS)
{
    int64 a = PG_GETARG_INT64(0);
    int64 b = PG_GETARG_INT64(1);
    PG_RETURN_BOOL(a < b);
}

PG_FUNCTION_INFO_V1(tsid_le);
Datum tsid_le(PG_FUNCTION_ARGS)
{
    int64 a = PG_GETARG_INT64(0);
    int64 b = PG_GETARG_INT64(1);
    PG_RETURN_BOOL(a <= b);
}

PG_FUNCTION_INFO_V1(tsid_ge);
Datum tsid_ge(PG_FUNCTION_ARGS)
{
    int64 a = PG_GETARG_INT64(0);
    int64 b = PG_GETARG_INT64(1);
    PG_RETURN_BOOL(a >= b);
}

PG_FUNCTION_INFO_V1(tsid_gt);
Datum tsid_gt(PG_FUNCTION_ARGS)
{
    int64 a = PG_GETARG_INT64(0);
    int64 b = PG_GETARG_INT64(1);
    PG_RETURN_BOOL(a > b);
}

PG_FUNCTION_INFO_V1(tsid_cmp);
Datum tsid_cmp(PG_FUNCTION_ARGS)
{
    int64 a = PG_GETARG_INT64(0);
    int64 b = PG_GETARG_INT64(1);
    PG_RETURN_INT32((a < b) ? -1 : (a > b) ? 1 : 0);
}

PG_FUNCTION_INFO_V1(tsid_hash);
Datum tsid_hash(PG_FUNCTION_ARGS)
{
    int64 id = PG_GETARG_INT64(0);
    PG_RETURN_INT32((uint32)(id ^ (id >> 32)));
}

/* Функции сравнения TSID с TEXT */
PG_FUNCTION_INFO_V1(tsid_eq_text);
Datum tsid_eq_text(PG_FUNCTION_ARGS)
{
    int64 id = PG_GETARG_INT64(0);
    text * str = PG_GETARG_TEXT_PP(1);
    char * cstr = text_to_cstring(str);
    char buf[TSID_CHARS+1];
    tsid_str(id, buf, ALPHABET_UPPERCASE);
    PG_RETURN_BOOL(strcmp(buf, cstr) == 0);
}

PG_FUNCTION_INFO_V1(tsid_ne_text);
Datum tsid_ne_text(PG_FUNCTION_ARGS)
{
    int64 id = PG_GETARG_INT64(0);
    text * str = PG_GETARG_TEXT_PP(1);
    char * cstr = text_to_cstring(str);
    char buf[TSID_CHARS+1];
    tsid_str(id, buf, ALPHABET_UPPERCASE);
    PG_RETURN_BOOL(strcmp(buf, cstr) != 0);
}

PG_FUNCTION_INFO_V1(tsid_lt_text);
Datum tsid_lt_text(PG_FUNCTION_ARGS)
{
    int64 id = PG_GETARG_INT64(0);
    text * str = PG_GETARG_TEXT_PP(1);
    char * cstr = text_to_cstring(str);
    char buf[TSID_CHARS+1];
    tsid_str(id, buf, ALPHABET_UPPERCASE);
    PG_RETURN_BOOL(strcmp(buf, cstr) < 0);
}

PG_FUNCTION_INFO_V1(tsid_le_text);
Datum tsid_le_text(PG_FUNCTION_ARGS)
{
    int64 id = PG_GETARG_INT64(0);
    text * str = PG_GETARG_TEXT_PP(1);
    char * cstr = text_to_cstring(str);
    char buf[TSID_CHARS+1];
    tsid_str(id, buf, ALPHABET_UPPERCASE);
    PG_RETURN_BOOL(strcmp(buf, cstr) <= 0);
}

PG_FUNCTION_INFO_V1(tsid_gt_text);
Datum tsid_gt_text(PG_FUNCTION_ARGS)
{
    int64 id = PG_GETARG_INT64(0);
    text * str = PG_GETARG_TEXT_PP(1);
    char * cstr = text_to_cstring(str);
    char buf[TSID_CHARS+1];
    tsid_str(id, buf, ALPHABET_UPPERCASE);
    PG_RETURN_BOOL(strcmp(buf, cstr) > 0);
}

PG_FUNCTION_INFO_V1(tsid_ge_text);
Datum tsid_ge_text(PG_FUNCTION_ARGS)
{
    int64 id = PG_GETARG_INT64(0);
    text * str = PG_GETARG_TEXT_PP(1);
    char * cstr = text_to_cstring(str);
    char buf[TSID_CHARS+1];
    tsid_str(id, buf, ALPHABET_UPPERCASE);
    PG_RETURN_BOOL(strcmp(buf, cstr) >= 0);
}

/* Функции сравнения TEXT с TSID */
PG_FUNCTION_INFO_V1(text_eq_tsid);
Datum text_eq_tsid(PG_FUNCTION_ARGS)
{
    text * str = PG_GETARG_TEXT_PP(0);
    int64 id = PG_GETARG_INT64(1);
    char * cstr = text_to_cstring(str);
    char buf[TSID_CHARS+1];
    tsid_str(id, buf, ALPHABET_UPPERCASE);
    PG_RETURN_BOOL(strcmp(buf, cstr) == 0);
}

PG_FUNCTION_INFO_V1(text_ne_tsid);
Datum text_ne_tsid(PG_FUNCTION_ARGS)
{
    text * str = PG_GETARG_TEXT_PP(0);
    int64 id = PG_GETARG_INT64(1);
    char * cstr = text_to_cstring(str);
    char buf[TSID_CHARS+1];
    tsid_str(id, buf, ALPHABET_UPPERCASE);
    PG_RETURN_BOOL(strcmp(buf, cstr) != 0);
}

PG_FUNCTION_INFO_V1(text_lt_tsid);
Datum text_lt_tsid(PG_FUNCTION_ARGS)
{
    text * str = PG_GETARG_TEXT_PP(0);
    int64 id = PG_GETARG_INT64(1);
    char * cstr = text_to_cstring(str);
    char buf[TSID_CHARS+1];
    tsid_str(id, buf, ALPHABET_UPPERCASE);
    PG_RETURN_BOOL(strcmp(cstr, buf) < 0);  
}    

PG_FUNCTION_INFO_V1(text_le_tsid);
Datum text_le_tsid(PG_FUNCTION_ARGS)
{
    text * str = PG_GETARG_TEXT_PP(0);
    int64 id = PG_GETARG_INT64(1);
    char * cstr = text_to_cstring(str);
    char buf[TSID_CHARS+1];
    tsid_str(id, buf, ALPHABET_UPPERCASE);
    PG_RETURN_BOOL(strcmp(cstr, buf) <= 0);
}

PG_FUNCTION_INFO_V1(text_gt_tsid);
Datum text_gt_tsid(PG_FUNCTION_ARGS)
{
    text * str = PG_GETARG_TEXT_PP(0);
    int64 id = PG_GETARG_INT64(1);
    char * cstr = text_to_cstring(str);
    char buf[TSID_CHARS+1];
    tsid_str(id, buf, ALPHABET_UPPERCASE);
    PG_RETURN_BOOL(strcmp(cstr, buf) > 0);
}

PG_FUNCTION_INFO_V1(text_ge_tsid);
Datum text_ge_tsid(PG_FUNCTION_ARGS)
{
    text * str = PG_GETARG_TEXT_PP(0);
    int64 id = PG_GETARG_INT64(1);
    char * cstr = text_to_cstring(str);
    char buf[TSID_CHARS+1];
    tsid_str(id, buf, ALPHABET_UPPERCASE);
    PG_RETURN_BOOL(strcmp(cstr, buf) >= 0);
}

/* Функции сравнения TSID с BIGINT */
PG_FUNCTION_INFO_V1(tsid_eq_int8);
Datum tsid_eq_int8(PG_FUNCTION_ARGS)
{
    int64 id = PG_GETARG_INT64(0);
    int64 val = PG_GETARG_INT64(1);
    PG_RETURN_BOOL(id == val);
}

PG_FUNCTION_INFO_V1(tsid_ne_int8);
Datum tsid_ne_int8(PG_FUNCTION_ARGS)
{
    int64 id = PG_GETARG_INT64(0);
    int64 val = PG_GETARG_INT64(1);
    PG_RETURN_BOOL(id != val);
}

PG_FUNCTION_INFO_V1(tsid_lt_int8);
Datum tsid_lt_int8(PG_FUNCTION_ARGS)
{
    int64 id = PG_GETARG_INT64(0);
    int64 val = PG_GETARG_INT64(1);
    PG_RETURN_BOOL(id < val);
}

PG_FUNCTION_INFO_V1(tsid_le_int8);
Datum tsid_le_int8(PG_FUNCTION_ARGS)
{
    int64 id = PG_GETARG_INT64(0);
    int64 val = PG_GETARG_INT64(1);
    PG_RETURN_BOOL(id <= val);
}

PG_FUNCTION_INFO_V1(tsid_gt_int8);
Datum tsid_gt_int8(PG_FUNCTION_ARGS)
{
    int64 id = PG_GETARG_INT64(0);
    int64 val = PG_GETARG_INT64(1);
    PG_RETURN_BOOL(id > val);
}

PG_FUNCTION_INFO_V1(tsid_ge_int8);
Datum tsid_ge_int8(PG_FUNCTION_ARGS)
{
    int64 id = PG_GETARG_INT64(0);
    int64 val = PG_GETARG_INT64(1);
    PG_RETURN_BOOL(id >= val);
}

/* Функции сравнения BIGINT с TSID */
PG_FUNCTION_INFO_V1(int8_eq_tsid);
Datum int8_eq_tsid(PG_FUNCTION_ARGS)
{
    int64 val = PG_GETARG_INT64(0);
    int64 id = PG_GETARG_INT64(1);
    PG_RETURN_BOOL(val == id);
}

PG_FUNCTION_INFO_V1(int8_ne_tsid);
Datum int8_ne_tsid(PG_FUNCTION_ARGS)
{
    int64 val = PG_GETARG_INT64(0);
    int64 id = PG_GETARG_INT64(1);
    PG_RETURN_BOOL(val != id);
}

PG_FUNCTION_INFO_V1(int8_lt_tsid);
Datum int8_lt_tsid(PG_FUNCTION_ARGS)
{
    int64 val = PG_GETARG_INT64(0);
    int64 id = PG_GETARG_INT64(1);
    PG_RETURN_BOOL(val < id);
}

PG_FUNCTION_INFO_V1(int8_le_tsid);
Datum int8_le_tsid(PG_FUNCTION_ARGS)
{
    int64 val = PG_GETARG_INT64(0);
    int64 id = PG_GETARG_INT64(1);
    PG_RETURN_BOOL(val <= id);
}

PG_FUNCTION_INFO_V1(int8_gt_tsid);
Datum int8_gt_tsid(PG_FUNCTION_ARGS)
{
    int64 val = PG_GETARG_INT64(0);
    int64 id = PG_GETARG_INT64(1);
    PG_RETURN_BOOL(val > id);
}

PG_FUNCTION_INFO_V1(int8_ge_tsid);
Datum int8_ge_tsid(PG_FUNCTION_ARGS)
{
    int64 val = PG_GETARG_INT64(0);
    int64 id = PG_GETARG_INT64(1);
    PG_RETURN_BOOL(val >= id);
}
