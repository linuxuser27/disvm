implement Command;

include "sys.m";
include "draw.m";
include "Command.m";
include "external_module.m";

sys : Sys;

init(cxt: ref Draw->Context, args: list of string)
{
    a := "foobar!";
    sys = load Sys Sys->PATH;
    big_val := big 16r12345;
    sys->print("Hello, %s %o %d %x %bd %f\n%s%%%% %H\n\n", "world!", 8, 10, 16, big_val, 0.1, "------------------\n", a);

    while (args != nil)
    {
      arg := hd args;
      sys->print("-- %s --\n", arg);
      args = tl args;
    }

    test_function_refs();
    test_alt();
    test_channel();
    test_spawn();
    test_exceptions();
    test_case();
    test_discriminated_union();
    test_external_module();
    test_fixed_point();
    test_exponent();
    test_string();
    test_array();
    test_list();
    test_branch();
    test_for_loop_function_call();
}

str_cmp(s1: string, s2: string): int
{
    if (s1 < s2)
        return -1;
    else if (s1 > s2)
        return 1;

    return 0;
}

test_function_refs()
{
    sys->print("test_function_refs()\n");
    f : ref fn(s1: string, s2: string): int;
    f = str_cmp;

    s1 := "abcdefg";
    s2 := "ABCDEFG";

    if (f(s1, s2) != 1) exit;
}

test_alt()
{
    sys->print("test_alt()\n");
    int_chan_10 := chan of int;
    int_chan_20 := chan of int;
    int_chan_done := chan of int;

    spawn alt1(int_chan_10, int_chan_20, int_chan_done);

    d : int = 0;
    while (d == 0)
    {
        alt
        {
            r1 := <- int_chan_10 =>
                if (r1 != 10) exit;
            r2 := <- int_chan_20 =>
                if (r2 != 20) exit;
            d = <- int_chan_done =>
                ;
        }
    }

    spawn alt2(int_chan_10, int_chan_20, int_chan_done);

    unused : int = 0;
    int_chan_unused1 := chan of int;
    int_chan_unused2 := chan of int;

    d = 0;
    while(d == 0)
    {
        alt
        {
            unused =<- int_chan_unused1 =>
                exit;
            int_chan_unused2 <-= unused =>
                exit;
            * =>
                ;
        }

        alt
        {
            int_chan_10 <-= 10  =>
                ;
            int_chan_20 <-= 20 =>
                ;
            d = <- int_chan_done =>
                ;
        }

        alt
        {
            unused =<- int_chan_unused1 =>
                exit;
            int_chan_unused2 <-= unused =>
                exit;
            * =>
                ;
        }
    }
}

alt1(ten : chan of int, twenty : chan of int, done : chan of int)
{
    twenty <-= 20;
    ten <-= 10;
    ten <-= 10;
    twenty <-= 20;
    done <-= 1;
}

alt2(ten : chan of int, twenty : chan of int, done : chan of int)
{
    v : int;
    v =<- twenty;
    if (v != 20) exit;
    v =<- ten;
    if (v != 10) exit;
    v =<- twenty;
    if (v != 20) exit;
    v =<- twenty;
    if (v != 20) exit;
    v =<- ten;
    if (v != 10) exit;
    v =<- ten;
    if (v != 10) exit;
    done <-= 1;
}

test_channel()
{
    sys->print("test_channel()\n");
    test_channel_int(nil);
    test_channel_int_buffered(nil, nil);

    test_channel_adt(nil);
    test_channel_adt_buffered(nil, nil);

    test_channel_ref_adt(nil);
    test_channel_ref_adt_buffered(nil, nil);
}

test_channel_int(c : chan of int)
{
    if (c == nil)
    {
        c_l := chan of int;
        spawn test_channel_int(c_l);

        m := <- c_l;
        if (m != 10) exit;

        c_l <-= 20;
    }
    else
    {
        c <-= 10;
        m := <- c;

        if (m != 20) exit;
    }
}

