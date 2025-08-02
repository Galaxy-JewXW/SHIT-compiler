#ifndef SIMPLEFLOAT_H
#define SIMPLEFLOAT_H

#include <bitset>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

// 目标平台的浮点数格式 (IEEE 754 Single-Precision)
namespace IEEE754_Single {
constexpr int EXPONENT_BITS = 8;
constexpr int MANTISSA_BITS = 23;
constexpr int EXPONENT_BIAS = 127;

static_assert(sizeof(float) == sizeof(uint32_t), "float and uint32_t must be the same size");

class SimpleFloat {
public:
    union FloatBits {
        float f;
        uint32_t u;
    };

private:
    FloatBits data{};

    void from_long_double(const long double val) {
        // 1. 处理特殊值
        if (std::isnan(val)) {
            // 创建一个标准的 quiet NaN
            data.u = 0x7FC00000;
            return;
        }
        if (std::isinf(val)) {
            data.u = val < 0 ? 0xFF800000 : 0x7F800000;
            return;
        }
        if (val == 0.0) {
            data.u = std::signbit(val) ? 0x80000000 : 0x00000000;
            return;
        }

        // 提取符号位
        const bool is_negative = std::signbit(val);
        const uint32_t sign_bit = is_negative ? 1 : 0;
        const long double abs_val = std::abs(val);

        // 规格化：将 abs_val 分解为 mantissa * 2^exponent
        // frexpl 返回的尾数在 [0.5, 1.0) 区间
        int exponent;
        long double mantissa = std::frexp(abs_val, &exponent);

        // 调整尾数到 [1.0, 2.0) 区间，这是 IEEE 754 规格化数的标准形式
        mantissa *= 2.0L;
        exponent -= 1;

        // 4. 计算带偏移的指数
        const int biased_exponent = exponent + EXPONENT_BIAS;

        // 检查上溢/下溢
        if (biased_exponent >= (1 << EXPONENT_BITS)) {
            // 上溢
            from_long_double(is_negative
                                 ? -std::numeric_limits<long double>::infinity()
                                 : std::numeric_limits<long double>::infinity());
            return;
        }
        if (biased_exponent <= 0) {
            // 下溢 (简化处理为0)
            from_long_double(is_negative ? -0.0 : 0.0);
            return;
        }

        // 计算尾数位
        // 减去隐藏的整数位 '1'
        const long double fraction = mantissa - 1.0L;
        // 将小数部分转换为23位整数，并进行简单的四舍五入
        const auto mantissa_bits = static_cast<uint32_t>(std::lround(fraction * (1ULL << MANTISSA_BITS)));

        // 组合所有部分
        data.u = (sign_bit << 31) |
                 (static_cast<uint32_t>(biased_exponent) << MANTISSA_BITS) |
                 mantissa_bits;
    }

public:
    SimpleFloat() = default;

    explicit SimpleFloat(const std::string &literal) {
        double val = 0.0;
        // 检查是否以 "0x" 或 "0X" 开头
        if (literal.size() >= 2 && (literal[0] == '0' && (literal[1] == 'x' || literal[1] == 'X'))) {
            // 十六进制格式 - 使用 std::strtod
            char* endptr;
            val = std::strtod(literal.c_str(), &endptr);
            if (endptr == literal.c_str() + literal.size()) {
                from_long_double(val);
            } else {
                std::cerr << "Error: Could not parse hex literal '" << literal << "'\n";
                data.u = 0;
            }
        } else {
            // 十进制格式 - 使用 std::from_chars
            const char* start = literal.c_str();
            const char *end = start + literal.size();
            if (const auto [ptr, ec] = std::from_chars(start, end, val, std::chars_format::general);
                ec == std::errc{} && ptr == end) {
                from_long_double(val);
            } else {
                std::cerr << "Error: Could not parse decimal literal '" << literal << "'\n";
                data.u = 0;
            }
        }
    }

    [[nodiscard]] float to_float() const {
        return data.f;
    }

    [[nodiscard]] uint32_t bits() const {
        return data.u;
    }

    void encode(const long double val) {
        from_long_double(val);
    }

    void encode(const double val) {
        from_long_double(static_cast<long double>(val));
    }

    void details() const {
        const std::bitset<32> bits(data.u);
        std::cout << "  Sign: " << bits[31] << std::endl;
        std::cout << "  Exponent: " << bits.to_string().substr(1, 8) << std::endl;
        std::cout << "  Mantissa: " << bits.to_string().substr(9, 23) << std::endl;
    }
};
}

#endif //SIMPLEFLOAT_H
