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

#include <parser/Parser.h>
#include <parser/ParseError.h>
#include <util/debug.h>

// for debug
#ifdef NDEBUG
#define INLINE inline
#else
#define INLINE
#endif


// helper macro
#define NEXT_TOKEN() \
    do {\
        this->curTokenKind = this->lexer->nextToken(this->curToken);\
    } while(0)

#define LN() this->lexer->getLineNum()

#define HAS_NL() this->lexer->isPrevNewLine()

#define GEN_LA_CASE(CASE) case CASE:
#define GEN_LA_ALTER(CASE) CASE,

// for lookahead
#define EACH_LA_interpolation(OP) \
    OP(APPLIED_NAME) \
    OP(SPECIAL_NAME) \
    OP(START_INTERP)

#define EACH_LA_primary(OP) \
    OP(NEW) \
    OP(INT_LITERAL) \
    OP(FLOAT_LITERAL) \
    OP(STRING_LITERAL) \
    OP(OPEN_DQUOTE) \
    OP(START_SUB_CMD) \
    OP(APPLIED_NAME) \
    OP(SPECIAL_NAME) \
    OP(LP) \
    OP(LB) \
    OP(LBC)

#define EACH_LA_expression(OP) \
    OP(NOT) \
    OP(PLUS) \
    OP(MINUS) \
    EACH_LA_primary(OP)

#define EACH_LA_statement(OP) \
    OP(ASSERT) \
    OP(BREAK) \
    OP(CONTINUE) \
    OP(DO) \
    OP(EXPORT_ENV) \
    OP(FOR) \
    OP(IF) \
    OP(IMPORT_ENV) \
    OP(LET) \
    OP(RETURN) \
    OP(TRY) \
    OP(THROW) \
    OP(VAR) \
    OP(WHILE) \
    OP(COMMAND) \
    OP(LINE_END) \
    EACH_LA_expression(OP)

#define EACH_LA_toplevelStatement(OP) \
    OP(CLASS) \
    OP(FUNCTION) \
    EACH_LA_statement(OP)

#define EACH_LA_typeName(OP) \
    OP(EXTENDS) \
    OP(NEW) \
    OP(NOT) \
    OP(AS) \
    OP(IN) \
    OP(IS) \
    OP(IDENTIFIER)

#define EACH_LA_name(OP) \
    OP(FUNC) \
    EACH_LA_typeName(OP)

#define EACH_LA_varDecl(OP) \
    OP(VAR) \
    OP(LET)

#define EACH_LA_redir(OP) \
    OP(REDIR_OP) \
    OP(REDIR_OP_NO_ARG)

#define EACH_LA_cmdArg(OP) \
    OP(CMD_ARG_PART) \
    OP(STRING_LITERAL) \
    OP(OPEN_DQUOTE) \
    OP(START_SUB_CMD) \
    EACH_LA_interpolation(OP)


#define E_ALTER(alt) \
    do {\
        if(this->curTokenKind == INVALID) {\
            throw InvalidTokenError(LN(), this->curToken);\
        }\
        throw NoViableAlterError(LN(), this->curTokenKind, this->curToken, alt);\
    } while(0)

// for check converted number range
#define CONVERT_TO_NUM(out, n, kind, token, func) \
    do {\
        int status;\
        out = func(token, status);\
        if(status != 0) { throw OutOfRangeNumError(n, kind, token); }\
    } while(0)


#define RET_NODE(node) return std::unique_ptr<Node>(node)


// ####################
// ##     Parser     ##
// ####################

Parser::Parser(Lexer *lexer) :
        lexer(lexer), curTokenKind(), curToken() {
}

Parser::~Parser() {
}

void Parser::setLexer(Lexer *lexer) {
    this->lexer = lexer;
}

RootNode *Parser::parse() {
    // first, fetch token.
    NEXT_TOKEN();

    // start parsing
    std::unique_ptr<RootNode> rootNode = this->parse_toplevel();
    return rootNode.release();
}

INLINE void Parser::matchToken(TokenKind expected) {
    if(this->curTokenKind != expected) {
        if(this->curTokenKind == INVALID) {
            throw InvalidTokenError(LN(), this->curToken);
        }
        throw TokenMismatchError(LN(), this->curTokenKind, this->curToken, expected);
    }
    NEXT_TOKEN();
}

