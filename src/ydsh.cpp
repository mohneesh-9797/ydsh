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

#include <cstring>
#include <pwd.h>
#include <iostream>
#include <clocale>
#include <csignal>
#include <cstdlib>
#include <algorithm>

#include <dirent.h>
#include <unistd.h>
#include <execinfo.h>
#include <sys/utsname.h>

#include <ydsh/ydsh.h>
#include <config.h>
#include <embed.h>

#include "ast/node_dumper.h"
#include "parser/lexer.h"
#include "parser/parser.h"
#include "parser/type_checker.h"
#include "core/context.h"
#include "core/symbol.h"
#include "misc/num.h"
#include "misc/files.h"


using namespace ydsh;
using namespace ydsh::ast;
using namespace ydsh::parser;
using namespace ydsh::core;

struct DSContext {
    RuntimeContext ctx;
    Parser parser;
    TypeChecker checker;
    unsigned int lineNum;

    // option
    flag32_set_t option;

    // previously computed prompt
    std::string prompt;

    struct {
        /**
         * kind of execution status.
         */
        unsigned int type;

        /**
         * for error location.
         */
        unsigned int lineNum;

        const char *errorKind;
    } execStatus;

    DSContext();
    ~DSContext() = default;

    /**
     * get exit status of recently executed command.(also exit command)
     */
    int getExitStatus() {
        return typeAs<Int_Object>(this->ctx.getExitStatus())->getValue();
    }

    void resetStatus() {
        this->execStatus.type = DS_STATUS_SUCCESS;
        this->execStatus.lineNum = 0;
        this->execStatus.errorKind = "";
    }

    void updateStatus(unsigned int type, unsigned int lineNum, const char *errorKind) {
        this->execStatus.type = type;
        this->execStatus.lineNum = lineNum;
        this->execStatus.errorKind = errorKind;
    }

    void handleParseError(const Lexer &lexer, const ParseError &e);
    void handleTypeError(const Lexer &lexer, const TypeCheckError &e);

    int eval(Lexer &lexer);

    /**
     * call only once.
     */
    void initBuiltinVar();

    /**
     * call only once
     */
    void loadEmbeddedScript();

    static const unsigned int originalShellLevel;

    /**
     * if environmental variable SHLVL dose not exist, set 0.
     */
    static unsigned int getShellLevel();
};


// #######################
// ##     DSContext     ##
// #######################

DSContext::DSContext() :
        ctx(), parser(), checker(this->ctx.getPool(), this->ctx.getSymbolTable()),
        lineNum(1), option(0), prompt(), execStatus() {
    // set locale
    setlocale(LC_ALL, "");
    setlocale(LC_MESSAGES, "C");

    // update shell level
    setenv("SHLVL", std::to_string(originalShellLevel + 1).c_str(), 1);

    // set some env
    if(getenv("HOME") == nullptr) {
        struct passwd *pw = getpwuid(getuid());
        if(pw == nullptr) {
            perror("getpwuid failed\n");
            exit(1);
        }
        setenv("HOME", pw->pw_dir, 1);
    }
}

/**
 * not allow dumb terminal
 */
static bool isSupportedTerminal(int fd) {
    const char *term = getenv("TERM");
    return term != nullptr && strcasecmp(term, "dumb") != 0 && isatty(fd) != 0;
}

enum class TermColor : int {   // ansi color code
    NOP     = -1,
    Reset   = 0,
    // actual term color
    Black   = 30,
    Red     = 31,
    Green   = 32,
    Yellow  = 33,
    Blue    = 34,
    Magenta = 35,
    Cyan    = 36,
    White   = 37,
};

static TermColor color(TermColor color, bool isatty) {
    return isatty ? color : TermColor::NOP;
}

static std::ostream &operator<<(std::ostream &stream, TermColor color) {
    if(color != TermColor::NOP) {
        stream << "\033[" << static_cast<unsigned int>(color) << "m";
    }
    return stream;
}

static void formatErrorLine(bool isatty, const Lexer &lexer, const Token &errorToken) {
    Token lineToken = lexer.getLineToken(errorToken, true);

    // print error line
    std::cerr << color(TermColor::Cyan, isatty) << lexer.toTokenText(lineToken)
    << color(TermColor::Reset, isatty) << std::endl;

    // print line marker
    std::cerr << color(TermColor::Green, isatty) << lexer.formatLineMarker(lineToken, errorToken)
    << color(TermColor::Reset, isatty) << std::endl;
}

