#include "utils/binary_writer.hpp"

#include <bit>
#include <stdexcept>
#include <limits>

using namespace htap::utils;

BinaryWriter::BinaryWriter(std::ostream& out) : out_(out) {}

void BinaryWriter::write_u8(uint8_t value) {
    write_bytes(&value, sizeof(value));
}

void BinaryWriter::write_u32(uint32_t value) {
    write_le(value);
}

void BinaryWriter::write_u64(uint64_t value) {
    write_le(value);
}

void BinaryWriter::write_i64(int64_t value) {
    write_le(static_cast<uint64_t>(value));
}

void BinaryWriter::write_double(double value) {
    write_bytes(&value, sizeof(value));
}

void BinaryWriter::write_bytes(const void* data, size_t size) {
    out_.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));

    if (!out_)
        throw std::ios_base::failure("BinaryWriter: write failed");
}

void BinaryWriter::write_string(std::string_view str) {
    if (str.size() > MAX_STRING_LENGTH)
        throw std::length_error("BinaryWriter: string too large");

    write_u32(static_cast<uint32_t>(str.size()));

    if (!str.empty())
        write_bytes(str.data(), str.size());
}

// private helper for little-endiannes

template <typename T>
void BinaryWriter::write_le(T value) {
    static_assert(std::is_integral_v<T>, "write_le only supports integral types");

    if constexpr (std::endian::native == std::endian::big)
        value = std::byteswap(value);

    write_bytes(&value, sizeof(value));
}

template void BinaryWriter::write_le<uint32_t>(uint32_t);
template void BinaryWriter::write_le<uint64_t>(uint64_t);
