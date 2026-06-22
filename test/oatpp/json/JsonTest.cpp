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

#include "JsonTest.hpp"

#include <cmath>
#include <limits>
#include <cstring>

#include "oatpp/json/ObjectMapper.hpp"
#include "oatpp/utils/Conversion.hpp"
#include "oatpp/base/Log.hpp"
#include "oatpp/macro/codegen.hpp"

namespace oatpp { namespace json {

namespace {

#include OATPP_CODEGEN_BEGIN(DTO)

// ============================================================================
// DTO definitions for testing
// ============================================================================

class SimplePrimitiveDto : public oatpp::DTO {
  DTO_INIT(SimplePrimitiveDto, DTO)
  DTO_FIELD(Int8,    f_int8);
  DTO_FIELD(UInt8,   f_uint8);
  DTO_FIELD(Int16,   f_int16);
  DTO_FIELD(UInt16,  f_uint16);
  DTO_FIELD(Int32,   f_int32);
  DTO_FIELD(UInt32,  f_uint32);
  DTO_FIELD(Int64,   f_int64);
  DTO_FIELD(UInt64,  f_uint64);
  DTO_FIELD(Float32, f_float32);
  DTO_FIELD(Float64, f_float64);
  DTO_FIELD(Boolean, f_bool);
  DTO_FIELD(String,  f_str);
};

class RequiredFieldDto : public oatpp::DTO {
  DTO_INIT(RequiredFieldDto, DTO)
  DTO_FIELD_INFO(f_required) {
    info->required = true;
  }
  DTO_FIELD(String, f_required);
  DTO_FIELD(String, f_optional);
};

class ChildDto : public oatpp::DTO {
  DTO_INIT(ChildDto, DTO)
  DTO_FIELD(String, name);
  DTO_FIELD(Int32, value);
};

class RequiredChildDto : public oatpp::DTO {
  DTO_INIT(RequiredChildDto, DTO)
  DTO_FIELD_INFO(name) {
    info->required = true;
  }
  DTO_FIELD(String, name);
};

class ComplexDto : public oatpp::DTO {
  DTO_INIT(ComplexDto, DTO)
  DTO_FIELD(String, title);
  DTO_FIELD(Object<ChildDto>, child);
  DTO_FIELD(List<String>, string_list);
  DTO_FIELD(List<Int32>, int_list);
  DTO_FIELD(List<Object<ChildDto>>, object_list);
  DTO_FIELD(Vector<Float64>, float_vector);
  DTO_FIELD(Fields<String>, string_map);
  DTO_FIELD(UnorderedFields<Int32>, int_map);
  DTO_FIELD(UnorderedSet<String>, string_set);
};

class NestedParentDto : public oatpp::DTO {
  DTO_INIT(NestedParentDto, DTO)
  DTO_FIELD(String, name);
  DTO_FIELD(Object<ComplexDto>, complex);
};

class AnyContainerDto : public oatpp::DTO {
  DTO_INIT(AnyContainerDto, DTO)
  DTO_FIELD(Any, any_field);
  DTO_FIELD(List<Any>, any_list);
};

class ParentWithRequiredChildDto : public oatpp::DTO {
  DTO_INIT(ParentWithRequiredChildDto, DTO)
  DTO_FIELD(String, field_string);
  DTO_FIELD(Object<RequiredChildDto>, child);
};

ENUM(TestEnum, v_int32,
  VALUE(V1, 10, "enum-value-1"),
  VALUE(V2, 20, "enum-value-2"),
  VALUE(V3, 30, "enum-value-3")
);

class EnumContainerDto : public oatpp::DTO {
  DTO_INIT(EnumContainerDto, DTO)
  DTO_FIELD(Enum<TestEnum>::AsString, enum_str);
  DTO_FIELD(Enum<TestEnum>::AsNumber, enum_num);
};

class DeepNodeDto : public oatpp::DTO {
  DTO_INIT(DeepNodeDto, DTO)
  DTO_FIELD(String, label);
  DTO_FIELD(Int32, index);
  DTO_FIELD(Object<DeepNodeDto>, next);
};

class EmptyDto : public oatpp::DTO {
  DTO_INIT(EmptyDto, DTO)
};

class EmptyContainersDto : public oatpp::DTO {
  DTO_INIT(EmptyContainersDto, DTO)
  DTO_FIELD(List<String>, empty_list) = List<String>::createShared();
  DTO_FIELD(Fields<String>, empty_map) = Fields<String>::createShared();
  DTO_FIELD(Vector<Int32>, empty_vector) = Vector<Int32>::createShared();
  DTO_FIELD(UnorderedSet<String>, empty_set) = UnorderedSet<String>::createShared();
};

#include OATPP_CODEGEN_END(DTO)

} // anonymous namespace

// ============================================================================
// Helper: round-trip test
// ============================================================================

template<typename T>
static typename T::Wrapper roundTrip(const typename T::Wrapper& original,
                                      oatpp::json::ObjectMapper& mapper,
                                      bool expectSuccess = true)
{
  auto json = mapper.writeToString(original);
  if (!expectSuccess) {
    OATPP_ASSERT(json == nullptr);
    return nullptr;
  }
  OATPP_ASSERT(json != nullptr);

  auto caret = oatpp::utils::parser::Caret(json);
  auto result = mapper.readFromCaret<typename T::Wrapper>(caret);
  OATPP_ASSERT(result != nullptr);

  auto json2 = mapper.writeToString(result);
  OATPP_ASSERT(json2 != nullptr);
  OATPP_ASSERT(json == json2);

  return result;
}

// ============================================================================
// Test runner
// ============================================================================

void JsonTest::onRun() {

  oatpp::json::ObjectMapper mapper;

  // ==========================================================================
  // SECTION 1: Primitive types — Int8
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Int8: positive")
    auto val = oatpp::Int8(static_cast<v_int8>(42));
    auto json = mapper.writeToString(val);
    OATPP_ASSERT(json == "42");
    auto result = mapper.readFromString<oatpp::Int8>(json);
    OATPP_ASSERT(result == 42);
  }
  {
    OATPP_LOGd(TAG, "Int8: negative")
    auto val = oatpp::Int8(static_cast<v_int8>(-100));
    auto json = mapper.writeToString(val);
    OATPP_ASSERT(json == "-100");
    auto result = mapper.readFromString<oatpp::Int8>(json);
    OATPP_ASSERT(result == -100);
  }
  {
    OATPP_LOGd(TAG, "Int8: zero")
    auto val = oatpp::Int8(static_cast<v_int8>(0));
    auto json = mapper.writeToString(val);
    OATPP_ASSERT(json == "0");
    auto result = mapper.readFromString<oatpp::Int8>(json);
    OATPP_ASSERT(result.getValue(-1) == 0);
  }
  {
    OATPP_LOGd(TAG, "Int8: min")
    auto val = oatpp::Int8(std::numeric_limits<v_int8>::min());
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::Int8>(json);
    OATPP_ASSERT(result == std::numeric_limits<v_int8>::min());
  }
  {
    OATPP_LOGd(TAG, "Int8: max")
    auto val = oatpp::Int8(std::numeric_limits<v_int8>::max());
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::Int8>(json);
    OATPP_ASSERT(result == std::numeric_limits<v_int8>::max());
  }
  {
    OATPP_LOGd(TAG, "Int8: null")
    oatpp::Int8 nullVal;
    auto json = mapper.writeToString(nullVal);
    OATPP_ASSERT(json == "null");
    auto result = mapper.readFromString<oatpp::Int8>(json);
    OATPP_ASSERT(result == nullptr);
  }