void DSContext::handleParseError(const Lexer &lexer, const ParseError &e) {
    /**
     * show parse error message
     */
    unsigned int errorLineNum = lexer.getSourceInfoPtr()->getLineNum(e.getErrorToken().pos);
    if(e.getTokenKind() == EOS) {
        errorLineNum--;
    }

    const bool isatty = isSupportedTerminal(STDERR_FILENO);

    std::cerr << lexer.getSourceInfoPtr()->getSourceName() << ":" << errorLineNum << ":"
    << color(TermColor::Magenta, isatty) << " [syntax error] " << color(TermColor::Reset, isatty)
    << e.getMessage() << std::endl;
    formatErrorLine(isatty, lexer, e.getErrorToken());

    /**
     * update execution status
     */
    this->updateStatus(DS_STATUS_PARSE_ERROR, errorLineNum, e.getErrorKind());
}

void DSContext::handleTypeError(const Lexer &lexer, const TypeCheckError &e) {
    unsigned int errorLineNum = lexer.getSourceInfoPtr()->getLineNum(e.getStartPos());

    const bool isatty = isSupportedTerminal(STDERR_FILENO);

    /**
     * show type error message
     */
    std::cerr << lexer.getSourceInfoPtr()->getSourceName() << ":" << errorLineNum << ":"
    << color(TermColor::Magenta, isatty) << " [semantic error] " << color(TermColor::Reset, isatty)
    << e.getMessage() << std::endl;
    formatErrorLine(isatty, lexer, e.getToken());

    /**
     * update execution status
     */
    this->updateStatus(DS_STATUS_TYPE_ERROR,
                       lexer.getSourceInfoPtr()->getLineNum(e.getStartPos()), e.getKind());
}

int DSContext::eval(Lexer &lexer) {
    this->resetStatus();

    lexer.setLineNum(this->lineNum);
    RootNode rootNode;

    // parse
    try {
        this->parser.parse(lexer, rootNode);
        this->lineNum = lexer.getLineNum();

        if(hasFlag(this->option, DS_OPTION_DUMP_UAST)) {
            std::cout << "### dump untyped AST ###" << std::endl;
            NodeDumper::dump(std::cout, this->ctx.getPool(), rootNode);
            std::cout << std::endl;
        }
    } catch(const ParseError &e) {
        this->handleParseError(lexer, e);
        this->lineNum = lexer.getLineNum();
        return 1;
    }

    // type check
    try {
        this->checker.checkTypeRootNode(rootNode);

        if(hasFlag(this->option, DS_OPTION_DUMP_AST)) {
            std::cout << "### dump typed AST ###" << std::endl;
            NodeDumper::dump(std::cout, this->ctx.getPool(), rootNode);
            std::cout << std::endl;
        }
    } catch(const TypeCheckError &e) {
        this->handleTypeError(lexer, e);
        this->checker.recover();
        return 1;
    }

    if(hasFlag(this->option, DS_OPTION_PARSE_ONLY)) {
        return 0;
    }

    // eval
    EvalStatus s;
    try {
        s = rootNode.eval(this->ctx);
    } catch(const InternalError &e) {
        s = EvalStatus::THROW;
    }

    if(s != EvalStatus::SUCCESS) {
        unsigned int errorLineNum = 0;
        DSValue thrownObj = this->ctx.getThrownObject();
        if(dynamic_cast<Error_Object *>(thrownObj.get()) != nullptr) {
            Error_Object *obj = typeAs<Error_Object>(thrownObj);
            errorLineNum = getOccuredLineNum(obj->getStackTrace());
        }

        DSType &thrownType = *thrownObj->getType();
        if(this->ctx.getPool().getInternalStatus().isSameOrBaseTypeOf(thrownType)) {
            if(thrownType == this->ctx.getPool().getShellExit()) {
                if(hasFlag(this->option, DS_OPTION_TRACE_EXIT)) {
                    this->ctx.loadThrownObject();
                    typeAs<Error_Object>(this->ctx.pop())->printStackTrace(this->ctx);
                }
                this->updateStatus(DS_STATUS_EXIT, errorLineNum, "");
                return this->getExitStatus();
            }
            if(thrownType == this->ctx.getPool().getAssertFail()) {
                this->ctx.loadThrownObject();
                typeAs<Error_Object>(this->ctx.pop())->printStackTrace(this->ctx);
                this->updateStatus(DS_STATUS_ASSERTION_ERROR, errorLineNum, "");
                return 1;
            }
        }
        this->ctx.reportError();
        this->checker.recover(false);
        this->updateStatus(DS_STATUS_RUNTIME_ERROR, errorLineNum,
                           this->ctx.getPool().getTypeName(*thrownObj->getType()).c_str());
        return 1;
    }
    return this->getExitStatus();
}

