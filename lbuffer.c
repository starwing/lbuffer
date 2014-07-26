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

lb_Buffer *lb_newbuffer(lua_State *L) {
    lb_Buffer *B = (lb_Buffer*)lua_newuserdata(L, sizeof(lb_Buffer));
    lb_buffinit(L, B);
    get_metatable_fast(L);
    lua_setmetatable(L, -2);
    return B;
}

lb_Buffer *lb_copybuffer(lb_Buffer *B) {
    lb_Buffer *nb = lb_newbuffer(B->L);
    lb_addlstring(nb, B->b, B->n);
    return nb;
}

void lb_resetbuffer(lb_Buffer *B) {
    lua_State *L = B->L;
    if (B->b != B->initb) { /* remove old buffer */
        lua_pushnil(L);
        lua_rawsetp(L, LUA_REGISTRYINDEX, B);
    }
    lb_buffinit(L, B);
}

lb_Buffer *lb_testbuffer(lua_State *L, int narg) {
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

lb_Buffer *lb_checkbuffer(lua_State *L, int narg) {
    lb_Buffer *b = lb_testbuffer(L, narg);
    if (b == NULL)
        type_error(L, narg, LB_LIBNAME);
    return b;
}

lb_Buffer *lb_pushbuffer(lua_State *L, const char *str, size_t len) {
    lb_Buffer *B = lb_newbuffer(L);
    lb_addlstring(B, str, len);
    return B;
}


/* compatible with lua api */

int lb_isbufferorstring(lua_State *L, int narg) {
    return lua_isstring(L, narg) || lb_testbuffer(L, narg) != NULL;
}

const char *lb_tolstring(lua_State *L, int narg, size_t *plen) {
    lb_Buffer *B;
    const char *str = lua_tolstring(L, narg, plen);
    if (str == NULL && (B = lb_testbuffer(L, narg)) != NULL) {
        if (plen != NULL) *plen = B->n;
        str = B->b;
    }
    return str;
}

const char *lb_checklstring(lua_State *L, int narg, size_t *plen) {
    const char *s = lb_tolstring(L, narg, plen);
    if (s == NULL)
        type_error(L, narg, "buffer/string");
    return s;
}

const char *lb_optlstring(lua_State *L, int narg, const char *def, size_t *plen) {
    if (lua_isnoneornil(L, narg)) {
        if (plen != NULL) *plen = def ? strlen(def) : 0;
        return def;
    }
    return lb_checklstring(L, narg, plen);
}

/*
 * cc: lua='lua52' flags+='-s -O2 -Wall -pedantic -mdll -Id:/$lua/include' libs+='d:/$lua/$lua.dll'
 * cc: flags+='-DLB_REDIR_STRLIB=1 -DLB_FILEHANDLE'
 * cc: flags+='-DLUA_BUILD_AS_DLL' input='lb*.c' output='buffer.dll'
 * cc: run='$lua test.lua'
 */
