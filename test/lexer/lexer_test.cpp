#include "gtest/gtest.h"

#include <lexer.h>
#include <type.h>

#ifndef LEXER_TEST_DIR
#define LEXER_TEST_DIR "./"
#endif

using namespace ydsh;

/**
 * ############################
 * #  Lv0: test file loading  #
 * ############################
 */
TEST(LexerTest_Lv0, case1) {
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");

        FILE *fp = fopen(LEXER_TEST_DIR  "/" "lexer_test.cpp", "r");
        ASSERT_FALSE(fp == 0);
    });
}

/**
 * #####################################################
 * #  Lv1: statement token and Lexer::toTokenText().  ##
 * #####################################################
 */

class LexerTest_Lv1 : public ::testing::Test {
public:
    Lexer *lexer;
    std::vector<std::pair<TokenKind, Token>> tokens;

public:
    LexerTest_Lv1() = default;

    virtual ~LexerTest_Lv1() {
        delete this->lexer;
        this->lexer = 0;
    }

    virtual void SetUp() {
    }

    virtual void TearDown() {
    }

    // for test
    virtual void initLexer(const char *text) {
        this->lexer = new Lexer("(string)", text);
    }

    virtual void initLexer(const char *text, LexerMode mode) {
        this->initLexer(text);
        this->lexer->setLexerMode(mode);
    }

private:
    virtual void tokenize() {
        SCOPED_TRACE("");

        Token t;
        TokenKind k;
        do {
            SCOPED_TRACE("");
            k = this->lexer->nextToken(t);
            this->tokens.push_back(std::make_pair(k, t));
        } while(k != EOS && k != INVALID);
    }

    virtual void assertKind(TokenKind expected, TokenKind actual) {
        SCOPED_TRACE("");
        ASSERT_STREQ(TO_NAME(expected), TO_NAME(actual));
    }

    virtual void assertToken(unsigned int index, TokenKind expectedKind, const char *expectedText) {
        SCOPED_TRACE("");
        ASSERT_TRUE(index < this->tokens.size());
        auto pair = this->tokens[index];
        this->assertKind(expectedKind, pair.first);

        std::string text = this->lexer->toTokenText(pair.second);
        ASSERT_STREQ(expectedText, text.c_str());
    }

public:
    virtual void assertTokens(std::vector<std::pair<TokenKind, const char *>> expectedList) {
        SCOPED_TRACE("");

        this->tokenize();

        const unsigned int size = expectedList.size();
        ASSERT_EQ(size, this->tokens.size());

        for(unsigned int i = 0; i < size; i++) {
            this->assertToken(i, expectedList[i].first, expectedList[i].second);
        }
    }

    virtual void assertLexerMode(LexerMode mode) {
        ASSERT_STREQ(toModeName(mode), toModeName(this->lexer->getLexerMode()));
    }
};

typedef std::vector<std::pair<TokenKind, const char *>> ExpectedList;

void addPair(ExpectedList &) {
}

template <typename ...T>
void addPair(ExpectedList &list, TokenKind kind, const char *text, T&& ...rest) {
    list.push_back(std::make_pair(kind, text));
    addPair(list, std::forward<T>(rest)...);
}

template <typename ...T>
ExpectedList expect(TokenKind kind, const char *text, T&& ...rest) {
    ExpectedList list;
    addPair(list, kind, text, std::forward<T>(rest)...);
    return list;
};

#define EXPECT(...) this->assertTokens(expect(__VA_ARGS__))

