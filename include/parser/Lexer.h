/*
 * Copyright (C) 2015 Nagisa Sekiguchi
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PARSER_LEXER_H_
#define PARSER_LEXER_H_

#include <stdio.h>
#include <vector>
#include <string>

#include <parser/TokenKind.h>

struct Token {
    unsigned int startPos;

    /**
     * actually EOS token text is empty string,
     * but size of EOS is 1 (due to lexer implementation).
     */
    unsigned int size;
};

typedef enum {
    yycSTMT,
    yycEXPR,
    yycDSTRING,
    yycCMD,
} LexerMode;

class Lexer {
private:
    /**
     * may be null, if input source is string. not closed it.
     */
    FILE *fp;

    unsigned int bufSize;

    /**
     * must terminate null character.
     */
    unsigned char *buf;

    /**
     * current reading pointer of buf.
     */
    unsigned char *cursor;

    /**
     * limit of buf.
     */
    unsigned char *limit;

    /**
     * for backtracking.
     */
    unsigned char *marker;

    /**
     * initial value is 1.
     */
    unsigned int lineNum;

    bool endOfFile;

    std::vector<LexerMode> modeStack;

    const static unsigned int DEFAULT_SIZE = 256;

    Lexer(unsigned int initSize, bool fixed = false);

public:
    Lexer(unsigned int initSize, FILE *fp);

    /**
     * equivalent to Lexer(DEFAULT_SIZE, fp).
     */
    Lexer(FILE *fp);

    /**
     * copy src to this->buf.
     * src must terminate null character.
     */
    Lexer(const char *src);

    /**
     * create copy of lexer.
     * internal state is initialized.
     * fp is always null.
     */
    Lexer(const Lexer &lexer);

    ~Lexer();

private:
    /**
     * used for constructor. not use it.
     */
    void copySrcBuf(const void *srcBuf);

    /**
     * if this->usedSize + needSize > this->maxSize, expand buf.
     */
    void expandBuf(unsigned int needSize);

public:
    /**
     * fill buffer. called from this->nextToken().
     */
    bool fill(int n);

    /**
     * get current reading position.
     */
    unsigned int getPos() const;

    unsigned int getBufSize() const;

    /**
     * used size of buf. must be this->getUsedSize() <= this->getBufSize().
     */
    unsigned int getUsedSize() const;

    /**
     * lexer entry point.
     * write next token to token.
     * return the kind of next token.
     */
    TokenKind nextToken(Token &token);

    void setLineNum(unsigned int lineNum);
    unsigned int getLineNum() const;

    // token to value converting api.
    std::string toTokenText(Token &token);

    /**
     * convert token to string (single quote string or double quote string)
     */
    std::string toString(Token &token, bool isSingleQuote = true);

    /**
     * convert token to name(remove '$' char)
     * ex. $hoge, ${hoge}, hoge
     */
    std::string toName(Token &token);

    int toInt(Token &token);
};

#endif /* PARSER_LEXER_H_ */
