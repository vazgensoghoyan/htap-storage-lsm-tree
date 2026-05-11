#include <gtest/gtest.h>
#include <sstream>
#include <cstring>
#include <cstdint>

#include "utils/binary_writer.hpp"

using namespace htap::utils;

template <typename T>
static T read_le(const std::string& buf, size_t offset = 0) {
    T value;
    std::memcpy(&value, buf.data() + offset, sizeof(T));

    if constexpr (std::endian::native == std::endian::big)
        value = std::byteswap(value);

    return value;
}

TEST(BinaryWriter, WriteU8) {
    std::ostringstream oss;
    BinaryWriter w(oss);

    size_t written = w.write_u8(0xAB);

    auto data = oss.str();

    ASSERT_EQ(written, 1);
    ASSERT_EQ(static_cast<uint8_t>(data[0]), 0xAB);
}

TEST(BinaryWriter, WriteU16) {
    std::ostringstream oss;
    BinaryWriter w(oss);

    size_t written = w.write_u16(0x1234);

    auto data = oss.str();

    ASSERT_EQ(written, 2);

    uint16_t val = read_le<uint16_t>(data);
    EXPECT_EQ(val, 0x1234);
}

TEST(BinaryWriter, WriteU32) {
    std::ostringstream oss;
    BinaryWriter w(oss);

    size_t written = w.write_u32(0x11223344);

    auto data = oss.str();

    ASSERT_EQ(written, 4);

    uint32_t val = read_le<uint32_t>(data);
    EXPECT_EQ(val, 0x11223344);
}

TEST(BinaryWriter, WriteU64) {
    std::ostringstream oss;
    BinaryWriter w(oss);

    size_t written = w.write_u64(0x1122334455667788ULL);

    auto data = oss.str();

    ASSERT_EQ(written, 8);

    uint64_t val = read_le<uint64_t>(data);
    EXPECT_EQ(val, 0x1122334455667788ULL);
}

TEST(BinaryWriter, WriteI64) {
    std::ostringstream oss;
    BinaryWriter w(oss);

    int64_t input = -42;
    size_t written = w.write_i64(input);

    auto data = oss.str();

    ASSERT_EQ(written, 8);

    int64_t val = read_le<int64_t>(data);
    EXPECT_EQ(val, input);
}

TEST(BinaryWriter, WriteBytes) {
    std::ostringstream oss;
    BinaryWriter w(oss);

    const char buf[] = {1, 2, 3, 4, 5};

    size_t written = w.write_bytes(buf, sizeof(buf));

    auto data = oss.str();

    ASSERT_EQ(written, 5);
    EXPECT_EQ(data.size(), 5);
    EXPECT_EQ(std::memcmp(data.data(), buf, 5), 0);
}

TEST(BinaryWriter, WriteString) {
    std::ostringstream oss;
    BinaryWriter w(oss);

    std::string str = "hello";

    size_t written = w.write_string(str);

    auto data = oss.str();

    ASSERT_EQ(written, 2 + str.size());

    uint16_t len = read_le<uint16_t>(data);
    EXPECT_EQ(len, str.size());

    EXPECT_EQ(std::string(data.begin() + 2, data.end()), str);
}

TEST(BinaryWriter, WritePodStruct) {
    std::ostringstream oss;
    BinaryWriter w(oss);

    struct Pod {
        uint32_t a;
        uint16_t b;
        uint8_t c;
    } input{0xAABBCCDD, 0xEEFF, 0x11};

    size_t written = w.write_pod(input);

    auto data = oss.str();

    ASSERT_EQ(written, sizeof(Pod));

    Pod out;
    std::memcpy(&out, data.data(), sizeof(Pod));

    EXPECT_EQ(out.a, input.a);
    EXPECT_EQ(out.b, input.b);
    EXPECT_EQ(out.c, input.c);
}

TEST(BinaryWriter, WriteDouble) {
    std::ostringstream oss;
    BinaryWriter w(oss);

    double input = 3.141592653589793;
    size_t written = w.write_double(input);

    auto data = oss.str();

    ASSERT_EQ(written, sizeof(double));

    double out;
    std::memcpy(&out, data.data(), sizeof(double));

    EXPECT_EQ(out, input);
}
