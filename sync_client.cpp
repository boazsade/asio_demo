#include "sync_client.hh"

namespace asio = boost::asio;
using asio::ip::tcp;

namespace sync {
auto connect(const char* to, const char* port, boost::asio::io_context& ctx) -> std::optional<tcp::socket> {
    if (!(to && port)) {
      std::cerr << "we have null points " << std::boolalpha << (to == nullptr) << ", " << (port == nullptr) << std::endl;
      return std::nullopt;
    }
    tcp::resolver resolver(ctx);
    tcp::resolver::results_type endpoints = resolver.resolve(to, port);
    std::cout << "connecting to remote server: " << to << ":" << port << std::endl;
    // Try each endpoint until we successfully establish a connection.
    tcp::socket socket(ctx);
    boost::system::error_code ec;
    boost::asio::connect(socket, endpoints, ec);
    if (ec) {
	    std::cerr << "failed to connect to " << to << ":" << port << " - " << ec.message() << "\n";
      return std::nullopt;
    }
    return socket;
}

auto send_request(tcp::socket& with, const char* host, const std::string& response) -> bool {
    boost::asio::streambuf request;
    std::ostream request_stream(&request);
    request_stream << "GET " << response << " HTTP/1.0\r\n";
    request_stream << "Host: " << host << "\r\n";
    request_stream << "Accept: */*\r\n";
    request_stream << "Connection: close\r\n\r\n";

    // Send the request.
    boost::system::error_code ec;
    boost::asio::write(with, request, ec);
    if (ec) {
      std::cerr << "error sending request: " << ec.message() << "\n";
	    return false;
    }
    return true;
}

auto handle_response(tcp::socket& from) -> std::optional<std::string> {
    boost::asio::streambuf response;
    boost::asio::read_until(from, response, "\r\n");

    // Check that response is OK.
    std::istream response_stream(&response);
    std::string http_version;
    boost::system::error_code ec;
    response_stream >> http_version;
    unsigned int status_code{10000};
    response_stream >> status_code;
    std::string status_message;
    std::getline(response_stream, status_message);
    if (!response_stream || http_version.size() < 6 || http_version.substr(0, 5) != "HTTP/") {
      std::cout << "Invalid response: '" << status_message << "'\n";
      return {};
    }
    if (status_code != 200) {
      std::cout << "Error from server: Response returned with status code " << status_code << "\n";
      return std::nullopt;
    }

    // Read the response headers, which are terminated by a blank line.
    boost::asio::read_until(from, response, "\r\n\r\n", ec);
    if (ec) {
      std::cerr << "failed to read header: " << ec.message() << "\n";
      return {};
    }

    // Process the response headers.
    std::string header;
    while (std::getline(response_stream, header) && header != "\r") {
      //std::cout << header << "\n";
    }
    //std::cout << "\n";

    std::ostringstream output;
    // Write whatever content we already have to output.
    if (response.size() > 0) {
      output << &response;
    }

    // Read until EOF, writing data to output as we go.
    boost::system::error_code error;
    auto r = boost::asio::read(from, response, boost::asio::transfer_at_least(1), error);
    while (r && !error) {
      output << &response;
      r = boost::asio::read(from, response, boost::asio::transfer_at_least(1), error);
    }
    if (error != boost::asio::error::eof) {
      std::cerr << "got invalid error of " << ec.message() << "\n";
      return std::nullopt;
    }
    return output.str();
}
}	// end of namespace sync
