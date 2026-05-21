#pragma once // utils/binary_writer.hpp

#include <cstdint>
#include <string>
#include <string_view>
#include <ostream>
#include <type_traits>

namespace htap::utils {

inline constexpr size_t MAX_STRING_LENGTH = 1024;

// little endian writes

class BinaryWriter {
public:
    explicit BinaryWriter(std::ostream& out);

    size_t write_u8(uint8_t value);
    size_t write_u16(uint16_t value);
    size_t write_u32(uint32_t value);
    size_t write_u64(uint64_t value);

    size_t write_i64(int64_t value);

    size_t write_double(double value);

    size_t write_bytes(const void* data, size_t size);

    size_t write_string(std::string_view str); // записывает 2 байта длины и потом строку

    template <typename T>
    size_t write_pod(const T& value) {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        return write_bytes(&value, sizeof(T));
    }

private:
    template <typename T>
    size_t write_le(T value)  {
        static_assert(std::is_integral_v<T>, "write_le only supports integral types");

        if constexpr (std::endian::native == std::endian::big)
            value = std::byteswap(value);

        return write_bytes(&value, sizeof(value));
    }

private:
    std::ostream& out_;
};

} // namespace htap::utils
