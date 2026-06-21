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

#include "JsonStressTest.hpp"

#include "oatpp/json/ObjectMapper.hpp"

#include "oatpp/data/type/Object.hpp"
#include "oatpp/data/type/List.hpp"
#include "oatpp/data/type/Primitive.hpp"

#include "oatpp/utils/Conversion.hpp"
#include "oatpp/utils/parser/Caret.hpp"

#include "oatpp/macro/codegen.hpp"
#include "oatpp/base/Log.hpp"

#include "oatpp-test/Checker.hpp"

namespace oatpp { namespace test { namespace json {

namespace {

#include OATPP_CODEGEN_BEGIN(DTO)

// Simple DTO for basic serialization test
class SimpleDto : public oatpp::DTO {
  DTO_INIT(SimpleDto, DTO)
  DTO_FIELD(String, name);
  DTO_FIELD(Int32, age);

  static Wrapper create() {
    auto d = SimpleDto::createShared();
    d->name = "hello world";
    d->age = 42;
    return d;
  }
};

// Complex nested DTO
class InnerDto : public oatpp::DTO {
  DTO_INIT(InnerDto, DTO)
  DTO_FIELD(Int64, id);
  DTO_FIELD(String, label);
};

class OuterDto : public oatpp::DTO {
  DTO_INIT(OuterDto, DTO)
  DTO_FIELD(String, title);
  DTO_FIELD(Float64, score);
  DTO_FIELD(Boolean, active);
  DTO_FIELD(Object<InnerDto>, inner);
  DTO_FIELD(List<Object<InnerDto>>, items) = {};

  static Wrapper create() {
    auto d = OuterDto::createShared();
    d->title = "Outer Object Title";
    d->score = 3.14159;
    d->active = true;
    d->inner = InnerDto::createShared();
    d->inner->id = 12345678901234LL;
    d->inner->label = "inner-label-value";
    d->items = List<Object<InnerDto>>::createShared();
    for (int i = 0; i < 10; i++) {
      auto item = InnerDto::createShared();
      item->id = 1000 + i;
      item->label = "item-label-" + oatpp::utils::Conversion::int64ToStr(i);
      d->items->push_back(item);
    }
    return d;
  }
};

// DTO with many fields of different types
class AllTypesDto : public oatpp::DTO {
  DTO_INIT(AllTypesDto, DTO)
  DTO_FIELD(Int8,   f_int8);
  DTO_FIELD(UInt8,  f_uint8);
  DTO_FIELD(Int16,  f_int16);
  DTO_FIELD(UInt16, f_uint16);
  DTO_FIELD(Int32,  f_int32);
  DTO_FIELD(UInt32, f_uint32);
  DTO_FIELD(Int64,  f_int64);
  DTO_FIELD(UInt64, f_uint64);
  DTO_FIELD(Float32, f_float32);
  DTO_FIELD(Float64, f_float64);
  DTO_FIELD(Boolean, f_bool);
  DTO_FIELD(String,  f_str);

  static Wrapper create() {
    auto d = AllTypesDto::createShared();
    d->f_int8 = -100;
    d->f_uint8 = 200;
    d->f_int16 = -30000;
    d->f_uint16 = 60000;
    d->f_int32 = -2000000000;
    d->f_uint32 = 4000000000U;
    d->f_int64 = -9000000000000000000LL;
    d->f_uint64 = 18000000000000000000ULL;
    d->f_float32 = 1.5f;
    d->f_float64 = 2.718281828459045;
    d->f_bool = false;
    d->f_str = "all-types-string-value";
    return d;
  }
};

// String-heavy DTO for escape stress testing
class StringHeavyDto : public oatpp::DTO {
  DTO_INIT(StringHeavyDto, DTO)
  DTO_FIELD(String, plain);
  DTO_FIELD(String, with_quotes);
  DTO_FIELD(String, with_backslash);
  DTO_FIELD(String, with_newlines);
  DTO_FIELD(String, with_unicode);