test_channel_int_buffered(c : chan of int, r : chan of int)
{
    if (c == nil)
    {
        c_l := chan[3] of int;
        r_l := chan of int;
        spawn test_channel_int_buffered(c_l, r_l);

        m := <- c_l;
        if (m != 10) exit;
        m += <- c_l;
        if (m != 20) exit;
        m += <- c_l;
        if (m != 30) exit;
        m += <- c_l;
        if (m != 40) exit;
        m += <- c_l;
        if (m != 50) exit;

        r_l <-= m;
    }
    else
    {
        c <-= 10;
        c <-= 10;
        c <-= 10;
        c <-= 10;
        c <-= 10;
        m := <- r;

        if (m != 50) exit;
    }
}

ChannelAdt : adt
{
    s : string;
    d : real;
};

test_channel_adt(c : chan of ChannelAdt)
{
    if (c == nil)
    {
        c_l := chan of ChannelAdt;
        spawn test_channel_adt(c_l);

        m := <- c_l;
        if (m.s != "from") exit;
        if (m.d != 1.0) exit;

        c_l <-= ("to", 2.0);
    }
    else
    {
        c <-= ("from", 1.0);
        m := <- c;

        if (m.s != "to") exit;
        if (m.d != 2.0) exit;
    }
}

test_channel_adt_buffered(c : chan of ChannelAdt, r : chan of ChannelAdt)
{
    if (c == nil)
    {
        c_l := chan[1] of ChannelAdt;
        r_l := chan of ChannelAdt;
        spawn test_channel_adt_buffered(c_l, r_l);

        m := <- c_l;
        if (m.s != "from") exit;
        if (m.d != 1.0) exit;
        m = <- c_l;
        if (m.s != "from") exit;
        if (m.d != 1.0) exit;
        m = <- c_l;
        if (m.s != "from") exit;
        if (m.d != 1.0) exit;

        r_l <-= ("to", 2.0);
    }
    else
    {
        c <-= ("from", 1.0);
        c <-= ("from", 1.0);
        c <-= ("from", 1.0);
        m := <- r;

        if (m.s != "to") exit;
        if (m.d != 2.0) exit;
    }
}

test_channel_ref_adt(c : chan of ref ChannelAdt)
{
    if (c == nil)
    {
        c_l := chan of ref ChannelAdt;
        spawn test_channel_ref_adt(c_l);

        m := <- c_l;
        if (m.s != "from") exit;
        if (m.d != 1.0) exit;

        c_l <-= ref ("to", 2.0);
    }
    else
    {
        c <-= ref ("from", 1.0);
        m := <- c;

        if (m.s != "to") exit;
        if (m.d != 2.0) exit;
    }
}

test_channel_ref_adt_buffered(c : chan of ref ChannelAdt, r : chan of ref ChannelAdt)
{
    if (c == nil)
    {
        c_l := chan[1] of ref ChannelAdt;
        r_l := chan of ref ChannelAdt;
        spawn test_channel_ref_adt_buffered(c_l, r_l);

        m := <- c_l;
        if (m.s != "from") exit;
        if (m.d != 1.0) exit;
        m = <- c_l;
        if (m.s != "from") exit;
        if (m.d != 1.0) exit;
        m = <- c_l;
        if (m.s != "from") exit;
        if (m.d != 1.0) exit;

        r_l <-= ref ("to", 2.0);
    }
    else
    {
        c <-= ref ("from", 1.0);
        c <-= ref ("from", 1.0);
        c <-= ref ("from", 1.0);
        m := <- r;

        if (m.s != "to") exit;
        if (m.d != 2.0) exit;
    }
}

test_spawn()
{
    sys->print("test_spawn()\n");
    spawn th1();
}

th1()
{
    a := 9;
    b := 11;
    c := a + b;
    if (c != 20) exit;
}

TESTEXCEPTION : exception(int, int);

test_exceptions()
{
    sys->print("test_exceptions()\n");
    {
        raise TESTEXCEPTION(5,10);
    } exception e {
        TESTEXCEPTION =>
            (a,b) := e;
            if (a != 5 || b != 10) exit;

        * => exit;
    }

    {
        raise "bad time";
    } exception e {
        "bad time" => ;
        * => exit;
    }

    s := 0;
    {
        raise_exception_1(0);
    } exception e {
        TESTEXCEPTION => s = 1;
        * => ;
    }
    if (s != 1) exit;

    s = 0;
    {
        raise_exception_1(1);
    } exception e {
        "exception string" => s = 1;
        * => ;
    }
    if (s != 1) exit;

    s = 0;
    {
        raise_exception_n(7, "a");
    } exception e {
        TESTEXCEPTION =>
            (a,b) := e;
            if (a != 0) exit;
            s = 1;
        * => ;
    }
    if (s != 1) exit;
}

