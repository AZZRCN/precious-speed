/**
 * @file Integer.h
 * @brief Header file for an efficient C++ arbitrary-precision integer arithmetic library.
 *
 * This header provides high-performance arbitrary-precision integer types and related algorithms,
 * supporting construction, arithmetic operations, and input/output for both unsigned and signed big integers.
 * It is suitable for scenarios requiring large number computations.
 *
 * @author [masonxiong](https://www.luogu.com.cn/user/446979)
 * @date 2025-8-24
 */

#ifndef INTEGER_H
#define INTEGER_H 20250824L

#include <cmath>
#include <limits>
#include <string>
#include <vector>
#include <cstring>
#include <cstdint>
#include <utility>
#include <complex>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <immintrin.h>

#ifdef ENABLE_VALIDITY_CHECK
#define VALIDITY_CHECK(condition, errorType, message) if (!(condition)) throw errorType(message);
#else
#define VALIDITY_CHECK(condition, errorType, message)
#endif

namespace detail {
    struct InputHelper {
        std::uint32_t table[0x10000];

        _GLIBCXX14_CONSTEXPR InputHelper() : table() {
            for (std::uint32_t i = 0; i != 0x10000; table[i++] = std::uint32_t(-1));
            for (std::uint32_t i = 48; i != 58; ++i)
                for (std::uint32_t j = 48; j != 58; ++j)
                    table[i << 8 | j] = (j & 15) * 10 + (i & 15);
        }

        std::uint32_t operator()(const char* value) const {
            return table[*reinterpret_cast<const std::uint16_t*>(value)];
        }
    };

    struct OutputHelper {
        std::uint32_t table[10000];

        _GLIBCXX14_CONSTEXPR OutputHelper() : table() {
            for (std::uint32_t* i = table, a = 48; a != 58; ++a)
                for (std::uint32_t b = 48; b != 58; ++b)
                    for (std::uint32_t c = 48; c != 58; ++c)
                        for (std::uint32_t d = 48; d != 58; ++d)
                            *i++ = a | (b << 8) | (c << 16) | (d << 24);
        }

        const char* operator()(std::uint32_t value) const {
            return reinterpret_cast<const char*>(table + value);
        }
    };

    struct TransformHelper {
        __m128d* twiddleFactors;
        std::uint32_t length;
        
        TransformHelper() : twiddleFactors(new __m128d[1]()), length(1) {
            *twiddleFactors = _mm_set_pd(0.0, 1.0);
        }

        ~TransformHelper() {
            delete[] twiddleFactors;
        }

        static inline __m128d complexMultiply(__m128d first, __m128d second) {
            return _mm_fmaddsub_pd(_mm_unpacklo_pd(first, first), second, _mm_unpackhi_pd(first, first) * _mm_permute_pd(second, 1));
        }

        static inline __m128d complexMultiplyConjugate(__m128d first, __m128d second) {
            return _mm_fmsubadd_pd(_mm_unpacklo_pd(second, second), first, _mm_unpackhi_pd(second, second) * _mm_permute_pd(first, 1));
        }

        static inline __m128d complexMultiplySpecial(__m128d first, __m128d second) {
            return _mm_fmadd_pd(_mm_unpacklo_pd(first, first), second, _mm_unpackhi_pd(first, first) * _mm_permute_pd(second, 1));
        }

        static inline __m128d complexScalarMultiply(__m128d complex, double scalar) {
            return complex * _mm_set1_pd(scalar);
        }

        void resize(std::uint32_t transformLength) {
            if (transformLength > length << 1) {
                std::uint32_t halfLog = std::__lg(transformLength) >> 1, halfSize = 1 << halfLog;
                __m128d* baseFactors = new __m128d[halfSize << 1]();
                const double angleStep = acos(-1.0) / halfSize, fineAngleStep = angleStep / halfSize;
                for (std::uint32_t i = 0, j = (halfSize * 3) >> 1, phaseAccumulator = 0; i != halfSize; phaseAccumulator -= halfSize - (j >> __builtin_ctz(++i))) {
                    std::complex<double> first = std::polar(1.0, phaseAccumulator * angleStep), second = std::polar(1.0, phaseAccumulator * fineAngleStep);
                    baseFactors[i] = _mm_set_pd(first.imag(), first.real()), baseFactors[i | halfSize] = _mm_set_pd(second.imag(), second.real());
                }
                __m128d* newFactors = reinterpret_cast<__m128d*>(std::memcpy(new __m128d[transformLength >> 1], twiddleFactors, length << 4));
                delete[] twiddleFactors, twiddleFactors = newFactors;
                for (std::uint32_t i = length; i != transformLength >> 1; ++i)
                    twiddleFactors[i] = complexMultiply(baseFactors[i & (halfSize - 1)], baseFactors[halfSize | (i >> halfLog)]);
                delete[] baseFactors, length = transformLength >> 1;
            }
        }

        void decimationInFrequency(__m128d* dataArray, std::uint32_t transformSize) {
            for (std::uint32_t blockSize = transformSize >> 1, stepSize = transformSize; blockSize; stepSize = blockSize, blockSize >>= 1) {
                for (__m128d* currentElement = dataArray; currentElement != dataArray + blockSize; ++currentElement) {
                    const __m128d evenElement = *currentElement, oddElement = currentElement[blockSize];
                    *currentElement = evenElement + oddElement, currentElement[blockSize] = evenElement - oddElement;
                }
                for (__m128d* blockStart = dataArray + stepSize, *twiddlePointer = twiddleFactors + 1; blockStart != dataArray + transformSize; blockStart += stepSize, ++twiddlePointer) {
                    for (__m128d* currentElement = blockStart; currentElement != blockStart + blockSize; ++currentElement) {
                        const __m128d evenElement = *currentElement, oddElement = complexMultiply(currentElement[blockSize], *twiddlePointer);
                        *currentElement = evenElement + oddElement, currentElement[blockSize] = evenElement - oddElement;
                    }
                }
            }
        }

