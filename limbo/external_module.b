implement ExtModule;

include "external_module.m";

spawn_main()
{
    a := return_int();
}

return_int(): int
{
    return module_constant;
}

double_int(input: int): int
{
    return input + input;
}
    
return_datatype(): DataType
{
    v : DataType;
    v.value = module_constant;

    return double_datatype_value(v);
}

double_datatype_value(input: DataType): DataType
{
    input.value *= 2;
    return input;
}

return_datatype_ref(): ref DataType
{
    d := return_datatype();
    return ref d;
}

double_datatype_ref_value(input: ref DataType): ref DataType
{
    input.value *= 2;
    return input;
}

Table.alloc(n: int) : ref Table
{
    return ref Table(array[n] of list of (string,string));
}

Table.hash(ht: self ref Table, s: string) : int
{
    h := 0;
    for (i := 0; i < len s; i++)
        h = (h << 1) ^ int s[i];
    h %= len ht.tab;
    if (h < 0)
        h += len ht.tab;
    return h;
}

Table.add(ht: self ref Table, name: string, val: string)
{
    h := ht.hash(name);
    for (p := ht.tab[h]; p != nil; p = tl p) {
        (tname, nil) := hd p;
        if (tname == name) {
            # illegal: hd p = (tname, val);
            return;
        }
    }
    ht.tab[h] = (name, val) :: ht.tab[h];
}

Table.lookup(ht: self ref Table, name: string) : (int, string)
{
    h := ht.hash(name);
    for (p := ht.tab[h]; p != nil; p = tl p) {
        (tname, tval) := hd p;
        if (tname == name)
            return (1, tval);
    }
    return (0, "");
}