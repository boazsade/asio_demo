#pragma once
#include <string>

namespace comm {

auto multi_connect(const std::string& host, const std::string& port, const std::string& resource, std::size_t count) -> int;

}	// end of namespace  comm