  // ==========================================================================
  // SECTION 2: Primitive types — UInt8
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "UInt8: value")
    auto val = oatpp::UInt8(static_cast<v_uint8>(200));
    auto json = mapper.writeToString(val);
    OATPP_ASSERT(json == "200");
    auto result = mapper.readFromString<oatpp::UInt8>(json);
    OATPP_ASSERT(result == 200);
  }
  {
    OATPP_LOGd(TAG, "UInt8: max")
    auto val = oatpp::UInt8(std::numeric_limits<v_uint8>::max());
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::UInt8>(json);
    OATPP_ASSERT(result == std::numeric_limits<v_uint8>::max());
  }
  {
    OATPP_LOGd(TAG, "UInt8: zero")
    auto val = oatpp::UInt8(static_cast<v_uint8>(0));
    auto json = mapper.writeToString(val);
    OATPP_ASSERT(json == "0");
    auto result = mapper.readFromString<oatpp::UInt8>(json);
    OATPP_ASSERT(result == 0);
  }
  {
    OATPP_LOGd(TAG, "UInt8: null")
    oatpp::UInt8 nullVal;
    auto json = mapper.writeToString(nullVal);
    OATPP_ASSERT(json == "null");
    auto result = mapper.readFromString<oatpp::UInt8>(json);
    OATPP_ASSERT(result == nullptr);
  }

  // ==========================================================================
  // SECTION 3: Primitive types — Int16
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Int16: positive")
    auto val = oatpp::Int16(static_cast<v_int16>(30000));
    auto json = mapper.writeToString(val);
    OATPP_ASSERT(json == "30000");
    auto result = mapper.readFromString<oatpp::Int16>(json);
    OATPP_ASSERT(result == 30000);
  }
  {
    OATPP_LOGd(TAG, "Int16: negative")
    auto val = oatpp::Int16(static_cast<v_int16>(-30000));
    auto json = mapper.writeToString(val);
    OATPP_ASSERT(json == "-30000");
    auto result = mapper.readFromString<oatpp::Int16>(json);
    OATPP_ASSERT(result == -30000);
  }
  {
    OATPP_LOGd(TAG, "Int16: min")
    auto val = oatpp::Int16(std::numeric_limits<v_int16>::min());
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::Int16>(json);
    OATPP_ASSERT(result == std::numeric_limits<v_int16>::min());
  }
  {
    OATPP_LOGd(TAG, "Int16: max")
    auto val = oatpp::Int16(std::numeric_limits<v_int16>::max());
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::Int16>(json);
    OATPP_ASSERT(result == std::numeric_limits<v_int16>::max());
  }
  {
    OATPP_LOGd(TAG, "Int16: null")
    oatpp::Int16 nullVal;
    auto json = mapper.writeToString(nullVal);
    OATPP_ASSERT(json == "null");
    auto result = mapper.readFromString<oatpp::Int16>(json);
    OATPP_ASSERT(result == nullptr);
  }

  // ==========================================================================
  // SECTION 4: Primitive types — UInt16
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "UInt16: value")
    auto val = oatpp::UInt16(static_cast<v_uint16>(60000));
    auto json = mapper.writeToString(val);
    OATPP_ASSERT(json == "60000");
    auto result = mapper.readFromString<oatpp::UInt16>(json);
    OATPP_ASSERT(result == 60000);
  }
  {
    OATPP_LOGd(TAG, "UInt16: max")
    auto val = oatpp::UInt16(std::numeric_limits<v_uint16>::max());
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::UInt16>(json);
    OATPP_ASSERT(result == std::numeric_limits<v_uint16>::max());
  }
  {
    OATPP_LOGd(TAG, "UInt16: null")
    oatpp::UInt16 nullVal;
    auto json = mapper.writeToString(nullVal);
    OATPP_ASSERT(json == "null");
    auto result = mapper.readFromString<oatpp::UInt16>(json);
    OATPP_ASSERT(result == nullptr);
  }

  // ==========================================================================
  // SECTION 5: Primitive types — Int32
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Int32: positive")
    auto val = oatpp::Int32(2000000000);
    auto json = mapper.writeToString(val);
    OATPP_ASSERT(json == "2000000000");
    auto result = mapper.readFromString<oatpp::Int32>(json);
    OATPP_ASSERT(result == 2000000000);
  }
  {
    OATPP_LOGd(TAG, "Int32: negative")
    auto val = oatpp::Int32(-2000000000);
    auto json = mapper.writeToString(val);
    OATPP_ASSERT(json == "-2000000000");
    auto result = mapper.readFromString<oatpp::Int32>(json);
    OATPP_ASSERT(result == -2000000000);
  }
  {
    OATPP_LOGd(TAG, "Int32: zero")
    auto val = oatpp::Int32(0);
    auto json = mapper.writeToString(val);
    OATPP_ASSERT(json == "0");
    auto result = mapper.readFromString<oatpp::Int32>(json);
    OATPP_ASSERT(result == 0);
  }
  {
    OATPP_LOGd(TAG, "Int32: min")
    auto val = oatpp::Int32(std::numeric_limits<v_int32>::min());
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::Int32>(json);
    OATPP_ASSERT(result == std::numeric_limits<v_int32>::min());
  }
  {
    OATPP_LOGd(TAG, "Int32: max")
    auto val = oatpp::Int32(std::numeric_limits<v_int32>::max());
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::Int32>(json);
    OATPP_ASSERT(result == std::numeric_limits<v_int32>::max());
  }
  {
    OATPP_LOGd(TAG, "Int32: null")
    oatpp::Int32 nullVal;
    auto json = mapper.writeToString(nullVal);
    OATPP_ASSERT(json == "null");
    auto result = mapper.readFromString<oatpp::Int32>(json);
    OATPP_ASSERT(result == nullptr);
  }

  // ==========================================================================
  // SECTION 6: Primitive types — UInt32
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "UInt32: value")
    auto val = oatpp::UInt32(4000000000u);
    auto json = mapper.writeToString(val);
    OATPP_ASSERT(json == "4000000000");
    auto result = mapper.readFromString<oatpp::UInt32>(json);
    OATPP_ASSERT(result == 4000000000u);
  }
  {
    OATPP_LOGd(TAG, "UInt32: max")
    auto val = oatpp::UInt32(std::numeric_limits<v_uint32>::max());
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::UInt32>(json);
    OATPP_ASSERT(result == std::numeric_limits<v_uint32>::max());
  }
  {
    OATPP_LOGd(TAG, "UInt32: zero")
    auto val = oatpp::UInt32(0u);
    auto json = mapper.writeToString(val);
    OATPP_ASSERT(json == "0");
    auto result = mapper.readFromString<oatpp::UInt32>(json);
    OATPP_ASSERT(result == 0u);
  }
  {
    OATPP_LOGd(TAG, "UInt32: null")
    oatpp::UInt32 nullVal;
    auto json = mapper.writeToString(nullVal);
    OATPP_ASSERT(json == "null");
    auto result = mapper.readFromString<oatpp::UInt32>(json);
    OATPP_ASSERT(result == nullptr);
  }

  // ==========================================================================
  // SECTION 7: Primitive types — Int64
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Int64: positive")
    auto val = oatpp::Int64(9000000000000000000LL);
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::Int64>(json);
    OATPP_ASSERT(result == 9000000000000000000LL);
  }
  {
    OATPP_LOGd(TAG, "Int64: negative")
    auto val = oatpp::Int64(-9000000000000000000LL);
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::Int64>(json);
    OATPP_ASSERT(result == -9000000000000000000LL);
  }
  {
    OATPP_LOGd(TAG, "Int64: zero")
    oatpp::Int64 val;
    val = static_cast<v_int64>(0);
    auto json = mapper.writeToString(val);
    OATPP_ASSERT(json == "0");
    auto result = mapper.readFromString<oatpp::Int64>(json);
    OATPP_ASSERT(result == 0LL);
  }
  {
    OATPP_LOGd(TAG, "Int64: min")
    auto val = oatpp::Int64(std::numeric_limits<v_int64>::min());
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::Int64>(json);
    OATPP_ASSERT(result == std::numeric_limits<v_int64>::min());
  }
  {
    OATPP_LOGd(TAG, "Int64: max")
    auto val = oatpp::Int64(std::numeric_limits<v_int64>::max());
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::Int64>(json);
    OATPP_ASSERT(result == std::numeric_limits<v_int64>::max());
  }
  {
    OATPP_LOGd(TAG, "Int64: null")
    oatpp::Int64 nullVal;
    auto json = mapper.writeToString(nullVal);
    OATPP_ASSERT(json == "null");
    auto result = mapper.readFromString<oatpp::Int64>(json);
    OATPP_ASSERT(result == nullptr);
  }

  // ==========================================================================
  // SECTION 8: Primitive types — UInt64
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "UInt64: value")
    auto val = oatpp::UInt64(18000000000000000000ULL);
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::UInt64>(json);
    OATPP_ASSERT(result == 18000000000000000000ULL);
  }
  {
    OATPP_LOGd(TAG, "UInt64: max")
    auto val = oatpp::UInt64(std::numeric_limits<v_uint64>::max());
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::UInt64>(json);
    OATPP_ASSERT(result == std::numeric_limits<v_uint64>::max());
  }
  {
    OATPP_LOGd(TAG, "UInt64: zero")
    oatpp::UInt64 val;
    val = static_cast<v_uint64>(0);
    auto json = mapper.writeToString(val);
    OATPP_ASSERT(json == "0");
    auto result = mapper.readFromString<oatpp::UInt64>(json);
    OATPP_ASSERT(result == 0ULL);
  }
  {
    OATPP_LOGd(TAG, "UInt64: null")
    oatpp::UInt64 nullVal;
    auto json = mapper.writeToString(nullVal);
    OATPP_ASSERT(json == "null");
    auto result = mapper.readFromString<oatpp::UInt64>(json);
    OATPP_ASSERT(result == nullptr);
  }

  // ==========================================================================
  // SECTION 9: Float32
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Float32: positive")
    auto val = oatpp::Float32(3.14f);
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::Float32>(json);
    OATPP_ASSERT(std::fabs(result.getValue(0.0f) - 3.14f) < 0.001f);
  }
  {
    OATPP_LOGd(TAG, "Float32: negative")
    auto val = oatpp::Float32(-3.14f);
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::Float32>(json);
    OATPP_ASSERT(std::fabs(result.getValue(0.0f) - (-3.14f)) < 0.001f);
  }
  {
    OATPP_LOGd(TAG, "Float32: zero")
    auto json = mapper.writeToString(oatpp::Float32(0.0f));
    auto result = mapper.readFromString<oatpp::Float32>(json);
    OATPP_ASSERT(result == 0.0f);
  }
  {
    OATPP_LOGd(TAG, "Float32: scientific notation")
    auto result = mapper.readFromString<oatpp::Float32>("1.5e2");
    OATPP_ASSERT(std::fabs(result.getValue(0.0f) - 150.0f) < 1.0f);
  }
  {
    OATPP_LOGd(TAG, "Float32: negative scientific notation")
    auto result = mapper.readFromString<oatpp::Float32>("-1.5e2");
    OATPP_ASSERT(std::fabs(result.getValue(0.0f) - (-150.0f)) < 1.0f);
  }
  {
    OATPP_LOGd(TAG, "Float32: scientific notation with sign")
    auto result = mapper.readFromString<oatpp::Float32>("1.5e+2");
    OATPP_ASSERT(std::fabs(result.getValue(0.0f) - 150.0f) < 1.0f);
  }
  {
    OATPP_LOGd(TAG, "Float32: negative exponent")
    auto result = mapper.readFromString<oatpp::Float32>("1.5e-2");
    OATPP_ASSERT(std::fabs(result.getValue(0.0f) - 0.015f) < 0.001f);
  }
  {
    OATPP_LOGd(TAG, "Float32: uppercase E")
    auto result = mapper.readFromString<oatpp::Float32>("1.5E3");
    OATPP_ASSERT(std::fabs(result.getValue(0.0f) - 1500.0f) < 1.0f);
  }
  {
    OATPP_LOGd(TAG, "Float32: null")
    oatpp::Float32 nullVal;
    auto json = mapper.writeToString(nullVal);
    OATPP_ASSERT(json == "null");
    auto result = mapper.readFromString<oatpp::Float32>(json);
    OATPP_ASSERT(result == nullptr);
  }

  // ==========================================================================
  // SECTION 10: Float64
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Float64: positive")
    auto val = oatpp::Float64(3.14159265358979);
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::Float64>(json);
    OATPP_ASSERT(std::fabs(result.getValue(0.0) - 3.14159265358979) < 1e-12);
  }
  {
    OATPP_LOGd(TAG, "Float64: negative")
    auto val = oatpp::Float64(-2.718281828459045);
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::Float64>(json);
    OATPP_ASSERT(std::fabs(result.getValue(0.0) - (-2.718281828459045)) < 1e-12);
  }
  {
    OATPP_LOGd(TAG, "Float64: zero")
    auto json = mapper.writeToString(oatpp::Float64(0.0));
    auto result = mapper.readFromString<oatpp::Float64>(json);
    OATPP_ASSERT(result == 0.0);
  }
  {
    OATPP_LOGd(TAG, "Float64: scientific notation")
    auto result = mapper.readFromString<oatpp::Float64>("1.23456789e10");
    OATPP_ASSERT(std::fabs(result.getValue(0.0) - 1.23456789e10) < 1e5);
  }
  {
    OATPP_LOGd(TAG, "Float64: negative scientific notation")
    auto result = mapper.readFromString<oatpp::Float64>("-1.23456789e-10");
    OATPP_ASSERT(std::fabs(result.getValue(0.0) - (-1.23456789e-10)) < 1e-22);
  }
  {
    OATPP_LOGd(TAG, "Float64: null")
    oatpp::Float64 nullVal;
    auto json = mapper.writeToString(nullVal);
    OATPP_ASSERT(json == "null");
    auto result = mapper.readFromString<oatpp::Float64>(json);
    OATPP_ASSERT(result == nullptr);
  }

  // ==========================================================================
  // SECTION 11: Boolean
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Boolean: true")
    auto json = mapper.writeToString(oatpp::Boolean(true));
    OATPP_ASSERT(json == "true");
    auto result = mapper.readFromString<oatpp::Boolean>(json);
    OATPP_ASSERT(result == true);
  }
  {
    OATPP_LOGd(TAG, "Boolean: false")
    auto json = mapper.writeToString(oatpp::Boolean(false));
    OATPP_ASSERT(json == "false");
    auto result = mapper.readFromString<oatpp::Boolean>(json);
    OATPP_ASSERT(result == false);
  }
  {
    OATPP_LOGd(TAG, "Boolean: null")
    oatpp::Boolean nullVal;
    auto json = mapper.writeToString(nullVal);
    OATPP_ASSERT(json == "null");
    auto result = mapper.readFromString<oatpp::Boolean>(json);
    OATPP_ASSERT(result == nullptr);
  }

  // ==========================================================================
  // SECTION 12: String
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "String: simple")
    auto val = oatpp::String("hello world");
    auto json = mapper.writeToString(val);
    OATPP_ASSERT(json == "\"hello world\"");
    auto result = mapper.readFromString<oatpp::String>(json);
    OATPP_ASSERT(result == "hello world");
  }
  {
    OATPP_LOGd(TAG, "String: empty")
    auto val = oatpp::String("");
    auto json = mapper.writeToString(val);
    OATPP_ASSERT(json == "\"\"");
    auto result = mapper.readFromString<oatpp::String>(json);
    OATPP_ASSERT(result == "");
  }
  {
    OATPP_LOGd(TAG, "String: with quotes")
    auto val = oatpp::String("he said \"hello\"");
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::String>(json);
    OATPP_ASSERT(result == "he said \"hello\"");
  }
  {
    OATPP_LOGd(TAG, "String: with backslash")
    auto val = oatpp::String("C:\\path\\to\\file");
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::String>(json);
    OATPP_ASSERT(result == "C:\\path\\to\\file");
  }
  {
    OATPP_LOGd(TAG, "String: with newline")
    auto val = oatpp::String("line1\nline2");
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::String>(json);
    OATPP_ASSERT(result == "line1\nline2");
  }
  {
    OATPP_LOGd(TAG, "String: with tab")
    auto val = oatpp::String("col1\tcol2");
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::String>(json);
    OATPP_ASSERT(result == "col1\tcol2");
  }
  {
    OATPP_LOGd(TAG, "String: with carriage return")
    auto val = oatpp::String("line1\r\nline2");
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::String>(json);
    OATPP_ASSERT(result == "line1\r\nline2");
  }
  {
    OATPP_LOGd(TAG, "String: with unicode CJK")
    auto val = oatpp::String("Hello 世界");
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::String>(json);
    OATPP_ASSERT(result == "Hello 世界");
  }
  {
    OATPP_LOGd(TAG, "String: with emoji")
    auto val = oatpp::String("Hello 🌍!");
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::String>(json);
    OATPP_ASSERT(result == "Hello 🌍!");
  }
  {
    OATPP_LOGd(TAG, "String: null")
    oatpp::String nullVal;
    auto json = mapper.writeToString(nullVal);
    OATPP_ASSERT(json == "null");
    auto result = mapper.readFromString<oatpp::String>(json);
    OATPP_ASSERT(result == nullptr);
  }
  {
    OATPP_LOGd(TAG, "String: long string")
    std::string longStr(10000, 'x');
    auto val = oatpp::String(longStr);
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::String>(json);
    OATPP_ASSERT(result->size() == 10000);
    OATPP_ASSERT(result == longStr);
  }
  {
    OATPP_LOGd(TAG, "String: all escape sequences")
    auto result = mapper.readFromString<oatpp::String>("\"\\b\\f\\n\\r\\t\\\\\\\"\"");
    OATPP_ASSERT(result == "\b\f\n\r\t\\\"");
  }

  // ==========================================================================
  // SECTION 13: Enum types
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Enum: serialize as string V1")
    oatpp::Fields<oatpp::Enum<TestEnum>::AsString> map = {{"e", TestEnum::V1}};
    auto json = mapper.writeToString(map);
    OATPP_ASSERT(json == "{\"e\":\"enum-value-1\"}");
    auto result = mapper.readFromString<oatpp::Fields<oatpp::Enum<TestEnum>::AsString>>(json);
    OATPP_ASSERT(result["e"] == TestEnum::V1);
  }
  {
    OATPP_LOGd(TAG, "Enum: serialize as string V2")
    oatpp::Fields<oatpp::Enum<TestEnum>::AsString> map = {{"e", TestEnum::V2}};
    auto json = mapper.writeToString(map);
    OATPP_ASSERT(json == "{\"e\":\"enum-value-2\"}");
  }
  {
    OATPP_LOGd(TAG, "Enum: serialize as string null")
    oatpp::Fields<oatpp::Enum<TestEnum>::AsString> map = {{"e", nullptr}};
    auto json = mapper.writeToString(map);
    OATPP_ASSERT(json == "{\"e\":null}");
    auto result = mapper.readFromString<oatpp::Fields<oatpp::Enum<TestEnum>::AsString>>(json);
    OATPP_ASSERT(result["e"] == nullptr);
  }
  {
    OATPP_LOGd(TAG, "Enum: serialize as number")
    oatpp::Fields<oatpp::Enum<TestEnum>::AsNumber> map = {{"e", TestEnum::V2}};
    auto json = mapper.writeToString(map);
    OATPP_ASSERT(json == "{\"e\":20}");
  }
  {
    OATPP_LOGd(TAG, "Enum: serialize as number null")
    oatpp::Fields<oatpp::Enum<TestEnum>::AsNumber> map = {{"e", nullptr}};
    auto json = mapper.writeToString(map);
    OATPP_ASSERT(json == "{\"e\":null}");
  }
  {
    OATPP_LOGd(TAG, "Enum: String NotNull error on null")
    bool caught = false;
    oatpp::Fields<oatpp::Enum<TestEnum>::AsString::NotNull> map = {{"e", nullptr}};
    try {
      auto json = mapper.writeToString(map);
    } catch (const std::runtime_error&) {
      caught = true;
    }
    OATPP_ASSERT(caught);
  }
  {
    OATPP_LOGd(TAG, "Enum: Number NotNull error on null")
    bool caught = false;
    oatpp::Fields<oatpp::Enum<TestEnum>::AsNumber::NotNull> map = {{"e", nullptr}};
    try {
      auto json = mapper.writeToString(map);
    } catch (const std::runtime_error&) {
      caught = true;
    }
    OATPP_ASSERT(caught);
  }
  {
    OATPP_LOGd(TAG, "Enum: deserialize from string JSON")
    auto map = mapper.readFromString<oatpp::Fields<oatpp::Enum<TestEnum>::AsString>>(
        "{\"e\":\"enum-value-3\"}");
    OATPP_ASSERT(map["e"] == TestEnum::V3);
  }
  {
    OATPP_LOGd(TAG, "Enum: deserialize from number JSON")
    auto map = mapper.readFromString<oatpp::Fields<oatpp::Enum<TestEnum>::AsNumber>>(
        "{\"e\":10}");
    OATPP_ASSERT(map["e"] == TestEnum::V1);
  }
  {
    OATPP_LOGd(TAG, "Enum: deserialize null string NotNull fails")
    bool caught = false;
    try {
      auto map = mapper.readFromString<oatpp::Fields<oatpp::Enum<TestEnum>::AsString::NotNull>>(
          "{\"e\":null}");
    } catch (const std::runtime_error&) {
      caught = true;
    }
    OATPP_ASSERT(caught);
  }

  // ==========================================================================
  // SECTION 14: Simple DTO with all primitive types
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "DTO: all primitives round-trip")
    auto dto = SimplePrimitiveDto::createShared();
    dto->f_int8 = static_cast<v_int8>(-100);
    dto->f_uint8 = static_cast<v_uint8>(200);
    dto->f_int16 = static_cast<v_int16>(-30000);
    dto->f_uint16 = static_cast<v_uint16>(60000);
    dto->f_int32 = -2000000000;
    dto->f_uint32 = 4000000000u;
    dto->f_int64 = -9000000000000000000LL;
    dto->f_uint64 = 18000000000000000000ULL;
    dto->f_float32 = 3.14f;
    dto->f_float64 = 2.718281828459045;
    dto->f_bool = true;
    dto->f_str = "test string value";

    auto result = roundTrip<SimplePrimitiveDto>(dto, mapper);

    OATPP_ASSERT(result->f_int8 == dto->f_int8);
    OATPP_ASSERT(result->f_uint8 == dto->f_uint8);
    OATPP_ASSERT(result->f_int16 == dto->f_int16);
    OATPP_ASSERT(result->f_uint16 == dto->f_uint16);
    OATPP_ASSERT(result->f_int32 == dto->f_int32);
    OATPP_ASSERT(result->f_uint32 == dto->f_uint32);
    OATPP_ASSERT(result->f_int64 == dto->f_int64);
    OATPP_ASSERT(result->f_uint64 == dto->f_uint64);
    OATPP_ASSERT(std::fabs(result->f_float32.getValue(0.0f) - 3.14f) < 0.001f);
    OATPP_ASSERT(std::fabs(result->f_float64.getValue(0.0) - 2.718281828459045) < 1e-12);
    OATPP_ASSERT(result->f_bool == true);
    OATPP_ASSERT(result->f_str == "test string value");
  }

  // ==========================================================================
  // SECTION 15: DTO with all null fields
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "DTO: all null primitives")
    auto dto = SimplePrimitiveDto::createShared();
    auto json = mapper.writeToString(dto);
    OATPP_LOGv(TAG, "all-null json='{}'", json->c_str())
    auto caret = oatpp::utils::parser::Caret(json);
    auto result = mapper.readFromCaret<oatpp::Object<SimplePrimitiveDto>>(caret);
    OATPP_ASSERT(result != nullptr);
    OATPP_ASSERT(result->f_int8 == nullptr);
    OATPP_ASSERT(result->f_int32 == nullptr);
    OATPP_ASSERT(result->f_int64 == nullptr);
    OATPP_ASSERT(result->f_float32 == nullptr);
    OATPP_ASSERT(result->f_float64 == nullptr);
    OATPP_ASSERT(result->f_bool == nullptr);
    OATPP_ASSERT(result->f_str == nullptr);
  }

  // ==========================================================================
  // SECTION 16: DTO with boundary values
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "DTO: boundary values")
    auto dto = SimplePrimitiveDto::createShared();
    dto->f_int8 = std::numeric_limits<v_int8>::min();
    dto->f_uint8 = std::numeric_limits<v_uint8>::max();
    dto->f_int16 = std::numeric_limits<v_int16>::min();
    dto->f_uint16 = std::numeric_limits<v_uint16>::max();
    dto->f_int32 = std::numeric_limits<v_int32>::min();
    dto->f_uint32 = std::numeric_limits<v_uint32>::max();
    dto->f_int64 = std::numeric_limits<v_int64>::min();
    dto->f_uint64 = std::numeric_limits<v_uint64>::max();
    dto->f_float32 = std::numeric_limits<v_float32>::max();
    dto->f_float64 = std::numeric_limits<v_float64>::min();
    dto->f_bool = false;
    dto->f_str = "";

    auto json = mapper.writeToString(dto);
    auto caret = oatpp::utils::parser::Caret(json);
    auto result = mapper.readFromCaret<oatpp::Object<SimplePrimitiveDto>>(caret);
    OATPP_ASSERT(result != nullptr);
    OATPP_ASSERT(result->f_int8 == std::numeric_limits<v_int8>::min());
    OATPP_ASSERT(result->f_uint8 == std::numeric_limits<v_uint8>::max());
    OATPP_ASSERT(result->f_int16 == std::numeric_limits<v_int16>::min());
    OATPP_ASSERT(result->f_uint16 == std::numeric_limits<v_uint16>::max());
    OATPP_ASSERT(result->f_int32 == std::numeric_limits<v_int32>::min());
    OATPP_ASSERT(result->f_uint32 == std::numeric_limits<v_uint32>::max());
    OATPP_ASSERT(result->f_int64 == std::numeric_limits<v_int64>::min());
    OATPP_ASSERT(result->f_uint64 == std::numeric_limits<v_uint64>::max());
    OATPP_ASSERT(result->f_bool == false);
    OATPP_ASSERT(result->f_str == "");
  }

  // ==========================================================================
  // SECTION 17: Required field validation
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Required field: serialize with null → error")
    auto dto = RequiredFieldDto::createShared();
    oatpp::String result;
    bool caught = false;
    try {
      result = mapper.writeToString(dto);
    } catch (const std::runtime_error&) {
      caught = true;
    }
    OATPP_ASSERT(caught || result == nullptr);
  }
  {
    OATPP_LOGd(TAG, "Required field: filled → success")
    auto dto = RequiredFieldDto::createShared();
    dto->f_required = "I am required";
    dto->f_optional = "I am optional";
    auto json = mapper.writeToString(dto);
    OATPP_ASSERT(json != nullptr);
    auto caret = oatpp::utils::parser::Caret(json);
    auto result = mapper.readFromCaret<oatpp::Object<RequiredFieldDto>>(caret);
    OATPP_ASSERT(result != nullptr);
    OATPP_ASSERT(result->f_required == "I am required");
    OATPP_ASSERT(result->f_optional == "I am optional");
  }
  {
    OATPP_LOGd(TAG, "Required field: deserialize null → error")
    bool caught = false;
    try {
      auto dto = mapper.readFromString<oatpp::Object<RequiredFieldDto>>(
          R"({"f_required":null, "f_optional":"opt"})");
    } catch (const std::runtime_error&) {
      caught = true;
    }
    OATPP_ASSERT(caught);
  }

  // ==========================================================================
  // SECTION 18: Nested DTO objects
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Nested DTO: basic nesting")
    auto dto = ComplexDto::createShared();
    dto->title = "Complex Object";
    dto->child = ChildDto::createShared();
    dto->child->name = "child-name";
    dto->child->value = 42;

    auto result = roundTrip<ComplexDto>(dto, mapper);
    OATPP_ASSERT(result->title == "Complex Object");
    OATPP_ASSERT(result->child != nullptr);
    OATPP_ASSERT(result->child->name == "child-name");
    OATPP_ASSERT(result->child->value == 42);
  }
  {
    OATPP_LOGd(TAG, "Nested DTO: null child")
    auto dto = ComplexDto::createShared();
    dto->title = "No Child";

    auto json = mapper.writeToString(dto);
    auto caret = oatpp::utils::parser::Caret(json);
    auto result = mapper.readFromCaret<oatpp::Object<ComplexDto>>(caret);
    OATPP_ASSERT(result != nullptr);
    OATPP_ASSERT(result->title == "No Child");
    OATPP_ASSERT(result->child == nullptr);
  }
  {
    OATPP_LOGd(TAG, "Nested DTO: required child error on serialize")
    auto dto = ParentWithRequiredChildDto::createShared();
    dto->field_string = "parent-field";
    dto->child = RequiredChildDto::createShared();
    bool caught = false;
    try {
      auto json = mapper.writeToString(dto);
    } catch (const std::runtime_error&) {
      caught = true;
    }
    OATPP_ASSERT(caught);
  }
  {
    OATPP_LOGd(TAG, "Nested DTO: required child success")
    auto dto = ParentWithRequiredChildDto::createShared();
    dto->field_string = "parent-field";
    dto->child = RequiredChildDto::createShared();
    dto->child->name = "child-has-name";
    auto json = mapper.writeToString(dto);
    OATPP_ASSERT(json != nullptr);
  }
  {
    OATPP_LOGd(TAG, "Nested DTO: double nesting")
    auto dto = NestedParentDto::createShared();
    dto->name = "outer";
    dto->complex = ComplexDto::createShared();
    dto->complex->title = "inner";
    dto->complex->child = ChildDto::createShared();
    dto->complex->child->name = "deep-child";
    dto->complex->child->value = 99;

    auto result = roundTrip<NestedParentDto>(dto, mapper);
    OATPP_ASSERT(result->name == "outer");
    OATPP_ASSERT(result->complex != nullptr);
    OATPP_ASSERT(result->complex->title == "inner");
    OATPP_ASSERT(result->complex->child != nullptr);
    OATPP_ASSERT(result->complex->child->name == "deep-child");
    OATPP_ASSERT(result->complex->child->value == 99);
  }

  // ==========================================================================
  // SECTION 19: Collections — List
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "List: String list round-trip")
    auto list = oatpp::List<oatpp::String>::createShared();
    list->push_back("item1");
    list->push_back("item2");
    list->push_back("item3");

    auto json = mapper.writeToString(list);
    auto result = mapper.readFromString<oatpp::List<oatpp::String>>(json);
    OATPP_ASSERT(result->size() == 3);
    OATPP_ASSERT(result[0] == "item1");
    OATPP_ASSERT(result[1] == "item2");
    OATPP_ASSERT(result[2] == "item3");
  }
  {
    OATPP_LOGd(TAG, "List: empty list")
    auto list = oatpp::List<oatpp::Int32>::createShared();
    auto json = mapper.writeToString(list);
    OATPP_ASSERT(json == "[]");
    auto result = mapper.readFromString<oatpp::List<oatpp::Int32>>(json);
    OATPP_ASSERT(result->size() == 0);
  }
  {
    OATPP_LOGd(TAG, "List: Int32 list")
    auto list = oatpp::List<oatpp::Int32>::createShared();
    list->push_back(10);
    list->push_back(20);
    list->push_back(30);
    list->push_back(-40);

    auto json = mapper.writeToString(list);
    auto result = mapper.readFromString<oatpp::List<oatpp::Int32>>(json);
    OATPP_ASSERT(result->size() == 4);
    OATPP_ASSERT(result[0] == 10);
    OATPP_ASSERT(result[1] == 20);
    OATPP_ASSERT(result[2] == 30);
    OATPP_ASSERT(result[3] == -40);
  }
  {
    OATPP_LOGd(TAG, "List: Float64 list")
    auto list = oatpp::List<oatpp::Float64>::createShared();
    list->push_back(1.1);
    list->push_back(2.2);
    list->push_back(3.3);

    auto json = mapper.writeToString(list);
    auto result = mapper.readFromString<oatpp::List<oatpp::Float64>>(json);
    OATPP_ASSERT(result->size() == 3);
    OATPP_ASSERT(std::fabs(result[0].getValue(0.0) - 1.1) < 0.001);
    OATPP_ASSERT(std::fabs(result[1].getValue(0.0) - 2.2) < 0.001);
    OATPP_ASSERT(std::fabs(result[2].getValue(0.0) - 3.3) < 0.001);
  }
  {
    OATPP_LOGd(TAG, "List: Boolean list")
    auto list = oatpp::List<oatpp::Boolean>::createShared();
    list->push_back(true);
    list->push_back(false);
    list->push_back(true);

    auto json = mapper.writeToString(list);
    auto result = mapper.readFromString<oatpp::List<oatpp::Boolean>>(json);
    OATPP_ASSERT(result->size() == 3);
    OATPP_ASSERT(result[0] == true);
    OATPP_ASSERT(result[1] == false);
    OATPP_ASSERT(result[2] == true);
  }
  {
    OATPP_LOGd(TAG, "List: nested object list")
    auto list = oatpp::List<oatpp::Object<ChildDto>>::createShared();
    for (int i = 0; i < 5; i++) {
      auto child = ChildDto::createShared();
      child->name = "child-" + oatpp::utils::Conversion::int32ToStr(i);
      child->value = i * 10;
      list->push_back(child);
    }

    auto json = mapper.writeToString(list);
    auto result = mapper.readFromString<oatpp::List<oatpp::Object<ChildDto>>>(json);
    OATPP_ASSERT(result->size() == 5);
    for (v_uint64 i = 0; i < 5; i++) {
      OATPP_ASSERT(result[i]->name == "child-" + oatpp::utils::Conversion::int32ToStr(static_cast<v_int32>(i)));
      OATPP_ASSERT(result[i]->value == static_cast<v_int32>(i) * 10);
    }
  }
  {
    OATPP_LOGd(TAG, "List: with null elements")
    auto list = oatpp::List<oatpp::String>::createShared();
    list->push_back("keep");
    list->push_back(nullptr);
    list->push_back("also-keep");

    auto json = mapper.writeToString(list);
    auto result = mapper.readFromString<oatpp::List<oatpp::String>>(json);
    OATPP_ASSERT(result != nullptr);
  }

  // ==========================================================================
  // SECTION 20: Collections — Vector
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Vector: String vector round-trip")
    auto vec = oatpp::Vector<oatpp::String>::createShared();
    vec->push_back("v1");
    vec->push_back("v2");
    vec->push_back("v3");

    auto json = mapper.writeToString(vec);
    auto result = mapper.readFromString<oatpp::Vector<oatpp::String>>(json);
    OATPP_ASSERT(result->size() == 3);
    OATPP_ASSERT((*result)[0] == "v1");
    OATPP_ASSERT((*result)[1] == "v2");
    OATPP_ASSERT((*result)[2] == "v3");
  }
  {
    OATPP_LOGd(TAG, "Vector: empty vector")
    auto vec = oatpp::Vector<oatpp::Int64>::createShared();
    auto json = mapper.writeToString(vec);
    OATPP_ASSERT(json == "[]");
    auto result = mapper.readFromString<oatpp::Vector<oatpp::Int64>>(json);
    OATPP_ASSERT(result->size() == 0);
  }

  // ==========================================================================
  // SECTION 21: Collections — UnorderedSet
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "UnorderedSet: round-trip")
    oatpp::UnorderedSet<oatpp::String> set({"a", "b", "c", "a" /* duplicate */});

    auto json = mapper.writeToString(set);
    auto result = mapper.readFromString<oatpp::UnorderedSet<oatpp::String>>(json);
    OATPP_ASSERT(result != nullptr);
    OATPP_ASSERT(result->size() == 3);
  }
  {
    OATPP_LOGd(TAG, "UnorderedSet: with duplicates in JSON")
    auto result = mapper.readFromString<oatpp::UnorderedSet<oatpp::String>>(
        R"(["x","y","z","x","y"])");
    OATPP_ASSERT(result != nullptr);
    OATPP_ASSERT(result->size() == 3);
  }
  {
    OATPP_LOGd(TAG, "UnorderedSet: empty")
    auto set = oatpp::UnorderedSet<oatpp::String>::createShared();
    auto json = mapper.writeToString(set);
    OATPP_ASSERT(json == "[]");
  }

  // ==========================================================================
  // SECTION 22: Maps — Fields (PairList)
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Fields: round-trip")
    oatpp::Fields<oatpp::String> map = {{"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"}};

    auto json = mapper.writeToString(map);
    auto result = mapper.readFromString<oatpp::Fields<oatpp::String>>(json);
    OATPP_ASSERT(result->size() == 3);
    OATPP_ASSERT(result["k1"] == "v1");
    OATPP_ASSERT(result["k2"] == "v2");
    OATPP_ASSERT(result["k3"] == "v3");
  }
  {
    OATPP_LOGd(TAG, "Fields: empty map")
    auto map = oatpp::Fields<oatpp::Int32>::createShared();
    auto json = mapper.writeToString(map);
    OATPP_ASSERT(json == "{}");
    auto result = mapper.readFromString<oatpp::Fields<oatpp::Int32>>(json);
    OATPP_ASSERT(result->size() == 0);
  }
  {
    OATPP_LOGd(TAG, "Fields: Int32 values")
    oatpp::Fields<oatpp::Int32> map = {{"a", 1}, {"b", 2}, {"c", 3}};

    auto json = mapper.writeToString(map);
    auto result = mapper.readFromString<oatpp::Fields<oatpp::Int32>>(json);
    OATPP_ASSERT(result["a"] == 1);
    OATPP_ASSERT(result["b"] == 2);
    OATPP_ASSERT(result["c"] == 3);
  }
  {
    OATPP_LOGd(TAG, "Fields: Iterator order preservation")
    auto map = oatpp::Fields<oatpp::String>::createShared();
    for (int i = 0; i < 12; i++) {
      auto key = "key" + oatpp::utils::Conversion::int32ToStr(i);
      auto val = "pair_item" + oatpp::utils::Conversion::int32ToStr(i);
      map[key] = val;
    }

    auto json = mapper.writeToString(map);
    auto result = mapper.readFromString<oatpp::Fields<oatpp::String>>(json);
    OATPP_ASSERT(result != nullptr);

    v_int32 i = 0;
    for (auto& pair : *result) {
      OATPP_ASSERT(pair.first == "key" + oatpp::utils::Conversion::int32ToStr(i));
      OATPP_ASSERT(pair.second == "pair_item" + oatpp::utils::Conversion::int32ToStr(i));
      i++;
    }
    OATPP_ASSERT(i == 12);
  }

  // ==========================================================================
  // SECTION 23: Maps — UnorderedFields (UnorderedMap)
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "UnorderedFields: round-trip")
    auto map = oatpp::UnorderedFields<oatpp::String>::createShared();
    (*map)["uk1"] = "uv1";
    (*map)["uk2"] = "uv2";

    auto json = mapper.writeToString(map);
    auto result = mapper.readFromString<oatpp::UnorderedFields<oatpp::String>>(json);
    OATPP_ASSERT(result->size() == 2);
    OATPP_ASSERT((*result)["uk1"] == "uv1");
    OATPP_ASSERT((*result)["uk2"] == "uv2");
  }
  {
    OATPP_LOGd(TAG, "UnorderedFields: empty")
    auto map = oatpp::UnorderedFields<oatpp::Float64>::createShared();
    auto json = mapper.writeToString(map);
    OATPP_ASSERT(json == "{}");
    auto result = mapper.readFromString<oatpp::UnorderedFields<oatpp::Float64>>(json);
    OATPP_ASSERT(result->size() == 0);
  }

  // ==========================================================================
  // SECTION 24: Any type
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Any: String value")
    auto dto = AnyContainerDto::createShared();
    dto->any_field = oatpp::String("hello any");

    auto json = mapper.writeToString(dto);
    auto result = mapper.readFromString<oatpp::Object<AnyContainerDto>>(json);
    OATPP_ASSERT(result != nullptr);
    OATPP_ASSERT(result->any_field.getStoredType() == oatpp::String::Class::getType());
    OATPP_ASSERT(result->any_field.retrieve<oatpp::String>() == "hello any");
  }
  {
    OATPP_LOGd(TAG, "Any: Integer value (stored as Int64 in JSON)")
    auto dto = AnyContainerDto::createShared();
    dto->any_field = oatpp::Int32(42);

    auto json = mapper.writeToString(dto);
    auto result = mapper.readFromString<oatpp::Object<AnyContainerDto>>(json);
    OATPP_ASSERT(result != nullptr);
    // JSON integers without type info are deserialized as Int64 by default
    OATPP_ASSERT(result->any_field.retrieve<oatpp::Int64>() == 42);
  }
  {
    OATPP_LOGd(TAG, "Any: Float64 value")
    auto dto = AnyContainerDto::createShared();
    dto->any_field = oatpp::Float64(3.14159);

    auto json = mapper.writeToString(dto);
    auto result = mapper.readFromString<oatpp::Object<AnyContainerDto>>(json);
    OATPP_ASSERT(result != nullptr);
    OATPP_ASSERT(std::fabs(result->any_field.retrieve<oatpp::Float64>().getValue(0.0) - 3.14159) < 1e-12);
  }
  {
    OATPP_LOGd(TAG, "Any: Boolean value")
    auto dto = AnyContainerDto::createShared();
    dto->any_field = oatpp::Boolean(false);

    auto json = mapper.writeToString(dto);
    auto result = mapper.readFromString<oatpp::Object<AnyContainerDto>>(json);
    OATPP_ASSERT(result != nullptr);
    OATPP_ASSERT(result->any_field.getStoredType() == oatpp::Boolean::Class::getType());
    OATPP_ASSERT(result->any_field.retrieve<oatpp::Boolean>() == false);
  }
  {
    OATPP_LOGd(TAG, "Any: large Integer")
    auto dto = AnyContainerDto::createShared();
    dto->any_field = oatpp::Int64(9223372036854775807LL);

    auto json = mapper.writeToString(dto);
    auto result = mapper.readFromString<oatpp::Object<AnyContainerDto>>(json);
    OATPP_ASSERT(result != nullptr);
    OATPP_ASSERT(result->any_field.retrieve<oatpp::Int64>() == 9223372036854775807LL);
  }
  {
    OATPP_LOGd(TAG, "Any: negative Integer")
    auto dto = AnyContainerDto::createShared();
    dto->any_field = oatpp::Int64(-1234567890LL);

    auto json = mapper.writeToString(dto);
    auto result = mapper.readFromString<oatpp::Object<AnyContainerDto>>(json);
    OATPP_ASSERT(result != nullptr);
    OATPP_ASSERT(result->any_field.retrieve<oatpp::Int64>() == -1234567890LL);
  }
  {
    OATPP_LOGd(TAG, "Any: exponential Float")
    auto dto = AnyContainerDto::createShared();
    dto->any_field = oatpp::Float64(1.2345e30);

    auto json = mapper.writeToString(dto);
    auto result = mapper.readFromString<oatpp::Object<AnyContainerDto>>(json);
    OATPP_ASSERT(result != nullptr);
  }
  {
    OATPP_LOGd(TAG, "Any: null")
    auto dto = AnyContainerDto::createShared();

    auto json = mapper.writeToString(dto);
    auto result = mapper.readFromString<oatpp::Object<AnyContainerDto>>(json);
    OATPP_ASSERT(result != nullptr);
    OATPP_ASSERT(result->any_field == nullptr);
  }

  // ==========================================================================
  // SECTION 25: Any list — mixed types
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Any list: mixed types")
    auto dto = AnyContainerDto::createShared();
    dto->any_list = oatpp::List<oatpp::Any>::createShared();
    dto->any_list->push_back(oatpp::String("string in any list"));
    dto->any_list->push_back(oatpp::Int32(100));
    dto->any_list->push_back(oatpp::Float64(3.14));
    dto->any_list->push_back(oatpp::Boolean(true));

    auto json = mapper.writeToString(dto);
    auto result = mapper.readFromString<oatpp::Object<AnyContainerDto>>(json);
    OATPP_ASSERT(result != nullptr);
    OATPP_ASSERT(result->any_list != nullptr);
  }

  // ==========================================================================
  // SECTION 26: Empty containers in DTO
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Empty containers in DTO")
    auto dto = EmptyContainersDto::createShared();

    auto json = mapper.writeToString(dto);
    auto result = mapper.readFromString<oatpp::Object<EmptyContainersDto>>(json);
    OATPP_ASSERT(result != nullptr);
    OATPP_ASSERT(result->empty_list != nullptr);
    OATPP_ASSERT(result->empty_list->size() == 0);
    OATPP_ASSERT(result->empty_map != nullptr);
    OATPP_ASSERT(result->empty_map->size() == 0);
    OATPP_ASSERT(result->empty_vector != nullptr);
    OATPP_ASSERT(result->empty_vector->size() == 0);
    OATPP_ASSERT(result->empty_set != nullptr);
    OATPP_ASSERT(result->empty_set->size() == 0);
  }

  // ==========================================================================
  // SECTION 27: Empty DTO
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Empty DTO round-trip")
    auto dto = EmptyDto::createShared();
    auto json = mapper.writeToString(dto);
    OATPP_ASSERT(json == "{}");
    auto result = mapper.readFromString<oatpp::Object<EmptyDto>>(json);
    OATPP_ASSERT(result != nullptr);
  }

  // ==========================================================================
  // SECTION 28: Complex DTO with all collections filled
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Complex DTO with all fields filled")
    auto dto = ComplexDto::createShared();
    dto->title = "Complex Full DTO";
    dto->child = ChildDto::createShared();
    dto->child->name = "child-1";
    dto->child->value = 100;

    dto->string_list = {"s1", "s2", "s3"};
    dto->int_list = {10, 20, 30, 40, 50};
    dto->float_vector = {1.1, 2.2, 3.3};

    auto c1 = ChildDto::createShared(); c1->name = "obj-1"; c1->value = 1;
    auto c2 = ChildDto::createShared(); c2->name = "obj-2"; c2->value = 2;
    dto->object_list = {c1, c2};

    dto->string_map = {{"a", "A"}, {"b", "B"}};
    dto->int_map = {{"x", 24}, {"y", 25}};
    dto->string_set = {"alpha", "beta", "gamma"};

    auto json = mapper.writeToString(dto);
    auto result = mapper.readFromString<oatpp::Object<ComplexDto>>(json);
    OATPP_ASSERT(result != nullptr);
    OATPP_ASSERT(result->title == "Complex Full DTO");
    OATPP_ASSERT(result->child->name == "child-1");
    OATPP_ASSERT(result->child->value == 100);
    OATPP_ASSERT(result->string_list->size() == 3);
    OATPP_ASSERT(result->int_list->size() == 5);
    OATPP_ASSERT(result->float_vector->size() == 3);
    OATPP_ASSERT(result->object_list->size() == 2);
    OATPP_ASSERT(result->string_map->size() == 2);
    OATPP_ASSERT(result->int_map->size() == 2);
    OATPP_ASSERT(result->string_set->size() == 3);
  }

  // ==========================================================================
  // SECTION 29: Enum container DTO
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Enum container DTO round-trip")
    auto dto = EnumContainerDto::createShared();
    dto->enum_str = TestEnum::V1;
    dto->enum_num = TestEnum::V3;

    auto json = mapper.writeToString(dto);
    auto result = mapper.readFromString<oatpp::Object<EnumContainerDto>>(json);
    OATPP_ASSERT(result != nullptr);
    OATPP_ASSERT(result->enum_str == TestEnum::V1);
    OATPP_ASSERT(result->enum_num == TestEnum::V3);
  }

  // ==========================================================================
  // SECTION 30: Deep nesting
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Deep nesting: 10 levels")
    auto head = DeepNodeDto::createShared();
    head->label = "level-0";
    head->index = 0;

    auto current = head;
    for (int i = 1; i < 10; i++) {
      auto next = DeepNodeDto::createShared();
      next->label = "level-" + oatpp::utils::Conversion::int32ToStr(i);
      next->index = i;
      current->next = next;
      current = next;
    }

    auto json = mapper.writeToString(head);
    auto result = mapper.readFromString<oatpp::Object<DeepNodeDto>>(json);
    OATPP_ASSERT(result != nullptr);

    auto node = result;
    for (int i = 0; i < 10; i++) {
      OATPP_ASSERT(node != nullptr);
      OATPP_ASSERT(node->label == "level-" + oatpp::utils::Conversion::int32ToStr(i));
      OATPP_ASSERT(node->index == i);
      node = node->next;
    }
    OATPP_ASSERT(node == nullptr);
  }

  // ==========================================================================
  // SECTION 31: Whitespace handling in JSON
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Whitespace: spaces, tabs, newlines in JSON")
    auto result = mapper.readFromString<oatpp::Object<SimplePrimitiveDto>>(
        "{\n\r\t \"f_int32\"\n\r\t :\n\r\t  42\n\r\t }");
    OATPP_ASSERT(result != nullptr);
    OATPP_ASSERT(result->f_int32 == 42);
  }
  {
    OATPP_LOGd(TAG, "Whitespace: compact JSON")
    auto result = mapper.readFromString<oatpp::Object<SimplePrimitiveDto>>(
        R"({"f_str":"hello","f_int32":100,"f_bool":true})");
    OATPP_ASSERT(result != nullptr);
    OATPP_ASSERT(result->f_str == "hello");
    OATPP_ASSERT(result->f_int32 == 100);
    OATPP_ASSERT(result->f_bool == true);
  }

  // ==========================================================================
  // SECTION 32: Beautifier mode
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Beautifier: serialization includes newlines")
    oatpp::json::ObjectMapper beautifierMapper;
    beautifierMapper.serializerConfig().json.useBeautifier = true;

    auto dto = SimplePrimitiveDto::createShared();
    dto->f_int32 = 42;
    dto->f_str = "hello";

    auto json = beautifierMapper.writeToString(dto);
    OATPP_ASSERT(json != nullptr);
    OATPP_ASSERT(std::strchr(json->c_str(), '\n') != nullptr);

    auto result = mapper.readFromString<oatpp::Object<SimplePrimitiveDto>>(json);
    OATPP_ASSERT(result != nullptr);
    OATPP_ASSERT(result->f_int32 == 42);
    OATPP_ASSERT(result->f_str == "hello");
  }

  // ==========================================================================
  // SECTION 33: Top-level primitive deserialization
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Top-level: deserialize plain integer")
    auto result = mapper.readFromString<oatpp::Int32>("12345");
    OATPP_ASSERT(result == 12345);
  }
  {
    OATPP_LOGd(TAG, "Top-level: deserialize plain string")
    auto result = mapper.readFromString<oatpp::String>("\"plain string\"");
    OATPP_ASSERT(result == "plain string");
  }
  {
    OATPP_LOGd(TAG, "Top-level: deserialize true")
    auto result = mapper.readFromString<oatpp::Boolean>("true");
    OATPP_ASSERT(result == true);
  }
  {
    OATPP_LOGd(TAG, "Top-level: deserialize false")
    auto result = mapper.readFromString<oatpp::Boolean>("false");
    OATPP_ASSERT(result == false);
  }
  {
    OATPP_LOGd(TAG, "Top-level: deserialize null")
    auto result = mapper.readFromString<oatpp::String>("null");
    OATPP_ASSERT(result == nullptr);
  }
  {
    OATPP_LOGd(TAG, "Top-level: deserialize float")
    auto result = mapper.readFromString<oatpp::Float64>("-1.2345e-10");
    OATPP_ASSERT(std::fabs(result.getValue(0.0) - (-1.2345e-10)) < 1e-22);
  }

  // ==========================================================================
  // SECTION 34: Number edge cases
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Number: negative zero-like")
    auto result = mapper.readFromString<oatpp::Int32>("-0");
    OATPP_ASSERT(result == 0);
  }
  {
    OATPP_LOGd(TAG, "Number: large positive")
    auto result = mapper.readFromString<oatpp::Int64>("9223372036854775807");
    OATPP_ASSERT(result == 9223372036854775807LL);
  }

  // ==========================================================================
  // SECTION 35: Maps with various key types
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Map: keys with special characters")
    oatpp::Fields<oatpp::String> map = {
      {"key with spaces", "val1"},
      {"key-with-dashes", "val2"},
      {"key_with_underscores", "val3"},
      {"key.with.dots", "val4"}
    };

    auto json = mapper.writeToString(map);
    auto result = mapper.readFromString<oatpp::Fields<oatpp::String>>(json);
    OATPP_ASSERT(result["key with spaces"] == "val1");
    OATPP_ASSERT(result["key-with-dashes"] == "val2");
    OATPP_ASSERT(result["key_with_underscores"] == "val3");
    OATPP_ASSERT(result["key.with.dots"] == "val4");
  }

  // ==========================================================================
  // SECTION 36: String escaping edge cases
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "String escape: double backslash")
    auto val = oatpp::String("\\\\server\\share\\path");
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::String>(json);
    OATPP_ASSERT(result == "\\\\server\\share\\path");
  }
  {
    OATPP_LOGd(TAG, "String escape: mixed escapes")
    auto val = oatpp::String("tab\there\nnewline\rreturn\ffeed\bbackspace");
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::String>(json);
    OATPP_ASSERT(result == "tab\there\nnewline\rreturn\ffeed\bbackspace");
  }
  {
    OATPP_LOGd(TAG, "String: very long string with special chars")
    std::string mixed;
    for (int i = 0; i < 1000; i++) {
      mixed += "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"[i % 36];
      if (i % 50 == 0) mixed += "\n";
      if (i % 73 == 0) mixed += "\"";
      if (i % 97 == 0) mixed += "\\";
    }
    auto val = oatpp::String(mixed);
    auto json = mapper.writeToString(val);
    auto result = mapper.readFromString<oatpp::String>(json);
    OATPP_ASSERT(result == mixed);
  }

  // ==========================================================================
  // SECTION 37: Large integer arrays
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Large array: 100 elements")
    auto list = oatpp::List<oatpp::Int32>::createShared();
    for (int i = 0; i < 100; i++) {
      list->push_back(i * 10);
    }

    auto json = mapper.writeToString(list);
    auto result = mapper.readFromString<oatpp::List<oatpp::Int32>>(json);
    OATPP_ASSERT(result->size() == 100);
    for (v_uint64 i = 0; i < 100; i++) {
      OATPP_ASSERT(result[i] == static_cast<v_int32>(i) * 10);
    }
  }

  // ==========================================================================
  // SECTION 38: Float precision edge cases
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Float: very small number")
    auto result = mapper.readFromString<oatpp::Float64>("1e-300");
    OATPP_ASSERT(result.getValue(1.0) < 1e-200);
  }
  {
    OATPP_LOGd(TAG, "Float: very large number")
    auto result = mapper.readFromString<oatpp::Float64>("1e300");
    OATPP_ASSERT(result.getValue(0.0) > 1e200);
  }
  {
    OATPP_LOGd(TAG, "Float: decimal without integer part")
    auto result = mapper.readFromString<oatpp::Float64>("0.5");
    OATPP_ASSERT(std::fabs(result.getValue(0.0) - 0.5) < 0.001);
  }
  {
    OATPP_LOGd(TAG, "Float: integer without decimal")
    auto result = mapper.readFromString<oatpp::Float64>("42");
    OATPP_ASSERT(std::fabs(result.getValue(0.0) - 42.0) < 0.001);
  }

  // ==========================================================================
  // SECTION 39: Nested maps and lists inside DTOs
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "ComplexDTO: nested collections round-trip")
    auto dto = ComplexDto::createShared();
    dto->title = "Nested Collections";
    dto->string_list = {"a", "b"};
    dto->int_list = {1, 2, 3};
    dto->float_vector = {0.1, 0.2};
    dto->string_map = {{"key1", "val1"}};
    dto->int_map = {{"num1", 100}};
    dto->string_set = {"x", "y"};

    auto json = mapper.writeToString(dto);
    auto caret = oatpp::utils::parser::Caret(json);
    auto result = mapper.readFromCaret<oatpp::Object<ComplexDto>>(caret);
    OATPP_ASSERT(result != nullptr);
    OATPP_ASSERT(result->title == "Nested Collections");
    OATPP_ASSERT(result->string_list->size() == 2);
    OATPP_ASSERT(result->int_list->size() == 3);
    OATPP_ASSERT(result->float_vector->size() == 2);
    OATPP_ASSERT(result->string_map->size() == 1);
    OATPP_ASSERT(result->int_map->size() == 1);
    OATPP_ASSERT(result->string_set->size() == 2);
  }

  // ==========================================================================
  // SECTION 40: Zero values of all types
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Number: zero values of all types")
    OATPP_ASSERT(mapper.writeToString(oatpp::Int8(static_cast<v_int8>(0))) == "0");
    OATPP_ASSERT(mapper.writeToString(oatpp::UInt8(static_cast<v_uint8>(0))) == "0");
    OATPP_ASSERT(mapper.writeToString(oatpp::Int16(static_cast<v_int16>(0))) == "0");
    OATPP_ASSERT(mapper.writeToString(oatpp::UInt16(static_cast<v_uint16>(0))) == "0");
    OATPP_ASSERT(mapper.writeToString(oatpp::Int32(0)) == "0");
    OATPP_ASSERT(mapper.writeToString(oatpp::UInt32(0u)) == "0");
    {
      oatpp::Int64 v64; v64 = static_cast<v_int64>(0);
      OATPP_ASSERT(mapper.writeToString(v64) == "0");
    }
    {
      oatpp::UInt64 vu64; vu64 = static_cast<v_uint64>(0);
      OATPP_ASSERT(mapper.writeToString(vu64) == "0");
    }
  }

  // ==========================================================================
  // SECTION 41: JSON array with mixed nulls
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "List: with null and values mixed")
    auto result = mapper.readFromString<oatpp::List<oatpp::Int32>>("[1, null, 3, null, 5]");
    OATPP_ASSERT(result != nullptr);
    OATPP_ASSERT(result->size() >= 3);
  }

  // ==========================================================================
  // SECTION 42: Cached Caret reuse
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "Caret: reuse for multiple deserializations")
    auto json = mapper.writeToString(oatpp::Int32(999));
    oatpp::utils::parser::Caret caret(json);

    for (int i = 0; i < 100; i++) {
      caret.setPosition(0);
      auto result = mapper.readFromCaret<oatpp::Int32>(caret);
      OATPP_ASSERT(result == 999);
    }
  }

  // ==========================================================================
  // SECTION 43: Serialization of DTO missing optional fields
  // ==========================================================================

  {
    OATPP_LOGd(TAG, "DTO: partially filled fields")
    auto dto = SimplePrimitiveDto::createShared();
    dto->f_int32 = 123;
    dto->f_str = "only-two-fields";

    auto json = mapper.writeToString(dto);
    auto result = mapper.readFromString<oatpp::Object<SimplePrimitiveDto>>(json);
    OATPP_ASSERT(result->f_int32 == 123);
    OATPP_ASSERT(result->f_str == "only-two-fields");
    OATPP_ASSERT(result->f_int8 == nullptr);
    OATPP_ASSERT(result->f_bool == nullptr);
    OATPP_ASSERT(result->f_float64 == nullptr);
  }

  OATPP_LOGi(TAG, "All JsonTest tests PASSED");

}

}}
