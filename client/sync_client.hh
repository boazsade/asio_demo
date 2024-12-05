#pragma once
#include "network_fwd.hh"
#include <string>
#include <optional>

namespace comm {
auto http_handle_response(tcp::socket& from) -> std::optional<std::string>;
auto http_send_request(tcp::socket& with, const char* host, const std::string& resource) -> bool;

auto tcp_handle_response(tcp::socket& from, const std::string_view delimiter) -> std::optional<std::string>;
auto tcp_send_request(tcp::socket& with, const std::string& request) -> bool;

auto connect(const char* to, const char* port, boost::asio::io_context& ctx) -> std::optional<tcp::socket>;

auto http_upload(tcp::socket& connection, const char* host, const std::string& resource, const std::string& body) -> std::optional<std::string>;
}	// end of namespace comm
