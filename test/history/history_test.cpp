#include "gtest/gtest.h"

#include <fstream>

#include <ydsh/ydsh.h>
#include <constant.h>
#include <vm.h>

#include "../test_common.h"


class HistoryTest : public ExpectOutput, public TempFileFactory {
protected:
    DSState *state{nullptr};
    bool evaled{false};

public:
    HistoryTest() {
        this->state = DSState_create();
        std::string value;
        value += '"';
        value += this->tmpFileName;
        value += '"';
        this->assignValue(VAR_HISTFILE, std::move(value));
    }

    ~HistoryTest() override {
        DSState_delete(&this->state);
    }

    void setHistSize(unsigned int size, bool sync = true) {
        this->assignUintValue(VAR_HISTSIZE, size);
        if(sync) {
            DSState_syncHistorySize(this->state);
        }
    }

    void setHistFileSize(unsigned int size) {
        this->assignUintValue(VAR_HISTFILESIZE, size);
    }

    Output evalInChild(const std::string &code) {
        IOConfig config{IOConfig::INHERIT, IOConfig::INHERIT, IOConfig::INHERIT};
        return ProcBuilder::spawn(config, [&] {
            int ret = DSState_eval(this->state, nullptr, code.c_str(), code.size(), nullptr);
            return ret;
        }).waitAndGetResult(true);
    }

    void eval(const char *code) {
        if(!this->evaled) {
            this->evaled = true;
            const char *func = R"EOF(
            function assertEach($expect : [String], $actual : [String]) {
                assert $expect.size() == $actual.size() : "size: ${$expect.size()} != ${$actual.size()}"
                let size = $expect.size()
                for(var i = 0; $i < $size; $i++) {
                    assert $expect[$i] == $actual[$i] : "expect[$i] = ${$expect[$i]}, actual[$i] = ${$actual[$i]}"
                }
            }
            )EOF";
            DSError e;
            DSState_eval(this->state, "(builtin)", func, strlen(func), &e);
            auto kind = e.kind;
            DSError_release(&e);
            ASSERT_EQ(DS_ERROR_KIND_SUCCESS, kind);
        }
        std::string c(code);
        c += "\n";
        c += "exit $?";

        auto result = this->evalInChild(c);
        ExpectOutput::expect(result, 0, WaitStatus::EXITED);
    }

private:
    void assignValue(const char *varName, std::string &&value) {
        std::string str = "$";
        str += varName;
        str += " = ";
        str += value;

        auto r = DSState_eval(this->state, "(dummy)", str.c_str(), str.size(), nullptr);
        SCOPED_TRACE("");
        ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(r == 0));
    }

    void assignUintValue(const char *varName, unsigned int value) {
        std::string str = std::to_string(value);
        str += "u";
        this->assignValue(varName, std::move(str));
    }
};

TEST_F(HistoryTest, base) {
    SCOPED_TRACE("");
    auto *history = DSState_history(this->state);

    // default size
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(0u, DSHistory_size(history)));

    // after synchronization (call DSState_syncHistory)
    this->setHistSize(100, false);
    DSState_syncHistorySize(this->state);
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(0u, DSHistory_size(history)));
}

TEST_F(HistoryTest, add) {
    this->setHistSize(2);
    DSState_addHistory(this->state, "aaa");
    DSState_addHistory(this->state, "bbb");

    auto *history = DSState_history(this->state);

    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(2u, DSHistory_size(history)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ("aaa", DSHistory_get(history, 0)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ("bbb", DSHistory_get(history, 1)));

    DSState_addHistory(this->state, "ccc");
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(2u, DSHistory_size(history)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ("bbb", DSHistory_get(history, 0)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ("ccc", DSHistory_get(history, 1)));

    DSState_addHistory(this->state, "ccc");
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(2u, DSHistory_size(history)));
}


TEST_F(HistoryTest, set) {
    SCOPED_TRACE("");

    this->setHistSize(10);
    DSState_addHistory(this->state, "aaa");
    DSState_addHistory(this->state, "bbb");

    auto *history = DSState_history(this->state);
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(2u, DSHistory_size(history)));

    DSHistory_set(history, 1, "ccc");
    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ("ccc", DSHistory_get(history, 1)));

    DSHistory_set(history, 3, "ccc");    // do nothing, if out of range
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(2u, DSHistory_size(history)));

    DSHistory_set(history, 1000, "ccc");    // do nothing, if out of range
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(2u, DSHistory_size(history)));
}

TEST_F(HistoryTest, remove) {
    SCOPED_TRACE("");

    this->setHistSize(10);
    DSState_addHistory(this->state, "aaa");
    DSState_addHistory(this->state, "bbb");
    DSState_addHistory(this->state, "ccc");
    DSState_addHistory(this->state, "ddd");
    DSState_addHistory(this->state, "eee");

    auto *history = DSState_history(this->state);
    DSHistory_delete(history, 2);
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(4u, DSHistory_size(history)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ("aaa", DSHistory_get(history, 0)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ("bbb", DSHistory_get(history, 1)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ("ddd", DSHistory_get(history, 2)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ("eee", DSHistory_get(history, 3)));

    DSHistory_delete(history, 3);
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(3u, DSHistory_size(history)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ("aaa", DSHistory_get(history, 0)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ("bbb", DSHistory_get(history, 1)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ("ddd", DSHistory_get(history, 2)));

    // do nothing, if out of range
    DSHistory_delete(history, 6);
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(3u, DSHistory_size(history)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ("aaa", DSHistory_get(history, 0)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ("bbb", DSHistory_get(history, 1)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ("ddd", DSHistory_get(history, 2)));

    // do nothing, if out of range
    DSHistory_delete(history, 600);
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(3u, DSHistory_size(history)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ("aaa", DSHistory_get(history, 0)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ("bbb", DSHistory_get(history, 1)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ("ddd", DSHistory_get(history, 2)));
}

TEST_F(HistoryTest, clear) {
    SCOPED_TRACE("");

    this->setHistSize(10);
    DSState_addHistory(this->state, "aaa");
    DSState_addHistory(this->state, "bbb");
    DSState_addHistory(this->state, "ccc");
    DSState_addHistory(this->state, "ddd");
    DSState_addHistory(this->state, "eee");

    auto *history = DSState_history(this->state);
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(5u, DSHistory_size(history)));

    DSState_clearHistory(this->state);
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(0u, DSHistory_size(history)));
}

TEST_F(HistoryTest, resize) {
    this->setHistSize(10);
    for(unsigned int i = 0; i < 10; i++) {
        DSState_addHistory(this->state, std::to_string(i).c_str());
    }

    auto *history = DSState_history(this->state);
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(10u, DSHistory_size(history)));

    this->setHistSize(5);
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(5u, DSHistory_size(history)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ("0", DSHistory_get(history, 0)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ("1", DSHistory_get(history, 1)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ("2", DSHistory_get(history, 2)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ("3", DSHistory_get(history, 3)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ("4", DSHistory_get(history, 4)));
}

TEST_F(HistoryTest, file) {
    this->setHistSize(10);
    for(unsigned int i = 0; i < 10; i++) {
        DSState_addHistory(this->state, std::to_string(i).c_str());
    }

    auto *history = DSState_history(this->state);
    this->setHistFileSize(15);
    /**
     * 0, 1, 2, 3, 4, 5, 6, 7, 8, 9
     */
    DSState_saveHistory(this->state, nullptr);
    DSState_clearHistory(this->state);
    DSState_loadHistory(this->state, nullptr);
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(10u, DSHistory_size(history)));

    for(unsigned int i = 0; i < 10; i++) {
        ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ(std::to_string(i).c_str(), DSHistory_get(history, i)));
    }

    for(unsigned int i = 0; i < 5; i++) {
        DSState_addHistory(this->state, std::to_string(i + 10).c_str());
    }

    for(unsigned int i = 0; i < 10; i++) {
        ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ(std::to_string(i + 5).c_str(), DSHistory_get(history, i)));
    }

    /**
     * previous hist file content
     * 0, 1, 2, 3, 4, 5, 6, 7, 8, 9
     *
     * newly added content
     * 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
     *
     * current hist file content
     * 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
     */
    DSState_saveHistory(this->state, nullptr);
    DSState_clearHistory(this->state);
    this->setHistSize(15);
    DSState_loadHistory(this->state, nullptr);
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(10u, DSHistory_size(history)));
    for(unsigned int i = 0; i < 10; i++) {
        ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ(std::to_string(i + 5).c_str(), DSHistory_get(history, i)));
    }

    // not overwrite history file when buffer size is 0
    DSState_clearHistory(this->state);
    DSState_saveHistory(this->state, nullptr);
    DSState_loadHistory(this->state, nullptr);
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(10u, DSHistory_size(history)));
    for(unsigned int i = 0; i < 10; i++) {
        ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ(std::to_string(i + 5).c_str(), DSHistory_get(history, i)));
    }

    // not overwrite history file when hist file size is 0
    this->setHistFileSize(0);
    DSState_clearHistory(this->state);
    DSState_addHistory(this->state, "hoge");
    DSState_saveHistory(this->state, nullptr);
    DSState_loadHistory(this->state, nullptr);
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(11u, DSHistory_size(history)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ("hoge", DSHistory_get(history, 0)));
    for(unsigned int i = 1; i < 11; i++) {
        ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ(std::to_string(i + 4).c_str(), DSHistory_get(history, i)));
    }
}

TEST_F(HistoryTest, file2) {
    this->setHistFileSize(DS_HISTFILESIZE_LIMIT + 10);
    this->setHistSize(DS_HISTFILESIZE_LIMIT);

    for(unsigned int i = 0; i < DS_HISTFILESIZE_LIMIT; i++) {
        DSState_addHistory(this->state, std::to_string(i).c_str());
    }
    
    auto *history = DSState_history(this->state);
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(DS_HISTFILESIZE_LIMIT, DSHistory_size(history)));

    DSState_saveHistory(this->state, nullptr);
    DSState_clearHistory(this->state);
    DSState_loadHistory(this->state, nullptr);

    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(DS_HISTFILESIZE_LIMIT, DSHistory_size(history)));
    for(unsigned int i = 0; i < DS_HISTFILESIZE_LIMIT; i++) {
        ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ(std::to_string(i).c_str(), DSHistory_get(history, i)));
    }
}

TEST_F(HistoryTest, cmd_invalid) {
    const char *code = R"EOF(
            history -x
            assert $? == 2
            assert "$(history -xz 2>&1)" == "ydsh: history: -xz: invalid option
history: history [-c] [-d offset] or history [-rw] [file]"
            true
    )EOF";
    ASSERT_NO_FATAL_FAILURE(ASSERT_(this->eval(code)));
}

TEST_F(HistoryTest, cmd_print) {
    this->setHistSize(10);

    // emtpy history
    const char *code = R"EOF(
            $assertEach(new [String](), $(history))
    )EOF";
    ASSERT_NO_FATAL_FAILURE(ASSERT_(this->eval(code)));

    // print all
    DSState_addHistory(this->state, "a");
    DSState_addHistory(this->state, "b");
    DSState_addHistory(this->state, "c");
    DSState_addHistory(this->state, "d");
    DSState_addHistory(this->state, "e");

    code = R"EOF(
        $IFS = $'\n'
        $assertEach(["    1  a",
                     "    2  b",
                     "    3  c",
                     "    4  d",
                     "    5  e"], $(history))
    )EOF";
    ASSERT_NO_FATAL_FAILURE(ASSERT_(this->eval(code)));

    // print latest entry
    DSState_addHistory(this->state, "a");
    DSState_addHistory(this->state, "b");
    DSState_addHistory(this->state, "c");
    DSState_addHistory(this->state, "d");
    DSState_addHistory(this->state, "e");

    code = R"EOF(
        $IFS = $'\n'
        $assertEach(["    6  a",
                     "    7  b",
                     "    8  c",
                     "    9  d",
                     "   10  e"], $(history 5))
    )EOF";
    ASSERT_NO_FATAL_FAILURE(ASSERT_(this->eval(code)));

    // out of range
    code = R"EOF(
        $IFS = $'\n'
        $assertEach(["    1  a",
                     "    2  b",
                     "    3  c",
                     "    4  d",
                     "    5  e",
                     "    6  a",
                     "    7  b",
                     "    8  c",
                     "    9  d",
                     "   10  e"], $(history 500))
    )EOF";
    ASSERT_NO_FATAL_FAILURE(ASSERT_(this->eval(code)));

    // invalid number
    code = R"EOF(
        history hoge
        assert $? == 1
        assert "$(history hoge 2>&1)" == "ydsh: history: hoge: numeric argument required"
        true
    )EOF";
    ASSERT_NO_FATAL_FAILURE(ASSERT_(this->eval(code)));

    // too many arg
    code = R"EOF(
        history hoge
        assert $? == 1
        assert "$(history hoge 2 2>&1)" == "ydsh: history: too many arguments"
        true
    )EOF";
    ASSERT_NO_FATAL_FAILURE(ASSERT_(this->eval(code)));
}

