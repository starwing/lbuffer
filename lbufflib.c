#define LUA_LIB
#include "lbuffer.h"
#include "lualib.h" /* for LUA_FILEHANDLE */


#include <stdarg.h>
#include <ctype.h>
#include <string.h>


#ifdef LB_REPLACE_LUA_API
#  undef lua_isstring
#  undef lua_tolstring
#  undef luaL_checklstring
#  undef luaL_optlstring
#endif

#define return_self(L) do { lua_settop(L, 1); return 1; } while (0)
#define uchar(ch) ((unsigned char)(ch))


static size_t posrelat(int offset, size_t len) {
    if (offset >= 1 && (size_t)offset <= len)
        return offset - 1;
    else if (offset <= -1 && (size_t)-offset <= len)
        return offset + len;
    return offset > 0 ? len : 0;
}

static size_t rangerelat(lua_State *L, int idx, size_t *plen) {
    size_t i = posrelat(luaL_optint(L, idx, 1), *plen);
    int sj = luaL_optint(L, idx + 1, -1);
    size_t j = posrelat(sj, *plen);
    *plen = i <= j ? j - i + (sj != 0 && j != *plen) : 0;
    return i;
}

static int type_error(lua_State *L, int narg, const char *tname) {
    const char *msg = lua_pushfstring(L, "%s expected, got %s",
                                      tname, luaL_typename(L, narg));
    return luaL_argerror(L, narg, msg);
}


/* buffer information */

static int Lisbuffer(lua_State *L) {
    return lb_testbuffer(L, 1) != NULL;
}

static int L__tostring(lua_State *L) {
    lb_Buffer *B;
    if ((B = lb_testbuffer(L, 1)) == NULL)
        luaL_tolstring(L, 1, NULL);
    else
        lua_pushlstring(L, B->b, B->n);
    return 1;
}

static int Ltohex(lua_State *L) {
    lb_Buffer B;
    size_t i, len, seplen, gseplen = 0;
    const char *str = lb_tolstring(L, 1, &len);
    const char *sep, *gsep = NULL;
    int upper, group = -1, col = 0;
    int has_group = lua_type(L, 2) == LUA_TNUMBER, arg = 2;
    if (has_group) group = lua_tointeger(L, arg++);
    if (group == 0) group = -1;
    sep = lb_optlstring(L, arg++, "", &seplen);
    if (has_group) gsep = lb_optlstring(L, arg++, "\n", &gseplen);
    upper = lua_toboolean(L, arg++);
    lb_buffinit(L, &B);
    for (i = 0; i < len; ++i, ++col) {
        char *hexa = upper ? "0123456789ABCDEF" : "0123456789abcdef";
        if (col == group)
            col = 0, lb_addlstring(&B, gsep, gseplen);
        else if (i != 0)
            lb_addlstring(&B, sep, seplen);
        lb_addchar(&B, hexa[uchar(str[i]) >> 4]);
        lb_addchar(&B, hexa[uchar(str[i]) & 0xF]);
    }
    lb_pushresult(&B);
    return 1;
}

static int Lquote(lua_State *L) {
    size_t i, len;
    const char *str = lb_tolstring(L, 1, &len);
    lb_Buffer B;
    lb_buffinit(L, &B);
    lb_addchar(&B, '"');
    for (i = 0; i < len; ++i) {
        if (str[i] != '"' && str[i] != '\\' && isprint(str[i]))
            lb_addchar(&B, uchar(str[i]));
        else {
            char *numa = "0123456789";
            switch (uchar(str[i])) {
            case '\a': lb_addstring(&B, "\\a"); break;
            case '\b': lb_addstring(&B, "\\b"); break;
            case '\f': lb_addstring(&B, "\\f"); break;
            case '\n': lb_addstring(&B, "\\n"); break;
            case '\r': lb_addstring(&B, "\\r"); break;
            case '\t': lb_addstring(&B, "\\t"); break;
            case '\v': lb_addstring(&B, "\\v"); break;
            case '\\': lb_addstring(&B, "\\\\"); break;
            default:
               lb_addchar(&B, '\\');
               lb_addchar(&B, numa[uchar(str[i])/100%10]);
               lb_addchar(&B, numa[uchar(str[i])/10%10]);
               lb_addchar(&B, numa[uchar(str[i])%10]);
               break;
            }
        }
    }
    lb_addchar(&B, '"');
    lb_pushresult(&B);
    return 1;
}

static int Ltopointer(lua_State *L) {
    lb_Buffer *B = lb_checkbuffer(L, 1);
    size_t offset = posrelat(luaL_optint(L, 2, 1), B->n);
    lua_pushlightuserdata(L, offset == B->n ? NULL : &B->b[offset]);
    return 1;
}

static int Lcmp(lua_State *L) {
    size_t l1, l2;
    const char *s1 = lb_checklstring(L, 1, &l1);
    const char *s2 = lb_checklstring(L, 2, &l2);
    int res;
    if ((res = memcmp(s1, s2, l1 < l2 ? l1 : l2)) == 0)
        res = l1 - l2;
    lua_pushinteger(L, res > 0 ? 1 : res < 0 ? -1 : 0);
    return 1;
}

static int Leq(lua_State *L) {
    /* We can do this slightly faster than lb_cmp() by comparing
     * string length first.  */
    size_t l1, l2;
    const char *s1 = lb_checklstring(L, 1, &l1);
    const char *s2 = lb_checklstring(L, 2, &l2);
    lua_pushboolean(L, l1 == l2 && memcmp(s1, s2, l1) == 0);
    return 1;
}

static int auxipairs(lua_State *L) {
    lb_Buffer *B = lb_checkbuffer(L, 1);
    int key = luaL_checkint(L, 2) + 1;
    if (key <= 0 || (size_t)key > B->n) return 0;
    lua_pushinteger(L, key);
    lua_pushinteger(L, uchar(B->b[key - 1]));
    return 2;
}

