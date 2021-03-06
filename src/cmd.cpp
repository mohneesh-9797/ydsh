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

#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <poll.h>
#include <sys/resource.h>

#include <cstdlib>
#include <ydsh/ydsh.h>

#include "vm.h"
#include "misc/num_util.hpp"
#include "misc/files.h"

extern char **environ;  //NOLINT

namespace ydsh {

// builtin command definition
static int builtin___gets(DSState &state, ArrayObject &argvObj);
static int builtin___puts(DSState &state, ArrayObject &argvObj);
static int builtin_cd(DSState &state, ArrayObject &argvObj);
static int builtin_check_env(DSState &state, ArrayObject &argvObj);
static int builtin_complete(DSState &state, ArrayObject &argvObj);
static int builtin_echo(DSState &state, ArrayObject &argvObj);
static int builtin_exit(DSState &state, ArrayObject &argvObj);
static int builtin_false(DSState &state, ArrayObject &argvObj);
static int builtin_fg_bg(DSState &state, ArrayObject &argvObj);
static int builtin_hash(DSState &state, ArrayObject &argvObj);
static int builtin_help(DSState &state, ArrayObject &argvObj);
static int builtin_kill(DSState &state, ArrayObject &argvObj);
static int builtin_pwd(DSState &state, ArrayObject &argvObj);
static int builtin_read(DSState &state, ArrayObject &argvObj);
static int builtin_setenv(DSState &state, ArrayObject &argvObj);
static int builtin_shctl(DSState &state, ArrayObject &argvObj);
static int builtin_test(DSState &state, ArrayObject &argvObj);
static int builtin_true(DSState &state, ArrayObject &argvObj);
static int builtin_ulimit(DSState &state, ArrayObject &argvObj);
static int builtin_umask(DSState &state, ArrayObject &argvObj);
static int builtin_unsetenv(DSState &state, ArrayObject &argvObj);

static constexpr struct {
    const char *commandName;
    builtin_command_t cmd_ptr;
    const char *usage;
    const char *detail;
} builtinCommands[] {
        {":", builtin_true, "",
                "    Null command.  Always success (exit status is 0)."},
        {"__gets", builtin___gets, "",
                "    Read standard input and write to standard output."},
        {"__puts", builtin___puts, "[-1 arg1] [-2 arg2]",
                "    Print specified argument to standard output/error and print new line.\n"
                "    Options:\n"
                "        -1    print to standard output\n"
                "        -2    print to standard error"},
        {"bg", builtin_fg_bg, "[job_spec ...]",
                "    Move jobs to the background.\n"
                "    If JOB_SPEC is not present, latest job is used."},
        {"cd", builtin_cd, "[-LP] [dir]",
                "    Changing the current directory to DIR.  The Environment variable\n"
                "    HOME is the default DIR.  A null directory name is the same as\n"
                "    the current directory.  If -L is specified, use logical directory \n"
                "    (with symbolic link).  If -P is specified, use physical directory \n"
                "    (without symbolic link).  Default is -L."},
        {"checkenv", builtin_check_env, "variable ...",
                "    Check existence of specified environmental variables.\n"
                "    If all of variables are exist and not empty string, exit with 0."},
        {"command", nullptr, "[-pVv] command [arg ...]",
                "    Execute COMMAND with ARGs excepting user defined command.\n"
                        "    If -p option is specified, search command from default PATH.\n"
                        "    If -V or -v option are specified, print description of COMMAND.\n"
                        "    -V option shows more detailed information."},
        {"complete", builtin_complete, "line",
                "    Show completion candidates."},
        {"echo", builtin_echo, "[-neE] [arg ...]",
                "    Print argument to standard output and print new line.\n"
                "    Options:\n"
                "        -n    not print new line\n"
                "        -e    interpret some escape sequence\n"
                "                  \\\\    backslash\n"
                "                  \\a    bell\n"
                "                  \\b    backspace\n"
                "                  \\c    ignore subsequent string\n"
                "                  \\e    escape sequence\n"
                "                  \\E    escape sequence\n"
                "                  \\f    form feed\n"
                "                  \\n    newline\n"
                "                  \\r    carriage return\n"
                "                  \\t    horizontal tab\n"
                "                  \\v    vertical tab\n"
                "                  \\0nnn N is octal number.  NNN can be 0 to 3 number\n"
                "                  \\xnn  N is hex number.  NN can be 1 to 2 number\n"
                "        -E    disable escape sequence interpretation"},
        {"eval", nullptr, "[arg ...]",
                "    Evaluate ARGs as command."},
        {"exec", nullptr, "[-c] [-a name] file [args ...]",
                "    Execute FILE and replace this shell with specified program.\n"
                "    If FILE is not specified, the redirections take effect in this shell.\n"
                "    IF FILE execution fail, terminate this shell immediately\n"
                "    Options:\n"
                "        -c    cleaner environmental variable\n"
                "        -a    specify set program name(default is FILE)"},
        {"exit", builtin_exit, "[n]",
                "    Exit the shell with a status of N.  If N is omitted, the exit\n"
                "    status is $?."},
        {"false", builtin_false, "",
                "    Always failure (exit status is 1)."},
        {"fg", builtin_fg_bg, "[job_spec]",
                "    Move job to the foreground.\n"
                "    If JOB_SPEC is not present, latest job is used."},
        {"hash", builtin_hash, "[-r] [command ...]",
                "    Cache file path of specified commands.  If -r option is supplied,\n"
                "    removes specified command path (if not specified, remove all cache).\n"
                "    If option is not supplied, display all cached path."},
        {"help", builtin_help, "[-s] [pattern ...]",
                "    Display helpful information about builtin commands."},
        {"kill", builtin_kill, "[-s signal] pid | jobspec ... or kill -l [signal...]",
                "    Send a signal to a process or job.\n"
                "    If signal is not specified, then SIGTERM is assumed.\n"
                "    Options:\n"
                "        -s sig    send a signal.  SIG is a signal name or signal number\n"
                "        -l        list the signal names"},
        {"pwd", builtin_pwd, "[-LP]",
                "    Print the current working directory(absolute path).\n"
                "    If -L specified, print logical working directory.\n"
                "    If -P specified, print physical working directory\n"
                "    (without symbolic link).  Default is -L."},
        {"read", builtin_read, "[-r] [-p prompt] [-f field separator] [-u fd] [-t timeout] [name ...]",
                "    Read from standard input.\n"
                "    Options:\n"
                "        -r         disable backslash escape\n"
                "        -p         specify prompt string\n"
                "        -f         specify field separator (if not, use IFS)\n"
                "        -s         disable echo back\n"
                "        -u         specify file descriptor\n"
                "        -t timeout set timeout second (only available if input fd is a tty)"},
        {"setenv", builtin_setenv, "[name=env ...]",
                "    Set environmental variables."},
        {"shctl", builtin_shctl, "[subcommand]",
                "    Query and set runtime information\n"
                "    Subcommands:\n"
                "        is-interactive      return 0 if shell is interactive mode.\n"
                "        is-sourced          return 0 if current script is sourced.\n"
                "        backtrace           print stack trace.\n"
                "        function            print current function/command name.\n"
                "        show  [OPTION ...]  print runtime option setting.\n"
                "        set   OPTION ...    set/enable/on runtime option.\n"
                "        unset OPTION ...    unset/disable/off runtime option"},
        {"test", builtin_test, "[expr]",
                "    Unary or Binary expressions.\n"
                "    If expression is true, return 0\n"
                "    If expression is false, return 1\n"
                "    If operand or operator is invalid, return 2\n"
                "\n"
                "    String operators:\n"
                "        -z STRING      check if string is empty\n"
                "        -n STRING\n"
                "        STRING         check if string is not empty\n"
                "        STRING1 = STRING2\n"
                "        STRING1 == STRING2\n"
                "                       check if strings are equal\n"
                "        STRING1 != STRING2\n"
                "                       check if strings are not equal\n"
                "        STRING1 < STRING2\n"
                "                       check if STRING1 is less than STRING2 with dictionary order\n"
                "        STRING1 > STRING2\n"
                "                       check if STRING2 is greater than STRING2 with dictionary order\n"
                "    Integer operators:\n"
                "        INT1 -eq INT2  check if integers are equal\n"
                "        INT1 -ne INT2  check if integers are not equal\n"
                "        INT1 -lt INT2  check if INT1 is less than INT2\n"
                "        INT1 -gt INT2  check if INT1 is greater than INT2\n"
                "        INT1 -le INT2  check if INT1 is less than or equal to INT2\n"
                "        INT1 -ge INT2  check if INT1 is greater than or equal to INT2\n"
                "\n"
                "    Integer value is signed int 64.\n"
                "\n"
                "    File operators:\n"
                "        -a FILE\n"
                "        -e FILE        check if file exists\n"
                "        -b FILE        check if file is block device\n"
                "        -c FILE        check if file is character device\n"
                "        -d FILE        check if file is a directory\n"
                "        -f FILE        check if file is a regular file\n"
                "        -g FILE        check if file has set-group-id bit\n"
                "        -h FILE\n"
                "        -L FILE        check if file is a symbolic link\n"
                "        -k FILE        check if file has sticky bit\n"
                "        -p FILE        check if file is a named pipe\n"
                "        -r FILE        check if file is readable\n"
                "        -s FILE        check if file is not empty\n"
                "        -S FILE        check if file is a socket\n"
                "        -t FD          check if file descriptor is a terminal\n"
                "        -u FILE        check if file has set-user-id bit\n"
                "        -w FILE        check if file is writable\n"
                "        -x FILE        check if file is executable\n"
                "        -O FILE        check if file is effectively owned by user\n"
                "        -G FILE        check if file is effectively owned by group\n"
                "\n"
                "        FILE1 -nt FILE2  check if file1 is newer than file2\n"
                "        FILE1 -ot FILE2  check if file1 is older than file2\n"
                "        FILE1 -ef FILE2  check if file1 and file2 refer to the same file"},
        {"true", builtin_true, "",
                "    Always success (exit status is 0)."},
        {"ulimit", builtin_ulimit, "[-H | -S] [-a | -"
                                   #define DEF(O, R, S, N, D) O
                                   #include "ulimit-def.in"
                                   #undef DEF
                                   " [value]]",
                "    Set or show resource limits of the shell and processes started by the shell.\n"
                "    If VALUE is `soft', `hard' and `unlimited', represent current soft limit\n"
                "    and hard limit and no limit. If no option specified, assume `-f'.\n"
                "    Options.\n"
                "        -H    use `hard' resource limit\n"
                "        -S    use `soft' resource limit (default)\n"
                "        -a    show all resource limits"
#define DEF(O, R, S, N, D) "\n        -" O "    " D
                #include "ulimit-def.in"
#undef DEF
                },
        {"umask", builtin_umask, "[-p] [-S] [mode]",
                "    Display or set file mode creation mask.\n"
                "    Set the calling process's file mode creation mask to MODE.\n"
                "    If MODE is omitted, prints current value of mask.\n"
                "    Options.\n"
                "        -p    if mode is omitted, print current mask in a form that may be reused as input\n"
                "        -S    print current mask in a symbolic form"},
        {"unsetenv", builtin_unsetenv, "[name ...]",
                "    Unset environmental variables."},
};

unsigned int getBuiltinCommandSize() {
    return arraySize(builtinCommands);
}

const char *getBuiltinCommandName(unsigned int index) {
    assert(index < getBuiltinCommandSize());
    return builtinCommands[index].commandName;
}

static CStringHashMap<unsigned int> initBuiltinMap() {
    CStringHashMap<unsigned int> map;
    for(unsigned int i = 0; i < arraySize(builtinCommands); i++) {
        map.insert(std::make_pair(builtinCommands[i].commandName, i));
    }
    return map;
}

/**
 * return null, if not found builtin command.
 */
builtin_command_t lookupBuiltinCommand(const char *commandName) {
    /**
     * builtin command name and index.
     */
    static auto builtinMap = initBuiltinMap();

    auto iter = builtinMap.find(commandName);
    if(iter == builtinMap.end()) {
        return nullptr;
    }
    return builtinCommands[iter->second].cmd_ptr;
}

static void printAllUsage(FILE *fp) {
    for(const auto &e : builtinCommands) {
        fprintf(fp, "%s %s\n", e.commandName, e.usage);
    }
}

static bool startsWith(const char *prefix, const char *target) {
    const unsigned int prefixSize = strlen(prefix);
    const unsigned int targetSize = strlen(target);
    return prefixSize <= targetSize && strncmp(prefix, target, prefixSize) == 0;
}

/**
 * if not found command, return false.
 */
static bool printUsage(FILE *fp, const char *prefix, bool isShortHelp = true) {
    bool matched = false;
    for(const auto &e : builtinCommands) {
        const char *cmdName = e.commandName;
        if(startsWith(prefix, cmdName)) {
            fprintf(fp, "%s: %s %s\n", cmdName, cmdName, e.usage);
            if(!isShortHelp) {
                fprintf(fp, "%s\n", e.detail);
            }
            matched = true;
        }
    }
    return matched;
}

static int builtin_help(DSState &, ArrayObject &argvObj) {
    const unsigned int size = argvObj.getValues().size();

    if(size == 1) {
        printAllUsage(stdout);
        return 0;
    }
    bool isShortHelp = false;
    bool foundValidCommand = false;
    for(unsigned int i = 1; i < size; i++) {
        const char *arg = str(argvObj.getValues()[i]);
        if(strcmp(arg, "-s") == 0 && size == 2) {
            printAllUsage(stdout);
            foundValidCommand = true;
        } else if(strcmp(arg, "-s") == 0 && i == 1) {
            isShortHelp = true;
        } else {
            if(printUsage(stdout, arg, isShortHelp)) {
                foundValidCommand = true;
            }
        }
    }
    if(!foundValidCommand) {
        ERROR(argvObj, "no help topics match `%s'.  Try `help help'.", str(argvObj.getValues()[size - 1]));
        return 1;
    }
    return 0;
}

static int showUsage(const ArrayObject &obj) {
    printUsage(stderr, str(obj.getValues()[0]));
    return 2;
}

int invalidOptionError(const ArrayObject &obj, const GetOptState &s) {
    ERROR(obj, "-%c: invalid option", s.optOpt);
    return showUsage(obj);
}

static int invalidOptionError(const ArrayObject &obj, const char *opt) {
    ERROR(obj, "%s: invalid option", opt);
    return showUsage(obj);
}

static int builtin_cd(DSState &state, ArrayObject &argvObj) {
    GetOptState optState;
    bool useLogical = true;
    for(int opt; (opt = optState(argvObj, "PL")) != -1;) {
        switch(opt) {
        case 'P':
            useLogical = false;
            break;
        case 'L':
            useLogical = true;
            break;
        default:
            return invalidOptionError(argvObj, optState);
        }
    }

    unsigned int index = optState.index;
    const char *dest = nullptr;
    bool useOldpwd = false;
    if(index < argvObj.getValues().size()) {
        dest = str(argvObj.getValues()[index]);
        if(strcmp(dest, "-") == 0) {
            dest = getenv(ENV_OLDPWD);
            if(dest == nullptr) {
                ERROR(argvObj, "OLDPWD not set");
                return 1;
            }
            useOldpwd = true;
        }
    } else {
        dest = getenv(ENV_HOME);
        if(dest == nullptr) {
            ERROR(argvObj, "HOME not set");
            return 1;
        }
    }

    if(useOldpwd && dest != nullptr) {
        printf("%s\n", dest);
    }

    if(!changeWorkingDir(state, dest, useLogical)) {
        PERROR(argvObj, "%s", dest);
        return 1;
    }
    return 0;
}

static int builtin_check_env(DSState &, ArrayObject &argvObj) {
    const unsigned int size = argvObj.getValues().size();
    if(size == 1) {
        return showUsage(argvObj);
    }
    for(unsigned int i = 1; i < size; i++) {
        const char *env = getenv(str(argvObj.getValues()[i]));
        if(env == nullptr || strlen(env) == 0) {
            return 1;
        }
    }
    return 0;
}

static int builtin_echo(DSState &, ArrayObject &argvObj) {
    bool newline = true;
    bool interpEscape = false;

    GetOptState optState;
    for(int opt; (opt = optState(argvObj, "neE")) != -1;) {
        switch(opt) {
        case 'n':
            newline = false;
            break;
        case 'e':
            interpEscape = true;
            break;
        case 'E':
            interpEscape = false;
            break;
        default:
            goto END;
        }
    }

    END:
    // print argument
    if(optState.index > 1 && strcmp(str(argvObj.getValues()[optState.index - 1]), "--") == 0) {
        optState.index--;
    }

    unsigned int index = optState.index;
    const unsigned int argc = argvObj.getValues().size();
    bool firstArg = true;
    for(; index < argc; index++) {
        if(firstArg) {
            firstArg = false;
        } else {
            fputc(' ', stdout);
        }
        if(!interpEscape) {
            fputs(str(argvObj.getValues()[index]), stdout);
            continue;
        }
        const char *arg = str(argvObj.getValues()[index]);
        for(unsigned int i = 0; arg[i] != '\0'; i++) {
            int ch = arg[i];
            if(ch == '\\' && arg[i + 1] != '\0') {
                switch(arg[++i]) {
                case '\\':
                    ch = '\\';
                    break;
                case 'a':
                    ch = '\a';
                    break;
                case 'b':
                    ch = '\b';
                    break;
                case 'c':   // stop printing
                    return 0;
                case 'e':
                case 'E':
                    ch = '\033';
                    break;
                case 'f':
                    ch = '\f';
                    break;
                case 'n':
                    ch = '\n';
                    break;
                case 'r':
                    ch = '\r';
                    break;
                case 't':
                    ch = '\t';
                    break;
                case 'v':
                    ch = '\v';
                    break;
                case '0': {
                    int v = 0;
                    for(unsigned int c = 0; c < 3; c++) {
                        if(isOctal(arg[i + 1])) {
                            v *= 8;
                            v += arg[++i] - '0';
                        } else {
                            break;
                        }
                    }
                    ch = static_cast<char>(v);
                    break;
                }
                case 'x': {
                    if(isHex(arg[i + 1])) {
                        int v = hexToNum(arg[++i]);
                        if(isHex(arg[i + 1])) {
                            v *= 16;
                            v += hexToNum(arg[++i]);
                        }
                        ch = static_cast<char>(v);
                        break;
                    }
                    i--;
                    break;
                }
                default:
                    i--;
                    break;
                }
            }
            fputc(ch, stdout);
        }
    }

    if(newline) {
        fputc('\n', stdout);
    }
    return 0;
}

static int parseExitStatus(const DSState &state, const ArrayObject &argvObj) {
    int64_t ret = state.getGlobal(BuiltinVarOffset::EXIT_STATUS).asInt();
    if(argvObj.getValues().size() > 1) {
        const char *num = str(argvObj.getValues()[1]);
        auto pair = convertToNum<int64_t>(num);
        if(pair.second) {
            ret = pair.first;
        }
    }
    return maskExitStatus(ret);
}

static int builtin_exit(DSState &state, ArrayObject &argvObj) {
    int ret = parseExitStatus(state, argvObj);

    if(hasFlag(state.compileOption, CompileOption::INTERACTIVE)) {
        state.jobTable.send(SIGHUP);
    }

    std::string str("terminated by exit ");
    str += std::to_string(ret);
    raiseError(state, TYPE::_ShellExit, std::move(str), ret);
    return ret;
}

static int builtin_true(DSState &, ArrayObject &) {
    return 0;
}

static int builtin_false(DSState &, ArrayObject &) {
    return 1;
}

/**
 * for stdin redirection test
 */
static int builtin___gets(DSState &, ArrayObject &) {
    char buf[256];
    int readSize = 0;
    while((readSize = read(STDIN_FILENO, buf, arraySize(buf))) > 0) {
        int r = write(STDOUT_FILENO, buf, readSize);
        (void) r;
    }
    return 0;
}

/**
 * for stdout/stderr redirection test
 */
static int builtin___puts(DSState &, ArrayObject &argvObj) {
    GetOptState optState;
    for(int opt; (opt = optState(argvObj, "1:2:")) != -1;) {
        switch(opt) {
        case '1':
            fputs(optState.optArg, stdout);
            fputc('\n', stdout);
            fflush(stdout);
            break;
        case '2':
            fputs(optState.optArg, stderr);
            fputc('\n', stderr);
            fflush(stderr);
            break;
        default:
            return 1;
        }
    }
    return 0;
}

static int builtin_pwd(DSState &state, ArrayObject &argvObj) {
    bool useLogical = true;

    GetOptState optState;
    for(int opt; (opt = optState(argvObj, "LP")) != -1;) {
        switch(opt) {
        case 'L':
            useLogical = true;
            break;
        case 'P':
            useLogical = false;
            break;
        default:
            return invalidOptionError(argvObj, optState);
        }
    }

    auto workdir = getWorkingDir(state, useLogical);
    if(!workdir) {
        PERROR(argvObj, ".");
        return 1;
    }
    printf("%s\n", workdir.get());
    return 0;
}

#define EACH_STR_COMP_OP(OP) \
    OP(STR_EQ, "==", ==) \
    OP(STR_EQ2, "=", ==) \
    OP(STR_NE, "!=", !=) \
    OP(STR_LT, "<", <) \
    OP(STR_GT, ">", >)


#define EACH_INT_COMP_OP(OP) \
    OP(EQ, "-eq", ==) \
    OP(NE, "-ne", !=) \
    OP(LT, "-lt", <) \
    OP(GT, "-gt", >) \
    OP(LE, "-le", <=) \
    OP(GE, "-ge", >=)

#define EACH_FILE_COMP_OP(OP) \
    OP(NT, "-nt", %) \
    OP(OT, "-ot", %) \
    OP(EF, "-ef", %)


enum class BinaryOp : unsigned int {
    INVALID,
#define GEN_ENUM(E, S, O) E,
    EACH_STR_COMP_OP(GEN_ENUM)
    EACH_INT_COMP_OP(GEN_ENUM)
    EACH_FILE_COMP_OP(GEN_ENUM)
#undef GEN_ENUM
};

static BinaryOp resolveBinaryOp(StringRef opStr) {
    const struct {
        const char *k;
        BinaryOp op;
    } table[] = {
#define GEN_ENTRY(E, S, O) {S, BinaryOp::E},
            EACH_INT_COMP_OP(GEN_ENTRY)
            EACH_STR_COMP_OP(GEN_ENTRY)
            EACH_FILE_COMP_OP(GEN_ENTRY)
#undef GEN_ENTRY
    };
    for(auto &e : table) {
        if(opStr == e.k) {
            return e.op;
        }
    }
    return BinaryOp::INVALID;
}

static bool compareStr(const DSValue &left, BinaryOp op, const DSValue &right) {
    auto x = left.asStrRef();
    auto y = right.asStrRef();

    switch(op) {
#define GEN_CASE(E, S, O) case BinaryOp::E: return x O y;
    EACH_STR_COMP_OP(GEN_CASE)
#undef GEN_CASE
    default:
        break;
    }
    return false;
}

static bool compareInt(int64_t x, BinaryOp op, int64_t y) {
    switch(op) {
#define GEN_CASE(E, S, O) case BinaryOp::E: return x O y;
    EACH_INT_COMP_OP(GEN_CASE)
#undef GEN_CASE
    default:
        break;
    }
    return false;
}

static bool operator<(const timespec &left, const timespec &right) {
    if(left.tv_sec == right.tv_sec) {
        return left.tv_nsec < right.tv_nsec;
    }
    return left.tv_sec < right.tv_sec;
}

static bool compareFile(const DSValue &left, BinaryOp op, const DSValue &right) {
    auto x = left.asStrRef();
    auto y = right.asStrRef();

    struct stat st1;
    struct stat st2;

    if(stat(x.data(), &st1) != 0) {
        return false;
    }
    if(stat(y.data(), &st2) != 0) {
        return false;
    }

    switch(op) {
    case BinaryOp::NT:
#ifdef __APPLE__
        return st2.st_mtimespec < st1.st_mtimespec;
#else
        return st2.st_mtim < st1.st_mtim;
#endif
    case BinaryOp::OT:
#ifdef __APPLE__
        return st1.st_mtimespec < st2.st_mtimespec;
#else
        return st1.st_mtim < st2.st_mtim;
#endif
    case BinaryOp::EF:
        return st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino;
    default:
        return false;
    }
}

static int parseFD(const char *value) {
    if(value == strstr(value, "/dev/fd/")) {
        value += strlen("/dev/fd/");
    }
    auto ret = convertToNum<int32_t>(value);
    if(!ret.second || ret.first < 0) {
        return -1;
    }
    return ret.first;
}

static int builtin_test(DSState &, ArrayObject &argvObj) {
    bool result = false;
    unsigned int argc = argvObj.getValues().size();
    const unsigned int argSize = argc - 1;

    switch(argSize) {
    case 0: {
        result = false;
        break;
    }
    case 1: {
        result = !argvObj.getValues()[1].asStrRef().empty();  // check if string is not empty
        break;
    }
    case 2: {   // unary op
        const char *op = str(argvObj.getValues()[1]);
        const auto &obj = argvObj.getValues()[2];
        const char *value = str(obj);
        if(strlen(op) != 2 || op[0] != '-') {
            ERROR(argvObj, "%s: invalid unary operator", op);
            return 2;
        }

        const char opKind = op[1];  // ignore -
        switch(opKind) {
        case 'z': { // check if string is empty
            result = obj.asStrRef().empty();
            break;
        }
        case 'n': { // check if string not empty
            result = !obj.asStrRef().empty();
            break;
        }
        case 'a':
        case 'e': {
            result = access(value, F_OK) == 0;  // check if file exists
            break;
        }
        case 'b': {
            result = S_ISBLK(getStMode(value)); // check if file is block device
            break;
        }
        case 'c': {
            result = S_ISCHR(getStMode(value)); // check if file is character device
            break;
        }
        case 'd': {
            result = S_ISDIR(getStMode(value)); // check if file is directory
            break;
        }
        case 'f': {
            result = S_ISREG(getStMode(value)); // check if file is regular file.
            break;
        }
        case 'g': {
            result = S_IS_PERM_(getStMode(value), S_ISUID); // check if file has set-uid-bit
            break;
        }
        case 'h':
        case 'L': {
            mode_t mode = 0;
            struct stat st{};
            if(lstat(value, &st) == 0) {
                mode = st.st_mode;
            }
            result = S_ISLNK(mode); // check if file is symbolic-link
            break;
        }
        case 'k': {
            result = S_IS_PERM_(getStMode(value), S_ISVTX); // check if file has sticky bit
            break;
        }
        case 'p': {
            result = S_ISFIFO(getStMode(value));    // check if file is a named pipe
            break;
        }
        case 'r': {
            result = access(value, R_OK) == 0;  // check if file is readable
            break;
        }
        case 's': {
            struct stat st{};
            result = stat(value, &st) == 0 && st.st_size != 0;  // check if file is not empty
            break;
        }
        case 'S': {
            result = S_ISSOCK(getStMode(value));    // check file is a socket
            break;
        }
        case 't': {
            int fd = parseFD(value);
            result = fd > -1 && isatty(fd) != 0;    //  check if FD is a terminal
            break;
        }
        case 'u': {
            result = S_IS_PERM_(getStMode(value), S_ISUID); // check file has set-user-id bit
            break;
        }
        case 'w': {
            result = access(value, W_OK) == 0;   // check if file is writable
            break;
        }
        case 'x': {
            result = access(value, X_OK) == 0;  // check if file is executable
            break;
        }
        case 'O': {
            struct stat st{};
            result = stat(value, &st) == 0 && st.st_uid == geteuid();   // check if file is effectively owned
            break;
        }
        case 'G': {
            struct stat st{};
            result = stat(value, &st) == 0 && st.st_gid == getegid();   // check if file is effectively owned by group
            break;
        }
        default: {
            ERROR(argvObj, "%s: invalid unary operator", op);
            return 2;
        }
        }
        break;
    }
    case 3: {   // binary op
        const auto &left = argvObj.getValues()[1];
        auto op = argvObj.getValues()[2].asStrRef();
        auto opKind = resolveBinaryOp(op);
        const auto &right = argvObj.getValues()[3];

        switch(opKind) {
#define GEN_CASE(E, S, O) case BinaryOp::E:
        EACH_STR_COMP_OP(GEN_CASE) {
            result = compareStr(left, opKind, right);
            break;
        }
        EACH_INT_COMP_OP(GEN_CASE) {
            auto pair = convertToNum<int64_t>(str(left));
            int64_t n1 = pair.first;
            if(!pair.second) {
                ERROR(argvObj, "%s: must be integer", str(left));
                return 2;
            }

            pair = convertToNum<int64_t>(str(right));
            long n2 = pair.first;
            if(!pair.second) {
                ERROR(argvObj, "%s: must be integer", str(right));
                return 2;
            }

            result = compareInt(n1, opKind, n2);
            break;
        }
        EACH_FILE_COMP_OP(GEN_CASE) {
            result = compareFile(left, opKind, right);
            break;
        }
#undef GEN_CASE
        case BinaryOp::INVALID:
            ERROR(argvObj, "%s: invalid binary operator", op.data());   //FIXME:
            return 2;
        }
        break;
    }
    default: {
        ERROR(argvObj, "too many arguments");
        return 2;
    }
    }
    return result ? 0 : 1;
}

static int xfgetc(int fd, int timeout) {
    signed char ch;
    do {
        errno = 0;

        if(timeout > -2) {
            struct pollfd pollfd[1];
            pollfd[0].fd = fd;
            pollfd[0].events = POLLIN;
            if(poll(pollfd, 1, timeout) != 1) {
                return EOF;
            }
        }

        if(read(fd, &ch, 1) <= 0) {
            ch = EOF;
        }
    } while(static_cast<int>(ch) == EOF && (errno == EAGAIN || errno == EINTR));
    return ch;
}

static int builtin_read(DSState &state, ArrayObject &argvObj) {  //FIXME: timeout, UTF-8
    const char *prompt = "";
    const char *ifs = nullptr;
    unsigned int ifsSize = 0;
    bool backslash = true;
    bool noecho = false;
    int fd = STDIN_FILENO;
    int timeout = -1;

    GetOptState optState;
    for(int opt; (opt = optState(argvObj, ":rp:f:su:t:")) != -1;) {
        switch(opt) {
        case 'p':
            prompt = optState.optArg;
            break;
        case 'f':
            ifs = optState.optArg;
            ifsSize = argvObj.getValues()[optState.index - 1].asStrRef().size();
            break;
        case 'r':
            backslash = false;
            break;
        case 's':
            noecho = true;
            break;
        case 'u': {
            const char *value = optState.optArg;
            fd = parseFD(value);
            if(fd < 0) {
                ERROR(argvObj, "%s: invalid file descriptor", value);
                return 1;
            }
            break;
        }
        case 't': {
            auto ret = convertToNum<int64_t>(optState.optArg);
            int64_t t = ret.first;
            if(ret.second) {
                if(t > -1 && t <= INT32_MAX) {
                    t *= 1000;
                    if(t > -1 && t <= INT32_MAX) {
                        timeout = static_cast<int>(t);
                        break;
                    }
                }
            }
            ERROR(argvObj, "%s: invalid timeout specification", optState.optArg);
            return 1;
        }
        case ':':
            ERROR(argvObj, "-%c: option require argument", optState.optOpt);
            return 2;
        default:
            return invalidOptionError(argvObj, optState);
        }
    }

    const unsigned int argc = argvObj.getValues().size();
    unsigned int index = optState.index;
    const bool isTTY = isatty(fd) != 0;

    // check ifs
    if(ifs == nullptr) {
        auto strRef = state.getGlobal(BuiltinVarOffset::IFS).asStrRef();
        ifs = strRef.data();
        ifsSize = strRef.size();
    }

    // clear old variable before read
    state.setGlobal(toIndex(BuiltinVarOffset::REPLY), DSValue::createStr());    // clear REPLY
    typeAs<MapObject>(state.getGlobal(BuiltinVarOffset::REPLY_VAR)).clear();      // clear reply


    const unsigned int varSize = argc - index;  // if zero, store line to REPLY
    const unsigned int varIndex = toIndex(varSize == 0 ? BuiltinVarOffset::REPLY : BuiltinVarOffset::REPLY_VAR);
    std::string strBuf;

    // show prompt
    if(isTTY) {
        fputs(prompt, stderr);
        fflush(stderr);
    }

    // change tty state
    struct termios tty{};
    struct termios oldtty{};
    if(noecho && isTTY) {
        tcgetattr(fd, &tty);
        oldtty = tty;
        tty.c_lflag &= ~(ECHO | ECHOK | ECHONL);
        tcsetattr(fd, TCSANOW, &tty);
    }

    // read line
    if(!isTTY) {
        timeout = -2;
    }
    unsigned int skipCount = 1;
    int ch;
    for(bool prevIsBackslash = false; (ch = xfgetc(fd, timeout)) != EOF;
            prevIsBackslash = backslash && ch == '\\' && !prevIsBackslash) {
        if(ch == '\n') {
            if(prevIsBackslash) {
                continue;
            }
            break;
        }
        if(ch == '\\' && !prevIsBackslash && backslash) {
            continue;
        }

        bool fieldSep = isFieldSep(ifsSize, ifs, ch) && !prevIsBackslash;
        if(fieldSep && skipCount > 0) {
            if(isSpace(ch)) {
                continue;
            }
            if(--skipCount == 1) {
                continue;
            }
        }
        skipCount = 0;
        if(fieldSep && index < argc - 1) {
            auto &obj = typeAs<MapObject>(state.getGlobal(varIndex));
            auto varObj = argvObj.getValues()[index];
            auto valueObj = DSValue::createStr(std::move(strBuf));
            obj.set(std::move(varObj), std::move(valueObj));
            strBuf = "";
            index++;
            skipCount = isSpace(ch) ? 2 : 1;
            continue;
        }
        strBuf += static_cast<char>(ch);
    }

    // remove last spaces
    if(!strBuf.empty()) {
        // check if field separator has spaces
        bool hasSpace = false;
        for(unsigned int i = 0; ifs[i] != '\0'; i++) {
            if((hasSpace = isSpace(ifs[i]))) {
                break;
            }
        }

        if(hasSpace) {
            while(!strBuf.empty() && isSpace(strBuf.back())) {
                strBuf.pop_back();
            }
        }
    }

    if(varSize == 0) {
        state.setGlobal(varIndex, DSValue::createStr(std::move(strBuf)));
        strBuf = "";
    }

    // set rest variable
    for(; index < argc; index++) {
        auto &obj = typeAs<MapObject>(state.getGlobal(varIndex));
        auto varObj = argvObj.getValues()[index];
        auto valueObj = DSValue::createStr(std::move(strBuf));
        obj.set(std::move(varObj), std::move(valueObj));
        strBuf = "";
    }

    // restore tty setting
    if(noecho && isTTY) {
        tcsetattr(fd, TCSANOW, &oldtty);
    }

    // report error
    int ret = ch == EOF ? 1 : 0;
    if(ret != 0 && errno != 0) {
        PERROR(argvObj, "%d", fd);
    }
    return ret;
}

static int builtin_hash(DSState &state, ArrayObject &argvObj) {
    bool remove = false;

    // check option
    const unsigned int argc = argvObj.getValues().size();
    unsigned int index = 1;
    for(; index < argc; index++) {
        const char *arg = str(argvObj.getValues()[index]);
        if(arg[0] != '-') {
            break;
        }
        if(strcmp(arg, "-r") == 0) {
            remove = true;
        } else {
            return invalidOptionError(argvObj, arg);
        }
    }

    const bool hasNames = index < argc;
    if(hasNames) {
        for(; index < argc; index++) {
            const char *name = str(argvObj.getValues()[index]);
            if(remove) {
                state.pathCache.removePath(name);
            } else {
                if(state.pathCache.searchPath(name) == nullptr) {
                    ERROR(argvObj, "%s: not found", name);
                    return 1;
                }
            }
        }
    } else {
        if(remove) {    // remove all cache
            state.pathCache.clear();
        } else {    // show all cache
            const auto cend = state.pathCache.end();
            if(state.pathCache.begin() == cend) {
                fputs("hash: file path cache is empty\n", stdout);
                return 0;
            }
            for(auto &entry : state.pathCache) {
                printf("%s=%s\n", entry.first, entry.second.c_str());
            }
        }
    }
    return 0;
}

// for completor debugging
static int builtin_complete(DSState &state, ArrayObject &argvObj) {
    const unsigned int argc = argvObj.getValues().size();
    if(argc != 2) {
        return showUsage(argvObj);
    }

    auto strRef = argvObj.getValues()[1].asStrRef();
    completeLine(state, strRef.data(), strRef.size());
    auto &ret = typeAs<ArrayObject>(state.getGlobal(BuiltinVarOffset::COMPREPLY));
    for(const auto &e : ret.getValues()) {
        fputs(str(e), stdout);
        fputc('\n', stdout);
    }
    return 0;
}

static int builtin_setenv(DSState &, ArrayObject &argvObj) {
    if(argvObj.size() == 1) {
        for(unsigned int i = 0; environ[i] != nullptr; i++) {
            const char *e = environ[i];
            fprintf(stdout, "%s\n", e);
        }
        return 0;
    }

    auto end = argvObj.getValues().end();
    for(auto iter = argvObj.getValues().begin() + 1; iter != end; ++iter) {
        const char *kv = str(*iter);
        auto *ptr = strchr(kv, '=');
        errno = EINVAL;
        if(ptr != nullptr && ptr != kv) {
            std::string name(kv, ptr - kv);
            if(setenv(name.c_str(), ptr + 1, 1) == 0) {
                continue;
            }
        }
        PERROR(argvObj, "%s", kv);
        return 1;
    }
    return 0;
}

static int builtin_unsetenv(DSState &, ArrayObject &argvObj) {
    auto end = argvObj.getValues().end();
    for(auto iter = argvObj.getValues().begin() + 1; iter != end; ++iter) {
        const char *envName = str(*iter);
        if(unsetenv(envName) != 0) {
            PERROR(argvObj, "%s", envName);
            return 1;
        }
    }
    return 0;
}

static std::pair<int, bool> toInt32(const char *str) {
    return convertToNum<int32_t>(str);
}

static int toSigNum(const char *str) {
    if(isDecimal(*str)) {
        auto pair = toInt32(str);
        if(!pair.second) {
            return -1;
        }
        auto sigList = getUniqueSignalList();
        return std::binary_search(sigList.begin(), sigList.end(), pair.first) ? pair.first : -1;
    }
    return getSignalNum(str);
}

static bool printNumOrName(const char *str) {
    if(isDecimal(*str)) {
        auto pair = toInt32(str);
        if(!pair.second) {
            return false;
        }
        const char *name = getSignalName(pair.first);
        if(name == nullptr) {
            return false;
        }
        printf("%s\n", name);
    } else {
        int sigNum = getSignalNum(str);
        if(sigNum < 0) {
            return false;
        }
        printf("%d\n", sigNum);
    }
    fflush(stdout);
    return true;
}

static bool killProcOrJob(DSState &state, ArrayObject &argvObj, const char *arg, int sigNum) {
    bool isJob = *arg == '%';
    auto pair = toInt32(isJob ? arg + 1 : arg);
    if(!pair.second) {
        ERROR(argvObj, "%s: arguments must be process or job IDs", arg);
        return false;
    }

    if(isJob) {
        if(pair.first > 0) {
            auto job = state.jobTable.findEntry(static_cast<unsigned int>(pair.first));
            if(job) {
                job->send(sigNum);
                return true;
            }
        }
        ERROR(argvObj, "%s: no such job", arg);
        return false;
    }

    if(kill(pair.first, sigNum) < 0) {
        PERROR(argvObj, "%s", arg);
        return false;
    }
    return true;
}

// -s sig (pid | jobspec ...)
// -l
static int builtin_kill(DSState &state, ArrayObject &argvObj) {
    int sigNum = SIGTERM;
    bool listing = false;

    if(argvObj.getValues().size() == 1) {
        return showUsage(argvObj);
    }

    GetOptState optState;
    const int opt = optState(argvObj, ":ls:");
    switch(opt) {
    case 'l':
        listing = true;
        break;
    case 's':
    case '?': {
        const char *sigStr = optState.optArg;
        if(opt == '?') {
            sigStr = str(argvObj.getValues()[optState.index++]) + 1;
        }
        sigNum = toSigNum(sigStr);
        if(sigNum == -1) {
            ERROR(argvObj, "%s: invalid signal specification", sigStr);
            return 1;
        }
        break;
    }
    case ':':
        ERROR(argvObj, "-%c: option requires argument", optState.optOpt);
        return 1;
    default:
        break;
    }

    auto begin = argvObj.getValues().begin() + optState.index;
    const auto end = argvObj.getValues().end();

    if(begin == end) {
        if(listing) {
            auto sigList = getUniqueSignalList();
            unsigned int size = sigList.size();
            for(unsigned int i = 0; i < size; i++) {
                printf("%2d) SIG%s", sigList[i], getSignalName(sigList[i]));
                if(i % 5 == 4 || i == size - 1) {
                    fputc('\n', stdout);
                } else {
                    fputc('\t', stdout);
                }
            }
            return 0;
        }
        return showUsage(argvObj);
    }

    unsigned int count = 0;
    for(; begin != end; ++begin) {
        const char *arg = str(*begin);
        if(listing) {
            if(!printNumOrName(arg)) {
                count++;
                ERROR(argvObj, "%s: invalid signal specification", arg);
            }
        } else {
            if(killProcOrJob(state, argvObj, arg, sigNum)) {
                count++;
            }
        }
    }

    if(listing && count > 0) {
        return 1;
    }
    if(!listing && count == 0) {
        return 1;
    }
    return 0;
}

static Job tryToGetJob(const JobTable &table, const char *name) {
    if(*name == '%') {
        name++;
    }
    Job job;
    auto pair = toInt32(name);
    if(pair.second && pair.first > -1) {
        job = table.findEntry(pair.first);
    }
    return job;
}

static int builtin_fg_bg(DSState &state, ArrayObject &argvObj) {
    if(!state.isJobControl()) {
        ERROR(argvObj, "no job control in this shell");
        return 1;
    }

    bool fg = strcmp("fg", str(argvObj.getValues()[0])) == 0;
    unsigned int size = argvObj.getValues().size();
    assert(size > 0);
    Job job;
    const char *arg = "current";
    if(size == 1) {
        job = state.jobTable.getLatestEntry();
    } else {
        arg = str(argvObj.getValues()[1]);
        job = tryToGetJob(state.jobTable, arg);
    }

    int ret = 0;
    if(job) {
        if(fg) {
            tcsetpgrp(STDIN_FILENO, getpgid(job->getPid(0)));
        }
        job->send(SIGCONT);
    } else {
        ERROR(argvObj, "%s: no such job", arg);
        ret = 1;
        if(fg) {
            return ret;
        }
    }

    if(fg) {
        int s = state.jobTable.waitAndDetach(job, true);    //FIXME: check root shell
        tryToBeForeground(state);
        state.jobTable.updateStatus();
        return s;
    }

    // process remain arguments
    for(unsigned int i = 2; i < size; i++) {
        arg = str(argvObj.getValues()[i]);
        job = tryToGetJob(state.jobTable, arg);
        if(job) {
            job->send(SIGCONT);
        } else {
            ERROR(argvObj, "%s: no such job", arg);
            ret = 1;
        }
    }
    return ret;
}

// for ulimit command

static constexpr flag8_t RLIM_HARD = 1u << 0;
static constexpr flag8_t RLIM_SOFT = 1u << 1;

struct ulimitOp {
    char op;
    char resource;
    char shift;
    const char *name;

