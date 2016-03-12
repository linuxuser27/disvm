//
// Dis VM
// File: runtime.h
// Author: arr
//

#ifndef _DISVM_SRC_INCLUDE_RUNTIME_H_
#define _DISVM_SRC_INCLUDE_RUNTIME_H_

#include <cstdint>
#include <cassert>
#include <atomic>
#include <vector>
#include <string>
#include <memory>
#include <deque>
#include <unordered_map>
#include <mutex>
#include <functional>
#include "opcodes.h"

namespace disvm
{
    // VM forward declation
    class vm_t;

    namespace runtime
    {
        //
        // VM primitive data type definitions
        //

        using byte_t = uint8_t;
        using short_word_t = int16_t;
        using word_t = int32_t;
        using short_real_t = float;
        using real_t = double;
        using big_t = int64_t;
        using pointer_t = std::size_t *;
        using vm_pc_t = word_t;

        //
        // VM memory management
        //

        // VM forward declation
        class vm_alloc_t;

        // Finalizer callback for allocations.
        using vm_alloc_instance_finalizer_t = void(*)(vm_alloc_t *);

        // VM type description
        class type_descriptor_t final
        {
        public: // static
            static vm_alloc_instance_finalizer_t no_finalizer;

            // Create a type descriptor for type of the supplied size and pointers based on the supplied map.
            static std::shared_ptr<const type_descriptor_t> create(const word_t size);
            static std::shared_ptr<const type_descriptor_t> create(const word_t size, const std::vector<byte_t> &pointer_map);
            static std::shared_ptr<const type_descriptor_t> create(
                const word_t size,
                const word_t pointer_map_length,
                const byte_t *pointer_map,
                vm_alloc_instance_finalizer_t finalizer = type_descriptor_t::no_finalizer);

        private:
            type_descriptor_t(word_t size_in_bytes, word_t map_in_bytes, const byte_t * pointer_map, vm_alloc_instance_finalizer_t finalizer);

        public:
            type_descriptor_t(const type_descriptor_t&) = delete;
            type_descriptor_t& operator=(const type_descriptor_t&) = delete;

            const word_t size_in_bytes;
            const word_t map_in_bytes;
            const byte_t * const pointer_map;
            const vm_alloc_instance_finalizer_t finalizer;
        };

        // Access to type descriptors for VM intrinsic types.
        class intrinsic_type_desc final
        {
        private:
            friend vm_t;
            static std::atomic_bool is_initialized;
            static void initialize();

        public:
            // Templated type descriptor getter
            template<typename IntrinsicType>
            static std::shared_ptr<const type_descriptor_t> type();
        };

        // Type used to indicate allocated memory should be zeroed out.
        struct zero_memory_t
        { };

        // VM memory allocation
        class vm_alloc_t
        {
        public: // static
            static const zero_memory_t zero_memory;

            static void *operator new(std::size_t sz);
            static void operator delete(void *ptr);

            static vm_alloc_t *allocate(std::shared_ptr<const type_descriptor_t> td);
            static vm_alloc_t *allocate(std::shared_ptr<const type_descriptor_t> td, const zero_memory_t &);
            static vm_alloc_t *copy(const vm_alloc_t &other);
            static vm_alloc_t *from_allocation(pointer_t allocation);

            template <typename T>
            static T *from_allocation(pointer_t allocation)
            {
                auto alloc_inst = vm_alloc_t::from_allocation(allocation);
                assert(alloc_inst == nullptr || alloc_inst->alloc_type == intrinsic_type_desc::type<T>());
                return dynamic_cast<T *>(alloc_inst);
            }

        protected:
            vm_alloc_t(std::shared_ptr<const type_descriptor_t> td);

        public:
            virtual ~vm_alloc_t();

            std::size_t add_ref();
            std::size_t release();
            std::size_t get_ref_count() const;

            std::shared_ptr<const type_descriptor_t> alloc_type;

            // Reserved for use by the garbage collector.
            // This should not be accessed by any other component.
            pointer_t gc_reserved;

        public:
            pointer_t allocation() const;

