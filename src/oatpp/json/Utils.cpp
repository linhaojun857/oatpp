/***************************************************************************
 *
 * Project         _____    __   ____   _      _
 *                (  _  )  /__\ (_  _)_| |_  _| |_
 *                 )(_)(  /(__)\  )( (_   _)(_   _)
 *                (_____)(__)(__)(__)  |_|    |_|
 *
 *
 * Copyright 2018-present, Leonid Stryzhevskyi <lganzzzo@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************************/

#include "Utils.hpp"

#include "oatpp/encoding/Unicode.hpp"
#include "oatpp/encoding/Hex.hpp"

#include "oatpp/data/type/Primitive.hpp"
#include "oatpp/data/type/Enum.hpp"
#include "oatpp/data/type/Any.hpp"
#include "oatpp/data/type/Tree.hpp"
#include "oatpp/data/type/List.hpp"
#include "oatpp/data/type/Vector.hpp"
#include "oatpp/data/type/PairList.hpp"
#include "oatpp/data/type/UnorderedMap.hpp"
#include "oatpp/data/type/UnorderedSet.hpp"

#include <cstdio>
#include <cmath>
#include <cstring>
#include <limits>

namespace oatpp { namespace json{

// ============================================================================
// Lookup tables (yyjson-style, compile-time constant)
// ============================================================================

const uint8_t CHAR_CLASS[256] = {
  /* 0x00-0x0F */ CC_CONTROL, CC_CONTROL, CC_CONTROL, CC_CONTROL, CC_CONTROL, CC_CONTROL, CC_CONTROL, CC_CONTROL,
                  CC_CONTROL, CC_WS|CC_CONTROL, CC_WS|CC_CONTROL, CC_CONTROL, CC_WS|CC_CONTROL, CC_WS|CC_CONTROL, CC_CONTROL, CC_CONTROL,
  /* 0x10-0x1F */ CC_CONTROL, CC_CONTROL, CC_CONTROL, CC_CONTROL, CC_CONTROL, CC_CONTROL, CC_CONTROL, CC_CONTROL,
                  CC_CONTROL, CC_CONTROL, CC_CONTROL, CC_CONTROL, CC_CONTROL, CC_CONTROL, CC_CONTROL, CC_CONTROL,
  /* 0x20-0x2F */ CC_WS,   0, CC_QUOTE|CC_ESCAPE|CC_STRUCT, 0, 0, 0, 0, 0,
                  0, 0, 0, 0, CC_STRUCT, 0, 0, 0,
  /* 0x30-0x3F */ CC_DIGIT|CC_HEX, CC_DIGIT|CC_HEX, CC_DIGIT|CC_HEX, CC_DIGIT|CC_HEX,
                  CC_DIGIT|CC_HEX, CC_DIGIT|CC_HEX, CC_DIGIT|CC_HEX, CC_DIGIT|CC_HEX,
                  CC_DIGIT|CC_HEX, CC_DIGIT|CC_HEX, CC_STRUCT, 0, 0, 0, 0, 0,
  /* 0x40-0x4F */ 0, CC_HEX, CC_HEX, CC_HEX, CC_HEX, CC_HEX, CC_HEX, 0,
                  0, 0, 0, 0, 0, 0, 0, 0,
  /* 0x50-0x5F */ 0, 0, 0, 0, 0, 0, 0, 0,
                  0, 0, 0, CC_STRUCT, CC_BACKSLASH|CC_ESCAPE|CC_STRUCT, CC_STRUCT, 0, 0,
  /* 0x60-0x6F */ 0, CC_HEX, CC_HEX, CC_HEX, CC_HEX, CC_HEX, CC_HEX, 0,
                  0, 0, 0, 0, 0, 0, 0, 0,
  /* 0x70-0x7F */ 0, 0, 0, 0, 0, 0, 0, 0,
                  0, 0, 0, CC_STRUCT, 0, CC_STRUCT, 0, 0,
  /* 0x80-0xFF: multi-byte UTF-8 lead bytes — all pass through as-is */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

// ============================================================================
// Fast string classification
// ============================================================================

// Detect bytes that need JSON escaping within an 8-byte word.
// Returns non-zero if any byte is 0x00-0x1F (control), 0x22 ('"'), or 0x5C ('\\').
static inline uint64_t hasEscapeBytesInWord(uint64_t word) {
  // Control chars (0x00-0x1F): (word - 0x20) sets bit 7 for bytes < 0x20
  uint64_t ctrl = ((word - 0x2020202020202020ULL) & ~word) & 0x8080808080808080ULL;
  // Byte == 0x22 ('"')
  uint64_t x = word ^ 0x2222222222222222ULL;
  uint64_t quote = ((x - 0x0101010101010101ULL) & ~x) & 0x8080808080808080ULL;
  // Byte == 0x5C ('\\')
  x = word ^ 0x5C5C5C5C5C5C5C5CULL;
  uint64_t backslash = ((x - 0x0101010101010101ULL) & ~x) & 0x8080808080808080ULL;
  return ctrl | quote | backslash;
}

bool Utils::isSimpleString(const char* data, v_buff_size size) {
  v_buff_size i = 0;

  // Process 8 bytes at a time using word-level bitwise checks
  while (i + 8 <= size) {
    uint64_t word;
    std::memcpy(&word, data + i, 8);
    if (hasEscapeBytesInWord(word)) {
      // One or more bytes in this word need escaping — fall back to
      // byte-at-a-time scan to find the exact culprit
      for (int j = 0; j < 8; j++) {
        uint8_t cls = CHAR_CLASS[static_cast<uint8_t>(data[i + j])];
        if (cls & (CC_ESCAPE | CC_CONTROL)) return false;
      }
    }
    i += 8;
  }

  // Process remaining bytes
  for (; i < size; i++) {
    uint8_t cls = CHAR_CLASS[static_cast<uint8_t>(data[i])];
    if (cls & (CC_ESCAPE | CC_CONTROL)) return false;
  }
  return true;
}

// ============================================================================
// Fast integer-to-string: Meyer's algorithm with DIGIT_PAIRS lookup
// ============================================================================

namespace {
  // Pre-computed two-digit string representations: "00".."99"
  constexpr char DIGIT_PAIRS[200] = {
    '0','0','0','1','0','2','0','3','0','4','0','5','0','6','0','7','0','8','0','9',
    '1','0','1','1','1','2','1','3','1','4','1','5','1','6','1','7','1','8','1','9',
    '2','0','2','1','2','2','2','3','2','4','2','5','2','6','2','7','2','8','2','9',
    '3','0','3','1','3','2','3','3','3','4','3','5','3','6','3','7','3','8','3','9',
    '4','0','4','1','4','2','4','3','4','4','4','5','4','6','4','7','4','8','4','9',
    '5','0','5','1','5','2','5','3','5','4','5','5','5','6','5','7','5','8','5','9',
    '6','0','6','1','6','2','6','3','6','4','6','5','6','6','6','7','6','8','6','9',
    '7','0','7','1','7','2','7','3','7','4','7','5','7','6','7','7','7','8','7','9',
    '8','0','8','1','8','2','8','3','8','4','8','5','8','6','8','7','8','8','8','9',
    '9','0','9','1','9','2','9','3','9','4','9','5','9','6','9','7','9','8','9','9',
  };

  // Write uint64 to buffer (right-to-left, two digits at a time)
  // Returns pointer to first character.
  inline char* writeUInt64(uint64_t v, char* bufEnd) {
    while (v >= 100) {
      uint32_t idx = static_cast<uint32_t>(v % 100) * 2;
      v /= 100;
      *--bufEnd = DIGIT_PAIRS[idx + 1];
      *--bufEnd = DIGIT_PAIRS[idx];
    }
    if (v < 10) {
      *--bufEnd = static_cast<char>('0' + v);
    } else {
      uint32_t idx = static_cast<uint32_t>(v) * 2;
      *--bufEnd = DIGIT_PAIRS[idx + 1];
      *--bufEnd = DIGIT_PAIRS[idx];
    }
    return bufEnd;
  }
} // anonymous namespace

v_buff_size Utils::int64ToChars(v_int64 value, char* buffer) noexcept {
  if (value == 0) {
    buffer[0] = '0';
    return 1;
  }

  char* p = buffer;

  // Handle INT64_MIN (cannot negate)
  if (value == std::numeric_limits<v_int64>::min()) {
    // "-9223372036854775808"
    std::memcpy(p, "-9223372036854775808", 20);
    return 20;
  }

  uint64_t absVal;
  if (value < 0) {
    *p++ = '-';
    absVal = static_cast<uint64_t>(-value);
  } else {
    absVal = static_cast<uint64_t>(value);
  }

  // Max 20 digits for uint64. Write from right to left.
  char bufEnd[24];
  char* start = writeUInt64(absVal, bufEnd + 24);
  v_buff_size len = static_cast<v_buff_size>((bufEnd + 24) - start);
  std::memcpy(p, start, len);
  return static_cast<v_buff_size>(p - buffer) + len;
}

v_buff_size Utils::uint64ToChars(v_uint64 value, char* buffer) noexcept {
  if (value == 0) {
    buffer[0] = '0';
    return 1;
  }

  char bufEnd[24];
  char* start = writeUInt64(value, bufEnd + 24);
  v_buff_size len = static_cast<v_buff_size>((bufEnd + 24) - start);
  std::memcpy(buffer, start, len);
  return len;
}

v_buff_size Utils::float64ToChars(v_float64 value, char* buffer) noexcept {
  // Handle special values
  if (std::isnan(value)) {
    // Per JSON spec, NaN is not valid; output 0.0 as safe fallback.
    std::memcpy(buffer, "0.0", 3);
    return 3;
  }
  if (std::isinf(value)) {
    if (value > 0) {
      std::memcpy(buffer, "1e999", 5);
      return 5;
    } else {
      std::memcpy(buffer, "-1e999", 6);
      return 6;
    }
  }

  // Fast path 1: if the value is a whole number within int64 range,
  // use integer formatting (common case in JSON).
  if (value >= static_cast<v_float64>(std::numeric_limits<v_int64>::min()) &&
      value <= static_cast<v_float64>(std::numeric_limits<v_int64>::max())) {
    v_float64 intPart;
    if (std::modf(value, &intPart) == 0.0) {
      return int64ToChars(static_cast<v_int64>(value), buffer);
    }
  }

  // Fast path 2: format simple decimal floats as "int.frac" instead of
  // calling snprintf. This handles common benchmark values like i * 1.5.
  // We try up to 6 decimal digits, stopping when the round-trip matches.
  v_float64 absVal = value < 0 ? -value : value;
  if (absVal >= 1e-10 && absVal < 1e14) {
    v_float64 scale = 1.0;
    for (int digits = 1; digits <= 6; digits++) {
      scale *= 10.0;
      v_float64 scaled = absVal * scale + 0.5; // round to nearest
      if (scaled > static_cast<v_float64>(std::numeric_limits<v_int64>::max()))
        break;

      v_int64 intVal = static_cast<v_int64>(scaled);
      v_float64 check = static_cast<v_float64>(intVal) / scale;
      if (check == absVal) {
        // Round-trip matches — format as integer + decimal fraction
        char* p = buffer;
        if (value < 0) *p++ = '-';

        v_int64 denominator = static_cast<v_int64>(scale);
        v_int64 integerPart = intVal / denominator;
        v_int64 fracPart = intVal % denominator;

        auto n = Utils::int64ToChars(integerPart, p);
        p += n;

        if (fracPart > 0) {
          *p++ = '.';
          // Format fraction with leading zeros, then strip trailing zeros
          char fracBuf[24];
          auto fn = Utils::int64ToChars(fracPart + denominator, fracBuf);
          // fracBuf is "1xxxxx" — skip the leading '1'
          v_buff_size fLen = fn - 1;
          char* fStart = fracBuf + 1;
          // Strip trailing zeros
          while (fLen > 0 && fStart[fLen - 1] == '0') fLen--;
          std::memcpy(p, fStart, fLen);
          p += fLen;
        }
        return static_cast<v_buff_size>(p - buffer);
      }
    }
  }

  // Fallback: use snprintf for edge cases (very large/small exponents, etc.)
  int len = std::snprintf(buffer, 64, "%.16g", value);
  return static_cast<v_buff_size>(len > 0 ? len : 0);
}

// ============================================================================
// Fast number parsing (replaces strtol / strtod)
// ============================================================================

bool Utils::parseInt64(const char* data, v_buff_size len, v_int64& result) noexcept {
  if (len == 0) return false;

  bool negative = false;
  v_buff_size i = 0;

  if (data[0] == '-') {
    negative = true;
    i = 1;
    if (len == 1) return false;
  }

  v_uint64 acc = 0;
  // Max positive int64 is 9223372036854775807 (19 digits)
  constexpr v_uint64 MAX_POS = static_cast<v_uint64>(std::numeric_limits<v_int64>::max());
  constexpr v_uint64 OVERFLOW_LIMIT = MAX_POS / 10;

  while (i < len) {
    uint8_t c = static_cast<uint8_t>(data[i]);
    if (!(CHAR_CLASS[c] & CC_DIGIT)) return false;
    v_uint64 digit = c - '0';

    if (acc > OVERFLOW_LIMIT) return false;  // overflow
    if (acc == OVERFLOW_LIMIT) {
      v_uint64 maxDigit = negative ? (MAX_POS % 10 + 1) : (MAX_POS % 10);
      if (digit > maxDigit) return false;
    }

    acc = acc * 10 + digit;
    i++;
  }

  if (negative) {
    if (acc > MAX_POS + 1) return false;  // INT64_MIN case
    result = -static_cast<v_int64>(acc);
  } else {
    if (acc > MAX_POS) return false;
    result = static_cast<v_int64>(acc);
  }
  return true;
}

bool Utils::parseUInt64(const char* data, v_buff_size len, v_uint64& result) noexcept {
  if (len == 0) return false;

  v_uint64 acc = 0;
  constexpr v_uint64 MAX_UINT64 = std::numeric_limits<v_uint64>::max();
  constexpr v_uint64 OVERFLOW_LIMIT = MAX_UINT64 / 10;

  for (v_buff_size i = 0; i < len; i++) {
    uint8_t c = static_cast<uint8_t>(data[i]);
    if (!(CHAR_CLASS[c] & CC_DIGIT)) return false;
    v_uint64 digit = c - '0';

    if (acc > OVERFLOW_LIMIT) return false;
    if (acc == OVERFLOW_LIMIT && digit > MAX_UINT64 % 10) return false;

    acc = acc * 10 + digit;
  }

  result = acc;
  return true;
}

bool Utils::parseFloat64(const char* data, v_buff_size len, v_float64& result) noexcept {
  if (len == 0) return false;

  // Fast path 1: try integer parse first (most JSON numbers are integers)
  v_int64 intVal;
  if (parseInt64(data, len, intVal)) {
    bool isPureInt = true;
    for (v_buff_size j = 0; j < len; j++) {
      if (data[j] == '.' || data[j] == 'e' || data[j] == 'E') { isPureInt = false; break; }
    }
    if (isPureInt) {
      result = static_cast<v_float64>(intVal);
      return true;
    }
  }

  // Fast path 2: manual parsing without strtod.
  // Avoids libc strtod overhead (locale, error handling, etc.).
  v_buff_size i = 0;

  // Sign
  v_float64 sign = 1.0;
  if (data[i] == '-') { sign = -1.0; i++; }
  else if (data[i] == '+') { i++; }

  if (i >= len) return false;

  // Integer part
  v_float64 value = 0.0;
  bool hasDigits = false;
  while (i < len && (CHAR_CLASS[static_cast<uint8_t>(data[i])] & CC_DIGIT)) {
    value = value * 10.0 + static_cast<v_float64>(data[i] - '0');
    hasDigits = true;
    i++;
  }

  // Fractional part
  if (i < len && data[i] == '.') {
    i++;
    v_float64 fracWeight = 0.1;
    while (i < len && (CHAR_CLASS[static_cast<uint8_t>(data[i])] & CC_DIGIT)) {
      value += static_cast<v_float64>(data[i] - '0') * fracWeight;
      fracWeight *= 0.1;
      hasDigits = true;
      i++;
    }
  }

  if (!hasDigits) return false;

  // Exponent part — use strtod for anything beyond trivial exponents
  // to guarantee round-trip precision for edge cases like 1.2345e30.
  if (i < len && (data[i] == 'e' || data[i] == 'E')) {
    // Fall back to strtod for numbers with exponents
    char buf[128];
    if (len < static_cast<v_buff_size>(sizeof(buf) - 1)) {
      std::memcpy(buf, data, static_cast<size_t>(len));
      buf[len] = '\0';
      char* endp = nullptr;
      result = std::strtod(buf, &endp);
      return (endp == buf + len);
    }
    // Buffer too large — rare edge case, use pow fallback
    i++;
    bool expNegative = false;
    if (i < len && data[i] == '-') { expNegative = true; i++; }
    else if (i < len && data[i] == '+') { i++; }

    v_int32 expVal = 0;
    bool hasExpDigits = false;
    while (i < len && (CHAR_CLASS[static_cast<uint8_t>(data[i])] & CC_DIGIT)) {
      expVal = expVal * 10 + (data[i] - '0');
      hasExpDigits = true;
      i++;
    }
    if (!hasExpDigits) return false;
    v_float64 expFactor = std::pow(10.0, static_cast<v_float64>(expNegative ? -expVal : expVal));
    value *= expFactor;
  }

  if (i < len) return false;

  result = sign * value;
  return true;
}

// ============================================================================
// Original private helpers (kept for backward compatibility)
// ============================================================================

v_buff_size Utils::calcEscapedStringSize(const char* data, v_buff_size size, v_buff_size& safeSize, v_uint32 flags) {
  v_buff_size result = 0;
  v_buff_size i = 0;
  safeSize = size;
  while (i < size) {
    v_char8 a = static_cast<v_char8>(data[i]);
    if(a < 32) {
      i ++;

      switch (a) {

        case '\b':
        case '\f':
        case '\n':
        case '\r':
        case '\t': result += 2; break; // '\n'

        default:
          result += 6; // '￿' - 6 chars
          break;

      }

    } else if(a < 128){
      i ++;

      switch (a) {
        case '\"':
        case '\\': result += 2; break; // '\/'

        case '/':
          result ++;
          if((flags & FLAG_ESCAPE_SOLIDUS) > 0) result ++;
          break;

        default:
          result ++;
          break;

      }

    } else {
      v_buff_size charSize = oatpp::encoding::Unicode::getUtf8CharSequenceLength(a);
      if(charSize != 0) {
        if(i + charSize > size) {
          safeSize = i;
        }
        if (!(flags & FLAG_ESCAPE_UTF8CHAR)) {
          result += charSize; // output as-is
        } else if(i + charSize <= size) {
          result += escapeUtf8CharSize(&data[i]);
        } else {
          result += charSize; // invalid char. output as-is
        }
        i += charSize;
      } else {
        // invalid char
        i ++;
        result ++;
      }
    }
  }
  return result;
}

v_buff_size Utils::calcUnescapedStringSize(const char* data, v_buff_size size, v_int64& errorCode, v_buff_size& errorPosition) {
  errorCode = 0;
  v_buff_size result = 0;
  v_buff_size i = 0;

  while (i < size) {
    v_char8 a = static_cast<v_char8>(data[i]);
    if(a == '\\'){

      if(i + 1 == size){
        errorCode = ERROR_CODE_INVALID_ESCAPED_CHAR;
        errorPosition = i;
        return 0;
      }

      v_char8 b = static_cast<v_char8>(data[i + 1]);

      if(b == '"' || b == '\\' || b == '/' || b == 'b' || b == 'f' || b == 'n' || b == 'r' || b == 't'){
        result += 1;
        i += 2;
      } else if(b == 'u'){

        if(i + 6 > size){
          errorCode = ERROR_CODE_INVALID_ESCAPED_CHAR;
          errorPosition = i;
          return 0;
        }

        if(data[i + 2] == '+') { // not JSON standard case
          if(i + 11 > size){
            errorCode = ERROR_CODE_INVALID_ESCAPED_CHAR;
            errorPosition = i;
            return 0;
          }
          v_uint32 code;
          errorCode = encoding::Hex::readUInt32(&data[i + 3], code);
          if(errorCode != 0){
            errorPosition = i + 3;
            return 0;
          }
          i += 11;
          result += encoding::Unicode::getUtf8CharSequenceLengthForCode(code);
        } else {
          v_uint16 code;
          errorCode = encoding::Hex::readUInt16(&data[i + 2], code);
          if(errorCode != 0){
            errorPosition = i + 2;
            return 0;
          }

          if(code >= 0xD800 && code <= 0xDBFF){
            if(i + 12 > size){
              errorCode = ERROR_CODE_INVALID_SURROGATE_PAIR;
              errorPosition = i;
              return 0;
            }
            v_uint16 low;
            errorCode = encoding::Hex::readUInt16(&data[i + 8], low);
            if(errorCode != 0){
              errorPosition = i + 8;
              return 0;
            }

            if(low >= 0xDC00 && low <= 0xDFFF){
              v_uint32 bigCode = static_cast<v_uint32>(encoding::Unicode::utf16SurrogatePairToCode(static_cast<v_int16>(code), static_cast<v_int16>(low)));
              i += 12;
              result += encoding::Unicode::getUtf8CharSequenceLengthForCode(bigCode);
            } else {
              errorCode = ERROR_CODE_INVALID_SURROGATE_PAIR;
              errorPosition = i;
              return 0;
            }

          } else {
            i += 6;
            result += encoding::Unicode::getUtf8CharSequenceLengthForCode(code);
          }
        }

      } else {
        errorCode = ERROR_CODE_INVALID_ESCAPED_CHAR;
        errorPosition = i;
        return 0;
      }

    } else {
      i ++;
      result ++;
    }

  }

  return result;
}

v_buff_size Utils::escapeUtf8CharSize(const char* sequence) {
  v_buff_size length;
  v_int32 code = oatpp::encoding::Unicode::encodeUtf8Char(sequence, length);
  if(code < 0x00010000) {
    return 6;
  } else if(code < 0x00200000) {
    return 12;
  } else {
    return 11;
  }
}

v_buff_size Utils::escapeUtf8Char(const char* sequence, p_char8 buffer){
  v_buff_size length;
  v_int32 code = oatpp::encoding::Unicode::encodeUtf8Char(sequence, length);
  if(code < 0x00010000) {
    buffer[0] = '\\';
    buffer[1] = 'u';
    oatpp::encoding::Hex::writeUInt16(v_uint16(code), &buffer[2]);
    return 6;
  } else if(code < 0x00200000) {
    v_int16 high;
    v_int16 low;
    oatpp::encoding::Unicode::codeToUtf16SurrogatePair(code, high, low);
    buffer[0] = '\\';
    buffer[1] = 'u';
    oatpp::encoding::Hex::writeUInt16(static_cast<v_uint16>(high), &buffer[2]);
    buffer[6] = '\\';
    buffer[7] = 'u';
    oatpp::encoding::Hex::writeUInt16(static_cast<v_uint16>(low), &buffer[8]);
    return 12;
  } else {
    buffer[0] = '\\';
    buffer[1] = 'u';
    buffer[2] = '+';
    oatpp::encoding::Hex::writeUInt32(static_cast<v_uint32>(code), &buffer[2]);
    return 11;
  }
}

// ============================================================================
// Escape / unescape string (optimized with lookup tables)
// ============================================================================

oatpp::String Utils::escapeString(const char* data, v_buff_size size, v_uint32 flags) {
  // Fast path: no escaping needed (uses CHAR_CLASS lookup table)
  if (isSimpleString(data, size)) {
    // Still need to check solius flag and '/' characters
    if (!(flags & FLAG_ESCAPE_SOLIDUS) || std::memchr(data, '/', size) == nullptr) {
      return String(data, size);
    }
  }

  v_buff_size safeSize;
  v_buff_size escapedSize = calcEscapedStringSize(data, size, safeSize, flags);
  if(escapedSize == size) {
    return String(data, size);
  }
  String result(escapedSize);
  auto resultData = reinterpret_cast<p_char8>(result->data());
  v_buff_size pos = 0;

  {
    v_buff_size i = 0;
    while (i < safeSize) {
      v_char8 a = static_cast<v_char8>(data[i]);
      if (a < 32) {

        switch (a) {

          case '\b': resultData[pos] = '\\'; resultData[pos + 1] = 'b'; pos += 2; break;
          case '\f': resultData[pos] = '\\'; resultData[pos + 1] = 'f'; pos += 2; break;
          case '\n': resultData[pos] = '\\'; resultData[pos + 1] = 'n'; pos += 2; break;
          case '\r': resultData[pos] = '\\'; resultData[pos + 1] = 'r'; pos += 2; break;
          case '\t': resultData[pos] = '\\'; resultData[pos + 1] = 't'; pos += 2; break;

          default:
            resultData[pos] = '\\';
            resultData[pos + 1] = 'u';
            oatpp::encoding::Hex::writeUInt16(a, &resultData[pos + 2]);
            pos += 6;
            break;

        }

        i++;

      }
      else if (a < 128) {

        switch (a) {
          case '\"': resultData[pos] = '\\'; resultData[pos + 1] = '"'; pos += 2; break;
          case '\\': resultData[pos] = '\\'; resultData[pos + 1] = '\\'; pos += 2; break;

          case '/':
            if((flags & FLAG_ESCAPE_SOLIDUS) > 0) {
              resultData[pos] = '\\';
              resultData[pos + 1] = '/';
              pos += 2;
            } else {
              resultData[pos] = static_cast<v_char8>(data[i]);
              pos++;
            }
            break;

          default:
            resultData[pos] = static_cast<v_char8>(data[i]);
            pos++;
            break;

        }

        i++;
      }
      else {
        v_buff_size charSize = oatpp::encoding::Unicode::getUtf8CharSequenceLength(a);
        if (charSize != 0) {
          if (!(flags & FLAG_ESCAPE_UTF8CHAR)) {
            std::memcpy(reinterpret_cast<void*>(&resultData[pos]), reinterpret_cast<void*>(const_cast<char*>(&data[i])), static_cast<size_t>(charSize));
            pos += charSize;
          }
          else {
            pos += escapeUtf8Char(&data[i], &resultData[pos]);
          }
          i += charSize;
        }
        else {
          // invalid char
          resultData[pos] = static_cast<v_char8>(data[i]);
          i++;
          pos++;
        }
      }
    }
  }

  if(size > safeSize){
    for(v_buff_size i = pos; static_cast<size_t>(i) < result->size(); i ++){
      resultData[i] = '?';
    }
  }

  return result;
}

void Utils::unescapeStringToBuffer(const char* data, v_buff_size size, p_char8 resultData){

  v_buff_size i = 0;
  v_buff_size pos = 0;

  while (i < size) {
    v_char8 a = static_cast<v_char8>(data[i]);

    if(a == '\\'){
      v_char8 b = static_cast<v_char8>(data[i + 1]);
      if(b != 'u'){
        switch (b) {
          case '"': resultData[pos] = '"'; pos ++; break;
          case '\\': resultData[pos] = '\\'; pos ++; break;
          case '/': resultData[pos] = '/'; pos ++; break;
          case 'b': resultData[pos] = '\b'; pos ++; break;
          case 'f': resultData[pos] = '\f'; pos ++; break;
          case 'n': resultData[pos] = '\n'; pos ++; break;
          case 'r': resultData[pos] = '\r'; pos ++; break;
          case 't': resultData[pos] = '\t'; pos ++; break;
          default: break;
        }
        i += 2;
      } else {
        if(data[i + 2] == '+'){ // Not JSON standard case
          v_uint32 code;
          encoding::Hex::readUInt32(&data[i + 3], code);
          i += 11;
          pos += encoding::Unicode::decodeUtf8Char(static_cast<v_int32>(code), &resultData[pos]);
        } else {

          v_uint16 code;
          encoding::Hex::readUInt16(&data[i + 2], code);

          if(code >= 0xD800 && code <= 0xDBFF){
            v_uint16 low;
            encoding::Hex::readUInt16(&data[i + 8], low);
            v_uint32 bigCode = static_cast<v_uint32>(encoding::Unicode::utf16SurrogatePairToCode(static_cast<v_int16>(code), static_cast<v_int16>(low)));
            pos += encoding::Unicode::decodeUtf8Char(static_cast<v_int32>(bigCode), &resultData[pos]);
            i += 12;
          } else {
            pos += encoding::Unicode::decodeUtf8Char(code, &resultData[pos]);
            i += 6;
          }

        }
      }
    } else {
      resultData[pos] = a;
      pos ++;
      i++;
    }

  }

}

oatpp::String Utils::unescapeString(const char* data, v_buff_size size, v_int64& errorCode, v_buff_size& errorPosition) {

  // Fast path: no backslash -> direct copy
  if (std::memchr(data, '\\', size) == nullptr) {
    errorCode = 0;
    return String(data, size);
  }

  v_buff_size unescapedSize = calcUnescapedStringSize(data, size, errorCode, errorPosition);
  if(errorCode != 0){
    return nullptr;
  }
  String result(unescapedSize);
  if(unescapedSize == size) {
    std::memcpy(reinterpret_cast<void*>(result->data()), data, static_cast<size_t>(size));
  } else {
    unescapeStringToBuffer(data, size, reinterpret_cast<p_char8>(result->data()));
  }
  return result;

}

std::string Utils::unescapeStringToStdString(const char* data, v_buff_size size, v_int64& errorCode, v_buff_size& errorPosition){

  v_buff_size unescapedSize = calcUnescapedStringSize(data, size, errorCode, errorPosition);
  if(errorCode != 0){
    return "";
  }
  std::string result;
  result.resize(static_cast<size_t>(unescapedSize));
  if(unescapedSize == size) {
    std::memcpy(reinterpret_cast<void*>(result.data()), data, static_cast<size_t>(size));
  } else {
    unescapeStringToBuffer(data, size, reinterpret_cast<p_char8>(result.data()));
  }
  return result;

}

const char* Utils::preparseString(ParsingCaret& caret, v_buff_size& size){

  if(caret.canContinueAtChar('"', 1)){

    const char* data = caret.getData();
    v_buff_size pos = caret.getPosition();
    v_buff_size pos0 = pos;
    v_buff_size length = caret.getDataSize();

    while (pos < length) {
      v_char8 a = static_cast<v_char8>(data[pos]);
      if(a == '"'){
        size = pos - pos0;
        return &data[pos0];
      } else if(a == '\\') {
        pos += 2;
      } else {
        pos ++;
      }
    }
    caret.setPosition(caret.getDataSize());
    caret.setError("[oatpp::json::Utils::preparseString()]: Error. '\"' - expected", ERROR_CODE_PARSER_QUOTE_EXPECTED);
  } else {
    caret.setError("[oatpp::json::Utils::preparseString()]: Error. '\"' - expected", ERROR_CODE_PARSER_QUOTE_EXPECTED);
  }

  return nullptr;

}

oatpp::String Utils::parseString(ParsingCaret& caret) {

  v_buff_size size;
  const char* data = preparseString(caret, size);

  if(data != nullptr) {

    v_buff_size pos = caret.getPosition();

    // Fast path: no backslash in raw string -> direct copy
    if (std::memchr(data, '\\', size) == nullptr) {
      caret.setPosition(pos + size + 1);
      return String(data, size);
    }

    v_int64 errorCode;
    v_buff_size errorPosition;
    auto result = unescapeString(data, size, errorCode, errorPosition);
    if(errorCode != 0){
      caret.setError("[oatpp::json::Utils::parseString()]: Error. Call to unescapeString() failed", errorCode);
      caret.setPosition(pos + errorPosition);
    } else {
      caret.setPosition(pos + size + 1);
    }

    return result;

  }

  return nullptr;

}

std::string Utils::parseStringToStdString(ParsingCaret& caret){

  v_buff_size size;
  auto data = preparseString(caret, size);

  if(data != nullptr) {

    v_buff_size pos = caret.getPosition();

    v_int64 errorCode;
    v_buff_size errorPosition;
    const std::string& result = unescapeStringToStdString(data, size, errorCode, errorPosition);
    if(errorCode != 0){
      caret.setError("[oatpp::json::Utils::parseStringToStdString()]: Error. Call to unescapeStringToStdString() failed", errorCode);
      caret.setPosition(pos + errorPosition);
    } else {
      caret.setPosition(pos + size + 1);
    }

    return result;

  }

  return "";

}

bool Utils::findDecimalSeparatorInCurrentNumber(ParsingCaret& caret) {
  utils::parser::Caret::StateSaveGuard stateGuard(caret);

  // search until a decimal or exponent separator is found or no more digits/sign or data available
  while(caret.canContinue()) {
    if (caret.isAtChar(JSON_DECIMAL_SEPARATOR) ||
        caret.isAtChar('e') || caret.isAtChar('E')) {
      return true;
    }
    if (!caret.isAtDigitChar() && !caret.isAtChar('-')) {
      return false;
    }
    caret.inc();
  }
  return false;
}

bool Utils::isBuiltinType(const data::type::Type* type) {
  if (!type) return false;
  const auto& cid = type->classId;

  return
    // Primitives
    cid == oatpp::String::Class::CLASS_ID   ||
    cid == oatpp::Int8::Class::CLASS_ID     ||
    cid == oatpp::UInt8::Class::CLASS_ID    ||
    cid == oatpp::Int16::Class::CLASS_ID    ||
    cid == oatpp::UInt16::Class::CLASS_ID   ||
    cid == oatpp::Int32::Class::CLASS_ID    ||
    cid == oatpp::UInt32::Class::CLASS_ID   ||
    cid == oatpp::Int64::Class::CLASS_ID    ||
    cid == oatpp::UInt64::Class::CLASS_ID   ||
    cid == oatpp::Float32::Class::CLASS_ID  ||
    cid == oatpp::Float64::Class::CLASS_ID  ||
    cid == oatpp::Boolean::Class::CLASS_ID  ||

    // Void
    cid == data::type::__class::Void::CLASS_ID ||

    // Abstract types
    cid == data::type::__class::AbstractObject::CLASS_ID ||
    cid == data::type::__class::AbstractEnum::CLASS_ID   ||
    cid == data::type::__class::AbstractList::CLASS_ID   ||
    cid == data::type::__class::AbstractVector::CLASS_ID ||
    cid == data::type::__class::AbstractPairList::CLASS_ID ||
    cid == data::type::__class::AbstractUnorderedMap::CLASS_ID ||
    cid == data::type::__class::AbstractUnorderedSet::CLASS_ID ||

    // Special
    cid == oatpp::Any::Class::CLASS_ID ||
    cid == oatpp::Tree::Class::CLASS_ID;
}

}}
