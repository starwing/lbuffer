#define LUA_LIB
#include "lbuffer.h"


#include <ctype.h>
#include <string.h>


#if LUA_VERSION_NUM >= 502
static int luaL_typerror (lua_State *L, int narg, const char *tname) {
  const char *msg = lua_pushfstring(L, "%s expected, got %s",
                                    tname, luaL_typename(L, narg));
  return luaL_argerror(L, narg, msg);
}
#endif

#ifdef LB_OVERLOAD_LUA_API
#  undef lua_isstring
#  undef lua_tolstring
#  undef luaL_checklstring
#  undef luaL_optlstring
#endif

#if LUA_VERSION_NUM < 502
#define BUFFER_LIBNAME "buffer"
#endif
#define uchar(ch) ((unsigned char)(ch))


/* buffer maintenance */

static char *grow_buffer(lua_State *L, buffer *b, size_t len) {
    if (len > b->len) return lb_realloc(L, b, len);
    return b->str;
}

static void fill_str(buffer *b, int pos, size_t fill_len, const char *str, size_t len) {
    if (str == NULL || len <= 1)
        memset(&b->str[pos], len == 1 ? str[0] : 0, fill_len);
    else if (fill_len != 0) {
        int i, count = fill_len / len;
        for (i = (str == &b->str[pos] ? 1 : 0); i < count; ++i) {
            memcpy(&b->str[pos+i*len], str, len);
        }
        memcpy(&b->str[pos+count*len], str, fill_len % len);
    }
}

static size_t real_offset(int offset, size_t len) {
    if (offset >= 1 && (size_t)offset <= len)
        return offset - 1;
    else if (offset <= -1 && (size_t)-offset <= len)
        return offset + len;
    return offset > 0 ? len - 1 : 0;
}

static size_t real_range(lua_State *L, int narg, size_t *plen) {
    if (lua_gettop(L) >= narg) {
        size_t i = real_offset(luaL_optinteger(L, narg, 1), *plen);
        size_t j = real_offset(luaL_optinteger(L, narg+1, -1), *plen);
        *plen = i <= j ? j - i + 1 : 0;
        return i;
    }
    return 0;
}

/* buffer information */

static int lb_isbuf(lua_State *L) {
#ifdef LB_SUBBUFFER
    buffer *b;
    return lb_isbuffer(L, 1)
        && (b = (buffer*)lua_touserdata(L, 1)) != NULL
        && !lb_isinvalidsub(b);
#else
    return lb_isbuffer(L, 1);
#endif
}

static int lb_tostring(lua_State *L) {
#ifdef LB_SUBBUFFER
    buffer *b = NULL;
    if (lb_isbuffer(L, 1)) {
        if ((b = (buffer*)lua_touserdata(L, 1)) != NULL
                && lb_isinvalidsub(b)) {
            lua_pushfstring(L, "(invalid subbuffer): %p", b);
            return 1;
        }
    }
#else
    buffer *b = lb_tobuffer(L, 1);
#endif
    if (b != NULL)
        lua_pushlstring(L, b->str, b->len);
    else {
        size_t len;
        const char *str = lua_tolstring(L, 1, &len);
        lua_pushlstring(L, str, len);
    }
    return 1;
}

static int lb_tohex(lua_State *L) {
    size_t i, len, seplen;
    const char *str = lb_tolstring(L, 1, &len);
    const char *sep = lb_optlstring(L, 2, NULL, &seplen);
    int upper = lua_toboolean(L, 3);
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    for (i = 0; i < len; ++i) {
        char buff[2];
        if (i != 0) luaL_addlstring(&b, sep, seplen);
        sprintf(buff, (upper ? "%02X" : "%02x"), uchar(str[i]));
        luaL_addlstring(&b, buff, 2);
    }
    luaL_pushresult(&b);
    return 1;
}

static int lb_quote(lua_State *L) {
    size_t i, len;
    const char *str = lb_tolstring(L, 1, &len);
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    luaL_addchar(&b, '"');
    for (i = 0; i < len; ++i) {
        if (isprint(str[i]) && str[i] != '"')
            luaL_addchar(&b, uchar(str[i]));
        else {
            char buff[4];
            sprintf(buff, "\\%03d", uchar(str[i]));
            luaL_addlstring(&b, buff, 4);
        }
    }
    luaL_addchar(&b, '"');
    luaL_pushresult(&b);
    return 1;
}

static int lb_topointer(lua_State *L) {
    buffer *b = lb_checkbuffer(L, 1);
    size_t offset = real_offset(luaL_optinteger(L, 2, 1), b->len);
    lua_pushlightuserdata(L, &b->str[offset]);
    return 1;
}

