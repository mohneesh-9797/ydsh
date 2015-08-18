#include <gtest/gtest.h>

#include <misc/buffer.hpp>

using namespace ydsh::misc;

typedef FlexBuffer<unsigned int> IBuffer;

TEST(BufferTest, case1) {
    IBuffer buffer;

    ASSERT_NO_FATAL_FAILURE({
        ASSERT_EQ(0, buffer.size());
        ASSERT_EQ(0, buffer.capacity());
    });

    // append
    for(unsigned int i = 0; i < IBuffer::MINIMUM_CAPACITY; i++) {
        buffer += i;
    }

    ASSERT_NO_FATAL_FAILURE({
        ASSERT_EQ(IBuffer::MINIMUM_CAPACITY, buffer.size());
        ASSERT_EQ(IBuffer::MINIMUM_CAPACITY, buffer.capacity());
    });

    // get
    for(unsigned int i = 0; i < IBuffer::MINIMUM_CAPACITY; i++) {
        ASSERT_NO_FATAL_FAILURE({
            ASSERT_EQ(i, buffer[i]);
            ASSERT_EQ(i, buffer.at(i));
        });
    }

    // set
    ASSERT_NO_FATAL_FAILURE({
        buffer[5] = 90;
        ASSERT_EQ(90, buffer[5]);
        ASSERT_EQ(3, ++buffer[2]);
        ASSERT_EQ(0, buffer[0]++);
        ASSERT_EQ(1, buffer[0]);
    });

    // out of range
    bool raied = false;
    try {
        buffer.at(buffer.size() + 2) = 999;
    } catch(const std::out_of_range &e) {
        raied = true;
        std::string msg("size is: ");
        msg += std::to_string(buffer.size());
        msg += ", but index is: ";
        msg += std::to_string(buffer.size() + 2);
        ASSERT_NO_FATAL_FAILURE({ASSERT_STREQ(msg.c_str(), e.what());});
    }
    ASSERT_NO_FATAL_FAILURE({ASSERT_TRUE(raied);});
}

TEST(BufferTest, case2) {
    IBuffer buffer;

    unsigned int size = IBuffer::MINIMUM_CAPACITY + 2;
    for(unsigned int i = 0; i < size; i++) {
        buffer += i;    // expand buffer.
    }

    unsigned int cap = IBuffer::MINIMUM_CAPACITY + (IBuffer::MINIMUM_CAPACITY >> 1);
    ASSERT_NO_FATAL_FAILURE({
        ASSERT_EQ(size, buffer.size());
        ASSERT_EQ(cap, buffer.capacity());
    });

    for(unsigned int i = 0; i < size; i++)  {
        ASSERT_NO_FATAL_FAILURE({ASSERT_EQ(i, buffer[i]);});
    }

    ASSERT_NO_FATAL_FAILURE({
        buffer.clear();
        ASSERT_EQ(0, buffer.size());
        ASSERT_EQ(cap, buffer.capacity());
    });

    ASSERT_NO_FATAL_FAILURE({   // remove
        delete[] buffer.remove();
        ASSERT_EQ(0, buffer.size());
        ASSERT_EQ(0, buffer.capacity());
        ASSERT_TRUE(buffer.get() == nullptr);
    });

    unsigned int v[] = {10, 20, 30};
    ASSERT_NO_FATAL_FAILURE({   // reuse
        buffer.append(v, sizeof(v) / sizeof(unsigned int));
        ASSERT_EQ(3, buffer.size());
        ASSERT_EQ(IBuffer::MINIMUM_CAPACITY, buffer.capacity());
        ASSERT_EQ(v[0], buffer[0]);
        ASSERT_EQ(v[1], buffer[1]);
        ASSERT_EQ(v[2], buffer[2]);
    });
}

TEST(BufferTest, case3) {
    ByteBuffer buffer;
    const char *s = "hello world!!";
    unsigned int len = strlen(s);
    buffer.append(s, strlen(s));
    unsigned int cap = buffer.capacity();

    for(unsigned int i = 0; i < len; i++) {
        ASSERT_NO_FATAL_FAILURE(ASSERT_EQ(s[i], buffer[i]));
    }

    ByteBuffer buffer2(std::move(buffer));

    ASSERT_NO_FATAL_FAILURE({   // after move
        ASSERT_EQ(0, buffer.size());
        ASSERT_EQ(0, buffer.capacity());
        ASSERT_TRUE(buffer.get() == nullptr);

        ASSERT_EQ(len, buffer2.size());
        ASSERT_EQ(cap, buffer2.capacity());
        ASSERT_TRUE(memcmp(s, buffer2.get(), buffer2.size()) == 0);
    });

    buffer2 += buffer;
    ASSERT_NO_FATAL_FAILURE({   // do nothing
        ASSERT_EQ(len, buffer2.size());
        ASSERT_EQ(cap, buffer2.capacity());
    });

    ByteBuffer buffer3;
    buffer3 += buffer2;
    buffer3 += ' ';
    buffer3 += buffer2;
    buffer3 += '\0';

    ASSERT_NO_FATAL_FAILURE({
        ASSERT_TRUE(memcmp(s, buffer2.get(), buffer2.size()) == 0);
        ASSERT_STREQ("hello world!! hello world!!", buffer3.get());
    });
}

typedef __detail_flex_buffer::FlexBuffer<const char *, unsigned char> StrBuffer;
TEST(BufferTest, case4) {
    StrBuffer buffer;

    const char *v[] = {
            "AAA", "BBB", "CCC", "DDD", "EEE",
            "FFF", "GGG", "HHH", "III", "JJJ",
    };

    const unsigned int size = sizeof(v) / sizeof(const char *);

    bool raised = false;
    try {
        for(unsigned int i = 0; i < 30; i++) {
            buffer.append(v, size);
        }
    } catch(const std::length_error &e) {
        ASSERT_NO_FATAL_FAILURE({ASSERT_STREQ("reach maximum capacity", e.what());});
        raised = true;
    }
    ASSERT_NO_FATAL_FAILURE({ASSERT_TRUE(raised);});
}

TEST(BufferTest, case5) {
    const StrBuffer::size_type size = 254;
    StrBuffer buffer(size);

    ASSERT_NO_FATAL_FAILURE({
        ASSERT_EQ(StrBuffer::MAXIMUM_CAPACITY - 1, buffer.capacity());
    });

    const char *v[size];
    buffer.append(v, size);
}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

