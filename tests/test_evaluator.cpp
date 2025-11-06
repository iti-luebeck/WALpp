#include <gtest/gtest.h>
#include "wal/core.h"

class EvaluatorTest : public ::testing::Test {
protected:
    std::shared_ptr<wal::Wal> wal;
    
    void SetUp() override {
        wal = std::make_shared<wal::Wal>();
    }
};

// Basic evaluation tests
TEST_F(EvaluatorTest, EvalLiteral) {
    auto result = wal->eval_str("42");
    EXPECT_TRUE(result.is_int());
    EXPECT_EQ(result.to_int(), 42);
}

TEST_F(EvaluatorTest, EvalBoolean) {
    auto t = wal->eval_str("#t");
    EXPECT_TRUE(t.is_bool());
    EXPECT_TRUE(t.to_bool());
}

TEST_F(EvaluatorTest, EvalString) {
    auto result = wal->eval_str("\"hello\"");
    EXPECT_TRUE(result.is_string());
    EXPECT_EQ(result.to_string(), "hello");
}

// Math operators
TEST_F(EvaluatorTest, AddIntegers) {
    auto result = wal->eval_str("(+ 1 2)");
    EXPECT_TRUE(result.is_int());
    EXPECT_EQ(result.to_int(), 3);
}

TEST_F(EvaluatorTest, AddMultiple) {
    auto result = wal->eval_str("(+ 1 2 3 4)");
    EXPECT_EQ(result.to_int(), 10);
}

TEST_F(EvaluatorTest, Subtract) {
    auto result = wal->eval_str("(- 10 3)");
    EXPECT_EQ(result.to_int(), 7);
}

TEST_F(EvaluatorTest, SubtractNegate) {
    auto result = wal->eval_str("(- 5)");
    EXPECT_EQ(result.to_int(), -5);
}

TEST_F(EvaluatorTest, Multiply) {
    auto result = wal->eval_str("(* 3 4)");
    EXPECT_EQ(result.to_int(), 12);
}

TEST_F(EvaluatorTest, Divide) {
    auto result = wal->eval_str("(/ 10 2)");
    EXPECT_DOUBLE_EQ(result.to_float(), 5.0);
}

TEST_F(EvaluatorTest, Power) {
    auto result = wal->eval_str("(** 2 3)");
    EXPECT_DOUBLE_EQ(result.to_float(), 8.0);
}

TEST_F(EvaluatorTest, Floor) {
    auto result = wal->eval_str("(floor 3.7)");
    EXPECT_EQ(result.to_int(), 3);
}

TEST_F(EvaluatorTest, Ceil) {
    auto result = wal->eval_str("(ceil 3.2)");
    EXPECT_EQ(result.to_int(), 4);
}

TEST_F(EvaluatorTest, Round) {
    auto result = wal->eval_str("(round 3.5)");
    EXPECT_EQ(result.to_int(), 4);
}

TEST_F(EvaluatorTest, Mod) {
    auto result = wal->eval_str("(mod 10 3)");
    EXPECT_EQ(result.to_int(), 1);
}

// Comparison operators
TEST_F(EvaluatorTest, Equality) {
    auto result = wal->eval_str("(= 5 5)");
    EXPECT_TRUE(result.is_bool());
    EXPECT_TRUE(result.to_bool());
    
    auto result2 = wal->eval_str("(= 5 3)");
    EXPECT_FALSE(result2.to_bool());
}

TEST_F(EvaluatorTest, NotEqual) {
    auto result = wal->eval_str("(!= 5 3)");
    EXPECT_TRUE(result.to_bool());
    
    auto result2 = wal->eval_str("(!= 5 5)");
    EXPECT_FALSE(result2.to_bool());
}

TEST_F(EvaluatorTest, LessThan) {
    EXPECT_TRUE(wal->eval_str("(< 3 5)").to_bool());
    EXPECT_FALSE(wal->eval_str("(< 5 3)").to_bool());
}

TEST_F(EvaluatorTest, GreaterThan) {
    EXPECT_TRUE(wal->eval_str("(> 5 3)").to_bool());
    EXPECT_FALSE(wal->eval_str("(> 3 5)").to_bool());
}

