/*
 * Copyright (C) 2017-2018 Nagisa Sekiguchi
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

#ifndef YDSH_TEST_COMMON_H
#define YDSH_TEST_COMMON_H

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <process.h>
#include <ansi.h>

using namespace process;

// common utility for test

class TempFileFactory {
protected:
    char *tmpDirName{nullptr};
    char *tmpFileName{nullptr};

    virtual ~TempFileFactory();

    void createTemp();

    void deleteTemp();

public:
    const char *getTempDirName() const {
        return this->tmpDirName;
    }

    const char *getTempFileName() const {
        return this->tmpFileName;
    }

    std::string createTempFile(const char *baseName, const std::string &content) const;

private:
    void freeName();
};

std::string format(const char *fmt, ...) __attribute__ ((format(printf, 1, 2)));

class Extractor {
private:
    const char *str;

public:
    explicit Extractor(const char *str) : str(str) {}

    template <typename ...Arg>
    int operator()(Arg &&... arg) {
        return this->delegate(std::forward<Arg>(arg)...);
    }

private:
    void consumeSpace();

    int extract(unsigned int &value);

    int extract(int &value);

    int extract(std::string &value);

    int extract(const char *value);

    int delegate() {
        this->consumeSpace();
        return strlen(this->str);
    }

    template <typename F, typename ...T>
    int delegate(F &&first, T &&...rest) {
        int ret = this->extract(std::forward<F>(first));
        return ret == 0 ? this->delegate(std::forward<T>(rest)...) : ret;
    }
};


#define ASSERT_(F) do { SCOPED_TRACE(""); F; } while(false)

struct ExpectOutput : public ::testing::Test {
    void expect(const Output &output, int status = 0,
                WaitStatus::Kind type = WaitStatus::EXITED,
                const char *out = "", const char *err = "") {
        SCOPED_TRACE("");

        if(out != nullptr) {
            ASSERT_EQ(out, output.out);
        }
        if(err != nullptr) {
            ASSERT_EQ(err, output.err);
        }
        ASSERT_EQ(status, output.status.value);
        ASSERT_EQ(type, output.status.kind);
    }

    void expect(ProcBuilder &&builder, int status, const char *out = "", const char *err = "") {
        SCOPED_TRACE("");

        ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(out != nullptr));
        ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(err != nullptr));

        auto result = builder.execAndGetResult(false);
        this->expect(result, status, WaitStatus::EXITED, out, err);
    }
};

class InteractiveBase : public ExpectOutput {
private:
    ProcHandle handle;

    const std::string binPath;

    const std::string workingDir;

protected:
    explicit InteractiveBase(const char *binPath, const char *dir) : binPath(binPath), workingDir(dir) {}

    template <typename ... T>
    void invoke(T && ...args) {
        termios term;
        xcfmakesane(term);
        this->handle = ProcBuilder{this->binPath.c_str(), std::forward<T>(args)...}
                .addEnv("TERM", "xterm")
                .setWorkingDir(this->workingDir.c_str())
                .setIn(IOConfig::PTY)
                .setOut(IOConfig::PTY)
                .setErr(IOConfig::PIPE)
                .setTerm(term)();
    }

    void send(const char *str) {
        int r = write(this->handle.in(), str, strlen(str));
        (void) r;
        fsync(this->handle.in());
    }

    std::pair<std::string, std::string> readAll();

    void expectRegex(const char *out = "", const char *err = "") {
        SCOPED_TRACE("");

        ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(out != nullptr));
        ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(err != nullptr));

        auto pair = this->readAll();
        ASSERT_NO_FATAL_FAILURE(ASSERT_THAT(pair.first, ::testing::MatchesRegex(out)));
        ASSERT_NO_FATAL_FAILURE(ASSERT_THAT(pair.second, ::testing::MatchesRegex(err)));
    }

    void expect(const char *out = "", const char *err = "") {
        SCOPED_TRACE("");

        ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(out != nullptr));
        ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(err != nullptr));

        auto pair = this->readAll();
        ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(out, pair.first));
        ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(err, pair.second));
    }

    void sendAndExpect(const char *str, const char *out = "", const char *err = "") {
        this->send(str);
        this->send("\r");

        std::string eout = str;
        eout += "\r\n";
        eout += out;
        this->expect(eout.c_str(), err);
    }

    void waitAndExpect(int status = 0, WaitStatus::Kind type = WaitStatus::EXITED,
                       const char *out = "", const char *err = "") {
        auto ret = this->handle.waitAndGetResult(false);
        ExpectOutput::expect(ret, status, type, out, err);
    }
};


#endif //YDSH_TEST_COMMON_H
