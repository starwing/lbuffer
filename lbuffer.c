#define LUA_LIB
#include "lbuffer.h"


#include <string.h>


#define MAX_SIZE_T ((size_t)(~(size_t)0) - 2)


#if LUA_VERSION_NUM < 502
void lua_rawgetp(lua_State *L, int narg, const void *p) {
    lua_pushlightuserdata(L, (void*)p);
    lua_rawget(L, narg < 0 ? narg - 1 : narg);
}

void lua_rawsetp(lua_State *L, int narg, const void *p) {
    lua_pushlightuserdata(L, (void*)p);
    lua_insert(L, -2);
    lua_rawset(L, narg < 0 ? narg - 1 : narg);
}

int lua_absindex(lua_State *L, int idx) {
    return (idx > 0 || idx <= LUA_REGISTRYINDEX)
           ? idx
           : lua_gettop(L) + idx + 1;
}

const char *luaL_tolstring(lua_State *L, int idx, size_t *plen) {
    if (!luaL_callmeta(L, idx, "__tostring")) {  /* no metafield? */
        switch (lua_type(L, idx)) {
            case LUA_TNUMBER:
            case LUA_TSTRING:
                lua_pushvalue(L, idx);
                break;
            case LUA_TBOOLEAN:
                lua_pushstring(L, (lua_toboolean(L, idx) ? "true" : "false"));
                break;
            case LUA_TNIL:
                lua_pushliteral(L, "nil");
                break;
            default:
                lua_pushfstring(L, "%s: %p", luaL_typename(L, idx),
                        lua_topointer(L, idx));
                break;
        }
    }
    return lua_tolstring(L, -1, plen);
}
#endif

#ifdef LB_REPLACE_LUA_API
#  undef lua_isstring
#  undef lua_tolstring
#  undef luaL_checklstring
#  undef luaL_optlstring
#endif

static int type_error(lua_State *L, int narg, const char *tname) {
    const char *msg = lua_pushfstring(L, "%s expected, got %s",
                                      tname, luaL_typename(L, narg));
    return luaL_argerror(L, narg, msg);
}


/* luaL_Buffer compatible interface */

LB_API void lb_buffinit(lua_State *L, lb_Buffer *B) {
    B->L = L;
    B->b = B->initb;
    B->n = 0;
    B->size = LUAL_BUFFERSIZE;
}

LB_API char *lb_prepbuffsize(lb_Buffer *B, size_t sz) {
    lua_State *L = B->L;
    if (B->size - B->n < sz) {  /* not enough space? */
        char *newbuff;
        size_t newsize = B->size * 2;  /* double buffer size */
        if (newsize - B->n < sz)  /* not big enough? */
            newsize = B->n + sz;
        if (newsize < B->n || newsize - B->n < sz)
            luaL_error(L, "buffer too large");
        /* create larger buffer */
        newbuff = (char*)lua_newuserdata(L, newsize * sizeof(char));
        /* move content to new buffer */
        memcpy(newbuff, B->b, B->n * sizeof(char));
        /* remove old buffer and archor new buffer */
        lua_pushvalue(L, -1);
        lua_rawsetp(L, LUA_REGISTRYINDEX, B);
        B->b = newbuff;
        B->size = newsize;
    }
    return &B->b[B->n];
}

LB_API void lb_addlstring(lb_Buffer *B, const char *s, size_t l) {
    char *b = lb_prepbuffsize(B, l);
    memcpy(b, s, l * sizeof(char));
    lb_addsize(B, l);
}

LB_API void lb_addstring(lb_Buffer *B, const char *s) {
    lb_addlstring(B, s, strlen(s));
}

LB_API void lb_addpadding(lb_Buffer *B, int ch, size_t l) {
    char *b = lb_prepbuffsize(B, l);
    memset(b, ch, l * sizeof(char));
    lb_addsize(B, l);
}

LB_API void lb_addvalue(lb_Buffer *B) {
    lua_State *L = B->L;
    size_t l;
    const char *s = luaL_tolstring(L, -1, &l);
    lb_addlstring(B, s, l);
    lua_pop(L, 1);
}

LB_API void lb_pushresult(lb_Buffer *B) {
    lua_pushlstring(B->L, B->b, B->n);
    lb_resetbuffer(B);
}


/* buffer type routines */

static void get_metatable_fast(lua_State *L) {
    lua_rawgetp(L, LUA_REGISTRYINDEX, (void*)LB_METAKEY);
}

