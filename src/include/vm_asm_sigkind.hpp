//
// Dis VM
// File: vm_asm_sigkind.hpp
// Author: arr
//

#ifndef _DISVM_SRC_INCLUDE_VM_ASM_SIGKIND_HPP_
#define _DISVM_SRC_INCLUDE_VM_ASM_SIGKIND_HPP_

#include <iosfwd>
#include <sstream>
#include <cassert>
#include <cstdint>
#include "runtime.hpp"

namespace disvm
{
    namespace assembly
    {
        // Namespace named after enum type in Limbo compiler
        // Source: limbo/types.c
        namespace sigkind
        {
            // Signature stream
            class sig_stream_t final
            {
            public: // static
                static uint32_t compute_signature_hash(const char *);

                // Type delimiter
                static constexpr char delim();

            public:
                std::string get_signature() const;

                uint32_t get_signature_hash() const;

                sig_stream_t &operator<<(const char);
                sig_stream_t &operator<<(const char *);
                sig_stream_t &operator<<(const uint32_t);
                sig_stream_t &operator<<(const sig_stream_t &);

            private:
                mutable std::stringstream _ss;
            };

            constexpr char sig_stream_t::delim() { return ','; }

            // Type context modifiers
            enum class modifier_id_t : uint8_t
            {
                self = 'S',
                cycle = 'y',
            };

            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::modifier_id_t &);

            // Instrinsic VM types
            enum class type_id_t : uint8_t
            {
                unknown = 0, // This will be interpreted as a 'null' value which will help debug where the type building failed.
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
                adt_pick_tag = 'p',
                tuple = 't',
                module = 'm',
                function = 'f',
                function_varargs = 0xff, // "f*"
            };

            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::type_id_t &);

            template<type_id_t T>
            struct Tbasic_type
            {
                static const type_id_t id;

                constexpr operator type_id_t() const
                {
                    return Tbasic_type::id;
                }
            };

            template<type_id_t T>
            const type_id_t Tbasic_type<T>::id = T;

            using Tnone = Tbasic_type<type_id_t::none>;
            using Tbyte = Tbasic_type<type_id_t::byte>;
            using Tinteger = Tbasic_type<type_id_t::integer>;
            using Treal = Tbasic_type<type_id_t::real>;
            using Tbig = Tbasic_type<type_id_t::big>;
            using Tstring = Tbasic_type<type_id_t::string>;
            using Tpoly = Tbasic_type<type_id_t::poly>;

            class Tfixed final : public Tbasic_type<type_id_t::fixed>
            {
            public:
                Tfixed(double scale);

                friend sig_stream_t &operator<<(sig_stream_t &os, const Tfixed &);