TEST_F(LexerTest_Lv1, assert_tok) {
    const char *text = "assert";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(ASSERT, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, break_tok) {
    const char *text = "break";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(BREAK, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, catch_tok) {
    const char *text = "catch";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(CATCH, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, class_tok) {
    const char *text = "class";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(CLASS, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, continue_tok) {
    const char *text = "continue";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(CONTINUE, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, do_tok) {
    const char *text = "do";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(DO, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, elif_tok) {
    const char *text = "elif";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(ELIF, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, else_tok) {
    const char *text = "else";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(ELSE, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, exportenv_tok) {
    const char *text = "export-env";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(EXPORT_ENV, text, EOS, "");
        this->assertLexerMode(yycNAME);
    });
}

TEST_F(LexerTest_Lv1, finally_tok) {
    const char *text = "finally";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(FINALLY, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, for_tok) {
    const char *text = "for";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(FOR, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, function_tok) {
    const char *text = "function";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(FUNCTION, text, EOS, "");
        this->assertLexerMode(yycNAME);
    });
#undef TEXT
}

TEST_F(LexerTest_Lv1, if_tok) {
    const char *text = "if";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(IF, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, importenv_tok) {
    const char *text = "import-env";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(IMPORT_ENV, text, EOS, "");
        this->assertLexerMode(yycNAME);
    });
}

TEST_F(LexerTest_Lv1, interface_tok) {
    const char *text = "interface";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(INTERFACE, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, let_tok) {
    const char *text = "let";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(LET, text, EOS, "");
        this->assertLexerMode(yycNAME);
    });
}

TEST_F(LexerTest_Lv1, new_tok) {
    const char *text = "new";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(NEW, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, not_tok) {
    const char *text = "not";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(NOT, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, return_tok) {
    const char *text = "return";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(RETURN, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, try_tok) {
    const char *text = "try";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(TRY, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, throw_tok) {
    const char *text = "throw";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(THROW, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, alias_tok) {
    const char *text = "alias";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(ALIAS, text, EOS, "");
        this->assertLexerMode(yycNAME);
    });
}

TEST_F(LexerTest_Lv1, var_tok) {
    const char *text = "var";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(VAR, text, EOS, "");
        this->assertLexerMode(yycNAME);
    });
}

TEST_F(LexerTest_Lv1, while_tok1) {
    const char *text = "while";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(WHILE, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, while_tok2) {
    const char *text = "while";
    SCOPED_TRACE("");
    this->initLexer(text, yycEXPR);
    ASSERT_NO_FATAL_FAILURE(EXPECT(WHILE, text, EOS, ""));
    ASSERT_NO_FATAL_FAILURE(this->assertLexerMode(yycSTMT));
}

TEST_F(LexerTest_Lv1, plus_tok1) {
    const char *text = "+";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(PLUS, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, plus_tok2) {
    const char *text = "+";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(PLUS, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, minus_tok1) {
    const char *text = "-";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(MINUS, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, minus_tok2) {
    const char *text = "-";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(MINUS, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

/**
 * literal test
 */
// integer literal
TEST_F(LexerTest_Lv1, int_literal1) {
    const char *text = "0";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(INT32_LITERAL, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, int_literal2) {
    const char *text = "123408";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(INT32_LITERAL, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, int_literal3) {
    const char *text = "9";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(INT32_LITERAL, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, int_literal4) {
    const char *text = "759801";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(INT32_LITERAL, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, int_literal5) {
    const char *text = "0xabcdef0123456789ABCDEF";
    this->initLexer(text);
    ASSERT_NO_FATAL_FAILURE(EXPECT(INT32_LITERAL, text, EOS, ""));
    ASSERT_NO_FATAL_FAILURE(this->assertLexerMode(yycEXPR));
}

TEST_F(LexerTest_Lv1, int_literal6) {
    const char *text = "0xabcdef0123456789ABCDEF_u";
    this->initLexer(text);
    ASSERT_NO_FATAL_FAILURE(EXPECT(UINT32_LITERAL, text, EOS, ""));
    ASSERT_NO_FATAL_FAILURE(this->assertLexerMode(yycEXPR));
}

TEST_F(LexerTest_Lv1, int_literal7) {
    const char *text = "0o12345670_b";
    this->initLexer(text);
    ASSERT_NO_FATAL_FAILURE(EXPECT(BYTE_LITERAL, text, EOS, ""));
    ASSERT_NO_FATAL_FAILURE(this->assertLexerMode(yycEXPR));
}

// invalid int literal
TEST_F(LexerTest_Lv1, invaild_int_literal1) {
    const char *text = "014";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(INT32_LITERAL, "0", INVALID, "1");
    });
}

TEST_F(LexerTest_Lv1, invalid_int_literal2) {
    const char *text = "-23";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(MINUS, "-", INT32_LITERAL, "23", EOS, "");
    });
}

// float literal
TEST_F(LexerTest_Lv1, float_literal1) {
    const char *text = "0.010964";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(FLOAT_LITERAL, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, float_literal2) {
    const char *text = "103.0109640";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(FLOAT_LITERAL, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, float_literal3) {
    const char *text = "0.010964e0";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(FLOAT_LITERAL, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, float_literal4) {
    const char *text = "12.010964E-102";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(FLOAT_LITERAL, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, float_literal5) {
    const char *text = "0.00";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(FLOAT_LITERAL, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

// invalid float literal
TEST_F(LexerTest_Lv1, invalid_float_literal1) {
    const char *text = "0.010964e+01";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(FLOAT_LITERAL, "0.010964e+0", INVALID, "1");
    });
}

TEST_F(LexerTest_Lv1, invalid_float_literal2) {
    const char *text = "0012.04e-78";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(INT32_LITERAL, "0", INVALID, "0");
    });
}

// string literal
TEST_F(LexerTest_Lv1, string_literal1) {
    const char *text = "''";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(STRING_LITERAL, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, string_literal2) {
    const char *text = "'fhrωu4あ\t3\"5^*&!@#~AFG '";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(STRING_LITERAL, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, string_literal3) {
    const char *text = "'\\t\\n\\r\\\\'";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(STRING_LITERAL, "'\\t\\n\\r\\\\'", EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, string_litearl4) {
    const char *text = "'\n\thoge\\\"'";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycCMD);
        EXPECT(STRING_LITERAL, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, estring_literal1) {
    const char *text = "$'\\''";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(STRING_LITERAL, "$'\\''", EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, estring_literal2) {
    const char *text = "$'\\n'";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(STRING_LITERAL, "$'\\n'", EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, estring_literal3) {
    const char *text = "$'\\\\'";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(STRING_LITERAL, "$'\\\\'", EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, estring_literal4) {
    const char *text = "$'\\\\'";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycCMD);
        EXPECT(STRING_LITERAL, "$'\\\\'", EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}


// invalid string literal
TEST_F(LexerTest_Lv1, invalid_string_literal) {
    const char *text = "'\\''";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(STRING_LITERAL, "'\\'", INVALID, "'");
    });
}

TEST_F(LexerTest_Lv1, path_literal1) {
    const char *text = "p'/hoge'";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(PATH_LITERAL, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, path_literal2) {
    const char *text = "p'/org/example'";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(PATH_LITERAL, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, invalid_path_iteral1) {
    const char *text = "p'hoge'";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, "p", STRING_LITERAL, "'hoge'", EOS, "");
    });
}


TEST_F(LexerTest_Lv1, string_expr1) {
    const char *text = "\"hello word\"";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(OPEN_DQUOTE, "\"", STR_ELEMENT, "hello word", CLOSE_DQUOTE, "\"", EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, string_expr2) {
    const char *text = "\"hello ${a} word\"";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(OPEN_DQUOTE, "\"", STR_ELEMENT, "hello ",
               APPLIED_NAME, "${a}", STR_ELEMENT, " word",
               CLOSE_DQUOTE, "\"", EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, string_expr3) {
    const char *text = "\"hello\\\"world\"";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(OPEN_DQUOTE, "\"", STR_ELEMENT, "hello\\\"world",
               CLOSE_DQUOTE, "\"", EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, string_expr4) {
    const char *text = "\"hello\\$world\"";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(OPEN_DQUOTE, "\"", STR_ELEMENT, "hello\\$world",
               CLOSE_DQUOTE, "\"", EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, string_expr5) {
    const char *text = "\"\\\\\"";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(OPEN_DQUOTE, "\"", STR_ELEMENT, "\\\\",
               CLOSE_DQUOTE, "\"", EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, string_expr6) {
    const char *text = "\"\n\t\\$\\\\$(ls)hoge\"";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycCMD);
        EXPECT(OPEN_DQUOTE, "\"", STR_ELEMENT, "\n\t\\$\\\\",
               START_SUB_CMD, "$(", COMMAND, "ls", RP, ")", STR_ELEMENT, "hoge",
               CLOSE_DQUOTE, "\"", EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, invalid_string_expr) {
    const char *text = "\"hello$\"";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(OPEN_DQUOTE, "\"", STR_ELEMENT, "hello", INVALID, "$");
    });
}

// regex literal
TEST_F(LexerTest_Lv1, regex1) {
    const char *text = "$/hoge/";
    this->initLexer(text);
    ASSERT_NO_FATAL_FAILURE(EXPECT(REGEX_LITERAL, text, EOS, ""));
    ASSERT_NO_FATAL_FAILURE(this->assertLexerMode(yycEXPR));
}

TEST_F(LexerTest_Lv1, regex2) {
    const char *text = "$/ho\\/ge/";
    this->initLexer(text);
    ASSERT_NO_FATAL_FAILURE(EXPECT(REGEX_LITERAL, text, EOS, ""));
    ASSERT_NO_FATAL_FAILURE(this->assertLexerMode(yycEXPR));
}

TEST_F(LexerTest_Lv1, invalid_regex1) {
    const char *text = "$/ho/ge/";
    this->initLexer(text);
    ASSERT_NO_FATAL_FAILURE(EXPECT(REGEX_LITERAL, "$/ho/", INVALID, "g"));
    ASSERT_NO_FATAL_FAILURE(this->assertLexerMode(yycEXPR));
}

TEST_F(LexerTest_Lv1, invalid_regex2) {
    const char *text = "$/ho\nge/";
    this->initLexer(text);
    ASSERT_NO_FATAL_FAILURE(EXPECT(INVALID, "$"));
}

TEST_F(LexerTest_Lv1, signal1) {
    const char *text = "%'de1'";
    this->initLexer(text);
    ASSERT_NO_FATAL_FAILURE(EXPECT(SIGNAL_LITERAL, text, EOS, ""));
    ASSERT_NO_FATAL_FAILURE(this->assertLexerMode(yycEXPR));
}

TEST_F(LexerTest_Lv1, signal2) {
    const char *text = "%'INT'";
    this->initLexer(text);
    ASSERT_NO_FATAL_FAILURE(EXPECT(SIGNAL_LITERAL, text, EOS, ""));
    ASSERT_NO_FATAL_FAILURE(this->assertLexerMode(yycEXPR));
}

TEST_F(LexerTest_Lv1, invalid_signal) {
    const char *text = "%'000'";
    this->initLexer(text);
    ASSERT_NO_FATAL_FAILURE(EXPECT(COMMAND, "%", STRING_LITERAL, "'000'", EOS, ""));
    ASSERT_NO_FATAL_FAILURE(this->assertLexerMode(yycSTMT));
}


TEST_F(LexerTest_Lv1, subCmd1) {
    const char *text = "$(";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(START_SUB_CMD, text, EOS, "");
        this->assertLexerMode(yycSTMT);
        this->lexer->popLexerMode();
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, subCmd2) {
    const char *text = "$(";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycDSTRING);
        EXPECT(START_SUB_CMD, text, EOS, "");
        this->assertLexerMode(yycSTMT);
        this->lexer->popLexerMode();
        this->assertLexerMode(yycDSTRING);
    });
}

TEST_F(LexerTest_Lv1, subCmd3) {
    const char *text = "$(";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycCMD);
        EXPECT(START_SUB_CMD, text, EOS, "");
        this->assertLexerMode(yycSTMT);
        this->lexer->popLexerMode();
        this->assertLexerMode(yycCMD);
    });
}

// applied name
TEST_F(LexerTest_Lv1, appliedName1) {
    const char *text = "$w10i_fArhue";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(APPLIED_NAME, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, appliedName2) {
    const char *text ="$__0";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(APPLIED_NAME, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, appliedName3) {
    const char *text = "$hoge";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(APPLIED_NAME, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, appliedName4) {
    const char *text = "$ab02";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycDSTRING);
        EXPECT(APPLIED_NAME, text, STR_ELEMENT, "\n", EOS, "");
        this->assertLexerMode(yycDSTRING);
    });
}

TEST_F(LexerTest_Lv1, appliedName5) {
    const char *text = "${ab02}";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycDSTRING);
        EXPECT(APPLIED_NAME, text, STR_ELEMENT, "\n", EOS, "");
        this->assertLexerMode(yycDSTRING);
    });
}

TEST_F(LexerTest_Lv1, appliedName6) {
    const char *text = "$ab02";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycCMD);
        EXPECT(APPLIED_NAME, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, appliedName7) {
    const char *text = "${ab02}";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycCMD);
        EXPECT(APPLIED_NAME, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, appliedName8) {
    const char *text = "$hello[";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycCMD);
        EXPECT(APPLIED_NAME_WITH_BRACKET, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

// special name
TEST_F(LexerTest_Lv1, specialName1) {
    const char *text = "$@";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(SPECIAL_NAME, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, specialName2) {
    const char *text = "$0";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(SPECIAL_NAME, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, specialName3) {
    const char *text = "$?";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(SPECIAL_NAME, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, specialName4) {
    const char *text = "$@";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycDSTRING);
        EXPECT(SPECIAL_NAME, text, STR_ELEMENT, "\n", EOS, "");
        this->assertLexerMode(yycDSTRING);
    });
}

TEST_F(LexerTest_Lv1, specialName5) {
    const char *text = "${0}";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycDSTRING);
        EXPECT(SPECIAL_NAME, text, STR_ELEMENT, "\n", EOS, "");
        this->assertLexerMode(yycDSTRING);
    });
}

TEST_F(LexerTest_Lv1, specialName6) {
    const char *text = "$?";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycCMD);
        EXPECT(SPECIAL_NAME, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, specialName7) {
    const char *text = "${@}";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycCMD);
        EXPECT(SPECIAL_NAME, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, specialName8) {
    const char *text = "$#";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(SPECIAL_NAME, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, specialName9) {
    const char *text = "$6";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(SPECIAL_NAME, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, specialName10) {
    const char *text = "${9}";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycCMD);
        EXPECT(SPECIAL_NAME, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, specialName11) {
    const char *text = "$2[";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycCMD);
        EXPECT(SPECIAL_NAME_WITH_BRACKET, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, specialName12) {
    const char *text = "$?[";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycCMD);
        EXPECT(SPECIAL_NAME_WITH_BRACKET, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}


/**
 * brace test
 */
TEST_F(LexerTest_Lv1, LP1) {
    const char *text = "(";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(LP, text, EOS, "");
        this->assertLexerMode(yycSTMT);
        this->lexer->popLexerMode();
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, LP2) {
    const char *text = "(";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(LP, text, EOS, "");
        this->assertLexerMode(yycSTMT);
        this->lexer->popLexerMode();
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, RP1) {
    const char *text = ")";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(INVALID, ")");
    });
}

TEST_F(LexerTest_Lv1, RP2) {
    const char *text = ")";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycSTMT);
        EXPECT(RP, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, RP3) {
    const char *text = ")";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        this->lexer->pushLexerMode(yycEXPR);
        EXPECT(RP, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, LB1) {
    const char *text = "[";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(LB, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, LB2) {
    const char *text = "[";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(LB, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}


TEST_F(LexerTest_Lv1, RB1) {
    const char *text = "]";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(INVALID, text);
    });
}

TEST_F(LexerTest_Lv1, RB2) {
    const char *text = "]";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycEXPR);
        EXPECT(RB, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, RB3) {
    const char *text = "]";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycSTMT);
        EXPECT(RB, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, LBC1) {
    const char *text = "{";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(LBC, text, EOS, "");
        this->assertLexerMode(yycSTMT);
        this->lexer->popLexerMode();
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, LBC2) {
    const char *text = "{";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(LBC, text, EOS, "");
        this->assertLexerMode(yycSTMT);
        this->lexer->popLexerMode();
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, RBC1) {
    const char *text = "}";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(INVALID, "}");
    });
}

TEST_F(LexerTest_Lv1, RBC2) {
    const char *text = "}";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycSTMT);
        EXPECT(RBC, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, RBC3) {
    const char *text = "}";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycEXPR);
        EXPECT(RBC, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

/*
 * command token
 */
TEST_F(LexerTest_Lv1, CMD1) {
    const char *text = "\\assert";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, "\\assert", EOS, "");
    });
}

TEST_F(LexerTest_Lv1, CMD2) {
    const char *text = "\\ \\t\\r\\n\\;\\'\\\"\\`\\|\\&\\<\\>\\(\\)\\{\\}\\$\\#\\!\\[\\]\\8";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, CMD3) {
    const char *text = "あ漢ω";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, CMD4) {
    const char *text = "l\\";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, "l\\\n", EOS, "");
    });
}

TEST_F(LexerTest_Lv1, CMD5) {
    const char *text = "\\";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(EOS, "");
    });
}

