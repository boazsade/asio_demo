#include "client.hh"
#include "sync_client.hh"
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

namespace asio = boost::asio;
using asio::ip::tcp;
auto async_post(auto executor, auto op, auto&& token) {
    return asio::async_compose<decltype(token), void()>(
        [executor, fn = std::move(op)](auto& self) mutable {
            asio::post(executor, 
              [fn = std::move(fn), self = std::move(self)]() mutable {
                fn();
                std::move(self).complete();
            });
        },
        token, executor);
}


auto for_each_parallel(auto ex, auto action, auto& input_range, auto&& token) {
    return asio::async_compose<decltype(token), void(std::vector<size_t>)>(
        [ex, action, &input_range](auto& self) mutable {
            auto&& ops = input_range                                                        //
                | std::ranges::views::transform([&](auto& elem) {                                //
                             return async_post(                                             //
                                 ex, [&elem, &action]() { action(elem); }, asio::deferred); //
                         });
            using Op = std::decay_t<decltype(ops.front())>;
            std::vector<Op> vops(ops.begin(), ops.end());

            namespace X = asio::experimental;
            auto grp    = X::make_parallel_group(vops);

            std::move(grp).async_wait(                                                           //
                X::wait_for_all(), [self = std::move(self)](std::vector<size_t> order) mutable { //
                    std::move(self).complete(std::move(order));
                });
        },
        token, ex);
}



auto test_multi(const char* host, const char* port, const char* resource, boost::asio::io_context& ctx) -> int {
    boost::asio::io_context io_context;

    asio::thread_pool tp(3);
    boost::asio::io_context* ctx_handle[] {&ctx, &ctx, &ctx};

    for (auto i : std::ranges::views::iota(0, 5)) {
      std::cout << "running for the " << i << " time.." << std::endl;
      auto fp = for_each_parallel(tp.get_executor(),
          [resource, host, port](auto& ctx) {
              if (auto s = connect(host, port, *ctx); s) {
                if (send_request(s.value(), host, resource)) {
                  const auto a{handle_response(*s)};
                  if (a) {
                    std::cout << "successfully read from server: " << a.value().size() << std::endl;
                  }
                } else {
                  std::cerr << "send failed" << std::endl;
                }
              } else {
                std::cerr << "connect failed" << std::endl;
              }
      }, ctx_handle, asio::use_future);
      std::cout << "successfully lunched" << std::endl;
      fp.wait();
      std::cout << "the wait is over" << std::endl;
    }
    return 0;
}

int main(int argc, char* argv[]) {
  if (argc != 4) {
    std::cout << "Usage: sync_client <server> <port> <path>\n";
    std::cout << "Example:\n";
    std::cout << "  sync_client www.boost.org 8080 /LICENSE_1_0.txt\n";
    return 1;
  }

  try {
      
      return async::multi_connect(argv[1], argv[2], argv[3], 2);

      boost::asio::io_context io_context;
      if (argc == 0) {
      if (test_multi(argv[1], argv[2], argv[3], io_context) < 0) {
        std::cerr << "failed to run in parallel, exiting..\n";
        return -1;
      }
      }
      auto socket{sync::connect(argv[1], argv[2], io_context)};
      if (!socket) {
        return -1;
      }
      if (!sync::send_request(*socket, argv[1], argv[3])) {
        std::cerr << "failed to send request '" << argv[3] << "'\n";
        return -1;
      }
      if (auto response = sync::handle_response(*socket); response)  {
        std::cout << "The response from the server:\n" << *response << "\n";
        return 0;
      } else {
        std::cerr << "failed to read response from the server!!\n";
        return -1;
      }
  } catch (std::exception& e) {
    std::cout << "Exception: " << e.what() << "\n";
  }

  return 0;
}
