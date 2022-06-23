//
// Dis VM
// File: channel.cpp
// Author: arr
//

#include <runtime.hpp>
#include <vm_memory.hpp>
#include <debug.hpp>
#include <exceptions.hpp>

using disvm::debug::component_trace_t;
using disvm::debug::log_level_t;

using disvm::runtime::managed_ptr_t;
using disvm::runtime::intrinsic_type_desc;
using disvm::runtime::type_descriptor_t;
using disvm::runtime::vm_alloc_t;
using disvm::runtime::vm_array_t;
using disvm::runtime::vm_channel_t;
using disvm::runtime::vm_channel_request_t;
using disvm::runtime::vm_request_mutex_t;
using disvm::runtime::word_t;

vm_request_mutex_t::vm_request_mutex_t()
    : pending_request{ false }
{ }

vm_channel_request_t::vm_channel_request_t(uint32_t thread_id, vm_request_mutex_t &request_mutex)
    : data{ nullptr }
    , request_mutex{ request_mutex }
    , thread_id{ thread_id }
{ }

managed_ptr_t<const type_descriptor_t> vm_channel_t::type_desc()
{
    return intrinsic_type_desc::type<vm_channel_t>();
}

vm_channel_t::vm_channel_t(
    managed_ptr_t<const type_descriptor_t> td,
    data_transfer_func_t transfer,
    word_t buffer_len)
    : vm_alloc_t(vm_channel_t::type_desc())
    , _data_buffer{ nullptr }
    , _data_buffer_head{ 0 }
    , _data_buffer_count{ 0 }
    , _data_transfer{ transfer }
    , _data_type{ std::move(td) }
{
    if (buffer_len < 0)
        throw out_of_range_memory{};

    if (buffer_len > 0)
        _data_buffer = new vm_array_t{ _data_type, buffer_len };

    disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "init: vm channel");
}

vm_channel_t::~vm_channel_t()
{
    dec_ref_count_and_free(_data_buffer);
    disvm::debug::log_msg(component_trace_t::memory, log_level_t::debug, "destroy: vm channel");
}

managed_ptr_t<const type_descriptor_t> vm_channel_t::get_data_type() const
{
    return _data_type;
}

bool vm_channel_t::send_data(vm_channel_request_t &send_request)
{
    assert(send_request.request_handled_callback != nullptr && send_request.data != nullptr);

    std::lock(_data_lock, send_request.request_mutex.get().pending_request_lock);
    std::unique_lock<std::mutex> data_lock{ _data_lock, std::adopt_lock };
    std::lock_guard<std::mutex> curr_req_lock{ send_request.request_mutex.get().pending_request_lock, std::adopt_lock };

    if (!send_request.request_mutex.get().pending_request)
        return false;

    // Assert the sender cannot also be a receiver in the channel
    assert(std::end(_data_receivers) == std::find_if(
        std::begin(_data_receivers),
        std::end(_data_receivers),
        [&send_request](const vm_channel_request_t &req) { return req.thread_id == send_request.thread_id; }));

    auto pending_req_lock = std::unique_lock<std::mutex>{};
    for (;;)
    {
        if (_data_receivers.empty())
        {
            // Check if there is room in the channel buffer
            if (_data_buffer != nullptr)
            {
                if (try_send_to_buffer(send_request))
                    return true;
            }

            if (disvm::debug::is_component_tracing_enabled<component_trace_t::channel>())
                disvm::debug::log_msg(component_trace_t::channel, log_level_t::debug, "channel: send: queue: %d", send_request.thread_id);

            _data_senders.emplace_back(std::move(send_request));
            return false;
        }

        auto &receiver_maybe = _data_receivers.front();

        // Take lock and check the pending request status
        pending_req_lock = std::unique_lock<std::mutex>{ receiver_maybe.request_mutex.get().pending_request_lock };
        if (receiver_maybe.request_mutex.get().pending_request)
        {
            // Indicate both request mutexes as non-pending
            send_request.request_mutex.get().pending_request = false;
            receiver_maybe.request_mutex.get().pending_request = false;
            break;
        }

        pending_req_lock.unlock();
        _data_receivers.pop_front();
    }

    // Remove the receiver from the list
    auto receiver = std::move(_data_receivers.front());
    _data_receivers.pop_front();

    // Transfer data
    _data_transfer(receiver.data, send_request.data, _data_type);

    // Release channel and pending request lock after data transfer
    pending_req_lock.unlock();
    data_lock.unlock();

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::channel>())
        disvm::debug::log_msg(component_trace_t::channel, log_level_t::debug, "channel: send: %d %d", send_request.thread_id, receiver.thread_id);

    receiver.request_handled_callback(*this);

    return true;
}

