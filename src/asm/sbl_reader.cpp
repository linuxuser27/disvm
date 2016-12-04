//
// Dis VM
// File: sbl_reader.cpp
// Author: arr
//

#include <cassert>
#include <string>
#include <vector>
#include <cerrno>
#include <memory>
#include <vm_asm.h>
#include <utils.h>
#include <buffered_reader.h>

using namespace disvm;
using namespace disvm::symbol;

namespace
{
    enum class sbl_version_t
    {
        unknown,
        sbl_2_0,
        sbl_2_1,
    };

    struct header_t
    {
        sbl_version_t version_id;
        std::string module_name;
    };

    using file_table_t = std::vector<std::string>;

    struct sbl_read_context_t
    {
        int32_t pc_count;
        int32_t last_file;
        int32_t last_line;
    };

    struct pc_item_t
    {
        source_ref_t source;
        int32_t debug_statement;
    };

    using pc_table_t = std::vector<pc_item_t>;

    struct type_t
    {
        type_class_t type_class;
        int32_t value_index_1;
        int32_t value_index_2;
    };

    struct id_item_t
    {
        uint32_t offset;
        std::string name;
        source_ref_t source;
        type_t type;
    };

    using id_table_t = std::vector<id_item_t>;

    struct func_item_t
    {
        runtime::vm_pc_t entry_pc;
        runtime::vm_pc_t limit_pc;
        std::string name;
        id_table_t args;
        id_table_t locals;
        type_t return_type;
    };

    using func_table_t = std::vector<func_item_t>;

    struct name_source_size_t
    {
        std::string name;
        source_ref_t source;
        int32_t size_in_bytes;
        id_table_t id_table;
        std::unique_ptr<std::vector<name_source_size_t>> tag_table;
    };

    using adt_item_t = name_source_size_t;

    struct tuple_item_t
    {
        int32_t size_in_bytes;
        id_table_t id_table;
    };

    struct type_table_t
    {
        static const uint32_t string_table_mask = 0x80000000;
        static const uint32_t inner_type_table_mask = 0x40000000;
        static const uint32_t source_table_mask = 0x20000000;
        static const uint32_t adt_table_mask = 0x10000000;
        static const uint32_t tuple_type_mask = 0x08000000;
        static const uint32_t types_mask = 0;

        std::vector<type_t> types;
        std::vector<std::string> strings_table;
        std::vector<type_t> inner_type_table;
        std::vector<source_ref_t> source_table;
        std::vector<name_source_size_t> adt_table;
        std::vector<tuple_item_t> tuple_table;

        template<typename T>
        const T &resolve_table_index(int32_t idx) const;
    };

    template<>
    const std::string &type_table_t::resolve_table_index<std::string>(int32_t idx) const
    {
        const auto idx_local = idx & ~string_table_mask;
        if (0 > idx_local || idx_local >= strings_table.size())
            throw std::out_of_range{ "Invalid string table index" };

        return strings_table[idx_local];
    }

    template<>
    const type_t &type_table_t::resolve_table_index<type_t>(int32_t idx) const
    {
        if ((static_cast<uint32_t>(idx) & inner_type_table_mask) != 0)
        {
            const auto idx_local = idx & ~inner_type_table_mask;
            if (0 > idx_local || idx_local >= inner_type_table.size())
                throw std::out_of_range{ "Invalid inner type table index" };

            return inner_type_table[idx_local];
        }
        else
        {
            if (0 > idx || static_cast<uint32_t>(idx) >= types.size())
                throw std::out_of_range{ "Invalid type table index" };

            return types[idx];
        }
    }

    template<>
    const source_ref_t &type_table_t::resolve_table_index<source_ref_t>(int32_t idx) const
    {
        const auto idx_local = idx & ~source_table_mask;
        if (0 > idx_local || idx_local >= source_table.size())
            throw std::out_of_range{ "Invalid source table index" };

        return source_table[idx_local];
    }

