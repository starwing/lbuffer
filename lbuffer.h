#ifndef LBUFFER_H
#define LBUFFER_H


#define LB_LIBNAME "buffer"
#define LB_VERSION "0.2"


#include <lua.h>
#include <lauxlib.h>


#ifndef LB_API
# define LB_API LUA_API
#endif

/* compatible apis */
#if LUA_VERSION_NUM < 502
#  define LUA_OK                        0
#  define lua_getuservalue              lua_getfenv
#  define lua_setuservalue              lua_setfenv
#  define lua_rawlen                    lua_objlen
#  define luaL_setfuncs(L,l,nups)       luaI_openlib((L),NULL,(l),(nups))
#  define luaL_newlibtable(L,l)	\
    lua_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)
#  define luaL_newlib(L,l) \
    (luaL_newlibtable(L,l), luaL_setfuncs(L,l,0))

LUA_API void lua_rawgetp (lua_State *L, int idx, const void *p);
LUA_API void lua_rawsetp (lua_State *L, int idx, const void *p);
LUA_API int  lua_absindex (lua_State *L, int idx);
#endif /* LUA_VERSION_NUM < 502 */


/* luaL_Buffer compatible interface */

typedef struct lb_Buffer {
    char *b;
    size_t size;
    size_t n;
    lua_State *L;
    char initb[LUAL_BUFFERSIZE];
} lb_Buffer;

#define lb_addchar(B,c) \
  ((void)((B)->n < (B)->size || lb_prepbuffsize((B), 1)), \
   ((B)->b[(B)->n++] = (c)))

#define lb_buffinitsize(L,B,sz) (lb_buffinit((L),(B)),lb_prepbuffsize((B),(sz)))
#define lb_addsize(B,s)	 ((B)->n += (s))
#define lb_prepbuffer(B)  lb_prepbuffsize((B), LUAL_BUFFERSIZE)
#define lb_pushresultsize(B,sz) (lb_addsize(B,sz),lb_pushresult(b))


LB_API void  lb_buffinit     (lua_State *L, lb_Buffer *B);
LB_API char *lb_prepbuffsize (lb_Buffer *B, size_t sz);
LB_API void  lb_addlstring   (lb_Buffer *B, const char *s, size_t l);
LB_API void  lb_addstring    (lb_Buffer *B, const char *s);
LB_API void  lb_addvalue     (lb_Buffer *B);
LB_API void  lb_pushresult   (lb_Buffer *B);


/* buffer type routines */

#define LB_METAKEY 0xF7B2FFE7

LUALIB_API int luaopen_buffer (lua_State *L);

LB_API lb_Buffer *lb_newbuffer  (lua_State *L);
LB_API lb_Buffer *lb_copybuffer (lb_Buffer *B);
LB_API void lb_resetbuffer(lb_Buffer *B);

LB_API int lb_pack   (lb_Buffer *B, const char *fmt, int args);
LB_API int lb_unpack (const char *s, size_t n, const char *fmt);
LB_API int lb_packint   (lb_Buffer *B, size_t wide, lua_Integer i);
LB_API int lb_unpackint (const char *s, size_t n, size_t wide, lua_Integer *pi);

LB_API lb_Buffer *lb_testbuffer  (lua_State *L, int idx);
LB_API lb_Buffer *lb_checkbuffer (lua_State *L, int idx);
LB_API lb_Buffer *lb_pushbuffer  (lua_State *L, const char *str, size_t len);

LB_API int          lb_isbufferorstring (lua_State *L, int idx);
LB_API const char  *lb_tolstring        (lua_State *L, int idx, size_t *plen);
LB_API const char  *lb_checklstring     (lua_State *L, int idx, size_t *plen);
LB_API const char  *lb_optlstring       (lua_State *L, int idx, const char *def, size_t *plen);

#ifdef LB_REPLACE_LUA_API
#  define lua_isstring      lb_isbufferorstring
#  define lua_tolstring     lb_tolstring
#  define luaL_checklstring lb_checklstring
#  define luaL_optlstring   lb_optlstring
#endif /* LB_REPLACE_LUA_API */


#endif /* LBUFFER_H */
