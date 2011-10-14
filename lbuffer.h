#ifndef LBUFFER_H
#define LBUFFER_H


#include <lua.h>
#include <lauxlib.h>


#define BUFFER_VERSION "0.1"
#define BUFFER_MTNAME "buffer"
#define BUFFER_HEADER size_t len; char *str


#ifdef LB_SUBBUFFER
#define LB_SUBS_MAX             4
#define LB_SUB                  -1
#define LB_INVALID_SUB          -2

typedef struct subbuffer {
    BUFFER_HEADER;
    int subtype;
    struct buffer *parent;
} subbuffer;
#endif

typedef struct buffer {
    BUFFER_HEADER;
#ifdef LB_SUBBUFFER
    int subcount;
    struct subbuffer *subs[LB_SUBS_MAX];
#endif
} buffer;


#define lb_checkbuffer(L, narg) ((buffer*)luaL_checkudata(L, narg, BUFFER_MTNAME))
#define lb_deletebuffer(L, b)   lb_realloc((L), (b), 0)
#ifdef LB_SUBBUFFER
#  define lb_issubbuffer(b)     (((subbuffer*)(b))->subtype == LB_SUB)
#  define lb_isinvalidsub(b)    (((subbuffer*)(b))->subtype == LB_INVALID_SUB)
#endif

buffer     *lb_initbuffer   (buffer *b);
buffer     *lb_newbuffer    (lua_State *L);
buffer     *lb_copybuffer   (lua_State *L, buffer *b);
char       *lb_realloc      (lua_State *L, buffer *b, size_t len);
int         lb_isbuffer     (lua_State *L, int narg);
buffer     *lb_tobuffer     (lua_State *L, int narg);

int         lb_isbufferorstring(lua_State *L, int narg);
buffer     *lb_pushbuffer   (lua_State *L, const char *str, size_t len);
const char *lb_pushlstring  (lua_State *L, const char *str, size_t len);
const char *lb_tolstring    (lua_State *L, int narg, size_t *plen);
const char *lb_checklstring (lua_State *L, int narg, size_t *plen);
const char *lb_optlstring   (lua_State *L, int narg, const char *def, size_t *plen);

LUALIB_API int luaopen_buffer(lua_State *L);

#ifdef LB_SUBBUFFER
#undef      lb_checkbuffer
buffer     *lb_checkbuffer  (lua_State *L, int narg);
buffer     *lb_newsubbuffer (lua_State *L, buffer *b, size_t begin, size_t end);
subbuffer  *lb_initsubbuffer(subbuffer *b);
void        lb_removesubbuffer (subbuffer *b);
#endif

#ifdef LB_OVERLOAD_LUA_API
#  define lua_isstring      lb_isbufferorstring
#  define lua_tolstring     lb_tolstring
#  define luaL_checklstring lb_checklstring
#  define luaL_optlstring   lb_optlstring
#endif

#endif /* LBUFFER_H */