INLINE Token Parser::matchAndGetToken(TokenKind expected) {
    if(this->curTokenKind != expected) {
        if(this->curTokenKind == INVALID) {
            throw InvalidTokenError(LN(), this->curToken);
        }
        throw TokenMismatchError(LN(), this->curTokenKind, this->curToken, expected);
    }
    Token token = this->curToken;
    NEXT_TOKEN();
    return token;
}

INLINE TokenKind Parser::consumeAndGetKind() {
    TokenKind kind = this->curTokenKind;
    NEXT_TOKEN();
    return kind;
}

INLINE void Parser::hasNoNewLine() {
    if(HAS_NL()) {
        throw TokenMismatchError(LN(), NEW_LINE, this->curToken, this->curTokenKind);
    }
}

// parse rule definition

std::unique_ptr<RootNode> Parser::parse_toplevel() {
    std::unique_ptr<RootNode> rootNode(new RootNode());

    bool next = true;
    while(next) {
        switch(this->curTokenKind) {
        EACH_LA_toplevelStatement(GEN_LA_CASE) {
            // parse
            std::unique_ptr<Node> node = this->parse_toplevelStatement();

            rootNode->addNode(node.release());
            break;
        }
        default: {
            next = false;
            break;
        }
        }
    }

    this->matchToken(EOS);
    return rootNode;
}

INLINE std::unique_ptr<Node> Parser::parse_toplevelStatement() {
    static TokenKind alters[] = {
            EACH_LA_toplevelStatement(GEN_LA_ALTER)
            DUMMY
    };

    switch(this->curTokenKind) {
    case CLASS:
    case FUNCTION:
        fatal("not implemented rule: %s\n", getTokenName(this->curTokenKind)); //FIXME:
        break;
    EACH_LA_statement(GEN_LA_CASE)
        return this->parse_statement();
    default:
        E_ALTER(alters);
    }
    return std::unique_ptr<Node>(nullptr);    //FIXME:
}

std::unique_ptr<TypeToken> Parser::parse_typeName() {
    static TokenKind alters[] = {
            EACH_LA_name(GEN_LA_ALTER)
            DUMMY
    };

    switch(this->curTokenKind) {
    EACH_LA_typeName(GEN_LA_CASE) {
        auto typeToken = std::unique_ptr<ClassTypeToken>(new ClassTypeToken(LN(), this->parse_name()));
        if(!HAS_NL() && this->curTokenKind == LA) {
            this->matchToken(LA);
            this->hasNoNewLine();

            auto reified = std::unique_ptr<ReifiedTypeToken>(
                    new ReifiedTypeToken(typeToken.release()));
            reified->addElementTypeToken(this->parse_typeName().release());

            while(!HAS_NL() && this->curTokenKind == COMMA) {
                this->hasNoNewLine();
                this->matchToken(COMMA);
                this->hasNoNewLine();

                reified->addElementTypeToken(this->parse_typeName().release());
            }
            this->hasNoNewLine();
            this->matchToken(RA);
            return std::unique_ptr<TypeToken>(reified.release());
        }
        return std::unique_ptr<TypeToken>(typeToken.release());
    }
    case FUNC: {
        this->matchToken(FUNC);
        this->hasNoNewLine();
        this->matchToken(LA);
        this->hasNoNewLine();

        // parse return type
        auto func = std::unique_ptr<FuncTypeToken>(
                new FuncTypeToken(this->parse_typeName().release()));

        this->hasNoNewLine();
        if(this->curTokenKind == COMMA) {   // ,[
            this->matchToken(COMMA);
            this->hasNoNewLine();

            this->matchToken(LB);
            this->hasNoNewLine();

            // parse first arg type
            func->addParamTypeToken(this->parse_typeName().release());

            // rest arg type
            while(!HAS_NL() && this->curTokenKind == COMMA) {
                this->matchToken(COMMA);
                this->hasNoNewLine();
                func->addParamTypeToken(this->parse_typeName().release());
            }
            this->hasNoNewLine();
            this->matchToken(RB);
        }

        this->hasNoNewLine();
        this->matchAndGetToken(RA);
        return std::unique_ptr<TypeToken>(func.release());
    }
    default:
        E_ALTER(alters);
        return std::unique_ptr<TypeToken>(nullptr);
    }
}