static int Lipairs(lua_State *L) {
    lb_Buffer *B = lb_checkbuffer(L, 1);
    int pos = posrelat(luaL_optint(L, 2, 1), B->n);
    lua_pushcfunction(L, auxipairs);
    lua_insert(L, 1);
    lua_pushinteger(L, pos);
    return 3;
}

static int Lsetlen(lua_State *L) {
    lb_Buffer *B = lb_checkbuffer(L, 1);
    int newlen = lua_tointeger(L, 2);
    if (newlen < 0) newlen += B->n;
    if (newlen < 0) newlen = 0;
    if ((size_t)newlen <= B->n)
        B->n = (size_t)newlen;
    else
        lb_addpadding(B, 0, (size_t)newlen - B->n);
    return_self(L);
}

static int Llen(lua_State *L) {
    size_t len;
    lb_checklstring(L, 1, &len);
    lua_pushinteger(L, len);
    return 1;
}

static int Lbyte(lua_State *L) {
    lb_Buffer *B = lb_checkbuffer(L, 1);
    size_t i, len = B->n, pos = rangerelat(L, 2, &len);
    luaL_checkstack(L, len, "string slice too long");
    for (i = 0; i < len; ++i)
        lua_pushinteger(L, uchar(B->b[pos+i]));
    return len;
}

static int Lchar(lua_State *L) {
    lb_Buffer *B = NULL;
    size_t i, n = lua_gettop(L);
    int invalid = 0;
    char *p;
    if ((B = lb_testbuffer(L, 1)) == NULL) {
        B = lb_newbuffer(L);
        lua_insert(L, 1);
        invalid = -1;
        n += 1;
    }
    p = lb_prepbuffsize(B, n);
    for (i = 2; i <= n; ++i) {
        int c = luaL_checkint(L, i);
        if (uchar(c) != c && invalid > 0)
            invalid += i;
        p[i - 2] = uchar(c);
    }
    lb_addsize(B, n - 1);
    lua_settop(L, 1);
    if (invalid > 0) {
        lua_pushinteger(L, invalid);
        return 2;
    }
    return 1;
}


/* buffer routines */

static void* testudata(lua_State *L, int narg, const char *tname) {
    void *p = lua_touserdata(L, narg);
    if (p != NULL) {  /* value is a userdata? */
        if (lua_getmetatable(L, narg)) {  /* does it have a metatable? */
            lua_getfield(L, LUA_REGISTRYINDEX, tname);  /* get correct metatable */
            if (!lua_rawequal(L, -1, -2))  /* not the same? */
                p = NULL;  /* value is a userdata with wrong metatable */
            lua_pop(L, 2);  /* remove both metatables */
            return p;
        }
    }
    return NULL;  /* value is not a userdata with a metatable */
}

static const char *readfile(lua_State *L, int narg, size_t *plen) {
    /* narg must absolute index */
    if (testudata(L, narg, LUA_FILEHANDLE) != NULL) {
        int top = lua_gettop(L);
        lua_getfield(L, narg, "read");
        lua_insert(L, narg);
        if (top == narg) {
            lua_pushliteral(L, "*a");
            top += 1;
        }
        lua_call(L, top - narg + 1, 1);
        return lua_tolstring(L, narg, plen);
    }
    return NULL;
}

static const char *udtolstring(lua_State *L, int idx, size_t *plen) {
    void *u = NULL;
    if ((u = (void*)readfile(L, idx, plen)) != NULL)
        return (const char*)u;
    if ((u = lua_touserdata(L, idx)) != NULL) {
        int len;
        if (!lua_isnumber(L, idx + 1)) {
            type_error(L, idx + 1, "number");
            return NULL; /* avoid warning */
        }
        len = (int)lua_tointeger(L, idx + 1);
        if (plen != NULL) *plen = len >= 0 ? len : 0;
#ifdef LB_COMPAT_TOLUA
        if (!lua_islightuserdata(L, idx)) /* compatble with tolua */
            u = *(void**)u;
#endif /* LB_COMPAT_TOLUA */
    }
    if (u == NULL && plen != NULL) *plen = 0;
    return (const char*)u;
}

static const char *check_strarg(lua_State *L, int idx,
        size_t *plen, size_t *ppadlen) {
    lb_Buffer *B;
    switch (lua_type(L, idx)) {
    case LUA_TNONE:
    case LUA_TNIL:
        if (plen)    *plen = 0;
        if (ppadlen) *ppadlen = 0;
        return NULL;

    case LUA_TNUMBER:
        if (plen)    *plen = lua_tointeger(L, idx);
        if (lua_type(L, idx+1) == LUA_TNUMBER)
            type_error(L, idx+1, "nil/string/buffer/file/userdata");
        return check_strarg(L, idx+1, ppadlen, NULL);

    case LUA_TSTRING:
        if (ppadlen) *ppadlen = 0;
        {
            size_t len;
            const char *s = lua_tolstring(L, idx, &len);
            size_t pos = rangerelat(L, idx + 1, &len);
            if (plen) *plen = len;
            return &s[pos];
        }

    case LUA_TLIGHTUSERDATA:
        if (ppadlen) *ppadlen = 0;
        return udtolstring(L, idx, plen);

    case LUA_TUSERDATA:
        if (ppadlen) *ppadlen = 0;
        if ((B = lb_testbuffer(L, idx)) != NULL) {
            size_t len = B->n;
            const char *b = &B->b[rangerelat(L, idx + 1, &len)];
            if (plen) *plen = len;
            return b;
        }
        return udtolstring(L, idx, plen);

    default:
        type_error(L, idx, "nil/number/string/buffer/file/userdata");
    }
    return NULL;
}