    template<>
    const name_source_size_t &type_table_t::resolve_table_index<name_source_size_t>(int32_t idx) const
    {
        const auto idx_local = idx & ~adt_table_mask;
        if (0 > idx_local || idx_local >= adt_table.size())
            throw std::out_of_range{ "Invalid adt table index" };

        return adt_table[idx_local];
    }

    template<>
    const tuple_item_t &type_table_t::resolve_table_index<tuple_item_t>(int32_t idx) const
    {
        const auto idx_local = idx & ~tuple_type_mask;
        if (0 > idx_local || idx_local >= tuple_table.size())
            throw std::out_of_range{ "Invalid tuple table index" };
    
        return tuple_table[idx_local];
    }

    // sbl: header file-table pc-table type-table func-table data-table
    struct sbl_file_t final
    {
        header_t header;

        file_table_t file_table;
        pc_table_t pc_table;
        type_table_t type_table;
        func_table_t func_table;
        id_table_t data_table;
    };

    // header: <magic> '\n' module '\n'
    // module: <string>
    header_t read_header(util::buffered_reader_t &r)
    {
        header_t header;
        auto v = r.get_as_string_until('\n');
        if (v.compare("limbo .sbl 2.1") == 0)
            header.version_id = sbl_version_t::sbl_2_1;
        else if (v.compare("limbo .sbl 2.0") == 0)
            header.version_id = sbl_version_t::sbl_2_0;
        else
            throw std::runtime_error{ "Unknown symbol file version" };

        header.module_name = r.get_as_string_until('\n');

        return header;
    }

    // file-table: <count> '\n' file*
    // file: <string> '\n'
    file_table_t read_file_table(util::buffered_reader_t &r)
    {
        auto count_str = r.get_as_string_until('\n');

        file_table_t file_table;
        const auto count = std::stoi(count_str);
        file_table.resize(count);
        for (auto i = 0; i < count; ++i)
        {
            auto file = r.get_as_string_until('\n');
            file_table[i] = std::move(file);
        }

        return file_table;
    }

    // src: pos ',' pos
    // pos: file':'? line'.'? char
    // file: <int>
    // line: <int>
    // char: <int>
    source_ref_t read_src_entry(const std::string &src_str, sbl_read_context_t &fc)
    {
        auto src_parts = util::split(src_str, ",");
        if (src_parts.size() != 2)
            throw std::runtime_error{ "Invalid source item" };

        source_ref_t src;

        // Initialize the source with the last state
        src.source_id = fc.last_file;
        src.begin_line = fc.last_line;
        src.end_line = fc.last_line;
        for (auto i = std::size_t{ 0 }; i < src_parts.size(); ++i)
        {
            const auto &pos = src_parts[i];
            const auto len = pos.length();

            // Parse position
            for (auto j = std::size_t{ 0 }; j < len; ++j)
            {
                auto consumed = std::size_t{ 0 };
                auto v = std::stoi(pos.data() + j, &consumed);
                j += consumed;
                if (j >= len)
                {
                    if (i == 0)
                        src.begin_column = v;

                    src.end_column = v;
                }
                else if (pos[j] == '.')
                {
                    if (i == 0)
                        src.begin_line = v;

                    src.end_line = v;
                    fc.last_line = v;
                }
                else if (pos[j] == ':')
                {
                    assert((i == 0 || src.source_id == v) && "Source position should not change across statements");
                    src.source_id = v;
                    fc.last_file = v;
                }
            }
        }

        return src;
    }

    // pc-table: <count> '\n' pc-item*
    // pc-item: src ' ' stmt '\n'
    // stmt: <int>
    pc_table_t read_pc_table(util::buffered_reader_t &r, sbl_read_context_t &fc)
    {
        auto count_str = r.get_as_string_until('\n');

        uint32_t last_source = 0;
        uint32_t last_line = 0;

        pc_table_t pc_table;
        const auto count = std::stoi(count_str);
        pc_table.resize(count);
        for (auto i = 0; i < count; ++i)
        {
            auto item = r.get_as_string_until('\n');

            auto pc_item_parts = util::split(item);
            if (pc_item_parts.size() != 2)
                throw std::runtime_error{ "Invalid PC item" };

            pc_item_t curr_item;
            curr_item.source = read_src_entry(pc_item_parts[0], fc);
            curr_item.debug_statement = std::stoi(pc_item_parts[1]);

            // Record the current source and line ID since those are the new default values
            last_source = curr_item.source.source_id;
            last_line = curr_item.source.end_line;

            pc_table[i] = std::move(curr_item);
        }

        fc.pc_count = count;
        return pc_table;
    }