        void decimationInTime(__m128d* dataArray, std::uint32_t transformSize) {
            for (std::uint32_t blockSize = 1, stepSize = 2; blockSize < transformSize; blockSize = stepSize, stepSize <<= 1) {
                for (__m128d* currentElement = dataArray; currentElement != dataArray + blockSize; ++currentElement) {
                    const __m128d evenElement = *currentElement, oddElement = currentElement[blockSize];
                    *currentElement = evenElement + oddElement, currentElement[blockSize] = evenElement - oddElement;
                }
                for (__m128d* blockStart = dataArray + stepSize, *twiddlePointer = twiddleFactors + 1; blockStart != dataArray + transformSize; blockStart += stepSize, ++twiddlePointer) {
                    for (__m128d* currentElement = blockStart; currentElement != blockStart + blockSize; ++currentElement) {
                        const __m128d evenElement = *currentElement, oddElement = currentElement[blockSize];
                        *currentElement = evenElement + oddElement, currentElement[blockSize] = complexMultiplyConjugate(evenElement - oddElement, *twiddlePointer);
                    }
                }
            }
        }

        void frequencyDomainPointwiseMultiply(__m128d* firstArray, __m128d* secondArray, std::uint32_t transformSize) {
            const double normalizationFactor = 1.0 / transformSize, scalingFactor = normalizationFactor * 0.25;
            firstArray[0] = complexScalarMultiply(complexMultiplySpecial(firstArray[0], secondArray[0]), normalizationFactor);
            firstArray[1] = complexScalarMultiply(complexMultiply(firstArray[1], secondArray[1]), normalizationFactor);
            const __m128d conjugateMask = _mm_castsi128_pd(_mm_set_epi64x(std::int64_t(1ull << 63), 0));
            const __m128d negateMask = _mm_castsi128_pd(_mm_set_epi64x(std::int64_t(1ull << 63), std::int64_t(1ull << 63)));
            for (std::uint32_t blockStart = 2, blockEnd = 3; blockStart < transformSize; blockStart <<= 1, blockEnd <<= 1) {
                for (std::uint32_t forwardIndex = blockStart, backwardIndex = forwardIndex + blockStart - 1; forwardIndex < blockEnd; ++forwardIndex, --backwardIndex) {
                    const __m128d firstEven = _mm_add_pd(firstArray[forwardIndex], _mm_xor_pd(firstArray[backwardIndex], conjugateMask)), firstOdd  = _mm_sub_pd(firstArray[forwardIndex], _mm_xor_pd(firstArray[backwardIndex], conjugateMask));
                    const __m128d secondEven = _mm_add_pd(secondArray[forwardIndex], _mm_xor_pd(secondArray[backwardIndex], conjugateMask)), secondOdd  = _mm_sub_pd(secondArray[forwardIndex], _mm_xor_pd(secondArray[backwardIndex], conjugateMask));
                    const __m128d productA = _mm_sub_pd(complexMultiply(firstEven, secondEven), complexMultiply(complexMultiply(firstOdd, secondOdd), (forwardIndex & 1 ? _mm_xor_pd(twiddleFactors[forwardIndex >> 1], negateMask) : twiddleFactors[forwardIndex >> 1]))), productB = _mm_add_pd(complexMultiply(secondEven, firstOdd), complexMultiply(firstEven, secondOdd));
                    firstArray[forwardIndex] = complexScalarMultiply(_mm_add_pd(productA, productB), scalingFactor);
                    firstArray[backwardIndex] = _mm_xor_pd(complexScalarMultiply(_mm_sub_pd(productA, productB), scalingFactor), conjugateMask);
                }
            }
        }
    };

    static _GLIBCXX14_CONSTEXPR InputHelper I = {};
    static _GLIBCXX14_CONSTEXPR OutputHelper O = {};
    static TransformHelper T = {};
}

class UnsignedInteger;
class SignedInteger;

class UnsignedInteger {
public:
    static constexpr std::uint32_t Base = 100000000;
    static constexpr std::uint32_t TransformLimit = 4194304;
    static constexpr std::uint32_t BruteforceThreshold = 64;

    std::uint32_t* digits, length, capacity;

public:
    UnsignedInteger(std::uint32_t initialLength, std::uint32_t initialCapacity) : digits(new std::uint32_t[initialCapacity]), length(initialLength), capacity(initialCapacity) {}

    void resize(std::uint32_t newLength) {
        if (newLength > capacity) {
            std::uint32_t* newMemory = reinterpret_cast<std::uint32_t*>(std::memcpy(new std::uint32_t[capacity = newLength], digits, length << 2));
            delete[] digits, digits = newMemory;
        }
        length = newLength;
    }

    void construct(const char* value, std::uint32_t stringLength) {
        std::uint32_t* currentDigit = digits + length - 1;
        switch (stringLength & 7) {
            case 0: ++currentDigit; break;
            case 1: *currentDigit = *value & 15; break;
            case 2: *currentDigit = detail::I(value); break;
            case 3: *currentDigit = (*value & 15) * 100 + detail::I(value + 1); break;
            case 4: *currentDigit = detail::I(value) * 100 + detail::I(value + 2); break;
            case 5: *currentDigit = (*value & 15) * 10000 + detail::I(value + 1) * 100 + detail::I(value + 3); break;
            case 6: *currentDigit = detail::I(value) * 10000 + detail::I(value + 2) * 100 + detail::I(value + 4); break;
            case 7: *currentDigit = (*value & 15) * 1000000 + detail::I(value + 1) * 10000 + detail::I(value + 3) * 100 + detail::I(value + 5); break;
        }
        for (const char* position = value + (stringLength & 7); currentDigit != digits; *--currentDigit = detail::I(position) * 1000000 + detail::I(position + 2) * 10000 + detail::I(position + 4) * 100 + detail::I(position + 6), position += 8);
        for (; length > 1 && !digits[length - 1]; --length);
    }

    std::int32_t reverseCompare(const UnsignedInteger& other) const {
        constexpr std::uint32_t BlockSize = 64;
        const std::uint32_t remaining = length % BlockSize, *firstBlockPointer = digits + length - remaining, *secondBlockPointer = other.digits + length - remaining;
        if (std::memcmp(firstBlockPointer, secondBlockPointer, remaining << 2))
            for (const std::uint32_t *firstElementPointer = firstBlockPointer + remaining, *secondElementPointer = secondBlockPointer + remaining; firstElementPointer != firstBlockPointer; )
                if (*--firstElementPointer != *--secondElementPointer)
                    return std::int32_t(*firstElementPointer - *secondElementPointer);
        while (firstBlockPointer != digits)
            if (std::memcmp(firstBlockPointer -= BlockSize, secondBlockPointer -= BlockSize, BlockSize << 2))
                for (const std::uint32_t *firstElementPointer = firstBlockPointer + BlockSize, *secondElementPointer = secondBlockPointer + BlockSize; firstElementPointer != firstBlockPointer; )
                    if (*--firstElementPointer != *--secondElementPointer)
                        return std::int32_t(*firstElementPointer - *secondElementPointer);
        return 0;
    }

