//
// Dis VM
// File: opcode_tokens.cpp
// Author: arr
//

#include <cassert>
#include <cstring>
#include <vm_asm.h>

namespace
{
    const char * opcode_tokens[] =
    {
        "invalid",
        "alt",
        "nbalt",
        "goto",
        "call",
        "frame",
        "spawn",
        "runt",
        "load",
        "mcall",
        "mspawn",
        "mframe",
        "ret",
        "jmp",
        "casew",
        "exit",
        "new",
        "newa",
        "newcb",
        "newcw",
        "newcf",
        "newcp",
        "newcm",
        "newcmp",
        "send",
        "recv",
        "consb",
        "consw",
        "consp",
        "consf",
        "consm",
        "consmp", // 0x1f
        "headb",
        "headw",
        "headp",
        "headf",
        "headm",
        "headmp",
        "tail",
        "lea",
        "indx",
        "movp",
        "movm",
        "movmp",
        "movb",
        "movw",
        "movf",
        "cvtbw",
        "cvtwb",
        "cvtfw",
        "cvtwf",
        "cvtca",
        "cvtac",
        "cvtwc",
        "cvtcw",
        "cvtfc",
        "cvtcf",
        "addb",
        "addw",
        "addf",
        "subb",
        "subw",
        "subf",
        "mulb", // 0x3f
        "mulw",
        "mulf",
        "divb",
        "divw",
        "divf",
        "modw",
        "modb",
        "andb",
        "andw",
        "orb",
        "orw",
        "xorb",
        "xorw",
        "shlb",
        "shlw",
        "shrb",
        "shrw",
        "insc",
        "indc",
        "addc",
        "lenc",
        "lena",
        "lenl",
        "beqb",
        "bneb",
        "bltb",
        "bleb",
        "bgtb",
        "bgeb",
        "beqw",
        "bnew",
        "bltw", // 0x5f
        "blew",
        "bgtw",
        "bgew",
        "beqf",
        "bnef",
        "bltf",
        "blef",
        "bgtf",
        "bgef",
        "beqc",
        "bnec",
        "bltc",
        "blec",
        "bgtc",
        "bgec",
        "slicea",
        "slicela",
        "slicec",
        "indw",
        "indf",
        "indb",
        "negf",
        "movl",
        "addl",
        "subl",
        "divl",
        "modl",
        "mull",
        "andl",
        "orl",
        "xorl",
        "shll", // 0x7f
        "shrl",
        "bnel",
        "bltl",
        "blel",
        "bgtl",
        "bgel",
        "beql",
        "cvtlf",
        "cvtfl",
        "cvtlw",
        "cvtwl",
        "cvtlc",
        "cvtcl",
        "headl",
        "consl",
        "newcl",
        "casec",
        "indl",
        "movpc",
        "tcmp",
        "mnewz",
        "cvtrf",
        "cvtfr",
        "cvtws",
        "cvtsw",
        "lsrw",
        "lsrl",
        "eclr",  // not used
        "newz",
        "newaz",
        "raise",
        "casel", // 0x9f
        "mulx",
        "divx",
        "cvtxx",
        "mulx0",
        "divx0",
        "cvtxx0",
        "mulx1",
        "divx1",
        "cvtxx1",
        "cvtfx",
        "cvtxf",
        "expw",
        "expl",
        "expf",
        "self",
        "brkpt",
    };

    // Verify the length of the opcode strings array against the total number of opcodes.
    static_assert((sizeof(opcode_tokens) / sizeof(opcode_tokens[0])) == (static_cast<std::size_t>(disvm::opcode_t::last_opcode) + 1), "Missing opcode string");
}

const char *disvm::assembly::opcode_to_token(disvm::opcode_t o)
{
    const auto opcode_index = static_cast<std::size_t>(o);
    assert(opcode_index < (sizeof(opcode_tokens) / sizeof(opcode_tokens[0])));
    return opcode_tokens[opcode_index];
}

disvm::opcode_t disvm::assembly::token_to_opcode(const char *t)
{
    assert(t != nullptr);
    if (t == nullptr)
        return opcode_t::invalid;

    // [PERF] Linear search for the token could be improved, but the array storage would need to be reconsidered.
    for (auto i = std::size_t{ 0 }; i < (sizeof(opcode_tokens) / sizeof(opcode_tokens[0])); ++i)
    {
        if (std::strcmp(opcode_tokens[i], t) == 0)
            return static_cast<disvm::opcode_t>(i);
    }

    return opcode_t::invalid;
}
