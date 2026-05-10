#include <gtest/gtest.h>
#include <sstream>
#include <vector>
#include <cstdint>
#include <cstring>

#include "utils/binary_writer.hpp"

using htap::utils::BinaryWriter;

static std::vector<uint8_t> stream_bytes(std::ostringstream& oss) {
    const std::string& s = oss.str();
    return std::vector<uint8_t>(s.begin(), s.end());
}

TEST(BinaryWriter, WritesU8) {
    std::ostringstream oss(std::ios::binary);
    BinaryWriter w(oss);

    w.write_u8(0xAB);

    auto bytes = stream_bytes(oss);
    ASSERT_EQ(bytes.size(), 1);
    EXPECT_EQ(bytes[0], 0xAB);
}

TEST(BinaryWriter, WritesU32LittleEndian) {
    std::ostringstream oss(std::ios::binary);
    BinaryWriter w(oss);

    w.write_u32(0x11223344);

    auto bytes = stream_bytes(oss);
    ASSERT_EQ(bytes.size(), 4);

    EXPECT_EQ(bytes[0], 0x44);
    EXPECT_EQ(bytes[1], 0x33);
    EXPECT_EQ(bytes[2], 0x22);
    EXPECT_EQ(bytes[3], 0x11);
}

TEST(BinaryWriter, WritesU64LittleEndian) {
    std::ostringstream oss(std::ios::binary);
    BinaryWriter w(oss);

    w.write_u64(0x0102030405060708ULL);

    auto bytes = stream_bytes(oss);
    ASSERT_EQ(bytes.size(), 8);

    EXPECT_EQ(bytes[0], 0x08);
    EXPECT_EQ(bytes[1], 0x07);
    EXPECT_EQ(bytes[2], 0x06);
    EXPECT_EQ(bytes[3], 0x05);
    EXPECT_EQ(bytes[4], 0x04);
    EXPECT_EQ(bytes[5], 0x03);
    EXPECT_EQ(bytes[6], 0x02);
    EXPECT_EQ(bytes[7], 0x01);
}

TEST(BinaryWriter, WritesI64) {
    std::ostringstream oss(std::ios::binary);
    BinaryWriter w(oss);

    int64_t value = -1;
    w.write_i64(value);

    auto bytes = stream_bytes(oss);
    ASSERT_EQ(bytes.size(), 8);

    for (auto b : bytes) {
        EXPECT_EQ(b, 0xFF);
    }
}

TEST(BinaryWriter, WritesDouble) {
    std::ostringstream oss(std::ios::binary);
    BinaryWriter w(oss);

    double value = 3.141592653589793;
    w.write_double(value);

    auto bytes = stream_bytes(oss);
    ASSERT_EQ(bytes.size(), sizeof(double));

    double out;
    std::memcpy(&out, bytes.data(), sizeof(double));

    EXPECT_EQ(out, value);
}

TEST(BinaryWriter, WritesBytes) {
    std::ostringstream oss(std::ios::binary);
    BinaryWriter w(oss);

    const uint8_t data[] = {1, 2, 3, 4, 5};
    w.write_bytes(data, sizeof(data));

    auto bytes = stream_bytes(oss);

    ASSERT_EQ(bytes.size(), 5);
    EXPECT_EQ(bytes, (std::vector<uint8_t>{1,2,3,4,5}));
}

TEST(BinaryWriter, WritesString) {
    std::ostringstream oss(std::ios::binary);
    BinaryWriter w(oss);

    std::string str = "hello";
    w.write_string(str);

    auto bytes = stream_bytes(oss);

    ASSERT_EQ(bytes.size(), 9);

    uint32_t len;
    std::memcpy(&len, bytes.data(), sizeof(uint32_t));
    EXPECT_EQ(len, 5);

    std::string decoded(bytes.begin() + 4, bytes.end());
    EXPECT_EQ(decoded, "hello");
}

TEST(BinaryWriter, WritesPOD) {
    struct Test {
        int32_t a;
        uint16_t b;
    };

    std::ostringstream oss(std::ios::binary);
    BinaryWriter w(oss);

    Test t{0x11223344, 0x5566};
    w.write_pod(t);

    auto bytes = stream_bytes(oss);

    ASSERT_EQ(bytes.size(), sizeof(Test));

    Test out;
    std::memcpy(&out, bytes.data(), sizeof(Test));

    EXPECT_EQ(out.a, t.a);
    EXPECT_EQ(out.b, t.b);
}
