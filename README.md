# This is a demo for an ASIO async client

## Dependencies
- You would need to install `liburing-dev` on Linux
- This also depends on `glog` and `boost-asio` which are manage in this project with `conan`. If you are not using `conan` then you need to have boost development and glong installed as well.

## Build
```bash
conan install . -s build_type=Release
cmake --preset conan-release
cmake --build --preset conan-release

```
