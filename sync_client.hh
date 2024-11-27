#pragma once
#include <string>
#include <optional>
#include <boost/asio.hpp>

namespace sync {
auto handle_response(tcp::socket& from) -> std::optional<std::string>;
auto send_request(tcp::socket& with, const char* host, const std::string& response) -> bool;
auto connect(const char* to, const char* port, boost::asio::io_context& ctx) -> std::optional<tcp::socket>;
}	// end of namespace sync