static int lb_cmp(lua_State *L) {
    size_t l1, l2;
    const char *s1 = lb_checklstring(L, 1, &l1);
    const char *s2 = lb_checklstring(L, 2, &l2);
    int res;
    if ((res = memcmp(s1, s2, l1 < l2 ? l1 : l2)) == 0)
        res = l1 - l2;
    lua_pushinteger(L, res > 0 ? 1 : res < 0 ? -1 : 0);
    return 1;
}

static int lb_eq(lua_State *L) {
    /* We can do this slightly faster than lb_cmp() by comparing
     * string length first.
     */
    size_t l1, l2;
    const char *s1 = lb_checklstring(L, 1, &l1);
    const char *s2 = lb_checklstring(L, 2, &l2);
    int res;
    if ( l1 != l2 ) {
        res = 0;
    } else {
        res = !memcmp(s1, s2, l1);
    }
    lua_pushboolean(L, res);
    return 1;
}

static int lb_map(lua_State *L, int (*f)(int)) {
    buffer *b = lb_checkbuffer(L, 1);
    size_t i, len = b->len, pos = real_range(L, 2, &len);
    for (i = 0; i < len; ++i)
        b->str[pos+i] = uchar(f(b->str[pos+i]));
    lua_settop(L, 1);
    return 1;
}

static int lb_lower(lua_State *L) {
    return lb_map(L, tolower);
}

static int lb_upper(lua_State *L) {
    return lb_map(L, toupper);
}

static int lb_len(lua_State *L) {
    if (
#if LUA_VERSION_NUM >= 502
            !lb_isbuffer(L, 2) &&
#endif
            !lua_isnoneornil(L, 2)) {
        buffer *b = lb_checkbuffer(L, 1);
        size_t oldlen = b->len;
        int newlen = luaL_checkint(L, 2);
        if (newlen < 0) newlen += b->len;
        if (newlen < 0) newlen = 0;
        if (lb_realloc(L, b, newlen) && newlen > oldlen)
            memset(&b->str[oldlen], 0, newlen - oldlen);
        lua_pushinteger(L, b->len);
    }
    else {
        size_t len;
        lb_checklstring(L, 1, &len);
        lua_pushinteger(L, len);
    }
    return 1;
}

static int lb_alloc(lua_State *L) {
    buffer *b = lb_checkbuffer(L, 1);
    if (lb_realloc(L, b, luaL_checkint(L, 2))) {
        size_t len = 0;
        const char *str = lb_optlstring(L, 3, NULL, &len);
        size_t pos = real_range(L, 4, &len);
        fill_str(b, 0, b->len, &str[pos], len);
    }
    lua_settop(L, 1);
    lua_pushinteger(L, b->len);
    return 2;
}

static int lb_free(lua_State *L) {
    buffer *b = lb_checkbuffer(L, 1);
    lb_realloc(L, b, 0);
    return 0;
}

static int lb_byte(lua_State *L) {
    buffer *b = lb_checkbuffer(L, 1);
    size_t i;
    size_t len = b->len, pos = real_range(L, 2, &len);
    if (lua_isnoneornil(L, 3)) len = 1;
    luaL_checkstack(L, len, "string slice too long");
    for (i = 0; i < len; ++i)
        lua_pushinteger(L, uchar(b->str[pos+i]));
    return len;
}

static int lb_char(lua_State *L) {
    buffer *b = NULL;
    int invalid = 0;
    int i, n = lua_gettop(L);
    if ((b = lb_tobuffer(L, 1)) == NULL) {
        b = lb_newbuffer(L);
        lua_insert(L, 1);
        n += 1;
    }
    if (lb_realloc(L, b, n - 1) || n == 1) {
        for (i = 2; i <= n; ++i) {
            int c = luaL_checkint(L, i);
            b->str[i-2] = uchar(c);
            if (c != uchar(c) && !invalid)
                invalid = i - 1;
        }
    }
    lua_settop(L, 1);
    if (invalid) {
        lua_pushinteger(L, invalid);
        return 2;
    }
    return 1;
}

/* buffer operations */

enum cmd {
    cmd_init,
    cmd_append,
    cmd_assign,
    cmd_insert,
    cmd_set
};

static char *prepare_cmd(lua_State *L, buffer *b, enum cmd c, int pos, int len) {
    size_t oldlen = b->len;
    char *newstr = NULL;
    if (c == cmd_assign)
        return lb_realloc(L, b, pos + len);
    else if (c != cmd_insert)
        return grow_buffer(L, b, pos + len);
    else if ((newstr = lb_realloc(L, b, b->len+len)) != NULL)
        memmove(&b->str[pos+len], &b->str[pos], oldlen-pos);
    return newstr;
}

