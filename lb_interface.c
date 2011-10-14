#include "lbuffer.h"


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


/* buffer interface */

#ifdef LB_SUBBUFFER
buffer *lb_checkbuffer(lua_State *L, int narg) {
    buffer *b = (buffer*)luaL_checkudata(L, narg, BUFFER_MTNAME);
    if (b != NULL && lb_isinvalidsub(b))
        luaL_error(L, "invalid subbuffer (%p)", b);
    return b;
}

subbuffer *lb_initsubbuffer(subbuffer *b) {
    b->str = NULL;
    b->len = 0;
    b->subtype = LB_INVALID_SUB;
    b->parent = NULL;
    return b;
}

buffer *lb_newsubbuffer (lua_State *L, buffer *b, size_t begin, size_t end) {
    subbuffer *sb = (subbuffer*)lua_newuserdata(L, sizeof(subbuffer)); /* 1 */
    luaL_getmetatable(L, BUFFER_MTNAME); /* 2 */
    lua_setmetatable(L, -2); /* 2->1 */
    begin = begin > b->len ? b->len : begin;
    end = end > b->len ? b->len : end;

    if (b->subcount == LB_SUBS_MAX)
        lb_removesubbuffer(b->subs[0]);
    b->subs[b->subcount++] = sb;

    sb->str = &b->str[begin];
    sb->len = begin < end ? end - begin : 0;
    sb->parent = b;
    sb->subtype = LB_SUB;

    return (buffer*)sb;
}

void lb_removesubbuffer (subbuffer *b) {
    buffer *pb = b->parent;

    if (pb != NULL && lb_issubbuffer(b)) {
        size_t i, j;
        lb_initsubbuffer(b);
        for (i = j = 0; i < pb->subcount; ++i) {
            subbuffer *sb = pb->subs[i];
            if (sb != NULL && !lb_isinvalidsub(sb))
                pb->subs[j++] = sb;
        }
        pb->subcount = j;
    }
}

static char *realloc_subbuffer(lua_State *L, subbuffer *sb, size_t len) {
    if (!lb_isinvalidsub(sb)) {
        buffer *pb = sb->parent;
        size_t begin = sb->str - pb->str;
        size_t pb_oldlen = pb->len;
        int dlen = len - sb->len;

        if (dlen == 0)
            return sb->str;

        else if (dlen > 0) {
            if (lb_realloc(L, pb, pb->len + dlen)) {
#define MAINTAIN_SUBBUFFER() do { \
    size_t sb_oldend = begin + sb->len; \
    size_t sb_newend = begin + len; \
    sb->len = len; \
    memmove(&pb->str[sb_newend], &pb->str[sb_oldend], \
            pb_oldlen - sb_oldend); } while (0)

                MAINTAIN_SUBBUFFER();
                sb->str = &pb->str[begin];
                return sb->str;
            }
        }
        else { /* assume shorten length never fail */
            MAINTAIN_SUBBUFFER();
            if (lb_realloc(L, pb, pb->len + dlen)) {
                sb->str = &pb->str[begin];
                return sb->str;
            }
        }
    }
    return NULL;
}

static void redir_subbuffers(buffer *b, char *newstr, size_t len) {
    size_t i, j;

    if (len == 0) {
        for (i = 0; i < b->subcount; ++i) {
            if (b->subs[i] != NULL) {
                lb_initsubbuffer(b->subs[i]);
                b->subs[i] = NULL;
            }
        }
        b->subcount = 0;
    }
    else if (len >= b->len && newstr != b->str) {
        for (i = 0; i < b->subcount; ++i) {
            subbuffer *sb = b->subs[i];
            if (sb != NULL) {
                size_t begin = sb->str - b->str;
                sb->str = &newstr[begin];
            }
        }
    }
    else if (len < b->len) {
        for (i = j = 0; i < b->subcount; ++i) {
            subbuffer *sb = b->subs[i];
            if (sb != NULL) {
                size_t begin = sb->str - b->str;
                size_t end = begin + sb->len;
                if (begin > len)
                    lb_initsubbuffer(sb);
                else {
                    if (end > len && sb->len != 0)
                        sb->len = len - begin;
                    sb->str = &newstr[begin];
                    b->subs[j++] = b->subs[i];
                }
            }
        }
        b->subcount = j;
    }
}
#endif

