//
// Dis VM
// File: opcodes.h
// Author: arr
//

#ifndef _DISVM_SRC_INCLUDE_OPCODES_H_
#define _DISVM_SRC_INCLUDE_OPCODES_H_

#include <cstdint>

namespace disvm
{
    // Instruction codes for the Dis VM
    enum class opcode_t
    {
        invalid = 0,
        alt,
        nbalt,
        goto_,
        call,
        frame,
        spawn,
        runt,
        load,
        mcall,
        mspawn,
        mframe,
        ret,
        jmp,
        casew,
        exit,
        new_,
        newa,
        newcb,
        newcw,
        newcf,
        newcp,
        newcm,
        newcmp,
        send,
        recv,
        consb,
        consw,
        consp,
        consf,
        consm,
        consmp = 0x1f,
        headb,
        headw,
        headp,
        headf,
        headm,
        headmp,
        tail,
        lea,
        indx,
        movp,
        movm,
        movmp,
        movb,
        movw,
        movf,
        cvtbw,
        cvtwb,
        cvtfw,
        cvtwf,
        cvtca,
        cvtac,
        cvtwc,
        cvtcw,
        cvtfc,
        cvtcf,
        addb,
        addw,
        addf,
        subb,
        subw,
        subf,
        mulb = 0x3f,
        mulw,
        mulf,
        divb,
        divw,
        divf,
        modw,
        modb,
        andb,
        andw,
        orb,
        orw,
        xorb,
        xorw,
        shlb,
        shlw,
        shrb,
        shrw,
        insc,
        indc,
        addc,
        lenc,
        lena,
        lenl,
        beqb,
        bneb,
        bltb,
        bleb,
        bgtb,
        bgeb,
        beqw,
        bnew,
        bltw = 0x5f,
        blew,
        bgtw,
        bgew,
        beqf,
        bnef,
        bltf,
        blef,
        bgtf,
        bgef,
        beqc,
        bnec,
        bltc,
        blec,
        bgtc,
        bgec,
        slicea,
        slicela,
        slicec,
        indw,
        indf,
        indb,
        negf,
        movl,
        addl,
        subl,
        divl,
        modl,
        mull,
        andl,
        orl,
        xorl,
        shll = 0x7f,
        shrl,
        bnel,
        bltl,
        blel,
        bgtl,
        bgel,
        beql,
        cvtlf,
        cvtfl,
        cvtlw,
        cvtwl,
        cvtlc,
        cvtcl,
        headl,
        consl,
        newcl,
        casec,
        indl,
        movpc,
        tcmp,
        mnewz,
        cvtrf,
        cvtfr,
        cvtws,
        cvtsw,
        lsrw,
        lsrl,
        eclr,  // not used
        newz,
        newaz,
        raise,
        casel,
        mulx,
        divx,
        cvtxx,
        mulx0,
        divx0,
        cvtxx0,
        mulx1,
        divx1,
        cvtxx1,
        cvtfx,
        cvtxf,
        expw,
        expl,
        expf,
        self,

        // markers
        first_opcode = invalid,
        last_opcode = self
    };
}

#endif // _DISVM_SRC_INCLUDE_OPCODES_H_