static void apply_strarg(lb_Buffer *B, size_t pos,
        const char *s, size_t len, size_t padlen) {
    if (pos + len > B->size)
        lb_prepbuffsize(B, pos+len-B->n);
    if (len == 0) return;
    if (s != NULL && padlen == 0)
        memcpy(&B->b[pos], s, len);
    else if (s == NULL || padlen == 1)
        memset(&B->b[pos], s ? s[0] : 0, len);
    else {
        int i = 0, count = len / padlen;
        char *b = &B->b[pos];
        if (s == b) i = 1, b += padlen;
        for (; i < count; ++i, b += padlen)
            memcpy(b, s, padlen);
        memcpy(b, s, len % padlen);
    }
}

static int Lnew(lua_State *L) {
    lb_Buffer *B;
    size_t padlen, len;
    const char *s = check_strarg(L, 1, &len, &padlen);
    B = lb_newbuffer(L);
    apply_strarg(B, 0, s, len, padlen);
    B->b[len] = '\0';
    lb_addsize(B, len);
    return 1;
}

static int map_char(lua_State *L, int (*f)(int)) {
    lb_Buffer *B = lb_checkbuffer(L, 1);
    size_t first = posrelat(luaL_optint(L, 2, 1), B->n);
    size_t last = posrelat(luaL_optint(L, 3, -1), B->n);
    for (; first <= last; ++first)
        B->b[first] = uchar(f(B->b[first]));
    return_self(L);
}

static int Llower(lua_State *L) { return map_char(L, tolower); }
static int Lupper(lua_State *L) { return map_char(L, toupper); }

static int Linsert(lua_State *L) {
    lb_Buffer *B = lb_checkbuffer(L, 1);
    size_t len, padlen, pos = B->n;
    const char *s;
    if (lua_type(L, 2) != LUA_TNUMBER) { /* append */
        s = check_strarg(L, 2, &len, &padlen);
        apply_strarg(B, pos, s, len, padlen);
        lb_addsize(B, len);
    }
    else { /* insert */
        pos = posrelat(lua_tointeger(L, 2), B->n);
        s = check_strarg(L, 3, &len, &padlen);
        if (len != 0) {
            lb_prepbuffsize(B, len);
            memmove(&B->b[pos+len], &B->b[pos], B->n-pos);
            apply_strarg(B, pos, s, len, padlen);
            lb_addsize(B, len);
        }
    }
    return_self(L);
}

static int Lclear(lua_State *L) {
    lb_Buffer *B = lb_checkbuffer(L, 1);
    size_t padlen, len = B->n, pos = rangerelat(L, 2, &len);
    const char *s = luaL_optlstring(L, 4, NULL, &padlen);
    apply_strarg(B, pos, s, len, padlen);
    return_self(L);
}

static int Lset(lua_State *L) {
    lb_Buffer *B = lb_checkbuffer(L, 1);
    size_t len, padlen, pos;
    const char *s;
    if (lua_type(L, 2) != LUA_TNUMBER) { /* assign */
        s = check_strarg(L, 2, &len, &padlen);
        apply_strarg(B, 0, s, len, padlen);
        B->n = len;
    }
    else { /* overwrite */
        pos = posrelat(lua_tointeger(L, 2), B->n);
        s = check_strarg(L, 3, &len, &padlen);
        apply_strarg(B, pos, s, len, padlen);
        if (B->n < pos + len)
            B->n = pos + len;
    }
    return_self(L);
}

static int Lrep(lua_State *L) {
    lb_Buffer *B = lb_checkbuffer(L, 1);
    size_t len = B->n;
    const char *str = B->b;
    int rep = 0;
    if (lua_type(L, 2) == LUA_TNUMBER)
        rep = lua_tointeger(L, 2);
    else if ((str = lb_tolstring(L, 2, &len)) != NULL)
        rep = luaL_checkint(L, 3);
    else type_error(L, 2, "number/buffer/string");
    if (rep < 0) rep = 0;
    apply_strarg(B, 0, str, len*rep, len);
    B->n = len*rep;
    return_self(L);
}

static int Lcopy(lua_State *L) {
    lb_Buffer *B = lb_checkbuffer(L, 1);
    size_t len = B->n, pos = rangerelat(L, 2, &len);
    lb_pushbuffer(L, &B->b[pos], len);
    return 1;
}

static int Lmove(lua_State *L) {
    lb_Buffer *B = lb_checkbuffer(L, 1);
    int dst = luaL_checkint(L, 2);
    size_t len = B->n, pos = rangerelat(L, 3, &len);

    if (dst > 0) dst -= 1;
    if (dst < 0) dst += B->n;
    if (dst < 0) dst = 0;

    if (dst+len > B->n)
        lb_prepbuffsize(B, dst + len - B->n);
    memmove(&B->b[dst], &B->b[pos], len);
    if ((size_t)dst > B->n)
        memset(&B->b[B->n], 0, dst - B->n);
    if (B->n < dst+len)
        B->n = dst+len;
    return_self(L);
}

static int Lremove(lua_State *L) {
    lb_Buffer *B = lb_checkbuffer(L, 1);
    size_t len = B->n, pos = rangerelat(L, 2, &len);
    size_t end = pos + len;
    if (len != 0)
        memmove(&B->b[pos], &B->b[end], B->n - end);
    B->n -= len;
    return_self(L);
}

static void my_strrev(char *p1, char *p2) {
    for (--p2; p1 < p2; ++p1, --p2) {
        char t = *p1;
        *p1 = *p2;
        *p2 = t;
    }
}

static int Lreverse(lua_State *L) {
    lb_Buffer *B = lb_checkbuffer(L, 1);
    size_t len = B->n, pos = rangerelat(L, 2, &len);
    my_strrev(&B->b[pos], &B->b[pos + len]);
    return_self(L);
}

