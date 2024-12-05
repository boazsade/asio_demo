#include "logging.hh"
#include <mutex>

auto init_log() -> void {
#ifndef NO_GLOG
    static std::once_flag flag;
    std::call_once(
        flag,
        []() {
          FLAGS_alsologtostderr = 1;
    	  google::InitGoogleLogging("recorder");
       }
   );
#endif  // NO_GLOG
}
