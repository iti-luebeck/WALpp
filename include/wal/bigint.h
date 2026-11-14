#pragma once

#include <cstdint>
#include <cctype>
#include <string>
#include <vector>
#include <optional>
#include <algorithm>

namespace wal {

/**
 * @brief Arbitrary-precision integer, for the opt-in bigint mode.
 *
 * WAL++ represents integers as `int64_t`, which is the right default: it is what a
 * waveform's values and indices need, and it costs nothing. Upstream WAL is written
 * in Python, whose integers are unbounded, so a program that computes past 2^63 runs
 * there and used to fail here before it even started, because the literal would not
 * parse.
 *
 * This type closes that gap without making every program pay for it. It is used only
 * when bigint mode is on (see bigint_mode()), and only for values that do not fit in
 * an `int64_t`, so ordinary arithmetic keeps the native path.
 *
 * The magnitude is little-endian base 1e9, which makes decimal reading and printing
 * exact and simple. That is slower than a binary limb representation, and
 * deliberately so: this is the mode a user asks for when correctness past 64 bits
 * matters more than speed.
 */
class BigInt {
public:
    static constexpr uint32_t BASE = 1000000000u;
    static constexpr int BASE_DIGITS = 9;

    BigInt() = default;

    explicit BigInt(int64_t value) {
        negative_ = value < 0;
        // Negating INT64_MIN overflows, so go through the unsigned magnitude.
        uint64_t magnitude = negative_ ? (~static_cast<uint64_t>(value) + 1ULL)
                                       : static_cast<uint64_t>(value);
        while (magnitude > 0) {
            limbs_.push_back(static_cast<uint32_t>(magnitude % BASE));
            magnitude /= BASE;
        }
        trim();
    }

    /// Read a decimal literal. Returns nullopt if @p text is not one.
    static std::optional<BigInt> from_decimal(const std::string& text) {
        if (text.empty()) return std::nullopt;
        size_t i = 0;
        bool negative = false;
        if (text[0] == '+' || text[0] == '-') {
            negative = text[0] == '-';
            i = 1;
        }
        if (i >= text.size()) return std::nullopt;
        for (size_t k = i; k < text.size(); ++k) {
            if (!std::isdigit(static_cast<unsigned char>(text[k]))) return std::nullopt;
        }
        BigInt out;
        const std::string digits = text.substr(i);
        for (int end = static_cast<int>(digits.size()); end > 0; end -= BASE_DIGITS) {
            int start = std::max(0, end - BASE_DIGITS);
            out.limbs_.push_back(static_cast<uint32_t>(
                std::stoul(digits.substr(static_cast<size_t>(start),
                                         static_cast<size_t>(end - start)))));
        }
        out.negative_ = negative;
        out.trim();
        return out;
    }

    bool is_zero() const { return limbs_.empty(); }
    bool is_negative() const { return negative_ && !is_zero(); }

    /// Whether this value would survive a conversion to int64_t.
    bool fits_int64() const {
        if (limbs_.size() > 3) return false;
        // 2^63 is 19 digits, so three base-1e9 limbs (27 digits) can still overflow.
        unsigned __int128 magnitude = 0;
        for (size_t i = limbs_.size(); i-- > 0;) {
            magnitude = magnitude * BASE + limbs_[i];
        }
        const unsigned __int128 limit = negative_
            ? (static_cast<unsigned __int128>(1) << 63)
            : (static_cast<unsigned __int128>(1) << 63) - 1;
        return magnitude <= limit;
    }

    int64_t to_int64() const {
        unsigned __int128 magnitude = 0;
        for (size_t i = limbs_.size(); i-- > 0;) {
            magnitude = magnitude * BASE + limbs_[i];
        }
        if (negative_) {
            return static_cast<int64_t>(~static_cast<uint64_t>(magnitude) + 1ULL);
        }
        return static_cast<int64_t>(magnitude);
    }

    /// Lossy, for mixing with floats. Large values saturate the double range.
    double to_double() const {
        double out = 0.0;
        for (size_t i = limbs_.size(); i-- > 0;) {
            out = out * BASE + limbs_[i];
        }
        return negative_ ? -out : out;
    }

    std::string to_string() const {
        if (is_zero()) return "0";
        std::string out = negative_ ? "-" : "";
        out += std::to_string(limbs_.back());
        for (size_t i = limbs_.size() - 1; i-- > 0;) {
            std::string limb = std::to_string(limbs_[i]);
            out += std::string(static_cast<size_t>(BASE_DIGITS) - limb.size(), '0') + limb;
        }
        return out;
    }

    /// -1, 0 or 1.
    int compare(const BigInt& other) const {
        if (is_zero() && other.is_zero()) return 0;
        if (is_negative() != other.is_negative()) return is_negative() ? -1 : 1;
        int magnitude = compare_magnitude(limbs_, other.limbs_);
        return is_negative() ? -magnitude : magnitude;
    }

    bool operator==(const BigInt& other) const { return compare(other) == 0; }

    BigInt operator-() const {
        BigInt out = *this;
        if (!out.is_zero()) out.negative_ = !out.negative_;
        return out;
    }