static const char *udtolstring(lua_State *L, int narg, size_t *plen) {
    void *u = NULL;
    if ((u = lua_touserdata(L, narg)) != NULL) {
        int len = luaL_checkint(L, narg+1);
        if (plen != NULL) *plen = len >= 0 ? len : 0;
#ifdef LB_COMPAT_TOLUA
        if (!lua_islightuserdata(L, narg)) /* compatble with tolua */
            u = *(void**)u;
#endif
    }
    if (u == NULL && plen != NULL) *plen = 0;
    return (const char*)u;
}

static int do_cmd(lua_State *L, buffer *b, int narg, enum cmd c) {
    int pos;
    switch (c) {
        case cmd_append:
            pos = b->len; break;
        case cmd_insert: case cmd_set:
            pos = real_offset(luaL_checkint(L, narg++), b->len); break;
        default:
            pos = 0; break;
    }

    if (lb_isbufferorstring(L, narg)) {
        size_t len;
        const char *str = lb_tolstring(L, narg, &len);
        size_t i = real_range(L, narg+1, &len);
        if (prepare_cmd(L, b, c, pos, len))
            memcpy(&b->str[pos], &str[i], len);
    }
    else if (lua_type(L, narg) == LUA_TNUMBER) {
        size_t fill_len = lua_tointeger(L, narg);
        size_t len = 0;
        const char *str = NULL;
        if (!lua_isnoneornil(L, narg+1)) {
            if ((str = lb_tolstring(L, narg+1, &len)) != NULL)
                str += real_range(L, narg+2, &len);
            else if ((str = udtolstring(L, narg+1, &len)) == NULL)
                luaL_typerror(L, narg+1, "string, buffer or userdata");
        }
        if (prepare_cmd(L, b, c, pos, fill_len))
            fill_str(b, pos, fill_len, str, len);
    }
    else if (lua_isuserdata(L, narg)) {
        size_t len = 0;
        const char *str = udtolstring(L, narg, &len);
        if (prepare_cmd(L, b, c, pos, len))
            memcpy(&b->str[pos], str, len);
    }
    else if (!lua_isnoneornil(L, narg))
        luaL_typerror(L, narg, "string, buffer, number or userdata");
    lua_settop(L, narg-1);
    return 1;
}

static int lb_new(lua_State *L) {
    buffer *b = lb_newbuffer(L);
    lua_insert(L, 1);
    return do_cmd(L, b, 2, cmd_init);
}

#ifdef LB_SUBBUFFER
static int lb_sub(lua_State *L) {
    buffer *b = lb_checkbuffer(L, 1);
    size_t begin = real_offset(luaL_optinteger(L, 2, 1), b->len);
    int j = luaL_optinteger(L, 3, -1);
    size_t end = real_offset(j, b->len) + 1;
    if (j == 0 || end < begin)
        end = begin;
    lb_newsubbuffer(L, b, begin, end);
    return 1;
}

static int lb_subcount(lua_State *L) {
    buffer *b = (buffer*)luaL_checkudata(L, 1, BUFFER_MTNAME);
    if (b->subcount >= 0)
        lua_pushinteger(L, b->subcount);
    else if (b->subcount == LB_INVALID_SUB)
        lua_pushliteral(L, "invalid");
    else if (b->subcount == LB_SUB)
        lua_pushliteral(L, "sub");
    else
        lua_pushnil(L);
    return 1;
}
#endif

static int lb_rep(lua_State *L) {
    buffer *b = lb_checkbuffer(L, 1);
    size_t len = b->len;
    const char *str = NULL;
    int rep = 0;
    if (lua_type(L, 2) == LUA_TNUMBER)
        rep = lua_tointeger(L, 2);
    else if ((str = lb_tolstring(L, 2, &len)) != NULL)
        rep = luaL_checkint(L, 3);
    else luaL_typerror(L, 2, "number, buffer or string");
    if (lb_realloc(L, b, len * (rep >= 0 ? rep : 0)))
        fill_str(b, 0, b->len, str != NULL ? str : b->str, len);
    lua_settop(L, 1);
    return 1;
}

static int lb_append(lua_State *L) {
    return do_cmd(L, lb_checkbuffer(L, 1), 2, cmd_append);
}

static int lb_assign(lua_State *L) {
    return do_cmd(L, lb_checkbuffer(L, 1), 2, cmd_assign);
}

static int lb_insert(lua_State *L) {
    return do_cmd(L, lb_checkbuffer(L, 1), 2, cmd_insert);
}

static int lb_set(lua_State *L) {
    return do_cmd(L, lb_checkbuffer(L, 1), 2, cmd_set);
}

