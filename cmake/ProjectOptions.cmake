include_guard(GLOBAL)

option(XEM_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(XEM_WARNINGS_AS_ERRORS "Treat project warnings as errors" ON)
add_library(xem_project_options INTERFACE)
target_compile_features(xem_project_options INTERFACE cxx_std_20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
  target_compile_options(xem_project_options INTERFACE /W4 /permissive- /Zc:__cplusplus)
  if(XEM_WARNINGS_AS_ERRORS)
    target_compile_options(xem_project_options INTERFACE /WX)
  endif()
else()
  target_compile_options(xem_project_options INTERFACE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
    -Wformat=2 -Wundef -Wdouble-promotion)
  if(XEM_WARNINGS_AS_ERRORS)
    target_compile_options(xem_project_options INTERFACE -Werror)
  endif()
endif()

if(XEM_SANITIZERS)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(Clang|AppleClang|GNU)$" OR WIN32)
    message(FATAL_ERROR "The sanitizer preset requires Clang/GCC on Linux or macOS.")
  endif()
  target_compile_options(xem_project_options INTERFACE
    -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
  target_link_options(xem_project_options INTERFACE
    -fsanitize=address,undefined -fno-sanitize-recover=all)
endif()