    BigInt operator+(const BigInt& other) const {
        if (negative_ == other.negative_) {
            BigInt out;
            out.limbs_ = add_magnitude(limbs_, other.limbs_);
            out.negative_ = negative_;
            out.trim();
            return out;
        }
        // Different signs: subtract the smaller magnitude from the larger.
        int cmp = compare_magnitude(limbs_, other.limbs_);
        if (cmp == 0) return BigInt();
        BigInt out;
        if (cmp > 0) {
            out.limbs_ = sub_magnitude(limbs_, other.limbs_);
            out.negative_ = negative_;
        } else {
            out.limbs_ = sub_magnitude(other.limbs_, limbs_);
            out.negative_ = other.negative_;
        }
        out.trim();
        return out;
    }

    BigInt operator-(const BigInt& other) const { return *this + (-other); }

    BigInt operator*(const BigInt& other) const {
        if (is_zero() || other.is_zero()) return BigInt();
        std::vector<uint64_t> wide(limbs_.size() + other.limbs_.size(), 0);
        for (size_t i = 0; i < limbs_.size(); ++i) {
            uint64_t carry = 0;
            for (size_t k = 0; k < other.limbs_.size() || carry; ++k) {
                uint64_t current = wide[i + k] + carry;
                if (k < other.limbs_.size()) {
                    current += static_cast<uint64_t>(limbs_[i]) * other.limbs_[k];
                }
                wide[i + k] = current % BASE;
                carry = current / BASE;
            }
        }
        BigInt out;
        out.limbs_.assign(wide.begin(), wide.end());
        out.negative_ = negative_ != other.negative_;
        out.trim();
        return out;
    }

private:
    bool negative_ = false;
    std::vector<uint32_t> limbs_;   ///< little-endian, base 1e9, no leading zeros

    void trim() {
        while (!limbs_.empty() && limbs_.back() == 0) limbs_.pop_back();
        if (limbs_.empty()) negative_ = false;   // there is only one zero
    }

    static int compare_magnitude(const std::vector<uint32_t>& a,
                                 const std::vector<uint32_t>& b) {
        if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
        for (size_t i = a.size(); i-- > 0;) {
            if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
        }
        return 0;
    }

    static std::vector<uint32_t> add_magnitude(const std::vector<uint32_t>& a,
                                               const std::vector<uint32_t>& b) {
        std::vector<uint32_t> out;
        uint32_t carry = 0;
        for (size_t i = 0; i < std::max(a.size(), b.size()) || carry; ++i) {
            uint64_t sum = carry;
            if (i < a.size()) sum += a[i];
            if (i < b.size()) sum += b[i];
            out.push_back(static_cast<uint32_t>(sum % BASE));
            carry = static_cast<uint32_t>(sum / BASE);
        }
        return out;
    }

    /// Requires |a| >= |b|.
    static std::vector<uint32_t> sub_magnitude(const std::vector<uint32_t>& a,
                                               const std::vector<uint32_t>& b) {
        std::vector<uint32_t> out = a;
        int64_t borrow = 0;
        for (size_t i = 0; i < out.size(); ++i) {
            int64_t current = static_cast<int64_t>(out[i]) - borrow
                            - (i < b.size() ? static_cast<int64_t>(b[i]) : 0);
            if (current < 0) {
                current += BASE;
                borrow = 1;
            } else {
                borrow = 0;
            }
            out[i] = static_cast<uint32_t>(current);
        }
        return out;
    }
};

/**
 * @brief Whether arbitrary-precision integers are enabled.
 *
 * Off by default: 64-bit integers are what waveform values and indices need, and the
 * bigint path is slower. Turned on with `--bigint` on the command line, or with
 * set_bigint_mode().
 *
 * This is process-wide rather than per-interpreter because the PARSER consults it,
 * and the parser is used standalone in several places.
 *
 * Defined in one translation unit rather than as an inline function with a local
 * static: the extension module and libwalpp are separate shared libraries, and an
 * inline static gave each of them its OWN copy, so setting the mode from Python left
 * the parser inside libwalpp still in 64-bit mode.
 */
/**
 * @brief Whether upstream's permissive type coercion is enabled.
 *
 * Off by default. WAL++ keeps types apart unless a conversion is written out:
 * `(+ 1 "hi")` is an error, and `(= 0 #f)`, `(= 1 #t)` and `(= 1 1.0)` are all
 * false, because an int, a bool, a float and a string are four different things.
 *
 * That matters here more than it would in a general-purpose language. The VCD
 * reader represents a signal wider than 64 bits as a bit STRING, so a `+` that
 * coerced strings would turn `(+ wide 1)` into concatenation of a bit pattern and
 * hand back a plausible-looking wrong answer instead of failing.
 *
 * Upstream, being Python, coerces freely. `--compat-types` turns that on for
 * running upstream scripts unchanged, and says so rather than making it the
 * silent default.
 */
bool& compat_types_mode();

void set_compat_types_mode(bool enabled);

bool& bigint_mode();

void set_bigint_mode(bool enabled);

} // namespace wal