static int lb_clear(lua_State *L) {
    buffer *b = lb_checkbuffer(L, 1);
    size_t len = b->len, pos = real_range(L, 2, &len);
    memset(&b->str[pos], 0, len);
    lua_settop(L, 1);
    return 1;
}

static int lb_copy(lua_State *L) {
    buffer *b = lb_checkbuffer(L, 1);
    size_t len = b->len, pos = real_range(L, 2, &len);
    lb_pushlstring(L, &b->str[pos], len);
    return 1;
}

static int lb_move(lua_State *L) {
    buffer *b = lb_checkbuffer(L, 1);
    int dst = luaL_checkint(L, 2);
    size_t len = b->len, pos = real_range(L, 3, &len);
    size_t oldlen = b->len;

    if (dst > 0) dst -= 1;
    if (dst < 0) dst += b->len;
    if (dst < 0) dst = 0;

    if (grow_buffer(L, b, dst + len))
        memmove(&b->str[dst], &b->str[pos], len);
    if (dst > oldlen)
        memset(&b->str[oldlen], 0, dst - oldlen);
    lua_settop(L, 1);
    return 1;
}

static int lb_remove(lua_State *L) {
    buffer *b = lb_checkbuffer(L, 1);
    size_t len = b->len, pos = real_range(L, 2, &len);
    size_t end = pos + len;
    if (len != 0)
        memmove(&b->str[pos], &b->str[end], b->len-end);
    b->len -= len;
    lua_settop(L, 1);
    return 1;
}

/* pack/unpack bianry buffer */

#ifndef _MSC_VER
#  include <stdint.h>
#else
#  define uint32_t unsigned long
#  define uint64_t unsigned __int64
#  define int32_t signed long
#  define int64_t signed __int64
#endif
#if defined( __sparc__ ) || defined( __ppc__ )
#	define CPU_BIG_ENDIAN
#endif

#define BIG_ENDIAN 0x4321
#define LITTLE_ENDIAN 0x1234

#ifdef CPU_BIG_ENDIAN
#  define NATIVE_ENDIAN BIG_ENDIAN
#else
#  define NATIVE_ENDIAN LITTLE_ENDIAN
#endif

#define swap32(x) \
  (((x) >> 24) | (((x) >> 8) & 0xFF00) | (((x) & 0xFF00) << 8) | ((x) << 24))

typedef union numbuf_t {
    uint32_t i32;
    uint64_t i64;
    float f;
    double d;
} numbuf_t;

typedef struct parse_info {
    lua_State *L;
    buffer *b;
    size_t pos;
    int pack, endian;
    int narg, nret;
    const char *fmt;
} parse_info;

#define skip_white(s) do { while (*(s) == ' ' || *(s) == '\t'\
    || *(s) == '\r'|| *(s) == '\n') ++(s); } while(0)

static int parse_optint(const char **str, int *pn) {
    int retv = 0;
    const char *oldstr = *str;
    while (isdigit(**str)) retv = retv * 10 + uchar(*(*str)++ - '0');
    if (*str != oldstr) *pn = retv;
    return retv;
}

static void parse_fmtargs(parse_info *info, int *wide, int *count) {
    skip_white(info->fmt);
    parse_optint(&info->fmt, wide);
    skip_white(info->fmt);
    if (*info->fmt == '*') {
        ++info->fmt;
        skip_white(info->fmt);
        parse_optint(&info->fmt, count);
    }
    else if (*info->fmt == '$') {
        ++info->fmt;
        *count = -1;
    }
    skip_white(info->fmt);
}

static uint32_t read_int32(parse_info *info, int wide) {
    const char *str = info->b->str;
    int pos = info->pos; info->pos += wide;
    int n = 0;
    switch (wide) {
    default: luaL_error(info->L, "invalid wide: %d", wide); break;
    case 4:           n |= uchar(str[pos++]) & 0xFF;
    case 3:  n <<= 8; n |= uchar(str[pos++]) & 0xFF;
    case 2:  n <<= 8; n |= uchar(str[pos++]) & 0xFF;
    case 1:  n <<= 8; n |= uchar(str[pos++]) & 0xFF;
    }
    if (info->endian == LITTLE_ENDIAN)
        n = swap32(n) >> ((4 - wide)<<3);
    return n;
}

static void write_int32(parse_info *info, uint32_t buf, int wide) {
    char *str = info->b->str;
    int pos = info->pos; info->pos += wide;
    if (info->endian == BIG_ENDIAN)
        buf = swap32(buf) >> ((4 - wide)<<3);
    switch (wide) {
    default: luaL_error(info->L, "invalid wide: %d", wide); break;
    case 4: str[pos++] = uchar(buf) & 0xFF; buf >>= 8;
    case 3: str[pos++] = uchar(buf) & 0xFF; buf >>= 8;
    case 2: str[pos++] = uchar(buf) & 0xFF; buf >>= 8;
    case 1: str[pos++] = uchar(buf) & 0xFF;
    }
}

