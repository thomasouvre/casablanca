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
* http_windows_server.cpp
*
* HTTP Library: implementation of HTTP server API built on Windows HTTP Server APIs.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#include "cpprest/http_server.h"
#include "cpprest/http_server_api.h"
#include "cpprest/http_windows_server.h"
#include "cpprest/rawptrstream.h"

using namespace web; 
using namespace utility;
using namespace concurrency;
using namespace utility::conversions;
using namespace http::details;
using namespace http::experimental::listener;
using namespace http::experimental::details;

#define CHUNK_SIZE 64 * 1024

namespace web 
{ 
namespace http
{
namespace experimental 
{
namespace details
{

/// <summary>
/// String values for all HTTP Server API known headers.
/// NOTE: the order here is important it is from the _HTTP_HEADER_ID enum.
/// </summary>
static utility::string_t HttpServerAPIKnownHeaders[] =
{
    U("Cache-Control"),
    U("Connection"),
    U("Data"),
    U("Keep-Alive"),
    U("Pragma"),
    U("Trailer"),
    U("Transfer-Encoding"),
    U("Upgrade"),
    U("Via"),
    U("Warning"),
    U("Allow"),
    U("Content-Length"),
    U("Content-Type"),
    U("Content-Encoding"),
    U("Content-Language"),
    U("Content-Location"),
    U("Content-Md5"),
    U("Content-Range"),
    U("Expires"),
    U("Last-Modified"),
    U("Accept"),
    U("Accept-Charset"),
    U("Accept-Encoding"),
    U("Accept-Language"),
    U("Authorization"),
    U("Cookie"),
    U("Expect"),
    U("From"),
    U("Host"),
    U("If-Match"),
    U("If-Modified-Since"),
    U("If-None-Match"),
    U("If-Range"),
    U("If-Unmodified-Since"),
    U("Max-Forwards"),
    U("Proxy-Authorization"),
    U("Referer"),
    U("Range"),
    U("Te"),
    U("Translate"),
    U("User-Agent"),
    U("Request-Maximum"),
    U("Accept-Ranges"),
    U("Age"),
    U("Etag"),
    U("Location"),
    U("Proxy-Authenticate"),
    U("Retry-After"),
    U("Server"),
    U("Set-Cookie"),
    U("Vary"),
    U("Www-Authenticate"),
    U("Response-Maximum")
};

static void char_to_wstring(utf16string &dest, const char * src)
{
    dest = utility::conversions::to_utf16string(std::string(src));
}

http::method parse_request_method(const HTTP_REQUEST *p_request)
{
    http::method method;

    switch(p_request->Verb)
    {
    case HttpVerbGET:
        method = methods::GET;
        break;
    case HttpVerbPOST:
        method = methods::POST;
        break;
    case HttpVerbPUT:
        method = methods::PUT;
        break;
    case HttpVerbDELETE:
        method = methods::DEL;
        break;
    case HttpVerbHEAD:
        method = methods::HEAD;
        break;
    case HttpVerbOPTIONS:
        method = methods::OPTIONS;
        break;
    case HttpVerbTRACE:
        method = methods::TRCE;
        break;
    case HttpVerbCONNECT:
        method = methods::CONNECT;
        break;
    case HttpVerbUnknown:
        char_to_wstring(method, p_request->pUnknownVerb);
        break;
    case HttpVerbMOVE:
        method = _XPLATSTR("MOVE");
        break;
    case HttpVerbCOPY:
        method = _XPLATSTR("COPY");
        break;
    case HttpVerbPROPFIND:
        method = _XPLATSTR("PROPFIND");
        break;
    case HttpVerbPROPPATCH:
        method = _XPLATSTR("PROPPATCH");
        break;
    case HttpVerbMKCOL:
        method = _XPLATSTR("MKCOL");
        break;
    case HttpVerbLOCK:
        method = _XPLATSTR("LOCK");
        break;
    case HttpVerbUNLOCK:
        method = _XPLATSTR("UNLOCK");
        break;
    case HttpVerbSEARCH:
        method = _XPLATSTR("SEARCH");
        break;
    default:
        break;
    }
    return method;
}

void parse_http_headers(const HTTP_REQUEST_HEADERS &headers, http::http_headers & msgHeaders)
{
    //
    // This is weird for the 'KnownHeaders' but there is no way I can find with the HTTP Server API
    // to get all the raw headers. The known ones are stored in an array index by a HTTP Server API
    // enumeration.
    //
    // TFS 354587 As a perf optimization we could parse the headers from Windows on demand in the
    //      http_header class itself.
    for(USHORT i = 0; i < headers.UnknownHeaderCount; ++i)
    {
        utf16string unknown_header_name;
        char_to_wstring(unknown_header_name, headers.pUnknownHeaders[i].pName);

        // header value can be empty
        if(headers.pUnknownHeaders[i].RawValueLength > 0)
        {
            msgHeaders.add(unknown_header_name, utility::conversions::to_utf16string(headers.pUnknownHeaders[i].pRawValue));
        }
        else
        {
            msgHeaders[unknown_header_name] = U("");
        }
    }
    for(int i = 0; i < HttpHeaderMaximum; ++i)
    {
        if(headers.KnownHeaders[i].RawValueLength > 0)
        {
            msgHeaders.add(HttpServerAPIKnownHeaders[i], utility::conversions::to_utf16string(headers.KnownHeaders[i].pRawValue));
        }
    }
}

http_windows_server::http_windows_server()
{
    HTTPAPI_VERSION httpApiVersion = HTTPAPI_VERSION_2;
    HttpInitialize(httpApiVersion, HTTP_INITIALIZE_SERVER, NULL);
}

http_windows_server::~http_windows_server()
{
    HttpTerminate(HTTP_INITIALIZE_SERVER, NULL);
}

pplx::task<void> http_windows_server::register_listener(_In_ web::http::experimental::listener::details::http_listener_impl *pListener)
{
    unsigned long errorCode;

    // Create a url group for this listener.
    HTTP_URL_GROUP_ID urlGroupId;
    errorCode = HttpCreateUrlGroup(m_serverSessionId, &urlGroupId, 0);
    if(errorCode != NO_ERROR)
    {
        return pplx::task_from_exception<void>(http_exception(errorCode));
    }

    // Add listener's URI to the new group.
    http::uri u = pListener->uri();
    if (u.is_port_default())
    {
        // Windows HTTP Server API has issues when the port isn't set to 80 here -- it expects a url prefix string
        // which always includes the port number
        // http://msdn.microsoft.com/en-us/library/windows/desktop/aa364698(v=vs.85).aspx
        http::uri_builder builder(u);
        builder.set_port(80);
        u = builder.to_uri();
    }

    // Windows HTTP Server API will not accept a uri with an empty path, it must have a '/'.
    // Windows HTTP Server API will only accept decoded uri strings.
    utility::string_t host_uri = http::uri::decode(u.to_string());
    if(host_uri.back() != U('/') && u.query().empty() && u.fragment().empty())
    {
        host_uri.append(U("/"));
    }

    // inside here we check for a few specific error types that know about
    // there may be more possibilities for windows to return a different error
    errorCode = HttpAddUrlToUrlGroup(urlGroupId, host_uri.c_str(), (HTTP_URL_CONTEXT)pListener, 0);
    if(errorCode)
    {
        HttpCloseUrlGroup(urlGroupId);
        utility::stringstream_t os;

        if(errorCode == ERROR_ALREADY_EXISTS || errorCode == ERROR_SHARING_VIOLATION)
        {
            os << _XPLATSTR("Address '") << pListener->uri().to_string() << _XPLATSTR("' is already in use");
            return pplx::task_from_exception<void>(http_exception(errorCode, os.str()));
        }
        else if (errorCode == ERROR_ACCESS_DENIED)
        {
            os << _XPLATSTR("Access denied: attempting to add Address '") << pListener->uri().to_string() << _XPLATSTR("'. ");
            os << _XPLATSTR("Run as administrator to listen on an hostname other than localhost, or to listen on port 80.");
            return pplx::task_from_exception<void>(http_exception(errorCode, os.str()));
        }
        else
        {
            return pplx::task_from_exception<void>(http_exception(errorCode, _XPLATSTR("Error adding url to url group")));
        }
    }

    // Set timeouts.
    HTTP_TIMEOUT_LIMIT_INFO timeouts;
    const USHORT secs = static_cast<USHORT>(pListener->configuration().timeout().count());
    timeouts.EntityBody = secs;
    timeouts.DrainEntityBody = secs;
    timeouts.RequestQueue = secs;
    timeouts.IdleConnection = secs;
    timeouts.HeaderWait = secs;
    timeouts.Flags.Present = 1;
    errorCode = HttpSetUrlGroupProperty(
        urlGroupId, 
        HttpServerTimeoutsProperty,
        &timeouts,
        sizeof(HTTP_TIMEOUT_LIMIT_INFO));
    if(errorCode)
    {
        HttpCloseUrlGroup(urlGroupId);
        return pplx::task_from_exception<void>(http_exception(errorCode));
    }

    // Add listener registration.
    {
        pplx::extensibility::scoped_rw_lock_t lock(_M_listenersLock);
        if(_M_registeredListeners.find(pListener) != _M_registeredListeners.end())
        {
            HttpCloseUrlGroup(urlGroupId);
            throw std::invalid_argument("Error: http_listener is already registered");
        }
        _M_registeredListeners[pListener] = std::unique_ptr<listener_registration>(new listener_registration(urlGroupId));
    }

    // Associate Url group with request queue.
    HTTP_BINDING_INFO bindingInfo;
    bindingInfo.RequestQueueHandle = m_hRequestQueue;
    bindingInfo.Flags.Present = 1;
    errorCode = HttpSetUrlGroupProperty(
        urlGroupId,
        HttpServerBindingProperty,
        &bindingInfo,
        sizeof(HTTP_BINDING_INFO));
    if(errorCode)
    {
        HttpCloseUrlGroup(urlGroupId);
        return pplx::task_from_exception<void>(http_exception(errorCode));
    }

    return pplx::task_from_result();
}

pplx::task<void> http_windows_server::unregister_listener(_In_ web::http::experimental::listener::details::http_listener_impl *pListener)
{
    return pplx::create_task([=]()
    {
        // First remove listener registration.
        std::unique_ptr<listener_registration> registration;
        {
            pplx::extensibility::scoped_rw_lock_t lock(_M_listenersLock);
            registration = std::move(_M_registeredListeners[pListener]);
            _M_registeredListeners[pListener] = nullptr;
            _M_registeredListeners.erase(pListener);
        }

        // Then take the listener write lock to make sure there are no calls into the listener's
        // request handler.
        {
            pplx::extensibility::scoped_rw_lock_t lock(registration->m_requestHandlerLock);
        }

        // Next close Url group, no need to remove individual Urls.
        const unsigned long error_code = HttpCloseUrlGroup(registration->m_urlGroupId);
        if (error_code != NO_ERROR)
        {
            throw http_exception(error_code);
        }
    });
}

pplx::task<void> http_windows_server::start()
{
    // Initialize data.
    m_serverSessionId = 0; 
    m_hRequestQueue = nullptr;
    m_threadpool_io = nullptr;
    m_numOutstandingRequests = 0;
    m_zeroOutstandingRequests.set();

    // Open server session.
    HTTPAPI_VERSION httpApiVersion = HTTPAPI_VERSION_2;
    ULONG errorCode = HttpCreateServerSession(httpApiVersion, &m_serverSessionId, 0);
    if(errorCode)
    {
        return pplx::task_from_exception<void>(http_exception(errorCode));
    }

    // Create request queue.
    errorCode = HttpCreateRequestQueue(httpApiVersion, NULL, NULL, NULL, &m_hRequestQueue);
    if(errorCode)
    {
        return pplx::task_from_exception<void>(http_exception(errorCode));
    }

    // Create and start ThreadPool I/O so we can process asynchronous I/O.
    m_threadpool_io = CreateThreadpoolIo(m_hRequestQueue, &http_overlapped::io_completion_callback, NULL, NULL);
    if(m_threadpool_io == nullptr)
    {
        return pplx::task_from_exception<void>(http_exception(errorCode));
    }

    // Start request receiving task.
    m_receivingTask = pplx::create_task([this]() { receive_requests(); });

    return pplx::task_from_result();
}

pplx::task<void> http_windows_server::stop()
{
    // Shutdown request queue.
    if(m_hRequestQueue != nullptr)
    {
        HttpShutdownRequestQueue(m_hRequestQueue);
        m_receivingTask.wait();

        // Wait for all requests to be finished processing.
        m_zeroOutstandingRequests.wait();

        HttpCloseRequestQueue(m_hRequestQueue);
    }

    // Release resources.
    if(m_serverSessionId != 0) 
    {
        HttpCloseServerSession(m_serverSessionId);
    }
    if(m_threadpool_io != nullptr)
    {
        CloseThreadpoolIo(m_threadpool_io);
        m_threadpool_io = nullptr;
    }

    return pplx::task_from_result();
}

void http_windows_server::receive_requests()
{
    HTTP_REQUEST p_request;
    ULONG bytes_received;
    for(;;)
    {
        unsigned long error_code = HttpReceiveHttpRequest(
            m_hRequestQueue,
            HTTP_NULL_ID,
            0,
            &p_request,
            sizeof(HTTP_REQUEST),
            &bytes_received,
            0);

        if(error_code != NO_ERROR && error_code != ERROR_MORE_DATA)
        {
            break;
        }

        // Start processing the request
        auto pContext = new windows_request_context();
        auto pRequestContext = std::unique_ptr<_http_server_context>(pContext);
        http_request msg = http_request::_create_request(std::move(pRequestContext));
        pContext->async_process_request(p_request.RequestId, msg, bytes_received);
    }
}

pplx::task<void> http_windows_server::respond(http::http_response response)
{
    windows_request_context * p_context = static_cast<windows_request_context *>(response._get_server_context());
    return pplx::create_task(p_context->m_response_completed).then([p_context](::pplx::task<void> t)
    {
        // After response is sent, break circular reference between http_response and the request context.
        // Otherwise http_listener::close() can hang.
        p_context->m_response._get_impl()->_set_server_context(nullptr);
        t.get();
    });
}

windows_request_context::windows_request_context() 
    : m_sending_in_chunks(false),
      m_transfer_encoding(false),
      m_remaining_to_write(0)
{
    auto *pServer = static_cast<http_windows_server *>(http_server_api::server_api());
    if(++pServer->m_numOutstandingRequests == 1)
    {
        pServer->m_zeroOutstandingRequests.reset();
    }
}

windows_request_context::~windows_request_context()
{
    // Unfortunately have to work around a ppl task_completion_event bug that can cause AVs.
    // Bug is that task_completion_event accesses internal state after setting.
    // Workaround is to use a lock incurring additional synchronization, if can acquire
    // the lock then setting of the event has completed.
    std::unique_lock<std::mutex> lock(m_responseCompletedLock);

    auto *pServer = static_cast<http_windows_server *>(http_server_api::server_api());
    if(--pServer->m_numOutstandingRequests == 0)
    {
        pServer->m_zeroOutstandingRequests.set();
    }
}

void windows_request_context::async_process_request(HTTP_REQUEST_ID request_id, http_request msg, const unsigned long headers_size)
{
    auto *pServer = static_cast<http_windows_server *>(http_server_api::server_api());
    m_request_id = request_id;

    // Save the http_request as the member of windows_request_context for the callback use.
    m_msg = msg;

    m_request_buffer = std::unique_ptr<unsigned char[]>(new unsigned char[SafeInt<unsigned long>(headers_size)]);
    m_request = (HTTP_REQUEST *) m_request_buffer.get();

    // The read_headers_io_completion callback function.
    m_overlapped.set_http_io_completion([this](DWORD error, DWORD nBytes){ read_headers_io_completion(error, nBytes); });

    StartThreadpoolIo(pServer->m_threadpool_io);

    const unsigned long error_code = HttpReceiveHttpRequest(
            pServer->m_hRequestQueue,
            m_request_id,
            0,
            m_request,
            headers_size,
            NULL,
            &m_overlapped);
    
    if(error_code != NO_ERROR && error_code != ERROR_IO_PENDING)
    {
        CancelThreadpoolIo(pServer->m_threadpool_io);
        m_msg.reply(status_codes::InternalError);
    }
}

/// <summary>
///  The read request headers completion callback function.
/// </summary>
void windows_request_context::read_headers_io_completion(DWORD error_code, DWORD)
{
    if(error_code != NO_ERROR)
    {
        m_msg.reply(status_codes::InternalError);
    }
    else
    {
        // Parse headers.
        // CookedUrl.pFullUrl contains the canonicalized URL and it is not encoded
        // However, Query strings are opaque to http.sys and are passed as-is => CookedUrl.pFullUrl
        // contains an already encoded version of query string.
        uri_builder builder(uri::encode_uri(m_request->CookedUrl.pFullUrl));
        if (m_request->CookedUrl.QueryStringLength != 0)
        {
            builder.set_query(uri::decode(builder.query()));
        }
        m_msg.set_request_uri(builder.to_uri());
        m_msg.set_method(parse_request_method(m_request));
        parse_http_headers(m_request->Headers, m_msg.headers());

        // Start reading in body from the network.
        m_msg._get_impl()->_prepare_to_receive_data();
        read_request_body_chunk();

        // Dispatch request to the http_listener.
        dispatch_request_to_listener((web::http::experimental::listener::details::http_listener_impl *)m_request->UrlContext);
    }
}

void windows_request_context::read_request_body_chunk()
{
    auto *pServer = static_cast<http_windows_server *>(http_server_api::server_api());

    // The read_body_io_completion callback function
    m_overlapped.set_http_io_completion([this](DWORD error, DWORD nBytes){ read_body_io_completion(error, nBytes);});

    auto request_body_buf = m_msg._get_impl()->outstream().streambuf();
    auto body = request_body_buf.alloc(CHUNK_SIZE);

    // Once we allow users to set the output stream the following assert could fail.
    // At that time we would need compensation code that would allocate a buffer from the heap instead.
    _ASSERTE(body != nullptr);

    StartThreadpoolIo(pServer->m_threadpool_io);
    const ULONG error_code = HttpReceiveRequestEntityBody(
        pServer->m_hRequestQueue,
        m_request_id,
        HTTP_RECEIVE_REQUEST_ENTITY_BODY_FLAG_FILL_BUFFER,
        (PVOID)body,
        CHUNK_SIZE,
        NULL,
        &m_overlapped);

    if(error_code != ERROR_IO_PENDING && error_code != NO_ERROR)
    {
        // There was no more data to read.
        CancelThreadpoolIo(pServer->m_threadpool_io);
        request_body_buf.commit(0);
        if(error_code == ERROR_HANDLE_EOF)
        {
            m_msg._get_impl()->_complete(request_body_buf.in_avail());
        } 
        else
        {
            m_msg._get_impl()->_complete(0, std::make_exception_ptr(http_exception(error_code)));
        }
    }
}

/// <summary>
///  The read request body completion callback function.
/// </summary>
void windows_request_context::read_body_io_completion(DWORD error_code, DWORD bytes_read)
{
    auto request_body_buf = m_msg._get_impl()->outstream().streambuf();

    if (error_code == NO_ERROR)
    {
        request_body_buf.commit(bytes_read);
        read_request_body_chunk();
    }
    else if (error_code == ERROR_HANDLE_EOF)
    {
        request_body_buf.commit(0);
        m_msg._get_impl()->_complete(request_body_buf.in_avail());
    }
    else
    {
        request_body_buf.commit(0);
        m_msg._get_impl()->_complete(0, std::make_exception_ptr(http_exception(error_code)));
    }
}

void windows_request_context::dispatch_request_to_listener(_In_ web::http::experimental::listener::details::http_listener_impl *pListener)
{
    m_msg._set_listener_path(pListener->uri().path());
    
    // Save http_request copy to dispatch to user's handler in case content_ready() completes before.
    http_request request = m_msg;

    // Wait until the content download finished before replying.
    request.content_ready().then([=](pplx::task<http_request> requestBody)
    {
        // If an exception occurred while processing the body then there is no reason
        // to even try sending the response, just re-surface the same exception.
        try
        {
            requestBody.wait();
        }
        catch (...)
        {
            m_msg = http_request();
            cancel_request(std::current_exception());
            return;
        }

        // At this point the user entirely controls the lifetime of the http_request.
        m_msg = http_request();

        request.get_response().then([this](pplx::task<http::http_response> responseTask)
        {
            // Don't let an exception from sending the response bring down the server.
            try
            {
                m_response = responseTask.get();
            }
            catch (const pplx::task_canceled &)
            {
                // This means the user didn't respond to the request, allowing the
                // http_request instance to be destroyed. There is nothing to do then
                // so don't send a response.
                return;
            }
            catch (...)
            {
                m_response = http::http_response(status_codes::InternalError);
            }
            async_process_response();
        });
    });

    // Look up the lock for the http_listener.
    auto *pServer = static_cast<http_windows_server *>(http_server_api::server_api());
    pplx::extensibility::reader_writer_lock_t *pListenerLock;
    {
        pplx::extensibility::scoped_read_lock_t lock(pServer->_M_listenersLock);

        // It is possible the listener could have unregistered.
        if(pServer->_M_registeredListeners.find(pListener) == pServer->_M_registeredListeners.end())
        {
            request.reply(status_codes::NotFound);
            return;
        }
        pListenerLock = &pServer->_M_registeredListeners[pListener]->m_requestHandlerLock;

        // We need to acquire the listener's lock before releasing the registered listeners lock.
        // But we don't need to hold the registered listeners lock when calling into the user's code.
        pListenerLock->lock_read();
    }

    try
    {
        pListener->handle_request(request);
        pListenerLock->unlock();
    } 
    catch(...)
    {
        pListenerLock->unlock();
        request._reply_if_not_already(status_codes::InternalError);
    }
}

void windows_request_context::async_process_response()
{
    auto *pServer = static_cast<http_windows_server *>(http_server_api::server_api());

    HTTP_RESPONSE win_api_response;
    ZeroMemory(&win_api_response, sizeof(win_api_response));
    win_api_response.StatusCode = m_response.status_code();
    const std::string reason = utf16_to_utf8(m_response.reason_phrase());
    win_api_response.pReason = reason.c_str();
    win_api_response.ReasonLength = (USHORT)reason.size();

    size_t content_length = m_response._get_impl()->_get_content_length();

    m_headers = std::unique_ptr<HTTP_UNKNOWN_HEADER []>(new HTTP_UNKNOWN_HEADER[SafeSize(m_response.headers().size())]);
    m_headers_buffer.resize(SafeSize(m_response.headers().size()) * 2);

    win_api_response.Headers.UnknownHeaderCount = (USHORT)m_response.headers().size();
    win_api_response.Headers.pUnknownHeaders = m_headers.get();
    int headerIndex = 0;
    for(auto iter = m_response.headers().begin(); iter != m_response.headers().end(); ++iter, ++headerIndex)
    {
        m_headers_buffer[headerIndex * 2] = utf16_to_utf8(iter->first);
        m_headers_buffer[headerIndex * 2 + 1] = utf16_to_utf8(iter->second);
        win_api_response.Headers.pUnknownHeaders[headerIndex].NameLength = (USHORT)m_headers_buffer[headerIndex * 2].size();
        win_api_response.Headers.pUnknownHeaders[headerIndex].pName = m_headers_buffer[headerIndex * 2].c_str();
        win_api_response.Headers.pUnknownHeaders[headerIndex].RawValueLength = (USHORT)m_headers_buffer[headerIndex * 2 + 1].size();
        win_api_response.Headers.pUnknownHeaders[headerIndex].pRawValue = m_headers_buffer[headerIndex * 2 + 1].c_str();
    }

    // Send response callback function
    m_overlapped.set_http_io_completion([this](DWORD error, DWORD nBytes){ send_response_io_completion(error, nBytes);});

    m_remaining_to_write = content_length;

    // Figure out how to send the entity body of the message.
    if (content_length == 0)
    {
        // There's no data. This is easy!
        StartThreadpoolIo(pServer->m_threadpool_io);
        const unsigned long error_code = HttpSendHttpResponse(
            pServer->m_hRequestQueue,
            m_request_id,
            NULL,
            &win_api_response,
            NULL,
            NULL,
            NULL,
            NULL,
            &m_overlapped,
            NULL);

        if(error_code != NO_ERROR && error_code != ERROR_IO_PENDING)
        {
            CancelThreadpoolIo(pServer->m_threadpool_io);
            cancel_request(std::make_exception_ptr(http_exception(error_code)));
        }

        return;
    }

    // OK, so we need to chunk it up.
    _ASSERTE(content_length > 0);
    m_sending_in_chunks = (content_length != std::numeric_limits<size_t>::max());
    m_transfer_encoding = (content_length == std::numeric_limits<size_t>::max());

    StartThreadpoolIo(pServer->m_threadpool_io);
    const unsigned long error_code = HttpSendHttpResponse(
        pServer->m_hRequestQueue,
        m_request_id,
        HTTP_SEND_RESPONSE_FLAG_MORE_DATA,
        &win_api_response,
        NULL,
        NULL,
        NULL,
        NULL,
        &m_overlapped,
        NULL);

    if(error_code != NO_ERROR && error_code != ERROR_IO_PENDING)
    {
        CancelThreadpoolIo(pServer->m_threadpool_io);
        cancel_request(std::make_exception_ptr(http_exception(error_code)));
    }
}

/// <summary>
///  The send response headers completion callback function.
/// </summary>
void windows_request_context::send_response_io_completion(DWORD error_code, DWORD)
{
    if(error_code != NO_ERROR)
    {
        cancel_request(std::make_exception_ptr(http_exception(error_code)));
    }
    else
    {
        transmit_body();
    }
}

// Transmit the response body to the network
void windows_request_context::transmit_body()
{
    if ( !m_sending_in_chunks && !m_transfer_encoding )
    {
        // We are done sending data.
        std::unique_lock<std::mutex> lock(m_responseCompletedLock);
        m_response_completed.set();
        return;
    }
    
    // In both cases here we could perform optimizations to try and use acquire on the streams to avoid an extra copy.
    if ( m_sending_in_chunks )
    {
        SafeSize safeCount = m_remaining_to_write;
        size_t next_chunk_size = safeCount.Min(CHUNK_SIZE);
        m_body_data.resize(CHUNK_SIZE);

        streams::rawptr_buffer<unsigned char> buf(&m_body_data[0], next_chunk_size);

        m_response.body().read(buf, next_chunk_size).then([this](pplx::task<size_t> op)
        {
            size_t bytes_read = 0;
            
            // If an exception occurs surface the error to user on the server side
            // and cancel the request so the client sees the error.
            try { bytes_read = op.get(); } catch (...)
            {
                cancel_request(std::current_exception());
                return;
            }
            if ( bytes_read == 0 )
            {
                cancel_request(std::make_exception_ptr(http_exception(_XPLATSTR("Error unexpectedly encountered the end of the response stream early"))));
                return;
            }

            // Check whether this is the last one to send...
            m_remaining_to_write = m_remaining_to_write-bytes_read;
            m_sending_in_chunks = (m_remaining_to_write > 0);

            send_entity_body(&m_body_data[0], bytes_read);
        });
    }
    else
    {
        // We're transfer-encoding...
        const size_t body_data_length = CHUNK_SIZE+http::details::chunked_encoding::additional_encoding_space;
        m_body_data.resize(body_data_length);

        streams::rawptr_buffer<unsigned char> buf(&m_body_data[http::details::chunked_encoding::data_offset], body_data_length);

        m_response.body().read(buf, CHUNK_SIZE).then([this, body_data_length](pplx::task<size_t> op)
        {
            size_t bytes_read = 0;
            
            // If an exception occurs surface the error to user on the server side
            // and cancel the request so the client sees the error.
            try 
            { 
                bytes_read = op.get();
            } catch (...)
            {
                cancel_request(std::current_exception());
                return;
            }

            // Check whether this is the last one to send...
            m_transfer_encoding = (bytes_read > 0);
            size_t offset = http::details::chunked_encoding::add_chunked_delimiters(&m_body_data[0], body_data_length, bytes_read);

            auto data_length = bytes_read + (http::details::chunked_encoding::additional_encoding_space-offset);
            send_entity_body(&m_body_data[offset], data_length);
        });
    }
}

// Send the body through HTTP.sys
void windows_request_context::send_entity_body(_In_reads_(data_length) unsigned char * data, _In_ size_t data_length)
{
    HTTP_DATA_CHUNK dataChunk;
    memset(&dataChunk, 0, sizeof(dataChunk));
    dataChunk.DataChunkType = HttpDataChunkFromMemory;
    dataChunk.FromMemory.pBuffer = data;
    dataChunk.FromMemory.BufferLength = (ULONG)data_length;
    const bool this_is_the_last_chunk = !m_transfer_encoding && !m_sending_in_chunks;

    // Send response.
    auto *pServer = static_cast<http_windows_server *>(http_server_api::server_api());

    // Send response body callback function
    m_overlapped.set_http_io_completion([this](DWORD error, DWORD nBytes){ send_response_body_io_completion(error, nBytes); });

    StartThreadpoolIo(pServer->m_threadpool_io);
    auto error_code = HttpSendResponseEntityBody(
        pServer->m_hRequestQueue,
        m_request_id,
        this_is_the_last_chunk ? NULL : HTTP_SEND_RESPONSE_FLAG_MORE_DATA,
        1,
        &dataChunk,
        NULL,
        NULL,
        NULL,
        &m_overlapped,
        NULL);

    if(error_code != NO_ERROR && error_code != ERROR_IO_PENDING)
    {
        CancelThreadpoolIo(pServer->m_threadpool_io);
        cancel_request( std::make_exception_ptr(http_exception(error_code)));
    }
}

/// <summary>
///  The send response body completion callback function.
/// </summary>
void windows_request_context::send_response_body_io_completion(DWORD error_code, DWORD)
{
    if(error_code != NO_ERROR)
    {
        cancel_request(std::make_exception_ptr(http_exception(error_code)));
        return;
    }
    transmit_body();
}

/// <summary>
///  The cancel request completion callback function.
/// </summary>
void windows_request_context::cancel_request_io_completion(DWORD, DWORD)
{
    std::unique_lock<std::mutex> lock(m_responseCompletedLock);
    m_response_completed.set_exception(m_except_ptr);
}

void windows_request_context::cancel_request(std::exception_ptr except_ptr)
{
    auto *pServer = static_cast<http_windows_server *>(http_server_api::server_api());

    m_except_ptr = except_ptr;

    // Cancel request callback function. 
    m_overlapped.set_http_io_completion([this](DWORD error, DWORD nBytes){cancel_request_io_completion(error, nBytes);});

    StartThreadpoolIo(pServer->m_threadpool_io);

    auto error_code = HttpCancelHttpRequest(
        pServer->m_hRequestQueue,
        m_request_id, 
        &m_overlapped);
    
    if(error_code != NO_ERROR && error_code != ERROR_IO_PENDING)
    {
        CancelThreadpoolIo(pServer->m_threadpool_io);
        std::unique_lock<std::mutex> lock(m_responseCompletedLock);
        m_response_completed.set_exception(except_ptr);
    }
}

} // namespace details
} // namespace experimental
}} // namespace web::http