    // Forward declaration
    type_t read_type(util::buffered_reader_t &, type_table_t &, sbl_read_context_t &);

    // id-table: <count> '\n' id-item*
    // id-item: id-offset ':' id-name ':' src ' ' type '\n'
    // id-offset: <int>
    // id-name: <string>
    id_table_t read_id_table(util::buffered_reader_t &r, type_table_t &tt, sbl_read_context_t &fc)
    {
        auto count_str = r.get_as_string_until('\n');
        int32_t count = std::stoi(count_str);

        bool success;
        uint8_t tmp;
        id_table_t id_table;
        id_table.resize(count);
        for (auto i = 0; i < count; ++i)
        {
            const auto offset_part = r.get_as_string_until(':');
            if (offset_part.empty())
                throw std::runtime_error{ "Invalid ID table item (offset)" };

            const auto name_part = r.get_as_string_until(':');
            if (name_part.empty())
                throw std::runtime_error{ "Invalid ID table item (name)" };

            const auto source_part = r.get_as_string_until(' ');
            if (source_part.empty())
                throw std::runtime_error{ "Invalid ID table item (source)" };

            id_item_t e;
            e.offset = std::stoul(offset_part);
            e.name = std::move(name_part);
            e.source = read_src_entry(source_part, fc);
            e.type = read_type(r, tt, fc);

            std::tie(success, tmp) = r.get_next_byte();
            if (!success || tmp != '\n')
                throw std::runtime_error{ "Invalid ID table item" };

            id_table[i] = std::move(e);
        }

        return id_table;
    }

    // adt-item: adt-name ' ' src ' ' size '\n' id-table
    // adt-name: <string>
    // size: <int>
    //
    // tag-table: <count> '\n' tag-item*
    // tag-item: tag-name ':' src ' ' size '\n' id-table
    //         | tag-name ':' src '\n'
    // tag-name: <string>
    name_source_size_t create_name_source_size(std::vector<std::string> &parts, util::buffered_reader_t &r, type_table_t &tt, sbl_read_context_t &fc)
    {
        assert(parts.size() == 2 || parts.size() == 3);
        name_source_size_t item;
        item.name = std::move(parts[0]);
        item.source = read_src_entry(parts[1], fc);

        if (parts.size() == 3)
        {
            item.size_in_bytes = std::stoi(parts[2]);
            item.id_table = read_id_table(r, tt, fc);
        }

        return item;
    }

