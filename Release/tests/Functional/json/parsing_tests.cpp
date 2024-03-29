﻿/***
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
* parsing_tests.cpp
*
* Tests for JSON parsing.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#include <array>

#if defined(_MS_WINDOWS) || defined(__APPLE__)
#include <regex>
#elif defined(ANDROID)
#else
// GCC 4.8 doesn't support regex very well, fall back to Boost. Revist in GCC 4.9.
#include <boost/regex.hpp>
#endif

using namespace web; using namespace utility;
using namespace utility::conversions;

namespace tests { namespace functional { namespace json_tests {

    inline bool verify_parsing_error_msg(const std::string &str)
    {
        auto spattern = "^\\* Line \\d+, Column \\d+ Syntax error: .+";
#if defined(_MS_WINDOWS) || defined(__APPLE__)
        static std::regex pattern(spattern);
        return std::regex_match(str, pattern, std::regex_constants::match_flag_type::match_not_null);
#elif defined(ANDROID)
	return str.find("Syntax error: ") != std::string::npos;
#else
        static boost::regex pattern(spattern);
        return boost::regex_match(str, pattern, boost::regex_constants::match_flag_type::match_not_null);
#endif
    }

#pragma warning (disable: 4127) // const expression
#define VERIFY_PARSING_THROW(target) \
do { \
    try { \
        target; \
        VERIFY_IS_TRUE(false); \
    } \
    catch (const json::json_exception &e) { \
        VERIFY_IS_TRUE(verify_parsing_error_msg(e.what())); \
    } \
    catch (...) { \
        VERIFY_IS_TRUE(false); \
    } \
} while (false)

SUITE(parsing_tests)
{

TEST(stringstream_t)
{
    utility::stringstream_t ss0;
    ss0 << U("null");
    json::value v0 = json::value::parse(ss0);

    utility::stringstream_t ss1;
    ss1 << U("17");
    json::value v1 = json::value::parse(ss1);

    utility::stringstream_t ss2;
    ss2 << U("3.1415");
    json::value v2 = json::value::parse(ss2);

    utility::stringstream_t ss3;
    ss3 << U("true");
    json::value v3 = json::value::parse(ss3);

    utility::stringstream_t ss4;
    ss4 << U("\"Hello!\"");
    json::value v4 = json::value::parse(ss4);

    utility::stringstream_t ss8;
    ss8 << U("{ \"a\" : 10 }");
    json::value v8 = json::value::parse(ss8);

    utility::stringstream_t ss9;
    ss9 << U("[1,2,3,true]");
    json::value v9 = json::value::parse(ss9);

    VERIFY_ARE_EQUAL(v1.type(), json::value::Number);
    VERIFY_ARE_EQUAL(v2.type(), json::value::Number);
    VERIFY_ARE_EQUAL(v3.type(), json::value::Boolean);
    VERIFY_ARE_EQUAL(v4.type(), json::value::String);
    VERIFY_ARE_EQUAL(v8.type(), json::value::Object);
    VERIFY_ARE_EQUAL(v9.type(), json::value::Array);
}

TEST(whitespace_failure)
{
    VERIFY_PARSING_THROW(json::value::parse(U("  ")));
}

static const std::array<char, 4> whitespace_chars = { 0x20, 0x09, 0x0A, 0x0D };

TEST(whitespace_array)
{
    // Try all the whitespace characters before/after all the structural characters
    // whitespace characters according to RFC4627: space, horizontal tab, line feed or new line, carriage return
    // structural characters: [{]}:,

    // [,]
    for(auto ch : whitespace_chars)
    {
        utility::string_t input;
        input.append(2, ch);
        input.append(U("["));
        input.append(2, ch);
        input.append(U("1"));
        input.append(1, ch);
        input.append(U(","));
        input.append(4, ch);
        input.append(U("2"));
        input.append(1, ch);
        input.append(U("]"));
        input.append(2, ch);
        json::value val = json::value::parse(input);
        VERIFY_IS_TRUE(val.is_array());
        VERIFY_ARE_EQUAL(U("1"), val[0].serialize());
        VERIFY_ARE_EQUAL(U("2"), val[1].serialize());
    }
}

TEST(whitespace_object)
{
    // {:}
    for(auto ch : whitespace_chars)
    {
        utility::string_t input;
        input.append(2, ch);
        input.append(U("{"));
        input.append(2, ch);
        input.append(U("\"1\""));
        input.append(1, ch);
        input.append(U(":"));
        input.append(4, ch);
        input.append(U("2"));
        input.append(1, ch);
        input.append(U("}"));
        input.append(2, ch);
        json::value val = json::value::parse(input);
        VERIFY_IS_TRUE(val.is_object());
        VERIFY_ARE_EQUAL(U("2"), val[U("1"]).serialize());
    }
}

TEST(string_t)
{
    json::value str = json::value::parse(U("\"\\\"\""));
    VERIFY_ARE_EQUAL(U("\""), str.as_string());

    str = json::value::parse(U("\"\""));
    VERIFY_ARE_EQUAL(U(""), str.as_string());

    str = json::value::parse(U("\"\\\"ds\""));
    VERIFY_ARE_EQUAL(U("\"ds"), str.as_string());

    str = json::value::parse(U("\"\\\"\\\"\""));
    VERIFY_ARE_EQUAL(U("\"\""), str.as_string());

    // two character escapes
    str = json::value::parse(U("\"\\\\\""));
    VERIFY_ARE_EQUAL(U("\\"), str.as_string());

    str = json::value::parse(U("\"\\/\""));
    VERIFY_ARE_EQUAL(U("/"), str.as_string());

    str = json::value::parse(U("\"\\b\""));
    VERIFY_ARE_EQUAL(U("\b"), str.as_string());

    str = json::value::parse(U("\"\\f\""));
    VERIFY_ARE_EQUAL(U("\f"), str.as_string());

    str = json::value::parse(U("\"\\n\""));
    VERIFY_ARE_EQUAL(U("\n"), str.as_string());

    str = json::value::parse(U("\"\\r\""));
    VERIFY_ARE_EQUAL(U("\r"), str.as_string());

    str = json::value::parse(U("\"\\t\""));
    VERIFY_ARE_EQUAL(U("\t"), str.as_string());

    str = json::value::parse(U("\"\\u0041\""));
    VERIFY_ARE_EQUAL(U("A"), str.as_string());

    str = json::value::parse(U("\"\\u004B\""));
    VERIFY_ARE_EQUAL(U("K"), str.as_string());

    VERIFY_PARSING_THROW(json::value::parse(U("\"\\u0klB\"")));
}

TEST(comments_string)
{
    // Nothing but a comment
    VERIFY_PARSING_THROW(json::value::parse(U(" /* There's nothing but a comment here */  ")));
    VERIFY_PARSING_THROW(json::value::parse(U(" // There's nothing but a comment here\n")));

    // Some invalid comments
    VERIFY_PARSING_THROW(json::value::parse(U(" -22 /*/")));
    VERIFY_PARSING_THROW(json::value::parse(U(" -22 /* /* nested */ */")));

    // Correctly placed comments
    json::value num1 = json::value::parse(U("-22 // This is a trailing comment\n"));
    VERIFY_ARE_EQUAL(-22, num1.as_double());
    num1 = json::value::parse(U(" -22 /* This is a trailing comment with a // nested\n comment */"));
    VERIFY_ARE_EQUAL(-22, num1.as_double());
    json::value num2 = json::value::parse(U("// This is a leading comment\n -22"));
    VERIFY_ARE_EQUAL(-22, num2.as_double());
    json::value num3 = json::value::parse(U("-22 /* This is a trailing comment */"));
    VERIFY_ARE_EQUAL(-22, num3.as_double());
    json::value num4 = json::value::parse(U("/* This is a leading comment */ -22"));
    VERIFY_ARE_EQUAL(-22, num4.as_double());
    json::value num5 = json::value::parse(U("-22 /***/"));
    VERIFY_ARE_EQUAL(-22, num5.as_double());

    json::value obj1 = json::value::parse(U("{// A comment in the middle of an empty object\n}"));
    VERIFY_IS_TRUE(obj1.is_object());
    VERIFY_ARE_EQUAL(0u, obj1.size());
    json::value obj2 = json::value::parse(U("{/* A comment in the middle of an empty object */}"));
    VERIFY_IS_TRUE(obj2.is_object());
    VERIFY_ARE_EQUAL(0u, obj2.size());
    json::value obj3 = json::value::parse(U("{ \"test\" : // A comment in the middle of a non-empty object\n 2}"));
    VERIFY_IS_TRUE(obj3.is_object());
    VERIFY_ARE_EQUAL(1u, obj3.size());
    json::value obj4 = json::value::parse(U("{ \"test\" : /* A comment in the middle of a non-empty object */ 2}"));
    VERIFY_IS_TRUE(obj4.is_object());
    VERIFY_ARE_EQUAL(1u, obj4.size());

    json::value arr1 = json::value::parse(U("[// A comment in the middle of an empty array\n]"));
    VERIFY_IS_TRUE(arr1.is_array());
    VERIFY_ARE_EQUAL(0u, arr1.size());
    json::value arr2 = json::value::parse(U("[/* A comment in the middle of an empty array */]"));
    VERIFY_IS_TRUE(arr2.is_array());
    VERIFY_ARE_EQUAL(0u, arr2.size());
    json::value arr3 = json::value::parse(U("[ 1, // A comment in the middle of a non-array\n 2]"));
    VERIFY_IS_TRUE(arr3.is_array());
    VERIFY_ARE_EQUAL(2u, arr3.size());
    json::value arr4 = json::value::parse(U("[ 1, /* A comment in the middle of a non-empty array */ 2]"));
    VERIFY_IS_TRUE(arr4.is_array());
    VERIFY_ARE_EQUAL(2u, arr4.size());
}

