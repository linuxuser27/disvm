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
        // Construct a single address code from the addressing enumerations
        constexpr disvm::runtime::addr_code_t construct_address_code(
            disvm::runtime::address_mode_t src,
            disvm::runtime::address_mode_middle_t mid,
            disvm::runtime::address_mode_t dest)
        {
            return static_cast<uint8_t>(mid) << 6 | static_cast<uint8_t>(src) << 3 | static_cast<uint8_t>(dest);
        }

        // Get string repsentation of opcode
        const char *opcode_to_token(disvm::opcode_t);

        // Get the opcode from this string.
        // If the supplied string cannot be resolved or if it is null, the 'invalid' opcode is returned.
        disvm::opcode_t token_to_opcode(const char *);

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
                static const char delim = ',';

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

            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tarray &);
            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tref &);
            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tlist &);
            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tchannel &);

            // Reference to another previously defined type in the current signature
            class Ttype_ref final
            {
            public: // static
                static const char id = '@';

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
                ss << t1 << sig_stream_t::type_delim;
                nested_type(ss, t2, s...);
            }

            // Type whose definition depends on contained types
            template <type_id_t ID>
            class Tnested_aggregate_type : public Tbasic_type<ID>
            {
            public:  // static
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

            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Ttuple &);
            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tmodule &);

            // Abstract data type (ADT) pick tag
            // Note: This type should only be a part of an ADT instance.
            class Tadt_pick_tag final : public Tbasic_type<type_id_t::adt_pick_tag>
            {
            public:  // static
                static const char *tag_delim;
                static const char begin = '(';
                static const char end = ')';

                static Tadt_pick_tag create(const char *pick_tag, uint32_t adt_ref, std::initializer_list<const Tm> tms);

                friend sig_stream_t &operator<<(sig_stream_t &, const Tadt_pick_tag &);

            private:
                sig_stream_t _s;
                Tadt_pick_tag(sig_stream_t &s) : _s{ std::move(s) } { }
            };

            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tadt_pick_tag &);

            // Abstract data type (ADT)
            class Tadt final : public Tnested_aggregate_type<type_id_t::adt>
            {
            public: // static
                // Unhide base class version of function
                using Tnested_aggregate_type::create;

                // Create an ADT with a single pick tag
                static Tadt create(const Tadt_pick_tag &p)
                {
                    sig_stream_t ss;
                    ss << Tbasic_type::id
                        << Tadt::begin
                        << sig_stream_t::delim
                        << p
                        << Tadt::end;

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
                        << Tadt::begin
                        << sig_stream_t::delim
                        << p
                        << sig_stream_t::delim;

                    nested_type(ss, t, nestedTypes...);

                    ss << Tadt::end;

                    return Tadt{ ss };
                }

            private:
                Tadt(sig_stream_t &s) : Tnested_aggregate_type{ std::move(s) } { }
            };

            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tnested_aggregate_type<type_id_t::adt> &);

            // Function type
            template<type_id_t T>
            class Tfunction_base_type final : public Tbasic_type<T>
            {
            public: // static
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

            using Tfunction = Tfunction_base_type<type_id_t::function>;
            using Tfunction_varargs = Tfunction_base_type<type_id_t::function_varargs>;

            sig_stream_t &operator<<(sig_stream_t &, disvm::assembly::sigkind::Tfunction &);
            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tfunction &);
            sig_stream_t &operator<<(sig_stream_t &, disvm::assembly::sigkind::Tfunction_varargs &);
            sig_stream_t &operator<<(sig_stream_t &, const disvm::assembly::sigkind::Tfunction_varargs &);
        }
    }

    namespace symbol
    {
        // Type class enum for symbols is unrelated to signature types enum.
        // The historical reason for this isn't recorded but is found in
        // the official Limbo compiler.
        enum class type_class_t : uint8_t
        {
            type_index = '@',
            adt = 'a',
            adt_pick = 'p',
            tuple = 't',
            module = 'm',
            function = 'F',
            array = 'A',
            channel = 'C',
            list = 'L',
            ref = 'R',
            none = 'n',
            nil = 'N',
            big = 'B',
            byte = 'b',
            integer = 'i',
            real = 'f',
            string = 's',
            poly = 'P',
        };

        // Reference into source code
        struct source_ref_t
        {
            int32_t source_id;
            int32_t begin_line;
            int32_t begin_column;
            int32_t end_line;
            int32_t end_column;
        };

        class symbol_debug_t
        {
        public:
            // Set the current pc
            virtual void set_current_pc(disvm::runtime::vm_pc_t) = 0;

            // Enumeration for advancing the pc
            enum class advance_pc_t
            {
                current, // Return the current pc
                next_pc,
                next_debug_statement, // Advance to the next debug statement (i.e. source language statement)
            };

            // Advance the current pc.
            // Returns 'true' if the function was successful, otherwise 'false'.
            virtual bool try_advance_pc(advance_pc_t, disvm::runtime::vm_pc_t *pc_after_advance = nullptr) = 0;

            // Return the name of the function encompassing the current pc.
            // This function will throw a 'std::runtime_error' if a function doesn't exist at the current pc.
            virtual const std::string & current_function_name() const = 0;

            // Return the source location for the current pc
            virtual source_ref_t current_source_location() const = 0;

            // Get source file name based on ID
            virtual const std::string &get_source_by_id(int32_t source_id) const = 0;
        };

        class symbol_data_t
        {
        public:
            // Get name of the module
            virtual const std::string& get_module_name() const = 0;

            // Get interface over symbol data targeted for debugging
            virtual std::unique_ptr<symbol_debug_t> get_debug() = 0;
        };

        std::unique_ptr<symbol_data_t> read(std::istream &);
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