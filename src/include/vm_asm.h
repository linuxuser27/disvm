//
// Dis VM
// File: vm_asm.h
// Author: arr
//

#ifndef _DISVM_SRC_INCLUDE_VM_ASM_H_
#define _DISVM_SRC_INCLUDE_VM_ASM_H_

#include <iosfwd>
#include <sstream>
#include <cassert>
#include <cstdint>
#include "opcodes.h"
#include "runtime.h"

namespace disvm
{
    namespace assembly
    {
        // Get string repsentation of opcode
        const char *opcode_to_token(disvm::opcode_t);

        // Get the opcode from this string.
        // If the supplied string cannot be resolved or if it is null, the 'invalid' opcode is returned.
        disvm::opcode_t token_to_opcode(const char *);

        // Namedspace named after enum type in Limbo compiler
        // Source: limbo/types.c
        namespace sigkind
        {
            // Signature stream
            class sig_stream_t
            {
            public: // static
                static uint32_t compute_signature_hash(const char *);

            public:
                std::string get_signature() const;

                uint32_t get_signature_hash() const;

                sig_stream_t &operator<<(const char);
                sig_stream_t &operator<<(const char *);
                sig_stream_t &operator<<(const sig_stream_t &);

            private:
                std::stringstream _ss;
            };

            // Type context modifiers
            enum class modifier_id_t : uint8_t
            {
                self = 'S',
                cycle = 'y',
            };

            sig_stream_t & operator<<(sig_stream_t &, const disvm::assembly::sigkind::modifier_id_t &);

            // Instrinsic VM types
            enum class type_id_t : uint8_t
            {
                unknown = 0,
                none = 'n',
                byte = 'b',
                integer = 'i',
                real = 'r',
                big = 'B',
                string = 's',
                poly = 'P',
                fixed = 'x',
                array = 'A',
                ref = 'R',
                list = 'L',
                channel = 'C',
                adt = 'a',
                //adtpick = 'p',
                tuple = 't',
                module = 'm',
                function = 'f',
                function_varargs = 0xff, // "f*"
            };

            sig_stream_t & operator<<(sig_stream_t &, const disvm::assembly::sigkind::type_id_t &);

            template<type_id_t T>
            struct Tbasic_type
            {
                static const type_id_t id = T;

                constexpr operator type_id_t() const
                {
                    return Tbasic_type::id;
                }
            };

            using Tnone = Tbasic_type<type_id_t::none>;
            using Tbyte = Tbasic_type<type_id_t::byte>;
            using Tinteger = Tbasic_type<type_id_t::integer>;
            using Treal = Tbasic_type<type_id_t::real>;
            using Tbig = Tbasic_type<type_id_t::big>;
            using Tstring = Tbasic_type<type_id_t::string>;
            using Tpoly = Tbasic_type<type_id_t::poly>;

            class Tfixed : public Tbasic_type<type_id_t::fixed>
            {
            public:
                Tfixed(double scale);

                friend sig_stream_t &operator<<(sig_stream_t &os, const Tfixed &);

            private:
                const double _scale;
            };

            sig_stream_t & operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tfixed &);

            // Type that contains/modifies another type
            template <type_id_t ID>
            class Tbasic_aggregate_type : public Tbasic_type<ID>
            {
            public:
                template<typename U>
                static Tbasic_aggregate_type create(const U &type)
                {
                    sig_stream_t ss;
                    ss << Tbasic_type::id << type;

                    return Tbasic_aggregate_type{ ss };
                }

                friend sig_stream_t &operator<<(sig_stream_t &, const Tbasic_aggregate_type &);

            private:
                sig_stream_t _s;
                Tbasic_aggregate_type(sig_stream_t &s) : _s{ std::move(s) } { }
            };

            using Tarray = Tbasic_aggregate_type<type_id_t::array>;
            using Tref = Tbasic_aggregate_type<type_id_t::ref>;
            using Tlist = Tbasic_aggregate_type<type_id_t::list>;
            using Tchannel = Tbasic_aggregate_type<type_id_t::channel>;