bool vm_channel_t::receive_data(vm_channel_request_t &receive_request)
{
    assert(receive_request.request_handled_callback != nullptr && receive_request.data != nullptr);

    std::lock(_data_lock, receive_request.request_mutex.get().pending_request_lock);
    std::unique_lock<std::mutex> data_lock{ _data_lock, std::adopt_lock };
    std::lock_guard<std::mutex> curr_req_lock{ receive_request.request_mutex.get().pending_request_lock, std::adopt_lock };

    if (!receive_request.request_mutex.get().pending_request)
        return false;

    // Assert the receiver cannot also be a sender in the channel
    assert(std::end(_data_senders) == std::find_if(
        std::begin(_data_senders),
        std::end(_data_senders),
        [&receive_request](const vm_channel_request_t &req) { return req.thread_id == receive_request.thread_id; }));

    auto pending_req_lock = std::unique_lock<std::mutex>{};
    for (;;)
    {
        if (_data_senders.empty())
        {
            // Check if there is data in the channel buffer
            if (_data_buffer != nullptr)
            {
                if (try_receive_from_buffer(receive_request))
                    return true;
            }

            if (disvm::debug::is_component_tracing_enabled<component_trace_t::channel>())
                disvm::debug::log_msg(component_trace_t::channel, log_level_t::debug, "channel: receive: queue: %d", receive_request.thread_id);

            _data_receivers.emplace_back(std::move(receive_request));
            return false;
        }

        auto &sender_maybe = _data_senders.front();

        // Take lock and check the pending request status
        pending_req_lock = std::unique_lock<std::mutex>{ sender_maybe.request_mutex.get().pending_request_lock };
        if (sender_maybe.request_mutex.get().pending_request)
        {
            // Indicate both request mutexes as non-pending
            receive_request.request_mutex.get().pending_request = false;
            sender_maybe.request_mutex.get().pending_request = false;
            break;
        }

        pending_req_lock.unlock();
        _data_senders.pop_front();
    }

    // Remove the sender from the list
    auto sender = std::move(_data_senders.front());
    _data_senders.pop_front();

    // Check if there is data in the channel buffer. This is being
    // done to avoid starving threads that found a full buffer when
    // attempting to satisfy a request.
    if (_data_buffer != nullptr && try_receive_from_buffer(receive_request))
    {
        // Data was taken from the buffer, so the current pending sender
        // can now use the empty slot.
        auto did_send = try_send_to_buffer(sender);
        assert(did_send && "Should be room in buffer");
    }
    else
    {
        // Transfer data since there wasn't anything in the buffer
        _data_transfer(receive_request.data, sender.data, _data_type);

        if (disvm::debug::is_component_tracing_enabled<component_trace_t::channel>())
            disvm::debug::log_msg(component_trace_t::channel, log_level_t::debug, "channel: receive: %d %d", receive_request.thread_id, sender.thread_id);
    }

    // Release channel and pending request lock after data transfer
    pending_req_lock.unlock();
    data_lock.unlock();

    sender.request_handled_callback(*this);

    return true;
}

void vm_channel_t::cancel_request(uint32_t thread_id)
{
    std::lock_guard<std::mutex> lock{ _data_lock };

    // Create a predicate function based on the input
    auto request_predicate = [thread_id](const vm_channel_request_t &req) { return req.thread_id == thread_id; };

    auto receiver_iter = std::find_if(std::begin(_data_receivers), std::end(_data_receivers), request_predicate);
    if (receiver_iter != std::end(_data_receivers))
    {
        _data_receivers.erase(receiver_iter);
    }
    else
    {
        auto sender_iter = std::find_if(std::begin(_data_senders), std::end(_data_senders), request_predicate);
        if (sender_iter == std::end(_data_senders))
            return;

        _data_senders.erase(sender_iter);
    }

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::channel>())
        disvm::debug::log_msg(component_trace_t::channel, log_level_t::debug, "channel: cancel: request: %d", thread_id);
}

word_t vm_channel_t::get_buffer_size() const
{
    if (_data_buffer == nullptr)
        return 0;
    else
        return _data_buffer->get_length();
}

bool vm_channel_t::try_send_to_buffer(vm_channel_request_t &send_request)
{
    assert(_data_buffer != nullptr);

    const auto buffer_length = static_cast<uint32_t>(_data_buffer->get_length());
    if (_data_buffer_count == buffer_length)
        return false;

    auto next_buffer_index = _data_buffer_head + _data_buffer_count;
    if (next_buffer_index >= buffer_length)
        next_buffer_index -= buffer_length;

    _data_buffer_count++;

    auto dest = _data_buffer->at(next_buffer_index);

    // Transfer data to buffer
    _data_transfer(dest, send_request.data, _data_type);
    send_request.request_mutex.get().pending_request = false;

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::channel>())
        disvm::debug::log_msg(component_trace_t::channel, log_level_t::debug, "channel: send: buffer: %d", send_request.thread_id);

    return true;
}

bool vm_channel_t::try_receive_from_buffer(vm_channel_request_t &receive_request)
{
    assert(_data_buffer != nullptr);

    if (_data_buffer_count == 0)
        return false;

    auto src = _data_buffer->at(_data_buffer_head);
    _data_buffer_head++;

    const auto buffer_length = _data_buffer->get_length();
    if (_data_buffer_head == static_cast<uint32_t>(buffer_length))
        _data_buffer_head = 0;

    _data_buffer_count--;

    // Transfer data from buffer
    _data_transfer(receive_request.data, src, _data_type);

    receive_request.request_mutex.get().pending_request = false;

    if (disvm::debug::is_component_tracing_enabled<component_trace_t::channel>())
        disvm::debug::log_msg(component_trace_t::channel, log_level_t::debug, "channel: receive: buffer: %d", receive_request.thread_id);

    return true;
}