    std::pair<UnsignedInteger, UnsignedInteger> bruteforceDivisionAndModulus(const UnsignedInteger& divisor) const {
        if (*this < divisor)
            return std::make_pair(UnsignedInteger(), *this);
        UnsignedInteger quotient(length - divisor.length + 1, length - divisor.length + 1), remainder = *this;
        quotient.length = length - divisor.length + 1;
        auto getEstimatedValue = [](const std::uint32_t* digitArray, std::uint32_t highIndex, std::uint32_t arrayLength) -> std::uint64_t {
            return std::uint64_t(10) * Base * ((highIndex + 1) < arrayLength ? digitArray[highIndex + 1] : 0) + std::uint64_t(10) * digitArray[highIndex] + (highIndex ? digitArray[highIndex - 1] : 0) / (Base / 10);
        };
        for (std::uint32_t currentPosition = length - divisor.length; ~currentPosition; --currentPosition) {
            std::uint32_t partialQuotient = 0;
            auto performSubtraction = [&]() {
                std::int64_t carry = 0;
                for (std::uint32_t digitIndex = 0; digitIndex != divisor.length; ++digitIndex) {
                    carry = carry - std::int64_t(partialQuotient) * divisor.digits[digitIndex] + remainder.digits[currentPosition + digitIndex];
                    remainder.digits[currentPosition + digitIndex] = std::uint32_t(carry % Base), carry /= Base;
                    if (remainder.digits[currentPosition + digitIndex] >= Base)
                        remainder.digits[currentPosition + digitIndex] += Base, --carry;
                }
                if (carry)
                    remainder.digits[currentPosition + divisor.length] += std::uint32_t(carry);
                quotient.digits[currentPosition] += partialQuotient;
            };
            for (quotient.digits[currentPosition] = 0; (partialQuotient = std::uint32_t(getEstimatedValue(remainder.digits, currentPosition + divisor.length - 1, length) / (getEstimatedValue(divisor.digits, divisor.length - 1, divisor.length) + 1))); performSubtraction());
            partialQuotient = 1;
            for (std::uint32_t digitIndex = divisor.length; digitIndex--; )
                if (remainder.digits[digitIndex + currentPosition] != divisor.digits[digitIndex] && (partialQuotient = divisor.digits[digitIndex] < remainder.digits[digitIndex + currentPosition], true))
                    break;
            if (partialQuotient)
                performSubtraction();
        }
        for (; quotient.length > 1 && !quotient.digits[quotient.length - 1]; --quotient.length);
        for (; remainder.length > 1 && !remainder.digits[remainder.length - 1]; --remainder.length);
        return std::make_pair(std::move(quotient), std::move(remainder));
    }

    UnsignedInteger rightShift(std::uint32_t shiftAmount) const {
        if (shiftAmount >= length)
            return UnsignedInteger();
        UnsignedInteger result(length - shiftAmount, length - shiftAmount);
        std::memcpy(result.digits, digits + shiftAmount, result.length << 2);
        for (; result.length > 1 && !result.digits[result.length - 1]; --result.length);
        return result;
    }

    UnsignedInteger leftShift(std::uint32_t shiftAmount) const {
        if (length == 0 || shiftAmount == 0)
            return *this;
        UnsignedInteger result(length + shiftAmount, length + shiftAmount);
        std::memset(result.digits, 0, shiftAmount << 2), std::memcpy(result.digits + shiftAmount, digits, length << 2);
        return result;
    }

    UnsignedInteger computeInverse(std::uint32_t precisionBits) const {
        if (length < BruteforceThreshold || precisionBits < length + BruteforceThreshold) {
            UnsignedInteger numerator(precisionBits + 1, precisionBits + 1);
            std::memset(numerator.digits, 0, precisionBits << 2), numerator.digits[precisionBits] = 1;
            return numerator.bruteforceDivisionAndModulus(*this).first;
        }
        const std::uint32_t halfPrecision = (precisionBits - length + 5) >> 1, shiftBack = halfPrecision > length ? 0 : length - halfPrecision;
        UnsignedInteger truncated = rightShift(shiftBack);
        const std::uint32_t newPrecision = halfPrecision + truncated.length;
        UnsignedInteger approximateInverse = truncated.computeInverse(newPrecision);
        UnsignedInteger result = (approximateInverse + approximateInverse).leftShift(precisionBits - newPrecision - shiftBack) - (*this * approximateInverse * approximateInverse).rightShift(2 * (newPrecision + shiftBack) - precisionBits);
        return --result;
    }
    
    std::pair<UnsignedInteger, UnsignedInteger> divisionAndModulus(const UnsignedInteger& other) const {
        if (*this < other)
            return std::make_pair(UnsignedInteger(), *this);
        if (length < BruteforceThreshold || other.length < BruteforceThreshold)
            return bruteforceDivisionAndModulus(other);
        const std::uint32_t precisionBits = length - other.length + 5, shiftBack = precisionBits > other.length ? 0 : other.length - precisionBits;
        UnsignedInteger adjustedDivisor = other.rightShift(shiftBack);
        if (shiftBack)
            ++adjustedDivisor;
        const std::uint32_t inversePrecision = precisionBits + adjustedDivisor.length;
        UnsignedInteger quotient = (*this * adjustedDivisor.computeInverse(inversePrecision)).rightShift(inversePrecision + shiftBack), remainder = *this - quotient * other;
        for (; remainder >= other; ++quotient, remainder -= other);
        return std::make_pair(std::move(quotient), std::move(remainder));
    }

public:
    UnsignedInteger() : digits(new std::uint32_t[1]()), length(1), capacity(1) {}

    UnsignedInteger(const UnsignedInteger& other) : digits(reinterpret_cast<std::uint32_t*>(std::memcpy(new std::uint32_t[other.length], other.digits, other.length << 2))), length(other.length), capacity(other.length) {}

    UnsignedInteger(UnsignedInteger&& other) : digits(other.digits), length(other.length), capacity(other.capacity) {
        other.digits = new std::uint32_t[other.length = other.capacity = 1]();
    }

