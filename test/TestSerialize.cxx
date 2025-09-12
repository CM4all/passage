// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Entity.hxx"

#include <gtest/gtest.h>

using std::string_view_literals::operator""sv;

TEST(Serialize, JustCommand)
{
	const Entity e{
		.command = "OK",
	};

	const auto result = e.Serialize();
	EXPECT_EQ(result, "OK");
}

TEST(Serialize, CommandWithParameters)
{
	const Entity e{
		.command = "FOO",
		.args = {"one", "two"},
	};

	const auto result = e.Serialize();
	EXPECT_EQ(result, "FOO one two");
}

TEST(Serialize, WithHeaders)
{
	const Entity e{
		.command = "FOO",
		.args = {"one", "two"},
		.headers = {{"abc", "1"}, {"def", "2"}},
	};

	const auto result = e.Serialize();
	EXPECT_EQ(result, "FOO one two\nabc:1\ndef:2\n");
}

TEST(Serialize, WithBinaryBody)
{
	const Entity e{
		.command = "FOO",
		.args = {"arg"},
		.body = std::string{"\x00\x01\x02\x03"sv},
	};

	const auto result = e.Serialize();
	EXPECT_EQ(result, "FOO arg\0\x00\x01\x02\x03"sv);
}

TEST(Serialize, Everything)
{
	const Entity e{
		.command = "FOO",
		.args = {"one", "two", "three"},
		.headers = {{"abc", "1"}, {"def", "2"}},
		.body = std::string{"\x00\x01\x02\x03"sv},
	};

	const auto result = e.Serialize();
	EXPECT_EQ(result, "FOO one two three\nabc:1\ndef:2\n\0\x00\x01\x02\x03"sv);
}

TEST(Serialize, QuotedParameters)
{
	const Entity e{
		.command = "FOO",
		.args = {"", "\"", "\\", " "},
	};

	const auto result = e.Serialize();
	EXPECT_EQ(result, R"(FOO "" "\"" "\\" " ")");
}
