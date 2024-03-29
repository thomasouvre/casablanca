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
* authentication_tests.cpp
*
* Tests cases for covering authentication using websocket_client
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

#if defined(__cplusplus_winrt) || !defined(_M_ARM)

using namespace web::experimental::web_sockets;
using namespace web::experimental::web_sockets::client;

using namespace tests::functional::websocket::utilities;

namespace tests { namespace functional { namespace websocket { namespace client {

SUITE(authentication_tests)
{

void auth_helper(test_websocket_server& server, utility::string_t username = U(""), utility::string_t password = U(""))
{
    server.set_http_handler([username, password](test_http_request& request)
    {
        test_http_response resp;
        if (request.username().empty()) // No credentials -> challenge the request 
        {
            resp.set_status_code(401); // Unauthorized.
            resp.set_realm("My Realm");
        }
        else if (request.username().compare(utility::conversions::to_utf8string(username))
            || request.password().compare(utility::conversions::to_utf8string(password)))
        {
            resp.set_status_code(403); // User name/password did not match: Forbidden - auth failure.
        }
        else
        {
            resp.set_status_code(200); // User name and passwords match. Successful auth.
        }
        return resp;
    });
}

// connect without credentials, when the server expects credentials
TEST_FIXTURE(uri_address, auth_no_credentials, "Ignore:Linux", "NYI", "Ignore:Apple", "NYI")
{
    test_websocket_server server;
    websocket_client client;
    auth_helper(server);
    VERIFY_THROWS(client.connect(m_uri).wait(), websocket_exception);
}

// Connect with credentials
TEST_FIXTURE(uri_address, auth_with_credentials, "Ignore:Linux", "NYI", "Ignore:Apple", "NYI")
{
    test_websocket_server server;
    websocket_client_config config;   
    web::credentials cred(U("user"), U("password"));
    config.set_credentials(cred);
    websocket_client client(config);

    auth_helper(server, cred.username(), cred.password());
    client.connect(m_uri).wait();
    client.close().wait();
}

// Send and receive text message over SSL
TEST_FIXTURE(uri_address, ssl_test, "Ignore", "NYI on desktop/apple/linux/android")
{
    websocket_client client;
    client.connect(U("wss://echo.websocket.org/")).wait();
    std::string body_str("hello");

    auto receive_task = client.receive().then([body_str](websocket_incoming_message ret_msg)
    {
        auto ret_str = ret_msg.extract_string().get();

        VERIFY_ARE_EQUAL(ret_msg.length(), body_str.length());
        VERIFY_ARE_EQUAL(body_str.compare(ret_str), 0);
        VERIFY_ARE_EQUAL(ret_msg.messge_type(), websocket_message_type::text_message);
    });

    websocket_outgoing_message msg;
    msg.set_utf8_message(body_str);
    client.send(msg).wait();

    receive_task.wait();
    client.close().wait();
}

} // SUITE(authentication_tests)

}}}}

#endif