        private:
            std::atomic<std::size_t> _ref_count;
        };

        //
        // VM intrinsic type definitions
        //

        // Type to contain the maximum UTF-8 character value
        using rune_t = uint32_t;

        // VM string type
        class vm_string_t final : public vm_alloc_t
        {
        public: //static
            static std::shared_ptr<const type_descriptor_t> type_desc();

            // Compare two string objects
            static int compare(const vm_string_t *s1, const vm_string_t *s2);

        public:
            vm_string_t();
            vm_string_t(std::size_t encoded_str_len, const uint8_t *encoded_str);
            vm_string_t(const vm_string_t &s1, const vm_string_t &s2);
            vm_string_t(const vm_string_t &other, word_t begin_index, word_t end_index);
            vm_string_t(const vm_string_t &) = delete;
            ~vm_string_t();

            // Returns this string.
            vm_string_t &append(const vm_string_t &other);

            word_t get_length() const;

            rune_t get_rune(word_t index) const;

            void set_rune(word_t index, rune_t value);

            int compare_to(const vm_string_t *s) const;

            const char *str() const;

        private:
            word_t _length;
            word_t _length_max;
            mutable std::mutex _encoded_str_lock;
            mutable char *_encoded_str;
            mutable union string_memory_t
            {
                uint8_t local[16]; // Optimization for short strings
                uint8_t *alloc;
            } _mem;

            uint8_t _character_size; // 1 byte (ASCII) or 4 bytes (multi-byte)
            bool _is_alloc;
        };

        // VM array type
        class vm_array_t final : public vm_alloc_t
        {
        public: //static
            static std::shared_ptr<const type_descriptor_t> type_desc();

        public:
            vm_array_t(std::shared_ptr<const type_descriptor_t> td, word_t count);
            vm_array_t(std::shared_ptr<const type_descriptor_t> td, word_t count, const zero_memory_t&);
            vm_array_t(vm_array_t *original, word_t begin_index, word_t count);
            vm_array_t(const vm_string_t *s);
            ~vm_array_t();

            std::shared_ptr<const type_descriptor_t> get_element_type() const;

            word_t get_length() const;

            pointer_t at(word_t index) const;

            template<typename T>
            T& at(word_t index) const
            {
                return *reinterpret_cast<T *>(at(index));
            }

            // Copy the contents of the source array into this array starting at index
            void copy_from(const vm_array_t &source, word_t this_begin_index);

        private:
            std::shared_ptr<const type_descriptor_t> _element_type;
            vm_array_t *_original;
            word_t _length;
            byte_t *_arr;
        };

        // VM list type
        class vm_list_t final : public vm_alloc_t
        {
        public: //static
            static std::shared_ptr<const type_descriptor_t> type_desc();

        public:
            vm_list_t(std::shared_ptr<const type_descriptor_t> td, vm_list_t *tail = nullptr);
            ~vm_list_t();

            std::shared_ptr<const type_descriptor_t> get_element_type() const;

            word_t get_length() const;

            vm_list_t* get_tail() const;

            vm_list_t* drop_tail();

            // Returns a pointer to the value
            pointer_t value() const;

        private:
            std::shared_ptr<const type_descriptor_t> _element_type;
            vm_list_t *_tail;

            // Utilizing a union so that all value types can 
            // be stored without allocating any extra memory.
            mutable union element_memory_t
            {
                pointer_t alloc;
                uint64_t local;
            } _mem;

            const bool _is_alloc;
        };

        // VM forward declation
        class vm_channel_t;

        // VM request mutex
        struct vm_request_mutex_t
        {
            vm_request_mutex_t();

            std::mutex pending_request_lock;
            bool pending_request;
        };

        // VM channel request
        class vm_channel_request_t final
        {
        public:
            // Create a request using the ID of the requesting thread and an associated request mutex.
            vm_channel_request_t(uint32_t thread_id, vm_request_mutex_t &request_mutex);

            uint32_t thread_id;

            // Callback used to indicate the request was handled in the supplied channel.
            // This is only called in the event the request was not immediately serviced.
            std::function<void(const vm_channel_t &)> request_handled_callback;

            // Data of the request
            pointer_t data;

            std::reference_wrapper<vm_request_mutex_t> request_mutex;
        };

        // VM channel type
        class vm_channel_t final : public vm_alloc_t
        {
        public: //static
            static std::shared_ptr<const type_descriptor_t> type_desc();

        public:
            vm_channel_t(
                std::shared_ptr<const type_descriptor_t> td,
                std::function<void(pointer_t, pointer_t, const type_descriptor_t *)> transfer,
                word_t buffer_len);
            ~vm_channel_t();

            std::shared_ptr<const type_descriptor_t> get_data_type() const;

            // Send data into the channel.
            // Returns true if the data was sent, otherwise request is queued and false returned.
            bool send_data(vm_channel_request_t &send_request);

            // Receive data from the channel.
            // Returns true if data was received, otherwise request is queued and false returned.
            bool receive_data(vm_channel_request_t &receive_request);

            // Cancel an outstanding request from a thread.
            void cancel_request(uint32_t thread_id);

        private:
            // Try to send data into the channel buffer.
            // Returns true if data was sent to buffer, otherwise false.
            bool try_send_to_buffer(vm_channel_request_t &send_request);

            // Try to receive data from the channel buffer.
            // Returns true if data was received from buffer, otherwise false.
            bool try_receive_from_buffer(vm_channel_request_t &receive_request);

        private:
            std::mutex _data_lock;
            std::shared_ptr<const type_descriptor_t> _data_type;
            std::function<void(pointer_t, pointer_t, const type_descriptor_t *)> _data_transfer;

            vm_array_t *_data_buffer;
            uint32_t _data_buffer_head; // Next element to remove from buffer
            uint32_t _data_buffer_count; // Count of elements in buffer

            using request_queue_t = std::deque<vm_channel_request_t>;
            request_queue_t _data_senders;
            request_queue_t _data_receivers;
        };

        // Alt channel entry.
        static_assert(sizeof(word_t) == sizeof(pointer_t), "Alt channel stack layout will be invalid.");
        struct vm_alt_channel_stack_layout_t
        {
            word_t alloc;
            word_t data;
        };

        // Alt channel layout as defined by the specification for the 'alt' instruction.
        struct vm_alt_stack_layout_t
        {
            word_t send_count;
            word_t receive_count;
            vm_alt_channel_stack_layout_t channels[1];
        };

        // Runtime constants
        struct runtime_constants
        {
            static const auto nil = std::size_t{ 0 };

            static const auto invalid_program_counter = vm_pc_t{ ~0 };

            static const auto default_register_count = std::size_t{ 4 };
            static const auto default_register_size_bytes = sizeof(word_t);

            // See VM frame definition
            static const auto fixed_point_register_1_frame_offset = std::size_t{ default_register_count * default_register_size_bytes };
            static const auto fixed_point_register_2_frame_offset = fixed_point_register_1_frame_offset + (2 * default_register_size_bytes);
        };

        // Addressing mode for middle instruction data
        enum class address_mode_middle_t
        {
            none = 0,
            small_immediate = 1,
            small_offset_indirect_fp = 2,
            small_offset_indirect_mp = 3,
        };

        // Middle instruction data
        struct middle_data_t
        {
            address_mode_middle_t mode;
            word_t register1;
        };

        // Addressing mode for source and destination instruction data
        enum class address_mode_t
        {
            offset_indirect_mp = 0,
            offset_indirect_fp = 1,
            immediate = 2,
            none = 3,
            offset_double_indirect_mp = 4,
            offset_double_indirect_fp = 5,
            reserved_1 = 6,
            reserved_2 = 7,
        };

        // Source and destination instruction data
        struct inst_data_generic_t
        {
            address_mode_t mode;
            word_t register1;
            word_t register2;
        };

        using src_data_t = inst_data_generic_t;
        using dest_data_t = inst_data_generic_t;

        // VM forward declation
        class vm_registers_t;

        // Instruction execution operation
        using vm_exec_t = void(*)(vm_registers_t &, vm_t &);

