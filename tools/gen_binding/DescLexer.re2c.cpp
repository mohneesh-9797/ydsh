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

#include <DescLexer.h>

// helper macro definition.
#define RET(k) do { kind = k; goto END; } while(0)

#define REACH_EOS() do { this->endOfString = true; goto EOS; } while(0)

#define SKIP() goto INIT

#define ERROR() do { RET(INVALID); } while(0)


DescTokenKind DescLexer::nextToken(Token &token) {
    /*!re2c
      re2c:define:YYCTYPE = "unsigned char";
      re2c:define:YYCURSOR = this->cursor;
      re2c:define:YYLIMIT = this->limit;
      re2c:define:YYMARKER = this->marker;
      re2c:define:YYCTXMARKER = this->ctxMarker;
      re2c:define:YYFILL:naked = 1;
      re2c:define:YYFILL@len = #;
      re2c:define:YYFILL = "if(!this->fill(#)) { REACH_EOS(); }";
      re2c:yyfill:enable = 1;
      re2c:indent:top = 1;
      re2c:indent:string = "    ";

      IDENTIFIER = [_a-zA-Z][_a-zA-Z0-9]*;
      OTHER = .;
    */

    INIT:
    unsigned int startPos = this->getPos();
    DescTokenKind kind = INVALID;
    /*!re2c
      "//!bind:"             { RET(DESC_PREFIX); }
      "function"             { RET(FUNC); }
      "constructor"          { RET(INIT); }
      "static"               { RET(STATIC); }
      "inline"               { RET(INLINE); }
      "bool"                 { RET(BOOL);}
      "RuntimeContext"       { RET(RCTX); }
      "Array"                { RET(ARRAY); }
      "Map"                  { RET(MAP); }
      "Tuple"                { RET(TUPLE); }
      IDENTIFIER             { RET(IDENTIFIER); }
      "<"                    { RET(TYPE_OPEN); }
      ">"                    { RET(TYPE_CLOSE); }
      "$" IDENTIFIER         { RET(VAR_NAME); }
      "("                    { RET(LP); }
      ")"                    { RET(RP); }
      ","                    { RET(COMMA); }
      ":"                    { RET(COLON); }
      "{"                    { RET(LBC); }
      "&"                    { RET(AND); }
      "?"                    { RET(OPT); }

      [ \t\r\n]+             { SKIP(); }
      "\000"                 { REACH_EOS(); }

      OTHER                  { RET(INVALID); }
    */

    END:
    token.pos = startPos;
    token.size = this->getPos() - startPos;
    return kind;

    EOS:
    token.pos = this->limit - this->buf;
    token.size = 0;
    return EOS;
}

const char *toString(DescTokenKind kind) {
    static const char *names[] = {
            #define GEN_NAME(TOK) #TOK,
            EACH_DESC_TOKEN(GEN_NAME)
            #undef GEN_NAME
    };

    return names[kind];
}

std::ostream &operator<<(std::ostream &stream, DescTokenKind kind) {
    return stream << toString(kind);
}