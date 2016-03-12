//
// Dis VM
// File: buffered_reader.h
// Author: arr
//

#ifndef _DISVM_SRC_VM_BUFFERED_READER_H_
#define _DISVM_SRC_VM_BUFFERED_READER_H_

#include <cstdint>
#include <cassert>
#include <vector>
#include <tuple>
#include <iosfwd>

namespace disvm
{
    namespace util
    {
        class buffered_reader_t
        {
        private: // static
            static const std::size_t buffer_size = 64 * 1024; // 64k is an optimization for the Windows filesystem manager

        public:
            buffered_reader_t(std::istream &stream)
                : _buffer(buffered_reader_t::buffer_size)
                , _current_index{ 0 }
                , _current_size{ 0 }
                , _stream{ stream }
            {
                assert(!stream.fail());
            }

            buffered_reader_t(const buffered_reader_t&) = delete;
            buffered_reader_t& operator=(const buffered_reader_t&) = delete;

            // Get the next 'n' number of bytes.
            // Returns the number of bytes retrieved.
            uint32_t get_next_bytes(uint32_t bytes_requested, uint8_t *buffer)
            {
                if (bytes_requested == 0)
                    return 0;

                assert(buffer != nullptr);

                auto bytes_requested_acc = uint32_t{0};
                std::size_t amount_in_buffer;

                for (;;)
                {
                    bool success = check_buffer_content(amount_in_buffer);
                    if (!success || amount_in_buffer == 0)
                        return 0;

                    // Buffer contains requested amount
                    if (bytes_requested <= amount_in_buffer)
                    {
                        std::memcpy(buffer, (_buffer.data() + _current_index), bytes_requested);
                        _current_index += bytes_requested;
                        bytes_requested_acc += bytes_requested;
                        return bytes_requested_acc;
                    }
                    else
                    {
                        // Buffer does not contain all the data requested
                        std::memcpy(buffer, (_buffer.data() + _current_index), amount_in_buffer);
                        _current_index += amount_in_buffer;

                        // Subtract the bytes consumed
                        bytes_requested -= amount_in_buffer;

                        bytes_requested_acc += amount_in_buffer;
                        buffer += amount_in_buffer;
                    }
                }
            }

            // Get the next byte in the buffer.
            // Returns success and the byte.
            std::tuple<bool, uint8_t> get_next_byte()
            {
                std::size_t amount_in_buffer;
                bool success = check_buffer_content(amount_in_buffer);
                if (!success || amount_in_buffer == 0)
                    return std::make_tuple(false, 0);

                return std::make_tuple(true, _buffer[_current_index++]);
            }

        private:
            bool check_buffer_content(std::size_t &amount_in_buffer)
            {
                // Signed so checking for underflow is easier.
                amount_in_buffer = 0;
                auto amount_in_buffer_local = _current_size - _current_index;
                if (amount_in_buffer_local <= 0)
                {
                    _current_index = 0;
                    _current_size = 0;

                    if (_stream.fail())
                        return false;

                    _stream.read(_buffer.data(), buffered_reader_t::buffer_size);
                    _current_size = _stream.fail() ? static_cast<std::size_t>(_stream.gcount()) : buffered_reader_t::buffer_size;
                    
                    // Reset amount in buffer
                    amount_in_buffer_local = buffered_reader_t::buffer_size;
                }

                amount_in_buffer = amount_in_buffer_local;
                return true;
            }

        private:
            std::istream &_stream;
            std::size_t _current_index;
            std::size_t _current_size;
            std::vector<char> _buffer;
        };
    }
}

#endif // _DISVM_SRC_VM_BUFFERED_READER_H_