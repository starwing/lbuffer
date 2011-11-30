lbuffer - a mutable string support to lua
=========================================

lbuffer is a C module for lua, it provides mutable string feature to
the lua_ language. it has all routines from lua's string module,
except pattern match and byte-code dump. and add several modify
functions. it provides:

    * change the value of buffer without copy it.
    * a pair of full feature pack/unpack functions.
    * get a subbuffer from the original buffer, and the changes to
      subbufer will affects the original buffer.

and you can add lbuffer.h and recompile the other C module to get full
lbuffer compatible with other lua module. just add: ::

    -DLB_REPLACE_LUA_API -include lbuffer.h

when you compile other Lua C modules (using gcc). e.g. if you compile
a lbuffer compatible md5_ module, you can use them together like
this: ::

    require 'buffer'
    require 'md5'
    print(md5.sumhexa(buffer "hello world"))


.. _lua: http://www.lua.org
.. _md5: https://github.com/keplerproject/md5


install
=======

compile it just like other lua module. in ``*nix``, just: ::

    gcc -shared -obuffer.so *.c -llua51

and in Windows using MinGW, just: ::

    gcc -mdll -DLUA_BUILD_AS_DLL -I/path/to/lua/include *.c /path/to/lua51.dll -o buffer.dll

and in Windows using MSVC, just create a Win32/DLL Project, and add
all .c and .h files to project, set output name to buffer.dll and
compile it.

if you want subbuffer feature, you need define a macro named
LB_SUBBUFFER, because subbuffer will slow the memory realloc function
in *all* buffers.

there are two method to do pack/unpack operations. defaultly we read
soem bits to a buffer, and cast it to int, and do bit swap. but you
can also choose bit-op ways to extract binary numbers in file, this
can be used in machines that has any number of bit in a byte, but this
may somehow slow a bit. define LB_ARTHBIT to enable this.

example
=======

there are some examples, and main usage please see test.lua.

first, you can use buffer just like using a normal string, you can
call string functions on it, but the functions are in buffer module of
course: ::

        local B = require 'buffer'
        local b = B'hello'
        print(#b)
        print(b:upper())
        print(b:reverse())

this will output: ::

    5
    HELLO
    OLLEH

notice that all these functions don't create a new buffer/string,
instead, it changes the content of operated buffer. so the output of
the last line is ``OLLEH``, but not ``olleh``.

beside the normal functions inherit from the standard string_ module,
there are also many functions that only in buffer module, see
reference_ of buffer to know these special functions, there are some
examples: ::
    
    $ lua -lbuffer
    Lua 5.1.4  Copyright (C) 1994-2008 Lua.org, PUC-Rio
    > b = buffer "abc"
    > =b:set(2, "pple")
    apple
    > =b:append "pie"
    applepie
    > =b:insert(-3, '-')
    apple-pie
    > =b:insert(1, '('):append ')'
    (apple-pie)
    > =b:assign "abc"
    abc
    > =b:tohex " "
    61 62 63
    > ='{ 0x'..b:tohex ", 0x"..' }'
    { 0x61, 0x62, 0x63 }
    > =b:alloc(5, "a")
    aaaaa   5
    > =b:len()
    5
    > =b:len(3)
    3
    > =b
    aaa
    > =b:free(), #b
    nil     0
    > =b:assign "abc" :eq "def"
    false
    > =b :cmp "def"
    -1
    > =b:move(3, 2)
    abbc
    > =b:clear()

    > =b:quote()
    "\000\000\000\000"
    >

.. _string: http://www.lua.org/manual/5.1/manual.html#5.4


and, beside all, buffer module has a pair of full featured pack/unpack
functions. it can be used extract struct from binary text to lua: ::

    -- read *.mo file
    function read_mofile(b)
        local info = b:unpack [[ {
            magic = i,
            revision = i,
            nstrings = i,
            orig_tab_offset = i,
            trans_tab_offset = i,
            hash_tab_size = i,
            hash_tab_offset = i,
        } ]]

        local trans = {}
        for i = 0, info.nstrings-1 do
            local o_len, o_offset = b:unpack(info.orig_tab_offset+8*i+1, "<ii")
            local t_len, t_offset = b:unpack(info.trans_tab_offset+8*i+1, "<ii")
            local os = b:unpack(o_offset+1, "s")
            local ts = b:unpack(t_offset+1, "s")
            trans[os] = ts
        end
        return info, trans
    end

for details, see reference of buffer module below.


reference
=========

.. _reference:
