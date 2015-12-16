#include <gtest/gtest.h>

#include <cstring>

#include <misc/unicode.hpp>

using namespace ydsh::misc;

class UnicodeTest : public ::testing::Test {
public:
    UnicodeTest() = default;
    virtual ~UnicodeTest() = default;

    virtual void SetUp() {
    }

    virtual void TearDown() {
    }

    void toCodePoint(const char *str, int &codePoint) {
        SCOPED_TRACE("");

        ASSERT_TRUE(str != nullptr);

        int code = 0;
        unsigned int len = strlen(str);

        unsigned int s = UnicodeUtil::utf8ToCodePoint(str, len == 0 ? 1 : len, code);
        ASSERT_NE(0u, s);
        ASSERT_NE(-1, code);

        codePoint = code;
    }

    void assertByteSize(const unsigned int size, const char *str) {
        SCOPED_TRACE("");

        ASSERT_TRUE(str != nullptr);

        unsigned int s = strlen(str);
        ASSERT_EQ(size, s == 0 ? 1 : s);

        ASSERT_EQ(size, UnicodeUtil::utf8ByteSize(str[0]));
    }

    void assertWidth(const int width, const char *str, bool ambiguousWidth2 = false) {
        SCOPED_TRACE("");

        int codePoint = 0;
        this->toCodePoint(str, codePoint);

        auto e = ambiguousWidth2 ? UnicodeUtil::TWO_WIDTH : UnicodeUtil::ONE_WIDTH;
        ASSERT_EQ(width, UnicodeUtil::width(codePoint, e));
    }
};

TEST_F(UnicodeTest, size) {
    SCOPED_TRACE("");

    ASSERT_NO_FATAL_FAILURE(this->assertByteSize(1, ""));
    ASSERT_NO_FATAL_FAILURE(this->assertByteSize(1, "1"));
    ASSERT_NO_FATAL_FAILURE(this->assertByteSize(1, "a"));
    ASSERT_NO_FATAL_FAILURE(this->assertByteSize(1, "\n"));
    ASSERT_NO_FATAL_FAILURE(this->assertByteSize(1, "\t"));
    ASSERT_NO_FATAL_FAILURE(this->assertByteSize(2, "å"));
    ASSERT_NO_FATAL_FAILURE(this->assertByteSize(2, "¶"));
    ASSERT_NO_FATAL_FAILURE(this->assertByteSize(2, "Ω"));
    ASSERT_NO_FATAL_FAILURE(this->assertByteSize(3, "あ"));
    ASSERT_NO_FATAL_FAILURE(this->assertByteSize(3, "解"));
    ASSERT_NO_FATAL_FAILURE(this->assertByteSize(3, "墨"));
    ASSERT_NO_FATAL_FAILURE(this->assertByteSize(3, "ｱ"));
    ASSERT_NO_FATAL_FAILURE(this->assertByteSize(4, "𪗱"));
    ASSERT_NO_FATAL_FAILURE(this->assertByteSize(4, "𣏤"));
    ASSERT_NO_FATAL_FAILURE(this->assertByteSize(4, "𣴀"));
}

TEST_F(UnicodeTest, base) {
    SCOPED_TRACE("");

    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(0, UnicodeUtil::width(0)));
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(-1, UnicodeUtil::width('\n')));
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(-1, UnicodeUtil::width('\t')));
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(1, UnicodeUtil::width('1')));
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(1, UnicodeUtil::width(' ')));
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(1, UnicodeUtil::width('\\')));
    ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(1, UnicodeUtil::width('~')));
}

TEST_F(UnicodeTest, multi) {
    SCOPED_TRACE("");

    ASSERT_NO_FATAL_FAILURE(this->assertWidth(-1, "\n"));
    ASSERT_NO_FATAL_FAILURE(this->assertWidth(-1, "\n", true));
    ASSERT_NO_FATAL_FAILURE(this->assertWidth(-1, "\r"));
    ASSERT_NO_FATAL_FAILURE(this->assertWidth(-1, "\r", true));
    ASSERT_NO_FATAL_FAILURE(this->assertWidth(0, ""));
    ASSERT_NO_FATAL_FAILURE(this->assertWidth(0, "", true));
    ASSERT_NO_FATAL_FAILURE(this->assertWidth(1, "a"));
    ASSERT_NO_FATAL_FAILURE(this->assertWidth(1, "a", true));
    ASSERT_NO_FATAL_FAILURE(this->assertWidth(1, "å"));
    ASSERT_NO_FATAL_FAILURE(this->assertWidth(1, "å", true));
    ASSERT_NO_FATAL_FAILURE(this->assertWidth(1, "◯"));
    ASSERT_NO_FATAL_FAILURE(this->assertWidth(2, "◯", true));
    ASSERT_NO_FATAL_FAILURE(this->assertWidth(1, "■"));
    ASSERT_NO_FATAL_FAILURE(this->assertWidth(2, "■", true));
    ASSERT_NO_FATAL_FAILURE(this->assertWidth(2, "ま"));
    ASSERT_NO_FATAL_FAILURE(this->assertWidth(2, "ま", true));

    unsigned char b[] = {0xCC, 0x88, 0};   // combining character
    ASSERT_NO_FATAL_FAILURE(this->assertWidth(0, (char *)b));
    ASSERT_NO_FATAL_FAILURE(this->assertWidth(0, (char *)b, true));

    ASSERT_NO_FATAL_FAILURE(this->assertWidth(1, "ｱ"));
    ASSERT_NO_FATAL_FAILURE(this->assertWidth(1, "ｱ", true));
}



int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}



