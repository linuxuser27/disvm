//
// Dis VM
// File: opcode_tokens.cpp
// Author: arr
//

#include <vm_asm.h>
#include <array>
#include "rfc1321\global.h"
#include "rfc1321\md5.h"

using namespace disvm::assembly::sigkind;

namespace
{
    // Compute the signature of the type using the MD5 checksum.
    // Based on the implementation from the Limbo compiler.
    // Source: limbo/types.c
    uint32_t _compute_sig(std::string &str)
    {
        MD5_CTX context;
        MD5Init(&context);

        MD5Update(&context, reinterpret_cast<unsigned char *>(const_cast<char *>(str.data())), str.length());

        std::array<uint8_t, 16> checksum;
        MD5Final(checksum.data(), &context);

        auto sig = uint32_t{ 0 };
        for (auto i = std::size_t{ 0 }; i < checksum.size(); i += 4)
            sig ^= checksum[i] | (checksum[i + 1] << 8) | (checksum[i + 2] << 16) | (checksum[i + 3] << 24);

        return sig;
    }
}

uint32_t sig_stream_t::compute_signature_hash(const char *sig)
{
    std::string sig_local{ sig };
    return _compute_sig(sig_local);
}

std::string sig_stream_t::get_signature() const
{
    return _ss.str();
}

uint32_t sig_stream_t::get_signature_hash() const
{
    auto sig_str = get_signature();
    return _compute_sig(sig_str);
}

sig_stream_t &sig_stream_t::operator<<(const char c)
{
    _ss << c;
    return *this;
}

sig_stream_t &sig_stream_t::operator<<(const char *s)
{
    _ss << s;
    return *this;
}

sig_stream_t &sig_stream_t::operator<<(const sig_stream_t &ss)
{
    _ss << ss._ss.rdbuf();
    return *this;
}

sig_stream_t &disvm::assembly::sigkind::operator<<(sig_stream_t &os, const modifier_id_t &id)
{
    return os << static_cast<char>(id);
}

sig_stream_t &disvm::assembly::sigkind::operator<<(sig_stream_t &os, const type_id_t &id)
{
    assert(id != type_id_t::unknown);

    if (id == type_id_t::function_varargs)
        return os << "f*";

    return os << static_cast<char>(id);
}

Tfixed::Tfixed(double scale)
    : _scale{ scale }
{ }

sig_stream_t &disvm::assembly::sigkind::operator<<(sig_stream_t &os, const Tfixed &f)
{
    char buffer[32];
    ::sprintf(buffer, "%g", f._scale);
    return os << Tfixed::id << buffer;
}

sig_stream_t &disvm::assembly::sigkind::operator<<(sig_stream_t &os, const Tarray &a)
{
    return os << a._s;
}

sig_stream_t &disvm::assembly::sigkind::operator<<(sig_stream_t &os, const Tref &r)
{
    return os << r._s;
}

sig_stream_t &disvm::assembly::sigkind::operator<<(sig_stream_t &os, const Tlist &l)
{
    return os << l._s;
}

sig_stream_t &disvm::assembly::sigkind::operator<<(sig_stream_t &os, const Tchannel &c)
{
    return os << c._s;
}

Ttype_ref::Ttype_ref(uint32_t num)
    : _num{ num }
{ }

sig_stream_t &disvm::assembly::sigkind::operator<<(sig_stream_t &os, const Ttype_ref &r)
{
    return os << '@' << r._num;
}

Tm::Tm(const char *name, const type_id_t &type_id)
    : _name{ name }
    , _type_id{ type_id }
    , _type_id_str{ nullptr }
{ }

Tm::Tm(const char *name, const char *type_id)
    : _name{ name }
    , _type_id{ type_id_t::unknown }
    , _type_id_str{ type_id }
{ }

sig_stream_t &disvm::assembly::sigkind::operator<<(sig_stream_t &os, const Tm &m)
{
    if (m._type_id_str == nullptr)
    {
        assert(m._type_id != type_id_t::unknown);
        return os << m._name << ':' << m._type_id;
    }
    else
    {
        assert(m._type_id == type_id_t::unknown);
        return os << m._name << ':' << m._type_id_str;
    }
}

const char Ttuple::begin = '(';
const char Ttuple::end = ')';

const char Tadt::begin = '(';
const char Tadt::end = ')';

const char Tmodule::begin = '{';
const char Tmodule::end = '}';

sig_stream_t &disvm::assembly::sigkind::operator<<(sig_stream_t &os, const Ttuple &t)
{
    return os << t._s;
}

sig_stream_t &disvm::assembly::sigkind::operator<<(sig_stream_t &os, const Tadt &a)
{
    return os << a._s;
}

sig_stream_t &disvm::assembly::sigkind::operator<<(sig_stream_t &os, const Tmodule &m)
{
    return os << m._s;
}

sig_stream_t &disvm::assembly::sigkind::operator<<(sig_stream_t &os, Tfunction &f)
{
    if (!f._fully_defined)
        f.returns(Tnone::id);

    return os << f._s;
}

sig_stream_t &disvm::assembly::sigkind::operator<<(sig_stream_t &os, const Tfunction &f)
{
    assert(!f._fully_defined && "Incomplete function");
    return os << f._s;
}

sig_stream_t &disvm::assembly::sigkind::operator<<(sig_stream_t &os, Tfunction_varargs &f)
{
    if (!f._fully_defined)
        f.returns(Tnone::id);

    return os << f._s;
}

sig_stream_t &disvm::assembly::sigkind::operator<<(sig_stream_t &os, const Tfunction_varargs &f)
{
    assert(!f._fully_defined && "Incomplete function");
    return os << f._s;
}