    // type:
    //  '@' type-index '\n'       # Type table reference
    //  'a' adt-item              # ADT
    //  'p' adt-item tag-table    # ADT-Pick
    //  't' size '.' id-table     # Tuple
    //  'm' module '\n' src       # Module
    //  'F' fn-name ' ' type      # Function
    //  'A' type                  # Array
    //  'C' type                  # Channel
    //  'L' type                  # List
    //  'R' type                  # Ref
    //  'n'                       # No type
    //  'N'                       # Nil
    //  'B'                       # Big
    //  'b'                       # Byte
    //  'i'                       # Int
    //  'f'                       # Real
    //  's'                       # String
    // type-index: <int>
    type_t read_type(util::buffered_reader_t &r, type_table_t &tt, sbl_read_context_t &fc)
    {
        bool success;
        uint8_t tc_byte;
        std::tie(success, tc_byte) = r.get_next_byte();
        if (!success)
            throw std::runtime_error{ "Invalid type" };

        const type_class_t tc = static_cast<type_class_t>(tc_byte);
        type_t t{ tc, 0, 0 };
        switch (t.type_class)
        {
        case type_class_t::none:
        case type_class_t::nil:
        case type_class_t::big:
        case type_class_t::byte:
        case type_class_t::integer:
        case type_class_t::real:
        case type_class_t::string:
        case type_class_t::poly:
            break;

        case type_class_t::array:
        case type_class_t::channel:
        case type_class_t::list:
        case type_class_t::ref:
            t.value_index_1 = type_table_t::inner_type_table_mask | tt.inner_type_table.size();
            tt.inner_type_table.push_back(read_type(r, tt, fc));
            break;

        case type_class_t::type_index:
        {
            auto type_index = r.get_as_string_until('\n');
            const auto idx = std::stoi(type_index);
            t.value_index_1 = type_table_t::types_mask | idx;
            break;
        }
        case type_class_t::adt:
        case type_class_t::adt_pick:
        {
            auto adt_line = r.get_as_string_until('\n');
            auto adt_parts = util::split(adt_line);
            if (adt_parts.size() != 3)
                throw std::runtime_error{ "Invalid adt item" };

            auto adt_item = create_name_source_size(adt_parts, r, tt, fc);

            // Check if the adt has a tag table
            if (t.type_class == type_class_t::adt_pick)
            {
                adt_item.tag_table = std::make_unique<std::vector<name_source_size_t>>();
                auto tmp = r.get_as_string_until('\n');

                const auto count = std::stoi(tmp);
                for (auto i = 0; i < count; ++i)
                {
                    tmp = r.get_as_string_until('\n');
                    auto tag_parts = util::split(tmp, " :");
                    if (tag_parts.size() != 2 && tag_parts.size() != 3)
                        throw std::runtime_error{ "Invalid tag item" };

                    auto tag = create_name_source_size(tag_parts, r, tt, fc);
                    adt_item.tag_table->push_back(std::move(tag));
                }
            }

            t.value_index_1 = type_table_t::adt_table_mask | tt.adt_table.size();
            tt.adt_table.push_back(std::move(adt_item));
            break;
        }
        case type_class_t::tuple:
        {
            // Record tuple size
            t.value_index_1 = type_table_t::tuple_type_mask | tt.tuple_table.size();

            tuple_item_t tuple{};
            auto size_str = r.get_as_string_until('.');
            tuple.size_in_bytes = std::stoi(size_str);

            tuple.id_table = read_id_table(r, tt, fc);
            tt.tuple_table.push_back(std::move(tuple));
            break;
        }
        case type_class_t::module:
        {
            // Record module name
            t.value_index_1 = type_table_t::string_table_mask | tt.strings_table.size();
            tt.strings_table.push_back(r.get_as_string_until('\n'));

            auto src_str = r.get_as_string_until('\n');
            t.value_index_2 = type_table_t::source_table_mask | tt.source_table.size();
            tt.source_table.push_back(read_src_entry(src_str, fc));
            break;
        }

        case type_class_t::function:
        {
            // Record function name
            t.value_index_1 = type_table_t::string_table_mask | tt.strings_table.size();
            auto name = r.get_as_string_until(' ');
            if (name.empty())
                throw std::runtime_error{ "Invalid function type" };

            tt.strings_table.push_back(std::move(name));

            // Record return type
            t.value_index_2 = type_table_t::inner_type_table_mask | tt.inner_type_table.size();
            tt.inner_type_table.push_back(read_type(r, tt, fc));
            break;
        }

        default:
            throw std::runtime_error{ "Invalid type value" };
        }

        return t;
    }

    // type-table: <count> '\n' type*
    type_table_t read_type_table(util::buffered_reader_t &r, sbl_read_context_t &fc)
    {
        auto count_str = r.get_as_string_until('\n');

        type_table_t type_table;
        const auto count = std::stoi(count_str);
        type_table.types.resize(count);
        for (auto i = 0; i < count; ++i)
            type_table.types[i] = read_type(r, type_table, fc);

        return type_table;
    }