TEST_F(EvaluatorTest, LessEqual) {
    EXPECT_TRUE(wal->eval_str("(<= 3 5)").to_bool());
    EXPECT_TRUE(wal->eval_str("(<= 5 5)").to_bool());
    EXPECT_FALSE(wal->eval_str("(<= 5 3)").to_bool());
}

TEST_F(EvaluatorTest, GreaterEqual) {
    EXPECT_TRUE(wal->eval_str("(>= 5 3)").to_bool());
    EXPECT_TRUE(wal->eval_str("(>= 5 5)").to_bool());
    EXPECT_FALSE(wal->eval_str("(>= 3 5)").to_bool());
}

// Logic operators
TEST_F(EvaluatorTest, And) {
    EXPECT_TRUE(wal->eval_str("(&& #t #t)").to_bool());
    EXPECT_FALSE(wal->eval_str("(&& #t #f)").to_bool());
}

TEST_F(EvaluatorTest, Or) {
    EXPECT_TRUE(wal->eval_str("(|| #t #f)").to_bool());
    EXPECT_FALSE(wal->eval_str("(|| #f #f)").to_bool());
}

TEST_F(EvaluatorTest, Not) {
    EXPECT_FALSE(wal->eval_str("(! #t)").to_bool());
    EXPECT_TRUE(wal->eval_str("(! #f)").to_bool());
}

// Variable binding
TEST_F(EvaluatorTest, DefineVariable) {
    wal->eval_str("(define x 42)");
    auto result = wal->eval_str("x");
    EXPECT_EQ(result.to_int(), 42);
}

TEST_F(EvaluatorTest, SetVariable) {
    wal->eval_str("(define x 10)");
    wal->eval_str("(set! x 20)");
    auto result = wal->eval_str("x");
    EXPECT_EQ(result.to_int(), 20);
}

// Control flow
TEST_F(EvaluatorTest, IfThen) {
    auto result = wal->eval_str("(if #t 1 2)");
    EXPECT_EQ(result.to_int(), 1);
}

TEST_F(EvaluatorTest, IfElse) {
    auto result = wal->eval_str("(if #f 1 2)");
    EXPECT_EQ(result.to_int(), 2);
}

TEST_F(EvaluatorTest, IfNoElse) {
    auto result = wal->eval_str("(if #f 1)");
    EXPECT_TRUE(result.is_nil());
}

// Lambdas and function calls
TEST_F(EvaluatorTest, SimpleLambda) {
    wal->eval_str("(define add (lambda (a b) (+ a b)))");
    auto result = wal->eval_str("(add 3 4)");
    EXPECT_EQ(result.to_int(), 7);
}

TEST_F(EvaluatorTest, NestedLambda) {
    wal->eval_str("(define double (lambda (x) (* x 2)))");
    wal->eval_str("(define quad (lambda (x) (double (double x))))");
    auto result = wal->eval_str("(quad 5)");
    EXPECT_EQ(result.to_int(), 20);
}

// Quote
TEST_F(EvaluatorTest, QuoteAtom) {
    auto result = wal->eval_str("'x");
    EXPECT_TRUE(result.is_symbol());
    EXPECT_EQ(result.to_symbol()->get_name(), "x");
}

TEST_F(EvaluatorTest, QuoteList) {
    auto result = wal->eval_str("'(+ 1 2)");
    EXPECT_TRUE(result.is_list());
    auto list = result.to_list();
    EXPECT_EQ(list->size(), 3);
}

// List operations
TEST_F(EvaluatorTest, ListConstruction) {
    auto result = wal->eval_str("(list 1 2 3)");
    EXPECT_TRUE(result.is_list());
    EXPECT_EQ(result.to_list()->size(), 3);
}

// Begin
TEST_F(EvaluatorTest, BeginSequence) {
    auto result = wal->eval_str("(begin (define x 1) (+ x 2))");
    EXPECT_EQ(result.to_int(), 3);
}

// Error handling
TEST_F(EvaluatorTest, UndefinedVariable) {
    EXPECT_THROW(wal->eval_str("undefined_var"), wal::EvalError);
}

TEST_F(EvaluatorTest, DivisionByZero) {
    EXPECT_THROW(wal->eval_str("(/ 10 0)"), wal::EvalError);
}

TEST_F(EvaluatorTest, WrongArgumentCount) {
    EXPECT_THROW(wal->eval_str("(+ )"), wal::EvalError);
}