    UnsignedInteger(const SignedInteger& other) : UnsignedInteger(*reinterpret_cast<const UnsignedInteger*>(&other)) {
        VALIDITY_CHECK(!(reinterpret_cast<const std::pair<UnsignedInteger, bool>*>(&other)->second), std::invalid_argument, "UnsignedInteger constructor error: the provided SignedInteger is negative. UnsignedInteger can only represent non-negative integers.")
    }

    template <typename unsignedIntegral, typename std::enable_if<std::is_unsigned<unsignedIntegral>::value>::type* = nullptr>
    UnsignedInteger(unsignedIntegral value) : UnsignedInteger(0, (std::numeric_limits<unsignedIntegral>::digits10 + 7) >> 3) {
        while (digits[length++] = std::uint32_t(value) % Base, value /= unsignedIntegral(Base));
    }

    template <typename signedIntegral, typename std::enable_if<std::is_signed<signedIntegral>::value && !std::is_floating_point<signedIntegral>::value>::type* = nullptr>
    UnsignedInteger(signedIntegral value) : UnsignedInteger(0, (std::numeric_limits<signedIntegral>::digits10 + 7) >> 3) {
        VALIDITY_CHECK(value >= 0, std::invalid_argument, "UnsignedInteger constructor error: the provided signed integer value = " + std::to_string(value) + " is negative. UnsignedInteger can only represent non-negative integers.")
        while (digits[length++] = std::uint32_t(value) % Base, value /= signedIntegral(Base));
    }

    template <typename floatingPoint, typename std::enable_if<std::is_floating_point<floatingPoint>::value>::type* = nullptr>
    UnsignedInteger(floatingPoint value) : UnsignedInteger(0, (std::numeric_limits<floatingPoint>::max_exponent10 + 7) >> 3) {
        VALIDITY_CHECK(value >= 0, std::invalid_argument, "UnsignedInteger constructor error: the provided floating point value = " + std::to_string(value) + " is negative. UnsignedInteger can only represent non-negative integers.")
        VALIDITY_CHECK(std::isfinite(value), std::invalid_argument, "UnsignedInteger constructor error: the provided floating point value = " + std::to_string(value) + " is not finite.")
        while (digits[length++] = std::uint32_t(std::fmod(value, Base)), (value = std::floor(value / Base)));
    }

