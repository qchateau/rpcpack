// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef PACKIO_SERVER_SESSION_H
#define PACKIO_SERVER_SESSION_H

#include <memory>
#include <boost/asio.hpp>
#include <msgpack.hpp>

#include "error_code.h"
#include "internal/log.h"
#include "internal/msgpack_rpc.h"
#include "internal/utils.h"

namespace packio {

template <typename Protocol, typename Dispatcher>
class server_session
    : public std::enable_shared_from_this<server_session<Protocol, Dispatcher>> {
public:
    using protocol_type = Protocol;
    using socket_type = typename protocol_type::socket;
    using std::enable_shared_from_this<server_session<Protocol, Dispatcher>>::shared_from_this;

    static constexpr size_t kBufferReserveSize = 4096;

    server_session(socket_type sock, std::shared_ptr<Dispatcher> dispatcher_ptr)
        : socket_{std::move(sock)}, dispatcher_ptr_{std::move(dispatcher_ptr)}
    {
        DEBUG("starting session {:p}", fmt::ptr(this));
    }

    ~server_session()
    {
        boost::system::error_code ec;
        socket_.cancel(ec);
        if (ec) {
            INFO("cancel failed: {}", ec.message());
        }
        DEBUG("stopped session {:p}", fmt::ptr(this));
    }

    socket_type& socket() { return socket_; }
    const socket_type& socket() const { return socket_; }

    void start() { async_read(std::make_unique<msgpack::unpacker>()); }

private:
    struct Call {
        msgpack_rpc_type type;
        id_type id;
        std::string name;
        msgpack::object args;
    };

    void async_read(std::unique_ptr<msgpack::unpacker> unpacker)
    {
        // abort R/W on error
        if (error_.load(std::memory_order_acquire)) {
            return;
        }

        auto self{shared_from_this()};
        unpacker->reserve_buffer(kBufferReserveSize);

        auto buffer = boost::asio::buffer(
            unpacker->buffer(), unpacker->buffer_capacity());
        socket_.async_read_some(
            buffer,
            [this, self, unpacker = std::move(unpacker)](
                boost::system::error_code ec, size_t length) mutable {
                if (ec) {
                    DEBUG("error: {}", ec.message());
                    error_.store(true, std::memory_order_release);
                    return;
                }

                TRACE("read: {}", length);
                unpacker->buffer_consumed(length);

                for (msgpack::object_handle call; unpacker->next(call);) {
                    // handle the call asynchronously (post)
                    // to schedule the next read immediately
                    // this will allow parallel call handling
                    // in multi-threaded environments
                    async_dispatch(std::move(call));
                }

                async_read(std::move(unpacker));
            });
    }

    void async_dispatch(msgpack::object_handle call)
    {
        auto self{shared_from_this()};
        boost::asio::post(
            socket_.get_executor(),
            [this, self, call = std::move(call)] { dispatch(call.get()); });
    }

    void dispatch(const msgpack::object& msgpack_call)
    {
        std::optional<Call> call = parse_call(msgpack_call);
        if (!call) {
            error_.store(true, std::memory_order_release);
            return;
        }

        auto completion_handler =
            [this, type = call->type, id = call->id, self = shared_from_this()](
                boost::system::error_code ec, msgpack::object_handle result) {
                if (type == msgpack_rpc_type::request) {
                    TRACE("result: {}", ec.message());
                    async_write(id, ec, std::move(result));
                }
            };

        const auto function = dispatcher_ptr_->get(call->name);
        if (function) {
            TRACE("call: {} (id={})", name, id);
            (*function)(completion_handler, call->args);
        }
        else {
            DEBUG("unknown function {}", name);
            completion_handler(make_error_code(error::unknown_function), {});
        }
    }

    std::optional<Call> parse_call(const msgpack::object& call)
    {
        if (call.type != msgpack::type::ARRAY || call.via.array.size < 3) {
            WARN("unexpected message type: {}", call.type);
            return std::nullopt;
        }

        try {
            int idx = 0;
            id_type id = 0;
            msgpack_rpc_type type = static_cast<msgpack_rpc_type>(
                call.via.array.ptr[idx++].as<int>());

            std::size_t expected_size;
            switch (type) {
            case msgpack_rpc_type::request:
                id = call.via.array.ptr[idx++].as<id_type>();
                expected_size = 4;
                break;
            case msgpack_rpc_type::notification:
                expected_size = 3;
                break;
            default:
                WARN("unexpected type: {}", type);
                return std::nullopt;
            }

            if (call.via.array.size != expected_size) {
                WARN("unexpected message size: {}", call.via.array.size);
                return std::nullopt;
            }

            std::string name = call.via.array.ptr[idx++].as<std::string>();
            const msgpack::object& args = call.via.array.ptr[idx++];

            return Call{type, id, name, args};
        }
        catch (msgpack::type_error& exc) {
            WARN("unexpected message type: {} ({})", type, exc.what());
            (void)exc;
            return std::nullopt;
        }
    }

    void async_write(
        id_type id,
        boost::system::error_code ec,
        msgpack::object_handle result_handle)
    {
        // abort R/W on error
        if (error_.load(std::memory_order_acquire)) {
            return;
        }

        auto self(shared_from_this());
        auto packer_buf = std::make_unique<msgpack::vrefbuffer>();
        msgpack::packer<msgpack::vrefbuffer> packer(*packer_buf);

        const auto pack = [&](auto&& error, auto&& result) {
            packer.pack(std::forward_as_tuple(
                static_cast<int>(msgpack_rpc_type::response),
                id,
                std::forward<decltype(error)>(error),
                std::forward<decltype(result)>(result)));
        };

        if (ec) {
            if (result_handle.get().is_nil()) {
                pack(ec.message(), msgpack::type::nil_t{});
            }
            else {
                pack(result_handle.get(), msgpack::type::nil_t{});
            }
        }
        else {
            pack(msgpack::type::nil_t{}, result_handle.get());
        }

        auto buffer = internal::buffer_to_asio(*packer_buf);
        boost::asio::async_write(
            socket_,
            buffer,
            [this,
             self,
             packer_buf = std::move(packer_buf),
             result_handle = std::move(result_handle)](
                boost::system::error_code ec, size_t length) {
                if (ec) {
                    DEBUG("error: {}", ec.message());
                    error_.store(true, std::memory_order_release);
                    return;
                }

                TRACE("write: {}", length);
                (void)length;
            });
    }

    socket_type socket_;
    std::shared_ptr<Dispatcher> dispatcher_ptr_;
    std::atomic<bool> error_{false};
};

} // packio

#endif // PACKIO_SERVER_SESSION_H
