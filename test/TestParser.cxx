/*
 * author: Max Kellermann <mk@cm4all.com>
 */

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
		"\n"
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
}
