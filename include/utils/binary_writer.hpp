#pragma once

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

    void write_u8(uint8_t value);
    void write_u32(uint32_t value);
    void write_u64(uint64_t value);

    void write_i64(int64_t value);

    void write_double(double value);

    void write_bytes(const void* data, size_t size);

    void write_string(std::string_view str);

    template <typename T>
    void write_pod(const T& value) {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        write_bytes(&value, sizeof(T));
    }

private:
    template <typename T>
    void write_le(T value);

private:
    std::ostream& out_;
};

} // namespace htap::utils