            sig_stream_t & operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tarray &);
            sig_stream_t & operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tref &);
            sig_stream_t & operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tlist &);
            sig_stream_t & operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tchannel &);

            // Reference to another previously defined type in the current signature
            struct Ttype_ref
            {
                Ttype_ref(uint32_t num);

                friend sig_stream_t &operator<<(sig_stream_t &os, const Ttype_ref &);

            private:
                const uint32_t _num;
            };

            sig_stream_t & operator<<(sig_stream_t &, const disvm::assembly::sigkind::Ttype_ref &);

            // Named member type
            struct Tm
            {
                Tm(const char *name, const type_id_t &type_id);
                Tm(const char *name, const char *type_id);

                friend sig_stream_t &operator<<(sig_stream_t &os, const Tm &);

            private:
                const char *_type_id_str;
                const type_id_t _type_id;
                const char *_name;
            };

            sig_stream_t & operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tm &);

            inline void nested_type(const sig_stream_t &)
            {
                return;
            }

            template<typename T, typename ...S>
            void nested_type(sig_stream_t &ss, const T &t, const S&...s)
            {
                ss << t;
                nested_type(ss, s...);
            }

            template<typename T1, typename T2, typename ...S>
            void nested_type(sig_stream_t &ss, const T1 &t1, const T2 &t2, const S&...s)
            {
                ss << t1 << ',';
                nested_type(ss, t2, s...);
            }

            // Type whose definition depends on contained types
            template <type_id_t ID>
            class Tnested_aggregate_type : public Tbasic_type<ID>
            {
            public:
                static const char begin;
                static const char end;

                template<typename ...NT>
                static Tnested_aggregate_type create(const NT&... nestedTypes)
                {
                    sig_stream_t ss;
                    ss << Tbasic_type::id << Tnested_aggregate_type::begin;

                    nested_type(ss, nestedTypes...);

                    ss << Tnested_aggregate_type::end;

                    return Tnested_aggregate_type{ ss };
                }

                friend sig_stream_t &operator<<(sig_stream_t &, const Tnested_aggregate_type &);

            private:
                sig_stream_t _s;
                Tnested_aggregate_type(sig_stream_t &s) : _s{ std::move(s) } { }
            };

            using Ttuple = Tnested_aggregate_type<type_id_t::tuple>;
            using Tadt = Tnested_aggregate_type<type_id_t::adt>;
            using Tmodule = Tnested_aggregate_type<type_id_t::module>;

            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Ttuple &);
            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tadt &);
            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tmodule &);

            // Function type
            template<type_id_t T>
            class Tfunction_base_type : public Tbasic_type<T>
            {
            public:
                static const char begin = '(';
                static const char end = ')';

                template<typename ...AT>
                static Tfunction_base_type create(AT... argTypes)
                {
                    Tfunction_base_type a;
                    a._s << Tfunction_base_type::id << Tfunction_base_type::begin;

                    nested_type(a._s, argTypes...);

                    a._s << Tfunction_base_type::end;
                    return a;
                }

                template<typename RT>
                Tfunction_base_type &returns(RT r)
                {
                    assert(!_fully_defined && "Function type already complete");
                    _s << r;
                    _fully_defined = true;

                    return *this;
                }

                friend sig_stream_t &operator<<(sig_stream_t &, Tfunction_base_type &);
                friend sig_stream_t &operator<<(sig_stream_t &, const Tfunction_base_type &);

            private:
                bool _fully_defined;
                sig_stream_t _s;
                Tfunction_base_type() : _fully_defined{ false } { }
            };

            using Tfunction = Tfunction_base_type<type_id_t::function>;
            using Tfunction_varargs = Tfunction_base_type<type_id_t::function_varargs>;

            sig_stream_t &operator<<(sig_stream_t &, disvm::assembly::sigkind::Tfunction &);
            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tfunction &);
            sig_stream_t &operator<<(sig_stream_t &, disvm::assembly::sigkind::Tfunction_varargs &);
            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tfunction_varargs &);
        }
    }

    namespace runtime
    {
        // Operators for printing bytecode instructions
        std::ostream& operator<<(std::ostream &, const disvm::runtime::inst_data_generic_t &);
        std::ostream& operator<<(std::ostream &, const disvm::runtime::middle_data_t &);
        std::ostream& operator<<(std::ostream &, const disvm::runtime::vm_exec_op_t &);
    }
}

#endif // _DISVM_SRC_INCLUDE_VM_ASM_H_