ExtModule : module 
{
    PATH : con "external_module.dis";

    module_constant : con 16r789abcde;

    DataType : adt 
    {
        value : int;
    };

    spawn_main: fn();

    return_int: fn(): int;

    double_int: fn(input: int): int;

    return_datatype: fn(): DataType;

    double_datatype_value: fn(input: DataType): DataType;

    return_datatype_ref: fn(): ref DataType;

    double_datatype_ref_value: fn(input: ref DataType): ref DataType;

    Table: adt 
    {
        tab: array of list of (string, string);

        alloc: fn(n: int) : ref Table;

        hash: fn(ht: self ref Table, name: string) : int;
        add: fn(ht: self ref Table, name: string, val: string);
        lookup: fn(ht: self ref Table, name: string) : (int, string);
    };
};