#pragma once
#ifndef __linux__
#   ifndef NO_GLOG
#       define NO_GLOG
#   endif   // NO_GLOG
#include <chrono>
#include <sstream>
#include <string>
#include <iomanip>

namespace internal {
inline auto timestamp() -> std::string {
    using namespace std::chrono;
    using clock = system_clock;
    
    const auto current_time_point {clock::now()};
    const auto current_time {clock::to_time_t (current_time_point)};
    const auto current_localtime {*std::localtime (&current_time)};
    const auto current_time_since_epoch {current_time_point.time_since_epoch()};
    const auto current_milliseconds {duration_cast<milliseconds> (current_time_since_epoch).count() % 1000};
    
    std::ostringstream stream;
    stream << std::put_time (&current_localtime, "%T") << "." << std::setw (3) << std::setfill ('0') << current_milliseconds;
    return stream.str();
}
}   // end of namespace internal

#   include <iostream>
#   ifndef FATAL
#       define FATAL 1
#   endif
#   ifndef ERROR
#       define ERROR 2
#   endif
#   ifndef WARNING
#       define WARNING 3
#   endif
#   ifndef INFO
#       define INFO 4
#   endif
#   define LOG(x) std::cout << ::internal::timestamp() << ": " << __func__ << ": "
#   ifndef ENDL
#       define ENDL  std::endl;
#   endif
//__PRETTY_FUNCTION__
#else
#   include <glog/logging.h>
#   define ENDL ""
#endif      // NO_GLOG

auto init_log() -> void;