std::unique_ptr<Node> Parser::parse_statement() {
    switch(this->curTokenKind) {
    case LINE_END: {
        unsigned int n = LN();
        this->matchToken(LINE_END);
        RET_NODE(new EmptyNode(n));
    }
    case ASSERT: {
        this->matchToken(ASSERT);
        auto node = std::unique_ptr<Node>(new AssertNode(LN(),
                this->parse_commandOrExpression().release()));
        this->parse_statementEnd();
        return node;
    }
    case BREAK: {
        this->matchToken(BREAK);
        auto node = std::unique_ptr<Node>(new BreakNode(LN()));
        this->parse_statementEnd();
        return node;
    }
    case CONTINUE: {
        this->matchToken(BREAK);
        auto node = std::unique_ptr<Node>(new ContinueNode(LN()));
        this->parse_statementEnd();
        return node;
    }
    case EXPORT_ENV: {
        this->matchToken(EXPORT_ENV);
        unsigned int n = LN();
        std::string name = this->parse_name();
        this->hasNoNewLine();
        this->matchToken(ASSIGN);
        auto node = std::unique_ptr<Node>(new ExportEnvNode(n, std::move(name),
                this->parse_expression().release()));
        this->parse_statementEnd();
        return node;
    }
    case FOR: {
        break; //FIXME:
    }
    case IF: {
        this->matchToken(IF);
        unsigned int n = LN();
        this->matchToken(LP);
        auto condNode = this->parse_commandOrExpression();
        this->matchToken(RP);
        auto thenNode = this->parse_block();

        if(this->curTokenKind == ELSE) {    //FIXME: else if
            NEXT_TOKEN();
            RET_NODE(new IfNode(n, condNode.release(),
                    thenNode.release(), this->parse_block().release()));
        } else {
            RET_NODE(new IfNode(n, condNode.release(), thenNode.release(), 0));
        }
    }
    case IMPORT_ENV: {
        this->matchToken(IMPORT_ENV);
        auto node = std::unique_ptr<Node>(new ImportEnvNode(LN(), this->parse_name()));
        this->parse_statementEnd();
        return node;
    }
    case RETURN: {
        this->matchToken(RETURN);
        unsigned int n = LN();
        auto node = std::unique_ptr<Node>(nullptr);
        if(!HAS_NL()) {
            node.reset(new ReturnNode(n));
        } else {
            node.reset(new ReturnNode(n, this->parse_expression().release()));
        }
        this->parse_statementEnd();
        return node;
    }
    case THROW: {
        this->matchToken(THROW);
        auto node = std::unique_ptr<Node>(new ThrowNode(LN(),
                this->parse_expression().release()));
        this->parse_statementEnd();
        return node;
    }
    case WHILE: {
        this->matchToken(WHILE);
        unsigned int n = LN();
        this->matchToken(LP);
        auto condNode = this->parse_commandOrExpression();
        this->matchToken(RP);
        RET_NODE(new WhileNode(n, condNode.release(), this->parse_block().release()));
    }
    case DO: {
        this->matchToken(DO);
        unsigned int n = LN();
        auto blockNode = this->parse_block();
        this->matchToken(WHILE);
        this->matchToken(LP);
        auto condNode = this->parse_commandOrExpression();
        this->matchToken(RP);
        auto node = std::unique_ptr<Node>(new DoWhileNode(n, blockNode.release(), condNode.release()));
        this->parse_statementEnd();
        return node;
    }
    case TRY: {
        this->matchToken(TRY);
        auto tryNode = std::unique_ptr<TryNode>(new TryNode(LN(), this->parse_block().release()));

        // parse catch
        bool next = true;
        while(next) {
            if(this->curTokenKind == CATCH) {
                tryNode->addCatchNode(this->parse_catchStatement().release());
            } else {
                next = false;
            }
        }

        // parse finally
        if(this->curTokenKind == FINALLY) {
            tryNode->addFinallyNode(this->parse_finallyBlock().release());
        }
        RET_NODE(tryNode.release());
    }
    case VAR:
    case LET: {
        auto node =  this->parse_variableDeclaration();
        this->parse_statementEnd();
        return node;
    }
    case COMMAND: {
        auto node = this->parse_commandListExpression();
        this->parse_statementEnd();
        return node;
    }
    default: {
        auto node = this->parse_expression();
        this->parse_statementEnd();
        return node;
    }
    }
    return std::unique_ptr<Node>(nullptr);
}