TEST(comments_stream)
{
    // Nothing but a comment
    {
    std::basic_stringstream<utility::char_t> stream;
    stream << U(" /* There's nothing but a comment here */ ");
    VERIFY_PARSING_THROW(json::value::parse(stream));
    }
    {
    std::basic_stringstream<utility::char_t> stream;
    stream << U(" // There's nothing but a comment here\n ");
    VERIFY_PARSING_THROW(json::value::parse(stream));
    }

    // Some invalid comments
    {
    std::basic_stringstream<utility::char_t> stream;
    stream << U(" -22 /*/");
    VERIFY_PARSING_THROW(json::value::parse(stream));
    }
    {
    std::basic_stringstream<utility::char_t> stream;
    stream << U(" -22 /* /* nested */ */");
    VERIFY_PARSING_THROW(json::value::parse(stream));
    }

    // Correctly placed comments
    {
    std::basic_stringstream<utility::char_t> stream;
    stream << U("-22 // This is a trailing comment\n");
    json::value num1 = json::value::parse(stream);
    VERIFY_ARE_EQUAL(-22, num1.as_double());
    }
    {
    std::basic_stringstream<utility::char_t> stream;
    stream << U(" -22 /* This is a trailing comment with a // nested\n comment */");
    json::value num1 = json::value::parse(stream);
    VERIFY_ARE_EQUAL(-22, num1.as_double());
    }
    {
    std::basic_stringstream<utility::char_t> stream;
    stream << U("// This is a leading comment\n -22");
    json::value num2 = json::value::parse(stream);
    VERIFY_ARE_EQUAL(-22, num2.as_double());
    }
    {
    std::basic_stringstream<utility::char_t> stream;
    stream << U("-22 /* This is a trailing comment */");
    json::value num3 = json::value::parse(stream);
    VERIFY_ARE_EQUAL(-22, num3.as_double());
    }
    {
    std::basic_stringstream<utility::char_t> stream;
    stream << U("/* This is a leading comment */ -22");
    json::value num4 = json::value::parse(stream);
    VERIFY_ARE_EQUAL(-22, num4.as_double());
    }
    {
    std::basic_stringstream<utility::char_t> stream;
    stream << U("-22 /***/");
    json::value num4 = json::value::parse(stream);
    VERIFY_ARE_EQUAL(-22, num4.as_double());
    }

    {
    std::basic_stringstream<utility::char_t> stream;
    stream << U("{// A comment in the middle of an empty object\n}");
    json::value obj1 = json::value::parse(stream);
    VERIFY_IS_TRUE(obj1.is_object());
    VERIFY_ARE_EQUAL(0u, obj1.size());
    }
    {
    std::basic_stringstream<utility::char_t> stream;
    stream << U("{/* A comment in the middle of an empty object */}");
    json::value obj2 = json::value::parse(stream);
    VERIFY_IS_TRUE(obj2.is_object());
    VERIFY_ARE_EQUAL(0u, obj2.size());
    }
    {
    std::basic_stringstream<utility::char_t> stream;
    stream << U("{ \"test1\" : // A comment in the middle of a non-empty object\n 2}");
    json::value obj3 = json::value::parse(stream);
    VERIFY_IS_TRUE(obj3.is_object());
    VERIFY_ARE_EQUAL(1u, obj3.size());
    }
    {
    std::basic_stringstream<utility::char_t> stream;
    stream << U("{ \"test1\" : /* A comment in the middle of a non-empty object */ 2}");
    json::value obj4 = json::value::parse(stream);
    VERIFY_IS_TRUE(obj4.is_object());
    VERIFY_ARE_EQUAL(1u, obj4.size());
    }

    {
    std::basic_stringstream<utility::char_t> stream;
    stream << U("[// A comment in the middle of an empty array\n]");
    json::value arr1 = json::value::parse(stream);
    VERIFY_IS_TRUE(arr1.is_array());
    VERIFY_ARE_EQUAL(0u, arr1.size());
    }
    {
    std::basic_stringstream<utility::char_t> stream;
    stream << U("[/* A comment in the middle of an empty array */]");
    json::value arr2 = json::value::parse(stream);
    VERIFY_IS_TRUE(arr2.is_array());
    VERIFY_ARE_EQUAL(0u, arr2.size());
    }
    {
    std::basic_stringstream<utility::char_t> stream;
    stream << U("[ 1, // A comment in the middle of a non-array\n 2]");
    json::value arr3 = json::value::parse(stream);
    VERIFY_IS_TRUE(arr3.is_array());
    VERIFY_ARE_EQUAL(2u, arr3.size());
    }
    {
    std::basic_stringstream<utility::char_t> stream;
    stream << U("[ 1, /* A comment in the middle of a non-empty array */ 2]");
    json::value arr4 = json::value::parse(stream);
    VERIFY_IS_TRUE(arr4.is_array());
    VERIFY_ARE_EQUAL(2u, arr4.size());
    }
}

