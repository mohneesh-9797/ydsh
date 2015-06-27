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

#include <core/handle_info.h>
#include <misc/debug.h>
#include <DescLexer.h>

#include <stdarg.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <unordered_map>

namespace {

using namespace ydsh::core;

typedef struct {
    const char *handleInfo;
    unsigned int pos;
} context_t;

static HandleInfo toNum(unsigned int num) {
    // check range
    if(num < 9) {
        HandleInfo info = (HandleInfo) (num + P_N0);
        switch(info) {
#define GEN_CASE(ENUM) case ENUM:
        EACH_HANDLE_INFO_NUM(GEN_CASE)
#undef GEN_CASE
            return info;
        default:
            break;
        }
    }
    fatal("out of range, must be 0~8\n");
    return P_N0;
}


static bool isType(context_t *ctx) {
#define GEN_CASE(ENUM) case ENUM:
    if(ctx->handleInfo[ctx->pos] != '\0') {
        switch(ctx->handleInfo[ctx->pos++]) {
        EACH_HANDLE_INFO_TYPE(GEN_CASE)
            return true;
        EACH_HANDLE_INFO_TYPE_TEMP(GEN_CASE)
            return true;
        EACH_HANDLE_INFO_NUM(GEN_CASE)
            return false;
        EACH_HANDLE_INFO_PTYPE(GEN_CASE)
            return true;
        }
    }
    return false;
#undef GEN_CASE
}

static int getNum(context_t *ctx) {
#define GEN_CASE(ENUM) case ENUM:
    if(ctx->handleInfo[ctx->pos] != '\0') {
        char ch = ctx->handleInfo[ctx->pos++];
        switch(ch) {
        EACH_HANDLE_INFO_TYPE(GEN_CASE)
            return -1;
        EACH_HANDLE_INFO_TYPE_TEMP(GEN_CASE)
            return -1;
        EACH_HANDLE_INFO_NUM(GEN_CASE)
            return (int) (ch - P_N0);
        EACH_HANDLE_INFO_PTYPE(GEN_CASE)
            return -1;
        }
    }
    return -1;
#undef GEN_ENUM
}

static bool verifyHandleInfo(char *handleInfo) {
    context_t ctx = {handleInfo, 0};

    /**
     * check return type
     */
    if(!isType(&ctx)) {
        return false;
    }

    /**
     * check param size
     */
    int paramSize = getNum(&ctx);
    if(paramSize < 0 || paramSize > 8) {
        return false;
    }

    /**
     * check param types
     */
    for(int i = 0; i < paramSize; i++) {
        if(!isType(&ctx)) {
            return false;
        }
    }

    /**
     * check null terminate
     */
    return ctx.handleInfo[ctx.pos] == '\0';
}

static std::string toTypeInfoName(HandleInfo info) {
    switch(info) {
#define GEN_NAME(INFO) case INFO: return std::string(#INFO);
    EACH_HANDLE_INFO(GEN_NAME)
#undef GEN_NAME
    default:
        fatal("illegal type info: %d", info);
        return std::string();
    }
}

class ProcessingError {
private:
    std::string message;

public:
    explicit ProcessingError(const char *message) :
            message(message) {
    }

    ~ProcessingError() = default;

    const std::string &getMessage() const {
        return this->message;
    }
};

/**
 * for processing error reporting
 */
static void error(const char *fmt, ...) {
    const static unsigned int size = 128;
    static char buf[size];  // error message must be under size.

    // formate message
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, size, fmt, args);
    va_end(args);

    throw ProcessingError(buf);
}

class HandleInfoMap {
private:
    std::unordered_map<std::string, HandleInfo> name2InfoMap;
    std::vector<std::pair<HandleInfo, std::string>> info2NameMap;

    HandleInfoMap();

public:
    ~HandleInfoMap() = default;

    static HandleInfoMap &getInstance();

    const std::string &getName(HandleInfo info);

    HandleInfo getInfo(const std::string &name);

private:
    void registerName(HandleInfo info, const char *name);
};