INLINE void Parser::parse_statementEnd() {
    switch(this->curTokenKind) {
    case EOS:
    case LINE_END:
        NEXT_TOKEN();
        break;
    default:
        if(!HAS_NL()) {
            throw TokenMismatchError(LN(), this->curTokenKind, this->curToken, NEW_LINE);
        }
        break;
    }
}

std::unique_ptr<BlockNode> Parser::parse_block() {
    this->matchToken(LBC);
    auto blockNode = std::unique_ptr<BlockNode>(new BlockNode);

    blockNode->addNode(this->parse_statement().release());

    bool next = true;
    while(next) {
        switch(this->curTokenKind) {
        EACH_LA_statement(GEN_LA_CASE) {
            blockNode->addNode(this->parse_statement().release());
            break;
        }
        default: {
            next = false;
            break;
        }
        }
    }

    this->matchToken(RBC);

    return blockNode;
}

INLINE std::string Parser::parse_name() {
    static TokenKind alters[] = {
            EACH_LA_name(GEN_LA_ALTER)
            DUMMY
    };

    switch(this->curTokenKind) {
    EACH_LA_name(GEN_LA_CASE) {
        Token token = this->curToken;
        NEXT_TOKEN();
        return this->lexer->toName(token);
    }
    default:
        E_ALTER(alters);
        return "";
    }
}

INLINE std::unique_ptr<Node> Parser::parse_variableDeclaration() {
    static TokenKind alters[] = {
            EACH_LA_varDecl(GEN_LA_ALTER)
            DUMMY
    };

    switch(this->curTokenKind) {
    EACH_LA_varDecl(GEN_LA_CASE) {
        bool readOnly = this->consumeAndGetKind() != VAR;
        this->hasNoNewLine();

        unsigned int n = LN();
        std::string name = this->parse_name();
        this->hasNoNewLine();
        this->matchToken(ASSIGN);
        RET_NODE(new VarDeclNode(n, std::move(name),
                this->parse_commandOrExpression().release(), readOnly));
    }
    default:
        E_ALTER(alters);
        return std::unique_ptr<Node>(nullptr);
    }
}

std::unique_ptr<CatchNode> Parser::parse_catchStatement() {
    this->matchToken(CATCH);
    unsigned int n = LN();
    this->matchToken(LP);
    Token token = this->matchAndGetToken(APPLIED_NAME);
    std::unique_ptr<TypeToken> typeToken;
    if(!HAS_NL() && this->curTokenKind == COLON) {
        this->matchToken(COLON);
        this->hasNoNewLine();
        typeToken = this->parse_typeName();
    }
    this->matchToken(RP);
    auto blockNode = this->parse_block();

    if(typeToken) {
        return std::unique_ptr<CatchNode>(new CatchNode(n, this->lexer->toName(token),
                typeToken.release(), blockNode.release()));
    } else {
        return std::unique_ptr<CatchNode>(new CatchNode(n, this->lexer->toName(token),
                blockNode.release()));
    }
}
std::unique_ptr<Node> Parser::parse_finallyBlock() {
    this->matchToken(FINALLY);
    RET_NODE(new FinallyNode(LN(), this->parse_block().release()));
}

// command
INLINE std::unique_ptr<Node> Parser::parse_commandListExpression() {
    RET_NODE(new CmdContextNode(this->parse_orListCommand().release()));
}

INLINE std::unique_ptr<Node> Parser::parse_orListCommand() {
    auto node = this->parse_andListCommand();

    while(this->curTokenKind == OR_LIST) {
        this->matchToken(OR_LIST);
        node = std::unique_ptr<Node>(new CondOpNode(node.release(),
                this->parse_andListCommand().release(), false));
    }
    return node;
}

