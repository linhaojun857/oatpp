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
// Structural scan data types
// ============================================================================

struct StructuralItem {
  uint32_t pos;    // byte offset in JSON buffer
  uint8_t  type;   // '{' '}' '[' ']' ':' ',' '"'
  uint8_t  flags;  // 0: opening quote, 1: closing quote
};

// Forward declarations for recursive deserialize functions
static bool deserializeValue(const char* jsonData,
    const std::vector<StructuralItem>& items, size_t& idx,
    const data::type::Type* type, oatpp::Void& result,
    const data::mapping::TreeToObjectMapper::Config& mapperConfig);
static bool deserializeObject(const char* jsonData,
    const std::vector<StructuralItem>& items, size_t& idx,
    const data::type::Type* type, oatpp::Void& result,
    const data::mapping::TreeToObjectMapper::Config& mapperConfig);
static bool deserializeArray(const char* jsonData,
    const std::vector<StructuralItem>& items, size_t& idx,
    const data::type::Type* type, oatpp::Void& result,
    const data::mapping::TreeToObjectMapper::Config& mapperConfig);
static bool deserializeMap(const char* jsonData,
    const std::vector<StructuralItem>& items, size_t& idx,
    const data::type::Type* type, oatpp::Void& result,
    const data::mapping::TreeToObjectMapper::Config& mapperConfig);
static oatpp::String deserializeJsonString(const char* jsonData,
    const std::vector<StructuralItem>& items, size_t& idx);

// ============================================================================
// Stage 1: Scan JSON buffer for structural characters
// ============================================================================

static bool scanStructural(
    const uint8_t* buf, size_t len,
    std::vector<StructuralItem>& out)
{
  out.clear();
  out.reserve(len / 8);

  bool inString = false;

  for (size_t i = 0; i < len; i++) {
    uint8_t c = buf[i];
    uint8_t cls = CHAR_CLASS[c];

    /* Handle quote characters (open/close strings) */
    if (cls & CC_QUOTE) {
      if (inString) {
        /* Count preceding backslashes — odd count means escaped quote */
        size_t bsCount = 0, j = i;
        while (j > 0 && buf[j - 1] == '\\') { bsCount++; j--; }
        if (bsCount % 2 == 0) {
          inString = false;
          out.push_back({static_cast<uint32_t>(i), '"', 1}); /* closing quote */
        }
      } else {
        inString = true;
        out.push_back({static_cast<uint32_t>(i), '"', 0}); /* opening quote */
      }
      continue;
    }

    /* Skip everything inside strings */
    if (inString) continue;

    /* Record structural characters outside strings */
    if (cls & CC_STRUCT) {
      out.push_back({static_cast<uint32_t>(i), c, 0});
    }
  }

  return true;
}

// ============================================================================
// deserializeJsonString — extract and unescape a JSON string between two quotes
// ============================================================================

static oatpp::String deserializeJsonString(
    const char* jsonData,
    const std::vector<StructuralItem>& items,
    size_t& idx)
{
  /* idx must point to an opening '"' (flags=0) */
  uint32_t start = items[idx].pos + 1;
  idx++; /* skip opening quote */

  /* Find matching closing '"' (flags=1) */
  while (idx < items.size() &&
         !(items[idx].type == '"' && items[idx].flags == 1)) {
    idx++;
  }
  if (idx >= items.size()) return nullptr;

  uint32_t end = items[idx].pos;
  v_buff_size rawLen = static_cast<v_buff_size>(end - start);
  const char* strData = jsonData + start;

  /* Fast path: no backslash in raw string → no unescaping needed */
  if (!std::memchr(strData, '\\', rawLen)) {
    idx++;
    return oatpp::String(strData, rawLen);
  }

  /* Slow path: unescape the string */
  v_int64 errorCode;
  v_buff_size errorPos;
  auto result = Utils::unescapeString(strData, rawLen, errorCode, errorPos);
  idx++;
  return errorCode == 0 ? result : nullptr;
}