static void defineBuiltin(RootNode &rootNode, const char *varName, DSValue &&value) {
    rootNode.addNode(new BindVarNode(varName, std::move(value)));
}

static void defineBuiltin(RootNode &rootNode, const char *varName, const DSValue &value) {
    rootNode.addNode(new BindVarNode(varName, value));
}

void DSContext::initBuiltinVar() {
    RootNode rootNode;

    /**
     * management object for D-Bus related function
     * must be DBus_Object
     */
    defineBuiltin(rootNode, "DBus", DSValue(DBus_Object::newDBus_Object(&this->ctx.getPool())));

    struct utsname name;
    if(uname(&name) == -1) {
        perror("cannot get utsname");
        exit(1);
    }

    /**
     * for os type detection.
     * must be String_Object
     */
    defineBuiltin(rootNode, "OSTYPE", DSValue::create<String_Object>(
            this->ctx.getPool().getStringType(), name.sysname));

#define XSTR(V) #V
#define STR(V) XSTR(V)
    /**
     * for version detection
     * must be String_Object
     */
    defineBuiltin(rootNode, "YDSH_VERSION", DSValue::create<String_Object>(
            this->ctx.getPool().getStringType(),
            STR(X_INFO_MAJOR_VERSION) "." STR(X_INFO_MINOR_VERSION) "." STR(X_INFO_PATCH_VERSION)));
#undef XSTR
#undef STR

    /**
     * default variable for read command.
     * must be String_Object
     */
    defineBuiltin(rootNode, "REPLY", this->ctx.getEmptyStrObj());

    std::vector<DSType *> types(2);
    types[0] = &this->ctx.getPool().getStringType();
    types[1] = types[0];

    /**
     * holding read variable.
     * must be Map_Object
     */
    defineBuiltin(rootNode, "reply", DSValue::create<Map_Object>(
            this->ctx.getPool().createReifiedType(this->ctx.getPool().getMapTemplate(), std::move(types))));

    /**
     * contains script argument(exclude script name). ($@)
     * must be Array_Object
     */
    defineBuiltin(rootNode, "@", DSValue::create<Array_Object>(this->ctx.getPool().getStringArrayType()));

    /**
     * contains size of argument. ($#)
     * must be Int_Object
     */
    defineBuiltin(rootNode, "#", DSValue::create<Int_Object>(this->ctx.getPool().getInt32Type(), 0));

    /**
     * contains exit status of most recent executed process. ($?)
     * must be Int_Object
     */
    defineBuiltin(rootNode, "?", DSValue::create<Int_Object>(this->ctx.getPool().getInt32Type(), 0));

    /**
     * process id of root shell. ($$)
     * must be Int_Object
     */
    defineBuiltin(rootNode, "$", DSValue::create<Int_Object>(this->ctx.getPool().getUint32Type(), getpid()));

    /**
     * represent shell or shell script name.
     * must be String_Object
     */
    defineBuiltin(rootNode, "0", DSValue::create<String_Object>(this->ctx.getPool().getStringType(), "ydsh"));

    /**
     * initialize positional parameter
     */
    for(unsigned int i = 0; i < 9; i++) {
        defineBuiltin(rootNode, std::to_string(i + 1).c_str(), this->ctx.getEmptyStrObj());
    }


    // ignore error check (must be always success)
    this->checker.checkTypeRootNode(rootNode);
    rootNode.eval(this->ctx);
}

void DSContext::loadEmbeddedScript() {
    Lexer lexer("(embed)", embed_script);
    this->eval(lexer);
    if(this->execStatus.type != DS_STATUS_SUCCESS) {
        fatal("broken embedded script\n");
    }
    this->ctx.getPool().commit();

    // rest some state
    this->lineNum = 1;
    this->ctx.updateExitStatus(0);
}

const unsigned int DSContext::originalShellLevel = getShellLevel();

