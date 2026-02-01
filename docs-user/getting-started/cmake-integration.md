# CMake integration

Zireael is intended to be embedded as a library in a wrapper or application.

## As a subproject (recommended)

```cmake
add_subdirectory(path/to/zireael)
target_link_libraries(my_app PRIVATE Zireael::zireael)
```

Public headers are provided under `include/zr/` (install/packaging is not
opinionated yet; subproject embedding is the supported path today).

## Options

Project options are defined in the top-level `CMakeLists.txt`. CI typically
sets:

- `-DZIREAEL_WARNINGS_AS_ERRORS=ON`
- `-DZIREAEL_BUILD_EXAMPLES=OFF`