raise_exception_1(v : int)
{
    {
        raise_exception_2(v);
    } exception e {
        "do not catch" => exit;
    }

    exit;
}

raise_exception_2(v : int)
{
    if (v == 0)
        raise TESTEXCEPTION(5,10);
    else
        raise "exception string";
}

raise_exception_n(d : int, s:string)
{
    s_local := s + s;

    if (d == 0)
        raise TESTEXCEPTION(d,d);
    else
        raise_exception_n(d - 1, s_local);
}

test_case()
{
    sys->print("test_case()\n");
    {
        s := 0;
        v := 150;
        case (v)
        {
            0 to 100 => s = 0;
            101 to 200 => s = 1;
            201 to 300 => s = 0;
            * => s = 0;
        }

        if (s != 1) exit;
    }

    {
        s := 0;
        v := "foo";
        case (v)
        {
            "bar" => s = 0;
            "FOO" or "foo" => s = 1;
            * => s = 0;
        }

        if (s != 1) exit;
    }

    {
        s := 0;
        v := 16r300000000;
        case (v)
        {
            16r100000000 => s = 0;
            16r200000000 or 16r300000000 => s = 1;
            16r400000000 => s = 0;
            * => s = 0;
        }

        if (s != 1) exit;
    }
}

Constant: adt
{
    name: string;
    pick
    {
    Str =>
        s: string;
    Real =>
        r: real;
    }
};

test_discriminated_union()
{
    sys->print("test_discriminated_union()\n");
    c : ref Constant = ref Constant.Real("pi", 3.14);
    pick x := c
    {
        Str =>
            exit;
        Real =>
            ;
    };

    if (tagof(c) != tagof(Constant.Real)) exit;
}

test_external_module()
{
    sys->print("test_external_module()\n");
    ext_mod := load ExtModule ExtModule->PATH;

    {
        module_int := ext_mod->return_int();
        if (module_int != ext_mod->module_constant) exit;
    }

    {
        thirty_four := ext_mod->double_int(17);
        if (thirty_four != 34) exit;
    }

    {
        d := ext_mod->return_datatype();
        dr := ext_mod->return_datatype_ref();
    }

    {
        spawn ext_mod->spawn_main();
    }
}

test_fixed_point()
{
    sys->print("test_fixed_point()\n");
    fpt : type fixed(0.125, 512.0);
    a := fpt(1.1);
    b := fpt(2.1);
    c := a * b;
    #if (c != fpt(2.31)) exit;
}

test_exponent()
{
    sys->print("test_exponent()\n");
    {
        base := 2;
        power := 3;
        result := base ** power;
        if (result != 8) exit;
        base **= power;
        if (result != base) exit;
    }

    {
        base := big 2;
        power := 3;
        result := base ** power;
        if (result != big 8) exit;
        base **= power;
        if (result != base) exit;
    }

    {
        base := real 2.0;
        power := 3;
        result := base ** power;
        if (result != real 8.0) exit;
        base **= power;
        if (result != base) exit;
    }

    {
        base := 2;
        power := 0;
        result := base ** power;
        if (result != 1) exit;
    }

    # Integer exponent rules
    {
        base := 2;
        power := 0;
        result := base ** power;
        if (result != 1) exit;

        base = 2;
        power = -1;
        result = base ** power;
        if (result != 0) exit; # Non integer result (i.e. 0)
    }

    # Real exponent rules
    {
        base := 2.0;
        power := 0;
        result := base ** power;
        if (result != 1.0) exit;

        base = 2.0;
        power = -1;
        result = base ** power;
        if (result != 0.5) exit;
    }
}