static void exchange(char *p1, char *p2, char *p3) {
    my_strrev(p1, p2);
    my_strrev(p2, p3);
    my_strrev(p1, p3);
}

static void exchange_split(char *p1, char *p2, char *p3, char *p4) {
    my_strrev(p1,  p2);
    my_strrev(p2,  p3);
    my_strrev(p3,  p4);
    my_strrev(p1,  p4);
}

static int Lswap(lua_State *L) {
    size_t p1, l1, p2, l2;
    lb_Buffer *B = lb_checkbuffer(L, 1);
    if (lua_isnoneornil(L, 3)) {
        p2 = posrelat(luaL_checkint(L, 2), B->n);
        l2 = B->n - p2;
        p1 = 0, l1 = p2;
    }
    else {
        l1 = B->n, p1 = rangerelat(L, 2, &l1);
        l2 = B->n, p2 = rangerelat(L, 4, &l2);
        if (lua_isnoneornil(L, 5)) {
            size_t new_p2 = p1 + l1;
            l2 = p2 - new_p2 + 1;
            p2 = new_p2;
        }
        else if (p1 + l1 > p2) {
            size_t old_l1 = l1, old_p2 = p2;
            l1 = p2 - p1;
            p2 = p1 + old_l1;
            l2 -= p2 - old_p2;
        }
    }
    if (p1 + l1 == p2)
        exchange(&B->b[p1], &B->b[p2], &B->b[p2 + l2]);
    else
        exchange_split(&B->b[p1], &B->b[p1 + l1],
                       &B->b[p2], &B->b[p2 + l2]);
    return_self(L);
}


/* bianry operations */

static size_t check_giargs(lua_State *L, int narg, size_t len, size_t *wide, int *bigendian) {
    size_t pos = posrelat(luaL_optint(L, narg, 1), len);
    *wide = luaL_optint(L, narg + 1, 4);
    if (*wide < 1 || *wide > 8)
        luaL_argerror(L, narg + 1, "only 1 to 8 wide support");
    switch (*luaL_optlstring(L, narg + 2, "native", NULL)) {
    case 'b': case 'B': case '>': *bigendian = 1; break;
    case 'l': case 'L': case '<': *bigendian = 0; break;
    case 'n': case 'N': case '=': *bigendian = LB_BIGENDIAN; break;
    default: luaL_argerror(L, 4, "only \"big\" or \"little\" or \"native\" endian support");
    }
    return pos;
}

static int Lgetint(lua_State *L) {
    lua_Integer i;
    size_t len;
    const char *str = lb_checklstring(L, 1, &len);
    int bigendian;
    size_t wide, pos = check_giargs(L, 2, len, &wide, &bigendian);
    if (pos + wide > len) return 0;
    lb_unpackint(&str[pos], wide, bigendian, &i);
    lua_pushinteger(L, i);
    return 1;
}

static int Lgetuint(lua_State *L) {
    lua_Integer i;
    size_t len;
    const char *str = lb_checklstring(L, 1, &len);
    int bigendian;
    size_t wide, pos = check_giargs(L, 2, len, &wide, &bigendian);
    if (pos + wide > len) return 0;
    lb_unpackuint(&str[pos], wide, bigendian, &i);
    lua_pushinteger(L, i);
    return 1;
}

static int Lsetuint(lua_State *L) {
    lb_Buffer *B = lb_checkbuffer(L, 1);
    lua_Integer i = luaL_checkinteger(L, 2);
    int bigendian;
    size_t wide, pos = check_giargs(L, 3, B->n, &wide, &bigendian);
    lb_atpos(B, pos, lb_packint(B, wide, bigendian, i));
    lua_settop(L, 1);
    return 1;
}


/* pack/unpack */

typedef struct parse_info {
    lb_Buffer *B;       /* working lb_Buffer */
    size_t pos;         /* current working position in buffer */
    int is_bigendian;
    int is_pack;
    int is_stringkey;
    unsigned int flags; /* see PIF_* flags below */
    int narg, nret;     /* numbers of arguments/return values */
    int level, index;   /* the level/index of nest table */
    int fmtpos;         /* the pos of fmt */
    const char *fmt;    /* the format string pointer */
} parse_info;

#define I(field) (info->field)

static int source(parse_info *info) {
    if (I(level) == 0)
        lua_pushvalue(I(B)->L, I(narg)++);
    else {
        if (!I(is_stringkey))
            lua_pushinteger(I(B)->L, I(index)++);
        lua_gettable(I(B)->L, -2);
    }
    return -1;
}

static const char *source_lstring(parse_info *info, size_t *plen) {
    int narg = source(info);
    if (lb_isbufferorstring(I(B)->L, narg))
        return lb_tolstring(I(B)->L, narg, plen);
    else if (I(level) == 0)
        type_error(I(B)->L, I(narg) - 1, "string");
    else {
        lua_pushfstring(I(B)->L,
                "buffer/string expected in [%d], got %s",
                I(index) - 1, luaL_typename(I(B)->L, narg));
        luaL_argerror(I(B)->L, I(narg) - 1, lua_tostring(I(B)->L, -1));
    }
    return NULL;
}

static lua_Integer source_integer(parse_info *info) {
    int narg = source(info);
    if (lua_isnumber(I(B)->L, narg))
        return lua_tointeger(I(B)->L, narg);
    else if (I(level) == 0)
        type_error(I(B)->L, I(narg) - 1, "integer");
    else {
        lua_pushfstring(I(B)->L,
                "integer expected in [%d], got %s",
                I(index) - 1, luaL_typename(I(B)->L, narg));
        luaL_argerror(I(B)->L, I(narg) - 1, lua_tostring(I(B)->L, -1));
    }
    return 0;
}

