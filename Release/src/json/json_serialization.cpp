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
* json_serialization.cpp
*
* HTTP Library: JSON parser and writer
*
* For the latest on this and related APIs, please see http://casablanca.codeplex.com.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#include <stdio.h>

#pragma warning(disable : 4127) // allow expressions like while(true) pass 
using namespace web;
using namespace web::json;
using namespace utility;
using namespace utility::conversions;


//
// JSON Serialization
//

#ifdef _MS_WINDOWS
void web::json::value::serialize(std::ostream& stream) const
{ 
    // This has better performance than writing directly to stream.
    std::string str;
    m_value->serialize_impl(str);
    stream << str; 
}
void web::json::value::format(std::basic_string<wchar_t> &string) const 
{
    m_value->format(string); 
}
#endif

void web::json::value::serialize(utility::ostream_t &stream) const 
{
    // This has better performance than writing directly to stream.
    utility::string_t str;
    m_value->serialize_impl(str);
    stream << str;  
}

void web::json::value::format(std::basic_string<char>& string) const
{
    m_value->format(string);
}

template<typename CharType>
void web::json::details::append_escape_string(std::basic_string<CharType>& str, const std::basic_string<CharType>& escaped)
{
    for (auto iter = escaped.begin(); iter != escaped.end(); ++iter)
    {
        switch (*iter)
        {
            case '\"': 
                str += '\\';
                str += '\"';
                break;
            case '\\': 
                str += '\\';
                str += '\\';
                break;
            case '\b':
                str += '\\';
                str += 'b';
                break;
            case '\f':
                str += '\\';
                str += 'f';
                break;
            case '\r':
                str += '\\';
                str += 'r';
                break;
            case '\n':
                str += '\\';
                str += 'n';
                break;
            case '\t':
                str += '\\';
                str += 't';
                break;
            default:
                str += *iter;
        }
    }
}

void web::json::details::format_string(const utility::string_t& key, utility::string_t& str)
{
    str.push_back('"');
    append_escape_string(str, key);
    str.push_back('"');
}

#ifdef _MS_WINDOWS
void web::json::details::format_string(const utility::string_t& key, std::string& str)
{
    str.push_back('"');
    append_escape_string(str, utility::conversions::to_utf8string(key));
    str.push_back('"');
}
#endif

void web::json::details::_String::format(std::basic_string<char>& str) const
{
    str.push_back('"');

    if(m_has_escape_char)
    {
        append_escape_string(str, utility::conversions::to_utf8string(m_string));
    }
    else
    {
        str.append(utility::conversions::to_utf8string(m_string));
    }

    str.push_back('"');
}

void web::json::details::_Number::format(std::basic_string<char>& stream) const 
{
    if(m_number.m_type != number::type::double_type)
    {
#ifdef _MS_WINDOWS
        // #digits + 1 to avoid loss + 1 for the sign + 1 for null terminator.
        const size_t tempSize = std::numeric_limits<uint64_t>::digits10 + 3;
        char tempBuffer[tempSize];

        // This can be improved performance-wise if we implement our own routine
        if (m_number.m_type == number::type::signed_type)
            _i64toa_s(m_number.m_intval, tempBuffer, tempSize, 10);
        else
            _ui64toa_s(m_number.m_uintval, tempBuffer, tempSize, 10);

        const auto numChars = strnlen_s(tempBuffer, tempSize);
        stream.append(tempBuffer, numChars);
#else
        std::stringstream ss;
        if (m_number.m_type == number::type::signed_type)
            ss << m_number.m_intval;
        else
            ss << m_number.m_uintval;

        stream.append(ss.str());
#endif
    }
    else
    {
        // #digits + 2 to avoid loss + 1 for the sign + 1 for decimal point + 5 for exponent (e+xxx) + 1 for null terminator
        const size_t tempSize = std::numeric_limits<double>::digits10 + 10;
        char tempBuffer[tempSize];
#ifdef _MS_WINDOWS
        const auto numChars = sprintf_s(tempBuffer, tempSize, "%.*g", std::numeric_limits<double>::digits10 + 2, m_number.m_value);
#else
        const auto numChars = snprintf(tempBuffer, tempSize, "%.*g", std::numeric_limits<double>::digits10 + 2, m_number.m_value);
#endif
        stream.append(tempBuffer, numChars);
    }
}

#ifdef _MS_WINDOWS

void web::json::details::_String::format(std::basic_string<wchar_t>& str) const
{
    str.push_back(L'"');

    if(m_has_escape_char)
    {
        append_escape_string(str, m_string);
    }
    else
    {
        str.append(m_string);
    }

    str.push_back(L'"');
}

void web::json::details::_Number::format(std::basic_string<wchar_t>& stream) const
{
    if(m_number.m_type != number::type::double_type)
    {
        // #digits + 1 to avoid loss + 1 for the sign + 1 for null terminator.
        const size_t tempSize = std::numeric_limits<uint64_t>::digits10 + 3;
        wchar_t tempBuffer[tempSize];
        
        if (m_number.m_type == number::type::signed_type)
            _i64tow_s(m_number.m_intval, tempBuffer, tempSize, 10);
        else
            _ui64tow_s(m_number.m_uintval, tempBuffer, tempSize, 10);

        stream.append(tempBuffer, wcsnlen_s(tempBuffer, tempSize));
    }
    else
    {
        // #digits + 2 to avoid loss + 1 for the sign + 1 for decimal point + 5 for exponent (e+xxx) + 1 for null terminator
        const size_t tempSize = std::numeric_limits<double>::digits10 + 10;
        wchar_t tempBuffer[tempSize];
        const int numChars = swprintf_s(tempBuffer, tempSize, L"%.*g", std::numeric_limits<double>::digits10 + 2, m_number.m_value);
        stream.append(tempBuffer, numChars);
    }
}

#endif

utility::string_t web::json::details::_String::as_string() const
{
    return m_string;
}

utility::string_t web::json::value::as_string() const
{
    return m_value->as_string();
}

utility::string_t json::value::serialize() const { return m_value->to_string(); }
