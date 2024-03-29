/***
* ==++==
*
* Copyright (c) Microsoft Corporation.  All rights reserved.
*
* ==--==
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* reply_helper_tests.cpp
*
* Tests cases covering the reply helper functions on HTTP response.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace utility;
using namespace web; 
using namespace web::http;
using namespace web::http::experimental::listener;

using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace listener {

SUITE(reply_helper_tests)
{

TEST_FIXTURE(uri_address, json)
{
    http_listener listener(m_uri);
    listener.open().wait();
    test_http_client::scoped_client client(m_uri);
    test_http_client * p_client = client.client();

    listener.support([](http_request request)
    {
        request.reply(status_codes::OK, json::value::parse(U("true"))).wait();
    });
    VERIFY_ARE_EQUAL(0, p_client->request(methods::POST, U("")));
    p_client->next_response().then([&](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::OK, U("application/json"), U("true"));
    }).wait();

    listener.close().wait();
}

TEST_FIXTURE(uri_address, string)
{
    http_listener listener(m_uri);
    listener.open().wait();
    test_http_client::scoped_client client(m_uri);
    test_http_client * p_client = client.client();

   listener.support([](http_request request)
    {
        request.reply(status_codes::OK, U("test str")).wait();
    });
    VERIFY_ARE_EQUAL(0, p_client->request(methods::POST, U("")));
    p_client->next_response().then([&](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::OK, U("text/plain; charset=utf-8"), U("test str"));
    }).wait();

    // content type and string body
    listener.support([](http_request request)
    {
        utility::string_t s(U("test str"));
        request.reply(status_codes::OK, s, U("custom content")).wait();
    });
    VERIFY_ARE_EQUAL(0, p_client->request(methods::POST, U("")));
    p_client->next_response().then([&](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::OK, U("custom content"), U("test str"));
    }).wait();

    // content type and move string body
    listener.support([](http_request request)
    {
        request.reply(status_codes::OK, std::move(utility::string_t(U("test str"))), U("text/plain")).wait();
    });
    VERIFY_ARE_EQUAL(0, p_client->request(methods::POST, U("")));
    p_client->next_response().then([&](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::OK, U("text/plain"), U("test str"));
    }).wait();

    listener.close().wait();
}

TEST_FIXTURE(uri_address, multiple_responses_to_request)
{
    http_listener listener(m_uri);
    listener.open().wait();
    test_http_client::scoped_client client(m_uri);
    test_http_client * p_client = client.client();

    listener.support([](http_request request)
    {
        http_response response(status_codes::OK);
        request.reply(response).wait();

        // try responding to the request again
        VERIFY_THROWS(request.reply(response).wait(), http_exception);
    });
    VERIFY_ARE_EQUAL(0, p_client->request(methods::GET, U("")));
    p_client->next_response().then([&](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::OK);
    }).wait();

    listener.close().wait();
}

}

}}}}