        // Opcode execution operation
        struct vm_exec_op_t
        {
            opcode_t opcode;
            src_data_t source;
            middle_data_t middle;
            dest_data_t destination;
        };

        // VM Instruction (code section of module)
        union vm_instruction_t
        {
            vm_exec_op_t op;
            vm_exec_t native;
        };

        // Flags that dictate the runtime operations of the module
        // [SPEC] Values past 'shared_module' are not fully documented.
        enum class runtime_flags_t
        {
            must_compile = 1 << 0,
            dont_compile = 1 << 1,
            share_module = 1 << 2, // [TODO] Respect this setting
            reserved_1 = 1 << 3, // DYNMOD
            has_import_deprecated = 1 << 4,
            has_handler = 1 << 5,
            has_import = 1 << 6,

            builtin = 1 << 7,
        };

        // Module header
        struct header_t
        {
            word_t magic_number;
            struct
            {
                word_t length;
                std::unique_ptr<byte_t[]> signature;
            } Signature;

            runtime_flags_t runtime_flag;
            word_t stack_extent;
            word_t code_size;
            word_t data_size;
            word_t type_size;
            word_t export_size;
            vm_pc_t entry_pc;
            word_t entry_type; // Stack frame type
        };

        // Export function entry
        struct export_function_t
        {
            export_function_t() = default;
            export_function_t(export_function_t &&);

            export_function_t(const export_function_t &) = delete;
            export_function_t &operator=(const export_function_t &) = delete;

            ~export_function_t();

            vm_pc_t pc;
            word_t frame_type;
            word_t sig;
            std::unique_ptr<vm_string_t> name;
        };

        // Import function entry
        struct import_function_t
        {
            import_function_t() = default;
            import_function_t(import_function_t &&);

            import_function_t(const import_function_t &) = delete;
            import_function_t &operator=(const import_function_t &) = delete;

            ~import_function_t();

            word_t sig;
            std::unique_ptr<vm_string_t> name;
        };

        // Import table for module
        struct import_vm_module_t
        {
            import_vm_module_t() = default;
            import_vm_module_t(import_vm_module_t &&);

            import_vm_module_t(const import_vm_module_t &) = delete;
            import_vm_module_t &operator=(const import_vm_module_t &) = delete;

            ~import_vm_module_t() = default;

            std::vector<import_function_t> functions;
        };

        // Exception entry for handler
        struct exception_t
        {
            exception_t() = default;
            exception_t(exception_t &&);

            exception_t(const exception_t &) = delete;
            exception_t &operator=(const exception_t &) = delete;

            ~exception_t();

            vm_pc_t pc;
            std::unique_ptr<vm_string_t> name;
        };

        // Exception handler
        struct handler_t
        {
            handler_t() = default;
            handler_t(handler_t &&);

            handler_t(const handler_t &) = delete;
            handler_t &operator=(const handler_t &) = delete;

            ~handler_t() = default;

            // Byte offset in the handling frame the exception should be placed.
            word_t exception_offset;
            vm_pc_t begin_pc;
            vm_pc_t end_pc;

            // Number of cases in exception table where the caught exception is of exception type.
            // See exceptions in Limbo language for details.
            word_t exception_type_count;

            std::shared_ptr<const type_descriptor_t> type_desc;
            std::vector<exception_t> exception_table;
        };

        using code_section_t = std::vector<vm_instruction_t>;
        using type_section_map_t = std::vector<std::shared_ptr<const type_descriptor_t>>;
        using export_section_t = std::unordered_multimap<word_t, const export_function_t>;
        using import_section_t = std::vector<import_vm_module_t>;
        using handler_section_t = std::vector<handler_t>;

        // VM module
        struct vm_module_t
        {
            vm_module_t() = default;
            vm_module_t(const vm_module_t &) = delete;
            vm_module_t &operator=(const vm_module_t &) = delete;

            ~vm_module_t();

            // Contains a magic number, digital signature, runtime flags, sizes of the other sections, and a description of the entry point
            header_t header;