static lua_Number source_number(parse_info *info) {
    int narg = source(info);
    if (lua_isnumber(I(B)->L, narg))
        return lua_tonumber(I(B)->L, narg);
    else if (I(level) == 0)
        type_error(I(B)->L, I(narg) - 1, "number");
    else {
        lua_pushfstring(I(B)->L,
                "number expected in [%d], got %s",
                I(index) - 1, luaL_typename(I(B)->L, narg));
        luaL_argerror(I(B)->L, I(narg) - 1, lua_tostring(I(B)->L, -1));
    }
    return 0;
}

static void sink(parse_info *info) {
    if (I(level) == 0)
        ++I(nret);
    else {
        if (!I(is_stringkey)) {
            lua_pushinteger(I(B)->L, I(index)++);
            lua_insert(I(B)->L, -2);
        }
        lua_settable(I(B)->L, -3);
    }
}

#define pack_checkstack(n) \
    luaL_checkstack(I(B)->L, (n), "too much top level formats")

static int fmterror(parse_info *info, const char *msgfmt, ...) {
    const char *msg;
    va_list list;
    va_start(list, msgfmt);
    msg = lua_pushvfstring(I(B)->L, msgfmt, list);
    va_end(list);
    return luaL_argerror(I(B)->L, I(fmtpos), msg);
}

static const char *lb_pushlstring(lua_State *L, const char *str, size_t len) {
    return lb_pushbuffer(L, str, len)->b;
}

#if LUA_VERSION_NUM < 502
static const char *my_lua_pushlstring(lua_State *L, const char *str, size_t len) {
    lua_pushlstring(L, str, len);
    return str;
}
#else
#  define my_lua_pushlstring lua_pushlstring
#endif

static int do_packfmt(parse_info *info, char fmt, size_t wide, int count) {
    size_t pos;
    int top = lua_gettop(I(B)->L);
    typedef const char *(*pushlstring_t)(lua_State * L, const char * str, size_t len);
    pushlstring_t pushlstring = isupper(fmt) ?  lb_pushlstring : my_lua_pushlstring;

#define SINK() do { if (I(level) == 0 && count < 0) pack_checkstack(1); \
        sink(info); } while (0)
#define BEGIN_PACK() \
    if (I(is_pack)) { \
        while ((count >= 0 || I(narg) <= top) && count--)
#define BEGIN_UNPACK() \
    } else { \
        size_t blen = I(B)->n; \
        if (count > 0) pack_checkstack(count); \
        while (count--)
#define END_PACK() \
    } break

    switch (fmt) {
    case 's': case 'S': /* zero-terminated string */
    case 'z': case 'Z': /* zero-terminated string */
        BEGIN_PACK() {
            size_t len;
            const char *s = source_lstring(info, &len);
            if (wide != 0 && len > wide) len = wide;
            lb_atpos(I(B), I(pos), {
                lb_addlstring(I(B), s, len);
                lb_addchar(I(B), '\0');
            });
            I(pos) += len + 1;
            lua_pop(I(B)->L, 1); /* pop source */
        }
        BEGIN_UNPACK() {
            size_t len = 0;
            while ((wide == 0 || len < wide)
                    && I(pos) + len < blen
                    && I(B)->b[I(pos) + len] != '\0')
                ++len;
            if ((fmt == 'z' || fmt == 'Z')
                    && (I(pos) + len) >= blen
                    && (wide == 0 || len < wide))
                return 0;
            pushlstring(I(B)->L, &I(B)->b[I(pos)], len); SINK();
            I(pos) += len;
            if (I(pos) < blen && I(B)->b[I(pos)] == '\0') ++I(pos);
        }
        END_PACK();

    case 'b': case 'B': /* byte */
    case 'c': case 'C': /* char */
        if (wide == 0) wide = 1;
        BEGIN_PACK() {
            size_t len;
            const char *s = source_lstring(info, &len);
            if (wide != 0 && len > wide) len = wide;
            lb_atpos(I(B), I(pos), {
                lb_addlstring(I(B), s, len);
                if (wide > len)
                    lb_addpadding(I(B), 0, wide - len);
            });
            I(pos) += len;
            lua_pop(I(B)->L, 1); /* pop source */
        }
        BEGIN_UNPACK() {
            if ((fmt == 'b' || fmt == 'B')
                    && (I(pos) + wide >= blen))
                return 0;
            if (I(pos) + wide > blen) wide = blen - I(pos);
            pushlstring(I(B)->L, &I(B)->b[I(pos)], wide); SINK();
            I(pos) += wide;
        }
        END_PACK();

    case 'd': case 'D': /* length preceded data */
    case 'p': case 'P': /* length preceded string */
        if (wide == 0) wide = 4;
        if (wide > 8) fmterror(
                info,
                "invalid wide of format '%c': only 1 to 8 supported.", fmt);
        BEGIN_PACK() {
            size_t len;
            const char *str = source_lstring(info, &len);
            lb_atpos(I(B), I(pos), {
                lb_packint(I(B), wide, I(is_bigendian),
                        (lua_Integer)len);
                lb_addlstring(I(B), str, len);
            });
            I(pos) += wide+len;
            lua_pop(I(B)->L, 1); /* pop source */
        }
        BEGIN_UNPACK() {
            lua_Integer len;
            if (I(pos) + wide > blen) return 0;
            lb_unpackuint(&I(B)->b[I(pos)], wide,
                    I(is_bigendian), &len);
            if (len > (size_t)(~(size_t)0)/2)
                fmterror(info, "string too big in format '%c'", fmt);
            if ((fmt == 'd' || fmt == 'D') && I(pos) + wide + len > blen)
                return 0;
            I(pos) += wide;
            if (I(pos) + len > blen)
                len = blen - I(pos);
            pushlstring(I(B)->L, &I(B)->b[I(pos)], len); SINK();
            I(pos) += len;
        }
        END_PACK();

    case 'i': case 'I': /* int */
    case 'u': case 'U': /* unsigned int */
        if (wide == 0) wide = 4;
        if (wide > 8) fmterror(
                info,
                "invalid wide of format '%c': only 1 to 8 supported.", fmt);
        BEGIN_PACK() {
            lb_atpos(I(B), I(pos), lb_packint(I(B), wide, I(is_bigendian),
                        source_integer(info)));
            I(pos) += wide;
            lua_pop(I(B)->L, 1); /* pop source */
        }
        BEGIN_UNPACK() {
            lua_Integer i;
            if (I(pos) + wide > blen) return 0;
            if (fmt == 'u' || fmt == 'U')
                lb_unpackuint(&I(B)->b[I(pos)], wide, 
                        I(is_bigendian), &i);
            else
                lb_unpackint(&I(B)->b[I(pos)], wide, 
                        I(is_bigendian), &i);
            I(pos) += wide;
            lua_pushinteger(I(B)->L, i); SINK();
        }
        END_PACK();

    case 'f': case 'F': /* float */
        if (wide == 0) wide = 4;
        if (wide != 4 && wide != 8) fmterror(
                info,
                "invalid wide of format '%c': only 4 or 8 supported.", fmt);
        BEGIN_PACK() {
            lua_Number num = source_number(info);
            lb_atpos(I(B), I(pos),
                    lb_packfloat(I(B), wide, I(is_bigendian), num));
            I(pos) += wide;
            lua_pop(I(B)->L, 1); /* pop source */
        }
        BEGIN_UNPACK() {
            lua_Number n;
            if (I(pos) + wide > blen) return 0;
            lb_unpackfloat(&I(B)->b[I(pos)], wide,
                    I(is_bigendian), &n);
            I(pos) += wide;
            lua_pushnumber(I(B)->L, n); SINK();
        }
        END_PACK();

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
            fmterror(info, "invalid count of format '%c'", fmt);
        if (pos > I(B)->n) pos = I(B)->n;
        I(pos) = pos;
        break;

    default:
        fmterror(info, "invalid format '%c'", fmt);
        break;
    }
    return 1;