test_string()
{
    sys->print("test_string()\n");
    count4 := "abcd";
    if (len count4 != 4) exit;

    array4 := array of byte count4;
    if (len array4 != 4) exit;

    count6 := "わがよたれぞ";
    if (len count6 != 6) exit;

    array18 := array of byte count6;
    if (len array18 != 18) exit;

    count6_2 := string array18;
    if (count6 != count6_2) exit;

    count7 := "खा सकता";
    if (len count7 != 7) exit;

    array19 := array of byte count7;
    if (len array19 != 19) exit;

    count36 := "abcdefghijklmnopqrstuvwxyz0123456789";
    if (len count36 != 36) exit;

    # Grow string
    {
        ch := 'a';
        str : string;
        str[len str] = ch;
        str[len str] = ch + 1;
        str[len str] = ch + 2;
        str[len str] = ch + 3;
        str[len str] = ch + 4;
        if (len str != 5) exit;
        if (str[2] != 'c') exit;

        str[2] = 'よ';
        if (len str != 5) exit;
        if (str[2] != 'よ') exit;
    }

    # Compare (ascii)
    {
        str_l := "a";
        str_l2 := "a";
        str_g := "z";

        if (!(str_l == str_l)) exit;
        if (!(str_l == str_l2)) exit;
        if (!(str_l <= str_l2)) exit;
        if (!(str_l >= str_l2)) exit;
        if (!(str_l < str_g)) exit;
        if (!(str_g > str_l)) exit;

        str_e : string;
        if (str_l == str_e) exit;
        if (!(str_l != str_e)) exit;
        if (str_l <= str_e) exit;
        if (!(str_l >= str_e)) exit;
        if (str_l < str_e) exit;
        if (!(str_g > str_e)) exit;

        if (str_e == str_l) exit;
        if (!(str_e != str_l)) exit;
        if (!(str_e <= str_l)) exit;
        if (str_e >= str_l) exit;
        if (!(str_e < str_l)) exit;
        if (str_e > str_g) exit;
    }

    # Compare (multi-byte)
    {
        str_l := "Α";
        str_l2 := "Α";
        str_g := "Ω";

        if (!(str_l == str_l)) exit;
        if (!(str_l == str_l2)) exit;
        if (!(str_l <= str_l2)) exit;
        if (!(str_l >= str_l2)) exit;
        if (!(str_l < str_g)) exit;
        if (!(str_g > str_l)) exit;

        str_e : string;
        if (str_l == str_e) exit;
        if (!(str_l != str_e)) exit;
        if (str_l <= str_e) exit;
        if (!(str_l >= str_e)) exit;
        if (str_l < str_e) exit;
        if (!(str_g > str_e)) exit;

        if (str_e == str_l) exit;
        if (!(str_e != str_l)) exit;
        if (!(str_e <= str_l)) exit;
        if (str_e >= str_l) exit;
        if (!(str_e < str_l)) exit;
        if (str_e > str_g) exit;
    }

    # Compare (multi-byte/ascii mix)
    {
        str_l := "a";
        str_l2 := "a";
        str_g := "Ω";

        if (!(str_l == str_l)) exit;
        if (!(str_l == str_l2)) exit;
        if (!(str_l <= str_l2)) exit;
        if (!(str_l >= str_l2)) exit;
        if (!(str_l < str_g)) exit;
        if (!(str_g > str_l)) exit;

        str_e : string;
        if (str_l == str_e) exit;
        if (!(str_l != str_e)) exit;
        if (str_l <= str_e) exit;
        if (!(str_l >= str_e)) exit;
        if (str_l < str_e) exit;
        if (!(str_g > str_e)) exit;

        if (str_e == str_l) exit;
        if (!(str_e != str_l)) exit;
        if (!(str_e <= str_l)) exit;
        if (str_e >= str_l) exit;
        if (!(str_e < str_l)) exit;
        if (str_e > str_g) exit;
    }

    # Concat
    {
        s1 := "1";
        s2 := "2";
        s3 : string;

        s4 := s1 + s2;
        if (s4 != "12") exit;

        s1 += s2;
        if (s1 != "12") exit;

        s5 := s3 + s1;
        s6 := s3 + s3;
    }

    # Slice
    {
        s1 := "01234567";
        r1 := s1[2:6];
        if (r1 != "2345") exit;

        s2 : string;
        r2 := s2[0:0];
        if (r2 != nil) exit;
    }

    # Convert number => string
    {
        w_val := 12345;
        s_w := string w_val;
        if (s_w != "12345") exit;

        r_val := 3.14;
        s_r := string r_val;
        if (s_r != "3.14") exit;

        b_val := big 987532;
        s_b := string b_val;
        if (s_b != "987532") exit;
    }

    # Convert string => number
    {
        s_w := "12345";
        w_val := int s_w;
        if (w_val != 12345) exit;

        s_r := "3.14";
        r_val := real s_r;
        if (r_val != 3.14) exit;

        s_b := "987532";
        b_val := big s_b;
        if (b_val != big 987532) exit;
    }
}