TEST_F(LexerTest_Lv1, CMD6) {
    const char *text = "d#d";
    SCOPED_TRACE("");

    this->initLexer(text);
    ASSERT_NO_FATAL_FAILURE(EXPECT(COMMAND, text, EOS, ""));
}

TEST_F(LexerTest_Lv1, CMD7) {
    const char *text = "d#";
    SCOPED_TRACE("");

    this->initLexer(text);
    ASSERT_NO_FATAL_FAILURE(EXPECT(COMMAND, text, EOS, ""));
}

TEST_F(LexerTest_Lv1, CMD8) {
    const char *text = "d!!";
    SCOPED_TRACE("");

    this->initLexer(text);
    ASSERT_NO_FATAL_FAILURE(EXPECT(COMMAND, text, EOS, ""));
}

TEST_F(LexerTest_Lv1, CMD9) {
    const char *text = "!hoge";
    SCOPED_TRACE("");

    this->initLexer(text);
    ASSERT_NO_FATAL_FAILURE(EXPECT(NOT, "!", COMMAND, "hoge", EOS, ""));
}

TEST_F(LexerTest_Lv1, CMD10) {
    const char *text = "\\$true";
    SCOPED_TRACE("");

    this->initLexer(text);
    ASSERT_NO_FATAL_FAILURE(EXPECT(COMMAND, text, EOS, ""));
}

