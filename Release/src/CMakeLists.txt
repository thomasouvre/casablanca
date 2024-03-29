include_directories(../include pch ${Boost_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})

# THE ORDER OF FILES IS VERY /VERY/ IMPORTANT
if(UNIX)
  set(SOURCES
    streams/linux/fileio_linux.cpp
    json/json.cpp
    json/json_parsing.cpp
    json/json_serialization.cpp
    utilities/asyncrt_utils.cpp
    pplx/pplx.cpp
    pplx/threadpool.cpp
    uri/uri.cpp
    uri/uri_builder.cpp
    uri/uri_parser.cpp
    http/common/http_msg.cpp
    http/common/http_helpers.cpp
    http/client/http_msg_client.cpp
    http/client/http_linux.cpp
    http/client/x509_cert_utilities.cpp
    http/listener/http_linux_server.cpp
    http/listener/http_listener.cpp
    http/listener/http_msg_listen.cpp
    http/listener/http_server_api.cpp
    http/oauth/oauth1.cpp
    http/oauth/oauth2.cpp
    websockets/client/ws_msg.cpp
    websockets/client/ws_client.cpp
    utilities/base64.cpp
    )
  if(APPLE)
    set(SOURCES ${SOURCES} pplx/apple/pplxapple.cpp)
    find_library(COREFOUNDATION CoreFoundation "/")
    find_library(SECURITY Security "/")
    set(EXTRALINKS ${COREFOUNDATION} ${SECURITY})
  else()
    set(SOURCES ${SOURCES} pplx/linux/pplxlinux.cpp)
  endif()

  if(ANDROID)
    set(EXTRALINKS "${CMAKE_BINARY_DIR}/../libiconv/r9d-9-armeabiv7a/lib/libiconv.so")
  endif()
elseif(WIN32)
  set(SOURCES
    json/json.cpp
    json/json_parsing.cpp
    json/json_serialization.cpp
    utilities/asyncrt_utils.cpp
    uri/uri.cpp
    uri/uri_builder.cpp
    uri/uri_parser.cpp
    http/common/http_msg.cpp
    http/common/http_helpers.cpp
    http/client/http_msg_client.cpp
    http/client/x509_cert_utilities.cpp
    http/listener/http_listener.cpp
    http/listener/http_msg_listen.cpp
    http/listener/http_server_api.cpp
    websockets/client/ws_msg.cpp
    websockets/client/ws_client.cpp
    utilities/base64.cpp
    http/client/http_win7.cpp
    http/listener/http_windows_server.cpp
    http/oauth/oauth1.cpp
    streams/windows/fileio.cpp
    streams/windows/ioscheduler.cpp
    utilities/dllmain.cpp
    )
  add_definitions(
    -D_ASYNCRT_EXPORT
    -D_PPLX_EXPORT
    -D_MBCS
    -D_USRDLL
    )
  set(EXTRALINKS Httpapi.lib Winhttp.dll)
endif()

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNINGS} -Werror -pedantic")

add_library(${Casablanca_LIBRARY} ${SOURCES})

target_link_libraries(${Casablanca_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT}
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_THREAD_LIBRARY}
  ${Boost_ATOMIC_LIBRARY}
  ${Boost_CHRONO_LIBRARY}
  ${Boost_LOCALE_LIBRARY}
  ${Boost_RANDOM_LIBRARY}
  ${EXTRALINKS}
  ${Boost_FRAMEWORK}
  ${OPENSSL_LIBRARIES}
  ${COREFOUNDATION}
  ${ANDROID_STL_FLAGS}
  )

# Portions specific to cpprest binary versioning.
set (CPPREST_VERSION_MAJOR 2)
set (CPPREST_VERSION_MINOR 2)
set (CPPREST_VERSION_REVISION 0)

if(WIN32)
  set_target_properties(${Casablanca_LIBRARY} PROPERTIES
    OUTPUT_NAME "${Casablanca_LIBRARY}_${CPPREST_VERSION_MAJOR}_${CPPREST_VERSION_MINOR}")
elseif(ANDROID)
  # Do not use SOVERSION on android. It is completely unsupported (and causes problems).
  # Perhaps revisit in the future? (NDK r9d, 8/7/14)
else()
  set_target_properties(${Casablanca_LIBRARY} PROPERTIES
    SOVERSION ${CPPREST_VERSION_MAJOR}.${CPPREST_VERSION_MINOR})
endif()
