#include "async_client.hh"
#include "log/logging.hh"
#include <iostream>
#include <vector>
#include <optional>
#include <sstream>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/algorithm/string.hpp>


namespace comm {
namespace {
namespace asio = boost::asio;
using asio::ip::tcp;
using asio::co_spawn;
using asio::detached;
using default_token = asio::deferred_t;
namespace this_coro = asio::this_coro;
using boost::asio::use_awaitable;


static std::atomic_int tid_gen = 0;
thread_local int const tid     = ++tid_gen;

static constexpr auto parse_len = [](const std::string& headers) {
    std::string entry;
    std::istringstream inputs(headers);
    std::string line;
    while (std::getline(inputs, line)) {      
      if (auto i = line.find_first_of(':'); i != std::string::npos) {
        if (boost::algorithm::icontains(line.substr(0, i), "Content-Length")) {
          return std::stol(line.substr(i+1));
        }
      }
    }
    return 0l;
};

auto tcp_async_send(tcp::socket& with_socket, const auto& raw_out_msg) -> asio::awaitable<std::size_t> {
  assert(!raw_out_msg.empty());

  if (!with_socket.is_open()) {
    LOG(WARNING) << "trying to send/read from closed connection" << ENDL;
    co_return 0;
  }

    auto s = co_await  boost::asio::async_write(
            with_socket, boost::asio::buffer(raw_out_msg.data(),
            raw_out_msg.size()), boost::asio::use_awaitable
    );
    if (s != raw_out_msg.size()) {
      LOG(ERROR) << "failed to send message size " << raw_out_msg.size() << " to the server at "
          << with_socket.remote_endpoint().address()
          << ":" << with_socket.remote_endpoint().port() << ENDL;
      co_return 0;
    }
    co_return s;
}

auto read_body(tcp::socket& socket, long len ) -> asio::awaitable<std::string> {
  try {
      // consume the headers as a all
      std::string buffer(len, '\0');
      std::size_t r{0};
      while (r < len) {
        auto [e, n] = co_await socket.async_read_some(
              asio::buffer(buffer, buffer.size()),
              boost::asio::as_tuple(boost::asio::use_awaitable)
        );
        r += n;
        if (!e) {          
          if (r >= len) {
            buffer.resize(len);
          }
          co_return buffer;
        } else {
            if (e != boost::asio::error::eof) {
                LOG(ERROR) << "error: got and error while trying to read headers " <<   e.message() << ENDL;                
            } else {
              if (n == len) {
                co_return buffer;
              }
            }
            socket.close();
            co_return std::string{};
        }
      }
    } catch (const std::exception& e) {
      LOG(ERROR) << "critical error while reading from socket " << e.what() << ENDL;
      socket.close();
    }
    co_return std::string{};
}

auto async_read_title(tcp::socket& socket, std::string& input) -> asio::awaitable<bool> {
    
    try {
      // consume the headers as a all
      auto [e, n] = co_await boost::asio::async_read_until(socket,
            asio::dynamic_buffer(input), "\r\n\r\n",
            boost::asio::as_tuple(boost::asio::use_awaitable)
      );      
      if (!e) {
          if (!input.empty()) {
            co_return true;
          }
      } else {
          if (e != boost::asio::error::eof) {
              LOG(ERROR) << "error: got and error while trying to read headers " <<   e.message() << ENDL;
          } else {
            LOG(ERROR) << "error: EOF while reading header" << ENDL;
          }
          socket.close();
          co_return false;
      }
    } catch (const std::exception& e) {
      LOG(ERROR) << "critical error while reading from socket " << e.what() << ENDL;
      socket.close();
    }
    co_return false;
}

auto async_send_read(tcp::socket& socket, const std::string& host, const std::string& resource) -> asio::awaitable<std::string> {
    using namespace std::string_literals;

    static constexpr std::size_t FRAME_SIZE{1'024 * 64};
    auto executor = co_await this_coro::executor;


    try {
        const std::string message{
            "GET "s + resource + " HTTP/1.1\r\n"s +
            "Host: "s +  host + "\r\n" +
            "Accept: */*\r\n"s +
            "Connection: close\r\n\r\n"s
         };

        auto s = co_await  boost::asio::async_write(socket, boost::asio::buffer(message, message.size()), boost::asio::use_awaitable);
        if (s != message.size()) {
            co_return "error: failed to send image header "s + message;
        }

        // read what the server sent
        std::string headers;
        const auto r = co_await async_read_title(socket, headers);
        if (r) {
          const auto len{parse_len(headers)};
          if (len == 0) {
            co_return "missing length in message"s;
          }
          // now we are ready to read the body
          
          auto body = co_await read_body(socket, len);
          co_return body;
        } else {
          LOG(ERROR) << "failed to read the headers!!" << ENDL;
          co_return "error reading headers"s;
        }
    } catch (const std::exception& e) {
      socket.close();
        co_return "error: while sending over by client "s 
            + ":" + std::to_string(socket.local_endpoint().port()) + " - " + e.what();
        
    }
    co_return "error"s;
}

auto read_from_server(tcp::socket& socket, std::span<uint8_t>& payload) -> asio::awaitable<size_t> {
  static const size_t MAX_BUFFER{1'024 * 64};
  std::array<uint8_t, MAX_BUFFER> data;

  auto size{payload.size()};

  auto read_into = [&payload, &data] (auto leftover, auto n) {
      if (n == 0) {
          return leftover;
      }
      const auto s{std::min(n, leftover)};
      const auto loc{(payload.size() - leftover)};
      std::copy(data.begin(), data.begin() + s, payload.begin() + loc);
      return leftover - s;
  };

  auto to_read = std::min(MAX_BUFFER, size);
  size_t total{0};
  while (socket.is_open() && size != 0) {        
      auto [e, n] = co_await socket.async_read_some(boost::asio::buffer(data, to_read),
                                  boost::asio::as_tuple(boost::asio::use_awaitable));
      total += n;
      if (e) {
          if (e != boost::asio::error::eof) {
              LOG(ERROR) << "error reading from socket: " << e.message() << ENDL;
          }
          socket.close();
          co_return 0;                   
      } else {
          size = read_into(size, n);
          to_read = std::min(MAX_BUFFER, size);
      }
  }
  
  co_return total;
}


}		// end of local namespace

auto async_tcp_read_write(tcp::socket& with_socket, const std::span<uint8_t> raw_out_msg, std::span<uint8_t>& results) -> asio::awaitable<size_t> {
  assert(!results.empty());

  try {
    auto s = co_await tcp_async_send(with_socket, raw_out_msg);
    if (s == 0) {
      co_return 0;
    }
    // now try to read from the remote host the message, we "know" what should be the message, so we have a buffer ready for that
    co_return co_await read_from_server(with_socket, results);
  } catch (const std::exception& e) {
    LOG(ERROR) << "critical error while trying to send/receive from "
            << with_socket.remote_endpoint().address()
            << ":" << with_socket.remote_endpoint().port()
            << ": " << e.what() << ENDL;
    with_socket.close();
    co_return 0;
  }

}

auto async_tcp_read(tcp::socket& with_socket, std::span<uint8_t>& results) -> asio::awaitable<size_t> {
  try  {
    co_return co_await read_from_server(with_socket, results);
  } catch (const std::exception& e) {
    LOG(ERROR) << "critical error while trying to receive from "
            << with_socket.remote_endpoint().address()
            << ":" << with_socket.remote_endpoint().port()
            << ": " << e.what() << ENDL;
    with_socket.close();
    co_return 0;
  }
}

auto async_http_client(tcp::socket& with_socket, const std::string& host, const std::string& resource) -> asio::awaitable<std::string> {
  auto r = co_await async_send_read(with_socket, host, resource);
  co_return r;
}

auto async_http_client(std::string host, std::string port, std::string resource) -> asio::awaitable<std::string> {
  auto executor = co_await this_coro::executor;
  LOG(INFO) << "trying to collect and read from client " << host << ":" << port <<std::endl;
  auto make_connection = [executor](auto& host, auto& port) -> std::optional<tcp::socket>{
    tcp::resolver resolver(executor);
    tcp::resolver::query query(host, port);
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
    tcp::socket socket(executor);
    boost::system::error_code ec;
    boost::asio::connect(socket, endpoint_iterator, ec);
    if (!ec) {
      return std::optional<tcp::socket>(std::move(socket));
    }
    return std::nullopt;
  };

  if (auto socket = make_connection(host, port); socket) {
    co_return co_await async_http_client(*socket, host, resource);
  } else {
    LOG(ERROR) << "failed to connect to remote server " << host << ":" << port << ENDL;
  }
  co_return std::string{};
}

auto async_connect(const std::string& host, const std::string& service) -> asio::awaitable<tcp::socket> {
  auto executor = co_await this_coro::executor;
  tcp::resolver r(executor);
  tcp::resolver::query q(host, service);
  tcp::socket s(executor);
  auto [e, res] = co_await r.async_resolve(q, boost::asio::as_tuple(boost::asio::use_awaitable));
  if (e) {
    LOG(ERROR) << "failed to resolve " << host << ":" << service << ENDL;
    s.close();
    co_return s;
  }
  auto [err, rr] = co_await boost::asio::async_connect(s, res, boost::asio::as_tuple(boost::asio::use_awaitable));
  if (e) {
    LOG(ERROR) << "connection to " << host << ":" << service << "failed: " << err.message() << ENDL;
    s.close();
  }
  co_return s;
}

auto async_http_connect_client(std::string host, std::string port, std::string resource) -> asio::awaitable<std::string> {
  auto executor = co_await this_coro::executor;
  auto s = co_await async_connect(host, port);
  if (s.is_open()) {
    co_return co_await async_send_read(s, host, resource);
  }
  co_return std::string{};
}

auto async_tcp_read_write(tcp::socket& with_socket, const std::string_view raw_out_msg, const std::string_view delimiter) -> asio::awaitable<std::string> {
  
    // now try to read from the remote host the message, we "know" what should be the message, so we have a buffer ready for that
  try {
    auto s = co_await tcp_async_send(with_socket, raw_out_msg);
    if (s == 0) {
      co_return std::string{};
    }
    std::string answer;
    auto [e, n] = co_await boost::asio::async_read_until(with_socket,
            asio::dynamic_buffer(answer), delimiter,
            boost::asio::as_tuple(boost::asio::use_awaitable)
      );      
      if (!e) {
        co_return answer;
      } else {
          if (e != boost::asio::error::eof) {
              LOG(ERROR) << "error: got and error while trying to read headers " <<   e.message() << ENDL;
          } else {
            LOG(ERROR) << "error: EOF while reading header" << ENDL;
          }
          with_socket.close();
          co_return std::string{};
      }
  } catch (const std::exception& e) {
    LOG(ERROR) << "critical error while trying to send/receive from "
            << with_socket.remote_endpoint().address()
            << ":" << with_socket.remote_endpoint().port()
            << ": " << e.what() << ENDL;
    with_socket.close();
    co_return std::string{};
  }
}

auto async_clinets(std::string host, std::string port, std::string resource, asio::io_context& ctx) -> asio::awaitable<void> {
  using namespace boost::asio::experimental::awaitable_operators;

  auto executor = co_await this_coro::executor;
  const auto [client1, client2] = co_await (
        co_spawn(executor, async_http_client(host, port, resource), use_awaitable) &&
        co_spawn(executor, async_http_client(host, port, resource), use_awaitable)
  );
  std::cout << "successfully finish waiting for the client to come with an answer:\n";
  std::cout << "First client:\n" << client1 << "\n--------------------------\n";
  std::cout << "Second client:\n" << client2 << "\n--------------------------\n";
  ctx.stop();
  co_return;

}

auto test_multi_connect(const std::string& host, const std::string& port, const std::string& resource, std::size_t count) -> int {

  asio::io_context ctx;
  static constexpr auto JOBS{2};
  static constexpr auto EXECUTERS{3};
  static constexpr auto WAITS_DEPTH{JOBS * EXECUTERS + 1};

  auto work = boost::asio::make_work_guard(ctx);
  co_spawn(ctx, async_clinets(host, port, resource, ctx), boost::asio::detached);
  ctx.run();
  return 1;
}
} // end of namespace async