static void read_binary(parse_info *info, numbuf_t *buf, int wide) {
    if (wide <= 4) buf->i32 = read_int32(info, wide);
    else {
        uint32_t lo, hi; /* in big endian */
        hi = read_int32(info, 4);
        lo = read_int32(info, wide - 4);
        buf->i64 = info->endian == BIG_ENDIAN ?
            ((uint64_t)hi << ((wide-4)<<3)) | lo :
            ((uint64_t)lo << 32) | hi;
    }
}

static void write_binary(parse_info *info, numbuf_t *buf, int wide) {
    if (wide <= 4) write_int32(info, buf->i32, wide);
    else if (info->endian == BIG_ENDIAN) {
        write_int32(info, buf->i64 >> 32, wide - 4);
        write_int32(info, buf->i64, 4);
    }
    else {
        write_int32(info, buf->i64, 4);
        write_int32(info, buf->i64 >> 32, wide - 4);
    }
}

static int do_fmt(parse_info *info, char fmt, int wide, int count) {
    numbuf_t buf;
    size_t pos;
    int top = lua_gettop(info->L);
    typedef void (*pushlstring_t)(lua_State *L, const char *str, size_t len);
    pushlstring_t pushlstring = isupper(fmt) ?
        (pushlstring_t)lb_pushlstring :
        (pushlstring_t)lua_pushlstring;

#define I(f) (info->f)
#define CHECK_STACK(n) luaL_checkstack(I(L), (n), "too much formats")
#define ADD_NRET() do { ++I(nret); \
    if (count < 0) CHECK_STACK(1); } while (0)
