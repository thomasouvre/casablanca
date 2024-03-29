set(CMAKE_LEGACY_CYGWIN_WIN32 0)
cmake_minimum_required(VERSION 2.6)
project(casablanca)

enable_testing()

set(WARNINGS)
set(ANDROID_STL_FLAGS)

# Platform (not compiler) specific settings
if(IOS)
  set(IOS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../Build_iOS")
  set(Boost_FRAMEWORK "-F ${IOS_SOURCE_DIR} -framework boost")
  set(Boost_INCLUDE_DIR "${IOS_SOURCE_DIR}/boost.framework/Headers")

  set(OPENSSL_FOUND 1)
  set(OPENSSL_INCLUDE_DIR "${IOS_SOURCE_DIR}/openssl/include")
  set(OPENSSL_LIBRARIES
    "${IOS_SOURCE_DIR}/openssl/lib/libcrypto.a"
    "${IOS_SOURCE_DIR}/openssl/lib/libssl.a"
    )

  # The cxx_flags must be set here, because the ios-cmake toolchain file unfortunately sets "-headerpad_max_install_names" which is not a valid clang flag.
  set(CMAKE_CXX_FLAGS "-fvisibility=hidden -fvisibility-inlines-hidden")

  set(BUILD_SHARED_LIBS OFF)
  set(BUILD_SAMPLES OFF)
  option(BUILD_TESTS "Build tests." ON)
elseif(ANDROID)
  set(ANDROID_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../Build_android")

  set(Boost_COMPILER "-gcc")
  set(Boost_USE_STATIC_LIBS ON)
  set(BOOST_ROOT "${CMAKE_BINARY_DIR}/../Boost-for-Android/build")
  set(BOOST_LIBRARYDIR "${CMAKE_BINARY_DIR}/../Boost-for-Android/build/lib")
  find_host_package(Boost 1.55 EXACT REQUIRED COMPONENTS random system thread locale filesystem chrono atomic)

  find_package(Threads REQUIRED)
  set(OPENSSL_FOUND 1)
  set(OPENSSL_INCLUDE_DIR "${CMAKE_BINARY_DIR}/../openssl/r9d-9-armeabiv7/include")
  set(OPENSSL_LIBRARIES
    "${CMAKE_BINARY_DIR}/../openssl/r9d-9-armeabiv7/lib/libssl.a"
    "${CMAKE_BINARY_DIR}/../openssl/r9d-9-armeabiv7/lib/libcrypto.a"
    )
  set(LIBCXX_STL "${ANDROID_NDK}/sources/cxx-stl/gnu-libstdc++/4.8/libs/armeabi-v7a/libgnustl_shared.so")
  set(ANDROID_STL_FLAGS
    gcc
    ${LIBCXX_STL}
    atomic
    c
    m
    dl
    -nodefaultlibs
    )

  set(BUILD_SHARED_LIBS ON)
  set(BUILD_SAMPLES OFF)
  option(BUILD_TESTS "Build tests." ON)
elseif(UNIX) # This includes OSX
  find_package(Boost REQUIRED COMPONENTS random chrono system thread locale regex filesystem)
  find_package(Threads REQUIRED)
  find_package(OpenSSL REQUIRED)

  option(BUILD_SHARED_LIBS "Build shared Libraries." ON)
  option(BUILD_TESTS "Build tests." ON)
  option(BUILD_SAMPLES "Build samples." ON)
elseif(WIN32)
  option(BUILD_SHARED_LIBS "Build shared Libraries." ON)
  option(BUILD_TESTS "Build tests." ON)
  option(BUILD_SAMPLES "Build samples." ON)

  add_definitions(-DUNICODE)

  if(NOT BUILD_SHARED_LIBS)
    # This causes cmake to not link the test libraries separately, but instead hold onto their object files.
    set(TEST_LIBRARY_TARGET_TYPE OBJECT)
  endif()

  set(LIB lib)
else()
  message(FATAL_ERROR "-- Unsupported Build Platform.")
endif()

# Compiler (not platform) specific settings
if(ANDROID)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-strict-aliasing")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-pedantic")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-attributes -Wno-pointer-arith")
  include_directories(
    "${ANDROID_NDK}/sources/cxx-stl/gnu-libstdc++/4.8/include"
    "${ANDROID_NDK}/sources/cxx-stl/gnu-libstdc++/4.8/libs/armeabi-v7a/include"
    "${ANDROID_NDK}/sources/cxx-stl/gnu-libstdc++/4.8/include/backward"
    )
elseif(("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang") OR IOS)
  message("-- Setting clang options")

  set(WARNINGS "-Wall -Wextra -Wcast-qual -Wconversion -Wformat=2 -Winit-self -Winvalid-pch -Wmissing-format-attribute -Wmissing-include-dirs -Wpacked -Wredundant-decls")
  set(OSX_SUPPRESSIONS "-Wno-overloaded-virtual -Wno-sign-conversion -Wno-deprecated -Wno-unknown-pragmas -Wno-reorder -Wno-char-subscripts -Wno-switch -Wno-unused-parameter -Wno-unused-variable -Wno-deprecated -Wno-unused-value -Wno-unknown-warning-option -Wno-return-type-c-linkage -Wno-unused-function -Wno-sign-compare -Wno-shorten-64-to-32 -Wno-reorder")
  set(WARNINGS "${WARNINGS} ${OSX_SUPPRESSIONS}")

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -Wno-return-type-c-linkage -Wno-unneeded-internal-declaration")
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++11")

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-strict-aliasing")
  set(STRICT_CXX_FLAGS ${WARNINGS} "-Werror -pedantic")
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  message("-- Setting gcc options")

  set(WARNINGS "-Wall -Wextra -Wunused-parameter -Wcast-align -Wcast-qual -Wconversion -Wformat=2 -Winit-self -Winvalid-pch -Wmissing-format-attribute -Wmissing-include-dirs -Wpacked -Wredundant-decls -Wunreachable-code")
  set(LINUX_SUPPRESSIONS "-Wno-deprecated -Wno-unknown-pragmas -Wno-reorder -Wno-unused-function -Wno-char-subscripts -Wno-switch -Wno-unused-but-set-parameter -Wno-unused-value -Wno-unused-local-typedefs")

  set(WARNINGS "${WARNINGS} ${LINUX_SUPPRESSIONS}")
  set(LD_FLAGS "${LD_FLAGS} -Wl,-z,defs")

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-strict-aliasing")
  set(STRICT_CXX_FLAGS ${WARNINGS} "-Werror -pedantic")
else()
  message("-- Unknown compiler, success is doubtful.")
endif()

# Reconfigure final output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/Binaries)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/Binaries)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/Binaries)

# These settings can be used by the test targets
set(Casablanca_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(Casablanca_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include ${Boost_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/libs/websocketpp)

set(Casablanca_LIBRARY ${LIB}cpprest)
set(Casablanca_LIBRARIES ${Casablanca_LIBRARY} ${Boost_LIBRARIES} ${Boost_FRAMEWORK})

# Everything in the project needs access to the casablanca include directories
include_directories(${Casablanca_INCLUDE_DIRS})

# Finally, the tests all use the same style declaration to build themselves, so we use a function
function(add_casablanca_test NAME SOURCES_VAR)
  add_library(${NAME} ${TEST_LIBRARY_TARGET_TYPE} ${${SOURCES_VAR}})
  message("-- Added test library ${NAME}")
  if (NOT TEST_LIBRARY_TARGET_TYPE STREQUAL "OBJECT")
    target_link_libraries(${NAME}
      ${LIB}httptest_utilities
      ${LIB}common_utilities
      ${LIB}unittestpp
      ${Casablanca_LIBRARIES}
      ${ANDROID_STL_FLAGS}
      )
    if (BUILD_SHARED_LIBS)
      add_test(NAME ${NAME}
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        COMMAND test_runner lib${NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
        )
    endif()
  endif()
endfunction()

add_subdirectory(src)

if(BUILD_TESTS)
  add_subdirectory(tests)
endif()

if(BUILD_SAMPLES)
  add_subdirectory(samples)
endif()

