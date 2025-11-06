#include <gtest/gtest.h>
#include "wal/parser.h"

class ParserTest : public ::testing::Test {
protected:
    wal::Parser parser;
};

TEST_F(ParserTest, ParseInteger) {
    auto value = parser.parse("42");
    EXPECT_TRUE(value.is_int());
    EXPECT_EQ(value.to_int(), 42);
}

TEST_F(ParserTest, ParseFloat) {
    auto value = parser.parse("3.14");
    EXPECT_TRUE(value.is_float());
    EXPECT_DOUBLE_EQ(value.to_float(), 3.14);
}

TEST_F(ParserTest, ParseBoolean) {
    auto true_val = parser.parse("#t");
    EXPECT_TRUE(true_val.is_bool());
    EXPECT_TRUE(true_val.to_bool());
    
    auto false_val = parser.parse("#f");
    EXPECT_TRUE(false_val.is_bool());
    EXPECT_FALSE(false_val.to_bool());
}

TEST_F(ParserTest, ParseSymbol) {
    auto value = parser.parse("foo");
    EXPECT_TRUE(value.is_symbol());
    EXPECT_EQ(value.to_symbol()->get_name(), "foo");
}

TEST_F(ParserTest, ParseString) {
    auto value = parser.parse("\"hello\"");
    EXPECT_TRUE(value.is_string());
    EXPECT_EQ(value.to_string(), "hello");
}

TEST_F(ParserTest, ParseList) {
    auto value = parser.parse("(+ 1 2)");
    EXPECT_TRUE(value.is_list());
    auto list = value.to_list();
    EXPECT_EQ(list->size(), 3);
    EXPECT_TRUE((*list)[0].is_symbol());
}

TEST_F(ParserTest, ParseNil) {
    auto value = parser.parse("#nil");
    EXPECT_TRUE(value.is_nil());
}

TEST_F(ParserTest, ParseNegativeNumber) {
    auto value = parser.parse("-42");
    EXPECT_TRUE(value.is_int());
    EXPECT_EQ(value.to_int(), -42);
}

TEST_F(ParserTest, ParseMultipleExpressions) {
    auto values = parser.parse_program("42 \"hello\" (+ 1 2)");
    EXPECT_EQ(values.size(), 3);
    EXPECT_TRUE(values[0].is_int());
    EXPECT_TRUE(values[1].is_string());
    EXPECT_TRUE(values[2].is_list());
}

TEST_F(ParserTest, ParseQuote) {
    auto value = parser.parse("'x");
    EXPECT_TRUE(value.is_list());
    auto list = value.to_list();
    EXPECT_EQ(list->size(), 2);
    EXPECT_TRUE((*list)[0].is_symbol());
    EXPECT_EQ((*list)[0].to_symbol()->get_name(), "quote");
}

TEST_F(ParserTest, ParseNestedLists) {
    auto value = parser.parse("(list (+ 1 2) (* 3 4))");
    EXPECT_TRUE(value.is_list());
    auto outer = value.to_list();
    EXPECT_EQ(outer->size(), 3);
    EXPECT_TRUE((*outer)[1].is_list());
    EXPECT_TRUE((*outer)[2].is_list());
}