// ============================================================================
// Helper: create a typed null value for enum from-interpretation
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

// ============================================================================
// Numeric value parsing helpers
// ============================================================================

/**
 * Parse a JSON number string into a strongly-typed integer value.
 * Handles all integer class IDs up to int64/uint64.
 */
static bool parseIntToType(
    const char* data, v_buff_size len,
    const data::type::ClassId& cid, oatpp::Void& result)
{
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
  } else if (cid == oatpp::UInt64::Class::CLASS_ID) {
    result = oatpp::UInt64(static_cast<v_uint64>(iv));
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
      if (interpCid == oatpp::Int32::Class::CLASS_ID) {
        ev = pdisp->fromInterpretation(
            oatpp::Int32(static_cast<v_int32>(iv)), useUnqualifiedEnumNames, e);
      } else if (interpCid == oatpp::Int64::Class::CLASS_ID) {
        ev = pdisp->fromInterpretation(
            oatpp::Int64(iv), useUnqualifiedEnumNames, e);
      } else if (interpCid == oatpp::Int16::Class::CLASS_ID) {
        ev = pdisp->fromInterpretation(
            oatpp::Int16(static_cast<v_int16>(iv)), useUnqualifiedEnumNames, e);
      } else if (interpCid == oatpp::Int8::Class::CLASS_ID) {
        ev = pdisp->fromInterpretation(
            oatpp::Int8(static_cast<v_int8>(iv)), useUnqualifiedEnumNames, e);
      } else {
        ev = pdisp->fromInterpretation(
            oatpp::Int64(iv), useUnqualifiedEnumNames, e);
      }
    }
  } else {
    v_float64 fv;
    if (Utils::parseFloat64(data, len, fv)) {
      if (interpCid == oatpp::Float64::Class::CLASS_ID) {
        ev = pdisp->fromInterpretation(
            oatpp::Float64(fv), useUnqualifiedEnumNames, e);
      } else if (interpCid == oatpp::Float32::Class::CLASS_ID) {
        ev = pdisp->fromInterpretation(
            oatpp::Float32(static_cast<v_float32>(fv)), useUnqualifiedEnumNames, e);
      } else {
        ev = pdisp->fromInterpretation(
            oatpp::Float64(fv), useUnqualifiedEnumNames, e);
      }
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

// ============================================================================
// deserializePrimitiveValue
// ============================================================================

static bool deserializePrimitiveValue(
    const char* jsonData,
    const StructuralItem& item,
    const std::vector<StructuralItem>& items,
    size_t& idx,
    const data::type::Type* type,
    oatpp::Void& result,
    const data::mapping::TreeToObjectMapper::Config& mapperConfig)
{
  /*
   * Determine the raw value span in the JSON buffer.
   *
   * For strings: item IS the opening quote. We find the matching closing quote
   * and the span is [opening_quote .. closing_quote] inclusive.
   *
   * For non-strings (null / true / false / numbers): item points to the
   * structural character AFTER the value (',' , '}', ']', or ':'). The span
   * is from the character after the previous structural item to item.pos.
   */
  bool isStringValue = (item.type == '"' && item.flags == 0);
  uint32_t valEnd, valStart;

  if (isStringValue) {
    valStart = item.pos;
    size_t closeIdx = idx + 1;
    while (closeIdx < items.size() &&
           !(items[closeIdx].type == '"' && items[closeIdx].flags == 1)) {
      closeIdx++;
    }
    if (closeIdx >= items.size()) return false;
    valEnd = items[closeIdx].pos + 1; /* include closing quote */
  } else {
    valEnd = item.pos;
    valStart = (idx > 0) ? items[idx - 1].pos + 1 : 0;
  }

  const char* valData = jsonData + valStart;
  v_buff_size valLen = static_cast<v_buff_size>(valEnd - valStart);

  /* Trim leading whitespace */
  while (valLen > 0 && (valData[0] == ' ' || valData[0] == '\t' ||
         valData[0] == '\n' || valData[0] == '\r')) {
    valData++;
    valLen--;
  }

  /* Trim trailing whitespace */
  while (valLen > 0 && (valData[valLen - 1] == ' ' ||
         valData[valLen - 1] == '\t' ||
         valData[valLen - 1] == '\n' ||
         valData[valLen - 1] == '\r')) {
    valLen--;
  }

  if (valLen == 0) return false;

  char firstChar = valData[0];

  // ------------------------------------------------------------------
  // String value
  // ------------------------------------------------------------------
  if (firstChar == '"') {
    result = deserializeJsonString(jsonData, items, idx);
    if (result && type) {
      const auto& cid = type->classId;

      if (cid == data::type::__class::AbstractEnum::CLASS_ID) {
        /* String to Enum conversion */
        auto str = result.cast<oatpp::String>();
        parseStringToEnum(str, type, result, mapperConfig.useUnqualifiedEnumNames);
      } else if (cid != oatpp::String::Class::CLASS_ID) {
        /* Lexical cast from string to target numeric type */
        if (mapperConfig.allowLexicalCasting) {
          auto str = result.cast<oatpp::String>();
          const char* sd = str->data();
          v_buff_size sl = static_cast<v_buff_size>(str->size());
          v_int64 iv;
          if (Utils::parseInt64(sd, sl, iv)) {
            parseIntToType(sd, sl, cid, result);
          }
        } else if (!mapperConfig.enabledInterpretations.empty()) {
          /* Try enabledInterpretations for custom types with String interpretation */
          auto* interp = type->findInterpretation(
              mapperConfig.enabledInterpretations);
          if (interp) {
            auto str = result.cast<oatpp::String>();
            result = interp->fromInterpretation(str);
          }
        }
      }
    }
    return true;
  }

  // ------------------------------------------------------------------
  // null
  // ------------------------------------------------------------------
  if (firstChar == 'n') {
    if (type && type->classId == data::type::__class::AbstractEnum::CLASS_ID) {
      /* Check NotNull constraint on enum */
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
    return true;
  }

  // ------------------------------------------------------------------
  // Boolean (or Enum from boolean)
  // ------------------------------------------------------------------
  if (firstChar == 't' || firstChar == 'f') {
    bool isTrue = (firstChar == 't');

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
    return true;
  }

  // ------------------------------------------------------------------
  // Number
  // ------------------------------------------------------------------
  bool isFloat = std::memchr(valData, '.', valLen) ||
                 std::memchr(valData, 'e', valLen) ||
                 std::memchr(valData, 'E', valLen);

  if (!type) return true; /* no target type → nothing to parse into */

  const auto& cid = type->classId;

  /* Enum from number */
  if (cid == data::type::__class::AbstractEnum::CLASS_ID) {
    parseIntToEnum(valData, valLen, type, isFloat, result,
                   mapperConfig.useUnqualifiedEnumNames);
    return true;
  }

  /* Parse into target type */
  if (!isFloat) {
    if (parseIntToType(valData, valLen, cid, result)) return true;
  }
  if (parseFloatToType(valData, valLen, cid, result)) return true;

  /* Try enabledInterpretations as fallback for custom types */
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
            result = interp->fromInterpretation(
                oatpp::Int32(static_cast<v_int32>(iv)));
          else if (interpCid == oatpp::Int16::Class::CLASS_ID)
            result = interp->fromInterpretation(
                oatpp::Int16(static_cast<v_int16>(iv)));
          else if (interpCid == oatpp::Int8::Class::CLASS_ID)
            result = interp->fromInterpretation(
                oatpp::Int8(static_cast<v_int8>(iv)));
          else if (interpCid == oatpp::Float64::Class::CLASS_ID)
            result = interp->fromInterpretation(
                oatpp::Float64(static_cast<v_float64>(iv)));
          else if (interpCid == oatpp::Float32::Class::CLASS_ID)
            result = interp->fromInterpretation(
                oatpp::Float32(static_cast<v_float32>(iv)));
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
            result = interp->fromInterpretation(
                oatpp::Float32(static_cast<v_float32>(fv)));
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

// ============================================================================
// deserializeValue — dispatch based on structural character
// ============================================================================

static bool deserializeValue(
    const char* jsonData,
    const std::vector<StructuralItem>& items,
    size_t& idx,
    const data::type::Type* type,
    oatpp::Void& result,
    const data::mapping::TreeToObjectMapper::Config& mapperConfig)
{
  if (idx >= items.size()) return false;
  const auto& item = items[idx];

  /* Skip stray closing quotes (should not appear as value starts) */
  if (item.type == '"' && item.flags == 1) {
    idx++;
    return deserializeValue(jsonData, items, idx, type, result, mapperConfig);
  }

  switch (item.type) {
    // ----------------------------------------------------------------
    // Object — may be DTO, PairList, or UnorderedMap
    // ----------------------------------------------------------------
    case '{':
      if (type && type->classId == data::type::__class::AbstractObject::CLASS_ID) {
        return deserializeObject(jsonData, items, idx, type, result, mapperConfig);
      }
      if (type && (type->classId == data::type::__class::AbstractPairList::CLASS_ID ||
                   type->classId == data::type::__class::AbstractUnorderedMap::CLASS_ID)) {
        return deserializeMap(jsonData, items, idx, type, result, mapperConfig);
      }
      /* Unknown object — skip over it */
      { int depth = 1; idx++;
        while (idx < items.size() && depth > 0) {
          if (items[idx].type == '{') depth++;
          else if (items[idx].type == '}') depth--;
          idx++;
        }
      }
      return true;

    // ----------------------------------------------------------------
    // Array — may be Vector, List, or UnorderedSet
    // ----------------------------------------------------------------
    case '[':
      if (type && (type->classId == data::type::__class::AbstractVector::CLASS_ID ||
                   type->classId == data::type::__class::AbstractList::CLASS_ID ||
                   type->classId == data::type::__class::AbstractUnorderedSet::CLASS_ID)) {
        return deserializeArray(jsonData, items, idx, type, result, mapperConfig);
      }
      /* Unknown array — skip over it */
      { int depth = 1; idx++;
        while (idx < items.size() && depth > 0) {
          if (items[idx].type == '[') depth++;
          else if (items[idx].type == ']') depth--;
          idx++;
        }
      }
      return true;

    // ----------------------------------------------------------------
    // Primitive (string / null / boolean / number)
    // ----------------------------------------------------------------
    default:
      return deserializePrimitiveValue(
          jsonData, item, items, idx, type, result, mapperConfig);
  }
}

// ============================================================================
// deserializeObject — deserialize a JSON object into a DTO
// ============================================================================

static bool deserializeObject(
    const char* jsonData,
    const std::vector<StructuralItem>& items,
    size_t& idx,
    const data::type::Type* type,
    oatpp::Void& result,
    const data::mapping::TreeToObjectMapper::Config& mapperConfig)
{
  idx++; /* skip '{' */

  auto dispatcher =
      static_cast<const data::type::__class::AbstractObject::PolymorphicDispatcher*>(
          type->polymorphicDispatcher);
  auto props = dispatcher->getProperties();

  result = dispatcher->createObject();
  auto* object = static_cast<oatpp::BaseObject*>(result.get());

  while (idx < items.size()) {
    /* Empty object — '}' ends immediately */
    if (items[idx].type == '}') { idx++; break; }

    /* Deserialize field key */
    if (items[idx].type != '"' || items[idx].flags != 0) return false;
    auto key = deserializeJsonString(jsonData, items, idx);
    if (!key) return false;

    /* Expect ':' separator */
    if (idx >= items.size() || items[idx].type != ':') return false;
    idx++; /* skip ':' */

    /* Look up the field in the DTO's property map */
    const oatpp::BaseObject::Property* prop = nullptr;
    if (mapperConfig.useUnqualifiedFieldNames) {
      auto uit = props->getUnqualifiedMap().find(key);
      if (uit != props->getUnqualifiedMap().end()) prop = uit->second;
    } else {
      auto it = props->getMap().find(key);
      if (it != props->getMap().end()) {
        prop = it->second;
      } else {
        /* Fallback: also try unqualified names for backward compatibility */
        auto uit = props->getUnqualifiedMap().find(key);
        if (uit != props->getUnqualifiedMap().end()) prop = uit->second;
      }
    }

    if (prop) {
      const data::type::Type* fieldType = prop->type;
      bool isAnyField = (fieldType == oatpp::Any::Class::getType());

      /* Resolve the type to parse against */
      const data::type::Type* parseType = fieldType;
      if (isAnyField && prop->info.typeSelector) {
        parseType = prop->info.typeSelector->selectType(object);
      } else if (isAnyField && !prop->info.typeSelector) {
        /* No typeSelector — peek JSON content to guess the type */
        const char* peek = jsonData + items[idx - 1].pos + 1;
        while (*peek == ' ' || *peek == '\t' || *peek == '\n' || *peek == '\r') peek++;
        if (*peek == '"') {
          parseType = oatpp::String::Class::getType();
        } else if (*peek == 't' || *peek == 'f') {
          parseType = oatpp::Boolean::Class::getType();
        } else if (*peek == 'n') {
          parseType = nullptr;
        } else if (*peek == '-' || (*peek >= '0' && *peek <= '9')) {
          const char* p = peek + 1;
          while (*p >= '0' && *p <= '9') p++;
          parseType = (*p == '.' || *p == 'e' || *p == 'E')
              ? oatpp::Float64::Class::getType()
              : oatpp::Int64::Class::getType();
        } else {
          parseType = nullptr;
        }
      }

      /* Parse the field value */
      oatpp::Void fieldValue;
      if (!deserializeValue(jsonData, items, idx, parseType, fieldValue,
                           mapperConfig)) {
        return false;
      }

      /* Set the field on the DTO object */
      if (isAnyField) {
        const data::type::Type* storedType = fieldValue.getValueType();

        if (!fieldValue && (prop->info.typeSelector || parseType)) {
          storedType = parseType;  /* parseType already reflects typeSelector resolution */
          auto ah = std::make_shared<data::type::AnyHandle>(
              std::shared_ptr<void>(nullptr), storedType);
          prop->set(object, oatpp::Void(ah, oatpp::Any::Class::getType()));
        } else {
          if (!storedType) storedType = parseType;
          if (!storedType && fieldValue.getPtr()) {
            storedType = oatpp::String::Class::getType();
          }
          auto ah = std::make_shared<data::type::AnyHandle>(
              fieldValue.getPtr(), storedType);
          prop->set(object, oatpp::Void(ah, oatpp::Any::Class::getType()));
        }
      } else {
        prop->set(object, fieldValue);
      }
    } else if (!mapperConfig.allowUnknownFields) {
      /* Unknown field — error if not allowed */
      return false;
    } else {
      /* Unknown field — skip its value */
      oatpp::Void dummy;
      if (!deserializeValue(jsonData, items, idx, nullptr, dummy,
                           mapperConfig)) return false;
    }

    /* Advance past ',' or '}' */
    if (idx < items.size() && items[idx].type == ',') {
      idx++;
    } else if (idx < items.size() && items[idx].type == '}') {
      idx++;
      break;
    }
  }

  /* Verify required fields are present */
  for (auto const& field : props->getList()) {
    if (field->info.required) {
      auto fv = field->get(object);
      if (!fv) return false;
    }
  }

  return true;
}

// ============================================================================
// deserializeArray — deserialize a JSON array into a collection
// ============================================================================

static bool deserializeArray(
    const char* jsonData,
    const std::vector<StructuralItem>& items,
    size_t& idx,
    const data::type::Type* type,
    oatpp::Void& result,
    const data::mapping::TreeToObjectMapper::Config& mapperConfig)
{
  idx++; /* skip '[' */

  auto dispatcher =
      static_cast<const data::type::__class::Collection::PolymorphicDispatcher*>(
          type->polymorphicDispatcher);
  result = dispatcher->createObject();
  const data::type::Type* itemType = dispatcher->getItemType();

  while (idx < items.size()) {
    oatpp::Void itemValue;
    bool ok = deserializeValue(jsonData, items, idx, itemType, itemValue,
                               mapperConfig);

    /* Empty array: deserializeValue fails on ']' (zero-length value) */
    if (!ok && items[idx].type == ']') { idx++; return true; }
    if (!ok) return false;

    dispatcher->addItem(result, itemValue);

    /* After deserializeValue, idx points to ',' or ']' */
    if (idx < items.size() && items[idx].type == ']') { idx++; return true; }
    if (idx < items.size() && items[idx].type == ',') idx++;
  }

  return false;
}

// ============================================================================
// deserializeMap — deserialize a JSON object into a map (PairList / UnorderedMap)
// ============================================================================

static bool deserializeMap(
    const char* jsonData,
    const std::vector<StructuralItem>& items,
    size_t& idx,
    const data::type::Type* type,
    oatpp::Void& result,
    const data::mapping::TreeToObjectMapper::Config& mapperConfig)
{
  idx++; /* skip '{' */

  auto dispatcher =
      static_cast<const data::type::__class::Map::PolymorphicDispatcher*>(
          type->polymorphicDispatcher);
  const data::type::Type* keyType = dispatcher->getKeyType();
  const data::type::Type* valueType = dispatcher->getValueType();
  result = dispatcher->createObject();

  while (idx < items.size()) {
    /* Empty map — '}' ends immediately */
    if (items[idx].type == '}') { idx++; return true; }

    /* Deserialize key (must be a JSON string) */
    if (items[idx].type != '"' || items[idx].flags != 0) return false;
    auto key = deserializeJsonString(jsonData, items, idx);
    if (!key) return false;

    /* Expect ':' separator */
    if (idx >= items.size() || items[idx].type != ':') return false;
    idx++; /* skip ':' */

    /* Parse the value */
    oatpp::Void kv, value;
    if (keyType->classId == oatpp::String::Class::CLASS_ID) {
      kv = key;
    }
    if (!deserializeValue(jsonData, items, idx, valueType, value,
                          mapperConfig)) return false;

    dispatcher->addItem(result, kv, value);

    /* After deserializeValue, idx points to ',' or '}' */
    if (idx < items.size() && items[idx].type == ',') {
      idx++;
    } else if (idx < items.size() && items[idx].type == '}') {
      idx++;
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

  if (!jsonData || jsonLen < 2 || !type) return nullptr;

  /* Stage 1: scan JSON buffer for structural characters */
  std::vector<StructuralItem> items;
  if (!scanStructural(reinterpret_cast<const uint8_t*>(jsonData), jsonLen, items)) {
    return nullptr;
  }

  /*
   * Handle top-level primitives.
   * true / false / null / numbers have no structural characters
   * ('{' '}' '[' ']' ',' ':'), so items will be empty. We synthesize a
   * separator-like marker at the end of the buffer for deserializePrimitiveValue
   * to use as a value boundary.
   */
  if (items.empty()) {
    if (jsonLen > 0) {
      StructuralItem si;
      si.pos = static_cast<uint32_t>(jsonLen);
      si.type = 0;
      si.flags = 0;
      items.push_back(si);
    } else {
      return nullptr;
    }
  }

  /* Stage 2+3: recursive value parse from structural index */
  size_t idx = 0;
  oatpp::Void result;
  if (deserializeValue(jsonData, items, idx, type, result, mapperConfig)) {
    caret.setPosition(caret.getDataSize());
    return result;
  }

  /* Report error if parsing failed */
  errorStack.push("[oatpp::json::FastDeserializer::deserialize()]: "
                   "Error. Can't deserialize.");
  return nullptr;
}

}} // namespace oatpp::json
