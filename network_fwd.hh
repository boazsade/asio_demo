#pragma once

#include <boost/asio.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>

namespace comm {

namespace asio = boost::asio;
using asio::ip::tcp;
}	// end of namespace comm
