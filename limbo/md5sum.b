implement Exec;

include "sys.m";
include "Exec.m";

sys : Sys;

block_size : con 64;
max_32 : con int 16rffffffff;

shift_map := array [] of {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

sine_table := array [] of {
    int 16rd76aa478, int 16re8c7b756, int 16r242070db, int 16rc1bdceee,
    int 16rf57c0faf, int 16r4787c62a, int 16ra8304613, int 16rfd469501,
    int 16r698098d8, int 16r8b44f7af, int 16rffff5bb1, int 16r895cd7be,
    int 16r6b901122, int 16rfd987193, int 16ra679438e, int 16r49b40821,
    int 16rf61e2562, int 16rc040b340, int 16r265e5a51, int 16re9b6c7aa,
    int 16rd62f105d, int 16r02441453, int 16rd8a1e681, int 16re7d3fbc8,
    int 16r21e1cde6, int 16rc33707d6, int 16rf4d50d87, int 16r455a14ed,
    int 16ra9e3e905, int 16rfcefa3f8, int 16r676f02d9, int 16r8d2a4c8a,
    int 16rfffa3942, int 16r8771f681, int 16r6d9d6122, int 16rfde5380c,
    int 16ra4beea44, int 16r4bdecfa9, int 16rf6bb4b60, int 16rbebfbc70,
    int 16r289b7ec6, int 16reaa127fa, int 16rd4ef3085, int 16r04881d05,
    int 16rd9d4d039, int 16re6db99e5, int 16r1fa27cf8, int 16rc4ac5665,
    int 16rf4292244, int 16r432aff97, int 16rab9423a7, int 16rfc93a039,
    int 16r655b59c3, int 16r8f0ccc92, int 16rffeff47d, int 16r85845dd1,
    int 16r6fa87e4f, int 16rfe2ce6e0, int 16ra3014314, int 16r4e0811a1,
    int 16rf7537e82, int 16rbd3af235, int 16r2ad7d2bb, int 16reb86d391};

leftrotate(x, c : int) : int
{
    # Limbo doesn't have an unsigned int so all right
    # shifts are arithmetic, not logical.
    addMask := 0;
    x1 := x;
    if (x1 < 0)
    {
        x1 = x1 & 16r7fffffff;
        addMask = 2 ** (c - 1);
    }

    return (x << c) | (x1 >> (32 - c)) | addMask;
}

md5_acc(block: array of byte, result: array of int)
{
    a := result[0];
    b := result[1];
    c := result[2];
    d := result[3];

    for (i := 0; i < block_size; i++)
    {
        f : int;
        g : int;
        case i
        {
            0 to 15 =>
                f = (b & c) | ((max_32 ^ b) & d);
                g = i;
            16 to 31 =>
                f = (b & d) | (c & (max_32 ^ d));
                g = ((5 * i) + 1) % 16;
            32 to 47 =>
                f = b ^ (c ^ d);
                g = ((3 * i) + 5) % 16;
            48 to 63 =>
                f = c ^ (b | (max_32 ^ d));
                g = (7 * i) % 16;
        }

        tmp := d;
        d = c;
        c = b;
        m := block[(g * 4):];
        v := int m[0] | (int m[1] << 8) | (int m[2] << 16) | (int m[3] << 24);
        b = b + leftrotate((a + f + sine_table[i] + v), shift_map[i]);
        a = tmp;
    }

    result[0] += a;
    result[1] += b;
    result[2] += c;
    result[3] += d;
}

pad_block(block: array of byte, curr_block_size: int, msg_size_bytes: big): array of byte
{
    if (curr_block_size > len(block) || len(block) != block_size) {
        raise "Invalid block or pad size";
    }

    # Zero out the excess block space
    for (i := curr_block_size; i < len(block); i++) {
        block[i] = byte 0;
    }

    aux_block : array of byte;
    pad_count := block_size - curr_block_size;

    # Check if there is room for the last bit and message size
    if (pad_count < 9) {
        aux_block = array [block_size] of byte;
    }

    # Set the last bit
    if (pad_count < 1) {
        aux_block[0] = byte 16r80;
    } else {
        block[curr_block_size] = byte 16r80;
        pad_count--;
        curr_block_size++;
    }

    # Set the message size in bits
    msg_size_bits_target : array of byte;
    msg_size_bits := (msg_size_bytes * big 8);
    if (pad_count < 8) {
        msg_size_bits_target = aux_block[56:];
    } else {
        msg_size_bits_target = block[56:];
    }

    mask : con byte 16rff;
    msg_size_bits_target[0] = byte msg_size_bits & mask;
    msg_size_bits_target[1] = byte (msg_size_bits >> 8) & mask;
    msg_size_bits_target[2] = byte (msg_size_bits >> 16) & mask;
    msg_size_bits_target[3] = byte (msg_size_bits >> 24) & mask;
    msg_size_bits_target[4] = byte (msg_size_bits >> 32) & mask;
    msg_size_bits_target[5] = byte (msg_size_bits >> 40) & mask;
    msg_size_bits_target[6] = byte (msg_size_bits >> 48) & mask;
    msg_size_bits_target[7] = byte (msg_size_bits >> 56) & mask;

    return aux_block;
}

print_block(name: string, block: array of byte)
{
    sys->print("%s\n", name);

    for (i := 0; i < len(block); i++)
    {
        if (i != 0 && i % 16 == 0)
            sys->print("\n");

        sys->print("%d ", int block[i]);
    }
    sys->print("\n");
}

init(args: list of string)
{
    sys = load Sys Sys->PATH;
    if (len(args) == 1) {
        sys->print("Usage: md5sum <file>");
        return;
    }

    fd := sys->open(hd tl args, sys->OREAD);
    if (fd == nil) {
        raise "Failed to open file";
    }

    checksum := array [] of {
        int 16r67452301,
        int 16refcdab89,
        int 16r98badcfe,
        int 16r10325476 };

    msg_size_bytes := big 0;
    block := array [block_size] of byte;
    c := sys->read(fd, block, block_size);
    while (c == block_size) {
        msg_size_bytes += big c;
        md5_acc(block, checksum);
        c = sys->read(fd, block, block_size);
    }

    msg_size_bytes += big c;

    # Pad the last block
    aux_block := pad_block(block, c, msg_size_bytes);
    md5_acc(block, checksum);
    if (aux_block != nil) {
        md5_acc(aux_block, checksum);
    }

    for (i := 0; i < len (checksum); ++i) {
        v := checksum[i];
        sys->print("%02x%02x%02x%02x", v & 16rff, (v >> 8) & 16rff, (v >> 16) & 16rff, (v >> 24) & 16rff);
    }
}