#undef SINK
#undef END_PACK
#undef BEGIN_UNPACK
#undef BEGIN_PACK
}

static int do_delimiter(parse_info *info, char fmt) {
    switch (fmt) {
    case '{':
        /* when meet a open-block, 3 value will be pushed onto stack:
         * the current index, the string key (or nil), and a new table
         * of block.  so the extra used stack space equals level * 3.
         * stack: [args] [[index][stringkey][table]] ... [stringkey]
         * NOTE: if you changed stack structure, you *MUST* change the
         * pop stack behavior in parse_fmt !!  */
        luaL_checkstack(I(B)->L, 4, "table level too big");
        if (!I(is_pack)) {
            if (!I(is_stringkey))
                lua_pushnil(I(B)->L);
            lua_pushinteger(I(B)->L, I(index));
            lua_insert(I(B)->L, -2);
            lua_newtable(I(B)->L);
        }
        else {
            source(info);
            lua_pushinteger(I(B)->L, I(index));
            lua_insert(I(B)->L, -2);
            lua_pushnil(I(B)->L);
            lua_insert(I(B)->L, -2);
        }
        I(level) += 1;
        I(index) = 1;
        break;

    case '}':
        if (I(level) <= 0)
            fmterror(info, "unbalanced '}' in format near "LUA_QS, I(fmt) - 1);
        I(index) = lua_tointeger(I(B)->L, -3);
        I(level) -= 1;
        lua_remove(I(B)->L, -3);
        if (I(is_pack)) {
            lua_pop(I(B)->L, 2);
        }
        else {
            if (!lua_isnil(I(B)->L, -2))
                I(is_stringkey) = 1;
            else {
                lua_remove(I(B)->L, -2);
                I(is_stringkey) = 0;
            }
            pack_checkstack(1);
            sink(info);
        }
        break;

    case '#': /* current pos */
        if (I(level) != 0)
            fmterror(info, "can only retrieve position out of block");
        lua_pushinteger(I(B)->L, I(pos) + 1);
        pack_checkstack(1);
        sink(info);
        break;

    case '<': /* little bigendian */
        I(is_bigendian) = 0; break;
    case '>': /* big bigendian */
        I(is_bigendian) = 1; break;
    case '=': /* native bigendian */
#if LB_BIGENDIAN
        I(is_bigendian) = 1; break;
#else
        I(is_bigendian) = 0; break;
#endif
        break;

    default:
        return 0;
    }
    return 1;
}

#define skip_white(s) do { while (*(s) == ' ' || *(s) == '\t' \
                || *(s) == '\r'|| *(s) == '\n' || *(s) == ',') ++(s); } while(0)

static int parse_optint(const char **str, unsigned int *pn) {
    unsigned int n = 0;
    const char *oldstr = *str;
    while (isdigit(**str)) n = n * 10 + uchar(*(*str)++ - '0');
    if (*str != oldstr) *pn = n;
    return n;
}

static void parse_fmtargs(parse_info *info, size_t *wide, int *count) {
    skip_white(I(fmt));
    parse_optint(&I(fmt), wide);
    skip_white(I(fmt));
    if (*I(fmt) == '*') {
        size_t ucount = *count;
        ++I(fmt);
        skip_white(I(fmt));
        parse_optint(&I(fmt), &ucount);
        *count = ucount;
    }
    else if (*I(fmt) == '$') {
        ++I(fmt);
        *count = -1;
    }
    skip_white(I(fmt));
}