TEST_F(LexerTest_Lv1, CMD11) {
    const char *text = "\\[\\]true";
    SCOPED_TRACE("");

    this->initLexer(text);
    ASSERT_NO_FATAL_FAILURE(EXPECT(COMMAND, text, EOS, ""));
}

TEST_F(LexerTest_Lv1, CMD12) {
    const char *text = "/usr/bin/[";
    SCOPED_TRACE("");

    this->initLexer(text);
    ASSERT_NO_FATAL_FAILURE(EXPECT(COMMAND, text, EOS, ""));
}

TEST_F(LexerTest_Lv1, CMD13) {
    const char *text = "/usr/bin/]";
    SCOPED_TRACE("");

    this->initLexer(text);
    ASSERT_NO_FATAL_FAILURE(EXPECT(COMMAND, text, EOS, ""));
}

TEST_F(LexerTest_Lv1, CMD_ARG1) {   // allow  '[' and ']'
    const char *text = "[[][";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycCMD);
        EXPECT(CMD_ARG_PART, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, CMD_ARG2) {
    const char *text = "abcd";
    SCOPED_TRACE("");

    this->initLexer(text);
    this->lexer->pushLexerMode(yycCMD);
    ASSERT_NO_FATAL_FAILURE(EXPECT(CMD_ARG_PART, text, EOS, ""));
}