INLINE std::unique_ptr<Node> Parser::parse_andListCommand() {
    auto node = this->parse_pipedCommand();

    while(this->curTokenKind == AND_LIST) {
        this->matchToken(AND_LIST);
        node = std::unique_ptr<Node>(new CondOpNode(node.release(),
                this->parse_pipedCommand().release(), true));
    }
    return node;
}

INLINE std::unique_ptr<Node> Parser::parse_pipedCommand() {
    auto node = this->parse_command();

    if(this->curTokenKind == PIPE) {
        auto piped = std::unique_ptr<PipedCmdNode>(new PipedCmdNode(node.release()));
        while(this->curTokenKind == PIPE) {
            this->matchToken(PIPE);
            piped->addCmdNodes(this->parse_command().release());
        }
        RET_NODE(piped.release());
    }
    RET_NODE(node.release());
}

INLINE std::unique_ptr<CmdNode> Parser::parse_command() {   //FIXME: redirect
    unsigned int n = LN();
    Token token = this->matchAndGetToken(COMMAND);
    auto node = std::unique_ptr<CmdNode>(new CmdNode(n, this->lexer->toCmdArg(token)));

    while(this->curTokenKind == CMD_SEP) {
        this->matchToken(CMD_SEP);
        node->addArgNode(this->parse_cmdArg().release());
    }
    return node;
}

INLINE std::unique_ptr<CmdArgNode> Parser::parse_cmdArg() {
    auto node = std::unique_ptr<CmdArgNode>(
            new CmdArgNode(this->parse_cmdArgSeg().release()));

    bool next = true;
    while(next) {
        switch(this->curTokenKind) {
        EACH_LA_cmdArg(GEN_LA_CASE) {
            node->addSegmentNode(this->parse_cmdArgSeg().release());
            break;
        }
        default: {
            next = false;
            break;
        }
        }
    }
    return node;
}

INLINE std::unique_ptr<Node> Parser::parse_cmdArgSeg() {
    static TokenKind alters[] = {
            EACH_LA_cmdArg(GEN_LA_ALTER)
            DUMMY
    };

    switch(this->curTokenKind) {
    case CMD_ARG_PART: {
        unsigned int n = LN();
        Token token = this->matchAndGetToken(CMD_ARG_PART);
        RET_NODE(new StringValueNode(n, this->lexer->toCmdArg(token)));
    }
    case STRING_LITERAL: {
        return this->parse_stringLiteral();
    }
    case OPEN_DQUOTE: {
        return this->parse_stringExpression();
    }
    case START_SUB_CMD: {
        return this->parse_commandSubstitution();
    }
    EACH_LA_interpolation(GEN_LA_CASE) {
        return this->parse_interpolation();
    }
    default: {
        E_ALTER(alters);
        return std::unique_ptr<Node>(nullptr);
    }
    }
}

// expression
INLINE std::unique_ptr<Node> Parser::parse_commandOrExpression() {
#define EACH_LA_condExpr(OP) \
    OP(COMMAND) \
    EACH_LA_expression(OP)

    static TokenKind alters[] = {
            EACH_LA_condExpr(GEN_LA_ALTER)
            DUMMY
    };

#undef EACH_LA_condExpr

    switch(this->curTokenKind) {
    case COMMAND:
        return this->parse_commandListExpression();
    EACH_LA_expression(GEN_LA_CASE)
        return this->parse_expression();
    default:
        E_ALTER(alters);
        return std::unique_ptr<Node>(nullptr);
    }
}

std::unique_ptr<Node> Parser::parse_expression() {
    return this->parse_assignment();
}

INLINE std::unique_ptr<Node> Parser::parse_assignment() {
    // parse left handle side
    auto node = this->parse_condOrExpression();

    // parse right handle side
    if(!HAS_NL()) {
        switch(this->curTokenKind) {
        case ASSIGN:
        case ADD_ASSIGN:
        case SUB_ASSIGN:
        case MUL_ASSIGN:
        case DIV_ASSIGN:
        case MOD_ASSIGN: {
            TokenKind kind = this->consumeAndGetKind();
            node = std::unique_ptr<Node>(createAssignNode(node.release(),
                    kind, this->parse_commandOrExpression().release()));
            break;
        }
        default:
            break;
        }
    }
    return node;
}