char *lb_realloc(lua_State *L, buffer *b, size_t len) {
#ifdef LB_SUBBUFFER
    if (b->subcount < 0)
        return realloc_subbuffer(L, (subbuffer*)b, len);
#endif

    if (len != b->len) {
        char *newstr = NULL;
        void *ud;
        lua_Alloc f;

        f = lua_getallocf(L, &ud);
        newstr = (char*)f(ud, b->str, b->len, len);
        if (len == 0 || newstr != NULL) {
#ifdef LB_SUBBUFFER
            redir_subbuffers(b, newstr, len);
#endif
            b->str = newstr;
            b->len = len;
        }
        return newstr;
    }
    return b->str;
}

int lb_isbuffer(lua_State *L, int narg) {
    if (lua_getmetatable(L, narg)) {
        lua_getfield(L, LUA_REGISTRYINDEX, BUFFER_MTNAME);
        if (!lua_isnil(L, -1) && lua_rawequal(L, -1, -2)) {
            lua_pop(L, 2);
            return 1;
        }
        lua_pop(L, 2);
    }
    return 0;
}

buffer *lb_tobuffer(lua_State *L, int narg) {
    if (lb_isbuffer(L, narg)) {
        buffer *b = (buffer*)lua_touserdata(L, narg);
#ifdef LB_SUBBUFFER
        if (lb_isinvalidsub(b))
            luaL_error(L, "invalid subbuffer (%p)", b);
#endif
        return b;
    }
    return NULL;
}

buffer *lb_initbuffer(buffer *b) {
    b->str = NULL;
    b->len = 0;
#ifdef LB_SUBBUFFER
    {
        size_t i;
        b->subcount = 0;
        for (i = 0; i < LB_SUBS_MAX; ++i)
            b->subs[i] = NULL;
    }
#endif
    return b;
}

buffer *lb_newbuffer(lua_State *L) {
    buffer *b = lb_initbuffer((buffer*)lua_newuserdata(L, sizeof(buffer))); /* 1 */
    luaL_getmetatable(L, BUFFER_MTNAME); /* 2 */
    lua_setmetatable(L, -2); /* 2->1 */
    return b;
}

buffer *lb_copybuffer(lua_State *L, buffer *b) {
    buffer *nb = lb_newbuffer(L);
    if (lb_realloc(L, nb, b->len))
        memcpy(nb->str, b->str, b->len);
    return nb;
}

buffer *lb_pushbuffer(lua_State *L, const char *str, size_t len) {
    buffer *b = lb_newbuffer(L);
    if (lb_realloc(L, b, len))
        memcpy(b->str, str, len);
    return b;
}

/* compatible with lua api */

int lb_isbufferorstring(lua_State *L, int narg) {
    return lua_type(L, narg) == LUA_TSTRING || lb_isbuffer(L, narg);
}

const char *lb_pushlstring(lua_State *L, const char *str, size_t len) {
    return lb_pushbuffer(L, str, len)->str;
}

const char *lb_checklstring(lua_State *L, int narg, size_t *plen) {
    if (lua_type(L, narg) == LUA_TSTRING)
        return lua_tolstring(L, narg, plen);
    else {
        buffer *b = lb_tobuffer(L, narg);
        if (b != NULL)
            return lb_tolstring(L, narg, plen);
        luaL_typerror(L, narg, "buffer or string");
        return NULL; /* avoid warning */
    }
}

const char *lb_optlstring(lua_State *L, int narg, const char *def, size_t *plen) {
    buffer *b = lb_tobuffer(L, narg);
    if (b != NULL)
        return lb_tolstring(L, narg, plen);
    if (lua_isstring(L, narg))
        return lua_tolstring(L, narg, plen);
    if (plen != NULL) *plen = def ? strlen(def) : 0;
    return def; /* avoid warning */
}

const char *lb_tolstring(lua_State *L, int narg, size_t *plen) {
    const char *str = lua_tolstring(L, narg, plen);
    if (str != NULL) return str;
    else {
        buffer *b = lb_tobuffer(L, narg);
        if (plen != NULL)
            *plen = (b == NULL ? 0 : b->len);
        if (b == NULL) return NULL;
        /* never return NULL with a valid buffer,
         * even if b->str is NULL. */
        return b->str != NULL ? b->str : "";
    }
}

/*
 * cc: flags+='-O4 -Wall -pedantic -mdll -Id:/lua/include' libs+='d:/lua/lua51.dll'
 * cc: flags+='-DLUA_BUILD_AS_DLL -DLB_SUBBUFFER' input='*.c' output='buffer.dll'
 * cc: run='lua test.lua'
 */