TEST_F(HistoryTest, cmd_delete) {
    this->setHistSize(10);

    DSState_addHistory(this->state, "a");
    DSState_addHistory(this->state, "b");
    DSState_addHistory(this->state, "c");
    DSState_addHistory(this->state, "d");
    DSState_addHistory(this->state, "e");
    DSState_addHistory(this->state, "f");
    DSState_addHistory(this->state, "g");
    DSState_addHistory(this->state, "h");
    DSState_addHistory(this->state, "i");
    DSState_addHistory(this->state, "j");

    // clear
    const char *code = R"EOF(
            history -c
            $assertEach(new [String](), $(history))
    )EOF";
    ASSERT_NO_FATAL_FAILURE(ASSERT_(this->eval(code)));

    // delete history
    code = R"EOF(
        history -d 3 -d 5
        $IFS = $'\n'
        $assertEach(["    1  a",
                     "    2  b",
                     "    3  c",
                     "    4  d",
                     "    5  f",
                     "    6  g",
                     "    7  h",
                     "    8  i",
                     "    9  j"], $(history))
    )EOF";
    ASSERT_NO_FATAL_FAILURE(ASSERT_(this->eval(code)));

    // delete history, but missing number
    code = R"EOF(
        history -d 3 -d
        assert $? == 2
        assert "$(history -d 2>&1)" == "ydsh: history: -d: option requires argument"
        true
    )EOF";
    ASSERT_NO_FATAL_FAILURE(ASSERT_(this->eval(code)));

    // invalid number
    code = R"EOF(
        history -d hoge
        assert $? == 1
        assert "$(history -d hoge 2>&1)" == "ydsh: history: hoge: history offset out of range"
        true
    )EOF";
    ASSERT_NO_FATAL_FAILURE(ASSERT_(this->eval(code)));

    // out of range
    code = R"EOF(
        history -d 999999
        assert $? == 1
        assert "$(history -d 999999 2>&1)" == "ydsh: history: 999999: history offset out of range"
        true
    )EOF";
    ASSERT_NO_FATAL_FAILURE(ASSERT_(this->eval(code)));

    // out of range
    code = R"EOF(
        history -d 0
        assert $? == 1
        assert "$(history -d 0 2>&1)" == "ydsh: history: 0: history offset out of range"
        true
    )EOF";
    ASSERT_NO_FATAL_FAILURE(ASSERT_(this->eval(code)));
}

TEST_F(HistoryTest, cmd_load) {
    this->setHistSize(10);

    FILE *fp = fopen(this->getTempFileName(), "w");
    ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(fp != nullptr));

    const char *v[] = {
            "a", "b", "c", "d", "e"
    };
    for(auto &e : v) {
        fprintf(fp, "%s\n", e);
    }
    fclose(fp);

    // load
    const char *code = R"EOF(
            history -r
            $IFS = $'\n'
            $assertEach(["    1  a",
                         "    2  b",
                         "    3  c",
                         "    4  d",
                         "    5  e"], $(history))
    )EOF";
    ASSERT_NO_FATAL_FAILURE(ASSERT_(this->eval(code)));

    // load invalid file
    code = R"EOF(
            history -r hfurehfurefewafzxc
            $IFS = $'\n'
            $assertEach(new [String](), $(history))
    )EOF";
    ASSERT_NO_FATAL_FAILURE(ASSERT_(this->eval(code)));

    // load specified file
    code = R"EOF(
            $IFS = $'\n'
            $assertEach(["    1  a",
                         "    2  b",
                         "    3  c",
                         "    4  d",
                         "    5  e"], $(history))
    )EOF";
    std::string c = "history -r ";
    c += this->getTempFileName();
    c += "\n";
    c += code;
    ASSERT_NO_FATAL_FAILURE(ASSERT_(this->eval(c.c_str())));
}

static std::vector<std::string> readFile(const std::string &fileName) {
    std::vector<std::string> v;
    std::ifstream input(fileName);
    for(std::string line; std::getline(input, line);) {
        v.push_back(std::move(line));
    }
    return v;
}

TEST_F(HistoryTest, cmd_save) {
    this->setHistSize(10);

    const char *v[] = {
            "a", "b", "c", "d", "e"
    };

    for(auto &e : v) {
        DSState_addHistory(this->state, e);
    }

    // save to specified file
    std::string code = "history -w ";
    code += this->getTempFileName();
    ASSERT_NO_FATAL_FAILURE(ASSERT_(this->eval(code.c_str())));
    auto hist = readFile(this->getTempFileName());
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(arraySize(v), hist.size()));
    for(unsigned int i = 0; i < hist.size(); i++) {
        ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ(v[i], hist[i].c_str()));
    }

    // save to file
    for(auto &e : v) {
        DSState_addHistory(this->state, e);
    }

    ASSERT_NO_FATAL_FAILURE(ASSERT_(this->eval("history -w")));
    hist = readFile(this->getTempFileName());
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(arraySize(v) * 2, hist.size()));
    for(unsigned int i = 0; i < hist.size(); i++) {
        unsigned int index = i % 5;
        ASSERT_NO_FATAL_FAILURE(ASSERT_STREQ(v[index], hist[index].c_str()));
    }

    // save and load
    const char *c = R"EOF(
            history -c
            history -w -r
            assert $? == 1
            assert "$(history -w -r 2>&1)" == "ydsh: history: cannot use more than one of -rw"
            true
    )EOF";
    ASSERT_NO_FATAL_FAILURE(ASSERT_(this->eval(c)));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}