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
* negative_parsing_tests.cpp
*
* Negative tests for JSON parsing.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web; 
using namespace utility;

namespace tests { namespace functional { namespace json_tests {

SUITE(negative_parsing_tests)
{

TEST(string_t)
{
    VERIFY_THROWS(json::value::parse(U("\"\\k\"")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("\" \" \"")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("\"\\u23A\"")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("\"\\uXY1A\"")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("\"asdf")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("\\asdf")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("\"\"\"\"")), json::json_exception);

    // '\', '"', and control characters must be escaped (0x1F and below).
    VERIFY_THROWS(json::value::parse(U("\"\\\"")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("\"")), json::json_exception);
    utility::string_t str(U("\""));
    str.append(1, 0x1F);
    str.append(U("\""));
    VERIFY_THROWS(json::value::parse(str.c_str()), json::json_exception);
}

TEST(numbers)
{
    VERIFY_THROWS(json::value::parse(U("-")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("-.")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("-e1")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("-1e")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("+1.1")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("1.1 E")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("1.1E-")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("1.1E.1")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("1.1E1.1")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("001.1")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("-.100")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("-.001")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U(".1")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("0.1.1")), json::json_exception);
}

// TFS 535589
json::value parse_help(utility::string_t str)
{
    utility::stringstream_t ss1;
    ss1 << str;
    return json::value::parse(ss1);
}

TEST(objects)
{
    VERIFY_THROWS(json::value::parse(U("}")), json::json_exception);
    VERIFY_THROWS(parse_help(U("{")), json::json_exception);
    VERIFY_THROWS(parse_help(U("{ 1, 10 }")), json::json_exception);
    VERIFY_THROWS(parse_help(U("{ : }")), json::json_exception);
    VERIFY_THROWS(parse_help(U("{ \"}")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("{")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("{ 1")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("{ \"}")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("{\"2\":")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("{\"2\":}")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("{\"2\": true")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("{\"2\": true false")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("{\"2\": true :false")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("{\"2\": false,}")), json::json_exception);
}

TEST(arrays)
{
    VERIFY_THROWS(json::value::parse(U("]")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("[")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("[ 1")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("[ 1,")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("[ 1,]")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("[ 1 2]")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("[ \"1\" : 2]")), json::json_exception);
    VERIFY_THROWS(parse_help(U("[,]")), json::json_exception);
    VERIFY_THROWS(parse_help(U("[ \"]")), json::json_exception);
    VERIFY_THROWS(parse_help(U("[\"2\", false,]")), json::json_exception);
}

TEST(literals_not_lower_case)
{
    VERIFY_THROWS(json::value::parse(U("NULL")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("FAlse")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("TRue")), json::json_exception);
}

TEST(incomplete_literals)
{
    VERIFY_THROWS(json::value::parse(U("nul")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("fal")), json::json_exception);
    VERIFY_THROWS(json::value::parse(U("tru")), json::json_exception);
}

// TFS#501321
TEST(exception_string)
{
    utility::string_t json_ip_str=U("");
    VERIFY_THROWS(json::value::parse(json_ip_str), json::json_exception);
}

TEST(boundary_chars)
{
    utility::string_t str(U("\""));
    str.append(1, 0x1F);
    str.append(U("\""));
    VERIFY_THROWS(parse_help(str), json::json_exception);
}

TEST(stream_left_over_chars)
{
    std::stringbuf buf;
    buf.sputn("[false]false", 12);
    std::istream stream(&buf);
    VERIFY_THROWS(json::value::parse(stream), json::json_exception);
}

// Test using Windows only API.
#ifdef _MS_WINDOWS
TEST(wstream_left_over_chars)
{
    std::wstringbuf buf;
    buf.sputn(L"[false]false", 12);
    std::wistream stream(&buf);
    VERIFY_THROWS(json::value::parse(stream), json::json_exception);
}
#endif

void garbage_impl(wchar_t ch)
{
    utility::string_t ss(U("{\"a\" : 10, \"b\":"));

    std::random_device rd;
    std::mt19937 eng(rd());
    std::uniform_int_distribution<unsigned int> dist(0, ch);

    for (int i = 0; i < 2500; i++)
        ss.push_back(static_cast<char_t>(dist(eng)));

    VERIFY_THROWS(json::value::parse(ss.c_str()), json::json_exception);
}

TEST(garbage_1)
{
    garbage_impl(0x7F);
}

TEST(garbage_2)
{
    garbage_impl(0xFF);
}

TEST(garbage_3)
{
    garbage_impl(0xFFFF);
}

}

}}}