            private:
                const double _scale;
            };

            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tfixed &);

            // Type that contains/modifies another type
            template <type_id_t ID>
            class Tbasic_aggregate_type : public Tbasic_type<ID>
            {
            public:
                template<typename U>
                static Tbasic_aggregate_type create(const U &type)
                {
                    sig_stream_t ss;
                    ss << Tbasic_type<ID>::id << type;

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

            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tarray &);
            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tref &);
            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tlist &);
            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tchannel &);

            // Reference to another previously defined type in the current signature
            class Ttype_ref final
            {
            public: // static
                static const char id;

            public:
                Ttype_ref(uint32_t num);

                friend sig_stream_t &operator<<(sig_stream_t &os, const Ttype_ref &);

            private:
                const uint32_t _num;
            };

            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Ttype_ref &);

            // Named member type
            class Tm final
            {
            public:
                template <typename T>
                Tm(const char *name, const T &type)
                {
                    _s << name << ':' << type;
                }

                friend sig_stream_t &operator<<(sig_stream_t &os, const Tm &);

            private:
                sig_stream_t _s;
            };

            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tm &);

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
                ss << t1 << sig_stream_t::delim();
                nested_type(ss, t2, s...);
            }

            // Type whose definition depends on contained types
            template <type_id_t ID>
            class Tnested_aggregate_type : public Tbasic_type<ID>
            {
            public: // static
                static constexpr char begin();
                static constexpr char end();

                template<typename ...NT>
                static Tnested_aggregate_type create(const NT&... nestedTypes)
                {
                    sig_stream_t ss;
                    ss << Tbasic_type<ID>::id << Tnested_aggregate_type::begin();

                    nested_type(ss, nestedTypes...);

                    ss << Tnested_aggregate_type::end();

                    return Tnested_aggregate_type{ ss };
                }

            public:
                Tnested_aggregate_type(sig_stream_t &s)
                    : _s{ std::move(s) }
                { }

                friend sig_stream_t &operator<<(sig_stream_t &, const Tnested_aggregate_type &);

            private:
                sig_stream_t _s;
            };

            using Ttuple = Tnested_aggregate_type<type_id_t::tuple>;
            using Tmodule = Tnested_aggregate_type<type_id_t::module>;
            using _Tadt = Tnested_aggregate_type<type_id_t::adt>;

            template<> constexpr char Ttuple::begin() { return '('; }
            template<> constexpr char Ttuple::end() { return ')'; }

            template<> constexpr char Tmodule::begin() { return '{'; }
            template<> constexpr char Tmodule::end() { return '}'; }

            template<> constexpr char _Tadt::begin() { return '('; }
            template<> constexpr char _Tadt::end() { return ')'; }

            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Ttuple &);
            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tmodule &);

            // Abstract data type (ADT) pick tag
            // Note: This type should only be a part of an ADT instance.
            class Tadt_pick_tag final : public Tbasic_type<type_id_t::adt_pick_tag>
            {
            public:  // static
                static constexpr const char *tag_delim();
                static constexpr char begin();
                static constexpr char end();

                static Tadt_pick_tag create(const char *pick_tag, uint32_t adt_ref, std::initializer_list<const Tm> tms);

                friend sig_stream_t &operator<<(sig_stream_t &, const Tadt_pick_tag &);

            private:
                sig_stream_t _s;
                Tadt_pick_tag(sig_stream_t &s) : _s{ std::move(s) } { }
            };

            constexpr const char *Tadt_pick_tag::tag_delim() { return "=>"; }
            constexpr char Tadt_pick_tag::begin() { return '('; }
            constexpr char Tadt_pick_tag::end() { return ')'; }

            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tadt_pick_tag &);

            // Abstract data type (ADT)
            class Tadt final : public _Tadt
            {
            public: // static
                // Unhide base class version of function
                using _Tadt::create;

                // Create an ADT with a single pick tag
                static Tadt create(const Tadt_pick_tag &p)
                {
                    sig_stream_t ss;
                    ss << Tbasic_type::id
                        << Tadt::begin()
                        << sig_stream_t::delim()
                        << p
                        << Tadt::end();

                    return Tadt{ ss };
                }

                template<typename T, typename ...NT>
                static Tadt create(const Tadt_pick_tag &p, const T &t, const NT&... nestedTypes)
                {
                    // [SPEC] When generating type signatures, the Limbo compiler
                    // doesn't identify the case where a pick tag is the first type
                    // so it always prefixes the nested signature with a leading ','.
                    // We special case this scenario to maintain compatibility. The
                    // single pick tag for an ADT must also be handled (above).
                    sig_stream_t ss;
                    ss << Tbasic_type::id
                        << Tadt::begin()
                        << sig_stream_t::delim()
                        << p
                        << sig_stream_t::delim();

                    nested_type(ss, t, nestedTypes...);

                    ss << Tadt::end();

                    return Tadt{ ss };
                }

            private:
                Tadt(sig_stream_t &s) : _Tadt{ s } { }
            };

            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tnested_aggregate_type<type_id_t::adt> &);

            // Function type
            template<type_id_t T>
            class Tfunction_base_type final : public Tbasic_type<T>
            {
            public: // static
                static constexpr char begin();
                static constexpr char end();

                template<typename ...AT>
                static Tfunction_base_type create(AT... argTypes)
                {
                    Tfunction_base_type a;
                    a._s << Tfunction_base_type::id << Tfunction_base_type::begin();

                    nested_type(a._s, argTypes...);

                    a._s << Tfunction_base_type::end();
                    return a;
                }

            public:
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

            template<type_id_t T>
            constexpr char Tfunction_base_type<T>::begin() { return '('; }

            template<type_id_t T>
            constexpr char Tfunction_base_type<T>::end() { return ')'; }

            using Tfunction = Tfunction_base_type<type_id_t::function>;
            using Tfunction_varargs = Tfunction_base_type<type_id_t::function_varargs>;

            sig_stream_t &operator<<(sig_stream_t &, disvm::assembly::sigkind::Tfunction &);
            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tfunction &);
            sig_stream_t &operator<<(sig_stream_t &, disvm::assembly::sigkind::Tfunction_varargs &);
            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tfunction_varargs &);
        }
    }
}

#endif // _DISVM_SRC_INCLUDE_VM_ASM_SIGKIND_HPP_