LB_API lb_Buffer *lb_newbuffer(lua_State *L) {
    lb_Buffer *B = (lb_Buffer*)lua_newuserdata(L, sizeof(lb_Buffer));
    lb_buffinit(L, B);
    get_metatable_fast(L);
    lua_setmetatable(L, -2);
    return B;
}

LB_API lb_Buffer *lb_copybuffer(lb_Buffer *B) {
    lb_Buffer *nb = lb_newbuffer(B->L);
    lb_addlstring(nb, B->b, B->n);
    return nb;
}

LB_API void lb_resetbuffer(lb_Buffer *B) {
    lua_State *L = B->L;
    if (B->b != B->initb) { /* remove old buffer */
        lua_pushnil(L);
        lua_rawsetp(L, LUA_REGISTRYINDEX, B);
    }
    lb_buffinit(L, B);
}

LB_API lb_Buffer *lb_testbuffer(lua_State *L, int narg) {
    void *p = lua_touserdata(L, narg);
    if (p != NULL &&  /* value is a userdata? */
            lua_getmetatable(L, narg)) {  /* does it have a metatable? */
        /* get correct metatable */
        get_metatable_fast(L);
        if (!lua_rawequal(L, -1, -2))  /* not the same? */
            p = NULL;  /* value is a userdata with wrong metatable */
        lua_pop(L, 2);  /* remove both metatables */
        return (lb_Buffer*)p;
    }
    return NULL;  /* value is not a userdata with a metatable */
}

LB_API lb_Buffer *lb_checkbuffer(lua_State *L, int narg) {
    lb_Buffer *b = lb_testbuffer(L, narg);
    if (b == NULL)
        type_error(L, narg, LB_LIBNAME);
    return b;
}

LB_API lb_Buffer *lb_pushbuffer(lua_State *L, const char *str, size_t len) {
    lb_Buffer *B = lb_newbuffer(L);
    lb_addlstring(B, str, len);
    return B;
}


/* compatible with lua api */

LB_API int lb_isbufferorstring(lua_State *L, int narg) {
    return lua_isstring(L, narg) || lb_testbuffer(L, narg) != NULL;
}

LB_API const char *lb_tolstring(lua_State *L, int narg, size_t *plen) {
    lb_Buffer *B;
    const char *str = lua_tolstring(L, narg, plen);
    if (str == NULL && (B = lb_testbuffer(L, narg)) != NULL) {
        if (plen != NULL) *plen = B->n;
        str = B->b;
    }
    return str;
}

LB_API const char *lb_checklstring(lua_State *L, int narg, size_t *plen) {
    const char *s = lb_tolstring(L, narg, plen);
    if (s == NULL)
        type_error(L, narg, "buffer/string");
    return s;
}

LB_API const char *lb_optlstring(lua_State *L, int narg, const char *def, size_t *plen) {
    if (lua_isnoneornil(L, narg)) {
        if (plen != NULL) *plen = def ? strlen(def) : 0;
        return def;
    }
    return lb_checklstring(L, narg, plen);
}


/* bit pack/unpack operations */

#ifndef _MSC_VER
#  include <stdint.h>
#else
#  define uint32_t unsigned long
#  define uint64_t unsigned __int64
#  define int32_t signed long
#  define int64_t signed __int64
#endif

typedef union numcast_t {
    uint32_t i32;
    float f;
    uint64_t i64;
    double d;
} numcast_t;

static uint32_t read_int32(const char *s, int bigendian, int wide) {
    uint32_t n = 0;
    if (bigendian) {
        switch (wide) {
        default: return 0;
        case 4:          n |= *s++ & 0xFF;
        case 3: n <<= 8; n |= *s++ & 0xFF;
        case 2: n <<= 8; n |= *s++ & 0xFF;
        case 1: n <<= 8; n |= *s++ & 0xFF;
        }
    }
    else {
        switch (wide) {
        default: return 0;
        case 4: n |= (*s++ & 0xFF) << 24;
        case 3: n |= (*s++ & 0xFF) << 16;
        case 2: n |= (*s++ & 0xFF) <<  8;
        case 1: n |= (*s++ & 0xFF);
        }
    }
    return n;
}

