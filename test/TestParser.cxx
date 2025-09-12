// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Entity.hxx"
#include "Parser.hxx"

#include <gtest/gtest.h>

using std::string_view_literals::operator""sv;

TEST(Parser, Simple)
{
	const auto e = ParseEntity("OK\n\n");
	EXPECT_EQ(e.command, "OK");
	EXPECT_TRUE(e.args.empty());
	EXPECT_TRUE(e.headers.empty());
}

TEST(Parser, OneArg)
{
	const auto e = ParseEntity("FOO one\n");
	EXPECT_EQ(e.command, "FOO");
	EXPECT_FALSE(e.args.empty());
	EXPECT_EQ(e.args.front(), "one");
	EXPECT_EQ(std::next(e.args.begin()), e.args.end());
	EXPECT_TRUE(e.headers.empty());
}

TEST(Parser, TwoArgs)
{
	const auto e = ParseEntity("FOO one two");
	EXPECT_EQ(e.command, "FOO");
	EXPECT_FALSE(e.args.empty());
	EXPECT_EQ(e.args.front(), "one");
	EXPECT_EQ(*std::next(e.args.begin()), "two");
	EXPECT_EQ(std::next(e.args.begin(), 2), e.args.end());
	EXPECT_TRUE(e.headers.empty());
}

TEST(Parser, Headers)
{
	const auto e = ParseEntity("FOO\n"
				   "abc: 1\n"
				   "def: 2");
	EXPECT_EQ(e.command, "FOO");
	EXPECT_TRUE(e.args.empty());
	EXPECT_FALSE(e.headers.empty());
	EXPECT_EQ(std::distance(e.headers.begin(), e.headers.end()), 2u);
	EXPECT_EQ(e.headers.find("abc")->second, "1");
	EXPECT_EQ(e.headers.find("def")->second, "2");
}

TEST(Parser, Full)
{
	static constexpr std::string_view payload =
		"FOO one two three\n"
		"abc: 1\n"
		"def: 2\n"
		"\0"
		"\x00\x01\x02\x03"sv;
	const auto e = ParseEntity(payload);
	EXPECT_EQ(e.command, "FOO");
	EXPECT_FALSE(e.args.empty());
	EXPECT_EQ(e.args.front(), "one");
	EXPECT_EQ(*std::next(e.args.begin()), "two");
	EXPECT_EQ(*std::next(e.args.begin(), 2), "three");
	EXPECT_EQ(std::next(e.args.begin(), 3), e.args.end());
	EXPECT_FALSE(e.headers.empty());
	EXPECT_EQ(std::distance(e.headers.begin(), e.headers.end()), 2u);
	EXPECT_EQ(e.headers.find("abc")->second, "1");
	EXPECT_EQ(e.headers.find("def")->second, "2");
	EXPECT_EQ(e.body, "\x00\x01\x02\x03"sv);
}

TEST(Parser, QuotedSimple)
{
	const auto e = ParseEntity("FOO \"hello\"");
	EXPECT_EQ(e.command, "FOO");
	EXPECT_FALSE(e.args.empty());
	EXPECT_EQ(e.args.front(), "hello");
	EXPECT_EQ(std::next(e.args.begin()), e.args.end());
}

TEST(Parser, QuotedEmpty)
{
	const auto e = ParseEntity("FOO \"\"");
	EXPECT_EQ(e.command, "FOO");
	EXPECT_FALSE(e.args.empty());
	EXPECT_EQ(e.args.front(), "");
	EXPECT_EQ(std::next(e.args.begin()), e.args.end());
}

TEST(Parser, QuotedWhitespace)
{
	const auto e = ParseEntity("FOO \"   \"");
	EXPECT_EQ(e.command, "FOO");
	EXPECT_FALSE(e.args.empty());
	EXPECT_EQ(e.args.front(), "   ");
	EXPECT_EQ(std::next(e.args.begin()), e.args.end());
}

TEST(Parser, QuotedBackslash)
{
	const auto e = ParseEntity(R"(FOO "\\")");
	EXPECT_EQ(e.command, "FOO");
	EXPECT_FALSE(e.args.empty());
	EXPECT_EQ(e.args.front(), "\\");
	EXPECT_EQ(std::next(e.args.begin()), e.args.end());
}

TEST(Parser, QuotedDoubleQuote)
{
	const auto e = ParseEntity(R"(FOO "\"")");
	EXPECT_EQ(e.command, "FOO");
	EXPECT_FALSE(e.args.empty());
	EXPECT_EQ(e.args.front(), "\"");
	EXPECT_EQ(std::next(e.args.begin()), e.args.end());
}

TEST(Parser, QuotedSingleQuote)
{
	const auto e = ParseEntity("FOO \"'\"");
	EXPECT_EQ(e.command, "FOO");
	EXPECT_FALSE(e.args.empty());
	EXPECT_EQ(e.args.front(), "'");
	EXPECT_EQ(std::next(e.args.begin()), e.args.end());
}

TEST(Parser, QuotedMultipleArgs)
{
	const auto e = ParseEntity(R"(FOO "first" "second\"quote" "third\\backslash")");
	EXPECT_EQ(e.command, "FOO");
	EXPECT_FALSE(e.args.empty());
	EXPECT_EQ(e.args.front(), "first");
	EXPECT_EQ(*std::next(e.args.begin()), "second\"quote");
	EXPECT_EQ(*std::next(e.args.begin(), 2), "third\\backslash");
	EXPECT_EQ(std::next(e.args.begin(), 3), e.args.end());
}

TEST(Parser, QuotedMixedArgs)
{
	const auto e = ParseEntity("FOO unquoted \"quoted\" another");
	EXPECT_EQ(e.command, "FOO");
	EXPECT_FALSE(e.args.empty());
	EXPECT_EQ(e.args.front(), "unquoted");
	EXPECT_EQ(*std::next(e.args.begin()), "quoted");
	EXPECT_EQ(*std::next(e.args.begin(), 2), "another");
	EXPECT_EQ(std::next(e.args.begin(), 3), e.args.end());
}