INLINE std::unique_ptr<Node> Parser::parse_condOrExpression() {
    auto node = this->parse_condAndExpression();

    while(!HAS_NL() && this->curTokenKind == COND_OR) {
        this->matchToken(COND_OR);
        node = std::unique_ptr<Node>(new CondOpNode(node.release(),
                this->parse_condAndExpression().release(), false));
    }
    return node;
}

INLINE std::unique_ptr<Node> Parser::parse_condAndExpression() {
    auto node = this->parse_orExpression();

    while(!HAS_NL() && this->curTokenKind == COND_AND) {
        this->matchToken(COND_AND);
        node = std::unique_ptr<Node>(new CondOpNode(node.release(),
                this->parse_orExpression().release(), true));
    }
    return node;
}

INLINE std::unique_ptr<Node> Parser::parse_orExpression() {
    auto node = this->parse_xorExpression();

    while(!HAS_NL() && this->curTokenKind == OR) {
        TokenKind op = this->consumeAndGetKind();
        node = std::unique_ptr<Node>(new BinaryOpNode(node.release(), op,
                this->parse_xorExpression().release()));
    }
    return node;
}

INLINE std::unique_ptr<Node> Parser::parse_xorExpression() {
    auto node = this->parse_andExpression();

    while(!HAS_NL() && this->curTokenKind == XOR) {
        TokenKind op = this->consumeAndGetKind();
        node = std::unique_ptr<Node>(new BinaryOpNode(node.release(), op,
                this->parse_andExpression().release()));
    }
    return node;
}

INLINE std::unique_ptr<Node> Parser::parse_andExpression() {
    auto node = this->parse_equalityExpression();

    while(!HAS_NL() && this->curTokenKind == AND) {
        TokenKind op = this->consumeAndGetKind();
        node = std::unique_ptr<Node>(new BinaryOpNode(node.release(), op,
                this->parse_equalityExpression().release()));
    }
    return node;
}

INLINE std::unique_ptr<Node> Parser::parse_equalityExpression() {
    auto node = this->parse_typeExpression();

    bool next = true;
    while(!HAS_NL() && next) {
        switch(this->curTokenKind) {
        case EQ:
        case NE:
        case RE_MATCH:
        case RE_UNMATCH: {
            TokenKind op = this->consumeAndGetKind();
            node = std::unique_ptr<Node>(new BinaryOpNode(node.release(), op,
                    this->parse_typeExpression().release()));
            break;
        }
        default: {
            next = false;
            break;
        }
        }
    }
    return node;
}


INLINE std::unique_ptr<Node> Parser::parse_typeExpression() {
    auto node = this->parse_relationalExpression();

    bool next = true;
    while(!HAS_NL() && next) {
        switch(this->curTokenKind) {
        case IS: {
            this->matchToken(IS);
            node = std::unique_ptr<Node>(new InstanceOfNode(node.release(),
                    this->parse_typeName().release()));
            break;
        }
        case AS: {
            this->matchToken(AS);
            node = std::unique_ptr<Node>(new CastNode(node.release(),
                    this->parse_typeName().release()));
            break;
        }
        default: {
            next = false;
            break;
        }
        }
    }
    return node;
}

INLINE std::unique_ptr<Node> Parser::parse_relationalExpression() {
    auto node = this->parse_addExpression();

    bool next = true;
    while(!HAS_NL() && next) {
        switch(this->curTokenKind) {
        case LA:
        case RA:
        case LE:
        case GE: {
            TokenKind op = this->consumeAndGetKind();
            node = std::unique_ptr<Node>(new BinaryOpNode(node.release(), op,
                    this->parse_addExpression().release()));
            break;
        }
        default: {
            next = false;
            break;
        }
        }
    }
    return node;
}

INLINE std::unique_ptr<Node> Parser::parse_addExpression() {
    auto node = this->parse_mulExpression();

    bool next = true;
    while(!HAS_NL() && next) {
        switch(this->curTokenKind) {
        case PLUS:
        case MINUS: {
            TokenKind op = this->consumeAndGetKind();
            node = std::unique_ptr<Node>(new BinaryOpNode(node.release(), op,
                    this->parse_mulExpression().release()));
            break;
        }
        default: {
            next = false;
            break;
        }
        }
    }
    return node;
}

