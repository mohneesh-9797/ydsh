/*
 * Copyright (C) 2015-2016 Nagisa Sekiguchi
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

#ifndef YDSH_PARSER_PARSER_BASE_HPP
#define YDSH_PARSER_PARSER_BASE_HPP

#include <vector>
#include <ostream>

#include "lexer_base.hpp"

namespace ydsh {
namespace parser_base {

template <typename T>
class ParseError {
private:
    T kind;
    Token errorToken;
    const char *errorKind;
    std::string message;

public:
    ParseError(T kind, Token errorToken, const char *errorKind, std::string &&message) :
            kind(kind), errorToken(errorToken),
            errorKind(errorKind), message(std::move(message)) { }

    ~ParseError() = default;

    const Token &getErrorToken() const {
        return this->errorToken;
    }

    T getTokenKind() const {
        return this->kind;
    }

    const char *getErrorKind() const {
        return this->errorKind;
    }

    const std::string &getMessage() const {
        return this->message;
    }
};

template<typename T, typename LexerImpl>
class ParserBase {
protected:
    LexerImpl *lexer;
    T curKind;
    Token curToken;

public:
    ParserBase() = default;

protected:
    ~ParserBase() = default;

    void fetchNext() {
        this->curKind = this->lexer->nextToken(this->curToken);
    }

    Token expect(T kind, bool fetchNext = true);

    T consume();

    void alternativeError(const unsigned int size, const T *const alters) const;

    static void raiseTokenMismatchedError(T kind, Token errorToken, T expected);
    static void raiseNoViableAlterError(T kind, Token errorToken,
                                        const unsigned int size, const T *const alters);
    static void raiseInvalidTokenError(T kind, Token errorToken);
};

// ########################
// ##     ParserBase     ##
// ########################

template<typename T, typename LexerImpl>
Token ParserBase<T, LexerImpl>::expect(T kind, bool fetchNext) {
    if(this->curKind != kind) {
        if(LexerImpl::isInvalidToken(this->curKind)) {
            raiseInvalidTokenError(this->curKind, this->curToken);
        }
        raiseTokenMismatchedError(this->curKind, this->curToken, kind);
    }
    Token token = this->curToken;
    if(fetchNext) {
        this->fetchNext();
    }
    return token;
}

template<typename T, typename LexerImpl>
T ParserBase<T, LexerImpl>::consume() {
    T kind = this->curKind;
    this->fetchNext();
    return kind;
}

template<typename T, typename LexerImpl>
void ParserBase<T, LexerImpl>::alternativeError(const unsigned int size, const T *const alters) const {
    if(LexerImpl::isInvalidToken(this->curKind)) {
        raiseInvalidTokenError(this->curKind, this->curToken);
    }
    raiseNoViableAlterError(this->curKind, this->curToken, size, alters);
}

template<typename T, typename LexerImpl>
void ParserBase<T, LexerImpl>::raiseTokenMismatchedError(T kind, Token errorToken, T expected) {
    std::string message("mismatched token: ");
    message += toString(kind);
    message += ", expected: ";
    message += toString(expected);

    throw ParseError<T>(kind, errorToken, "TokenMismatched", std::move(message));
}

template<typename T, typename LexerImpl>
void ParserBase<T, LexerImpl>::raiseNoViableAlterError(T kind, Token errorToken,
                                                       const unsigned int size, const T *const alters) {
    std::string message = "no viable alternative: ";
    message += toString(kind);
    if(size > 0 && alters != nullptr) {
        message += ", expected: ";
        for(unsigned int i = 0; i < size; i++) {
            if(i > 0) {
                message += ", ";
            }
            message += toString(alters[i]);
        }
    }

    throw ParseError<T>(kind, errorToken, "NoViableAlter", std::move(message));
}

template<typename T, typename LexerImpl>
void ParserBase<T, LexerImpl>::raiseInvalidTokenError(T kind, Token errorToken) {
    throw ParseError<T>(kind, errorToken, "InvalidToken", std::string("invalid token"));
}

} //namespace parser_base
} //namespace ydsh


#endif //YDSH_PARSER_PARSER_BASE_HPP
