#include <gtest/gtest.h>
#include "wal/ast.h"

class ASTTest : public ::testing::Test {
};

TEST_F(ASTTest, ValueInt) {
    wal::Value v(42);
    EXPECT_TRUE(v.is_int());
    EXPECT_FALSE(v.is_float());
    EXPECT_EQ(v.to_int(), 42);
}

TEST_F(ASTTest, ValueFloat) {
    wal::Value v(3.14);
    EXPECT_TRUE(v.is_float());
    EXPECT_FALSE(v.is_int());
    EXPECT_DOUBLE_EQ(v.to_float(), 3.14);
}

TEST_F(ASTTest, ValueBool) {
    wal::Value t(true);
    wal::Value f(false);
    EXPECT_TRUE(t.is_bool());
    EXPECT_TRUE(f.is_bool());
    EXPECT_TRUE(t.to_bool());
    EXPECT_FALSE(f.to_bool());
}

TEST_F(ASTTest, ValueString) {
    wal::Value v("hello");
    EXPECT_TRUE(v.is_string());
    EXPECT_EQ(v.to_string(), "hello");
}

TEST_F(ASTTest, ValueNil) {
    wal::Value v(nullptr);
    EXPECT_TRUE(v.is_nil());
}

TEST_F(ASTTest, ValueSymbol) {
    auto sym = std::make_shared<wal::Symbol>("x");
    wal::Value v(sym);
    EXPECT_TRUE(v.is_symbol());
    EXPECT_EQ(v.to_symbol()->get_name(), "x");
}

TEST_F(ASTTest, ValueList) {
    std::vector<wal::Value> elements;
    elements.push_back(wal::Value(1));
    elements.push_back(wal::Value(2));
    auto list = std::make_shared<wal::WList>(elements);
    wal::Value v(list);
    EXPECT_TRUE(v.is_list());
    EXPECT_EQ(v.to_list()->size(), 2);
}

TEST_F(ASTTest, TruthyValues) {
    EXPECT_TRUE(wal::Value(true).is_truthy());
    EXPECT_FALSE(wal::Value(false).is_truthy());
    EXPECT_FALSE(wal::Value(nullptr).is_truthy());
    EXPECT_TRUE(wal::Value(42).is_truthy());
    EXPECT_TRUE(wal::Value("string").is_truthy());
}

TEST_F(ASTTest, NumericConversion) {
    wal::Value i(42);
    EXPECT_EQ(i.to_float(), 42.0);
    
    wal::Value f(3.14);
    EXPECT_EQ(f.to_int(), 3);
}

TEST_F(ASTTest, ValueEquality) {
    EXPECT_EQ(wal::Value(42), wal::Value(42));
    EXPECT_NE(wal::Value(42), wal::Value(43));
    EXPECT_EQ(wal::Value("hello"), wal::Value("hello"));
    EXPECT_NE(wal::Value("hello"), wal::Value("world"));
}

TEST_F(ASTTest, StringRepr) {
    EXPECT_EQ(wal::Value(42).to_string_repr(), "42");
    EXPECT_EQ(wal::Value(true).to_string_repr(), "#t");
    EXPECT_EQ(wal::Value(false).to_string_repr(), "#f");
    EXPECT_EQ(wal::Value(nullptr).to_string_repr(), "nil");
    EXPECT_EQ(wal::Value("hello").to_string_repr(), "\"hello\"");
}

TEST_F(ASTTest, EnvironmentDefine) {
    auto env = std::make_shared<wal::Environment>();
    env->define("x", wal::Value(42));
    EXPECT_TRUE(env->is_defined("x"));
    EXPECT_EQ(env->get("x").to_int(), 42);
}

TEST_F(ASTTest, EnvironmentWrite) {
    auto env = std::make_shared<wal::Environment>();
    env->define("x", wal::Value(42));
    env->write("x", wal::Value(100));
    EXPECT_EQ(env->get("x").to_int(), 100);
}

TEST_F(ASTTest, EnvironmentParentLookup) {
    auto parent = std::make_shared<wal::Environment>();
    parent->define("x", wal::Value(42));
    
    auto child = std::make_shared<wal::Environment>(parent);
    EXPECT_EQ(child->get("x").to_int(), 42);
}

TEST_F(ASTTest, SymbolFullName) {
    auto sym1 = wal::Symbol("foo");
    EXPECT_EQ(sym1.get_name(), "foo");
    
    auto sym2 = wal::Symbol("bar", "scope1");
    EXPECT_EQ(sym2.get_scope(), "scope1");
}