INLINE std::unique_ptr<Node> Parser::parse_mulExpression() {
    auto node = this->parse_unaryExpression();

    bool next = true;
    while(!HAS_NL() && next) {
        switch(this->curTokenKind) {
        case MUL:
        case DIV:
        case MOD: {
            TokenKind op = this->consumeAndGetKind();
            node = std::unique_ptr<Node>(new BinaryOpNode(node.release(), op,
                    this->parse_unaryExpression().release()));
            break;
        }
        default: {
            next = false;
            break;
        }
        }
    }
    return node;
}

INLINE std::unique_ptr<Node> Parser::parse_unaryExpression() {
    switch(this->curTokenKind) {
    case PLUS:
    case MINUS:
    case NOT: {
        RET_NODE(createUnaryOpNode(this->consumeAndGetKind(),
                this->parse_unaryExpression().release()));
    }
    default: {
        return this->parse_suffixExpression();
    }
    }
}

INLINE std::unique_ptr<Node> Parser::parse_suffixExpression() {
    auto node = this->parse_memberExpression();

    switch(this->curTokenKind) {
    case INC:
    case DEC:
        RET_NODE(createSuffixNode(node.release(), this->consumeAndGetKind()));
    default:
        return node;
    }
}

INLINE std::unique_ptr<Node> Parser::parse_memberExpression() {
    auto node = this->parse_primaryExpression();

    bool next = true;
    while(!HAS_NL() && next) {
        switch(this->curTokenKind) {
        case ACCESSOR: {
            this->matchToken(ACCESSOR);
            this->hasNoNewLine();
            node = std::unique_ptr<Node>(
                    new AccessNode(node.release(), this->parse_name()));
            break;
        }
        case LB: {
            this->matchToken(LB);
            auto indexNode = this->parse_expression();
            this->matchToken(RB);
            node = std::unique_ptr<Node>(
                    createIndexNode(node.release(), indexNode.release()));
            break;
        }
        case LP: {
            node = std::unique_ptr<Node>(new ApplyNode(node.release(),
                    this->parse_arguments().release()));
            break;
        }
        default: {
            next = false;
            break;
        }
        }
    }
    return node;
}

INLINE std::unique_ptr<Node> Parser::parse_primaryExpression() {
    static TokenKind alters[] = {
            EACH_LA_primary(GEN_LA_ALTER)
            DUMMY
    };

    unsigned int n = LN();
    switch(this->curTokenKind) {
    case NEW: {
        this->matchToken(NEW);
        this->hasNoNewLine();
        RET_NODE(new NewNode(n, this->parse_typeName().release(),
                this->parse_arguments().release()));
    }
    case INT_LITERAL: {
        Token token = this->matchAndGetToken(INT_LITERAL);
        int value;
        CONVERT_TO_NUM(value, n, INT_LITERAL, token, this->lexer->toInt);
        RET_NODE(new IntValueNode(n, value));
    }
    case FLOAT_LITERAL: {
        Token token = this->matchAndGetToken(FLOAT_LITERAL);
        double value;
        CONVERT_TO_NUM(value, n, FLOAT_LITERAL, token, this->lexer->toDouble);
        RET_NODE(new FloatValueNode(n, value));
    }
    case STRING_LITERAL: {
        return this->parse_stringLiteral();
    }
    case OPEN_DQUOTE: {
        return this->parse_stringExpression();
    }
    case START_SUB_CMD: {
        return this->parse_commandSubstitution();
    }
    case APPLIED_NAME: {
        return this->parse_appliedName();
    }
    case SPECIAL_NAME: {
        return this->parse_specialName();
    }
    case LP: {  // expression or tuple
        this->matchToken(LP);
        auto node = this->parse_expression();
        if(this->curTokenKind == COMMA) {
            this->matchToken(COMMA);
            auto tuple = std::unique_ptr<TupleNode>(new TupleNode(n, node.release(),
                    this->parse_expression().release()));

            while(this->curTokenKind == COMMA) {
                this->matchToken(COMMA);
                tuple->addNode(this->parse_expression().release());
            }
            node.reset(tuple.release());
        }
        this->matchToken(RP);
        return node;
    }
    case LB: {
        this->matchToken(LB);
        auto node = std::unique_ptr<ArrayNode>(
                new ArrayNode(n, this->parse_expression().release()));
        while(this->curTokenKind == COMMA) {
            this->matchToken(COMMA);
            node->addExprNode(this->parse_expression().release());
        }
        this->matchToken(RB);
        RET_NODE(node.release());
    }
    case LBC: {
        this->matchToken(LBC);
        auto keyNode = this->parse_expression();

        this->hasNoNewLine();
        this->matchToken(COLON);
        this->hasNoNewLine();

        auto node = std::unique_ptr<MapNode>(new MapNode(n, keyNode.release(),
                this->parse_expression().release()));
        while(this->curTokenKind == COMMA) {
            this->matchToken(COMMA);
            auto keyNode = this->parse_expression();

            this->hasNoNewLine();
            this->matchToken(COLON);
            this->hasNoNewLine();

            node->addEntry(keyNode.release(), this->parse_expression().release());
        }
        this->matchToken(RBC);
        RET_NODE(node.release());
    }
    default:
        E_ALTER(alters);
        return std::unique_ptr<Node>(nullptr);
    }
}