    void print(flag8_set_t limOpt, unsigned int maxNameLen) const {
        rlimit limit{};
        getrlimit(this->resource, &limit);

        if(maxNameLen) {
            printf("-%c: %s  ", this->op, this->name);
            for(unsigned int len = strlen(this->name); len < maxNameLen; len++) {
                printf(" ");
            }
        }

        auto value = hasFlag(limOpt, RLIM_HARD) ? limit.rlim_max : limit.rlim_cur;
        if(value == RLIM_INFINITY) {
            printf("unlimited\n");
        } else {
            value >>= this->shift;
            printf("%llu\n", static_cast<unsigned long long>(value));
        }
        fflush(stdout);
    }
};

struct UlimitOptEntry {
    enum Kind : unsigned char {
        UNUSED,
        NUM,
        SOFT,
        HARD,
        UNLIMITED,
    };

    Kind kind{UNUSED};
    rlim_t value{0};

    explicit operator bool() const {
        return this->kind != UNUSED;
    }

    rlim_t getValue(const rlimit &limit) const {
        switch(this->kind) {
        case SOFT:
            return limit.rlim_cur;
        case HARD:
            return limit.rlim_max;
        case UNLIMITED:
            return RLIM_INFINITY;
        default:
            return this->value;
        }
    }
};

static constexpr ulimitOp ulimitOps[] = {
#define DEF(O, R, S, N, D) {O[0], R, S, N},
#include "ulimit-def.in"
#undef DEF
};

static unsigned int computeMaxNameLen() {
    unsigned int max = 0;
    for(auto &e : ulimitOps) {
        unsigned int len = strlen(e.name);
        if(len > max) {
            max = len;
        }
    }
    return max;
}

static bool parseUlimitOpt(const char *str, unsigned int index, UlimitOptEntry &entry) {
    static_assert(sizeof(rlim_t) == sizeof(uint64_t), "must be uint64");

    if(strcasecmp(str, "soft") == 0) {
        entry.kind = UlimitOptEntry::SOFT;
        return true;
    } else if(strcasecmp(str, "hard") == 0) {
        entry.kind = UlimitOptEntry::HARD;
        return true;
    } else if(strcasecmp(str, "unlimited") == 0) {
        entry.kind = UlimitOptEntry::UNLIMITED;
        return true;
    }

    auto pair = convertToNum<uint64_t>(str);
    if(!pair.second) {
        return false;
    }

    entry.value = pair.first;
    entry.value <<= ulimitOps[index].shift;
    entry.kind = UlimitOptEntry::NUM;
    return true;
}

struct UlimitOptEntryTable {
    unsigned long printSet{0};
    std::array<UlimitOptEntry, arraySize(ulimitOps)> entries;
    unsigned int count{0};