TEST_F(LexerTest_Lv1, CMD_ARG3) {
    const char *text = "a2134:*";
    SCOPED_TRACE("");

    this->initLexer(text);
    this->lexer->pushLexerMode(yycCMD);
    ASSERT_NO_FATAL_FAILURE(EXPECT(CMD_ARG_PART, text, EOS, ""));
}

TEST_F(LexerTest_Lv1, CMD_ARG4) {
    const char *text = "\\;あω𪗱";
    SCOPED_TRACE("");

    this->initLexer(text);
    this->lexer->pushLexerMode(yycCMD);
    ASSERT_NO_FATAL_FAILURE(EXPECT(CMD_ARG_PART, text, EOS, ""));
}

TEST_F(LexerTest_Lv1, CMD_ARG5) {
    const char *text = "??\\\n";
    SCOPED_TRACE("");

    this->initLexer(text);
    this->lexer->pushLexerMode(yycCMD);
    ASSERT_NO_FATAL_FAILURE(EXPECT(CMD_ARG_PART, text, EOS, ""));
}

TEST_F(LexerTest_Lv1, CMD_ARG6) {
    const char *text = "{}}{";
    SCOPED_TRACE("");

    this->initLexer(text);
    this->lexer->pushLexerMode(yycCMD);
    ASSERT_NO_FATAL_FAILURE(EXPECT(CMD_ARG_PART, text, EOS, ""));
}

TEST_F(LexerTest_Lv1, CMD_ARG7) {
    const char *text = "hfeiru#fr";
    SCOPED_TRACE("");

    this->initLexer(text);
    this->lexer->pushLexerMode(yycCMD);
    ASSERT_NO_FATAL_FAILURE(EXPECT(CMD_ARG_PART, text, EOS, ""));
}

TEST_F(LexerTest_Lv1, CMD_ARG8) {
    const char *text = "ああ#";
    SCOPED_TRACE("");

    this->initLexer(text);
    this->lexer->pushLexerMode(yycCMD);
    ASSERT_NO_FATAL_FAILURE(EXPECT(CMD_ARG_PART, text, EOS, ""));
}

TEST_F(LexerTest_Lv1, CMD_ARG9) {
    const char *text = "!hoge";
    SCOPED_TRACE("");

    this->initLexer(text);
    this->lexer->pushLexerMode(yycCMD);
    ASSERT_NO_FATAL_FAILURE(EXPECT(CMD_ARG_PART, text, EOS, ""));
}