static void parse_stringkey(parse_info *info) {
    skip_white(I(fmt));
    if (isalpha(*I(fmt)) || *I(fmt) == '_') {
        const char *curpos = I(fmt)++, *end;
        while (isalnum(*I(fmt)) || *I(fmt) == '_')
            ++I(fmt);
        end = I(fmt);
        skip_white(I(fmt));
        if (*I(fmt) != '=')
            I(fmt) = curpos;
        else {
            ++I(fmt);
            skip_white(I(fmt));
            if (*I(fmt) == '}' || *I(fmt) == '\0')
                fmterror(info, "key without format near "LUA_QS, curpos);
            if (I(level) == 0)
                fmterror(info, "key at top level near "LUA_QS, curpos);
            lua_pushlstring(I(B)->L, curpos, end - curpos);
            I(is_stringkey) = 1;
            return;
        }
    }
    I(is_stringkey) = 0;
}

static int parse_fmt(parse_info *info) {
    int fmt, insert_pos = 0;
    skip_white(I(fmt));
    if (*I(fmt) == '!') {
        insert_pos = 1; /* only enabled in unpack */
        ++I(fmt);
    }
    while (parse_stringkey(info), (fmt = *I(fmt)++) != '\0') {
        if (!do_delimiter(info, fmt)) {
            size_t wide = 0;
            int count = 1;
            parse_fmtargs(info, &wide, &count);
            if (!do_packfmt(info, fmt, wide, count)) {
                if (I(is_stringkey))
                    lua_pop(I(B)->L, 1);
                lua_pop(I(B)->L, I(level) * 3); /* 3 values per level */
                I(level) = 0;
                lua_pushnil(I(B)->L); ++I(nret);
                skip_white(I(fmt));
                /* skip any block */
                while (*I(fmt) == '{' || *I(fmt) == '}') {
                    ++I(fmt);
                    skip_white(I(fmt));
                }
                if ((fmt = *I(fmt)++) == '#')
                    do_delimiter(info, fmt);
                break;
            }
        }
    }
    if (I(level) != 0)
        fmterror(info, "unbalanced '{' in format");
    if (insert_pos) {
        lua_pushinteger(I(B)->L, I(pos) + 1);
        lua_insert(I(B)->L, -(++I(nret)));
    }
    return I(nret);
}

static int do_pack(lb_Buffer *B, int narg, int pack) {
    lua_State *L = B->L;
    parse_info info = {NULL};
    info.B = B;
    info.narg = narg;
    info.is_pack = pack;
#if LB_BIGENDIAN
    info.is_bigendian = 1;
#endif
    if (lua_type(L, info.narg) == LUA_TNUMBER)
        info.pos = posrelat(lua_tointeger(L, info.narg++), info.B->n);
    info.fmtpos = info.narg++;
    info.fmt = lb_checklstring(L, info.fmtpos, NULL);
    parse_fmt(&info);
    if (pack) {
        lua_pushinteger(L, info.pos + 1);
        lua_insert(L, -(++info.nret));
    }
    return info.nret;
}

static int Lpack(lua_State *L) {
    int res;
    lb_Buffer *B;
    if ((B = lb_testbuffer(L, 1)) != NULL) {
        res = do_pack(B, 2, 1);
        lua_pushvalue(L, 1);
    }
    else {
        lb_Buffer buff;
        lb_buffinit(L, &buff);
        res = do_pack(&buff, 1, 1);
        lb_copybuffer(&buff);
    }
    lua_insert(L, -res-1);
    return res+1;
}

static int Lunpack(lua_State *L) {
    if (lua_type(L, 1) == LUA_TSTRING) {
        lb_Buffer B; /* a fake buffer */
        lb_buffinit(L, &B);
        /* in unpack, all functions never changed the content of
         * buffer, so use force cast is safe */
        B.b = (char*)lua_tolstring(L, 1, &B.n);
        return do_pack(&B, 2, 0);
    }
    return do_pack(lb_checkbuffer(L, 1), 2, 0);
}

#undef I


/* meta methods */

static int L__gc(lua_State *L) {
    lb_Buffer *B;
    if ((B = lb_testbuffer(L, 1)) != NULL)
        lb_resetbuffer(B);
    return 0;
}

static int L__concat(lua_State *L) {
    size_t l1, l2;
    const char *s1 = lb_checklstring(L, 1, &l1);
    const char *s2 = lb_checklstring(L, 2, &l2);
    lb_Buffer *B = lb_newbuffer(L);
    lb_addlstring(B, s1, l1);
    lb_addlstring(B, s2, l2);
    return 1;
}

static int L__index(lua_State *L) {
    lb_Buffer *B;
    int pos;

    switch (lua_type(L, 2)) {
    case LUA_TSTRING:
        if (lua_getmetatable(L, 1)) {
            lua_pushvalue(L, 2);
            lua_rawget(L, -2);
            return 1;
        }
        return 0;

    case LUA_TNUMBER:
        B = lb_checkbuffer(L, 1);
        pos = lua_tointeger(L, 2);
        if (pos == 0 || (pos = posrelat(pos, B->n)) == B->n)
            return 0;
        lua_pushinteger(L, uchar(B->b[pos]));
        return 1;

    default:
        return 0;
    }
}

