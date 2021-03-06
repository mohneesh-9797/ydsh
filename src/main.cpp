/*
 * Copyright (C) 2015-2018 Nagisa Sekiguchi
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

#include <unistd.h>
#include <pwd.h>

#include <memory>

#include <ydsh/ydsh.h>
#include "misc/opt.hpp"
#include "misc/util.hpp"

using namespace ydsh;

struct Deleter {
    void operator()(DSState *state) const {
        DSState_delete(&state);
    }
};

using DSStateHandle = std::unique_ptr<DSState, Deleter>;

template <typename ...Arg>
static auto createState(Arg && ...arg) {
    return DSStateHandle(DSState_createWithMode(std::forward<Arg>(arg)...));
}

int exec_interactive(DSState *dsState);

static std::pair<DSErrorKind, int> loadRC(const DSStateHandle &handle, const std::string &rcfile) {
    DSError e{};
    int ret = DSState_loadModule(handle.get(), rcfile.c_str(), DS_MOD_FULLPATH | DS_MOD_IGNORE_ENOENT, &e);
    auto kind = e.kind;
    DSError_release(&e);

    // reset line num
    DSState_setLineNum(handle.get(), 1);

    return {kind, ret};
}

static const char *statusLogPath = nullptr;

static std::string escape(const char *str) {
    std::string value;
    value += '"';
    for(; str != nullptr && *str != '\0'; str++) {
        int ch = *str;
        if(ch == '\\' || ch == '"') {
            value += '\\';
        }
        value += static_cast<char>(ch);
    }
    value += '"';
    return value;
}

static void writeStatusLog(DSError &error) {
    if(statusLogPath == nullptr) {
        return;
    }

    FILE *fp = fopen(statusLogPath, "w");
    if(fp != nullptr) {
        fprintf(fp, "kind=%d lineNum=%d name=%s fileName=%s\n",
                error.kind, error.lineNum, escape(error.name).c_str(), escape(error.fileName).c_str());
        fclose(fp);
    }
}

template <typename Func, typename ...T>
static int apply(Func func, const DSStateHandle &handle, T&& ... args) {
    DSError error{};
    int ret = func(handle.get(), std::forward<T>(args)..., &error);
    writeStatusLog(error);
    DSError_release(&error);
    return ret;
}

static void showFeature(FILE *fp) {
    const char *featureNames[] = {
            "USE_LOGGING",
            "USE_SAFE_CAST",
    };

    const unsigned int featureBit = DSState_featureBit();
    for(unsigned int i = 0; i < arraySize(featureNames); i++) {
        if(hasFlag(featureBit, static_cast<unsigned int>(1u << i))) {
            fprintf(fp, "%s\n", featureNames[i]);
        }
    }
}

#define EACH_OPT(OP) \
    OP(DUMP_UAST,      "--dump-untyped-ast",  opt::OPT_ARG, "dump abstract syntax tree (before type checking)") \
    OP(DUMP_AST,       "--dump-ast",          opt::OPT_ARG, "dump abstract syntax tree (after type checking)") \
    OP(DUMP_CODE,      "--dump-code",         opt::OPT_ARG, "dump compiled code") \
    OP(PARSE_ONLY,     "--parse-only",        opt::NO_ARG, "not evaluate, parse only") \
    OP(CHECK_ONLY,     "--check-only",        opt::NO_ARG, "not evaluate, type check only") \
    OP(COMPILE_ONLY,   "--compile-only",      opt::NO_ARG, "not evaluate, compile only") \
    OP(DISABLE_ASSERT, "--disable-assertion", opt::NO_ARG, "disable assert statement") \
    OP(TRACE_EXIT,     "--trace-exit",        opt::NO_ARG, "trace execution process to exit command") \
    OP(VERSION,        "--version",           opt::NO_ARG, "show version and copyright") \
    OP(HELP,           "--help",              opt::NO_ARG, "show this help message") \
    OP(COMMAND,        "-c",                  opt::HAS_ARG, "evaluate argument") \
    OP(NORC,           "--norc",              opt::NO_ARG, "not load rc file (only available interactive mode)") \
    OP(EXEC,           "-e",                  opt::HAS_ARG, "execute command (ignore some options)") \
    OP(STATUS_LOG,     "--status-log",        opt::HAS_ARG, "write execution status to specified file (ignored in interactive mode or -e)") \
    OP(FEATURE,        "--feature",           opt::NO_ARG, "show available features") \
    OP(RC_FILE,        "--rcfile",            opt::HAS_ARG, "load specified rc file (only available interactive mode)") \
    OP(QUIET,          "--quiet",             opt::NO_ARG, "suppress startup message (only available interactive mode)") \
    OP(SET_ARGS,       "-s",                  opt::NO_ARG, "set arguments and read command from standard input") \
    OP(INTERACTIVE,    "-i",                  opt::NO_ARG, "run interactive mode") \
    OP(NOEXEC,         "-n",                  opt::NO_ARG, "equivalent to `--compile-only' option")

enum OptionKind {
#define GEN_ENUM(E, S, F, D) E,
    EACH_OPT(GEN_ENUM)
#undef GEN_ENUM
};


enum class InvocationKind {
    FROM_FILE,
    FROM_STDIN,
    FROM_STRING,
    BUILTIN,
};

static const char *version() {
    return DSState_version(nullptr);
}

static std::string getDefaultRCFilePath() {
    struct passwd *pw = getpwuid(getuid());
    std::string path = pw->pw_dir;
    path += "/.ydshrc";
    return path;
}

int main(int argc, char **argv) {
    opt::Parser<OptionKind> parser = {
#define GEN_OPT(E, S, F, D) {E, S, F, D},
            EACH_OPT(GEN_OPT)
#undef GEN_OPT
    };
    auto begin = argv + 1;
    auto end = argv + argc;
    opt::Result<OptionKind> result;

    InvocationKind invocationKind = InvocationKind::FROM_FILE;
    const char *evalText = nullptr;
    bool userc = true;
    std::string rcfile = getDefaultRCFilePath();
    bool quiet = false;
    bool forceInteractive = false;
    DSExecMode mode = DS_EXEC_MODE_NORMAL;
    unsigned short option = 0;
    bool noAssert = false;
    struct {
        const DSDumpKind kind;
        const char *path;
    } dumpTarget[3] = {
            {DS_DUMP_KIND_UAST, nullptr},
            {DS_DUMP_KIND_AST, nullptr},
            {DS_DUMP_KIND_CODE, nullptr},
    };


    while((result = parser(begin, end))) {
        switch(result.value()) {
        case DUMP_UAST:
            dumpTarget[0].path = result.arg() != nullptr ? result.arg() : "";
            break;
        case DUMP_AST:
            dumpTarget[1].path = result.arg() != nullptr ? result.arg() : "";
            break;
        case DUMP_CODE:
            dumpTarget[2].path = result.arg() != nullptr ? result.arg() : "";
            break;
        case PARSE_ONLY:
            mode = DS_EXEC_MODE_PARSE_ONLY;
            break;
        case CHECK_ONLY:
            mode = DS_EXEC_MODE_CHECK_ONLY;
            break;
        case COMPILE_ONLY:
        case NOEXEC:
            mode = DS_EXEC_MODE_COMPILE_ONLY;
            break;
        case DISABLE_ASSERT:
            noAssert = true;
            break;
        case TRACE_EXIT:
            setFlag(option, DS_OPTION_TRACE_EXIT);
            break;
        case VERSION:
            fprintf(stdout, "%s\n", version());
            return 0;
        case HELP:
            fprintf(stdout, "%s\n", version());
            parser.printOption(stdout);
            return 0;
        case COMMAND:
            invocationKind = InvocationKind::FROM_STRING;
            evalText = result.arg();
            goto INIT;
        case NORC:
            userc = false;
            break;
        case EXEC:
            invocationKind = InvocationKind::BUILTIN;
            statusLogPath = nullptr;
            --begin;
            goto INIT;
        case STATUS_LOG:
            statusLogPath = result.arg();
            break;
        case FEATURE:
            showFeature(stdout);
            return 0;
        case RC_FILE:
            rcfile = result.arg();
            break;
        case QUIET:
            quiet = true;
            break;
        case SET_ARGS:
            invocationKind = InvocationKind::FROM_STDIN;
            goto INIT;
        case INTERACTIVE:
            forceInteractive = true;
            break;
        }
    }
    if(result.error() != opt::END) {
        fprintf(stderr, "%s\n%s\n", result.formatError().c_str(), version());
        parser.printOption(stderr);
        return 1;
    }

    INIT:

    // init state
    auto state = createState(mode);
    DSState_setOption(state.get(), option);
    for(auto &e : dumpTarget) {
        if(e.path != nullptr) {
            DSState_setDumpTarget(state.get(), e.kind, e.path);
        }
    }
    if(noAssert) {
        DSState_unsetOption(state.get(), DS_OPTION_ASSERT);
    }


    // set rest argument
    char **shellArgs = begin;
    if(invocationKind == InvocationKind::FROM_FILE && (shellArgs[0] == nullptr || strcmp(shellArgs[0], "-") == 0)) {
        invocationKind = InvocationKind::FROM_STDIN;
    }

    // execute
    switch(invocationKind) {
    case InvocationKind::FROM_FILE: {
        const char *scriptName = shellArgs[0];
        DSState_setShellName(state.get(), scriptName);
        DSState_setArguments(state.get(), shellArgs + 1);
        return apply(DSState_loadAndEval, state, scriptName);
    }
    case InvocationKind::FROM_STDIN: {
        DSState_setArguments(state.get(), shellArgs);

        if(isatty(STDIN_FILENO) == 0 && !forceInteractive) {  // pipe line mode
            return apply(DSState_loadAndEval, state, nullptr);
        } else {    // interactive mode
            if(!quiet) {
                fprintf(stdout, "%s\n%s\n", version(), DSState_copyright());
            }
            if(userc) {
                auto ret = loadRC(state, rcfile);
                if(ret.first != DS_ERROR_KIND_SUCCESS) {
                    return ret.second;
                }
            }
            return exec_interactive(state.get());
        }
    }
    case InvocationKind::FROM_STRING: {
        DSState_setShellName(state.get(), shellArgs[0]);
        DSState_setArguments(state.get(), shellArgs[0] == nullptr ? nullptr : shellArgs + 1);
        return apply(DSState_eval, state, "(string)", evalText, strlen(evalText));
    }
    case InvocationKind::BUILTIN:
        return DSState_exec(state.get(), shellArgs);
    }
}
