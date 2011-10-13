require 'buffer'

-- borrowed from brimworks's lua-zlib
local counter = 1
local failed = false
function ok(assert_true, desc)
   local msg = ( assert_true and "ok " or "not ok " ) .. counter
   if ( not assert_true ) then
      failed = true
   end
   if ( desc ) then
      msg = msg .. " - " .. desc
   end
   print(msg)
   counter = counter + 1
end

function test_msg(msg)
    print(('-'):rep(20))
    print(msg)
    print(('-'):rep(20))
end

function test()
    print('buffer '..buffer._VERSION..' test')
    if jit then
        print('using', jit.version)
    end
    test_new()
    test_map()
    test_rep()
    test_alloc()
    test_sub()
    test_modify()
    test_byte()
    test_char()
    test_clear()
    test_copy()
    test_move()
    test_remove()
    test_pack()
    test_cmp()
    test_mt()
    if not failed then
        test_msg "** ALL TEST PASSED!!"
    else
        test_msg "** TEST FAILED!!"
    end
end

function test_new()
    test_msg "test initialize buffer"
    local b = buffer "one"
    ok(b :eq "one", "using string ("..b..')')
    local b = buffer(buffer "one")
    ok(b :eq "one", "using buffer ("..b..')')
    local pb = buffer "one" -- only get pointers from non-collected object!!
    local b = buffer(pb:topointer(2), 2)
    ok(b :eq "ne", "using userdata ("..b..")")
    local b = buffer("one", 0)
    ok(b :eq "one", "using string range 0 ("..b..")")
    local b = buffer("one", -2)
    ok(b :eq "ne", "using string range -2 ("..b..")")
    local b = buffer("one", 1, 0)
    ok(b :eq "o", "using string range 1, 0 ("..b..")")
    local b = buffer("one", 2, -2)
    ok(b :eq "n", "using string range 2, -2 ("..b..")")
    local b = buffer(buffer "one", 2, -2)
    ok(b :eq "n", "using buffer range 2, -2 ("..b..")")
    local b = buffer(10)
    ok(b :eq(("\0"):rep(10)), "using number ("..b:tohex' '..")")
    local b = buffer(10, "one")
    ok(b :eq "oneoneoneo", "using number and string ("..b..")")
    local b = buffer(10, buffer "one")
    ok(b :eq "oneoneoneo", "using number and buffer ("..b..")")
    local b = buffer(10, "one", 2, -2)
    ok(b :eq "nnnnnnnnnn", "using number and string range 2, -2 ("..b..")")
    local b = buffer(10, buffer "one", 2, -2)
    ok(b :eq "nnnnnnnnnn", "using number and buffer range 2, -2 ("..b..")")
    local b = buffer(10, buffer "one":topointer(2), 2)
    ok(b :eq "nenenenene", "using number and userdata ("..b..")")
end

function test_map()
    test_msg "test map operations"
    local b = buffer "HeLlO123" :upper()
    ok(b :eq "HELLO123", "toupper: "..b)
    local b = buffer "HeLlO123" :lower()
    ok(b :eq "hello123", "tolower: "..b)
end

