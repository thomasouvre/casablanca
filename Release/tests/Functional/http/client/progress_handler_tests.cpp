/***
* ==++==
*
* Copyright (c) Microsoft Corporation. All rights reserved. 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* ==--==
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* progress_handler_tests.cpp
*
* Tests cases manually building up HTTP requests with progress handlers.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

#ifdef _MS_WINDOWS
#include <WinError.h>
#endif

using namespace web::http;
using namespace web::http::client;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace client {

SUITE(progress_handler_tests)
{

TEST_FIXTURE(uri_address, set_progress_handler_no_bodies)
{
    http_client_config config;
    config.set_chunksize(512);

    http_client client(m_uri, config);
    const method mtd = methods::GET;
    utility::size64_t upsize = 4711u, downsize = 4711u;
    int calls = 0;

    http_request msg(mtd);
    msg.set_progress_handler(
        [&](message_direction::direction direction, utility::size64_t so_far) 
        { 
            calls += 1;
            if (direction == message_direction::upload) 
                upsize = so_far; 
            else 
                downsize = so_far; 
        });

    test_http_server::scoped_server scoped(m_uri);
    scoped.server()->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, mtd, U("/"));
        std::map<utility::string_t, utility::string_t> headers;
        p_request->reply(200, utility::string_t(U("OK")), headers);
    });

    auto response = client.request(msg).get();
    http_asserts::assert_response_equals(response, status_codes::OK);

    VERIFY_ARE_EQUAL(0, upsize);

    response.content_ready().wait();

    VERIFY_ARE_EQUAL(0, downsize);
    VERIFY_ARE_EQUAL(2, calls);
}

TEST_FIXTURE(uri_address, set_progress_handler_upload)
{
    http_client_config config;
    config.set_chunksize(512);

    http_client client(m_uri, config);
    const method mtd = methods::POST;
    utility::string_t data;
    utility::string_t content_type = U("text/plain; charset=utf-8");

    const size_t repeats = 5500;
    for (size_t i = 0; i < repeats; ++i)
        data.append(U("abcdefghihklmnopqrstuvwxyz"));
    
    utility::size64_t upsize = 4711u, downsize = 4711u;
    int calls = 0;

    http_request msg(mtd);
    msg.set_progress_handler(
        [&](message_direction::direction direction, utility::size64_t so_far) 
        { 
            calls += 1;
            if (direction == message_direction::upload) 
                upsize = so_far; 
            else 
                downsize = so_far; 
        });

    msg.set_body(data);

    test_http_server::scoped_server scoped(m_uri);
    scoped.server()->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, mtd, U("/"), content_type, data);
        std::map<utility::string_t, utility::string_t> headers;
        p_request->reply(200, utility::string_t(U("OK")), headers);
    });

    auto response = client.request(msg).get();
    http_asserts::assert_response_equals(response, status_codes::OK);

    VERIFY_ARE_EQUAL(26u*repeats, upsize);

    response.content_ready().wait();

    VERIFY_ARE_EQUAL(0, downsize);
    // We don't have very precise control over how much of a message is transferred
    // in each chunk being sent or received, so we can't make an exact comparison here.
    VERIFY_IS_TRUE(calls >= 3);
}

TEST_FIXTURE(uri_address, set_progress_handler_download)
{
    http_client_config config;
    config.set_chunksize(512);

    http_client client(m_uri, config);
    const method mtd = methods::GET;
    
    utility::size64_t upsize = 4711u, downsize = 4711u;
    int calls = 0;

    http_request msg(mtd);
    msg.set_progress_handler(
        [&](message_direction::direction direction, utility::size64_t so_far) 
        { 
            calls += 1;
            if (direction == message_direction::upload) 
                upsize = so_far; 
            else 
                downsize = so_far; 
        });

    const size_t repeats = 6000;

    test_http_server::scoped_server scoped(m_uri);
    scoped.server()->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, mtd, U("/"));
        std::string resp_data;
        for (size_t i = 0; i < repeats; ++i)
            resp_data.append("abcdefghihklmnopqrstuvwxyz");

        std::map<utility::string_t, utility::string_t> headers;
        headers[U("Content-Type")] = U("text/plain");
        p_request->reply(200, utility::string_t(U("OK")), headers, resp_data);
    });

    auto response = client.request(msg).get();
    http_asserts::assert_response_equals(response, status_codes::OK);

    VERIFY_ARE_EQUAL(0, upsize);

    response.content_ready().wait();

    VERIFY_ARE_EQUAL(26u*repeats, downsize);
    // We don't have very precise control over how much of a message is transferred
    // in each chunk being sent or received, so we can't make an exact comparison here.
    VERIFY_IS_TRUE(calls > 4);
}

TEST_FIXTURE(uri_address, set_progress_handler_upload_and_download)
{
    http_client_config config;
    config.set_chunksize(512);

    http_client client(m_uri, config);
    const method mtd = methods::POST;
    utility::string_t data;
    utility::string_t content_type = U("text/plain; charset=utf-8");

    const size_t repeats = 5500;
    for (size_t i = 0; i < repeats; ++i)
        data.append(U("abcdefghihklmnopqrstuvwxyz"));
    
    utility::size64_t upsize = 4711u, downsize = 4711u;
    int calls = 0;

    http_request msg(mtd);
    msg.set_progress_handler(
        [&](message_direction::direction direction, utility::size64_t so_far) 
        { 
            calls += 1;
            if (direction == message_direction::upload) 
                upsize = so_far; 
            else 
                downsize = so_far; 
        });

    msg.set_body(data);

    test_http_server::scoped_server scoped(m_uri);
    scoped.server()->next_request().then([&](test_request *p_request)
    {
        http_asserts::assert_test_request_equals(p_request, mtd, U("/"), content_type, data);
        std::string resp_data;
        for (size_t i = 0; i < repeats*2; ++i)
            resp_data.append("abcdefghihklmnopqrstuvwxyz");

        std::map<utility::string_t, utility::string_t> headers;
        headers[U("Content-Type")] = U("text/plain");
        p_request->reply(200, utility::string_t(U("OK")), headers, resp_data);
    });

    auto response = client.request(msg).get();
    http_asserts::assert_response_equals(response, status_codes::OK);

    VERIFY_ARE_EQUAL(26u*repeats, upsize);

    response.content_ready().wait();

    VERIFY_ARE_EQUAL(26u*repeats*2, downsize);
    // We don't have very precise control over how much of a message is transferred
    // in each chunk being sent or received, so we can't make an exact comparison here.
    VERIFY_IS_TRUE(calls > 4);
}

TEST_FIXTURE(uri_address, set_progress_handler_open_failure)
{
    http_client client(U("http://localhost323:-1"));

    const method mtd = methods::POST;
    utility::string_t data;
    utility::string_t content_type = U("text/plain; charset=utf-8");

    const size_t repeats = 5500;
    for (size_t i = 0; i < repeats; ++i)
        data.append(U("abcdefghihklmnopqrstuvwxyz"));
    
    utility::size64_t upsize = 4711u, downsize = 4711u;
    int calls = 0;

    http_request msg(mtd);
    // We should never see this handler called.
    msg.set_progress_handler(
        [&](message_direction::direction direction, utility::size64_t so_far) 
        { 
            calls += 1;
            if (direction == message_direction::upload) 
                upsize = so_far; 
            else 
                downsize = so_far; 
        });

    msg.set_body(data);

    auto response = client.request(msg);
    VERIFY_THROWS(response.get(), web::http::http_exception);
    VERIFY_ARE_EQUAL(4711u, upsize);
    VERIFY_ARE_EQUAL(4711u, downsize);
    VERIFY_ARE_EQUAL(0, calls);
}

TEST_FIXTURE(uri_address, set_progress_handler_request_timeout)
{
    test_http_server::scoped_server scoped(m_uri);
    http_client_config config;
    config.set_chunksize(512);
    config.set_timeout(utility::seconds(1));

    http_client client(m_uri, config);

    const method mtd = methods::POST;
    utility::string_t data;
    utility::string_t content_type = U("text/plain; charset=utf-8");

    const size_t repeats = 5500;
    for (size_t i = 0; i < repeats; ++i)
        data.append(U("abcdefghihklmnopqrstuvwxyz"));
    
    utility::size64_t upsize = 4711u, downsize = 4711u;
    int calls = 0;

    http_request msg(mtd);
    // We should never see this handler called for download, but for upload should still happen, since
    // there's a server (just not a very responsive one) and we're sending data to it.
    msg.set_progress_handler(
        [&](message_direction::direction direction, utility::size64_t so_far) 
        { 
            calls += 1;
            if (direction == message_direction::upload) 
                upsize = so_far; 
            else 
                downsize = so_far; 
        });

    msg.set_body(data);

    auto response = client.request(msg);
    VERIFY_THROWS_HTTP_ERROR_CODE(response.get(), std::errc::timed_out);
    VERIFY_ARE_EQUAL(26u*repeats, upsize);
    VERIFY_ARE_EQUAL(4711u, downsize);
    // We don't have very precise control over how much of the message is transferred
    // before the exception occurs, so we can't make an exact comparison here.
    VERIFY_IS_TRUE(calls >= 2);
}

TEST_FIXTURE(uri_address, upload_nobody_exception, "Ignore:Apple", "900236", "Ignore:Linux", "900236")
{
    test_http_server::scoped_server scoped(m_uri);
    http_client client(m_uri);
    http_request msg(methods::GET);

    scoped.server()->next_request().then([&](test_request *p_request)
    {
        p_request->reply(200, utility::string_t(U("OK")));
    });

    msg.set_progress_handler([](message_direction::direction, utility::size64_t)
    {
        // First all is for data upload completion
        throw std::invalid_argument("fake error");
    });

    VERIFY_THROWS(client.request(msg).get(), std::invalid_argument);
}

TEST_FIXTURE(uri_address, download_nobody_exception)
{
    test_http_server::scoped_server scoped(m_uri);
    http_client client(m_uri);
    http_request msg(methods::GET);

    scoped.server()->next_request().then([&](test_request *p_request)
    {
        p_request->reply(200, utility::string_t(U("OK")));
    });

    int numCalls = 0;
    msg.set_progress_handler([&](message_direction::direction, utility::size64_t)
    {
        if(++numCalls == 2)
        {
            // second is for data download
            throw std::invalid_argument("fake error");
        }
    });

    VERIFY_THROWS(client.request(msg).get().content_ready().get(), std::invalid_argument);
}

TEST_FIXTURE(uri_address, data_upload_exception, "Ignore:Linux", "898953", "Ignore:Apple", "898953")
{
    test_http_server::scoped_server scoped(m_uri);
    http_client client(m_uri);
    http_request msg(methods::PUT);
    msg.set_body(U("A"));

    msg.set_progress_handler([&](message_direction::direction, utility::size64_t)
    {
        throw std::invalid_argument("fake error");
    });

    VERIFY_THROWS(client.request(msg).get(), std::invalid_argument);
}

TEST_FIXTURE(uri_address, data_download_exception)
{
    test_http_server::scoped_server scoped(m_uri);
    http_client client(m_uri);
    http_request msg(methods::GET);

    scoped.server()->next_request().then([&](test_request *p_request)
    {
        std::string resp_data("abc");
        std::map<utility::string_t, utility::string_t> headers;
        headers[U("Content-Type")] = U("text/plain");
        p_request->reply(200, utility::string_t(U("OK")), headers, resp_data);
    });

    int numCalls = 0;
    msg.set_progress_handler([&](message_direction::direction, utility::size64_t)
    {
        if(++numCalls == 2)
        {
            // 2rd is for data download
            throw std::invalid_argument("fake error");
        }
    });

    VERIFY_THROWS(client.request(msg).get().content_ready().get(), std::invalid_argument);
}

}

}}}}