test_array()
{
    sys->print("test_array()\n");
    arr : array of int;
    a := len arr;
    if (a != 0) exit;

    test_array_byte();
    test_array_int();
    test_array_big();
    test_array_real();
}

test_array_byte()
{
    arr := array [] of { byte 1, byte 2, byte 3, byte 4, byte 5, byte 6 };

    sl1 := arr[2:5];
    if (len sl1 != 3) exit;
    if (sl1[0] != byte 3) exit;
    if (sl1[2] != byte 5) exit;

    sl2 := sl1[1:];
    if (len sl2 != 2) exit;
    if (sl2[0] != byte 4) exit;
    if (sl2[1] != byte 5) exit;

    sl3 := array[4] of byte;
    sl3[0] = byte 16ree;
    sl3[1:] = arr[3:5];
    sl3[3] = byte 16ree;
    if (sl3[1] != byte 4) exit;
    if (sl3[2] != byte 5) exit;
}

test_array_int()
{
    arr := array [] of { 1, 2, 3, 4, 5, 6 };

    sl1 := arr[2:5];
    if (len sl1 != 3) exit;
    if (sl1[0] != 3) exit;
    if (sl1[2] != 5) exit;

    sl2 := sl1[1:];
    if (len sl2 != 2) exit;
    if (sl2[0] != 4) exit;
    if (sl2[1] != 5) exit;

    sl3 := array[4] of int;
    sl3[0] = 16reeee;
    sl3[1:] = arr[3:5];
    sl3[3] = 16reeee;
    if (sl3[1] != 4) exit;
    if (sl3[2] != 5) exit;
}

test_array_big()
{
    arr := array [] of { 16r100000001, 16r100000002, 16r100000003, 16r100000004, 16r100000005, 16r100000006 };

    sl1 := arr[2:5];
    if (len sl1 != 3) exit;
    if (sl1[0] != 16r100000003) exit;
    if (sl1[2] != 16r100000005) exit;

    sl2 := sl1[1:];
    if (len sl2 != 2) exit;
    if (sl2[0] != 16r100000004) exit;
    if (sl2[1] != 16r100000005) exit;

    sl3 := array[4] of big;
    sl3[0] = 16reeeeeeeeeeeeeeee;
    sl3[1:] = arr[3:5];
    sl3[3] = 16reeeeeeeeeeeeeeee;
    if (sl3[1] != 16r100000004) exit;
    if (sl3[2] != 16r100000005) exit;
}

test_array_real()
{
    arr := array [] of { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };

    sl1 := arr[2:5];
    if (len sl1 != 3) exit;
    if (sl1[0] != 3.0) exit;
    if (sl1[2] != 5.0) exit;

    sl2 := sl1[1:];
    if (len sl2 != 2) exit;
    if (sl2[0] != 4.0) exit;
    if (sl2[1] != 5.0) exit;

    sl3 := array[4] of real;
    sl3[0] = 1.1111111;
    sl3[1:] = arr[3:5];
    sl3[3] = 1.1111111;
    if (sl3[1] != 4.0) exit;
    if (sl3[2] != 5.0) exit;
}

test_list()
{
    sys->print("test_list()\n");
    lst : list of int;
    l := len lst;
    if (l != 0) exit;

    test_list_byte();
    test_list_int();
    test_list_big();
    test_list_real();
    test_list_adt();
}