HandleInfoMap::HandleInfoMap() :
        name2InfoMap(), info2NameMap() {
#define REGISTER(ENUM) this->registerName(ENUM, #ENUM);
    EACH_HANDLE_INFO_TYPE(REGISTER)
    EACH_HANDLE_INFO_PTYPE(REGISTER)
    EACH_HANDLE_INFO_TYPE_TEMP(REGISTER)
#undef REGISTER
}

HandleInfoMap &HandleInfoMap::getInstance() {
    static HandleInfoMap map;
    return map;
}

const std::string &HandleInfoMap::getName(HandleInfo info) {
    for(auto &pair : this->info2NameMap) {
        if(pair.first == info) {
            return pair.second;
        }
    }
    fatal("not found handle info: %s\n", toTypeInfoName(info).c_str());

    const static std::string empty("");
    return empty;
}

HandleInfo HandleInfoMap::getInfo(const std::string &name) {
    auto iter = this->name2InfoMap.find(name);
    if(iter == this->name2InfoMap.end()) {
        error("not found type name: %s", name.c_str());
    }
    return iter->second;
}

void HandleInfoMap::registerName(HandleInfo info, const char *name) {
    std::string actualName(name);
    this->info2NameMap.push_back(std::make_pair(info, actualName));
    this->name2InfoMap.insert(std::make_pair(actualName, info));
}


class HandleInfoSerializer {
private:
    std::vector<HandleInfo> infos;

public:
    HandleInfoSerializer() = default;

    ~HandleInfoSerializer() = default;

    void add(HandleInfo info) {
        this->infos.push_back(info);
    }

    std::string toString() {
        std::string str("{");
        unsigned int size = this->infos.size();
        for(unsigned int i = 0; i < size; i++) {
            if(i > 0) {
                str += ", ";
            }
            str += toTypeInfoName(this->infos[i]);
        }
        str += "}";
        return str;
    }
};

struct TypeToken {
    virtual ~TypeToken() = default;

    virtual void serialize(HandleInfoSerializer &s) = 0;

    virtual bool isType(HandleInfo info) = 0;
};


class CommonTypeToken : public TypeToken {
private:
    HandleInfo info;

public:
    /**
     * not call it directory.
     */
    explicit CommonTypeToken(HandleInfo info) :
            info(info) {
    }

    ~CommonTypeToken() = default;

    void serialize(HandleInfoSerializer &s) {    // override
        s.add(this->info);
    }

    bool isType(HandleInfo info) {    // override
        return this->info == info;
    }

    static std::unique_ptr<TypeToken> newTypeToken(const std::string &name);
};

std::unique_ptr<TypeToken> CommonTypeToken::newTypeToken(const std::string &name) {
    return std::unique_ptr<TypeToken>(
            new CommonTypeToken(HandleInfoMap::getInstance().getInfo(name)));
}


class ReifiedTypeToken : public TypeToken {
private:
    std::unique_ptr<CommonTypeToken> typeTemp;

    /**
     * element size. must be equivalent to this->elements.size().
     * if requiredSize is 0, allow any elements
     */
    unsigned int requiredSize;

    std::vector<std::unique_ptr<TypeToken>> elements;

    ReifiedTypeToken(std::unique_ptr<CommonTypeToken> &&type, unsigned int elementSize) :
            typeTemp(type.release()), requiredSize(elementSize), elements() {
    }

public:
    ~ReifiedTypeToken() = default;

    void addElement(std::unique_ptr<TypeToken> &&type) {
        this->elements.push_back(std::move(type));
    }

    void serialize(HandleInfoSerializer &s);    // override

    bool isType(HandleInfo info) {    // override
        return this->typeTemp->isType(info);
    }

    static std::unique_ptr<ReifiedTypeToken> newReifiedTypeToken(const std::string &name);
};