TEST(empty_object_array)
{
    json::value obj = json::value::parse(U("{}"));
    VERIFY_IS_TRUE(obj.is_object());
    VERIFY_ARE_EQUAL(0u, obj.size());

    json::value arr = json::value::parse(U("[]"));
    VERIFY_IS_TRUE(arr.is_array());
    VERIFY_ARE_EQUAL(0u, arr.size());
}

TEST(bug_416116)
{
    json::value data2 = json::value::parse(U("\"δοκιμή\""));
    auto s = data2.serialize();

#pragma warning( push )
#pragma warning( disable : 4566 )
    VERIFY_ARE_EQUAL(s, U("\"δοκιμή\""));
#pragma warning( pop )
}

TEST(byte_ptr_parsing_array)
{
    char s[] = "[ \"test1\",true]";
    std::stringstream ss;
    ss << s;
    json::value v = json::value::parse(ss);
    auto s2 = v.serialize();

    VERIFY_ARE_EQUAL(s2, U("[\"test1\",true]"));

    std::stringstream os;
    v.serialize(os);
    VERIFY_ARE_EQUAL(s2, to_string_t(os.str()));
}

TEST(byte_ptr_parsing_object)
{
    char s[] = "{\"test1\":true }";
    std::stringstream ss;
    ss << s;
    json::value v = json::value::parse(ss);
    auto s2 = v.serialize();

    VERIFY_ARE_EQUAL(s2, U("{\"test1\":true}"));

    std::stringstream os;
    v.serialize(os);
    VERIFY_ARE_EQUAL(s2, to_string_t(os.str()));
}