test_list_byte()
{
    lst0 : list of byte;
    lst1 := byte 16rff :: lst0;
    lst2 := byte 16rfe :: lst1;
    lst3 := byte 16rfd :: lst2;

    if (len lst3 != 3) exit;
    if (hd lst3 != byte 16rfd) exit;
    if (hd tl tl lst3 != byte 16rff) exit;
}

test_list_int()
{
    lst0 : list of int;
    lst1 := 16rff00 :: lst0;
    lst2 := 16rfe00 :: lst1;
    lst3 := 16rfd00 :: lst2;

    if (len lst3 != 3) exit;
    if (hd lst3 != 16rfd00) exit;
    if (hd tl tl lst3 != 16rff00) exit;
}

test_list_big()
{
    lst0 : list of big;
    lst1 := 16rff000000ff000000 :: lst0;
    lst2 := 16rfe000000ff000000 :: lst1;
    lst3 := 16rfd000000ff000000 :: lst2;

    if (len lst3 != 3) exit;
    if (hd lst3 != 16rfd000000ff000000) exit;
    if (hd tl tl lst3 != 16rff000000ff000000) exit;
}

test_list_real()
{
    lst0 : list of real;
    lst1 := 3.0 :: lst0;
    lst2 := 2.0 :: lst1;
    lst3 := 1.0 :: lst2;

    if (len lst3 != 3) exit;
    if (hd lst3 != 1.0) exit;
    if (hd tl tl lst3 != 3.0) exit;
}

ListAdt : adt
{
    s : string;
    r : real;
};

test_list_adt()
{
    {
        lst0 : list of ListAdt;
        lst1 := ( "three", 3.0 ) :: lst0;
        lst2 := ( "two", 2.0 ) :: lst1;
        lst3 := ( "one", 1.0 ) :: lst2;

        if (len lst3 != 3) exit;
        if ((hd lst3).r != 1.0) exit;
        if ((hd tl tl lst3).r != 3.0) exit;
    }

    {
        e3 := ref ListAdt ( "three", 3.0 );
        e2 := ref ListAdt ( "two", 2.0 );
        e1 := ref ListAdt ( "one", 1.0 );
        lst0 : list of ref ListAdt;
        lst1 := e3 :: lst0;
        lst2 := e2 :: lst1;
        lst3 := e1 :: lst2;

        if (len lst3 != 3) exit;
        if ((hd lst3).r != 1.0) exit;
        if ((hd tl tl lst3).r != 3.0) exit;
    }
}

test_branch()
{
    sys->print("test_branch()\n");
    test_branch_byte();
    test_branch_int();
    test_branch_big();
    test_branch_real();
}

test_branch_byte()
{
    a := byte 16rff;
    b := byte 16rff;
    if (a < b) exit;
    if (a > b) exit;
    if (a != b) exit;
    if (!(a == b)) exit;
}

test_branch_int()
{
    a := 1024;
    b := 1024;
    if (a < b) exit;
    if (a > b) exit;
    if (a != b) exit;
    if (!(a == b)) exit;
}

test_branch_big()
{
    a := 16r10000000FFFFFFFF;
    b := 16r10000000FFFFFFFF;
    if (a < b) exit;
    if (a > b) exit;
    if (a != b) exit;
    if (!(a == b)) exit;
}

test_branch_real()
{
    a := 1.0;
    b := 1.0;
    if (a < b) exit;
    if (a > b) exit;
    if (a != b) exit;
    if (!(a == b)) exit;
}

test_for_loop_function_call()
{
    sys->print("test_for_loop_function_call()\n");
    for (j := 0; j < 20; j++)
    {
        is_odd := odd(j);
        is_even := even(j);

        if (j % 2 == 0)
        {
            if (is_odd != 0) exit;
            if (is_even != 1) exit;
        }
        else
        {
            if (is_odd != 1) exit;
            if (is_even != 0) exit;
        }
    }
}

odd(a: int) : int
{
    if (a <= 0)
        return 0;

    return even(a - 1);
}

even(a: int) : int
{
    if (a <= 0)
        return 1;

    return odd(a - 1);
}