unsigned int DSContext::getShellLevel() {
    char *shlvl = getenv("SHLVL");
    unsigned int level = 0;
    if(shlvl != nullptr) {
        int status;
        long value = ydsh::misc::convertToInt64(shlvl, status);
        if(status != 0) {
            level = 0;
        } else {
            level = value;
        }
    }
    return level;
}

static void segvHandler(int, siginfo_t *info, void *) {
    // write message
    long addr = (long)info->si_addr;
    if(addr > 0) { // stack over flow
        char msg[] = "+++++ detect Stack Overflow +++++\n";
        write(STDERR_FILENO, msg, strlen(msg));
    } else {
        char msg[] = "+++++ detect Segmentation fault +++++\n";
        write(STDERR_FILENO, msg, strlen(msg));
    }

    // get backtrace
    const unsigned int size = 128;
    void *buf[size];

    int retSize = backtrace(buf, size);
    backtrace_symbols_fd(buf, retSize, STDERR_FILENO);
    fsync(STDERR_FILENO);

    abort();
}

static void installSEGVHandler() {
    // init alternative stack (for signal handler)
    static char altStack[SIGSTKSZ];
    stack_t ss;
    ss.ss_sp = altStack;
    ss.ss_size = SIGSTKSZ;
    ss.ss_flags = 0;

    if(sigaltstack(&ss, nullptr) == -1) {
        perror("sigaltstack failed\n");
        exit(1);
    }

    // set signal handler for SIGSEGV
    struct sigaction act;
    act.sa_sigaction = segvHandler;
    act.sa_flags = SA_ONSTACK | SA_SIGINFO;
    sigfillset(&act.sa_mask);

    if(sigaction(SIGSEGV, &act, nullptr) < 0) {
        perror("setup signal handler failed\n");
        exit(1);
    }
}

// #####################################
// ##     public api of DSContext     ##
// #####################################

DSContext *DSContext_create() {
    installSEGVHandler();

    DSContext *ctx = new DSContext();
    ctx->initBuiltinVar();
    ctx->loadEmbeddedScript();
    return ctx;
}

void DSContext_delete(DSContext **ctx) {
    if(ctx != nullptr) {
        delete (*ctx);
        *ctx = nullptr;
    }
}

int DSContext_eval(DSContext *ctx, const char *sourceName, const char *source) {
    Lexer lexer(sourceName == nullptr ? "(stdin)" : sourceName, source);
    return ctx->eval(lexer);
}

int DSContext_loadAndEval(DSContext *ctx, const char *sourceName, FILE *fp) {
    Lexer lexer(sourceName == nullptr ? "(stdin)" : sourceName, fp);
    return ctx->eval(lexer);
}

int DSContext_exec(DSContext *ctx, char *const argv[]) {
    ctx->resetStatus();

    EvalStatus es = EvalStatus::SUCCESS;
    try {
        ctx->ctx.execBuiltinCommand(argv);
    } catch(const InternalError &e) {
        es = EvalStatus::THROW;
    }

    if(es != EvalStatus::SUCCESS) {
        DSType *thrownType = ctx->ctx.getThrownObject()->getType();
        if(*thrownType == ctx->ctx.getPool().getShellExit()) {
            ctx->execStatus.type = DS_STATUS_EXIT;
        }
    }
    return ctx->getExitStatus();
}

void DSContext_setLineNum(DSContext *ctx, unsigned int lineNum) {
    ctx->lineNum = lineNum;
}

unsigned int DSContext_lineNum(DSContext *ctx) {
    return ctx->lineNum;
}

void DSContext_setShellName(DSContext *ctx, const char *shellName) {
    if(shellName != nullptr) {
        ctx->ctx.updateScriptName(shellName);
    }
}

void DSContext_setArguments(DSContext *ctx, char *const args[]) {
    if(args == nullptr) {
        return;
    }

    ctx->ctx.initScriptArg();
    for(unsigned int i = 0; args[i] != nullptr; i++) {
        ctx->ctx.addScriptArg(args[i]);
    }
    ctx->ctx.finalizeScritArg();
}

static void setOptionImpl(DSContext *ctx, flag32_set_t flagSet, bool set) {
    if(hasFlag(flagSet, DS_OPTION_ASSERT)) {
        ctx->ctx.setAssertion(set);
    }
    if(hasFlag(flagSet, DS_OPTION_TOPLEVEL)) {
        ctx->ctx.setToplevelPrinting(set);
    }
}

void DSContext_setOption(DSContext *ctx, unsigned int optionSet) {
    setFlag(ctx->option, optionSet);
    setOptionImpl(ctx, optionSet, true);
}

