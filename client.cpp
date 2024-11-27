#include "client.hh"
#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <vector>
#include <boost/asio.hpp>
#include <optional>
#include <sstream>
#include <ranges>
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
#include <boost/algorithm/string.hpp>


namespace async {
namespace {
namespace asio = boost::asio;
using asio::ip::tcp;
using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using default_token = asio::deferred_t;
using tcp_acceptor = default_token::as_default_on_t<tcp::acceptor>;
using tcp_socket = default_token::as_default_on_t<tcp::socket>;
namespace this_coro = asio::this_coro;
using asio::use_awaitable;


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

auto read_body(tcp::socket& socket, long len ) -> awaitable<std::string> {
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
                std::cerr << "error: got and error while trying to read headers " <<   e.message() << "\n";                
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
      std::cerr << "critical error while reading from socket " << e.what() << "\n";
      socket.close();
    }
    co_return std::string{};
}

auto async_read_title(tcp::socket& socket, std::string& input) -> awaitable<bool> {
    
    try {
      // consume the headers as a all
      auto [e, n] = co_await asio::async_read_until(socket,
            asio::dynamic_buffer(input), "\r\n\r\n",
            boost::asio::as_tuple(boost::asio::use_awaitable)
      );      
      if (!e) {
          if (!input.empty()) {
            co_return true;
          }
      } else {
          if (e != boost::asio::error::eof) {
              std::cerr << "error: got and error while trying to read headers " <<   e.message() << "\n";
          } else {
            std::cerr << "error: EOF while reading header\n";
          }
          socket.close();
          co_return false;
      }
    } catch (const std::exception& e) {
      std::cerr << "critical error while reading from socket " << e.what() << "\n";
      socket.close();
    }
    co_return false;
}

auto async_send_read(tcp::socket& socket, const std::string& host, const std::string& resource) -> awaitable<std::string> {
    using namespace std::string_literals;

    static constexpr std::size_t FRAME_SIZE{1'024 * 64};
    auto executor = co_await this_coro::executor;


    try {
        std::string message{
              "GET "s + resource + " HTTP/1.0\r\n"s +
            "Host: "s+  host + "\r\n" +
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
          
          auto body= co_await read_body(socket, len);
          co_return body;
        } else {
          std::cerr << "failed to read the headers!!\n";
          co_return "error reading headers"s;
        }
    } catch (const std::exception& e) {
      socket.close();
        co_return "error: while sending over by client "s 
            + ":" + std::to_string(socket.local_endpoint().port()) + " - " + e.what();
        
    }
    co_return "error"s;
}

auto async_http_client(std::string host, std::string port, std::string resource) -> awaitable<std::string> {
  auto executor = co_await this_coro::executor;
  std::cout << "trying to collect and read from client " << host << ":" << port <<std::endl;
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
    auto r = co_await async_send_read(*socket, host, resource);
    //std::cout << "client successfully read " << r.size() << " message" <<std::endl;
    co_return r;
  } else {
    std::cerr << "failed to connect to remote server " << host << ":" << port << "\n";
  }
  co_return std::string{};
}

auto async_clinets(std::string host, std::string port, std::string resource) -> awaitable<void> {
  using namespace boost::asio::experimental::awaitable_operators;

  auto executor = co_await this_coro::executor;
  const auto [client1, client2] = co_await (
        co_spawn(executor, async_http_client(host, port, resource), use_awaitable) &&
        co_spawn(executor, async_http_client(host, port, resource), use_awaitable)
  );
  std::cout << "successfully finish waiting for the client to come with an answer:\n";
  std::cout << "First client:\n" << client1 << "\n--------------------------\n";
  std::cout << "Second client:\n" << client2 << "\n--------------------------\n";
  co_return;

}

}		// end of local namespace

auto multi_connect(const std::string& host, const std::string& port, const std::string& resource, std::size_t count) -> int {

  asio::io_context ctx;
  static constexpr auto JOBS{2};
  static constexpr auto EXECUTERS{3};
  static constexpr auto WAITS_DEPTH{JOBS * EXECUTERS + 1};

  auto work = boost::asio::make_work_guard(ctx);
  co_spawn(ctx, async_clinets(host, port, resource), boost::asio::detached);
  int i{0};
  while (i < WAITS_DEPTH) {
    auto c =  ctx.run_one();
    if (c) {
      i++;
      std::cout << std::endl << "##### we are at iteration number " << i << std::endl;
    } else {
      std::cout << "we did not have any execution, so we will break out" <<std::endl;
      break;
    }
    // so that we will not get stuck
  }
  std::cout << "going out after " << i << " iterations" << std::endl;
  return 1;
}