TEST_F(LexerTest_Lv1, CMD_ARG10) {
    const char *text = "qwwre!";
    SCOPED_TRACE("");

    this->initLexer(text);
    this->lexer->pushLexerMode(yycCMD);
    ASSERT_NO_FATAL_FAILURE(EXPECT(CMD_ARG_PART, text, EOS, ""));
}

TEST_F(LexerTest_Lv1, CMD_ARG11) {
    const char *text = "\\;q\\$wwre!";
    SCOPED_TRACE("");

    this->initLexer(text);
    this->lexer->pushLexerMode(yycCMD);
    ASSERT_NO_FATAL_FAILURE(EXPECT(CMD_ARG_PART, text, EOS, ""));
}

TEST_F(LexerTest_Lv1, CMD_ARG12) {
    const char *text = "q\\ w\\\twre!";
    SCOPED_TRACE("");

    this->initLexer(text);
    this->lexer->pushLexerMode(yycCMD);
    ASSERT_NO_FATAL_FAILURE(EXPECT(CMD_ARG_PART, text, EOS, ""));
}

/**
 * test expr token in stmt mode.
 */
TEST_F(LexerTest_Lv1, COLON1) {
    const char *text = ":";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, COLON2) {
    const char *text = ":";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(COLON, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, COMMA1) {
    const char *text = ",";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, COMMA2) {
    const char *text = ",";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(COMMA, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, MUL1) {
    const char *text = "*";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, MUL2) {
    const char *text = "*";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(MUL, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, DIV1) {
    const char *text = "/";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, DIV2) {
    const char *text = "/";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(DIV, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, MOD1) {
    const char *text = "%";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, MOD2) {
    const char *text = "%";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(MOD, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, LT1) {
    const char *text = "<";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(INVALID, "<");
    });
}

TEST_F(LexerTest_Lv1, LT2) {
    const char *text = "<";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(LA, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, GT1) {
    const char *text = ">";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(INVALID, ">");
    });
}

TEST_F(LexerTest_Lv1, GT2) {
    const char *text = ">";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(RA, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, LE1) {
    const char *text = "<=";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(INVALID, "<");
    });
}

TEST_F(LexerTest_Lv1, LE2) {
    const char *text = "<=";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(LE, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, GE1) {
    const char *text = ">=";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(INVALID, ">");
    });
}

TEST_F(LexerTest_Lv1, GE2) {
    const char *text = ">=";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(GE, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, EQ1) {
    const char *text = "==";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, EQ2) {
    const char *text = "==";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(EQ, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, NE1) {
    const char *text = "!=";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(NOT, "!", COMMAND, "=", EOS, "");
    });
}

TEST_F(LexerTest_Lv1, NE2) {
    const char *text = "!=";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(NE, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, AND1) {
    const char *text = "and";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, "and", EOS, "");
    });
}

TEST_F(LexerTest_Lv1, AND2) {
    const char *text = "and";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(AND, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, BG1) {
    const char *text = "&";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycCMD);
        EXPECT(BACKGROUND, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, BG2) {
    const char *text = "&";
    this->initLexer(text, yycEXPR);
    ASSERT_NO_FATAL_FAILURE(EXPECT(BACKGROUND, text, EOS, ""));
}

TEST_F(LexerTest_Lv1, BG3) {
    const char *text = "&!";
    this->initLexer(text, yycEXPR);
    ASSERT_NO_FATAL_FAILURE(EXPECT(DISOWN_BG, text, EOS, ""));
}

TEST_F(LexerTest_Lv1, BG4) {
    const char *text = "&!";
    this->initLexer(text);
    this->lexer->pushLexerMode(yycCMD);
    ASSERT_NO_FATAL_FAILURE(EXPECT(DISOWN_BG, text, EOS, ""));
}

TEST_F(LexerTest_Lv1, BG5) {
    const char *text = "&|";
    this->initLexer(text, yycEXPR);
    ASSERT_NO_FATAL_FAILURE(EXPECT(DISOWN_BG, text, EOS, ""));
}

TEST_F(LexerTest_Lv1, BG6) {
    const char *text = "&|";
    this->initLexer(text);
    this->lexer->pushLexerMode(yycCMD);
    ASSERT_NO_FATAL_FAILURE(EXPECT(DISOWN_BG, text, EOS, ""));
}

TEST_F(LexerTest_Lv1, OR1) {
    const char *text = "or";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, "or", EOS, "");
    });
}

TEST_F(LexerTest_Lv1, OR2) {
    const char *text = "or";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(OR, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, PIPE1) {
    const char *text = "|";
    this->initLexer(text, yycEXPR);
    ASSERT_NO_FATAL_FAILURE(EXPECT(PIPE, text, EOS, ""));
    ASSERT_NO_FATAL_FAILURE(this->assertLexerMode(yycSTMT));
}

TEST_F(LexerTest_Lv1, PIPE2) {
    const char *text = "|";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycCMD);
        EXPECT(PIPE, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, XOR1) {
    const char *text = "xor";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, "xor", EOS, "");
    });
}

TEST_F(LexerTest_Lv1, XOR2) {
    const char *text = "xor";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(XOR, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, XOR3) {   // invalid
    const char *text = "^";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(INVALID, "^");
    });
}