void DSContext_unsetOption(DSContext *ctx, unsigned int optionSet) {
    unsetFlag(ctx->option, optionSet);
    setOptionImpl(ctx, optionSet, false);
}

const char *DSContext_prompt(DSContext *ctx, unsigned int n) {
    const char *psName = nullptr;
    switch(n) {
    case 1:
        psName = VAR_PS1;
        break;
    case 2:
        psName = VAR_PS2;
        break;
    default:
        return "";
    }

    unsigned int index = ctx->ctx.getSymbolTable().lookupHandle(psName)->getFieldIndex();
    const DSValue &obj = ctx->ctx.getGlobal(index);

    ctx->ctx.interpretPromptString(typeAs<String_Object>(obj)->getValue(), ctx->prompt);
    return ctx->prompt.c_str();
}

int DSContext_supportDBus() {
    return hasFlag(DSContext_featureBit(), DS_FEATURE_DBUS) ? 1 : 0;
}

unsigned int DSContext_majorVersion() {
    return X_INFO_MAJOR_VERSION;
}

unsigned int DSContext_minorVersion() {
    return X_INFO_MINOR_VERSION;
}

unsigned int DSContext_patchVersion() {
    return X_INFO_PATCH_VERSION;
}

const char *DSContext_version() {
    return "ydsh, version " X_INFO_VERSION
            " (" X_INFO_SYSTEM "), build by " X_INFO_CPP " " X_INFO_CPP_V;
}

const char *DSContext_copyright() {
    return "Copyright (C) 2015-2016 Nagisa Sekiguchi";
}

unsigned int DSContext_featureBit() {
    unsigned int featureBit = 0;

#ifdef USE_LOGGING
    setFlag(featureBit, DS_FEATURE_LOGGING);
#endif

#ifdef USE_DBUS
    setFlag(featureBit, DS_FEATURE_DBUS);
#endif

#ifdef USE_SAFE_CAST
    setFlag(featureBit, DS_FEATURE_SAFE_CAST);
#endif

#ifdef USE_FIXED_TIME
    setFlag(featureBit, DS_FEATURE_FIXED_TIME);
#endif
    return featureBit;
}

unsigned int DSContext_status(DSContext *ctx) {
    return ctx->execStatus.type;
}

unsigned int DSContext_errorLineNum(DSContext *ctx) {
    return ctx->execStatus.lineNum;
}

const char *DSContext_errorKind(DSContext *ctx) {
    return ctx->execStatus.errorKind;
}

// for input complete

struct DSCandidates {
    std::vector<std::string> candidates;
};

static std::vector<std::string> computePathList(const char *pathVal) {
    std::vector<std::string> result;
    std::string buf;
    assert(pathVal != nullptr);

    for(unsigned int i = 0; pathVal[i] != '\0'; i++) {
        char ch = pathVal[i];
        if(ch == ':') {
            result.push_back(std::move(buf));
            buf = "";
        } else {
            buf += ch;
        }
    }
    if(!buf.empty()) {
        result.push_back(std::move(buf));
    }

    // expand tilde
    for(auto &s : result) {
        if(s[0] == '~') {
            std::string expanded = expandTilde(s.c_str());
            std::swap(s, expanded);
        }
    }

    return result;
}

static bool startsWith(const char *s1, const char *s2) {
    return s1 != nullptr && s2 != nullptr && strstr(s1, s2) == s1;
}

/**
 * append candidates to results
 */
static void completeCommandName(RuntimeContext &ctx, const std::string &token,
                                std::vector<std::string> &results) {
    // search user defined command
    for(auto &e : ctx.getUdcMap()) {
        const char *name = e.first;
        if(startsWith(name, token.c_str())) {
            results.push_back(name);
        }
    }

    // search builtin command
    const unsigned int bsize = getBuiltinCommandSize();
    for(unsigned int i = 0; i < bsize; i++) {
        const char *name = getBultinCommandName(i);
        if(startsWith(name, token.c_str())) {
            results.push_back(name);
        }
    }


    // search external command
    const char *path = getenv("PATH");
    if(path == nullptr) {
        return;
    }

    auto pathList(computePathList(path));
    for(const auto &p : pathList) {
        DIR *dir = opendir(p.c_str());
        if(dir == nullptr) {
            continue;
        }

        dirent *entry;
        while(true) {
            entry = readdir(dir);
            if(entry == nullptr) {
                break;
            }
            if(entry->d_type == DT_REG || entry->d_type == DT_LNK || entry->d_type == DT_UNKNOWN) {
                const char *name = entry->d_name;
                if(startsWith(name, token.c_str())) {
                    std::string fullpath(p);
                    fullpath += '/';
                    fullpath += name;
                    if(access(fullpath.c_str(), X_OK) == 0) {
                        results.push_back(name);
                    }
                }
            }
        }
    }
}