            // Describes a sequence of instructions for the virtual machine
            code_section_t code_section;

            // template module pointer
            std::unique_ptr<vm_alloc_t> original_mp;

            // Type descriptors describing the layout of pointers within data types
            type_section_map_t type_section;

            // Module name
            std::unique_ptr<vm_string_t> module_name;

            // List of functions exported by this module
            export_section_t export_section;

            // List of all functions imported from other modules
            import_section_t import_section;

            // Lists all exception handlers declared in the module
            handler_section_t handler_section;
        };

        // Reference to a function in an imported module.
        struct vm_module_function_ref_t
        {
            vm_pc_t entry_pc;
            word_t frame_type;
        };

        // Reference to a module
        class vm_module_ref_t final : public vm_alloc_t
        {
        public: //static
            static std::shared_ptr<const type_descriptor_t> type_desc();

        public:
            vm_module_ref_t(std::shared_ptr<const vm_module_t> module);
            vm_module_ref_t(std::shared_ptr<const vm_module_t> module, const import_vm_module_t &imports);
            ~vm_module_ref_t();

            std::shared_ptr<const vm_module_t> module;
            vm_alloc_t *mp_base;
            const code_section_t &code_section;
            const type_section_map_t &type_section;

            bool is_builtin_module() const;
            const vm_module_function_ref_t& get_function_ref(word_t index) const;

        private:
            const bool _builtin_module;
            std::unique_ptr<vm_array_t> _function_refs;
        };

        // VM stack frame
        class vm_frame_t final
        {
        public:
            vm_frame_t(std::shared_ptr<const type_descriptor_t> &frame_type);
            ~vm_frame_t();

            std::shared_ptr<const type_descriptor_t> frame_type;

            // Copy the frame contents
            void copy_frame_contents(const vm_frame_t &other);

            // Pointer to the base of the frame.
            pointer_t base() const;

            template<typename T>
            T& base() const
            {
                return *reinterpret_cast<T *>(base());
            }

            vm_pc_t &prev_pc() const;
            vm_frame_t *&prev_frame() const;

            // This is only set if this frame represents a module transition.
            vm_module_ref_t *&prev_module_ref() const;

            // Fixed point register positions in frame
            // [SPEC] These are not defined in the vm spec.

            // 'Stmp' in libinterp/xec.c
            pointer_t fixed_point_register_1() const;

            // 'Dtmp' in libinterp/xec.c
            pointer_t fixed_point_register_2() const;
        };

        // Callback for stack walk.
        // Returning 'false' during the stack walk will terminate the walk.
        // The second parameter is reserved for internal use by the VM and should not be manipulated.
        using vm_stack_walk_callback_t = std::function<bool(const vm_frame_t *f, const void *reserved)>;

        // VM stack
        class vm_stack_t final
        {
        public:
            vm_stack_t(std::size_t stack_extent);
            ~vm_stack_t();

            // Allocate a new frame instance.
            vm_frame_t *alloc_frame(std::shared_ptr<const type_descriptor_t> frame_type);

            // Push the latest allocated frame as the new top frame.
            vm_frame_t *push_frame();

            // Remove the top frame and return the next frame in the stack.
            vm_frame_t *pop_frame();

            // Return the current top frame.
            vm_frame_t *peek_frame() const;

            // Walk the stack getting a callback for each frame.
            // This function is __not__ thread safe.
            void walk_stack(vm_stack_walk_callback_t callback) const;

        private:
            std::unique_ptr<vm_alloc_t> _mem;
        };

        // Walk the supplied stack and faulting module find the handler, handling stack frame, and program counter for the supplied exception ID.
        // If an exception handler cannot be found an 'unhandled_user_exception' exception will be thrown.
        std::tuple<const handler_t *, vm_frame_t *, vm_pc_t> find_exception_handler(
            const vm_stack_t &stack,
            const vm_module_ref_t &faulting_module,
            const vm_pc_t faulting_pc,
            const vm_string_t &exception_id);

        // VM thread states
        enum class vm_thread_state_t
        {
            unknown = 0,