static void write_int32(char *s, int bigendian, uint32_t n, int wide) {
    if (bigendian) {
        switch (wide) {
            default: return;
            case 4: *s++ = (n >> 24) & 0xFF;
            case 3: *s++ = (n >> 16) & 0xFF;
            case 2: *s++ = (n >>  8) & 0xFF;
            case 1: *s++ = (n      ) & 0xFF;
        }
    }
    else {
        switch (wide) {
            default: return;
            case 4: *s++ = n & 0xFF; n >>= 8;
            case 3: *s++ = n & 0xFF; n >>= 8;
            case 2: *s++ = n & 0xFF; n >>= 8;
            case 1: *s++ = n & 0xFF;
        }
    }
}

static void read_binary(const char *str, int bigendian, numcast_t *buf, size_t wide) {
    if (wide <= 4)
        buf->i32 = read_int32(str, bigendian, wide);
    else if (bigendian) {
        buf->i64 = read_int32(str, bigendian, 4); buf->i64 <<= ((wide-4)<<3);
        buf->i64 |= read_int32(&str[4], bigendian, wide - 4);
    }
    else {
        buf->i64 = read_int32(str, bigendian, 4);
        buf->i64 |= (uint64_t)read_int32(str, bigendian, 4) << 32;
    }
}

static void write_binary(char *str, int bigendian, numcast_t *buf, size_t wide) {
    if (wide <= 4)
        write_int32(str, bigendian, buf->i32, wide);
    else if (bigendian) {
        write_int32(str, bigendian, (uint32_t)(buf->i64 >> 32), wide - 4);
        write_int32(&str[wide - 4], bigendian, (uint32_t)buf->i64, 4);
    }
    else {
        write_int32(str, bigendian, (uint32_t)buf->i64, 4);
        write_int32(&str[4], bigendian, (uint32_t)(buf->i64 >> 32), wide - 4);
    }
}

static void expand_sign(numcast_t *buf, size_t wide) {
    int shift = wide<<3;
    if (wide <= 4) {
        if (wide != 4 && ((uint32_t)1 << (shift - 1) & buf->i32) != 0)
            buf->i32 |= ~(uint32_t)0 << shift;
    }
    else {
        if (wide != 8 && ((uint64_t)1 << (shift - 1) & buf->i64) != 0)
            buf->i64 |= ~(uint64_t)0 << shift;
    }
}

LB_API int lb_packint(lb_Buffer *B, size_t wide, int bigendian, lua_Integer i) {
    numcast_t buff;
    /* we use int64_t with i64, because if we use uint64_t, the
     * high 32 bit of 64bit integer will be stripped, don't know
     * why it happened.  */
    if (wide <= 4)
        buff.i32 = (/*u*/int32_t)i;
    else
        buff.i64 = (/*u*/int64_t)i;
    write_binary(lb_prepbuffsize(B, wide),
            bigendian, &buff, wide);
    lb_addsize(B, wide);
    return wide;
}

LB_API int lb_packfloat(lb_Buffer *B, size_t wide, int bigendian, lua_Number n) {
    numcast_t buff;
    if (wide <= 4)
        buff.f = (float)n;
    else
        buff.d = (double)n;
    write_binary(lb_prepbuffsize(B, wide),
            bigendian, &buff, wide);
    lb_addsize(B, wide);
    return wide;
}

LB_API int lb_unpackint(const char *s, size_t wide, int bigendian, lua_Integer *pi) {
    numcast_t buff;
    read_binary(s, bigendian, &buff, wide);
    expand_sign(&buff, wide);
    if (wide <= 4)
        *pi = (lua_Integer)buff.i32;
    else
        *pi = (lua_Integer)buff.i64;
    return wide;
}

LB_API int lb_unpackuint(const char *s, size_t wide, int bigendian, lua_Integer *pi) {
    numcast_t buff;
    read_binary(s, bigendian, &buff, wide);
    if (wide <= 4)
        *pi = (lua_Integer)buff.i32;
    else
        *pi = (lua_Integer)buff.i64;
    return wide;
}

LB_API int lb_unpackfloat(const char *s, size_t wide, int bigendian, lua_Number *pn) {
    numcast_t buff;
    read_binary(s, bigendian, &buff, wide);
    if (wide <= 4)
        *pn = (lua_Number)buff.f;
    else
        *pn = (lua_Number)buff.d;
    return wide;
}

/*
 * cc: lua='lua52' flags+='-s -O2 -Wall -pedantic -mdll -Id:/$lua/include' libs+='d:/$lua/$lua.dll'
 * cc: flags+='-DLB_REDIR_STRLIB=1 -DLB_FILEHANDLE'
 * cc: flags+='-DLUA_BUILD_AS_DLL' input='lb*.c' output='buffer.dll'
 * cc: run='$lua test.lua'
 */
