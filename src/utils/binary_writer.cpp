#include "utils/binary_writer.hpp"

#include <bit>
#include <stdexcept>
#include <limits>

using namespace htap::utils;

BinaryWriter::BinaryWriter(std::ostream& out) : out_(out) {}

size_t BinaryWriter::write_u8(uint8_t value) {
    return write_bytes(&value, sizeof(value));
}

size_t BinaryWriter::write_u16(uint16_t value) {
    return write_le(value);
}

size_t BinaryWriter::write_u32(uint32_t value) {
    return write_le(value);
}

size_t BinaryWriter::write_u64(uint64_t value) {
    return write_le(value);
}

size_t BinaryWriter::write_i64(int64_t value) {
    return write_le(std::bit_cast<uint64_t>(value));
}

size_t BinaryWriter::write_double(double value) {
    return write_le(std::bit_cast<uint64_t>(value));
}

size_t BinaryWriter::write_bytes(const void* data, size_t size) {
    out_.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));

    if (!out_)
        throw std::ios_base::failure("BinaryWriter: write failed");

    return size;
}

size_t BinaryWriter::write_string(std::string_view str) {
    if (str.size() > MAX_STRING_LENGTH)
        throw std::length_error("BinaryWriter: string too large");

    size_t written = 0;

    written += write_u16(static_cast<uint16_t>(str.size()));
    written += write_bytes(str.data(), str.size());

    return written;
}
