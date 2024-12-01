#pragma once
#include "network_fwd.hh"
#include <string>
#include <atomic>
#include <memory>

namespace comm {


/// Use an existing connection to send and read from a socket
auto async_send_read(tcp::socket& socket, const std::string& host, const std::string& resource) -> asio::awaitable<std::string>;

/// Connect to a remote server and then send and read. The connection itself is synchronous
auto async_http_client(std::string host, std::string port, std::string resource) -> asio::awaitable<std::string>;

/// This will make am asyncrounds connection to a remote server and then read from it.
auto async_http_connect_client(std::string host, std::string port, std::string resource) -> asio::awaitable<std::string>;

/// This is for tests - we are lunching some connection and then read from them asyncrounds way using coroutines
auto multi_connect(const std::string& host, const std::string& port, const std::string& resource, std::size_t count) -> int;

}	// end of namespace  comm