void ReifiedTypeToken::serialize(HandleInfoSerializer &s) {
    // check element size
    unsigned int elementSize = this->elements.size();
    if(this->requiredSize > 0) {
        if(this->requiredSize != elementSize) {
            error("require %d, but is %d", this->requiredSize, this->elements.size());
        }
    }

    typeTemp->serialize(s);
    s.add(toNum(elementSize));
    for(auto &tok : this->elements) {
        tok->serialize(s);
    }
}

std::unique_ptr<ReifiedTypeToken> ReifiedTypeToken::newReifiedTypeToken(const std::string &name) {
    std::unique_ptr<CommonTypeToken> tok;
    unsigned int size = 0;
    if(name == toTypeInfoName(Array)) {
        tok.reset(new CommonTypeToken(Array));
        size = 1;
    } else if(name == toTypeInfoName(Map)) {
        tok.reset(new CommonTypeToken(Map));
        size = 2;
    } else if(name == toTypeInfoName(Tuple)) {
        tok.reset(new CommonTypeToken(Tuple));
        size = 0;
    } else {
        error("unsupported type template: %s", name.c_str());
    }
    return std::unique_ptr<ReifiedTypeToken>(new ReifiedTypeToken(std::move(tok), size));
}


class Element {
protected:
    /**
     * if true, treat as function,
     * if false, treat as constructor
     */
    bool func;

    /**
     * if this element is constructor, it is empty string
     */
    std::string funcName;

    /**
     * if true, this function is operator
     */
    bool op;

    /**
     * owner type which this element belongs to
     */
    std::unique_ptr<TypeToken> ownerType;

    /**
     * if this element represents for constructor, returnType is always void.
     */
    std::unique_ptr<TypeToken> returnType;

    /**
     * not contains receiver type if this element represents for function
     */
    std::vector<std::unique_ptr<TypeToken>> paramTypes;

    /**
     * contains parameter name and default value flag.
     */
    std::vector<std::pair<std::string, bool>> paramNames;

    /**
     * name of binding function
     */
    std::string actualFuncName;

    Element(std::string &&funcName, bool func, bool op) :
            func(func), funcName(std::move(funcName)), op(op), ownerType(), returnType(),
            paramTypes(), paramNames(), actualFuncName() {
    }

public:
    static std::unique_ptr<Element> newFuncElement(std::string &&funcName, bool op) {
        return std::unique_ptr<Element>(new Element(std::move(funcName), true, op));
    }

    static std::unique_ptr<Element> newInitElement() {
        std::unique_ptr<Element> element(new Element(std::string(""), false, false));
        element->setReturnType(std::unique_ptr<TypeToken>(new CommonTypeToken(Void)));
        return element;
    }

    virtual ~Element() = default;

    bool isFunc() {
        return this->func;
    }

    void addParam(std::string &&name, bool hasDefault, std::unique_ptr<TypeToken> &&type) {
        if(!this->ownerType) {  // treat first param type as receiver
            this->ownerType = std::move(type);
            return;
        }
        this->paramNames.push_back(std::make_pair(std::move(name), hasDefault));
        this->paramTypes.push_back(std::move(type));
    }

    void setReturnType(std::unique_ptr<TypeToken> &&type) {
        this->returnType = std::move(type);
    }

    void setActualFuncName(std::string &&name) {
        this->actualFuncName = std::move(name);
    }

    bool isOwnerType(HandleInfo info) {
        return this->ownerType->isType(info);
    }

    std::string toSerializedHandle() {
        HandleInfoSerializer s;
        this->returnType->serialize(s);
        s.add(toNum(this->paramTypes.size() + 1));
        this->ownerType->serialize(s);
        for(const std::unique_ptr<TypeToken> &t : this->paramTypes) {
            t->serialize(s);
        }
        return s.toString();
    }

    std::string toParamNames() {
        std::string str("{");
        str += "\"\"";
        for(auto &pair : this->paramNames) {
            str += ", ";
            str += "\"";
            str += pair.first;
            str += "\"";
        }
        str += "}";
        return str;
    }

    std::string toFuncName() {
        if(this->op) {
            return this->funcName;
        } else {
            std::string str("\"");
            str += this->funcName;
            str += "\"";
            return str;
        }
    }