#define BEGIN_PACK(n) \
    if (I(pack)) { \
        if (count < 0 || lb_realloc(I(L), I(b), (n))) { \
            while ((count >= 0 || I(narg) <= top) && count--)
#define BEGIN_UNPACK() \
        } \
    } else { \
        size_t blen = I(b)->len; \
        if (count > 0) CHECK_STACK(count); \
        while (count-- && I(pos) < blen)
#define END_PACK() \
    } break

    switch (fmt) {
    case 'z': case 'Z': /* zero-terminated string */
        BEGIN_PACK(I(b)->len) {
            size_t len;
            const char *str = lb_checklstring(I(L), I(narg)++, &len);
            if (wide != 0 && len > wide) len = wide;
            if (lb_realloc(I(L), I(b), I(pos)+len+1)) {
                memcpy(&I(b)->str[I(pos)], str, len);
                I(b)->str[I(pos)+len] = '\0';
                I(pos) += len + 1;
            }
        }
        BEGIN_UNPACK() {
            int begin = I(pos);
            size_t len = 0;
            while ((wide == 0 || len < wide)
                    && I(pos) < blen
                    && I(b)->str[I(pos)] != '\0')
                ++I(pos), ++len;
            pushlstring(I(L), &I(b)->str[begin], len);
            if (I(pos) < blen && I(b)->str[I(pos)] == '\0') ++I(pos);
            ADD_NRET();
        }
        END_PACK(); 

    case 'c': case 'C': /* char */
        if (wide == 0) wide = 1;
        BEGIN_PACK(I(pos) + wide * count) {
            size_t len;
            const char *str = lb_checklstring(I(L), I(narg)++, &len);
            if (wide != 0 && len > wide) len = wide;
            if (lb_realloc(I(L), I(b), I(pos) + wide)) {
                memcpy(&I(b)->str[I(pos)], str, len);
                if (len < wide)
                    memset(&I(b)->str[I(pos)+len], 0, wide - len);
                I(pos) += wide;
            }
        }
        BEGIN_UNPACK() {
            int begin = I(pos);
            size_t len = 0;
            while (len < wide && I(pos) < blen)
                ++I(pos), ++len;
            pushlstring(I(L), &I(b)->str[begin], len);
            if (len < wide && I(pos) < blen) ++I(pos);
            ADD_NRET();
        }
        END_PACK(); 

    case 'p': case 'P': /* length preceded string */
        if (wide == 0) wide = 4;
        if (wide > 8) luaL_error(I(L), "invalid wide of format '%c': "
                "only 1 to 8 supported.", fmt);
        BEGIN_PACK(I(b)->len) {
            size_t len;
            const char *str = lb_checklstring(I(L), I(narg)++, &len);
            if (lb_realloc(I(L), I(b), I(pos) + wide + len)) {
                if (wide > 4) buf.i64 = len;
                else buf.i32 = len;
                write_binary(info, &buf, wide);
                memcpy(&I(b)->str[I(pos)], str, len);
                I(pos) += len;
            }
        }
        BEGIN_UNPACK() {
            size_t len;
            if (I(pos) + wide > blen) return 0;
            read_binary(info, &buf, wide);
            if (wide <= 4)
                len = buf.i32;
            else if ((len = buf.i64) != buf.i64)
                luaL_error(I(L), "string too big in format '%c'", fmt);
            if (I(pos) + len > blen) return 0;
            pushlstring(I(L), &I(b)->str[I(pos)], len);
            I(pos) += len;
            ADD_NRET();
        }
        END_PACK();

    case 'i': case 'I': /* int */
        if (wide == 0) wide = 4;
        if (wide > 8) luaL_error(I(L), "invalid wide of format '%c': "
                "only 1 to 8 supported.", fmt);
        BEGIN_PACK(I(pos) + wide * count) {
            if (wide > 4)
                buf.i64 = (int64_t)luaL_checknumber(I(L), I(narg)++);
            else
                buf.i32 = (int32_t)luaL_checknumber(I(L), I(narg)++);
            if (count >= 0 || lb_realloc(I(L), I(b), I(pos) + wide))
                write_binary(info, &buf, wide);
        }
        BEGIN_UNPACK() {
            if (I(pos) + wide > blen) return 0;
            read_binary(info, &buf, wide);
            if (wide > 4) {
                if (wide != 8 && ((uint64_t)1 << (wide*8-1) & buf.i64) != 0)
                    buf.i64 |= ~(uint64_t)0 << (wide*8);
                lua_pushnumber(I(L), (int64_t)buf.i64);
            }
            else {
                if (wide != 4 && ((uint32_t)1 << (wide*8-1) & buf.i32) != 0)
                    buf.i32 |= ~(uint32_t)0 << (wide*8);
                lua_pushnumber(I(L), (int32_t)buf.i32);
            }
            ADD_NRET();
        }
        END_PACK();

    case 'u': case 'U': /* unsigend int */
        if (wide == 0) wide = 4;
        if (wide > 8) luaL_error(I(L), "invalid wide of format '%c': "
                "only 1 to 8 supported.", fmt);
        BEGIN_PACK(I(pos) + wide * count) {
            if (wide > 4)
                buf.i64 = (/*u*/int64_t)luaL_checknumber(I(L), I(narg)++);
            else
                buf.i32 = (/*u*/int32_t)luaL_checknumber(I(L), I(narg)++);
            if (count >= 0 || lb_realloc(I(L), I(b), I(pos) + wide))
                write_binary(info, &buf, wide);
        }
        BEGIN_UNPACK() {
            if (I(pos) + wide > blen) return 0;
            read_binary(info, &buf, wide);
            if (wide > 4)
                lua_pushnumber(I(L), buf.i64);
            else
                lua_pushnumber(I(L), buf.i32);
            ADD_NRET();
        }
        END_PACK();

    case 'f': case 'F': /* float */
        if (wide == 0) wide = 4;
        if (wide != 4 && wide != 8)
            luaL_error(I(L), "invalid wide of format '%c': "
                    "only 4 or 8 supported.", fmt);
        BEGIN_PACK(I(pos) + wide * count) {
            buf.d = luaL_checknumber(I(L), I(narg)++);
            if (wide == 4) buf.f = buf.d;
            if (count >= 0 || lb_realloc(I(L), I(b), I(pos) + wide)) {
                write_binary(info, &buf, wide);
                I(pos) += wide;
            }
        }
        BEGIN_UNPACK() {
            if (I(pos) + wide > blen) return 0;
            read_binary(info, &buf, wide);
            if (wide == 4)
                lua_pushnumber(I(L), buf.f);
            else
                lua_pushnumber(I(L), buf.d);
            ADD_NRET();
        }
        END_PACK(); 

    case '#': /* current pos */
        lua_pushinteger(I(L), I(pos) + 1);
        ADD_NRET();
        break;

    case '<': /* little endian */
        I(endian) = LITTLE_ENDIAN; break;
    case '>': /* big endian */
        I(endian) = BIG_ENDIAN; break;
    case '=': /* native endian */
        I(endian) = NATIVE_ENDIAN; break;

    case '@': /* seek for absolute address */
        pos = wide * count - 1; goto check_seek;
    case '+': /* seek for positive address */
        pos = I(pos) + wide * count; goto check_seek;
    case '-': /* seek for negitive address */
        pos = wide * count;
        if (I(pos) > pos)
            pos = I(pos) - pos;
        else
            pos = 0;
check_seek:
        if (count < 0)
            luaL_error(I(L), "invalid count for format '%c'", fmt);
        if (pos > I(b)->len) pos = I(b)->len;
        I(pos) = pos;
        break;

    default:
        luaL_error(I(L), "invalid format '%c'", fmt);
        break;
    }
    return 1;
