#include "sync_client.hh"
#include "log/logging.hh"

namespace comm {
auto connect(const char* to, const char* port, boost::asio::io_context& ctx) -> std::optional<tcp::socket> {
    if (!(to && port)) {
      LOG(ERROR) << "we have null points " << std::boolalpha << (to == nullptr) << ", " << (port == nullptr) << ENDL;
      return std::nullopt;
    }
    tcp::resolver resolver(ctx);
    tcp::resolver::results_type endpoints = resolver.resolve(to, port);
    LOG(INFO) << "connecting to remote server: " << to << ":" << port << ENDL;
    // Try each endpoint until we successfully establish a connection.
    tcp::socket socket(ctx);
    boost::system::error_code ec;
    boost::asio::connect(socket, endpoints, ec);
    if (ec) {
	    LOG(ERROR) << "failed to connect to " << to << ":" << port << " - " << ec.message() << ENDL;
      return std::nullopt;
    }
    return socket;
}

auto tcp_handle_response(tcp::socket& from, const std::string_view delimiter) -> std::optional<std::string> {
  // in this case we need to know how match to read so we have delimiter
  boost::asio::streambuf response;
  boost::system::error_code ec;
  //LOG(INFO) << "reading response while reading until: '" <<  delimiter << "'" << ENDL;
  auto s = boost::asio::read_until(from, response, delimiter, ec);
  if (ec) {
    LOG(ERROR) << "error while trying to read from socket" << ec.message() << ENDL;
    return std::nullopt;
  }
  //LOG(INFO) << "successfully read " << s << " bytes from the server" << ENDL;
  if (s) {
    std::ostringstream output;
    output << &response;
    return std::optional<std::string>(output.str());
  }
  return std::optional<std::string>(std::string{});
}

auto tcp_send_request(tcp::socket& with, const std::string& request) -> bool {
    boost::system::error_code ec;
    auto s = boost::asio::write(with, boost::asio::buffer(request), ec);
    if (ec) {
      LOG(ERROR) << "error sending request: " << ec.message() << ENDL;
	    return false;
    }
    //LOG(INFO) << "successfully send request [" << request << "] to server of size " << s << " out of " << request.size() << ENDL;
    return s == request.size();
}

auto http_send_request(tcp::socket& with, const char* host, const std::string& response) -> bool {
    boost::asio::streambuf request;
    std::ostream request_stream(&request);
    request_stream << "GET " << response << " HTTP/1.1\r\n";
    request_stream << "Host: " << host << "\r\n";
    request_stream << "Accept: */*\r\n";
    request_stream << "Connection: close\r\n\r\n";

    // Send the request.
    boost::system::error_code ec;
    boost::asio::write(with, request, ec);
    if (ec) {
      LOG(ERROR) << "error sending request: " << ec.message() << ENDL;
	    return false;
    }
    return true;
}

auto http_handle_response(tcp::socket& from) -> std::optional<std::string> {
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
      LOG(WARNING) << "Invalid response: '" << status_message << "'" << ENDL;
      return {};
    }
    if (status_code != 200) {
      LOG(WARNING) << "Error from server: Response returned with status code " << status_code << ENDL;
      return std::nullopt;
    }

    // Read the response headers, which are terminated by a blank line.
    boost::asio::read_until(from, response, "\r\n\r\n", ec);
    if (ec) {
      LOG(ERROR) << "failed to read header: " << ec.message() << ENDL;
      return {};
    }

    // Process the response headers.
    std::string header;
    while (std::getline(response_stream, header) && header != "\r") {
      //LOG(INFO) << header << ENDL;
    }
    //LOG(INFO) << ENDL;

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
      LOG(ERROR) << "got invalid error of " << ec.message() << ENDL;
      return std::nullopt;
    }
    return output.str();
}

auto http_upload(tcp::socket& connection, const char* host, const std::string& resource, const std::string& body) -> std::optional<std::string> {
    boost::asio::streambuf request;
    std::ostream request_stream(&request);
    request_stream << "POST " << resource << " HTTP/1.1\r\n";
    request_stream << "Host: " << host << "\r\n";
    request_stream << "Accept: */*\r\n";
    request_stream << "Content-Type: text/plain; charset=UTF-8\r\n";
    request_stream << "Content-Length: " << body.length() << "\r\n";
    request_stream << "Connection: close\r\n\r\n";
    request_stream << body;

    // Send the request.
    boost::system::error_code ec;
    const auto s = boost::asio::write(connection, request, ec);
    if (ec || s < body.size()) {
      LOG(ERROR) << "error sending request: " << ec.message() << ENDL;
	    return std::nullopt;
    }
    return http_handle_response(connection);
}

}	// end of namespace sync