TEST_F(LexerTest_Lv1, COND_AND1) {
    const char *text = "&&";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(INVALID, "&");
    });
}

TEST_F(LexerTest_Lv1, COND_AND2) {
    const char *text = "&&";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(COND_AND, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, COND_AND3) {
    const char *text = "&&";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycCMD);
        EXPECT(COND_AND, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, COND_OR1) {
    const char *text = "||";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(INVALID, "|");
    });
}

TEST_F(LexerTest_Lv1, COND_OR2) {
    const char *text = "||";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(COND_OR, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, COND_OR3) {
    const char *text = "||";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        this->lexer->pushLexerMode(yycCMD);
        EXPECT(COND_OR, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, MATCH1) {
    const char *text = "=~";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, MATCH2) {
    const char *text = "=~";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(MATCH, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, UNMATCH1) {
    const char *text = "!~";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(NOT, "!", COMMAND, "~", EOS, "");
    });
}

TEST_F(LexerTest_Lv1, UNMATCH2) {
    const char *text = "!~";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(UNMATCH, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, INC1) {
    const char *text = "++";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(PLUS, "+", PLUS, "+", EOS, "");
    });
}

TEST_F(LexerTest_Lv1, INC2) {
    const char *text = "++";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(INC, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, DEC1) {
    const char *text = "--";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(MINUS, "-", MINUS, "-", EOS, "");
    });
}

TEST_F(LexerTest_Lv1, DEC2) {
    const char *text = "--";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(DEC, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, ASSIGN1) {
    const char *text = "=";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, ASSIGN2) {
    const char *text = "=";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(ASSIGN, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, ADD_ASSIGN1) {
    const char *text = "+=";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(PLUS, "+", COMMAND, "=", EOS, "");
    });
}

TEST_F(LexerTest_Lv1, ADD_ASSIGN2) {
    const char *text = "+=";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(ADD_ASSIGN, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, SUB_ASSIGN1) {
    const char *text = "-=";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(MINUS, "-", COMMAND, "=", EOS, "");
    });
}

TEST_F(LexerTest_Lv1, SUB_ASSIGN2) {
    const char *text = "-=";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(SUB_ASSIGN, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, MUL_ASSIGN1) {
    const char *text = "*=";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, MUL_ASSIGN2) {
    const char *text = "*=";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(MUL_ASSIGN, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, DIV_ASSIGN1) {
    const char *text = "/=";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, DIV_ASSIGN2) {
    const char *text = "/=";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(DIV_ASSIGN, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, MOD_ASSIGN1) {
    const char *text = "%=";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, MOD_ASSIGN2) {
    const char *text = "%=";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(MOD_ASSIGN, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, AS1) {
    const char *text = "as";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, AS2) {
    const char *text = "as";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(AS, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, AS3) {
    const char *text = "as";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycNAME);
        EXPECT(IDENTIFIER, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, FUNC1) {
    const char *text = "Func";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, FUNC2) {
    const char *text = "Func";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycTYPE);
        EXPECT(FUNC, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, FUNC3) {
    const char *text = "Func";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycNAME);
        EXPECT(IDENTIFIER, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, IN1) {
    const char *text = "in";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, IN2) {
    const char *text = "in";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(IN, text, EOS, "");
        this->assertLexerMode(yycSTMT);
    });
}

TEST_F(LexerTest_Lv1, IN3) {
    const char *text = "in";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycNAME);
        EXPECT(IDENTIFIER, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, IS1) {
    const char *text = "is";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, IS2) {
    const char *text = "is";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(IS, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, IS3) {
    const char *text = "is";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycNAME);
        EXPECT(IDENTIFIER, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, TYPEOF1) {
    const char *text = "typeof";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycTYPE);
        EXPECT(TYPEOF, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, TYPEOF2) {
    const char *text = "typeof";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, TYPE_PATH) {
    const char *text = "org.freedesktop";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycTYPE);
        EXPECT(TYPE_PATH, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, NAME1) {
    const char *text = "assert";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycNAME);
        EXPECT(IDENTIFIER, text, EOS, "");
        this->assertLexerMode(yycEXPR);
    });
}

TEST_F(LexerTest_Lv1, NAME2) {
    const char *text = "assert";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycTYPE);
        EXPECT(IDENTIFIER, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, ACCESS1) {
    const char *text = ".";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, ACCESS2) {
    const char *text = ".";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text, yycEXPR);
        EXPECT(ACCESSOR, text, EOS, "");
        this->assertLexerMode(yycNAME);
    });
}

/**
 * new line, space and comment
 */
TEST_F(LexerTest_Lv1, LINE_END1) {
    const char *text = ";";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(LINE_END, text, EOS, "");
    });
}

TEST_F(LexerTest_Lv1, LINE_END2) {
    const char *text = ";";
    SCOPED_TRACE("");

    this->initLexer(text);
    this->lexer->pushLexerMode(yycEXPR);
    ASSERT_NO_FATAL_FAILURE(EXPECT(LINE_END, text, EOS, ""));
    ASSERT_NO_FATAL_FAILURE(this->assertLexerMode(yycSTMT));
}

TEST_F(LexerTest_Lv1, LINE_END3) {
    const char *text = ";";
    SCOPED_TRACE("");

    this->initLexer(text);
    this->lexer->pushLexerMode(yycCMD);
    ASSERT_NO_FATAL_FAILURE(EXPECT(LINE_END, text, EOS, ""));
    ASSERT_NO_FATAL_FAILURE(this->assertLexerMode(yycSTMT));
}

