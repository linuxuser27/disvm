//
// Dis VM
// File: buffered_reader.h
// Author: arr
//

#ifndef _DISVM_SRC_INCLUDE_BUFFERED_READER_H_
#define _DISVM_SRC_INCLUDE_BUFFERED_READER_H_

#include <cstdint>
#include <cassert>
#include <vector>
#include <sstream>
#include <tuple>
#include <iosfwd>

namespace disvm
{
    namespace util
    {
        class buffered_reader_t
        {
        public:
            buffered_reader_t(std::istream &stream)
                : _buffer(64 * 1024) // 64k is an optimization for the Windows filesystem manager
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

                auto bytes_requested_acc = uint32_t{ 0 };

                for (;;)
                {
                    const auto amount_in_buffer = check_buffer_content();
                    if (amount_in_buffer == 0)
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

            // Read from the stream as a string until the supplied delimiter is encountered.
            std::string get_as_string_until(const char delim)
            {
                std::stringstream ss;
                auto amount_in_buffer = check_buffer_content();
                while (amount_in_buffer != 0)
                {
                    while (_current_index < _current_size)
                    {
                        auto c = _buffer[_current_index++];
                        if (c == delim)
                            return ss.str();

                        ss << c;
                    }

                    amount_in_buffer = check_buffer_content();
                }

                return{};
            }

            // Read from the stream until the supplied value is encountered.
            bool get_bytes_until(const uint8_t delim, std::vector<uint8_t> &byte_buffer)
            {
                auto amount_in_buffer = check_buffer_content();
                while (amount_in_buffer != 0)
                {
                    while(_current_index < _current_size)
                    {
                        auto b = _buffer[_current_index++];
                        if (b == delim)
                            return true;

                        byte_buffer.push_back(b);
                    }

                    amount_in_buffer = check_buffer_content();
                }

                return false;
            }

            // Get the next byte in the buffer.
            // Returns success and the byte.
            std::tuple<bool, uint8_t> get_next_byte()
            {
                const auto amount_in_buffer = check_buffer_content();
                if (amount_in_buffer == 0)
                    return std::make_tuple(false, 0);

                return std::make_tuple(true, _buffer[_current_index++]);
            }

        private:
            std::size_t check_buffer_content()
            {
                if (_current_size <= _current_index)
                {
                    _current_index = 0;
                    _current_size = 0;

                    if (_stream.fail())
                        return 0;

                    _stream.read(_buffer.data(), _buffer.size());

                    // If the stream has failed, set the current size, otherwise the buffer size
                    _current_size = _stream.fail() ? static_cast<std::size_t>(_stream.gcount()) : _buffer.size();
                }

                return _current_size - _current_index;
            }

        private:
            std::istream &_stream;
            std::size_t _current_index;
            std::size_t _current_size;
            std::vector<char> _buffer;
        };
    }
}

#endif // _DISVM_SRC_INCLUDE_BUFFERED_READER_H_