    // func-table: <count> '\n' fn-item*
    // fn-item: fn-pc ':' fn-name '\n' args locals return
    // fn-pc: <int>
    // fn-name: <string>
    // args: id-table
    // locals: id-table
    // return: type
    func_table_t read_func_table(util::buffered_reader_t &r, type_table_t &tt, sbl_read_context_t &fc)
    {
        auto count_str = r.get_as_string_until('\n');

        const auto last_pc = fc.pc_count - 1;
        func_table_t func_table;
        const auto count = std::stoi(count_str);
        func_table.resize(count);
        for (auto i = 0; i < count; ++i)
        {
            func_item_t func;
            func.entry_pc = std::stoi(r.get_as_string_until(':'));
            func.limit_pc = last_pc;
            func.name = std::move(r.get_as_string_until('\n'));
            func.args = read_id_table(r, tt, fc);
            func.locals = read_id_table(r, tt, fc);
            func.return_type = read_type(r, tt, fc);

            // Update the previous functions limit
            if (i > 0)
            {
                assert(func_table[i - 1].entry_pc < func.entry_pc && "Function table in sbl file is assumed to be in pc order");
                func_table[i - 1].limit_pc = func.entry_pc - 1;
            }

            func_table[i] = std::move(func);
        }

        return func_table;
    }

    // data-table: id-table
    id_table_t read_data_table(util::buffered_reader_t &r, type_table_t &tt, sbl_read_context_t &fc)
    {
        return read_id_table(r, tt, fc);
    }
}

namespace
{
    std::string to_string(const type_t &t, const type_table_t &tt)
    {
        switch (t.type_class)
        {
        case type_class_t::none: return "<none>";
        case type_class_t::nil: return "nil";
        case type_class_t::big: return "big";
        case type_class_t::byte: return "byte";
        case type_class_t::integer: return "int";
        case type_class_t::real: return "real";
        case type_class_t::string: return "string";

        case type_class_t::poly:
            assert(false && "Not fully implemented");
            return "poly<T>";

        case type_class_t::array:
            return to_string(tt.resolve_table_index<type_t>(t.value_index_1), tt) + "[]";
        case type_class_t::channel:
            return "chan of " + to_string(tt.resolve_table_index<type_t>(t.value_index_1), tt);
        case type_class_t::list:
            return "list of " + to_string(tt.resolve_table_index<type_t>(t.value_index_1), tt);
        case type_class_t::ref:
            return "ref " + to_string(tt.resolve_table_index<type_t>(t.value_index_1), tt);
        case type_class_t::type_index:
            return to_string(tt.resolve_table_index<type_t>(t.value_index_1), tt);

        case type_class_t::adt:
        case type_class_t::adt_pick:
        {
            const auto &adt = tt.resolve_table_index<name_source_size_t>(t.value_index_1);
            return adt.name;
        }
        case type_class_t::tuple:
        {
            const auto &tuple = tt.resolve_table_index<tuple_item_t>(t.value_index_1);

            std::stringstream ss;
            ss << '(';

            auto first = true;
            for (const auto &t_t : tuple.id_table)
            {
                if (!first)
                    ss << ',';

                ss << to_string(t_t.type, tt);
                first = false;
            }

            ss << ')';
            return ss.str();
        }
        case type_class_t::module:
            return tt.resolve_table_index<std::string>(t.value_index_1);

        case type_class_t::function:
        {
            std::stringstream ss;
            ss << tt.resolve_table_index<std::string>(t.value_index_1)
                << ' '
                << to_string(tt.resolve_table_index<type_t>(t.value_index_2), tt);

            return ss.str();
        }

        default:
            throw std::runtime_error{ "Invalid type value" };
        }
    }

    std::string to_string(const func_item_t &f, const type_table_t &tt, function_name_format_t fmt)
    {
        std::stringstream ss;

        if (util::has_flag(fmt, function_name_format_t::name))
            ss << f.name;

        auto has_argument = false;
        if (util::has_flag(fmt, function_name_format_t::arguments))
        {
            has_argument = true;
            ss << '(';

            auto first = true;
            for (const auto &at : f.args)
            {
                if (!first)
                    ss << ',';

                ss << to_string(at.type, tt);
                first = false;
            }

            ss << ')';
        }

        if (util::has_flag(fmt, function_name_format_t::return_type))
        {
            if (has_argument)
                ss << ':';

            ss << to_string(f.return_type, tt);
        }

        return ss.str();
    }