#undef END_PACK
#undef BEGIN_UNPACK
#undef BEGIN_PACK
#undef ADD_NRET
#undef CHECK_STACK
#undef I
}

static int parse_fmt(parse_info *info) {
    int fmt, insert_pos = 0;
    skip_white(info->fmt);
    if (*info->fmt == '!') {
        insert_pos = !info->pack; /* only enabled in unpack */
        ++info->fmt; skip_white(info->fmt);
    }
    while ((fmt = *info->fmt++) != '\0') {
        int wide = 0, count = 1;
        parse_fmtargs(info, &wide, &count);
        if (!do_fmt(info, fmt, wide, count)) {
            if ((fmt = *info->fmt++) == '#') {
                parse_fmtargs(info, &wide, &count);
                do_fmt(info, fmt, 0, 0);
            }
            break;
        }
    }
    if (insert_pos) {
        lua_pushinteger(info->L, info->pos + 1);
        lua_insert(info->L, -(++info->nret));
    }
    return info->nret;
}

static int do_pack(lua_State *L, buffer *b, int narg, int pack) {
    parse_info info = {NULL};
    info.L = L;
    info.b = b;
    info.pack = pack;
    info.narg = narg;
    info.endian = NATIVE_ENDIAN;
    if (lua_type(L, info.narg) == LUA_TNUMBER)
        info.pos = real_offset(lua_tointeger(L, info.narg++), info.b->len);
    info.fmt = lb_checklstring(L, info.narg++, NULL);
    parse_fmt(&info);
    if (pack) {
        lua_pushinteger(L, info.pos + 1);
        lua_insert(L, -(++info.nret));
        lua_pushvalue(L, 1);
        lua_insert(L, -(++info.nret));
    }
    return info.nret;
}

static int lb_pack(lua_State *L) {
    buffer *b;
    if ((b = lb_tobuffer(L, 1)) == NULL) {
        b = lb_newbuffer(L);
        lua_insert(L, 1);
    }
    return do_pack(L, b, 2, 1);
}

static int lb_unpack(lua_State *L) {
    return do_pack(L, lb_checkbuffer(L, 1), 2, 0);
}

/* meta methods */

static int lb_gc(lua_State *L) {
    if (lb_isbuffer(L, 1)) {
        buffer *b = (buffer*)lua_touserdata(L, 1);
#ifdef LB_SUBBUFFER
        if (lb_issubbuffer(b)) {
            lb_removesubbuffer((subbuffer*)b);
            return 0;
        }
#endif
        lb_realloc(L, b, 0);
    }
    return 0;
}

static int lb_concat(lua_State *L) {
    size_t len;
    const char *str = lb_checklstring(L, 1, &len);
    buffer *b = lb_pushbuffer(L, str, len);
    lua_replace(L, 1);
    return do_cmd(L, b, 2, cmd_append);
}

static int check_offset(int offset, int len, int extra) {
    if (offset >= 0)
        offset -= 1;
    else
        offset += len;
    if (offset < 0 || offset >= len + extra)
        return -1;
    return offset;
}

static int lb_index(lua_State *L) {
#ifdef LB_SUBBUFFER
    buffer *b = (buffer*)luaL_checkudata(L, 1, BUFFER_MTNAME);
#else
    buffer *b = lb_checkbuffer(L, 1);
#endif
    int pos;

    switch (lua_type(L, 2)) {
    case LUA_TSTRING:
        lua_rawget(L, lua_upvalueindex(1));
        break;
    case LUA_TNUMBER:
        if ((pos = check_offset(luaL_checkint(L, 2), b->len, 0)) >= 0)
            lua_pushinteger(L, uchar(b->str[pos]));
        else
            lua_pushnil(L);
        break;
    default:
        lua_pushnil(L);
        break;
    }

    return 1;
}

static int lb_newindex(lua_State *L) {
    buffer *b = lb_checkbuffer(L, 1);
    int pos;
    if ((pos = check_offset(luaL_checkint(L, 2), b->len, 1)) < 0)
        return 0;

    if (pos == b->len && !grow_buffer(L, b, b->len + 1))
        return 0;

    if (lb_isbufferorstring(L, 3)) {
        size_t len, oldlen = b->len;
        const char *str = lb_tolstring(L, 3, &len);
        if (len == 1)
            b->str[pos] = str[0];
        else if (grow_buffer(L, b, b->len + len - 1)) {
            memmove(&b->str[pos+len], &b->str[pos+1], oldlen - pos - 1);
            memcpy(&b->str[pos], str, len);
        }
    }
    else {
        int value = luaL_checkint(L, 3);
        b->str[pos] = uchar(value);
    }
    return 0;
}