static int L__newindex(lua_State *L) {
    lb_Buffer *B = lb_checkbuffer(L, 1);
    int ch, pos = luaL_checkint(L, 2);
    size_t len;
    const char *s;

    if ((size_t)-pos < B->n || (size_t)pos > B->n+1)
        luaL_error(L, "invalid index #%d to buffer, should in %d-%d",
                pos, 1, B->n+1);

    if (pos < 0) pos += B->n;
    else pos -= 1;

    switch (lua_type(L, 3)) {
    case LUA_TNUMBER:
        ch = (int)lua_tointeger(L, 3);
set_char:
        if (pos != B->n)
            B->b[pos] = uchar(ch);
        else {
            lb_prepbuffsize(B, 1);
            B->b[pos] = uchar(ch);
            lb_addsize(B, 1);
        }
        return 1;

    case LUA_TSTRING:
    case LUA_TUSERDATA:
        s = lb_checklstring(L, 3, &len);
        if (len == 1) {
            ch = s[0];
            goto set_char;
        }
        else if (pos == B->n) { /* append */
            lb_prepbuffsize(B, len);
            memcpy(&B->b[pos], s, len);
            lb_addsize(B, len);
        }
        else { /* replace */
            lb_prepbuffsize(B, len - 1);
            memmove(&B->b[pos + len], &B->b[pos + 1], B->n - pos - 1);
            memcpy(&B->b[pos], s, len);
            lb_addsize(B, len - 1);
        }
        return 1;

    case LUA_TNIL:
    case LUA_TNONE:
        if (pos == B->n-1) B->n -= 1;

    default:
        type_error(L, 3, "string/buffer/number");
        return 0;
    }
}

static int Llibcall(lua_State *L) {
    lua_pushcfunction(L, Lnew);
    lua_replace(L, 1);
    lua_call(L, lua_gettop(L)-1, LUA_MULTRET);
    return lua_gettop(L);
}

#ifdef LB_REDIR_STRLIB
static int redir_to_strlib(lua_State *L, const char *name) {
    lb_Buffer *B = lb_testbuffer(L, 1);
    int i, base = 1, top = lua_gettop(L);
    if (B != NULL) {
        lua_pushlstring(L, B->b, B->n);
        lua_insert(L, 2);
        base += 1;
        top += 1;
    }
    for (i = base; i <= top; ++i) {
        lb_Buffer *b = lb_testbuffer(L, i);
        if (b != NULL) {
            lua_pushlstring(L, B->b, B->n);
            lua_replace(L, i);
        }
    }
    lua_getglobal(L, "string");
    lua_getfield(L, -1, name);
    lua_remove(L, -2);
    if (lua_isnil(L, -1))
        return luaL_error(L, "can not find function "LUA_QS" in "LUA_QS,
                          name, "string");
    lua_insert(L, base);
    lua_call(L, top - base + 1, LUA_MULTRET);
    if (lua_isstring(L, 2) && B != NULL) {
        size_t len;
        const char *str = lua_tolstring(L, 2, &len);
        B->n = 0;
        memcpy(lb_prepbuffsize(B, len), str, len);
        B->n = len;
        lua_remove(L, 2);
    }
    return lua_gettop(L);
}

#define redir_functions(X) \
    X(dump)   X(find)   X(format) X(gmatch) X(gsub)   X(match)

#define X(name) \
    static int lbR_##name (lua_State *L) \
    { return redir_to_strlib(L, #name); }
redir_functions(X)
#undef X
#endif /* LB_REDIR_STRLIB */

/* module registration */

int luaopen_buffer(lua_State *L) {
    luaL_Reg libs[] = {
#ifdef LB_REDIR_STRLIB
#define ENTRY(name) { #name, lbR_##name },
        redir_functions(ENTRY)
#undef  ENTRY
#endif /* LB_REDIR_STRLIB */

#define ENTRY(name) { #name, L##name }
        ENTRY(new),
        ENTRY(__gc),
        ENTRY(__concat),
        ENTRY(__tostring),
        ENTRY(__index),
        ENTRY(__newindex),
        { "__len", Llen },
        { "__eq",  Leq  },
#if LUA_VERSION_NUM >= 502
        { "__ipairs",  Lipairs    },
        { "__pairs",   Lipairs    },
#endif

        /* request */
        ENTRY(byte),
        ENTRY(cmp),
        ENTRY(eq),
        ENTRY(ipairs),
        ENTRY(isbuffer),
        ENTRY(len),
        ENTRY(quote),
        ENTRY(topointer),

        /* modify */
        ENTRY(char),
        ENTRY(clear),
        ENTRY(copy),
        ENTRY(insert),
        ENTRY(lower),
        ENTRY(move),
        ENTRY(remove),
        ENTRY(rep),
        ENTRY(reverse),
        ENTRY(set),
        ENTRY(setlen),
        ENTRY(swap),
        ENTRY(upper),

        /* binary support */
        ENTRY(tohex),
        ENTRY(getint),
        ENTRY(getuint),
        ENTRY(pack),
        { "setint", Lsetuint },
        ENTRY(setuint),
        ENTRY(unpack),
#undef ENTRY
        { NULL, NULL }
    };

    /* create metatable */
    if (luaL_newmetatable(L, LB_LIBNAME)) {
        luaL_setfuncs(L, libs, 0); /* 3->2 */
        lua_pushvalue(L, -1); /* 3 */
        lua_rawsetp(L, LUA_REGISTRYINDEX, (void*)LB_METAKEY); /* 3->env */
    }

    lua_createtable(L, 0, 1); /* 2 */
    lua_pushcfunction(L, Llibcall); /* 3 */
    lua_setfield(L, -2, "__call"); /* 3->2 */
    lua_setmetatable(L, -2); /* 2->1 */

    lua_pushliteral(L, LB_VERSION); /* 2 */
    lua_setfield(L, -2, "_VERSION"); /* 2->1 */

    return 1;
}

/*
 * cc: flags+='-shared -s -O2 -Wall -pedantic' libs+='-llua52'
 * cc: flags+='-DLB_REDIR_STRLIB=1 -DLB_FILEHANDLE -DLUA_BUILD_AS_DLL'
 * cc: input='lb*.c' output='buffer.dll' run='lua test.lua' */