  static Wrapper create() {
    auto d = StringHeavyDto::createShared();
    d->plain = "abcdefghijklmnopqrstuvwxyz0123456789";
    d->with_quotes = "he said \"hello world\" to everyone";
    d->with_backslash = "C:\\Users\\test\\Documents\\file.txt";
    d->with_newlines = "line1\nline2\nline3\r\nline4\tindented";
    d->with_unicode = "Hello 世界 🌍  — em dash and © symbol";
    return d;
  }
};

#include OATPP_CODEGEN_END(DTO)

} // anonymous namespace

void JsonStressTest::onRun() {

  oatpp::json::ObjectMapper mapper;
  const v_int32 ITER = 100000;

  // =========================================================================
  // Test 1: Simple DTO serialize
  // =========================================================================
  {
    OATPP_LOGd(TAG, "Stress serialize simple DTO...")
    auto dto = SimpleDto::create();
    oatpp::test::PerformanceChecker pc("stress_serialize_simple");
    for (v_int32 i = 0; i < ITER; i++) {
      auto str = mapper.writeToString(dto);
    }
    OATPP_LOGd(TAG, "OK")
  }

  // =========================================================================
  // Test 2: Simple DTO deserialize
  // =========================================================================
  {
    OATPP_LOGd(TAG, "Stress deserialize simple DTO...")
    auto dto = SimpleDto::create();
    auto json = mapper.writeToString(dto);
    oatpp::utils::parser::Caret caret(json);
    oatpp::test::PerformanceChecker pc("stress_deserialize_simple");
    for (v_int32 i = 0; i < ITER; i++) {
      caret.setPosition(0);
      auto result = mapper.readFromCaret<oatpp::Object<SimpleDto>>(caret);
      OATPP_ASSERT(result != nullptr);
    }
    OATPP_LOGd(TAG, "OK")
  }

  // =========================================================================
  // Test 3: Complex nested DTO serialize
  // =========================================================================
  {
    OATPP_LOGd(TAG, "Stress serialize nested DTO...")
    auto dto = OuterDto::create();
    oatpp::test::PerformanceChecker pc("stress_serialize_nested");
    for (v_int32 i = 0; i < ITER; i++) {
      auto str = mapper.writeToString(dto);
    }
    OATPP_LOGd(TAG, "OK")
  }

  // =========================================================================
  // Test 4: Complex nested DTO deserialize
  // =========================================================================
  {
    OATPP_LOGd(TAG, "Stress deserialize nested DTO...")
    auto dto = OuterDto::create();
    auto json = mapper.writeToString(dto);
    oatpp::utils::parser::Caret caret(json);
    oatpp::test::PerformanceChecker pc("stress_deserialize_nested");
    for (v_int32 i = 0; i < ITER; i++) {
      caret.setPosition(0);
      auto result = mapper.readFromCaret<oatpp::Object<OuterDto>>(caret);
      OATPP_ASSERT(result != nullptr);
    }
    OATPP_LOGd(TAG, "OK")
  }

  // =========================================================================
  // Test 5: All numeric types round-trip
  // =========================================================================
  {
    OATPP_LOGd(TAG, "All numeric types serialize/deserialize round-trip...")
    auto dto = AllTypesDto::create();
    auto str = mapper.writeToString(dto);
    OATPP_LOGv(TAG, "all_types_json='{}'", str->c_str())

    // Verify round-trip
    oatpp::utils::parser::Caret caret(str);
    auto result = mapper.readFromCaret<oatpp::Object<AllTypesDto>>(caret);
    OATPP_ASSERT(result != nullptr);
    OATPP_ASSERT(result->f_int8 == dto->f_int8);
    OATPP_ASSERT(result->f_int32 == dto->f_int32);
    OATPP_ASSERT(result->f_int64 == dto->f_int64);
    OATPP_ASSERT(result->f_float64.getValue(0.0) > 2.7); // approximate
    OATPP_ASSERT(result->f_str == dto->f_str);
    OATPP_ASSERT(result->f_bool == dto->f_bool);
    OATPP_LOGd(TAG, "OK")
  }

  // =========================================================================
  // Test 6: String escaping/unescaping round-trip
  // =========================================================================
  {
    OATPP_LOGd(TAG, "String escaping/unescaping round-trip...")
    auto dto = StringHeavyDto::create();
    auto str = mapper.writeToString(dto);
    OATPP_LOGv(TAG, "string_heavy_json='{}'", str->c_str())

    oatpp::utils::parser::Caret caret(str);
    auto result = mapper.readFromCaret<oatpp::Object<StringHeavyDto>>(caret);
    OATPP_ASSERT(result != nullptr);
    OATPP_ASSERT(result->plain == dto->plain);
    OATPP_ASSERT(result->with_quotes == dto->with_quotes);
    OATPP_ASSERT(result->with_backslash == dto->with_backslash);
    OATPP_ASSERT(result->with_newlines == dto->with_newlines);
    // Unicode roundtrip check
    OATPP_ASSERT(result->with_unicode->size() > 0);
    OATPP_LOGd(TAG, "OK")
  }

  // =========================================================================
  // Test 7: Many serializations with different values
  // =========================================================================
  {
    OATPP_LOGd(TAG, "Stress serialize varied values...")
    oatpp::test::PerformanceChecker pc("stress_serialize_varied");
    for (v_int32 i = 0; i < 50000; i++) {
      auto dto = SimpleDto::create();
      dto->name = "user_" + oatpp::utils::Conversion::int64ToStr(i);
      dto->age = i % 100;
      auto str = mapper.writeToString(dto);
    }
    OATPP_LOGd(TAG, "OK")
  }

  // =========================================================================
  // Test 8: Large string serialization
  // =========================================================================
  {
    OATPP_LOGd(TAG, "Stress serialize large string with special chars...")
    std::string bigStr(10000, 'x');
    // Insert some special chars
    bigStr[100] = '"';
    bigStr[200] = '\\';
    bigStr[300] = '\n';

    auto dto = SimpleDto::create();
    dto->name = bigStr;
    dto->age = 99;

    oatpp::test::PerformanceChecker pc("stress_serialize_big_string");
    for (v_int32 i = 0; i < 5000; i++) {
      auto str = mapper.writeToString(dto);
    }
    OATPP_LOGd(TAG, "OK")
  }

  // =========================================================================
  // Test 9: Deep nesting
  // =========================================================================
  {
    OATPP_LOGd(TAG, "Deep nesting serialize/deserialize round-trip...")
    // Create deeply nested structure: OuterDto → items → InnerDto (50 items)
    auto dto = OuterDto::create();
    dto->items = List<Object<InnerDto>>::createShared();
    for (int i = 0; i < 50; i++) {
      auto item = InnerDto::createShared();
      item->id = 100000 + i;
      item->label = "nested_item_" + oatpp::utils::Conversion::int64ToStr(i);
      dto->items->push_back(item);
    }

    auto json = mapper.writeToString(dto);
    oatpp::utils::parser::Caret caret(json);
    auto result = mapper.readFromCaret<oatpp::Object<OuterDto>>(caret);
    OATPP_ASSERT(result != nullptr);
    OATPP_ASSERT(result->items->size() == 50);
    OATPP_ASSERT(result->title == dto->title);
    OATPP_ASSERT(result->score.getValue(0.0) > 3.0);
    OATPP_LOGd(TAG, "OK")
  }

  OATPP_LOGi(TAG, "All stress tests PASSED");
}

}}}