TEST(Japanese)
{
    utility::string_t ws = U("\"こんにちは\"");
    std::string s = to_utf8string(ws);

    std::stringstream ss;
    ss << s;
    json::value v = json::value::parse(ss);
    auto s2 = v.serialize();

    VERIFY_ARE_EQUAL(s2, ws);

    std::stringstream os;
    v.serialize(os);
    VERIFY_ARE_EQUAL(s2, to_string_t(os.str()));
}

TEST(Russian)
{
    utility::string_t ws = U("{\"results\":[{\"id\":272655310,\"name\":\"Андрей Ив´анов\"}]}");
    json::value v1 = json::value::parse(ws);
    auto s2 = v1.serialize();

    VERIFY_ARE_EQUAL(s2, ws);

    std::string s = to_utf8string(ws);

    std::stringstream ss;
    ss << s;
    json::value v2 = json::value::parse(ss);
    auto s3 = v2.serialize();

    VERIFY_ARE_EQUAL(s3, ws);
}

utility::string_t make_deep_json_string(size_t depth)
{
    utility::string_t strval;
    for(size_t i=0; i<depth; ++i)
    {
        strval += U("{ \"a\" : 10, \"b\" : ");
    }
    strval += U("20");
    for(size_t i=0; i<depth; ++i)
    {
        strval += U("}");
    }
    return strval;
}