    class sbl_symbol_debug final : public symbol_debug_t
    {
    public:
        sbl_symbol_debug(std::shared_ptr<sbl_file_t> f)
            : _current_pc{ 0 }
            , _file{ f }
        {
            assert(_file != nullptr);
        }

    public: // symbol_debug_t
        void set_current_pc(disvm::runtime::vm_pc_t pc) override
        {
            assert(0 <= pc && pc < static_cast<int32_t>(_file->pc_table.size()));
            _current_pc = pc;
        }

        bool try_advance_pc(advance_pc_t adv, disvm::runtime::vm_pc_t *pc_after_advance) override
        {
            runtime::vm_pc_t new_pc = _current_pc;
            switch (adv)
            {
            case advance_pc_t::next_debug_statement:
            {
                auto iter = std::next(std::cbegin(_file->pc_table), new_pc);
                const auto current_dbg = iter->debug_statement;

                // Iterate until the next pc entry that is not the same debug statement or the end.
                while (iter != std::cend(_file->pc_table) && iter->debug_statement == current_dbg)
                {
                    ++iter;
                }

                if (iter == std::cend(_file->pc_table))
                    return false;

                new_pc = std::distance(std::cbegin(_file->pc_table), iter);
                break;
            }

            case advance_pc_t::next_pc:
                if (new_pc + 1 >= static_cast<runtime::vm_pc_t>(_file->pc_table.size()))
                    return false;
                ++new_pc;
                break;

            case advance_pc_t::current:
                assert(pc_after_advance != nullptr);
                break;

            default:
                throw std::runtime_error{ "Invalid advance value" };
            }

            if (pc_after_advance != nullptr)
                *pc_after_advance = new_pc;

            _current_pc = new_pc;
            return true;
        }

        std::string current_function_name(function_name_format_t f) const override
        {
            try
            {
                for (auto &e : _file->func_table)
                {
                    if (e.entry_pc <= _current_pc && _current_pc <= e.limit_pc)
                        return to_string(e, _file->type_table, f);
                }
            }
            catch (const std::exception &e)
            {
                // Avoid crashing during debugging
                assert(false && "The SBL file appears to be corrupt or inconsistent");
                throw std::runtime_error{ e.what() };
            }

            throw std::runtime_error{ "Unknown function" };
        }

        source_ref_t current_source_location() const override
        {
            assert(0 <= _current_pc && _current_pc < static_cast<int32_t>(_file->pc_table.size()));
            return _file->pc_table[_current_pc].source;
        }

        const std::string &get_source_by_id(int32_t source_id) const override
        {
            assert(0 <= source_id && source_id < static_cast<int32_t>(_file->file_table.size()));
            return _file->file_table[source_id];
        }

    private:
        runtime::vm_pc_t _current_pc;
        std::shared_ptr<sbl_file_t> _file;
    };

    class sbl_symbol_data final : public symbol_data_t
    {
    public:
        sbl_symbol_data(std::shared_ptr<sbl_file_t> f)
            : _file{ f }
        {
            assert(_file != nullptr);
        }

    public: // symbol_data_t
        const std::string & get_module_name() const override
        {
            return _file->header.module_name;
        }

        size_t get_instruction_count() const
        {
            return _file->pc_table.size();
        }

        std::unique_ptr<symbol_debug_t> get_debug() override
        {
            return std::make_unique<sbl_symbol_debug>(_file);
        }

    private:
        std::shared_ptr<sbl_file_t> _file;
    };
}

std::unique_ptr<symbol_data_t> disvm::symbol::read(std::istream &f)
{
    auto file = std::make_shared<sbl_file_t>();

    util::buffered_reader_t reader{ f };

    file->header = read_header(reader);
    file->file_table = read_file_table(reader);

    sbl_read_context_t fc{};
    file->pc_table = read_pc_table(reader, fc);
    file->type_table = read_type_table(reader, fc);
    file->func_table = read_func_table(reader, file->type_table, fc);
    file->data_table = read_data_table(reader, file->type_table, fc);

    return std::make_unique<sbl_symbol_data>(file);
}
