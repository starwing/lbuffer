#ifndef LBUFFER_H
#define LBUFFER_H


#include <lua.h>
#include <lauxlib.h>

extern const char lb_libname[];

#define LB_VERSION "0.1"
#define LB_STRUCT_HEADER size_t len; char *str

#define LB_API LUA_API

#if !defined(LB_SUBBUFFER) || LB_SUBBUFFER
#  undef  LB_SUBBUFFER
#  define LB_SUBBUFFER
#else
#  undef LB_SUBBUFFER
#endif

#if !defined(LB_REDIR_STRLIB) || LB_REDIR_STRLIB
#  undef  LB_REDIR_STRLIB
#  define LB_REDIR_STRLIB
#else
#  undef LB_REDIR_STRLIB
#endif

#ifdef LB_SUBBUFFER
#define LB_SBPTR_BOX            "subbuffer-ptrbox"
#define LB_SUB                 -1
#define LB_INVALID_SUB         -2

#ifndef LB_SUBS_MAX
#  define LB_SUBS_MAX           4
#endif

typedef struct subbuffer {
    LB_STRUCT_HEADER;
    int subtype;
    struct buffer *parent;
    struct subbuffer *subparent;
} subbuffer;
#endif /* LB_SUBBUFFER */

typedef struct buffer {
    LB_STRUCT_HEADER;
#ifdef LB_SUBBUFFER
    int subcount;
    struct subbuffer *subs[LB_SUBS_MAX];
#endif /* LB_SUBBUFFER */
} buffer;


#define lb_deletebuffer(L, b)   lb_realloc((L), (b), 0)
#ifdef LB_SUBBUFFER
#  define lb_issubbuffer(b)     (((subbuffer*)(b))->subtype == LB_SUB)
#  define lb_isinvalidsub(b)    (((subbuffer*)(b))->subtype == LB_INVALID_SUB)
#else
#  define lb_isinvalidsub(b)    ((void)(b), 0)
#endif /* LB_SUBBUFFER */


LB_API buffer  *lb_initbuffer (buffer *b);
LB_API buffer  *lb_newbuffer  (lua_State *L);
LB_API buffer  *lb_copybuffer (lua_State *L, buffer *b);
LB_API char    *lb_realloc    (lua_State *L, buffer *b, size_t len);

LB_API buffer      *lb_rawtestbuffer    (lua_State *L, int narg);
LB_API buffer      *lb_testbuffer       (lua_State *L, int narg);
LB_API buffer      *lb_checkbuffer      (lua_State *L, int narg);
LB_API buffer      *lb_pushbuffer       (lua_State *L, const char *str, size_t len);
LB_API const char  *lb_setbuffer        (lua_State *L, buffer *b, const char *str, size_t len);

LUALIB_API int      luaopen_buffer(lua_State *L);

#ifdef LB_SUBBUFFER
LB_API buffer      *lb_newsubbuffer (lua_State *L, buffer *b, size_t begin, size_t end);
LB_API subbuffer   *lb_initsubbuffer(subbuffer *b);
LB_API void         lb_removesubbuffer (subbuffer *b);
#endif /* LB_SUBBUFFER */

LB_API int          lb_isbufferorstring (lua_State *L, int narg);
LB_API const char  *lb_tolstring        (lua_State *L, int narg, size_t *plen);
LB_API const char  *lb_checklstring     (lua_State *L, int narg, size_t *plen);
LB_API const char  *lb_optlstring       (lua_State *L, int narg, const char *def, size_t *plen);

#ifdef LB_REPLACE_LUA_API
#  define lua_isstring      lb_isbufferorstring
#  define lua_tolstring     lb_tolstring
#  define luaL_checklstring lb_checklstring
#  define luaL_optlstring   lb_optlstring
#endif /* LB_REPLACE_LUA_API */

#endif /* LBUFFER_H */
