/***
* ==++==
*
* Copyright (c) Microsoft Corporation.  All rights reserved.
*
* ==--==
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* request_handler_tests.cpp
*
* Tests cases for covering the http_listener class itself.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web;
using namespace utility;
using namespace concurrency;
using namespace web::http;
using namespace web::http::experimental::listener;

using namespace tests::common::utilities;
using namespace tests::functional::http::utilities;

namespace tests { namespace functional { namespace http { namespace listener {

std::shared_ptr<web::http::experimental::listener::http_listener> uri_address::s_dummy_listener;

SUITE(request_handler_tests)
{

TEST_FIXTURE(uri_address, support)
{
    http_listener listener(m_uri);
    listener.open().wait();
    test_http_client::scoped_client client(m_uri);
    test_http_client * p_client = client.client();

    listener.support(U("CUSTOM"), [](http_request request)
    {
        http_asserts::assert_request_equals(request, U("CUSTOM"), U("/"));
        request.reply(status_codes::OK);
    });
    listener.support(methods::PUT, [](http_request request)
    {
        http_asserts::assert_request_equals(request, methods::PUT, U("/"));
        request.reply(status_codes::OK);
    });
    VERIFY_ARE_EQUAL(0, p_client->request(methods::PUT, U("/")));
    p_client->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::OK);
    }).wait();
    VERIFY_ARE_EQUAL(0, p_client->request(methods::DEL, U("/")));
    p_client->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::MethodNotAllowed);
    }).wait();
    VERIFY_ARE_EQUAL(0, p_client->request(U("CUSTOM"), U("/")));
    p_client->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::OK);
    }).wait();

    // Add one with a different case.
    listener.support(U("CUSToM"), [](http_request request)
    {
        http_asserts::assert_request_equals(request, U("CUSToM"), U("/"));
        request.reply(status_codes::Gone);
    });
    VERIFY_ARE_EQUAL(0, p_client->request(U("CUSToM"), U("/")));
    p_client->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::Gone);
    }).wait();

    // Add a general handler
    listener.support([](http_request request)
    {
        http_asserts::assert_request_equals(request, U("CuSToM"), U("/"));
        request.reply(status_codes::Created);
    });
    VERIFY_ARE_EQUAL(0, p_client->request(U("CUSToM"), U("/")));
    p_client->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::Gone);
    }).wait();
    VERIFY_ARE_EQUAL(0, p_client->request(U("CuSToM"), U("/")));
    p_client->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::Created);
    }).wait();
    listener.close().wait();
}

TEST_FIXTURE(uri_address, exceptions_in_handler)
{
    http_listener listener(m_uri);
    listener.open().wait();
    test_http_client::scoped_client client(m_uri);
    test_http_client * p_client = client.client();

    // throw exception
    listener.support(methods::GET, [](http_request request)
    {
        http_asserts::assert_request_equals(request, U("GET"), U("/"));
        throw std::runtime_error("");
    });
    VERIFY_ARE_EQUAL(0, p_client->request(U("GET"), U("/")));
    p_client->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::InternalError);
    }).wait();

    // throw exception, after replying first
    listener.support(methods::PUT, [](http_request request)
    {
        http_asserts::assert_request_equals(request, U("PUT"), U("/"));
        request.reply(status_codes::OK);
        throw 55;
    });
    VERIFY_ARE_EQUAL(0, p_client->request(U("PUT"), U("/")));
    p_client->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::OK);
    }).wait();
    listener.close().wait();
}

TEST_FIXTURE(uri_address, handle_options)
{
    http_listener listener(m_uri);
    listener.open().wait();
    test_http_client::scoped_client client(m_uri);
    test_http_client * p_client = client.client();

    listener.support(methods::GET, [](http_request){});
    listener.support(methods::PUT, [](http_request){});
    VERIFY_ARE_EQUAL(0, p_client->request(methods::OPTIONS, U("/")));
    p_client->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::OK);
        VERIFY_ARE_EQUAL(U("GET, PUT"), p_response->m_headers[U("Allow")]);
    }).wait();

    // try overridding the default OPTIONS handler
    listener.support(methods::OPTIONS, [](http_request request)
    {
        http_asserts::assert_request_equals(request, methods::OPTIONS, U("/"));
        request.reply(status_codes::NoContent);
    });
    VERIFY_ARE_EQUAL(0, p_client->request(methods::OPTIONS, U("/")));
    p_client->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::NoContent);
    }).wait();
    listener.close().wait();
}

TEST_FIXTURE(uri_address, handle_trace)
{
    http_listener listener(m_uri);
    listener.open().wait();
    test_http_client::scoped_client client(m_uri);
    test_http_client * p_client = client.client();

    VERIFY_ARE_EQUAL(0, p_client->request(methods::TRCE, U("/")));
    p_client->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::OK);
        std::string utf8_response;
        utf8_response.assign(p_response->m_data.begin(), p_response->m_data.end());
#ifdef _MS_WINDOWS
        VERIFY_ARE_EQUAL("TRACE http://localhost:34567/ HTTP/1.1\r\nConnection: Keep-Alive\r\nHost: localhost:34567\r\nUser-Agent: test_http_client\r\n\r\n", utf8_response);
#else
        VERIFY_ARE_EQUAL("TRACE http://localhost:34567/ HTTP/1.1\r\nConnection: Keep-Alive\r\nContent-Length: 0\r\nContent-Type: text/plain; charset=utf-8\r\nHost: localhost:34567\r\nUser-Agent: test_http_client\r\n\r\n", utf8_response);
#endif
    }).wait();

    // try overridding the default OPTIONS handler
    listener.support(methods::TRCE, [](http_request request)
    {
        http_asserts::assert_request_equals(request, methods::TRCE, U("/"));
        request.reply(status_codes::NoContent);
    });
    VERIFY_ARE_EQUAL(0, p_client->request(methods::TRCE, U("/")));
    p_client->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::NoContent);
    }).wait();
    listener.close().wait();
}

TEST_FIXTURE(uri_address, async_request_handler)
{
    http_listener listener(m_uri);
    pplx::extensibility::event_t e;
    listener.support([&e](http_request request)
    {
        e.set();
        request.reply(status_codes::OK).wait();
    });
    listener.open().wait();

    client::http_client client(m_uri);
    auto buf = streams::producer_consumer_buffer<uint8_t>();
    pplx::task<http_response> response = client.request(methods::PUT, U("/"), buf.create_istream(), U("text/plain"));

    e.wait();
    buf.close(std::ios_base::out).wait();
    response.wait();
    listener.close().wait();
}


TEST_FIXTURE(uri_address, multiple_listeners)
{
    http_listener listener1(U("http://localhost:45678/path1"));
    http_listener listener2(U("http://localhost:45678/path1/path2"));
    http_listener listener3(U("http://localhost:45678/path3"));
    listener1.open().wait();
    listener2.open().wait();
    listener3.open().wait();

    test_http_client::scoped_client client(U("http://localhost:45678"));
    test_http_client * p_client = client.client();

    // send a request to the first listener
    listener1.support(methods::GET, [](http_request request)
    {
        http_asserts::assert_request_equals(request, methods::GET, U("/"));
        request.reply(status_codes::NoContent);
    });
    VERIFY_ARE_EQUAL(0, p_client->request(methods::GET, U("/path1")));
    p_client->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::NoContent);
    }).wait();

    // send a request to the second listener
    listener2.support(methods::PUT, [](http_request request)
    {
        http_asserts::assert_request_equals(request, methods::PUT, U("/path4"));
        request.reply(status_codes::OK);
    });
    VERIFY_ARE_EQUAL(0, p_client->request(methods::PUT, U("/path1/path2/path4")));
    p_client->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::OK);
    }).wait();

    // send a request to the third listener
    listener3.support(methods::POST, [](http_request request)
    {
        http_asserts::assert_request_equals(request, methods::POST, U("/"));
        request.reply(status_codes::Created);
    });
    VERIFY_ARE_EQUAL(0, p_client->request(methods::POST, U("/path3")));
    p_client->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::Created);
    }).wait();

    // Remove the second listener and send a request again.
    listener2.close().wait();
    listener1.support(methods::GET, [](http_request request)
    {
        http_asserts::assert_request_equals(request, methods::GET, U("/path2/path4"));
        request.reply(status_codes::OK);
    });
    VERIFY_ARE_EQUAL(0, p_client->request(methods::GET, U("/path1/path2/path4")));
    p_client->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::OK);
    }).wait();

    listener3.close().wait();
    listener1.close().wait();
}

TEST_FIXTURE(uri_address, unregister_while_processing)
{
    http_listener listener1(U("http://localhost:45679/path1"));
    http_listener listener2(U("http://localhost:45679/path1/path2"));
    listener1.open().wait();
    listener2.open().wait();

    test_http_client::scoped_client client1(U("http://localhost:45679"));
    test_http_client * p_client1 = client1.client();
    test_http_client::scoped_client client2(U("http://localhost:45679"));
    test_http_client * p_client2 = client2.client();

    // first listener is used to wait until a request comes into the second
    // and then will try to close the second.
    pplx::extensibility::event_t secondRequest;
    listener1.support(methods::GET, [&](http_request request)
    {
        http_asserts::assert_request_equals(request, methods::GET, U("/"));
        secondRequest.wait();
        listener2.close().wait();
        request.reply(status_codes::OK);
    });
    VERIFY_ARE_EQUAL(0, p_client1->request(methods::GET, U("/path1")));
    listener2.support(methods::GET, [&](http_request request)
    {
        http_asserts::assert_request_equals(request, methods::GET, U("/"));
        secondRequest.set();
        os_utilities::sleep(200);
        request.reply(status_codes::OK);
    });
    VERIFY_ARE_EQUAL(0, p_client2->request(methods::GET, U("/path1/path2/")));
    p_client1->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::OK);
    }).wait();
    p_client2->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::OK);
    }).wait();
    listener1.close().wait();
}

TEST_FIXTURE(uri_address, multiple_requests)
{
    http_listener listener(m_uri);
    listener.open().wait();
    test_http_client::scoped_client client(m_uri);
    test_http_client * p_client = client.client();
    test_http_client::scoped_client client2(m_uri);
    test_http_client * p_client2 = client2.client();
    test_http_client::scoped_client client3(m_uri);
    test_http_client * p_client3 = client3.client();

    volatile unsigned long requestCount = 0;
    listener.support(methods::GET, [&](http_request request)
    {
        http_asserts::assert_request_equals(request, methods::GET, U("/path1"));
        os_utilities::interlocked_increment(&requestCount);
        while(requestCount != 3)
        {
            os_utilities::sleep(1);
        }
        request.reply(status_codes::OK);
    });
    VERIFY_ARE_EQUAL(0, p_client->request(methods::GET, U("/path1")));
    VERIFY_ARE_EQUAL(0, p_client2->request(methods::GET, U("/path1")));
    VERIFY_ARE_EQUAL(0, p_client3->request(methods::GET, U("/path1")));
    p_client->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::OK);
    }).wait();
    p_client2->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::OK);
    }).wait();
    p_client3->next_response().then([](test_response *p_response)
    {
        http_asserts::assert_test_response_equals(p_response, status_codes::OK);
    }).wait();

    listener.close().wait();
}

TEST_FIXTURE(uri_address, multiple_clients_multiple_requests)
{
    http_listener listener(m_uri);
    listener.open().wait();
    const size_t NUM_CLIENTS = 10;
    std::vector<std::unique_ptr<test_http_client>> clients;
    for(size_t i = 0; i < NUM_CLIENTS; ++i)
    {
        std::unique_ptr<test_http_client> client(new test_http_client(m_uri));
        VERIFY_ARE_EQUAL(0, client->open());
        clients.push_back(std::move(client));
    }

    listener.support(methods::GET, [&](http_request request)
    {
        http_asserts::assert_request_equals(request, methods::GET, U("/"));
        request.reply(status_codes::OK);
    });
    for(size_t j = 0; j < 10; ++j)
    {
        std::vector<pplx::task<void>> requests;
        for(size_t i = 0; i < NUM_CLIENTS; ++i)
        {
            VERIFY_ARE_EQUAL(0, clients[i]->request(methods::GET, U("/")));
            requests.push_back(clients[i]->next_response().then([&](test_response *p_response)
            {
                http_asserts::assert_test_response_equals(p_response, status_codes::OK);
            }));
        }
        pplx::when_all(requests.begin(), requests.end()).wait();
    }

    for(size_t i = 0; i < NUM_CLIENTS; ++i)
    {
        VERIFY_ARE_EQUAL(0, clients[i]->close());
    }
    listener.close().wait();
}

TEST_FIXTURE(uri_address, test_leaks)
{
    http_listener listener(m_uri);
    listener.open().wait();
    test_http_client::scoped_client client(m_uri);
    test_http_client * p_client = client.client();

    // pick a large number to see leaks easier
    const size_t nbytes = 1024*1000;

    listener.support(methods::PUT, [&](http_request message)
    {
        while(message.body().streambuf().in_avail() < nbytes);

        utility::string_t request = U("unknown");
        auto it = message.headers().find(U("ClientID"));
        if( it != message.headers().end() )
        {
            message.reply(status_codes::OK, U("Unknown command"));
        }
        else
        {
            message.reply(status_codes::OK, U("ClientID missing"));
        }
    });

    const int N = 1; // use large number of iterations to test for leaks
    for(int i = 0; i < N; ++i)
    {
        std::map<utility::string_t, utility::string_t> headers;
        headers[U("ClientID")] = U("123");
        headers[U("Request")] = U("Upload");
        headers[U("ImgNr")] = U("1");

        utility::char_t* pdata = new utility::char_t[nbytes];

        // this help recognizing the leaked memory in the CRT/VLD dump
        for(int i = 0; i < nbytes; i++) pdata[i] = U('a') + (i % 26);
        std::string data(pdata, pdata + nbytes);
        delete[] pdata;

        VERIFY_ARE_EQUAL(0, p_client->request(methods::PUT, U("/path1"), headers, data));
        p_client->next_response().then([](test_response *p_response)
        {
            http_asserts::assert_test_response_equals(p_response, status_codes::OK);
        }).wait();
    }
    listener.close().wait();
}

}

}}}}