static int lb_call(lua_State *L) {
    buffer *b = lb_newbuffer(L);
    lua_replace(L, 1);
    return do_cmd(L, b, 2, cmd_init);
}

static int auxipairs(lua_State *L) {
    buffer *b = lb_checkbuffer(L, 1);
    int key = luaL_checkint(L, 2) + 1;
    if (key <= 0 || key > b->len) return 0;
    lua_pushinteger(L, key);
    lua_pushinteger(L, uchar(b->str[key - 1]));
    return 2;
}

static int lb_ipairs(lua_State *L) {
    buffer *b = lb_checkbuffer(L, 1);
    int pos = real_offset(luaL_optinteger(L, 2, 0), b->len);
    lua_pushcfunction(L, auxipairs);
    lua_insert(L, 1);
    lua_pushinteger(L, pos);
    return 3;
}

/* module registration */

static luaL_Reg funcs[] = {
    { "isbuffer",  lb_isbuf     },
    { "new",       lb_new       },
    { "tostring",  lb_tostring  },
    { "tohex",     lb_tohex     },
    { "quote",     lb_quote     },
    { "topointer", lb_topointer },
    { "ipairs",    lb_ipairs    },
    { "cmp",       lb_cmp       },
    { "eq",        lb_eq        },
    { "lower",     lb_lower     },
    { "upper",     lb_upper     },
    { "rep",       lb_rep       },
    { "len",       lb_len       },
    { "alloc",     lb_alloc     },
    { "free",      lb_free      },
#ifdef LB_SUBBUFFER
    { "sub",       lb_sub       },
    { "subcount",  lb_subcount  },
#endif
    { "append",    lb_append    },
    { "assign",    lb_assign    },
    { "insert",    lb_insert    },
    { "set",       lb_set       },
    { "byte",      lb_byte      },
    { "char",      lb_char      },
    { "clear",     lb_clear     },
    { "copy",      lb_copy      },
    { "move",      lb_move      },
    { "remove",    lb_remove    },
    { "pack",      lb_pack      },
    { "unpack",    lb_unpack    },
};

static luaL_Reg mt[] = {
    { "__concat",  lb_concat    },
    { "__len",     lb_len       },
    { "__tostring",lb_tostring  },
    { "__index",   lb_index     },
    { "__newindex",lb_newindex  },
    { "__gc",      lb_gc        },
    { "__ipairs",  lb_ipairs    },
    { "__pairs",   lb_ipairs    },
};

LUALIB_API int luaopen_buffer(lua_State *L) {
#if LUA_VERSION_NUM >= 502
    luaL_newlib(L, funcs); /* 1 */
#else
    luaL_register(L, BUFFER_LIBNAME, funcs); /* 1 */
#endif
    lua_createtable(L, 0, 1); /* 2 */
    lua_pushliteral(L, "__call"); /* 3 */
    lua_pushcfunction(L, lb_call); /* 4 */
    lua_rawset(L, -3); /* 3,4->2 */
    lua_setmetatable(L, -2); /* 2->1 */

    lua_pushliteral(L, BUFFER_VERSION); /* 2 */
    lua_setfield(L, -2, "_VERSION"); /* 2->1 */
#ifdef LB_SUBBUFFER
    lua_pushinteger(L, LB_SUBS_MAX); /* 2 */
    lua_setfield(L, -2, "_SUBS_MAX"); /* 2->1 */
#endif

    /* create metatable */
    if (luaL_newmetatable(L, BUFFER_MTNAME)) { /* 2 */
#if LUA_VERSION_NUM >= 502
        lua_pushvalue(L, -2); /* (1)->3 */
        luaL_setfuncs(L, mt, 1);
#else
        luaL_register(L, NULL, mt);
        lua_pushliteral(L, "__index"); /* 3 */
        lua_pushvalue(L, -3); /* 4 */
        lua_pushcclosure(L, lb_index, 1); /* 4->4 */
        lua_rawset(L, -3); /* 3,4->2 */
#endif
    }
    lua_pop(L, 1); /* pop 2 */

    return 1;
}

/* cc: flags+='-O4 -mdll -Id:/lua/include' libs+='d:/lua/lua51.dll'
 * cc: flags+='-DLUA_BUILD_AS_DLL -DLB_SUBBUFFER' input='*.c' output='buffer.dll'
 * cc: run='lua test.lua'
 */