    UnsignedInteger(const char* value) {
        VALIDITY_CHECK(value, std::invalid_argument, "UnsignedInteger constructor error: the provided C-style string is a null pointer.")
        const std::uint32_t stringLength = std::uint32_t(std::strlen(value));
        digits = new std::uint32_t[length = capacity = (stringLength + 7) >> 3];
        VALIDITY_CHECK(stringLength, std::invalid_argument, "UnsignedInteger constructor error: the provided C-style string is empty. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        VALIDITY_CHECK(std::all_of(value, value + stringLength, [](char digit) -> bool { return std::isdigit(digit); }), std::invalid_argument, "UnsignedInteger constructor error: the provided C-style string value = \"" + std::string(value) + "\" contains non-digit characters. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        construct(value, stringLength);
    }

    UnsignedInteger(const std::string& value) : UnsignedInteger(std::uint32_t(value.size() + 7) >> 3, std::uint32_t(value.size() + 7) >> 3) {
        VALIDITY_CHECK(value.size(), std::invalid_argument, "UnsignedInteger constructor error: the provided string is empty. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        VALIDITY_CHECK(std::all_of(value.begin(), value.end(), [](char digit) -> bool { return std::isdigit(digit); }), std::invalid_argument, "UnsignedInteger constructor error: the provided string value = \"" + value + "\" contains non-digit characters. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        construct(value.data(), std::uint32_t(value.size()));
    }

    ~UnsignedInteger() {
        delete[] digits;
    }

    UnsignedInteger& operator=(const UnsignedInteger& other) {
        if (digits != other.digits) {
            if (capacity < other.length)
                delete[] digits, digits = new std::uint32_t[other.length], capacity = other.length;
            std::memcpy(digits, other.digits, (length = other.length) << 2);
        }
        return *this;
    }

    UnsignedInteger& operator=(UnsignedInteger&& other) {
        if (digits != other.digits)
            digits = other.digits, length = other.length, capacity = other.capacity, other.digits = new std::uint32_t[other.length = other.capacity = 1]();
        return *this;
    }

    UnsignedInteger& operator=(const SignedInteger& other) {
        VALIDITY_CHECK(!(reinterpret_cast<const std::pair<UnsignedInteger, bool>*>(&other)->second), std::invalid_argument, "UnsignedInteger operator= error: the provided SignedInteger is negative. UnsignedInteger can only represent non-negative integers.")
        return *this = *reinterpret_cast<const UnsignedInteger*>(&other);
    }

    template <typename unsignedIntegral, typename std::enable_if<std::is_unsigned<unsignedIntegral>::value>::type* = nullptr>
    UnsignedInteger& operator=(unsignedIntegral value) {
        if (length = 0, capacity < (std::numeric_limits<unsignedIntegral>::digits10 + 7) >> 3)
            delete[] digits, digits = new std::uint32_t[capacity = (std::numeric_limits<unsignedIntegral>::digits10 + 7) >> 3];
        while (digits[length++] = std::uint32_t(value) % Base, value /= unsignedIntegral(Base));
        return *this;
    }

    template <typename signedIntegral, typename std::enable_if<std::is_signed<signedIntegral>::value && !std::is_floating_point<signedIntegral>::value>::type* = nullptr>
    UnsignedInteger& operator=(signedIntegral value) {
        VALIDITY_CHECK(value >= 0, std::invalid_argument, "UnsignedInteger operator= error: the provided signed integer value = " + std::to_string(value) + " is negative. UnsignedInteger can only represent non-negative integers.")
        if (length = 0, capacity < (std::numeric_limits<signedIntegral>::digits10 + 7) >> 3)
            delete[] digits, digits = new std::uint32_t[capacity = (std::numeric_limits<signedIntegral>::digits10 + 7) >> 3];
        while (digits[length++] = std::uint32_t(value) % Base, value /= signedIntegral(Base));
        return *this;
    }

    template <typename floatingPoint, typename std::enable_if<std::is_floating_point<floatingPoint>::value>::type* = nullptr>
    UnsignedInteger& operator=(floatingPoint value) {
        VALIDITY_CHECK(value >= 0, std::invalid_argument, "UnsignedInteger operator= error: the provided floating point value = " + std::to_string(value) + " is negative. UnsignedInteger can only represent non-negative integers.")
        VALIDITY_CHECK(std::isfinite(value), std::invalid_argument, "UnsignedInteger operator= error: the provided floating point value = " + std::to_string(value) + " is not finite.")
        if (length = 0, capacity < (std::numeric_limits<floatingPoint>::max_exponent10 + 7) >> 3)
            delete[] digits, digits = new std::uint32_t[capacity = (std::numeric_limits<floatingPoint>::max_exponent10 + 7) >> 3];
        while (digits[length++] = std::uint32_t(std::fmod(value, Base)), (value = std::floor(value / Base)));
        return *this;
    }

    UnsignedInteger& operator=(const char* value) {
        VALIDITY_CHECK(value, std::invalid_argument, "UnsignedInteger operator= error: the provided C-style string is a null pointer.")
        const std::uint32_t stringLength = std::uint32_t(std::strlen(value));
        VALIDITY_CHECK(stringLength, std::invalid_argument, "UnsignedInteger operator= error: the provided C-style string is empty. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        VALIDITY_CHECK(std::all_of(value, value + stringLength, [](char digit) -> bool { return std::isdigit(digit); }), std::invalid_argument, "UnsignedInteger operator= error: the provided C-style string value = \"" + std::string(value) + "\" contains non-digit characters. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        if (capacity < (length = (stringLength + 7) >> 3))
            delete[] digits, digits = new std::uint32_t[capacity = length];
        return construct(value, stringLength), *this;
    }

    UnsignedInteger& operator=(const std::string& value) {
        VALIDITY_CHECK(value.size(), std::invalid_argument, "UnsignedInteger operator= error: the provided string is empty. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        VALIDITY_CHECK(std::all_of(value.begin(), value.end(), [](char digit) -> bool { return std::isdigit(digit); }), std::invalid_argument, "UnsignedInteger operator= error: the provided string value = \"" + value + "\" contains non-digit characters. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        if (capacity < (length = std::uint32_t(value.size() + 7) >> 3))
            delete[] digits, digits = new std::uint32_t[capacity = length];
        return construct(value.data(), std::uint32_t(value.size())), *this;
    }

    friend std::istream& operator>>(std::istream& stream, UnsignedInteger& destination) {
        std::string buffer;
        return stream >> buffer, destination = buffer, stream;
    }

    friend std::ostream& operator<<(std::ostream& stream, const UnsignedInteger& source) {
        return stream << static_cast<const char*>(source);
    }

    template <typename unsignedIntegral, typename std::enable_if<std::is_unsigned<unsignedIntegral>::value>::type* = nullptr>
    operator unsignedIntegral() const {
        unsignedIntegral result = 0;
        for (std::uint32_t* i = digits + length; i != digits; result = result * unsignedIntegral(Base) + unsignedIntegral(*--i));
        return result;
    }

    template <typename signedIntegral, typename std::enable_if<std::is_signed<signedIntegral>::value && !std::is_floating_point<signedIntegral>::value>::type* = nullptr>
    operator signedIntegral() const {
        signedIntegral result = 0;
        for (std::uint32_t* i = digits + length; i != digits; result = result * signedIntegral(Base) + signedIntegral(*--i));
        return result;
    }

    template <typename floatingPoint, typename std::enable_if<std::is_floating_point<floatingPoint>::value>::type* = nullptr>
    operator floatingPoint() const {
        floatingPoint result = 0;
        for (std::uint32_t* i = digits + length; i != digits; result = result * floatingPoint(Base) + floatingPoint(*--i));
        return result;
    }

    operator const char*() const {
        thread_local char* result = nullptr;
        thread_local std::uint32_t resultLength = 0;
        if (!length) {
            if (resultLength < 2)
                delete[] result, result = new char[resultLength = 2];
            return *reinterpret_cast<std::uint16_t*>(result) = 48 << 8, result;
        }
        if (resultLength < (length << 3 | 7))
            delete[] result, result = new char[resultLength = length << 3 | 7];
        char* resultPointer = result + 8;
        std::uint32_t* i = digits + length, numberLength = 0;
        for (std::uint32_t number = *--i; *--resultPointer = char(48 | number % 10), ++numberLength, number /= 10; );
        for (std::memmove(result, resultPointer, numberLength), resultPointer = result + numberLength; i-- != digits; std::memcpy(resultPointer, detail::O(*i / 10000), 4), resultPointer += 4, std::memcpy(resultPointer, detail::O(*i % 10000), 4), resultPointer += 4);
        return *resultPointer = 0, result;
    }

    operator std::string() const {
        if (!length)
            return "0";
        std::uint32_t* i = digits + length;
        std::string result = std::to_string(*--i);
        for (result.reserve(length << 3); i-- != digits; result.append(detail::O(*i / 10000), 4), result.append(detail::O(*i % 10000), 4));
        return result;
    }

    operator bool() const {
        return length != 1 || *digits;
    }

#if __cplusplus >= 202002L
    std::strong_ordering operator<=>(const UnsignedInteger& other) const {
        return length == other.length ? reverseCompare(other) <=> 0 : length <=> other.length;
    }
#endif

    bool operator==(const UnsignedInteger& other) const {
        return length == other.length && !std::memcmp(digits, other.digits, length << 2);
    }

    bool operator!=(const UnsignedInteger& other) const {
        return length != other.length || std::memcmp(digits, other.digits, length << 2);
    }

    bool operator<(const UnsignedInteger& other) const {
        return length < other.length || (length == other.length && reverseCompare(other) < 0);
    }

    bool operator>(const UnsignedInteger& other) const {
        return length > other.length || (length == other.length && reverseCompare(other) > 0);
    }

    bool operator<=(const UnsignedInteger& other) const {
        return length < other.length || (length == other.length && reverseCompare(other) <= 0);
    }

    bool operator>=(const UnsignedInteger& other) const {
        return length > other.length || (length == other.length && reverseCompare(other) >= 0);
    }

    UnsignedInteger& operator+=(const UnsignedInteger& other) {
        if (length <= other.length) {
            const std::uint32_t oldLength = length;
            resize(other.length + 1), std::memset(digits + oldLength, 0, (length - oldLength) << 2);
        }
        std::uint32_t *thisDigit = digits, *thisEnd = digits + length - 1;
        for (std::uint32_t *otherDigit = other.digits, *otherEnd = other.digits + other.length; otherDigit != otherEnd; ++thisDigit, ++otherDigit)
            if ((*thisDigit += *otherDigit) >= Base)
                *thisDigit -= Base, ++*(thisDigit + 1);
        for (; thisDigit != thisEnd && *thisDigit >= Base; *thisDigit -= Base, ++*++thisDigit);
        for (; length > 1 && !digits[length - 1]; --length);
        return *this;
    }

    UnsignedInteger operator+(const UnsignedInteger& other) const {
        return UnsignedInteger(*this) += other;
    }

    UnsignedInteger& operator++() {
        std::uint32_t *thisDigit = digits, *thisEnd = digits + length - 1;
        for (++*thisDigit; thisDigit != thisEnd && *thisDigit >= Base; *thisDigit -= Base, ++*++thisDigit);
        if (thisDigit == thisEnd && *thisDigit >= Base)
            resize(length + 1), digits[length - 2] -= Base, digits[length - 1] = 1;
        return *this;
    }

    UnsignedInteger operator++(int) {
        UnsignedInteger result = *this;
        return ++*this, result;
    }

    UnsignedInteger& operator-=(const UnsignedInteger& other) {
        VALIDITY_CHECK(*this >= other, std::invalid_argument, "UnsignedInteger subtraction error: Attempted to subtract a larger UnsignedInteger \"" + other.operator std::string() + "\" from a smaller one \"" + operator std::string() + "\".")
        std::uint32_t *thisDigit = digits, *thisEnd = digits + length;
        for (std::uint32_t *otherDigit = other.digits, *otherEnd = other.digits + other.length; otherDigit != otherEnd; ++thisDigit, ++otherDigit)
            if ((*thisDigit -= *otherDigit) >= Base)
                *thisDigit += Base, --*(thisDigit + 1);
        for (; thisDigit != thisEnd && *thisDigit >= Base; *thisDigit += Base, --*++thisDigit);
        for (; length > 1 && !digits[length - 1]; --length);
        return *this;
    }

    UnsignedInteger operator-(const UnsignedInteger& other) const {
        return UnsignedInteger(*this) -= other;
    }

    UnsignedInteger& operator--() {
        VALIDITY_CHECK(bool(*this), std::invalid_argument, "UnsignedInteger subtraction error: Attempted to subtract a larger UnsignedInteger \"1\" from a smaller one \"0\".")
        std::uint32_t *thisDigit = digits, *thisEnd = digits + length - 1;
        for (--*thisDigit; thisDigit != thisEnd && *thisDigit >= Base; *thisDigit += Base, --*++thisDigit);
        return *this;
    }

    UnsignedInteger operator--(int) {
        UnsignedInteger result = *this;
        return --*this, result;
    }

    UnsignedInteger& operator*=(const UnsignedInteger& other) {
        if (length < BruteforceThreshold || other.length < BruteforceThreshold) {
            UnsignedInteger result(length + other.length - 1, length + other.length);
            std::uint64_t carry = 0;
            for (std::uint32_t i = 0; i != result.length; result.digits[i++] = std::uint32_t(carry % Base), carry /= Base)
                for (std::uint32_t j = (i >= length ? i - length + 1 : 0); j <= i && j < other.length; ++j)
                    carry += std::uint64_t(digits[i - j]) * other.digits[j];
            if (carry)
                result.digits[result.length] = std::uint32_t(carry), ++result.length;
            for (; result.length > 1 && !result.digits[result.length - 1]; --result.length);
            return *this = std::move(result);
        }
        VALIDITY_CHECK(length <= TransformLimit, std::invalid_argument, "UnsignedInteger multiplication error: left operand length (" + std::to_string(length) + ") exceeds Transform limit (" + std::to_string(TransformLimit) + ").")
        VALIDITY_CHECK(other.length <= TransformLimit, std::invalid_argument, "UnsignedInteger multiplication error: right operand length (" + std::to_string(other.length) + ") exceeds Transform limit (" + std::to_string(TransformLimit) + ").")
        thread_local double *firstArray = nullptr, *secondArray = nullptr;
        thread_local std::uint32_t allocatedSize = 0;
        const std::uint32_t resultLength = length + other.length, transformLength = 2u << std::__lg(resultLength - 1);
        if (allocatedSize < transformLength << 1)
            delete[] firstArray, delete[] secondArray, firstArray = new double[transformLength << 1](), secondArray = new double[transformLength << 1](), allocatedSize = transformLength << 1;
        std::memset(firstArray, 0, transformLength << 4), std::memset(secondArray, 0, transformLength << 4);
        for (std::uint32_t i = 0; i != length; firstArray[i << 1] = digits[i] % 10000u, firstArray[i << 1 | 1] = digits[i] / 10000u, ++i);
        for (std::uint32_t i = 0; i != other.length; secondArray[i << 1] = other.digits[i] % 10000u, secondArray[i << 1 | 1] = other.digits[i] / 10000u, ++i);
        detail::T.resize(transformLength), detail::T.decimationInFrequency(reinterpret_cast<__m128d*>(firstArray), transformLength), detail::T.decimationInFrequency(reinterpret_cast<__m128d*>(secondArray), transformLength);
        detail::T.frequencyDomainPointwiseMultiply(reinterpret_cast<__m128d*>(firstArray), reinterpret_cast<__m128d*>(secondArray), transformLength), detail::T.decimationInTime(reinterpret_cast<__m128d*>(firstArray), transformLength);
        UnsignedInteger result(resultLength, resultLength);
        std::uint64_t carry = 0;
        for (std::uint32_t i = 0; i < resultLength; ++i) {
            carry += std::uint64_t(std::int64_t(firstArray[i << 1] + 0.5) + std::int64_t(firstArray[i << 1 | 1] + 0.5) * 10000);
            result.digits[i] = std::uint32_t(carry % Base), carry /= Base;
        }
        for (; carry && result.length < result.capacity; result.digits[result.length++] = std::uint32_t(carry % Base), carry /= Base)
            if (result.length == result.capacity)
                result.resize(result.capacity << 1);
        for (; result.length > 1 && !result.digits[result.length - 1]; --result.length);
        return *this = std::move(result);
    }

    UnsignedInteger operator*(const UnsignedInteger& other) const {
        return UnsignedInteger(*this) *= other;
    }

    UnsignedInteger& operator/=(const UnsignedInteger& other) {
        VALIDITY_CHECK(bool(other), std::invalid_argument, "UnsignedInteger division error: divisor is zero.")
        return *this = std::move(divisionAndModulus(other).first);
    }

    UnsignedInteger operator/(const UnsignedInteger& other) const {
        return UnsignedInteger(*this) /= other;
    }

    UnsignedInteger& operator%=(const UnsignedInteger& other) {
        VALIDITY_CHECK(bool(other), std::invalid_argument, "UnsignedInteger modulus error: modulus is zero.")
        return *this = std::move(divisionAndModulus(other).second);
    }

    UnsignedInteger operator%(const UnsignedInteger& other) const {
        return UnsignedInteger(*this) %= other;
    }
};

UnsignedInteger operator""_UI(const char* literal, std::size_t) {
    return UnsignedInteger(literal);
}

class SignedInteger {
public:
    UnsignedInteger absolute;
    bool sign;

public:
    SignedInteger(const UnsignedInteger& initialAbsolute, bool initialSign) : absolute(initialAbsolute), sign(initialSign) {}

public:
    SignedInteger() : absolute(), sign() {}

    SignedInteger(const SignedInteger& other) : absolute(other.absolute), sign(other.sign) {}

    SignedInteger(SignedInteger&& other) : absolute(std::move(other.absolute)), sign(other.sign) {}

    SignedInteger(const UnsignedInteger& other) : absolute(other), sign() {}

    template <typename unsignedIntegral, typename std::enable_if<std::is_unsigned<unsignedIntegral>::value>::type* = nullptr>
    SignedInteger(unsignedIntegral value) : absolute(value), sign() {}

    template <typename signedIntegral, typename std::enable_if<std::is_signed<signedIntegral>::value && !std::is_floating_point<signedIntegral>::value>::type* = nullptr>
    SignedInteger(signedIntegral value) : absolute(std::abs(value)), sign(value < 0) {}

    template <typename floatingPoint, typename std::enable_if<std::is_floating_point<floatingPoint>::value>::type* = nullptr>
    SignedInteger(floatingPoint value) : absolute(std::abs(value)), sign(value < 0) {}

    SignedInteger(const char* value) {
        VALIDITY_CHECK(value, std::invalid_argument, "SignedInteger constructor error: the provided C-style string is a null pointer.")
        VALIDITY_CHECK(*value, std::invalid_argument, "SignedInteger constructor error: the provided C-style string is empty. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        VALIDITY_CHECK(std::all_of(value + (*value == '-'), value + std::strlen(value), [](char digit) -> bool { return std::isdigit(digit); }), std::invalid_argument, "SignedInteger constructor error: the provided C-style string value = \"" + std::string(value) + "\" contains non-digit characters. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        absolute = value + (sign = *value == '-');
    }

    SignedInteger(const std::string& value) {
        VALIDITY_CHECK(value.size(), std::invalid_argument, "SignedInteger operator= error: the provided string is empty. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        VALIDITY_CHECK(std::all_of(value.begin() + (value.front() == '-'), value.end(), [](char digit) -> bool { return std::isdigit(digit); }), std::invalid_argument, "SignedInteger operator= error: the provided string value = \"" + value + "\" contains non-digit characters. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        absolute = value.data() + (sign = value.front() == '-');
    }

    ~SignedInteger() = default;

    SignedInteger& operator=(const SignedInteger& other) {
        return absolute = other.absolute, sign = other.sign, *this;
    }

    SignedInteger& operator=(SignedInteger&& other) {
        return absolute = std::move(other.absolute), sign = other.sign, *this;
    }

    SignedInteger& operator=(const UnsignedInteger& other) {
        return absolute = other, sign = false, *this;
    }

    template <typename unsignedIntegral, typename std::enable_if<std::is_unsigned<unsignedIntegral>::value>::type* = nullptr>
    SignedInteger& operator=(unsignedIntegral value) {
        return absolute = value, sign = false, *this;
    }

    template <typename signedIntegral, typename std::enable_if<std::is_signed<signedIntegral>::value && !std::is_floating_point<signedIntegral>::value>::type* = nullptr>
    SignedInteger& operator=(signedIntegral value) {
        return absolute = std::abs(value), sign = value < 0, *this;
    }

    template <typename floatingPoint, typename std::enable_if<std::is_floating_point<floatingPoint>::value>::type* = nullptr>
    SignedInteger& operator=(floatingPoint value) {
        return absolute = std::abs(value), sign = value < 0, *this;
    }

    SignedInteger& operator=(const char* value) {
        VALIDITY_CHECK(value, std::invalid_argument, "SignedInteger operator= error: the provided C-style string is a null pointer.")
        VALIDITY_CHECK(*value, std::invalid_argument, "SignedInteger operator= error: the provided C-style string is empty. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        VALIDITY_CHECK(std::all_of(value + (*value == '-'), value + std::strlen(value), [](char digit) -> bool { return std::isdigit(digit); }), std::invalid_argument, "SignedInteger operator= error: the provided C-style string value = \"" + std::string(value) + "\" contains non-digit characters. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        return absolute = value + (sign = *value == '-'), *this;
    }

    SignedInteger& operator=(const std::string& value) {
        VALIDITY_CHECK(value.size(), std::invalid_argument, "SignedInteger operator= error: the provided string is empty. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        VALIDITY_CHECK(std::all_of(value.begin() + (value.front() == '-'), value.end(), [](char digit) -> bool { return std::isdigit(digit); }), std::invalid_argument, "SignedInteger operator= error: the provided string value = \"" + value + "\" contains non-digit characters. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        return absolute = value.data() + (sign = value.front() == '-'), *this;
    }

    friend std::istream& operator>>(std::istream& stream, SignedInteger& destination) {
        std::string buffer;
        return stream >> buffer, destination = buffer, stream;
    }

    friend std::ostream& operator<<(std::ostream& stream, const SignedInteger& source) {
        return stream << static_cast<const char*>(source);
    }

    template <typename unsignedIntegral, typename std::enable_if<std::is_unsigned<unsignedIntegral>::value>::type* = nullptr>
    operator unsignedIntegral() const {
        VALIDITY_CHECK(!sign, std::invalid_argument, "SignedInteger conversion error: Connot convert negative SignedInteger to unsigned type.")
        return unsignedIntegral(absolute);
    }

    template <typename signedIntegral, typename std::enable_if<std::is_signed<signedIntegral>::value && !std::is_floating_point<signedIntegral>::value>::type* = nullptr>
    operator signedIntegral() const {
        return sign ? -signedIntegral(absolute) : signedIntegral(absolute);
    }

    template <typename floatingPoint, typename std::enable_if<std::is_floating_point<floatingPoint>::value>::type* = nullptr>
    operator floatingPoint() const {
        return sign ? -floatingPoint(absolute) : floatingPoint(absolute);
    }

    operator const char*() const {
        thread_local char* result = nullptr;
        thread_local std::uint32_t resultLength = 0;
        const char* absoluteString = static_cast<const char*>(absolute);
        std::uint32_t absoluteLength = std::uint32_t(std::strlen(absoluteString));
        if (resultLength < absoluteLength + 5)
            delete[] result, result = new char[resultLength = absoluteLength + 5];
        char* digitStart = sign && bool(absolute) ? (*result = '-', result + 1) : result;
        std::memcpy(digitStart, absoluteString, absoluteLength), *(digitStart + absoluteLength) = 0;
        return result;
    }

    operator std::string() const {
        return sign ? "-" + absolute.operator std::string() : absolute.operator std::string();
    }

    operator bool() const {
        return bool(absolute);
    }

#if __cplusplus >= 202002L
    std::strong_ordering operator<=>(const SignedInteger& other) const {
        return sign != other.sign ? other.sign <=> sign : (sign ? other.absolute <=> absolute : absolute <=> other.absolute);
    }
#endif

    bool operator==(const SignedInteger& other) const {
        return sign == other.sign && absolute == other.absolute;
    }

    bool operator!=(const SignedInteger& other) const {
        return sign != other.sign || absolute != other.absolute;
    }

    bool operator<(const SignedInteger& other) const {
        return sign ? !other.sign || absolute > other.absolute : !other.sign && absolute < other.absolute;
    }

    bool operator>(const SignedInteger& other) const {
        return sign ? other.sign && absolute < other.absolute : other.sign || absolute > other.absolute;
    }

    bool operator<=(const SignedInteger& other) const {
        return sign ? !other.sign || absolute >= other.absolute : !other.sign && absolute <= other.absolute;
    }

    bool operator>=(const SignedInteger& other) const {
        return sign ? other.sign && absolute <= other.absolute : other.sign || absolute >= other.absolute;
    }

    SignedInteger& operator+=(const SignedInteger& other) {
        return sign == other.sign ? absolute += other.absolute : (absolute < other.absolute ? sign = !sign, absolute = other.absolute - absolute : absolute -= other.absolute), *this;
    }

    SignedInteger operator+(const SignedInteger& other) const {
        return SignedInteger(*this) += other;
    }

    SignedInteger& operator-=(const SignedInteger& other) {
        return sign != other.sign ? absolute += other.absolute : (absolute < other.absolute ? sign = !sign, absolute = other.absolute - absolute : absolute -= other.absolute), *this;
    }

    SignedInteger operator-(const SignedInteger& other) const {
        return SignedInteger(*this) -= other;
    }

    SignedInteger& operator*=(const SignedInteger& other) {
        return absolute *= other.absolute, sign ^= other.sign, *this;
    }

    SignedInteger operator*(const SignedInteger& other) const {
        return SignedInteger(*this) *= other;
    }

    SignedInteger& operator/=(const SignedInteger& other) {
        VALIDITY_CHECK(bool(other), std::invalid_argument, "SignedInteger division error: divisor is zero.")
        return absolute /= other.absolute, sign ^= other.sign, *this;
    }

    SignedInteger operator/(const SignedInteger& other) const {
        return SignedInteger(*this) /= other;
    }

    SignedInteger& operator%=(const SignedInteger& other) {
        VALIDITY_CHECK(bool(other), std::invalid_argument, "SignedInteger modulus error: modulus is zero.")
        return absolute %= other.absolute, *this;
    }

    SignedInteger operator%(const SignedInteger& other) const {
        return SignedInteger(*this) %= other;
    }
};

SignedInteger operator""_SI(const char* literal, std::size_t) {
    return SignedInteger(literal);
}

#undef VALIDITY_CHECK

#endif

#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif

namespace {
    static char oBuffer[32 << 20], *oCursor = oBuffer;

    static const char* iCursor = []() -> const char* {
#ifdef __linux__
        struct stat status;
        fstat(STDIN_FILENO, &status);
        return reinterpret_cast<const char*>(mmap(nullptr, status.st_size, PROT_READ, MAP_PRIVATE, STDIN_FILENO, 0));
#else
        static char iBuffer[16 << 20];
        std::fread(iBuffer, sizeof(char), sizeof(iBuffer) / sizeof(char), stdin);
        return iBuffer;
#endif
    }();

    void readFromCursor(UnsignedInteger& value) {
        const char* start = iCursor;
        while (true) {
            std::uint64_t data;
            std::memcpy(&data, iCursor, sizeof(data));
            if ((data = (~data) & (data - 0x2121212121212121) & 0x8080808080808080)) {
                const std::uint32_t stringLength = std::uint32_t((iCursor += __builtin_ctzll(data) / int(sizeof(std::uint64_t))) - start);
                if (value.capacity < (value.length = (stringLength + 7) >> 3))
                    delete[] value.digits, value.digits = new std::uint32_t[value.capacity = value.length];
                value.construct(start, stringLength), ++iCursor;
                return;
            }
            iCursor += sizeof(std::uint64_t);
        }
    }
}

int main() {
    std::cin.tie(nullptr)->sync_with_stdio(false);
    static std::uint32_t t;
    static SignedInteger a, b;
    ~detail::I(iCursor) && (t = t * 100 + detail::I(iCursor), iCursor += 2), ~detail::I(iCursor) && (t = t * 100 + detail::I(iCursor), iCursor += 2), ~detail::I(iCursor) && (t = t * 100 + detail::I(iCursor), iCursor += 2);
    std::isdigit(*iCursor) && (t = t * 10 + (*iCursor++ & 15)), ++iCursor;
    do {
        iCursor += (a.sign = *iCursor == 45), readFromCursor(a.absolute), iCursor += (b.sign = *iCursor == 45), readFromCursor(b.absolute);
        std::cout << (a += b) << '\n';
    } while (--t);
    return 0;
}