            blocked_in_alt,     // blocked in alt instruction
            blocked_sending,    // blocked waiting to send
            blocked_receiving,  // blocked waiting to receive

            debug,    // thread is running under a debugger
            ready,    // ready to run
            running,

            release,  // interpreter released - executing non-vm instructions

            exiting,  // exit instruction
            empty_stack,

            broken,   // thread crashed
        };

        // Forward declaration
        class vm_thread_t;

        // VM registers
        class vm_registers_t
        {
        public:
            vm_registers_t(
                const vm_thread_t &thread,
                vm_module_ref_t &entry);
            ~vm_registers_t();

            const vm_thread_t &thread;
            vm_stack_t stack;  // Frame pointer access (FP)
            vm_pc_t pc;  // Program counter (Index into module code section)
            vm_alloc_t *mp_base;  // Module data base pointer (MP)
            vm_module_ref_t *module_ref;  // Module reference
            vm_request_mutex_t request_mutex;
            vm_thread_state_t current_thread_state;
            pointer_t src;
            pointer_t mid;
            pointer_t dest;
        };

        // VM thread
        class vm_thread_t final : public vm_alloc_t
        {
        public: //static
            static std::shared_ptr<const type_descriptor_t> type_desc();

        public:
            vm_thread_t(
                vm_module_ref_t &entry,
                uint32_t parent_thread_id);
            vm_thread_t(
                vm_module_ref_t &entry,
                const vm_frame_t &initial_frame,
                vm_pc_t start_pc,
                uint32_t parent_thread_id);

            ~vm_thread_t();

            vm_thread_state_t execute(vm_t &virtual_machine, uint32_t quanta);

            vm_thread_state_t get_state() const;

            uint32_t get_thread_id() const;

            uint32_t get_parent_thread_id() const;

            const vm_registers_t& get_registers() const;

        private:
            const uint32_t _thread_id;
            const uint32_t _parent_thread_id;
            vm_registers_t _registers;
        };

        //
        // VM interfaces
        //

        // VM thread scheduler control interface
        class vm_scheduler_control_t
        {
        public:
            virtual ~vm_scheduler_control_t() = 0;

            // Indicates the scheduler should enqueue the blocked thread with the supplied ID.
            // There is no indication if this function failed or succeeded.
            virtual void enqueue_blocked_thread(uint32_t thread_id) = 0;

            // Get all VM threads
            virtual std::vector<std::shared_ptr<const vm_thread_t>> get_all_threads() const = 0;
        };

        // VM thread scheduler interface
        class vm_scheduler_t
        {
        public:
            virtual ~vm_scheduler_t() = 0;

            // Returns true if there are no vm threads executing, scheduled to be executed, or blocked.
            virtual bool is_idle() const = 0;

            // Get controller
            virtual vm_scheduler_control_t &get_controller() const = 0;

            // Schedule the supplied thread for execution
            virtual const vm_thread_t& schedule_thread(std::unique_ptr<vm_thread_t> thread) = 0;
        };

        // VM garbage collector interface
        class vm_garbage_collector_t
        {
        public:
            // The supplied allocation is guaranteed to be valid for the lifetime of the callback.
            using vm_alloc_callback_t = std::function<void(const vm_alloc_t *)>;

        public:
            virtual ~vm_garbage_collector_t() = 0;

            // Supply an allocation for the collector to track.
            // Note that it is assumed the collector implementation will increment the reference count on the supplied alloc.
            virtual void track_allocation(vm_alloc_t *alloc) = 0;

            // Enumerate allocations that are being tracked by the collector.
            // See callback type description.
            virtual void enum_tracked_allocations(vm_alloc_callback_t callback) const = 0;

            // Called by the scheduler to inform the collector it should attempt to free resources.
            // All VM threads are guaranteed to be idle when called.
            // Returns true if the collection algorithm was run, otherwise false.
            virtual bool collect(std::vector<std::shared_ptr<const vm_thread_t>> &threads) = 0;
        };
    }
}

#endif // _DISVM_SRC_INCLUDE_RUNTIME_H_