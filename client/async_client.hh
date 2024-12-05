#pragma once
#include "network_fwd.hh"
#include <span>
#include <string>

namespace comm {
    // This is a test function, we are not going to use this in production code    
auto test_multi_connect(const std::string& host, const std::string& port, const std::string& resource, std::size_t count) -> int;    
    // For this function we are opening the connection with the function from sync_client - connect
auto async_http_client(tcp::socket& with_socket, const std::string& host, const std::string& resource) -> boost::asio::awaitable<std::string>;
    // This function will open a connection and send a GET HTTP request, then handle the response from the server
auto async_http_client(std::string host, std::string port, std::string resource) -> boost::asio::awaitable<std::string>;

// This is a fully asynchronous connection as well as all other operations
auto async_http_connect_client(std::string host, std::string port, std::string resource) -> boost::asio::awaitable<std::string>;

// asynchronous connection is made to remote server
auto async_connect(const std::string& host, const std::string& service) -> boost::asio::awaitable<tcp::socket>;
    // Send TCP message that pass to the server the message in `raw_out_msg` and stores the results in `results`
    // it would also return number of bytes reads, if 0, it means that connection failed!
    // Make sure the connection using function from sync_client - connect.
    // Please note that the result span must points to a valid preallocated memory!!
auto async_tcp_read_write(tcp::socket& with_socket, const std::span<uint8_t> raw_out_msg, std::span<uint8_t>& results) -> boost::asio::awaitable<size_t>;

auto async_tcp_read(tcp::socket& with_socket, std::span<uint8_t>& results) -> boost::asio::awaitable<size_t>;

auto async_tcp_read_write(tcp::socket& with_socket, const std::string_view raw_out_msg, const std::string_view delimiter) -> boost::asio::awaitable<std::string>;

}       // end of namespace async