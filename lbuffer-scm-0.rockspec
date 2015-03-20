-- vim: ft=lua
package = "lbuffer"
version = "scm-0"

source = {
   url = "git://github.com/starwing/lbuffer.git",
}

description = {
   summary = "mutable string support to lua",
   detailed = [[
lbuffer is a C module for lua, it provides mutable string feature to
the lua_ language. it has all routines from lua's string module, add
several modify functions. it provides:

    * change the value of buffer without copy it.
    * a pair of full feature pack/unpack functions.
    * get a subbuffer from the original buffer, and the changes to
      subbufer will affects the original buffer.

]],
   homepage = "https://github.com/starwing/lbuffer",
   license = "MIT/X11",
}

dependencies = {
   "lua >= 5.1",
}

build = {
   copy_directories = {},

   type = "builtin",

   modules = {
      buffer = {
         "lbuffer.c",
         "lbufflib.c",
      }
   }
}
