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

#include "FastDeserializer.hpp"

#include "./Utils.hpp"

#include "oatpp/data/type/Object.hpp"
#include "oatpp/data/type/Primitive.hpp"
#include "oatpp/data/type/Enum.hpp"
#include "oatpp/data/type/Any.hpp"
#include "oatpp/data/type/Collection.hpp"
#include "oatpp/data/type/Map.hpp"
#include "oatpp/data/type/Vector.hpp"
#include "oatpp/data/type/List.hpp"
#include "oatpp/data/type/PairList.hpp"
#include "oatpp/data/type/UnorderedMap.hpp"
#include "oatpp/data/type/UnorderedSet.hpp"

#include <cstring>
#include <cstdlib>
#include <vector>
#include <cstdint>

namespace oatpp { namespace json {

// ============================================================================
// Helpers shared by the cursor-based parser
// ============================================================================

/**
 * Construct a null oatpp::Void with the correct interpretation type
 * so that fromInterpretation can run its constraint checks (e.g. NotNull).
 */
static oatpp::Void makeInterpNull(const data::type::Type* interpType) {
  const auto& cid = interpType->classId;

  if (cid == oatpp::Int32::Class::CLASS_ID)    return oatpp::Int32();
  if (cid == oatpp::Int64::Class::CLASS_ID)    return oatpp::Int64();
  if (cid == oatpp::Int16::Class::CLASS_ID)    return oatpp::Int16();
  if (cid == oatpp::Int8::Class::CLASS_ID)     return oatpp::Int8();
  if (cid == oatpp::UInt32::Class::CLASS_ID)   return oatpp::UInt32();
  if (cid == oatpp::UInt64::Class::CLASS_ID)   return oatpp::UInt64();
  if (cid == oatpp::Float32::Class::CLASS_ID)  return oatpp::Float32();
  if (cid == oatpp::Float64::Class::CLASS_ID)  return oatpp::Float64();
  if (cid == oatpp::String::Class::CLASS_ID)   return oatpp::String();

  return oatpp::Void(nullptr, interpType);
}

/**
 * Parse a JSON number string into a strongly-typed integer value.
 */
static bool parseIntToType(
    const char* data, v_buff_size len,
    const data::type::ClassId& cid, oatpp::Void& result)
{
  if (cid == oatpp::UInt64::Class::CLASS_ID) {
    v_uint64 uv;
    if (!Utils::parseUInt64(data, len, uv)) return false;
    result = oatpp::UInt64(uv);
    return true;
  }

  v_int64 iv;
  if (!Utils::parseInt64(data, len, iv)) return false;

  if (cid == oatpp::Int8::Class::CLASS_ID) {
    result = oatpp::Int8(static_cast<v_int8>(iv));
  } else if (cid == oatpp::Int16::Class::CLASS_ID) {
    result = oatpp::Int16(static_cast<v_int16>(iv));
  } else if (cid == oatpp::Int32::Class::CLASS_ID) {
    result = oatpp::Int32(static_cast<v_int32>(iv));
  } else if (cid == oatpp::Int64::Class::CLASS_ID) {
    result = oatpp::Int64(iv);
  } else if (cid == oatpp::UInt8::Class::CLASS_ID) {
    result = oatpp::UInt8(static_cast<v_uint8>(iv));
  } else if (cid == oatpp::UInt16::Class::CLASS_ID) {
    result = oatpp::UInt16(static_cast<v_uint16>(iv));
  } else if (cid == oatpp::UInt32::Class::CLASS_ID) {
    result = oatpp::UInt32(static_cast<v_uint32>(iv));
  } else if (cid == oatpp::Float32::Class::CLASS_ID) {
    result = oatpp::Float32(static_cast<v_float32>(iv));
  } else if (cid == oatpp::Float64::Class::CLASS_ID) {
    result = oatpp::Float64(static_cast<v_float64>(iv));
  } else if (cid == oatpp::String::Class::CLASS_ID) {
    result = oatpp::String(data, len);
  } else {
    result = oatpp::Int64(iv);
  }
  return true;
}

/**
 * Parse a JSON number string into a strongly-typed floating-point value.
 */
static bool parseFloatToType(
    const char* data, v_buff_size len,
    const data::type::ClassId& cid, oatpp::Void& result)
{
  v_float64 fv;
  if (!Utils::parseFloat64(data, len, fv)) return false;

  if (cid == oatpp::Float32::Class::CLASS_ID) {
    result = oatpp::Float32(static_cast<v_float32>(fv));
  } else if (cid == oatpp::Float64::Class::CLASS_ID) {
    result = oatpp::Float64(fv);
  } else if (cid == oatpp::Int32::Class::CLASS_ID) {
    result = oatpp::Int32(static_cast<v_int32>(fv));
  } else if (cid == oatpp::Int64::Class::CLASS_ID) {
    result = oatpp::Int64(static_cast<v_int64>(fv));
  } else if (cid == oatpp::String::Class::CLASS_ID) {
    result = oatpp::String(data, len);
  } else {
    result = oatpp::Float64(fv);
  }
  return true;
}

/**
 * Parse a numeric value into an Enum via fromInterpretation.
 */
static bool parseIntToEnum(
    const char* data, v_buff_size len,
    const data::type::Type* type, bool isFloat, oatpp::Void& result,
    bool useUnqualifiedEnumNames)
{
  auto pdisp = static_cast<const data::type::__class::AbstractEnum::PolymorphicDispatcher*>(
      type->polymorphicDispatcher);
  auto* interpType = pdisp->getInterpretationType();
  const auto& interpCid = interpType->classId;

  data::type::EnumInterpreterError e = data::type::EnumInterpreterError::OK;
  oatpp::Void ev;

  if (!isFloat) {
    v_int64 iv;
    if (Utils::parseInt64(data, len, iv)) {
      if (interpCid == oatpp::Int32::Class::CLASS_ID)
        ev = pdisp->fromInterpretation(oatpp::Int32(static_cast<v_int32>(iv)), useUnqualifiedEnumNames, e);
      else if (interpCid == oatpp::Int64::Class::CLASS_ID)
        ev = pdisp->fromInterpretation(oatpp::Int64(iv), useUnqualifiedEnumNames, e);
      else if (interpCid == oatpp::Int16::Class::CLASS_ID)
        ev = pdisp->fromInterpretation(oatpp::Int16(static_cast<v_int16>(iv)), useUnqualifiedEnumNames, e);
      else if (interpCid == oatpp::Int8::Class::CLASS_ID)
        ev = pdisp->fromInterpretation(oatpp::Int8(static_cast<v_int8>(iv)), useUnqualifiedEnumNames, e);
      else
        ev = pdisp->fromInterpretation(oatpp::Int64(iv), useUnqualifiedEnumNames, e);
    }
  } else {
    v_float64 fv;
    if (Utils::parseFloat64(data, len, fv)) {
      if (interpCid == oatpp::Float64::Class::CLASS_ID)
        ev = pdisp->fromInterpretation(oatpp::Float64(fv), useUnqualifiedEnumNames, e);
      else if (interpCid == oatpp::Float32::Class::CLASS_ID)
        ev = pdisp->fromInterpretation(oatpp::Float32(static_cast<v_float32>(fv)), useUnqualifiedEnumNames, e);
      else
        ev = pdisp->fromInterpretation(oatpp::Float64(fv), useUnqualifiedEnumNames, e);
    }
  }

  if (e == data::type::EnumInterpreterError::OK) {
    result = ev;
  }
  return true;
}

/**
 * Parse a string value into an Enum via fromInterpretation.
 */
static bool parseStringToEnum(
    const oatpp::String& str,
    const data::type::Type* type, oatpp::Void& result,
    bool useUnqualifiedEnumNames)
{
  auto pdisp = static_cast<const data::type::__class::AbstractEnum::PolymorphicDispatcher*>(
      type->polymorphicDispatcher);
  data::type::EnumInterpreterError e = data::type::EnumInterpreterError::OK;
  auto ev = pdisp->fromInterpretation(str, useUnqualifiedEnumNames, e);
  if (e == data::type::EnumInterpreterError::OK) {
    result = ev;
  }
  return true;
}

/**
 * Wrap a deserialized value in an AnyHandle for Any-typed targets.
 * Returns the original value if the target type is not Any.
 */
static oatpp::Void wrapInAnyHandle(const oatpp::Void& value, const data::type::Type* targetType) {
  if (!targetType || targetType->classId != oatpp::Any::Class::CLASS_ID) {
    return value;
  }
  const data::type::Type* storedType = value.getValueType();
  if (!storedType && value.getPtr()) {
    storedType = oatpp::String::Class::getType();
  }
  auto ah = std::make_shared<data::type::AnyHandle>(value.getPtr(), storedType);
  return oatpp::Void(ah, oatpp::Any::Class::getType());
}

// ============================================================================
// Single-pass cursor-based deserializer.
// Walks the raw JSON buffer directly without an intermediate StructuralItem
// vector, eliminating the scanStructural allocation and second pass.
// ============================================================================

// --- Cursor helpers ---

static inline void skipWhitespace(const char* data, size_t len, size_t& pos) {
  while (pos < len) {
    uint8_t cls = CHAR_CLASS[static_cast<uint8_t>(data[pos])];
    if (!(cls & CC_WS)) break;
    pos++;
  }
}

// Find the closing (unescaped) quote starting from `pos` which MUST point to
// an opening '"'.  Returns the content range [start, end) and advances `pos`
// past the closing quote.
static bool findStringRange(const char* data, size_t len, size_t& pos,
                            size_t& contentStart, size_t& contentEnd) {
  contentStart = pos + 1; // skip opening '"'
  size_t i = contentStart;
  while (i < len) {
    if (data[i] == '"') {
      // Count preceding backslashes — odd ⇒ escaped quote
      size_t bs = 0, j = i;
      while (j > contentStart && data[j - 1] == '\\') { bs++; j--; }
      if ((bs & 1) == 0) {
        contentEnd = i;
        pos = i + 1; // past closing quote
        return true;
      }
    }
    i++;
  }
  return false;
}

// Extract a (possibly escaped) string from the buffer range [start, end).
static oatpp::String parseStringFromRange(const char* data,
                                           size_t start, size_t end) {
  v_buff_size rawLen = static_cast<v_buff_size>(end - start);
  const char* strData = data + start;
  if (!std::memchr(strData, '\\', rawLen)) {
    return oatpp::String(strData, rawLen);
  }
  v_int64 errorCode;
  v_buff_size errorPos;
  return Utils::unescapeString(strData, rawLen, errorCode, errorPos);
}

// Find the end of a JSON number (or identifier: true/false/null).
// Returns the position just past the last character.
static size_t findValueEnd(const char* data, size_t len, size_t pos) {
  while (pos < len) {
    uint8_t cls = CHAR_CLASS[static_cast<uint8_t>(data[pos])];
    // Stop at whitespace, structural chars, or quote
    if (cls & (CC_WS | CC_STRUCT | CC_QUOTE)) break;
    pos++;
  }
  return pos;
}

// --- Forward declarations for recursive cursor-based functions ---

static bool deserializeValueCursor(const char* data, size_t len, size_t& pos,
    const data::type::Type* type, oatpp::Void& result,
    const data::mapping::TreeToObjectMapper::Config& mapperConfig);
static bool deserializeObjectCursor(const char* data, size_t len, size_t& pos,
    const data::type::Type* type, oatpp::Void& result,
    const data::mapping::TreeToObjectMapper::Config& mapperConfig);
static bool deserializeArrayCursor(const char* data, size_t len, size_t& pos,
    const data::type::Type* type, oatpp::Void& result,
    const data::mapping::TreeToObjectMapper::Config& mapperConfig);
static bool deserializeMapCursor(const char* data, size_t len, size_t& pos,
    const data::type::Type* type, oatpp::Void& result,
    const data::mapping::TreeToObjectMapper::Config& mapperConfig);

// --- deserializePrimitiveValueCursor ---

static bool deserializePrimitiveValueCursor(
    const char* data, size_t len, size_t& pos,
    const data::type::Type* type, oatpp::Void& result,
    const data::mapping::TreeToObjectMapper::Config& mapperConfig)
{
  skipWhitespace(data, len, pos);
  if (pos >= len) return false;

  char firstChar = data[pos];

  // String
  if (firstChar == '"') {
    size_t contentStart, contentEnd;
    if (!findStringRange(data, len, pos, contentStart, contentEnd))
      return false;
    result = parseStringFromRange(data, contentStart, contentEnd);

    if (result && type) {
      const auto& cid = type->classId;
      if (cid == data::type::__class::AbstractEnum::CLASS_ID) {
        auto str = result.cast<oatpp::String>();
        parseStringToEnum(str, type, result, mapperConfig.useUnqualifiedEnumNames);
      } else if (cid != oatpp::String::Class::CLASS_ID && cid != oatpp::Any::Class::CLASS_ID) {
        if (mapperConfig.allowLexicalCasting) {
          auto str = result.cast<oatpp::String>();
          const char* sd = str->data();
          v_buff_size sl = static_cast<v_buff_size>(str->size());
          parseIntToType(sd, sl, cid, result);
        } else if (!mapperConfig.enabledInterpretations.empty()) {
          auto* interp = type->findInterpretation(mapperConfig.enabledInterpretations);
          if (interp) {
            auto str = result.cast<oatpp::String>();
            result = interp->fromInterpretation(str);
          }
        }
      }
    }
    result = wrapInAnyHandle(result, type);
    return true;
  }

  // null
  if (firstChar == 'n') {
    pos += 4; // skip "null"
    if (type && type->classId == data::type::__class::AbstractEnum::CLASS_ID) {
      auto pdisp = static_cast<const data::type::__class::AbstractEnum::PolymorphicDispatcher*>(
          type->polymorphicDispatcher);
      auto* interpType = pdisp->getInterpretationType();
      data::type::EnumInterpreterError e = data::type::EnumInterpreterError::OK;
      auto ev = pdisp->fromInterpretation(
          makeInterpNull(interpType), mapperConfig.useUnqualifiedEnumNames, e);
      if (e != data::type::EnumInterpreterError::OK) return false;
      result = ev;
    } else {
      result = nullptr;
    }
    result = wrapInAnyHandle(result, type);
    return true;
  }

  // Boolean
  if (firstChar == 't' || firstChar == 'f') {
    bool isTrue = (firstChar == 't');
    pos += (isTrue ? 4 : 5); // skip "true" or "false"

    if (type && type->classId == data::type::__class::AbstractEnum::CLASS_ID) {
      auto pdisp = static_cast<const data::type::__class::AbstractEnum::PolymorphicDispatcher*>(
          type->polymorphicDispatcher);
      data::type::EnumInterpreterError e = data::type::EnumInterpreterError::OK;
      auto ev = pdisp->fromInterpretation(
          oatpp::Boolean(isTrue), mapperConfig.useUnqualifiedEnumNames, e);
      if (e == data::type::EnumInterpreterError::OK) result = ev;
    } else if (type && type->classId == oatpp::Boolean::Class::CLASS_ID) {
      result = oatpp::Boolean(isTrue);
    } else {
      result = oatpp::Boolean(isTrue);
    }
    result = wrapInAnyHandle(result, type);
    return true;
  }

  // Number — find its extent in the buffer
  size_t numEnd = findValueEnd(data, len, pos);
  const char* valData = data + pos;
  v_buff_size valLen = static_cast<v_buff_size>(numEnd - pos);

  if (valLen == 0) return false;
  pos = numEnd; // consume the number

  // Single-pass float detection (no triple memchr)
  bool isFloat = false;
  for (v_buff_size i = 0; i < valLen; i++) {
    char c = valData[i];
    if (c == '.' || c == 'e' || c == 'E') { isFloat = true; break; }
  }

  if (!type) return true;

  const auto& cid = type->classId;

  if (cid == oatpp::Any::Class::CLASS_ID) {
    if (!isFloat) {
      v_int64 iv;
      if (Utils::parseInt64(valData, valLen, iv)) {
        result = wrapInAnyHandle(oatpp::Int64(iv), type);
        return true;
      }
    }
    v_float64 fv;
    if (Utils::parseFloat64(valData, valLen, fv)) {
      result = wrapInAnyHandle(oatpp::Float64(fv), type);
      return true;
    }
    return false;
  }

  if (cid == data::type::__class::AbstractEnum::CLASS_ID) {
    parseIntToEnum(valData, valLen, type, isFloat, result,
                   mapperConfig.useUnqualifiedEnumNames);
    return true;
  }

  if (!isFloat) {
    if (parseIntToType(valData, valLen, cid, result)) return true;
  }
  if (parseFloatToType(valData, valLen, cid, result)) return true;

  if (!mapperConfig.enabledInterpretations.empty()) {
    auto* interp = type->findInterpretation(mapperConfig.enabledInterpretations);
    if (interp) {
      auto* interpType = interp->getInterpretationType();
      const auto& interpCid = interpType->classId;

      if (!isFloat) {
        v_int64 iv;
        if (Utils::parseInt64(valData, valLen, iv)) {
          if (interpCid == oatpp::Int64::Class::CLASS_ID)
            result = interp->fromInterpretation(oatpp::Int64(iv));
          else if (interpCid == oatpp::Int32::Class::CLASS_ID)
            result = interp->fromInterpretation(oatpp::Int32(static_cast<v_int32>(iv)));
          else if (interpCid == oatpp::Int16::Class::CLASS_ID)
            result = interp->fromInterpretation(oatpp::Int16(static_cast<v_int16>(iv)));
          else if (interpCid == oatpp::Int8::Class::CLASS_ID)
            result = interp->fromInterpretation(oatpp::Int8(static_cast<v_int8>(iv)));
          else if (interpCid == oatpp::Float64::Class::CLASS_ID)
            result = interp->fromInterpretation(oatpp::Float64(static_cast<v_float64>(iv)));
          else if (interpCid == oatpp::Float32::Class::CLASS_ID)
            result = interp->fromInterpretation(oatpp::Float32(static_cast<v_float32>(iv)));
          else if (interpCid == oatpp::String::Class::CLASS_ID) {
            char buf[24];
            auto n = Utils::int64ToChars(iv, buf);
            result = interp->fromInterpretation(oatpp::String(buf, n));
          }
          return result != nullptr;
        }
      } else {
        v_float64 fv;
        if (Utils::parseFloat64(valData, valLen, fv)) {
          if (interpCid == oatpp::Float64::Class::CLASS_ID)
            result = interp->fromInterpretation(oatpp::Float64(fv));
          else if (interpCid == oatpp::Float32::Class::CLASS_ID)
            result = interp->fromInterpretation(oatpp::Float32(static_cast<v_float32>(fv)));
          else if (interpCid == oatpp::String::Class::CLASS_ID) {
            char buf[64];
            auto n = Utils::float64ToChars(fv, buf);
            result = interp->fromInterpretation(oatpp::String(buf, n));
          }
          return result != nullptr;
        }
      }
    }
  }

  return false;
}

// --- deserializeValueCursor (main dispatch) ---

static bool deserializeValueCursor(
    const char* data, size_t len, size_t& pos,
    const data::type::Type* type, oatpp::Void& result,
    const data::mapping::TreeToObjectMapper::Config& mapperConfig)
{
  skipWhitespace(data, len, pos);
  if (pos >= len) return false;

  char c = data[pos];

  switch (c) {
    case '{':
      if (type && type->classId == data::type::__class::AbstractObject::CLASS_ID)
        return deserializeObjectCursor(data, len, pos, type, result, mapperConfig);
      if (type && (type->classId == data::type::__class::AbstractPairList::CLASS_ID ||
                   type->classId == data::type::__class::AbstractUnorderedMap::CLASS_ID))
        return deserializeMapCursor(data, len, pos, type, result, mapperConfig);
      if (type && type->classId == oatpp::Any::Class::CLASS_ID) {
        auto mapType = oatpp::UnorderedFields<oatpp::Any>::Class::getType();
        oatpp::Void mapResult;
        if (deserializeMapCursor(data, len, pos, mapType, mapResult, mapperConfig)) {
          result = wrapInAnyHandle(mapResult, type);
          return true;
        }
        return false;
      }
      // Unknown object — skip by depth
      { pos++; int depth = 1;
        while (pos < len && depth > 0) {
          if (data[pos] == '{') depth++;
          else if (data[pos] == '}') depth--;
          else if (data[pos] == '"') {
            // skip string contents (handle escapes)
            pos++;
            while (pos < len) {
              if (data[pos] == '"') {
                size_t bs = 0, j = pos;
                while (j > 0 && data[j-1] == '\\') { bs++; j--; }
                if ((bs & 1) == 0) break;
              }
              pos++;
            }
          }
          pos++;
        }
      }
      return true;

    case '[':
      if (type && (type->classId == data::type::__class::AbstractVector::CLASS_ID ||
                   type->classId == data::type::__class::AbstractList::CLASS_ID ||
                   type->classId == data::type::__class::AbstractUnorderedSet::CLASS_ID))
        return deserializeArrayCursor(data, len, pos, type, result, mapperConfig);
      if (type && type->classId == oatpp::Any::Class::CLASS_ID) {
        auto listType = oatpp::List<oatpp::Any>::Class::getType();
        oatpp::Void listResult;
        if (deserializeArrayCursor(data, len, pos, listType, listResult, mapperConfig)) {
          result = wrapInAnyHandle(listResult, type);
          return true;
        }
        return false;
      }
      // Unknown array — skip by depth
      { pos++; int depth = 1;
        while (pos < len && depth > 0) {
          if (data[pos] == '[') depth++;
          else if (data[pos] == ']') depth--;
          else if (data[pos] == '"') {
            pos++;
            while (pos < len) {
              if (data[pos] == '"') {
                size_t bs = 0, j = pos;
                while (j > 0 && data[j-1] == '\\') { bs++; j--; }
                if ((bs & 1) == 0) break;
              }
              pos++;
            }
          }
          pos++;
        }
      }
      return true;

    default:
      return deserializePrimitiveValueCursor(data, len, pos, type, result, mapperConfig);
  }
}

// --- deserializeObjectCursor ---

static bool deserializeObjectCursor(
    const char* data, size_t len, size_t& pos,
    const data::type::Type* type, oatpp::Void& result,
    const data::mapping::TreeToObjectMapper::Config& mapperConfig)
{
  pos++; // skip '{'

  auto dispatcher =
      static_cast<const data::type::__class::AbstractObject::PolymorphicDispatcher*>(
          type->polymorphicDispatcher);
  auto props = dispatcher->getProperties();

  result = dispatcher->createObject();
  auto* object = static_cast<oatpp::BaseObject*>(result.get());

  // Sequential property cache — same as before but using cursor
  const auto& propList = props->getList();
  auto nextPropIt = propList.begin();
  bool useSequentialCache = true;

  while (pos < len) {
    skipWhitespace(data, len, pos);
    if (pos >= len) return false;

    // Empty object or end
    if (data[pos] == '}') { pos++; break; }

    // Parse key string
    if (data[pos] != '"') return false;
    size_t keyStart, keyEnd;
    if (!findStringRange(data, len, pos, keyStart, keyEnd)) return false;
    // key is in data[keyStart .. keyEnd)

    skipWhitespace(data, len, pos);
    if (pos >= len || data[pos] != ':') return false;
    pos++; // skip ':'

    // Property lookup
    const oatpp::BaseObject::Property* prop = nullptr;

    // Fast path: sequential cache
    if (useSequentialCache && nextPropIt != propList.end()) {
      const auto& candidate = *nextPropIt;
      const std::string& expectedKey = mapperConfig.useUnqualifiedFieldNames
          ? candidate->unqualifiedName : candidate->name;
      size_t klen = static_cast<size_t>(keyEnd - keyStart);
      if (expectedKey.size() == klen &&
          std::memcmp(expectedKey.data(), data + keyStart, klen) == 0) {
        prop = candidate;
        ++nextPropIt;
      } else {
        useSequentialCache = false;
      }
    }

    if (!prop) {
      // Full map lookup — need an oatpp::String for the key
      auto keyStr = parseStringFromRange(data, keyStart, keyEnd);
      if (mapperConfig.useUnqualifiedFieldNames) {
        auto uit = props->getUnqualifiedMap().find(keyStr);
        if (uit != props->getUnqualifiedMap().end()) prop = uit->second;
      } else {
        auto it = props->getMap().find(keyStr);
        if (it != props->getMap().end()) {
          prop = it->second;
        } else {
          auto uit = props->getUnqualifiedMap().find(keyStr);
          if (uit != props->getUnqualifiedMap().end()) prop = uit->second;
        }
      }
    }

    if (prop) {
      const data::type::Type* fieldType = prop->type;
      bool isAnyField = (fieldType == oatpp::Any::Class::getType());
      const data::type::Type* parseType = fieldType;

      if (isAnyField && prop->info.typeSelector) {
        parseType = prop->info.typeSelector->selectType(object);
      } else if (isAnyField && !prop->info.typeSelector) {
        // Peek at the value from current cursor position
        skipWhitespace(data, len, pos);
        char peek = (pos < len) ? data[pos] : '\0';
        if (peek == '"') parseType = oatpp::String::Class::getType();
        else if (peek == 't' || peek == 'f') parseType = oatpp::Boolean::Class::getType();
        else if (peek == 'n') parseType = nullptr;
        else if (peek == '-' || (peek >= '0' && peek <= '9')) {
          // Find out if it's a float
          size_t peekPos = pos + 1;
          bool isFloat = false;
          while (peekPos < len) {
            char pc = data[peekPos];
            if (pc == '.' || pc == 'e' || pc == 'E') { isFloat = true; break; }
            if (!(CHAR_CLASS[static_cast<uint8_t>(pc)] & CC_DIGIT)) break;
            peekPos++;
          }
          parseType = isFloat ? oatpp::Float64::Class::getType()
                              : oatpp::Int64::Class::getType();
        } else {
          parseType = nullptr;
        }
      }

      oatpp::Void fieldValue;
      if (!deserializeValueCursor(data, len, pos, parseType, fieldValue, mapperConfig))
        return false;

      if (isAnyField) {
        const data::type::Type* storedType = fieldValue.getValueType();
        if (!fieldValue && (prop->info.typeSelector || parseType)) {
          storedType = parseType;
          auto ah = std::make_shared<data::type::AnyHandle>(
              std::shared_ptr<void>(nullptr), storedType);
          prop->set(object, oatpp::Void(ah, oatpp::Any::Class::getType()));
        } else {
          if (!storedType) storedType = parseType;
          if (!storedType && fieldValue.getPtr())
            storedType = oatpp::String::Class::getType();
          auto ah = std::make_shared<data::type::AnyHandle>(
              fieldValue.getPtr(), storedType);
          prop->set(object, oatpp::Void(ah, oatpp::Any::Class::getType()));
        }
      } else {
        prop->set(object, fieldValue);
      }
    } else if (!mapperConfig.allowUnknownFields) {
      return false;
    } else {
      // Skip unknown value
      oatpp::Void dummy;
      if (!deserializeValueCursor(data, len, pos, nullptr, dummy, mapperConfig))
        return false;
    }

    // Advance past ',' or '}'
    skipWhitespace(data, len, pos);
    if (pos < len && data[pos] == ',') {
      pos++;
    } else if (pos < len && data[pos] == '}') {
      pos++;
      break;
    }
  }

  // Verify required fields
  for (auto const& field : props->getList()) {
    if (field->info.required) {
      auto fv = field->get(object);
      if (!fv) return false;
    }
  }

  return true;
}

// --- deserializeArrayCursor (with element pre-count for reserve) ---

static bool deserializeArrayCursor(
    const char* data, size_t len, size_t& pos,
    const data::type::Type* type, oatpp::Void& result,
    const data::mapping::TreeToObjectMapper::Config& mapperConfig)
{
  pos++; // skip '['

  auto dispatcher =
      static_cast<const data::type::__class::Collection::PolymorphicDispatcher*>(
          type->polymorphicDispatcher);
  result = dispatcher->createObject();
  const data::type::Type* itemType = dispatcher->getItemType();

  // Pre-count elements for reserve (if the dispatcher supports it).
  // We scan ahead counting top-level commas.
  {
    size_t scanPos = pos;
    int depth = 0;
    size_t count = 0;
    bool isEmpty = false;
    bool inString = false;
    while (scanPos < len) {
      char c = data[scanPos];
      if (inString) {
        if (c == '"') {
          size_t bs = 0, j = scanPos;
          while (j > 0 && data[j - 1] == '\\') { bs++; j--; }
          if ((bs & 1) == 0) inString = false;
        }
        scanPos++;
        continue;
      }
      if (c == '"') { inString = true; scanPos++; continue; }
      if (c == '[') { depth++; scanPos++; continue; }
      if (c == ']') {
        if (depth == 0) { isEmpty = (count == 0 && scanPos == pos); break; }
        depth--;
        scanPos++;
        continue;
      }
      if (depth == 0 && c == ',') { count++; }
      scanPos++;
    }
    if (!isEmpty && count + 1 > 0) {
      // Try to reserve — the dispatcher may or may not support this.
      // For Vector<T>, dispatcher->createObject() + repeated addItem is the
      // only public API; we skip reserve to keep things general.
      (void)count; // reserved for future dispatcher->reserve() call
    }
  }

  while (pos < len) {
    skipWhitespace(data, len, pos);
    if (pos >= len) return false;

    if (data[pos] == ']') { pos++; return true; }

    oatpp::Void itemValue;
    if (!deserializeValueCursor(data, len, pos, itemType, itemValue, mapperConfig))
      return false;

    dispatcher->addItem(result, itemValue);

    skipWhitespace(data, len, pos);
    if (pos < len && data[pos] == ']') { pos++; return true; }
    if (pos < len && data[pos] == ',') pos++;
  }

  return false;
}

// --- deserializeMapCursor ---

static bool deserializeMapCursor(
    const char* data, size_t len, size_t& pos,
    const data::type::Type* type, oatpp::Void& result,
    const data::mapping::TreeToObjectMapper::Config& mapperConfig)
{
  pos++; // skip '{'

  auto dispatcher =
      static_cast<const data::type::__class::Map::PolymorphicDispatcher*>(
          type->polymorphicDispatcher);
  const data::type::Type* keyType = dispatcher->getKeyType();
  const data::type::Type* valueType = dispatcher->getValueType();
  result = dispatcher->createObject();

  while (pos < len) {
    skipWhitespace(data, len, pos);
    if (pos >= len) return false;

    if (data[pos] == '}') { pos++; return true; }

    // Parse key
    if (data[pos] != '"') return false;
    size_t keyStart, keyEnd;
    if (!findStringRange(data, len, pos, keyStart, keyEnd)) return false;
    auto key = parseStringFromRange(data, keyStart, keyEnd);

    skipWhitespace(data, len, pos);
    if (pos >= len || data[pos] != ':') return false;
    pos++; // skip ':'

    oatpp::Void kv, value;
    if (keyType->classId == oatpp::String::Class::CLASS_ID) {
      kv = key;
    }
    if (!deserializeValueCursor(data, len, pos, valueType, value, mapperConfig))
      return false;

    dispatcher->addItem(result, kv, value);

    skipWhitespace(data, len, pos);
    if (pos < len && data[pos] == ',') {
      pos++;
    } else if (pos < len && data[pos] == '}') {
      pos++;
      return true;
    }
  }

  return false;
}

// ============================================================================
// deserialize — public entry point
// ============================================================================

oatpp::Void FastDeserializer::deserialize(
    oatpp::utils::parser::Caret& caret,
    const data::type::Type* type,
    data::mapping::ErrorStack& errorStack,
    const data::mapping::TreeToObjectMapper::Config& mapperConfig,
    const Deserializer::Config& jsonConfig)
{
  (void)jsonConfig;

  const char* jsonData = caret.getData() + caret.getPosition();
  size_t jsonLen = caret.getDataSize() - caret.getPosition();

  if (!jsonData || jsonLen < 1 || !type) return nullptr;

  /* Single-pass cursor-based deserialization — no structural scan needed. */
  size_t pos = 0;
  oatpp::Void result;
  if (deserializeValueCursor(jsonData, jsonLen, pos, type, result, mapperConfig)) {
    caret.setPosition(caret.getDataSize());
    return result;
  }

  /* Report error if parsing failed */
  errorStack.push("[oatpp::json::FastDeserializer::deserialize()]: "
                   "Error. Can't deserialize.");
  return nullptr;
}

}} // namespace oatpp::json