TEST_F(LexerTest_Lv1, COMMENT1) {
    const char *text = "#fhreuvrei o";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(EOS, "");
    });
}

TEST_F(LexerTest_Lv1, COMMENT2) {
    const char *text = "#ああ  あ";
    SCOPED_TRACE("");

    this->initLexer(text, yycEXPR);
    ASSERT_NO_FATAL_FAILURE(EXPECT(EOS, ""));
}

TEST_F(LexerTest_Lv1, COMMENT3) {
    const char *text = "#hello  \t  ";
    SCOPED_TRACE("");

    this->initLexer(text, yycNAME);
    ASSERT_NO_FATAL_FAILURE(EXPECT(EOS, ""));
}

TEST_F(LexerTest_Lv1, COMMENT4) {
    const char *text = "#hferu";
    SCOPED_TRACE("");

    this->initLexer(text);
    this->lexer->pushLexerMode(yycTYPE);
    ASSERT_NO_FATAL_FAILURE(EXPECT(EOS, ""));
}

TEST_F(LexerTest_Lv1, COMMENT5) {
    const char *text = "#2345y;;::";
    SCOPED_TRACE("");

    this->initLexer(text);
    this->lexer->pushLexerMode(yycCMD);
    ASSERT_NO_FATAL_FAILURE(EXPECT(EOS, ""));
}

TEST_F(LexerTest_Lv1, EMPTY) {
const char *text = "";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(EOS, "");
    });
}

TEST_F(LexerTest_Lv1, SPACE1) {
    const char *text = "    \t   \n  ";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(EOS, "");
    });
}

TEST_F(LexerTest_Lv1, SPACE2) {
    const char *text = "   \n var \\\r\\\n";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(VAR, "var", EOS, "");
    });
}

TEST_F(LexerTest_Lv1, SPACE3) {
    const char *text = "\n  \n assert \\\r\\\n";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(ASSERT, "assert", EOS, "");
    });
}

TEST_F(LexerTest_Lv1, SPACE4) {
    const char *text = "\\\r\\\necho";
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        this->initLexer(text);
        EXPECT(COMMAND, "echo", EOS, "");
    });
}

TEST_F(LexerTest_Lv1, SPACE5) {
    const char *text = "    \t   \n  \\\n\\\r ";
    SCOPED_TRACE("");

    this->initLexer(text, yycEXPR);
    ASSERT_NO_FATAL_FAILURE(EXPECT(EOS, ""));
}

TEST_F(LexerTest_Lv1, SPACE6) {
    const char *text = "     \t    \n  \\\n\\\r    \t";
    SCOPED_TRACE("");

    this->initLexer(text);
    this->lexer->pushLexerMode(yycTYPE);
    ASSERT_NO_FATAL_FAILURE(EXPECT(EOS, ""));
}

TEST(LexerTest_Lv2, NEW_LINE) {
    ASSERT_NO_FATAL_FAILURE({
        SCOPED_TRACE("");
        Lexer lexer("(string)", "  \n  \n   assert  \n ");
        Token t;
        TokenKind k = lexer.nextToken(t);
        ASSERT_STREQ(TO_NAME(ASSERT), TO_NAME(k));
        ASSERT_TRUE(lexer.isPrevNewLine());

        k = lexer.nextToken(t);
        ASSERT_STREQ(TO_NAME(EOS), TO_NAME(k));
        ASSERT_TRUE(lexer.isPrevNewLine());
        k = lexer.nextToken(t);
        ASSERT_STREQ(TO_NAME(EOS), TO_NAME(k));
        ASSERT_FALSE(lexer.isPrevNewLine());
    });
}

TEST(LexerTest_Lv3, IllegalChar) {
    SCOPED_TRACE("");

    unsigned char str[] = {0x82, 0};    // broken UTF-8 code

    Lexer lexer("(string)", (char *)str);
    Token t;
    TokenKind k = lexer.nextToken(t);

    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ(TO_NAME(INVALID), TO_NAME(k)));
}

TEST(LineNumTest, case1) {
    SourceInfo info("dummy");
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(std::string("dummy"), info.getSourceName()));
}

TEST(LineNumTest, case2) {
    SourceInfo info("dummy");
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(1u, info.getLineNum(12))); // empty
}

TEST(LineNumTest, case3) {
    SourceInfo info("dummy");
    info.addNewlinePos(5);
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(1u, info.getLineNum(3)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(1u, info.getLineNum(4)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(1u, info.getLineNum(5)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(2u, info.getLineNum(6)));
}

TEST(LineNumTest, case4) {
    SourceInfo info("dummy");
    info.addNewlinePos(5);
    info.addNewlinePos(4);  // overwrite
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(1u, info.getLineNum(3)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(1u, info.getLineNum(4)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(1u, info.getLineNum(5)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(2u, info.getLineNum(6)));
}

TEST(LineNumTest, case5) {
    SourceInfo info("dummy");
    info.setLineNumOffset(4);
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(4u, info.getLineNum(5)));
    info.addNewlinePos(10);
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(4u, info.getLineNum(5)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(5u, info.getLineNum(13)));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
