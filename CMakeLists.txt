# MUNSphere — optional CMake build.
# run.bat is the primary build and needs only MinGW. Use this file if you
# have CMake installed:
#   cmake -S . -B build-cmake -G "MinGW Makefiles"
#   cmake --build build-cmake
#   ctest --test-dir build-cmake --output-on-failure
cmake_minimum_required(VERSION 3.16)
project(MUNSphere LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Every module folder is an include directory, as in run.bat.
file(GLOB MODULE_DIRS LIST_DIRECTORIES true ${CMAKE_SOURCE_DIR}/Module*)
set(INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/Shared)
foreach(dir ${MODULE_DIRS})
  if(IS_DIRECTORY ${dir})
    list(APPEND INCLUDE_DIRS ${dir})
  endif()
endforeach()

# C data structures: a static library so C stays separate from C++.
file(GLOB DSA_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Shared/*_dsa.c ${CMAKE_SOURCE_DIR}/Module*/*_dsa.c)
add_library(munsphere_dsa STATIC ${DSA_SOURCES})
target_include_directories(munsphere_dsa PUBLIC ${INCLUDE_DIRS})
target_compile_options(munsphere_dsa PRIVATE -Wall -Wextra)

# C++ application layer (everything except main.cpp), shared with tests.
file(GLOB CPP_SOURCES CONFIGURE_DEPENDS
     ${CMAKE_SOURCE_DIR}/Shared/*.cpp ${CMAKE_SOURCE_DIR}/Module*/*.cpp)
list(FILTER CPP_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")
add_library(munsphere_app STATIC ${CPP_SOURCES})
target_include_directories(munsphere_app PUBLIC ${INCLUDE_DIRS})
target_link_libraries(munsphere_app PUBLIC munsphere_dsa)
target_compile_options(munsphere_app PRIVATE -Wall -Wextra)
if(WIN32)
  target_compile_definitions(munsphere_app PUBLIC __USE_MINGW_ANSI_STDIO=1)
  target_link_libraries(munsphere_app PUBLIC ws2_32 bcrypt)
endif()

add_executable(munsphere ${CMAKE_SOURCE_DIR}/Module1_Conference/main.cpp)
target_link_libraries(munsphere PRIVATE munsphere_app)
if(WIN32 AND MINGW)
  target_link_options(munsphere PRIVATE -static)
endif()

# Tests run from the project root because they use tests/ for temp files.
enable_testing()
file(GLOB C_TESTS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/test_*.c)
foreach(src ${C_TESTS})
  get_filename_component(name ${src} NAME_WE)
  add_executable(${name} ${src})
  target_link_libraries(${name} PRIVATE munsphere_dsa)
  add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endforeach()
file(GLOB CPP_TESTS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/test_*.cpp)
foreach(src ${CPP_TESTS})
  get_filename_component(name ${src} NAME_WE)
  add_executable(${name} ${src})
  target_link_libraries(${name} PRIVATE munsphere_app)
  add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endforeach()