    const char *getActualFuncName() {
        return this->actualFuncName.c_str();
    }

    unsigned char toDefaultFlag() {
        unsigned char flag = 0;
        unsigned int size = this->paramNames.size();
        for(unsigned int i = 0; i < size; i++) {
            if(this->paramNames[i].second) {
                flag += (1 << (i + 1));
            }
        }
        return flag;
    }

    std::string toString() {
        std::string str("{");
        str += this->toFuncName();
        str += ", ";
        str += this->toSerializedHandle();
        str += ", ";
        str += this->toParamNames();
        str += ", ";
        str += this->getActualFuncName();
        str += ", ";
        str += std::to_string((int) this->toDefaultFlag());
        str += "}";
        return str;
    }
};

class Parser : public ydsh::parser_base::ParserBase<DescTokenKind, DescLexer> {
private:
    Parser() = default;

public:
    ~Parser() = default;

    /**
     * open file and parse.
     * after parsing, write results to elements.
     */
    static void parse(char *fileName, std::vector<std::unique_ptr<Element>> &elements);

private:
    static bool isDescriptor(const std::string &line);

    std::string toName(const Token &token) {
        Token t = token;
        t.startPos++;
        t.size--;
        return this->lexer->toTokenText(t);
    }

    void init(DescLexer &lexer) {
        this->lexer = &lexer;
        this->fetchNext();
    }

    std::unique_ptr<Element> parse_descriptor(const std::string &line);

    std::unique_ptr<Element> parse_funcDesc();

    std::unique_ptr<Element> parse_initDesc();

    void parse_params(const std::unique_ptr<Element> &element);

    std::unique_ptr<TypeToken> parse_type();

    void parse_funcDecl(const std::string &line, std::unique_ptr<Element> &element);

    void printParseError(const ParseError &e);
};

void Parser::parse(char *fileName, std::vector<std::unique_ptr<Element>> &elements) {
    std::ifstream input(fileName);
    if(!input) {
        fatal("cannot open file: %s\n", fileName);
    }

    Parser parser;
    unsigned int lineNum = 0;
    std::string line;
    bool foundDesc = false;

    std::unique_ptr<Element> element;
    while(std::getline(input, line)) {
        lineNum++;

        try {
            if(foundDesc) {
                parser.parse_funcDecl(line, element);
                elements.push_back(std::move(element));
                foundDesc = false;
                continue;
            }
            if(isDescriptor(line)) {
                foundDesc = true;
                element = parser.parse_descriptor(line);
            }
        } catch(const ProcessingError &e) {
            std::cerr << fileName << ":" << lineNum
            << ": [error] " << e.getMessage() << std::endl;
            std::cerr << line << std::endl;
            exit(EXIT_FAILURE);
        } catch(const ParseError &e) {
            std::cerr << fileName << ":" << lineNum << ": [error] ";
            parser.printParseError(e);
            std::cerr << line << std::endl;
            Token lineToken;
            lineToken.startPos = 0;
            lineToken.size = line.size();
            std::cerr << parser.lexer->formatLineMarker(lineToken, e.getErrorToken()) << std::endl;
            exit(EXIT_FAILURE);
        }
    }
}

bool Parser::isDescriptor(const std::string &line) {
    // skip space
    unsigned int pos = 0;
    unsigned int size = line.size();
    for(; pos < size; pos++) {
        char ch = line[pos];
        if(ch != ' ' && ch != '\t') {
            break;
        }
    }

    static const char prefix[] = "//!bind:";
    for(unsigned int i = 0; prefix[i] != '\0'; i++) {
        if(pos >= size) {
            return false;
        }
        if(line[pos++] != prefix[i]) {
            return false;
        }
    }
    return true;
}

