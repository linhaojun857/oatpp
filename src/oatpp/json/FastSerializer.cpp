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

#include "FastSerializer.hpp"

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

#include "oatpp/data/mapping/ObjectMapper.hpp"

#include <cstring>

namespace oatpp { namespace json {

// ============================================================================
// JSON string serialization
// ============================================================================

static void serializeJsonString(
    data::stream::ConsistentOutputStream* stream,
    const char* data, v_buff_size size, v_uint32 escapeFlags)
{
  stream->writeCharSimple('"');

  /* Fast path: no characters needing escape */
  if (Utils::isSimpleString(data, size) &&
      (!(escapeFlags & Utils::FLAG_ESCAPE_SOLIDUS) || !std::memchr(data, '/', size))) {
    stream->writeSimple(data, size);
    stream->writeCharSimple('"');
    return;
  }

  /* Slow path: scan and escape character by character */
  v_buff_size i = 0;
  while (i < size) {
    /* Find next safe segment */
    v_buff_size seg = i;
    while (i < size) {
      v_char8 c = static_cast<v_char8>(data[i]);
      if (c < 32 || c == '"' || c == '\\' ||
          (c == '/' && (escapeFlags & Utils::FLAG_ESCAPE_SOLIDUS))) {
        break;
      }
      i++;
    }

    /* Write safe segment */
    if (i > seg) {
      stream->writeSimple(data + seg, i - seg);
    }

    /* Write escaped character */
    if (i < size) {
      v_char8 c = static_cast<v_char8>(data[i]);
      switch (c) {
        case '\b': stream->writeSimple("\\b", 2); break;
        case '\f': stream->writeSimple("\\f", 2); break;
        case '\n': stream->writeSimple("\\n", 2); break;
        case '\r': stream->writeSimple("\\r", 2); break;
        case '\t': stream->writeSimple("\\t", 2); break;
        case '"':  stream->writeSimple("\\\"", 2); break;
        case '\\': stream->writeSimple("\\\\", 2); break;
        case '/':  stream->writeSimple("\\/", 2);  break;
        default:
          if (c < 32) {
            char buf[6] = {'\\', 'u', '0', '0', 0, 0};
            buf[4] = "0123456789abcdef"[c >> 4];
            buf[5] = "0123456789abcdef"[c & 0xF];
            stream->writeSimple(buf, 6);
          }
          break;
      }
      i++;
    }
  }
  stream->writeCharSimple('"');
}

// ============================================================================
// Null-value detection
// ============================================================================

static bool isNullData(const oatpp::Void& value, const data::type::Type* type) {
  if (!value) return true;

  const auto& cid = type->classId;

  if (cid == oatpp::Boolean::Class::CLASS_ID)  return !value.cast<oatpp::Boolean>().getPtr();
  if (cid == oatpp::Int8::Class::CLASS_ID)     return !value.cast<oatpp::Int8>();
  if (cid == oatpp::UInt8::Class::CLASS_ID)    return !value.cast<oatpp::UInt8>();
  if (cid == oatpp::Int16::Class::CLASS_ID)    return !value.cast<oatpp::Int16>();
  if (cid == oatpp::UInt16::Class::CLASS_ID)   return !value.cast<oatpp::UInt16>();
  if (cid == oatpp::Int32::Class::CLASS_ID)    return !value.cast<oatpp::Int32>();
  if (cid == oatpp::UInt32::Class::CLASS_ID)   return !value.cast<oatpp::UInt32>();
  if (cid == oatpp::Int64::Class::CLASS_ID)    return !value.cast<oatpp::Int64>();
  if (cid == oatpp::UInt64::Class::CLASS_ID)   return !value.cast<oatpp::UInt64>();
  if (cid == oatpp::Float32::Class::CLASS_ID)  return !value.cast<oatpp::Float32>();
  if (cid == oatpp::Float64::Class::CLASS_ID)  return !value.cast<oatpp::Float64>();
  if (cid == oatpp::String::Class::CLASS_ID)   return !value.cast<oatpp::String>();

  return false;
}

// ============================================================================
// Beautifier helpers (file-scope statics)
// ============================================================================

static void writeIndent(data::stream::ConsistentOutputStream* stream,
                         const Serializer::Config& jsonConfig, v_int32 level)
{
  if (!jsonConfig.useBeautifier || level <= 0) return;
  for (v_int32 i = 0; i < level; i++) {
    stream->writeSimple(jsonConfig.beautifierIndent->data(),
                        static_cast<v_buff_size>(jsonConfig.beautifierIndent->size()));
  }
}

static void writeNewline(data::stream::ConsistentOutputStream* stream,
                          const Serializer::Config& jsonConfig)
{
  if (jsonConfig.useBeautifier) {
    stream->writeSimple(jsonConfig.beautifierNewLine->data(),
                        static_cast<v_buff_size>(jsonConfig.beautifierNewLine->size()));
  }
}

static void writeContainerNewline(data::stream::ConsistentOutputStream* stream,
                                   const Serializer::Config& jsonConfig, v_int32 indent)
{
  writeNewline(stream, jsonConfig);
  writeIndent(stream, jsonConfig, indent);
}

// ============================================================================
// Numeric value serialization helpers
// ============================================================================

/**
 * Serialize a signed integer value to the output stream.
 * Buffer sizing: 8 chars for int8/int16, 16 for int32, 24 for int64.
 */
static bool serializeSignedInteger(
    data::stream::ConsistentOutputStream* stream,
    const std::shared_ptr<void>& ptr,
    const data::type::ClassId& cid)
{
  if (!ptr) {
    stream->writeSimple("null", 4);
    return true;
  }

  if (cid == oatpp::Int8::Class::CLASS_ID) {
    char buf[8];
    auto n = Utils::int64ToChars(*std::static_pointer_cast<v_int8>(ptr), buf);
    stream->writeSimple(buf, n);
  } else if (cid == oatpp::Int16::Class::CLASS_ID) {
    char buf[8];
    auto n = Utils::int64ToChars(*std::static_pointer_cast<v_int16>(ptr), buf);
    stream->writeSimple(buf, n);
  } else if (cid == oatpp::Int32::Class::CLASS_ID) {
    char buf[16];
    auto n = Utils::int64ToChars(*std::static_pointer_cast<v_int32>(ptr), buf);
    stream->writeSimple(buf, n);
  } else if (cid == oatpp::Int64::Class::CLASS_ID) {
    char buf[24];
    auto n = Utils::int64ToChars(*std::static_pointer_cast<v_int64>(ptr), buf);
    stream->writeSimple(buf, n);
  } else {
    return false;
  }
  return true;
}

/**
 * Serialize an unsigned integer value to the output stream.
 * Buffer sizing: 8 chars for uint8/uint16, 16 for uint32, 24 for uint64.
 */
static bool serializeUnsignedInteger(
    data::stream::ConsistentOutputStream* stream,
    const std::shared_ptr<void>& ptr,
    const data::type::ClassId& cid)
{
  if (!ptr) {
    stream->writeSimple("null", 4);
    return true;
  }

  if (cid == oatpp::UInt8::Class::CLASS_ID) {
    char buf[8];
    auto n = Utils::int64ToChars(*std::static_pointer_cast<v_uint8>(ptr), buf);
    stream->writeSimple(buf, n);
  } else if (cid == oatpp::UInt16::Class::CLASS_ID) {
    char buf[8];
    auto n = Utils::int64ToChars(*std::static_pointer_cast<v_uint16>(ptr), buf);
    stream->writeSimple(buf, n);
  } else if (cid == oatpp::UInt32::Class::CLASS_ID) {
    char buf[16];
    auto n = Utils::uint64ToChars(*std::static_pointer_cast<v_uint32>(ptr), buf);
    stream->writeSimple(buf, n);
  } else if (cid == oatpp::UInt64::Class::CLASS_ID) {
    char buf[24];
    auto n = Utils::uint64ToChars(*std::static_pointer_cast<v_uint64>(ptr), buf);
    stream->writeSimple(buf, n);
  } else {
    return false;
  }
  return true;
}

// ============================================================================
// Enum serialization helpers
// ============================================================================

/**
 * Serialize an Enum value. Returns false if a constraint (e.g. NotNull) is
 * violated, and pushes an error message to the optional errorStack.
 */
static bool serializeEnum(
    data::stream::ConsistentOutputStream* stream,
    const oatpp::Void& value,
    const data::type::Type* type,
    const data::mapping::ObjectToTreeMapper::Config& mapperConfig,
    const Serializer::Config& jsonConfig,
    data::mapping::ErrorStack& errorStack)
{
  auto pdisp = static_cast<const data::type::__class::AbstractEnum::PolymorphicDispatcher*>(
      type->polymorphicDispatcher);
  auto* interpType = pdisp->getInterpretationType();

  /* Convert enum to its interpreted value (string or number) */
  data::type::EnumInterpreterError e = data::type::EnumInterpreterError::OK;
  auto interp = pdisp->toInterpretation(value, mapperConfig.useUnqualifiedEnumNames, e);

  if (e != data::type::EnumInterpreterError::OK) {
    errorStack.push("[oatpp::json::FastSerializer::serialize()]: "
                     "Error. Enum constraint violated - 'NotNull'.");
    return false;
  }

  /* Null interpretation → serialize as "null" */
  if (!interp) {
    stream->writeSimple("null", 4);
    return true;
  }

  bool isNumeric = (interpType->classId != oatpp::String::Class::CLASS_ID);

  if (!isNumeric) {
    /* String interpretation */
    const auto& s = interp.cast<oatpp::String>();
    if (!s) {
      stream->writeSimple("null", 4);
      return true;
    }
    serializeJsonString(stream, s->data(),
        static_cast<v_buff_size>(s->size()), jsonConfig.escapeFlags);
    return true;
  }

  /* Numeric interpretation — serialize the underlying value directly */
  if (!value) {
    stream->writeSimple("null", 4);
    return true;
  }

  auto sp = value.getPtr();
  const auto& numCid = interpType->classId;

  if (numCid == oatpp::Float32::Class::CLASS_ID) {
    if (!sp) { stream->writeSimple("null", 4); return true; }
    char buf[64];
    auto n = Utils::float64ToChars(
        static_cast<v_float64>(*std::static_pointer_cast<v_float32>(sp)), buf);
    stream->writeSimple(buf, n);
    return true;
  }
  if (numCid == oatpp::Float64::Class::CLASS_ID) {
    if (!sp) { stream->writeSimple("null", 4); return true; }
    char buf[64];
    auto n = Utils::float64ToChars(*std::static_pointer_cast<v_float64>(sp), buf);
    stream->writeSimple(buf, n);
    return true;
  }

  return serializeSignedInteger(stream, sp, numCid) ||
         serializeUnsignedInteger(stream, sp, numCid);
}

// ============================================================================
// Forward declarations
// ============================================================================

static bool serializeImpl(
    data::stream::ConsistentOutputStream* stream,
    const oatpp::Void& variant,
    data::mapping::ErrorStack& errorStack,
    const data::mapping::ObjectToTreeMapper::Config& mapperConfig,
    const Serializer::Config& jsonConfig,
    v_int32 indent);

// ============================================================================
// DTO Object serialization
// ============================================================================

static bool serializeObject(
    data::stream::ConsistentOutputStream* stream,
    const oatpp::Void& value,
    const data::type::Type* type,
    const data::mapping::ObjectToTreeMapper::Config& mapperConfig,
    const Serializer::Config& jsonConfig,
    data::mapping::ErrorStack& errorStack,
    v_int32 indent)
{
  bool useUnqualifiedFieldNames = mapperConfig.useUnqualifiedFieldNames;
  bool includeNullFields = mapperConfig.includeNullFields;

  auto dispatcher = static_cast<const data::type::__class::AbstractObject::PolymorphicDispatcher*>(
      type->polymorphicDispatcher);
  auto props = dispatcher->getProperties();
  auto object = static_cast<oatpp::BaseObject*>(value.get());

  stream->writeCharSimple('{');
  writeContainerNewline(stream, jsonConfig, indent + 1);

  v_int32 childIndent = indent + 1;
  bool first = true;

  for (auto const& field : props->getList()) {
    oatpp::Void fv;

    /* Resolve field value, handling Any-with-typeSelector */
    if (field->info.typeSelector && field->type == oatpp::Any::Class::getType()) {
      const auto& any = field->get(object).cast<oatpp::Any>();
      fv = any.retrieve(field->info.typeSelector->selectType(object));
    } else {
      fv = field->get(object);
    }

    /* Required-field null check */
    if (field->info.required && isNullData(fv, fv.getValueType())) {
      if (!mapperConfig.alwaysIncludeRequired) {
        const std::string& key = useUnqualifiedFieldNames
            ? field->unqualifiedName : field->name;
        errorStack.push("[oatpp::json::FastSerializer::serialize()]: Error. "
                         + std::string(type->nameQualifier) + "::" + key
                         + " is required!");
        return false;
      }
    }

    /* Skip null fields if configured to do so */
    if (isNullData(fv, fv.getValueType())) {
      bool includeForRequired =
          field->info.required && mapperConfig.alwaysIncludeRequired;
      if (!includeNullFields && !jsonConfig.includeNullElements && !includeForRequired)
        continue;
    }

    /* Write comma + newline + indent before each field (except first) */
    if (!first) {
      stream->writeCharSimple(',');
      writeContainerNewline(stream, jsonConfig, childIndent);
    }
    first = false;

    /* Serialize field key */
    const std::string& key = useUnqualifiedFieldNames
        ? field->unqualifiedName : field->name;
    serializeJsonString(stream, key.data(),
        static_cast<v_buff_size>(key.size()), jsonConfig.escapeFlags);
    stream->writeCharSimple(':');
    if (jsonConfig.useBeautifier) {
      stream->writeCharSimple(' ');
    }

    /* Serialize value recursively */
    if (!serializeImpl(stream, fv, errorStack,
            mapperConfig, jsonConfig, childIndent)) {
      return false;
    }
  }

  writeContainerNewline(stream, jsonConfig, indent);
  stream->writeCharSimple('}');
  return true;
}

// ============================================================================
// Collection serialization (Vector / List / UnorderedSet)
// ============================================================================

static bool serializeCollection(
    data::stream::ConsistentOutputStream* stream,
    const oatpp::Void& value,
    const data::type::Type* type,
    const data::mapping::ObjectToTreeMapper::Config& mapperConfig,
    const Serializer::Config& jsonConfig,
    data::mapping::ErrorStack& errorStack,
    v_int32 indent)
{

  auto dispatcher = static_cast<const data::type::__class::Collection::PolymorphicDispatcher*>(
      type->polymorphicDispatcher);
  auto it = dispatcher->beginIteration(value);

  stream->writeCharSimple('[');
  writeContainerNewline(stream, jsonConfig, indent + 1);

  v_int32 childIndent = indent + 1;
  bool first = true;

  while (!it->finished()) {
    const auto& item = it->get();

    /* Skip null collection elements unless configured otherwise */
    if (!item && !mapperConfig.includeNullFields &&
        !mapperConfig.alwaysIncludeNullCollectionElements) {
      it->next();
      continue;
    }

    if (!first) {
      stream->writeCharSimple(',');
      writeContainerNewline(stream, jsonConfig, childIndent);
    }
    first = false;

    if (!serializeImpl(stream, item, errorStack,
            mapperConfig, jsonConfig, childIndent)) {
      return false;
    }

    it->next();
  }

  writeContainerNewline(stream, jsonConfig, indent);
  stream->writeCharSimple(']');
  return true;
}

// ============================================================================
// Map serialization (PairList / UnorderedMap)
// ============================================================================

static bool serializeMap(
    data::stream::ConsistentOutputStream* stream,
    const oatpp::Void& value,
    const data::type::Type* type,
    const data::mapping::ObjectToTreeMapper::Config& mapperConfig,
    const Serializer::Config& jsonConfig,
    data::mapping::ErrorStack& errorStack,
    v_int32 indent)
{

  auto dispatcher = static_cast<const data::type::__class::Map::PolymorphicDispatcher*>(
      type->polymorphicDispatcher);
  auto it = dispatcher->beginIteration(value);

  stream->writeCharSimple('{');
  writeContainerNewline(stream, jsonConfig, indent + 1);

  v_int32 childIndent = indent + 1;
  bool first = true;

  while (!it->finished()) {
    const auto& val = it->getValue();
    const auto& uk = it->getKey();

    /* Skip entries with null keys */
    if (!uk) { it->next(); continue; }

    const auto& key = oatpp::String(std::static_pointer_cast<std::string>(uk.getPtr()));
    if (!key) { it->next(); continue; }

    /* Skip entries with null values unless configured otherwise */
    if (!val && !mapperConfig.includeNullFields &&
        !mapperConfig.alwaysIncludeNullCollectionElements) {
      it->next();
      continue;
    }

    if (!first) {
      stream->writeCharSimple(',');
      writeContainerNewline(stream, jsonConfig, childIndent);
    }
    first = false;

    /* Serialize map key */
    serializeJsonString(stream, key->data(),
        static_cast<v_buff_size>(key->size()), jsonConfig.escapeFlags);
    stream->writeCharSimple(':');
    if (jsonConfig.useBeautifier) {
      stream->writeCharSimple(' ');
    }

    /* Serialize map value */
    if (!serializeImpl(stream, val, errorStack,
            mapperConfig, jsonConfig, childIndent)) {
      return false;
    }

    it->next();
  }

  writeContainerNewline(stream, jsonConfig, indent);
  stream->writeCharSimple('}');
  return true;
}

// ============================================================================
// serializeImpl — main dispatch (internal, carries indent for beautifier)
// ============================================================================

static bool serializeImpl(
    data::stream::ConsistentOutputStream* stream,
    const oatpp::Void& variant,
    data::mapping::ErrorStack& errorStack,
    const data::mapping::ObjectToTreeMapper::Config& mapperConfig,
    const Serializer::Config& jsonConfig,
    v_int32 indent)
{
  const auto* type = variant.getValueType();
  const std::vector<std::string>* enabledInterpretations =
      &mapperConfig.enabledInterpretations;

  const auto& cid = type->classId;

  // ------------------------------------------------------------------
  // Enum
  // ------------------------------------------------------------------
  if (cid == data::type::__class::AbstractEnum::CLASS_ID) {
    return serializeEnum(stream, variant, type, mapperConfig, jsonConfig, errorStack);
  }

  // ------------------------------------------------------------------
  // Null check for value types
  // ------------------------------------------------------------------
  if (!variant || isNullData(variant, type)) {
    stream->writeSimple("null", 4);
    return true;
  }

  // ------------------------------------------------------------------
  // Signed integers
  // ------------------------------------------------------------------
  if (cid == oatpp::Int8::Class::CLASS_ID) {
    auto v = variant.cast<oatpp::Int8>();
    char buf[8];
    auto n = Utils::int64ToChars(*v, buf);
    stream->writeSimple(buf, n);
    return true;
  }
  if (cid == oatpp::Int16::Class::CLASS_ID) {
    auto v = variant.cast<oatpp::Int16>();
    char buf[8];
    auto n = Utils::int64ToChars(*v, buf);
    stream->writeSimple(buf, n);
    return true;
  }
  if (cid == oatpp::Int32::Class::CLASS_ID) {
    auto v = variant.cast<oatpp::Int32>();
    char buf[16];
    auto n = Utils::int64ToChars(*v, buf);
    stream->writeSimple(buf, n);
    return true;
  }
  if (cid == oatpp::Int64::Class::CLASS_ID) {
    auto v = variant.cast<oatpp::Int64>();
    char buf[24];
    auto n = Utils::int64ToChars(*v, buf);
    stream->writeSimple(buf, n);
    return true;
  }

  // ------------------------------------------------------------------
  // Unsigned integers
  // ------------------------------------------------------------------
  if (cid == oatpp::UInt8::Class::CLASS_ID) {
    auto v = variant.cast<oatpp::UInt8>();
    char buf[8];
    auto n = Utils::int64ToChars(*v, buf);
    stream->writeSimple(buf, n);
    return true;
  }
  if (cid == oatpp::UInt16::Class::CLASS_ID) {
    auto v = variant.cast<oatpp::UInt16>();
    char buf[8];
    auto n = Utils::int64ToChars(*v, buf);
    stream->writeSimple(buf, n);
    return true;
  }
  if (cid == oatpp::UInt32::Class::CLASS_ID) {
    auto v = variant.cast<oatpp::UInt32>();
    char buf[16];
    auto n = Utils::uint64ToChars(*v, buf);
    stream->writeSimple(buf, n);
    return true;
  }
  if (cid == oatpp::UInt64::Class::CLASS_ID) {
    auto v = variant.cast<oatpp::UInt64>();
    char buf[24];
    auto n = Utils::uint64ToChars(*v, buf);
    stream->writeSimple(buf, n);
    return true;
  }

  // ------------------------------------------------------------------
  // Floating point
  // ------------------------------------------------------------------
  if (cid == oatpp::Float32::Class::CLASS_ID) {
    auto v = variant.cast<oatpp::Float32>();
    char buf[64];
    auto n = Utils::float64ToChars(static_cast<v_float64>(*v), buf);
    stream->writeSimple(buf, n);
    return true;
  }
  if (cid == oatpp::Float64::Class::CLASS_ID) {
    auto v = variant.cast<oatpp::Float64>();
    char buf[64];
    auto n = Utils::float64ToChars(*v, buf);
    stream->writeSimple(buf, n);
    return true;
  }

  // ------------------------------------------------------------------
  // Boolean
  // ------------------------------------------------------------------
  if (cid == oatpp::Boolean::Class::CLASS_ID) {
    auto v = variant.cast<oatpp::Boolean>();
    stream->writeSimple(*v ? "true" : "false", *v ? 4 : 5);
    return true;
  }

  // ------------------------------------------------------------------
  // String
  // ------------------------------------------------------------------
  if (cid == oatpp::String::Class::CLASS_ID) {
    const auto& s = variant.cast<oatpp::String>();
    serializeJsonString(stream, s->data(),
        static_cast<v_buff_size>(s->size()), jsonConfig.escapeFlags);
    return true;
  }

  // ------------------------------------------------------------------
  // Any (unwrap and recurse)
  // ------------------------------------------------------------------
  if (cid == oatpp::Any::Class::CLASS_ID) {
    auto ah = static_cast<data::type::AnyHandle*>(variant.get());
    return serializeImpl(stream, oatpp::Void(ah->ptr, ah->type), errorStack,
        mapperConfig, jsonConfig, indent);
  }

  // ------------------------------------------------------------------
  // DTO Object
  // ------------------------------------------------------------------
  if (cid == data::type::__class::AbstractObject::CLASS_ID) {
    return serializeObject(stream, variant, type, mapperConfig, jsonConfig,
        errorStack, indent);
  }

  // ------------------------------------------------------------------
  // Collections: Vector / List / UnorderedSet
  // ------------------------------------------------------------------
  if (cid == data::type::__class::AbstractVector::CLASS_ID ||
      cid == data::type::__class::AbstractList::CLASS_ID ||
      cid == data::type::__class::AbstractUnorderedSet::CLASS_ID) {
    return serializeCollection(stream, variant, type, mapperConfig, jsonConfig,
        errorStack, indent);
  }

  // ------------------------------------------------------------------
  // Maps: PairList / UnorderedMap
  // ------------------------------------------------------------------
  if (cid == data::type::__class::AbstractPairList::CLASS_ID ||
      cid == data::type::__class::AbstractUnorderedMap::CLASS_ID) {
    return serializeMap(stream, variant, type, mapperConfig, jsonConfig,
        errorStack, indent);
  }

  // ------------------------------------------------------------------
  // Unknown type — try interpretation chain
  // ------------------------------------------------------------------
  if (enabledInterpretations && !enabledInterpretations->empty()) {
    auto* interp = type->findInterpretation(*enabledInterpretations);
    if (interp) {
      auto interpreted = interp->toInterpretation(variant);
      if (interpreted && interpreted.getValueType()) {
        return serializeImpl(stream, interpreted, errorStack,
            mapperConfig, jsonConfig, indent);
      }
    }
  }

  return false;
}

// ============================================================================
// serialize — public entry point
// ============================================================================

bool FastSerializer::serialize(
    data::stream::ConsistentOutputStream* stream,
    const oatpp::Void& variant,
    data::mapping::ErrorStack& errorStack,
    const data::mapping::ObjectToTreeMapper::Config& mapperConfig,
    const Serializer::Config& jsonConfig)
{
  return serializeImpl(stream, variant, errorStack, mapperConfig, jsonConfig, 0);
}

}} // namespace oatpp::json
