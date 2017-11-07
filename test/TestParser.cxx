/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Entity.hxx"
#include "Parser.hxx"
#include "util/StringView.hxx"

#include <gtest/gtest.h>

TEST(Parser, Simple)
{
    const auto e = ParseEntity("OK\n\n");
    EXPECT_EQ(e.command, "OK");
    EXPECT_TRUE(e.args.empty());
}

TEST(Parser, OneArg)
{
    const auto e = ParseEntity("FOO one\n");
    EXPECT_EQ(e.command, "FOO");
    EXPECT_FALSE(e.args.empty());
    EXPECT_EQ(e.args.front(), "one");
    EXPECT_EQ(std::next(e.args.begin()), e.args.end());
}

TEST(Parser, TwoArgs)
{
    const auto e = ParseEntity("FOO one two");
    EXPECT_EQ(e.command, "FOO");
    EXPECT_FALSE(e.args.empty());
    EXPECT_EQ(e.args.front(), "one");
    EXPECT_EQ(*std::next(e.args.begin()), "two");
    EXPECT_EQ(std::next(e.args.begin(), 2), e.args.end());
}

TEST(Parser, Full)
{
    static constexpr char payload[] =
        "FOO one two three\n"
        "abc: 1\n"
        "def: 2\n"
        "\n"
        "\x00\x01\x02\x03";
    const auto e = ParseEntity({payload, sizeof(payload) - 1});
    EXPECT_EQ(e.command, "FOO");
    EXPECT_FALSE(e.args.empty());
    EXPECT_EQ(e.args.front(), "one");
    EXPECT_EQ(*std::next(e.args.begin()), "two");
    EXPECT_EQ(*std::next(e.args.begin(), 2), "three");
    EXPECT_EQ(std::next(e.args.begin(), 3), e.args.end());
}