std::unique_ptr<Element> Parser::parse_descriptor(const std::string &line) {
    DescLexer lexer(line.c_str());
    this->init(lexer);

    this->expect(DESC_PREFIX);

    switch(this->curToken.kind) {
    case FUNC:
        return this->parse_funcDesc();
    case INIT:
        return this->parse_initDesc();
    default: {
        std::vector<DescTokenKind> alters;
        alters.push_back(FUNC);
        alters.push_back(INIT);
        this->alternativeError(std::move(alters));
        break;
    }
    }
    return std::unique_ptr<Element>(nullptr);
}

std::unique_ptr<Element> Parser::parse_funcDesc() {
    this->expect(FUNC);

    std::unique_ptr<Element> element;

    // parser function name
    switch(this->curToken.kind) {
    case IDENTIFIER: {
        Token token;
        this->expect(IDENTIFIER, token);
        element = Element::newFuncElement(this->lexer->toTokenText(token), false);
        break;
    }
    case VAR_NAME: {
        Token token;
        this->expect(VAR_NAME, token);
        element = Element::newFuncElement(this->toName(token), true);
        break;
    }
    default: {
        std::vector<DescTokenKind> alters;
        alters.push_back(IDENTIFIER);
        alters.push_back(VAR_NAME);
        this->alternativeError(std::move(alters));
        return std::unique_ptr<Element>(nullptr);
    }
    }

    // parser parameter decl
    this->expect(LP);
    this->parse_params(element);
    this->expect(RP);

    this->expect(COLON);
    element->setReturnType(this->parse_type());

    return element;
}

std::unique_ptr<Element> Parser::parse_initDesc() {
    this->expect(INIT);

    std::unique_ptr<Element> element(Element::newInitElement());
    this->expect(LP);
    this->parse_params(element);
    this->expect(RP);

    return element;
}

void Parser::parse_params(const std::unique_ptr<Element> &element) {
    int count = 0;
    do {
        if(count++ > 0) {
            this->expect(COMMA);
        }

        Token token;
        this->expect(VAR_NAME, token);
        bool hasDefault = false;
        if(this->curToken.kind == OPT) {
            this->expect(OPT);
            hasDefault = true;
        }
        this->expect(COLON);
        std::unique_ptr<TypeToken> type(this->parse_type());

        element->addParam(this->toName(token), hasDefault, std::move(type));
    } while(this->curToken.kind == COMMA);
}

std::unique_ptr<TypeToken> Parser::parse_type() {
    switch(this->curToken.kind) {
    case IDENTIFIER: {
        Token token;
        this->expect(IDENTIFIER, token);
        return CommonTypeToken::newTypeToken(this->lexer->toTokenText(token));
    };
    case ARRAY:
    case MAP:
    case TUPLE: {
        auto token = this->curToken;
        this->fetchNext();

        auto type(ReifiedTypeToken::newReifiedTypeToken(this->lexer->toTokenText(token)));
        this->expect(TYPE_OPEN);

        unsigned int count = 0;
        do {
            if(count++ > 0) {
                this->expect(COMMA);
            }
            type->addElement(this->parse_type());
        } while(this->curToken.kind == COMMA);

        this->expect(TYPE_CLOSE);

        return std::unique_ptr<TypeToken>(type.release());
    };
    default:
        std::vector<DescTokenKind> alters;
        alters.push_back(IDENTIFIER);
        alters.push_back(ARRAY);
        alters.push_back(MAP);
        alters.push_back(TUPLE);
        this->alternativeError(std::move(alters));
        return std::unique_ptr<TypeToken>();
    }
}

void Parser::parse_funcDecl(const std::string &line, std::unique_ptr<Element> &element) {
    DescLexer lexer(line.c_str());
    this->init(lexer);

    this->expect(STATIC);
    this->expect(INLINE);
    this->expect(BOOL);

    Token token;
    this->expect(IDENTIFIER, token);
    std::string str(this->lexer->toTokenText(token));
    element->setActualFuncName(std::move(str));

    this->expect(LP);
    this->expect(RCTX);
    this->expect(AND);
    this->expect(IDENTIFIER);
    this->expect(RP);
    this->expect(LBC);
}