static void completeFileName(const std::string &token, std::vector<std::string> &results, bool onlyExec = true) {
    auto s = token.find_last_of('/');

    // complete tilde
    if(token[0] == '~' && s == std::string::npos) {
        for(struct passwd *entry = getpwent(); entry != nullptr; entry = getpwent()) {
            std::string name("~");
            name += entry->pw_name;
            if(startsWith(name.c_str(), token.c_str())) {
                name += '/';
                results.push_back(std::move(name));
            }
        }
        endpwent();
        return;
    }


    // complete file name
    std::string targetDir;
    bool isRoot = s == 0;
    if(isRoot) {
        targetDir = "/";
    } else if(s != std::string::npos) {
        targetDir = token.substr(0, s);
    }

    std::string qualifiedName;
    if(token[0] == '~') {
        targetDir = expandTilde(targetDir.c_str());
        qualifiedName = expandTilde(token.c_str());
    } else {
        qualifiedName = token;
    }

    DIR *dir = opendir(targetDir.c_str());
    if(dir == nullptr) {
        return;
    }

    dirent *entry;
    while(true) {
        entry = readdir(dir);
        if(entry == nullptr) {
            break;
        }

        if(strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, ".") == 0) {
            continue;
        }

        std::string fullpath(targetDir);
        if(!isRoot) {
            fullpath += '/';
        }
        fullpath += entry->d_name;

        if(startsWith(fullpath.c_str(), qualifiedName.c_str())) {
            if(onlyExec && S_ISREG(getStMode(fullpath.c_str()))
               && access(fullpath.c_str(), X_OK) != 0) {
                continue;
            }

            std::string name = entry->d_name;
            if(S_ISDIR(getStMode(fullpath.c_str()))) {
                name += '/';
            }
            results.push_back(std::move(name));
        }
    }
}

enum class CompletorKind {
    EMPTY,
    CMD,
    FILE,
};

static CompletorKind selectCompletor(const char *buf, size_t cursor, std::string &tokenStr) {
    // find command start index
    size_t startIndex = 0;
    bool prevIsSpace = true;
    for(size_t i = 0; i < cursor; i++) {
        const char ch = buf[i];
        if(ch == ' ') {
            prevIsSpace = true;
            continue;
        } else {
            if(prevIsSpace) {
                startIndex = i;
            }
            prevIsSpace = false;
        }
    }

    tokenStr = std::string(buf + startIndex, cursor - startIndex);

    if(tokenStr[0] == '~' || strchr(tokenStr.c_str(), '/') != nullptr) {
        return CompletorKind::FILE;
    } else {
        return CompletorKind::CMD;
    }
}


DSCandidates *DSContext_complete(DSContext *ctx, const char *buf, size_t cursor) {
    if(ctx == nullptr || buf == nullptr || cursor == 0) {
        return nullptr;
    }

    DSCandidates *c = new DSCandidates();
    std::string tokenStr;
    switch(selectCompletor(buf, cursor, tokenStr)) {
    case CompletorKind::EMPTY:
        break;  // do nothing
    case CompletorKind::CMD:
        completeCommandName(ctx->ctx, tokenStr, c->candidates);
        break;
    case CompletorKind::FILE:
        completeFileName(tokenStr, c->candidates);
        break;
    }

    // sort and deduplicate
    std::sort(c->candidates.begin(), c->candidates.end());
    c->candidates.erase(std::unique(c->candidates.begin(), c->candidates.end()), c->candidates.end());
    return c;
}

size_t DSCandidates_size(const DSCandidates *c) {
    return c == nullptr ? 0 : c->candidates.size();
}

const char *DSCandidates_get(const DSCandidates *c, size_t index) {
    if(c == nullptr || index >= c->candidates.size()) {
        return nullptr;
    }
    return c->candidates[index].c_str();
}

void DSCandidates_release(DSCandidates **c) {
    if(c != nullptr) {
        delete (*c);
        *c = nullptr;
    }
}