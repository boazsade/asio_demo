#pragma once
#include <string>
#include <optional>
#include <boost/asio.hpp>

namespace comm {
auto handle_response(boost::asio::ip::tcp::socket& from) -> std::optional<std::string>;
auto send_request(boost::asio::ip::tcp::socket& with, const char* host, const std::string& response) -> bool;
auto connect(const char* to, const char* port, boost::asio::io_context& ctx) -> std::optional<boost::asio::ip::tcp::socket>;
}	// end of namespace sync
