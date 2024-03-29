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
* http_listen.cpp
*
* HTTP Library: HTTP listener (server-side) APIs
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#include "cpprest/http_server_api.h"

using namespace web::http::experimental;

namespace web { namespace http
{
namespace experimental {
namespace listener
{

// Helper function to check URI components.
static void check_listener_uri(const http::uri &address)
{
    // Somethings like proper URI schema are verified by the URI class.
    // We only need to check certain things specific to HTTP.

#ifdef _MS_WINDOWS	
    //HTTP Server API includes SSL support 
    if(address.scheme() != U("http") && address.scheme() != U("https"))
    {
        throw std::invalid_argument("URI scheme must be 'http' or 'https'");
    }
#else
    if(address.scheme() == U("https"))
    {
        throw std::invalid_argument("Listeners using 'https' are not yet supported");
    }

    if(address.scheme() != U("http"))
    {
        throw std::invalid_argument("URI scheme must be 'http'");
    }
#endif

    if(address.host().empty())
    {
        throw std::invalid_argument("URI must contain a hostname.");
    }

    if(!address.query().empty())
    {
        throw std::invalid_argument("URI can't contain a query.");
    }

    if(!address.fragment().empty())
    {
        throw std::invalid_argument("URI can't contain a fragment.");
    }
}

details::http_listener_impl::http_listener_impl(http::uri address)
    : m_uri(std::move(address)), m_closed(true)
{
    check_listener_uri(m_uri);
}

details::http_listener_impl::http_listener_impl(http::uri address, http_listener_config config)
    : m_uri(std::move(address)), m_config(std::move(config)), m_closed(true)
{
    check_listener_uri(m_uri);
}

http_listener::~http_listener()
{
    if(m_impl)
    {
        // As a safe guard close the listener if not already done.
        // Users are required to call close, but this is just a safeguard.
        try
        {
            close().wait();
        } catch(...)
        {
        }
    }
}

pplx::task<void> details::http_listener_impl::open()
{
    // Do nothing if the open operation was already attempted
    // Not thread safe
    if (!m_closed) return pplx::task_from_result();

    if ( m_uri.is_empty() )
        throw std::invalid_argument("No URI defined for listener.");
    m_closed = false;

    return web::http::experimental::details::http_server_api::register_listener(this).then([this](pplx::task<void> openOp)
    {
        try
        {
            // If failed to open need to mark as closed.
            openOp.wait();
        }
        catch(...)
        {
            m_closed = true;
            throw;
        }
        return openOp;
    });
}

pplx::task<void> details::http_listener_impl::close()
{
    // Do nothing if the close operation was already attempted
    // Not thread safe.
    if (m_closed) return pplx::task_from_result();

    m_closed = true;
    return web::http::experimental::details::http_server_api::unregister_listener(this);
}

void details::http_listener_impl::handle_request(http_request msg)
{
    // Specific method handler takes priority over general.
    const method &mtd = msg.method();
    if(m_supported_methods.count(mtd))
    {
        m_supported_methods[mtd](msg);
    }
    else if(mtd == methods::OPTIONS)
    {
        handle_options(msg);
    }
    else if(mtd == methods::TRCE)
    {
        handle_trace(msg);
    }
    else if(m_all_requests != nullptr)
    {
        m_all_requests(msg);
    }
    else
    {
        // Method is not supported. 
        // Send back a list of supported methods to the client.
        http_response response(status_codes::MethodNotAllowed);
        response.headers().add(U("Allow"), get_supported_methods());
        msg.reply(response);
    }
}

utility::string_t details::http_listener_impl::get_supported_methods() const 
{
    utility::string_t allowed;
    bool first = true;
    for (auto iter = m_supported_methods.begin(); iter != m_supported_methods.end(); ++iter)
    {
        if(!first)
        {
            allowed += U(", ");
        }
        else
        {
            first = false;
        }
        allowed += (iter->first);
    }
    return allowed;
}

void details::http_listener_impl::handle_trace(http_request message)
{
    utility::string_t data = message.to_string();
    message.reply(status_codes::OK, data, U("message/http"));
}

void details::http_listener_impl::handle_options(http_request message)
{
    http_response response(status_codes::OK);
    response.headers().add(U("Allow"), get_supported_methods());
    message.reply(response);
}

} // namespace listener
} // namespace experimental
}} // namespace web::http