    int tryToUpdate(GetOptState &optState, ArrayObject &argvObj, int opt) {
        const char *arg = nullptr;
        if(optState.index < argvObj.getValues().size() && *str(argvObj.getValues()[optState.index]) != '-') {
            arg = str(argvObj.getValues()[optState.index++]);
        }
        if(!this->update(opt, arg)) {
            ERROR(argvObj, "%s: invalid number", arg);
            return 1;
        }
        return 0;
    }

private:
    bool update(int ch, const char *str) {
        this->count++;
        // search entry
        for(unsigned int index = 0; index < arraySize(ulimitOps); index++) {
            if(ulimitOps[index].op == ch) {
                auto &entry = this->entries[index];
                if(str) {
                    if(!parseUlimitOpt(str, index, entry)) {
                        return false;
                    }
                } else {
                    setFlag(this->printSet, 1LU << index);
                }
            }
        }
        return true;
    }
};


static int builtin_ulimit(DSState &, ArrayObject &argvObj) {
    flag8_set_t limOpt = 0;
    bool showAll = false;

    GetOptState optState;
    const char *optStr = "HSa"
#define DEF(O, R, S, N, D) O
#include "ulimit-def.in"
#undef DEF
            ;

    UlimitOptEntryTable table;

    // parse option
    for(int opt; (opt = optState(argvObj, optStr)) != -1;) {
        switch(opt) {
        case 'H':
            setFlag(limOpt, RLIM_HARD);
            break;
        case 'S':
            setFlag(limOpt, RLIM_SOFT);
            break;
        case 'a':
            showAll = true;
            break;
        case '?':
            return invalidOptionError(argvObj, optState);
        default:
            int ret = table.tryToUpdate(optState, argvObj, opt);
            if(ret) {
                return ret;
            }
            break;
        }
    }

    // parse remain
    if(table.count == 0) {
        int ret = table.tryToUpdate(optState, argvObj, 'f');
        if(ret) {
            return ret;
        }
    }

    if(limOpt == 0) {
        setFlag(limOpt, RLIM_SOFT);
    }

    if(showAll) {
        unsigned int maxDescLen = computeMaxNameLen();
        for(auto &e : ulimitOps) {
            e.print(limOpt, maxDescLen);
        }
        return 0;
    }

    // print or set limit
    unsigned int maxNameLen = 0;
    if(table.printSet > 0 && (table.printSet & (table.printSet - 1)) != 0) {
        maxNameLen = computeMaxNameLen();
    }
    for(unsigned int index = 0; index < static_cast<unsigned int>(table.entries.size()); index++) {
        if(table.entries[index]) {
            const auto &op = ulimitOps[index];
            rlimit limit{};
            getrlimit(op.resource, &limit);
            rlim_t value = table.entries[index].getValue(limit);
            if(hasFlag(limOpt, RLIM_SOFT)) {
                limit.rlim_cur = value;
            }
            if(hasFlag(limOpt, RLIM_HARD)) {
                limit.rlim_max = value;
            }
            if(setrlimit(op.resource, &limit) < 0) {
                PERROR(argvObj, "%s: cannot change limit", op.name);
                return 1;
            }
        }
        if(hasFlag(table.printSet, 1LU << index)) {
            ulimitOps[index].print(limOpt, maxNameLen);
        }
    }
    return 0;
}

enum class PrintMaskOp : unsigned int {
    ONLY_PRINT = 1 << 0,
    REUSE      = 1 << 1,
    SYMBOLIC   = 1 << 2,
};

template <> struct allow_enum_bitop<PrintMaskOp> : std::true_type {};

static void printMask(mode_t mask, PrintMaskOp op) {
    if(hasFlag(op, PrintMaskOp::SYMBOLIC)) {
        char buf[arraySize("u=rwx,g=rwx,o=rwx")];
        char *ptr = buf;

        /**
         *  u   g   o
         * rwx|rwx|rwx
         * 111 111 111
         *
         */
        for(auto &user : {'u', 'g', 'o'}) {
            if(ptr != buf) {
                *(ptr++) = ',';
            }
            *(ptr++) = user;
            *(ptr++) = '=';
            for(auto &perm : {'r', 'w', 'x'}) {
                if(!(mask & 0400)) {
                    *(ptr++) = perm;
                }
                mask <<= 1;
            }
        }
        *ptr = '\0';
        fprintf(stdout, "%s%s\n", hasFlag(op, PrintMaskOp::REUSE) ? "umask -S " : "", buf);
    } else if(hasFlag(op, PrintMaskOp::ONLY_PRINT)) {
        fprintf(stdout, "%s%04o\n", hasFlag(op, PrintMaskOp::REUSE) ? "umask " : "", mask);
    }
}

/**
 * MODE = [ugoa]* [+-=] [rwx]*
 *
 * @param value
 * @param mode
 * @return
 * if failed, return false
 */
static bool parseMode(const char *&value, mode_t &mode) {
    // [ugoa]*
    mode_t user = 0;
    for(bool next = true; next; ) {
        int ch = *(value++);
        switch(ch) {
        case 'u':
            user |= 0700;
            break;
        case 'g':
            user |= 0070;
            break;
        case 'o':
            user |= 0007;
            break;
        case 'a':
            user |= 0777;
            break;
        default:    // may be [-+=]
            next = false;
            --value;
            break;
        }
    }
    if(user == 0) {
        user = 0777;
    }

    // [-+=]
    char op = *(value++);
    if(op != '-' && op != '+' && op != '=') {
        return false;
    }

    // [rwx]*
    mode_t newMode = 0;
    while(*value && *value != ',') {
        int ch = *(value++);
        switch(ch) {
        case 'r':
            newMode |= 0444 & user;
            break;
        case 'w':
            newMode |= 0222 & user;
            break;
        case 'x':
            newMode |= 0111 & user;
            break;
        default:
            return false;
        }
    }

    // apply op
    if(op == '+') {
        unsetFlag(mode, newMode);
    } else if(op == '-') {
        setFlag(mode, newMode);
    } else {
        setFlag(mode, user);
        unsetFlag(mode, newMode);
    }
    return true;
}

struct SymbolicParseResult {
    bool success;
    char invalid;
    mode_t mode;
};

/**
 * MODES = MODE (, MODE)*
 *
 * @param value
 * @param mode
 * @return
 */
static SymbolicParseResult parseSymbolicMode(const char *value, mode_t mode) {
    SymbolicParseResult ret {
        .success = true,
        .invalid = 0,
        .mode = mode,
    };

    if(!parseMode(value, ret.mode)) {
        ret.success = false;
        ret.invalid = *(--value);
        return ret;
    }
    while(*value) {
        if(*(value++) == ',' && parseMode(value, ret.mode)) {
            continue;
        }
        ret.success = false;
        ret.invalid = *(--value);
        break;
    }
    return ret;
}

static int builtin_umask(DSState &, ArrayObject &argvObj) {
    auto op = PrintMaskOp::ONLY_PRINT;

    GetOptState optState;
    for(int opt; (opt = optState(argvObj, "pS")) != -1; ) {
        switch(opt) {
        case 'p':
            setFlag(op, PrintMaskOp::REUSE);
            break;
        case 'S':
            setFlag(op, PrintMaskOp::SYMBOLIC);
            break;
        default:
            return invalidOptionError(argvObj, optState);
        }
    }

    auto mask = umask(0);
    umask(mask);

    if(optState.index < argvObj.getValues().size()) {
        unsetFlag(op, PrintMaskOp::ONLY_PRINT | PrintMaskOp::REUSE);
        const char *value = str(argvObj.getValues()[optState.index]);
        if(isDecimal(*value)) {
            auto pair = convertToNum<int32_t>(value, 8);
            int num = pair.first;
            if(!pair.second || num < 0 || num > 0777) {
                ERROR(argvObj, "%s: octal number out of range (0000~0777)", value);
                return 1;
            }
            mask = num;
        } else {
            auto ret = parseSymbolicMode(value, mask);
            mask = ret.mode;
            if(!ret.success) {
                int ch = ret.invalid;
                if(isascii(ch) && ch != 0) {
                    ERROR(argvObj, "%c: invalid symbolic operator", ch);
                } else {
                    ERROR(argvObj, "0x%02x: invalid symbolic operator", ch);
                }
                return 1;
            }
        }
        umask(mask);
    }
    printMask(mask, op);
    return 0;
}

static int printBacktrace(const VMState &state) {
    auto traces = state.createStackTrace();
    for(auto &s : traces) {
        fprintf(stdout, "from %s:%d '%s()'\n",
                s.getSourceName().c_str(), s.getLineNum(), s.getCallerName().c_str());
    }
    return 0;
}

static int checkSourced(const VMState &state) {
    bool sourced = false;
    auto *curCode = state.getFrame().code;
    auto *initCode = state.getFrames().size() == 1 ? curCode : state.getFrames()[1].code;
    if(!initCode->is(CodeKind::NATIVE)) {
        assert(!curCode->is(CodeKind::NATIVE));
        const char *initPath = static_cast<const CompiledCode *>(initCode)->getSourceName();
        const char *curPath = static_cast<const CompiledCode *>(curCode)->getSourceName();
        sourced = strcmp(initPath, curPath) != 0;
    }
    return sourced ? 0 : 1;
}

static int printFuncName(const VMState &state) {
    auto *code = state.getFrame().code;
    const char *name = nullptr;
    if(!code->is(CodeKind::NATIVE) && !code->is(CodeKind::TOPLEVEL)) {
        name = static_cast<const CompiledCode *>(code)->getName();
    }
    fprintf(stdout, "%s\n", name != nullptr ? name : "<toplevel>");
    return name != nullptr ? 0 : 1;
}

static constexpr struct {
    RuntimeOption option;
    const char *name;
} runtimeOptions[] = {
#define GEN_OPT(E, V, N) {RuntimeOption::E, N},
        EACH_RUNTIME_OPTION(GEN_OPT)
#undef GEN_OPT
};

static RuntimeOption lookupRuntimeOption(const char *name) {
    for(auto &e : runtimeOptions) {
        if(strcmp(name, e.name) == 0) {
            return e.option;
        }
    }
    return RuntimeOption{};
}

static unsigned int computeMaxOptionNameSize() {
    unsigned int maxSize = 0;
    for(auto &e : runtimeOptions) {
        unsigned int size = strlen(e.name) + 2;
        if(size > maxSize) {
            maxSize = size;
        }
    }
    return maxSize;
}

static void printRuntimeOpt(const char *name, unsigned int size, bool set) {
    std::string value = name;
    value.append(size - strlen(name), ' ');
    value += (set ? "on" : "off");
    value += "\n";
    fputs(value.c_str(), stdout);
}

static int showOption(const DSState &state, const ArrayObject &argvObj) {
    const unsigned int size = argvObj.size();
    RuntimeOption foundSet{};
    if(size == 2) {
        foundSet = static_cast<RuntimeOption>(static_cast<unsigned int>(-1));
    } else {
        for(unsigned int i = 2; i < size; i++) {
            const char *name = str(argvObj.getValues()[i]);
            auto option = lookupRuntimeOption(name);
            if(empty(option)) {
                ERROR(argvObj, "undefined runtime option: %s", name);
                return 1;
            }
            setFlag(foundSet, option);
        }
    }

    // print
    const unsigned int maxNameSize = computeMaxOptionNameSize();
    for(auto &e : runtimeOptions) {
        if(hasFlag(foundSet, e.option)) {
            printRuntimeOpt(e.name, maxNameSize, hasFlag(state.runtimeOption, e.option));
        }
    }
    return 0;
}

static int setOption(DSState &state, const ArrayObject &argvObj, const bool set) {
    const unsigned int size = argvObj.size();
    if(size == 2) {
        ERROR(argvObj, "`%s' subcommand requires argument", set ? "set" : "unset");
        return 2;
    }

    bool foundMonitor = false;
    for(unsigned int i = 2; i < size; i++) {
        const char *name = str(argvObj.getValues()[i]);
        auto option = lookupRuntimeOption(name);
        if(empty(option)) {
            ERROR(argvObj, "undefined runtime option: %s", name);
            return 1;
        }
        if(option == RuntimeOption::MONITOR && !foundMonitor) {
            foundMonitor = true;
            setJobControlSignalSetting(state, set);
        }
        if(set) {
            setFlag(state.runtimeOption, option);
        } else {
            unsetFlag(state.runtimeOption, option);
        }
    }
    return 0;
}

static int builtin_shctl(DSState &state, ArrayObject &argvObj) {
    if(argvObj.size() > 1) {
        auto ref = argvObj.getValues()[1].asStrRef();
        if(ref == "backtrace") {
            return printBacktrace(state.getCallStack());
        } else if(ref == "is-sourced") {
            return checkSourced(state.getCallStack());
        } else if(ref == "is-interactive") {
            return hasFlag(state.compileOption, CompileOption::INTERACTIVE) ? 0 : 1;
        } else if(ref == "function") {
            return printFuncName(state.getCallStack());
        } else if(ref == "show") {
            return showOption(state, argvObj);
        } else if(ref == "set") {
            return setOption(state, argvObj, true);
        } else if(ref == "unset") {
            return setOption(state, argvObj, false);
        } else {
            ERROR(argvObj, "undefined subcommand: %s", ref.data());
            return 2;
        }
    }
    return 0;
}

} // namespace ydsh