INLINE std::unique_ptr<Node> Parser::parse_appliedName() {
    unsigned int n = LN();
    Token token = this->matchAndGetToken(APPLIED_NAME);
    RET_NODE(new VarNode(n, this->lexer->toName(token)));
}

INLINE std::unique_ptr<Node> Parser::parse_specialName() {
    unsigned int n = LN();
    Token token = this->matchAndGetToken(SPECIAL_NAME);
    RET_NODE(new SpecialCharNode(n, this->lexer->toName(token)));
}

INLINE std::unique_ptr<Node> Parser::parse_stringLiteral() {
    unsigned int n = LN();
    Token token = this->matchAndGetToken(STRING_LITERAL);
    RET_NODE(new StringValueNode(n, this->lexer->toString(token)));
}

INLINE std::unique_ptr<ArgsNode> Parser::parse_arguments() {
    this->matchToken(LP);

    auto node = std::unique_ptr<ArgsNode>(new ArgsNode());
    switch(this->curTokenKind) {
    EACH_LA_expression(GEN_LA_CASE) {
        node->addArg(this->parse_expression().release());
        while(this->curTokenKind == COMMA) {
            this->matchToken(COMMA);
            node->addArg(this->parse_expression().release());
        }
        break;
    }
    default:  // no args
        break;
    }

    this->matchToken(RP);
    return node;
}

INLINE std::unique_ptr<Node> Parser::parse_stringExpression() {
    unsigned int n = LN();
    this->matchToken(OPEN_DQUOTE);
    auto node = std::unique_ptr<StringExprNode>(new StringExprNode(n));

    bool next = true;
    while(next) {
        switch(this->curTokenKind) {
        case STR_ELEMENT: {
            unsigned int n = LN();
            Token token = this->matchAndGetToken(STR_ELEMENT);
            node->addExprNode(
                    new StringValueNode(n, this->lexer->toString(token, false)));
            break;
        }
        EACH_LA_interpolation(GEN_LA_CASE) {
            node->addExprNode(this->parse_interpolation().release());
            break;
        }
        case START_SUB_CMD: {
            node->addExprNode(this->parse_commandSubstitution().release());
            break;
        }
        default: {
            next = false;
            break;
        }
        }
    }

    this->matchToken(CLOSE_DQUOTE);
    RET_NODE(node.release());
}

INLINE std::unique_ptr<Node> Parser::parse_interpolation() {
    static TokenKind alters[] = {
            EACH_LA_interpolation(GEN_LA_ALTER)
            DUMMY
    };

    switch(this->curTokenKind) {
    case APPLIED_NAME: {
        return this->parse_appliedName();
    }
    case SPECIAL_NAME: {
        return this->parse_specialName();
    }
    case START_INTERP: {
        this->matchToken(START_INTERP);
        auto node = this->parse_expression();
        this->matchToken(RBC);
        return node;
    }
    default: {
        E_ALTER(alters);
        return std::unique_ptr<Node>(nullptr);
    }
    }
}

INLINE std::unique_ptr<Node> Parser::parse_commandSubstitution() {
    this->matchToken(START_SUB_CMD);
    auto node = this->parse_commandListExpression();
    this->matchToken(RP);
    return node;
}