TEST(deeply_nested)
{
#ifdef __APPLE__
    auto strGood = make_deep_json_string(32);
#else
    auto strGood = make_deep_json_string(128);
#endif
    // This should parse without issues:
    json::value::parse(strGood);

#ifdef __APPLE__
    auto strBad = make_deep_json_string(33);
#else
    auto strBad = make_deep_json_string(129);
#endif

    // But this one should throw:
    VERIFY_PARSING_THROW(json::value::parse(strBad));
}

 static bool compare_pairs(const std::pair<utility::string_t, json::value>& p1,
                           const std::pair<utility::string_t, json::value>& p2)
 {
   return p1.first < p2.first;
 }

TEST(unsorted_object_parsing)
{
    utility::stringstream_t ss;
    ss << U("{\"z\":2, \"a\":1}");
    json::value v = json::value::parse(ss);
    auto& obj = v.as_object();

    VERIFY_ARE_NOT_EQUAL(obj.find(U("a")), obj.end());
    VERIFY_ARE_NOT_EQUAL(obj.find(U("z")), obj.end());
    VERIFY_ARE_EQUAL(obj[U("a")], 1);
    VERIFY_ARE_EQUAL(obj[U("z")], 2);
    VERIFY_ARE_EQUAL(obj.size(), 2);

    VERIFY_IS_TRUE(::std::is_sorted(obj.begin(), obj.end(), compare_pairs));
}

TEST(keep_order_while_parsing)
{
    utility::stringstream_t ss;
    ss << U("{\"k\":3, \"j\":2, \"i\":1}");

    json::keep_object_element_order(true);
    struct restore {
        ~restore() {
            json::keep_object_element_order(false);
        }
    }_;

    json::value v = json::value::parse(ss);
    auto& obj = v.as_object();

    // Make sure collection stays unsorted:
    auto b = obj.begin();
    VERIFY_ARE_EQUAL(b[0].first, U("k"));
    VERIFY_ARE_EQUAL(b[1].first, U("j"));
    VERIFY_ARE_EQUAL(b[2].first, U("i"));

    // Make sure lookup still works:
    auto val_i = obj[U("i")];
    VERIFY_ARE_EQUAL(val_i.as_integer(), 1);

    auto val_j = obj[U("j")];
    VERIFY_ARE_EQUAL(val_j.as_integer(), 2);

    // Make sure 'a' goes to the back of the collection, and
    // can be looked up
    obj[U("a")] = 4;
    b = obj.begin();
    VERIFY_ARE_EQUAL(b[3].first, U("a"));
    VERIFY_ARE_EQUAL(obj[U("a")].as_integer(), 4);
}

} // SUITE(parsing_tests)

}}}