void Parser::printParseError(const ParseError &e) {
#define EACH_TYPE(OP) \
    OP(TokenMismatchedError) \
    OP(NoViableAlterError) \
    OP(InvalidTokenError)

#define PRINT(OP) if(dynamic_cast<const OP *>(&e) != nullptr) { \
    std::cerr << *static_cast<const OP *>(&e) << std::endl; return; }

    EACH_TYPE(PRINT)

#undef PRINT
#undef EACH_TYPE
}

#define OUT(fmt, ...) \
    do {\
        fprintf(fp, fmt, ## __VA_ARGS__);\
    } while(0)


struct TypeBind {
    HandleInfo info;
    std::string name;

    /**
     * may be nullptr,if has no constructor.
     */
    Element *initElement;

    std::vector<Element *> funcElements;

    explicit TypeBind(HandleInfo info) :
            info(info), name(HandleInfoMap::getInstance().getName(info)),
            initElement(), funcElements() {
    }

    ~TypeBind() = default;
};

static std::vector<TypeBind *> genTypeBinds(std::vector<std::unique_ptr<Element>> &elements) {
    std::vector<TypeBind *> binds;
#define GEN_BIND(ENUM) binds.push_back(new TypeBind(ENUM));
    EACH_HANDLE_INFO_TYPE(GEN_BIND)
    EACH_HANDLE_INFO_TYPE_TEMP(GEN_BIND)
#undef GEN_BIND

    for(const std::unique_ptr<Element> &element : elements) {
        bool matched = false;
        for(TypeBind *bind : binds) {
            if(element->isOwnerType(bind->info)) {
                matched = true;
                if(element->isFunc()) {
                    bind->funcElements.push_back(element.get());
                } else {
                    bind->initElement = element.get();
                }
                break;
            }
        }
        if(!matched) {
            fatal("invalid owner type\n");
        }
    }
    return binds;
}

static void gencode(const char *outFileName, const std::vector<TypeBind *> &binds) {
    FILE *fp = fopen(outFileName, "w");
    if(fp == NULL) {
        fatal("cannot open output file: %s", outFileName);
    }

    // write header
    OUT("#include <core/bind.h>\n");
    OUT("#include <core/builtin.h>\n");
    OUT("#include <core/symbol.h>\n");
    OUT("#include <core/DSType.h>\n");
    OUT("\n");
    OUT("namespace ydsh {\n");
    OUT("namespace core {\n");
    OUT("\n");

    // generate dummy
    OUT("native_type_info_t *info_Dummy() {\n");
    OUT("    static native_type_info_t info = {0, 0, {}};\n");
    OUT("    return &info;\n");
    OUT("}\n");
    OUT("\n");

    // generate type binding
    for(TypeBind *bind : binds) {
        OUT("native_type_info_t *info_%sType() {\n", bind->name.c_str());
        if(bind->initElement != 0) {
            OUT("    static NativeFuncInfo initInfo = %s;\n",
                bind->initElement->toString().c_str());
        }
        OUT("    static native_type_info_t info = {\n");
        OUT("        .initInfo = %s,\n", (bind->initElement == 0 ? "0" : "&initInfo"));
        OUT("        .methodSize = %ld,\n", bind->funcElements.size());
        OUT("        .funcInfos = {\n");
        for(Element *e : bind->funcElements) {
            OUT("            %s,\n", e->toString().c_str());
        }
        OUT("        }\n");
        OUT("    };\n");
        OUT("    return &info;\n");
        OUT("}\n");
        OUT("\n");
    }

    OUT("} // namespace core\n");
    OUT("} // namespace ydsh\n");
    fclose(fp);
}

} // namespace

int main(int argc, char **argv) {
    if(argc != 3) {
        fprintf(stderr, "usage: %s [target source] [generated code]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *inputFileName = argv[1];
    char *outputFileName = argv[2];

    std::vector<std::unique_ptr<Element>> elements;
    Parser::parse(inputFileName, elements);

    std::vector<TypeBind *> binds(genTypeBinds(elements));
    gencode(outputFileName, binds);

    exit(EXIT_SUCCESS);
}