function test_mt()
    test_msg "test metatable operations"
    local b = buffer "abc"
    ok(b[1] == 97 and b[2] == 98 and b[3] == 99 and not b[4],
        "index operation ("..b..")")
    ok(b[-3] == 97 and b[-2] == 98 and b[-1] == 99 and not b[0],
        "index operation - negitive index ("..b..")")
    b[2] = 'z'
    ok(b :eq 'azc', "newindex operation ("..b..")")
    b[4] = 112
    ok(b :eq 'azcp', "newindex operation - append number ("..b..")")
    b[5] = '-apple'
    ok(b :eq 'azcp-apple', "newindex operation - append string ("..b..")")
    ok(#b == 10, "length operation ("..#b..")")
    ok((b .. "(string)") :eq "azcp-apple(string)", "concat operation ("..b..")")
    local c = 0
    local bb = buffer()
    for i, v in b:ipairs() do
        bb[i] = v
        c = c + 1
    end
    ok(c == 10 and bb:eq(b), "ipairs operation ("..bb..")")
end

function test_rep()
    test_msg "test repeat operation"
    local b = buffer "abc"
    ok(b:rep(10) :eq (("abc"):rep(10)), "rep with number ("..b..")")
    ok(b:rep(0) :eq "", "rep with number 0 ("..b..")")
    ok(b:rep("abc", 10) :eq (("abc"):rep(10)), "rep with string and number ("..b..")")
    ok(b:rep("abc", 0) :eq "", "rep with string and number 0 ("..b..")")
    ok(b:rep(-2) :eq "", "rep with negitive number ("..b..")")
    ok(b:rep("abc", -2) :eq "", "rep with negitive number ("..b..")")
end

function test_alloc()
    test_msg "test alloc & free & len"
    local b = buffer(10)
    ok(#b == 10, "length of buffer ("..#b..")")
    ok(#b == b:len(), "length of buffer ("..#b..")")
    b:free()
    ok(#b == 0, "free buffer ("..#b..")")
    b:alloc(20)
    ok(#b == 20, "alloc buffer ("..#b..")")
    b:len(15)
    ok(#b == 15, "set length of buffer ("..#b..")")
    b:len(-14)
    ok(#b == 1, "set negitive length of buffer ("..#b..")")
    b:len(-10)
    ok(#b == 0, "set negitive length of buffer ("..#b..")")
end

function test_sub()
    if buffer.sub then
        test_msg "test subbuffer"
        print('sub count = '..buffer._SUBS_MAX)
        local b = buffer "apple-pie"
        do  local t = {}
            for i = 1, buffer._SUBS_MAX + 1 do
                t[i] = b:sub(i, i-1)
            end
            ok(tostring(t[1]):match "^%(invalid subbuffer%): %d+",
                "invalid subbuffer has special name ("..tostring(t[1])..")")
            ok(not t[1]:isbuffer(), "invalid subbuffer is not buffer ("..
                  (t[1]:isbuffer() and "true" or "false")..")")
        end
        collectgarbage()
        ok(b:subcount() == 0, "after collectgarbage the subbuffers cleared ("..b:subcount()..")")
        b:sub(1, 0):assign "(abc)"
        ok(b :eq "(abc)apple-pie", "subbuffer in front of original buffer ("..b..")")
        local b = buffer "apple-pie"
        local sb = b:sub(6, 6)
        ok(sb :eq '-', "subbuffer in the middle of buffer ("..sb..")")
        sb:assign "(xxx)"
        ok(b :eq "apple(xxx)pie", "subbuffer modified original buffer ("..b..")")
        b:len(5)
        ok(b :eq "apple", "original buffer set length to the begining of subbuffer ("..b..")")
        ok(sb:isbuffer(), "and subbuffer is also valid ("..sb..")")
        sb:append "-pie"
        ok(b :eq "apple-pie", "and subbuffer can append data to original buffer ("..b..")")
        sb = b:sub(6)
        ok(sb :eq "-pie", "subbuffer in the middle of buffer ("..sb..")")
        b:len(8)
        ok(sb :eq "-pi", "shorten original buffer, and subbuffer also shortted ("..sb..")")
    end
end

function test_modify()
    test_msg "test modified operations"
    local b = buffer "apple"
    b:append "-pie"
    ok(b :eq "apple-pie", "append operations ("..b..")")
    b:insert(7, "(xxx)-")
    ok(b :eq "apple-(xxx)-pie", "insert operations ("..b..")")
    b:set(7, "[===]")
    ok(b :eq "apple-[===]-pie", "set operations ("..b..")")
    b:assign "apple-pie"
    ok(b :eq "apple-pie", "assign operations ("..b..")")
end

function test_byte()
    test_msg "test byte operations"
    local b = buffer "apple-pie"
    ok(b:byte() == ('a'):byte(), 'byte of first byte of ('..b..'): '..b:byte())
    ok(b:byte(-1) == ('e'):byte(), 'byte of last byte of ('..b..'): '..b:byte(-1))
    ok(b:byte(-1) == ('e'):byte(), 'byte of last byte of ('..b..'): '..b:byte(-1))
    local t = {b:byte(1, -1)}
    ok(#t == #b, "bytes of 1, -1 length "..#t)
    local same = true
    for i = 1, #t do
        if t[i] ~= ("apple-pie"):byte(i) then
            same = false
        end
    end
    ok(same, "evey bytes from buffer is same as string")
end

function test_char()
    test_msg "test char operations"
    local b = buffer():char(1,2,3) 
    ok(#b == 3, "chars of three bytes is "..#b)
    local b = buffer "apple-pie":char(0x61,0x62,0x63)
    ok(b :eq "abc", "chars of three bytes is "..b)
end

function test_clear()
    test_msg "test clear operation"
    local b = buffer "apple-pie" :clear(1, 5)
    ok(b :eq "\0\0\0\0\0-pie", "clear prefix of buffer ("..b:quote()..")")
    local b = buffer "apple-pie" :clear(-3)
    ok(b :eq "apple-\0\0\0", "clear postfix of buffer ("..b:quote()..")")
    local b = buffer "apple-pie" :clear(-3, 20)
    ok(b :eq "apple-\0\0\0", "clear postfix of buffer ("..b:quote()..")")
end

function test_copy()
    test_msg "test copy operation"
    local b = buffer "apple-pie"
    ok(b:copy() :eq (b), "copy whole buffer ("..b:copy()..")")
    ok(b:copy(1,5) :eq "apple", "copy prefix of buffer ("..b:copy(1,5)..")")
    ok(b:copy(-3) :eq "pie", "copy postfix of buffer ("..b:copy(-3)..")")
    ok(b:copy(6,6) :eq "-", "copy middle of buffer ("..b:copy(6,6)..")")
    ok(b:copy(6,4) :eq "", "copy none of buffer ("..b:copy(6,4)..")")
end

function test_move()
    test_msg "test move operation"
    local b = buffer "apple-pie" :move(11, 7)
    ok(b :eq "apple-pie\0pie", "move postfix over end of buffer ("..b:quote()..')')
    local b = buffer "apple-pie" :move(1, 7)
    ok(b :eq "piele-pie", "move postfix to the head of buffer ("..b..")")
    local b = buffer "apple-pie" :move(-1, 7)
    ok(b :eq "apple-pipie", "move overlay of buffer ("..b..")")
    local b = buffer "apple-pie" :move(7, 1, 5)
    ok(b :eq "apple-apple", "move prefix of buffer ("..b..")")
    local b = buffer "apple-pie" :move(-3, 2, 4)
    ok(b :eq "apple-ppl", "move middle of buffer ("..b..")")
end

function test_remove()
    test_msg "test remove operation"
    local b = buffer "apple-pie"
    b:remove(6)
    ok(b :eq "apple", "remove end of buffer ("..b..')')
    b = buffer "apple-pie"
    b:remove(6, -4)
    ok(b :eq "applepie", "remove range of buffer ("..b..')')
    b:remove()
    ok(b :eq "", "remove all of buffer ("..b..')')
end

function test_cmp()
    test_msg "test cmp operation"
    ok(buffer("bb"):cmp(buffer("a")) == 1, "bb:cmp(a) -> 1")
    ok(buffer("b"):cmp(buffer("a")) == 1, "b:cmp(a) -> 1")
    ok(buffer("a"):cmp(buffer("bb")) == -1,  "a:cmp(bb) -> -1")
    ok(buffer("a"):cmp(buffer("b")) == -1,  "a:cmp(b) -> -1")
end

function test_pack()
end

test()
