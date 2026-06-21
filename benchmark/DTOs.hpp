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

#ifndef oatpp_benchmark_DTOs_hpp
#define oatpp_benchmark_DTOs_hpp

#include "oatpp/macro/codegen.hpp"
#include "oatpp/Types.hpp"

namespace benchmark {

#include OATPP_CODEGEN_BEGIN(DTO)

/**
 * Small DTO for basic JSON round-trip benchmarks.
 */
class SmallDto : public oatpp::DTO {
  DTO_INIT(SmallDto, DTO)
  DTO_FIELD(String, message);
  DTO_FIELD(Int32, value);
  DTO_FIELD(Float64, number);
};

/**
 * Large DTO with 15 fields for big-payload benchmarks.
 */
class LargeDto : public oatpp::DTO {
  DTO_INIT(LargeDto, DTO)
  DTO_FIELD(String, field1);
  DTO_FIELD(Int32, field2);
  DTO_FIELD(Float64, field3);
  DTO_FIELD(Boolean, field4);
  DTO_FIELD(String, field5);
  DTO_FIELD(String, field6);
  DTO_FIELD(String, field7);
  DTO_FIELD(Int32, field8);
  DTO_FIELD(Int32, field9);
  DTO_FIELD(Float64, field10);
  DTO_FIELD(Vector<Int32>, field11);
  DTO_FIELD(Vector<String>, field12);
  DTO_FIELD(Float32, field13);
  DTO_FIELD(Int64, field14);
  DTO_FIELD(Boolean, field15);
};

/**
 * Enum for testing enum serialization in MixedDto.
 */
ENUM(ColorEnum, v_int32,
  VALUE(RED, 0, "RED"),
  VALUE(GREEN, 1, "GREEN"),
  VALUE(BLUE, 2, "BLUE")
)

/**
 * Mixed-type DTO exercising String, Int, Float, Bool, Vector, and Enum.
 */
class MixedDto : public oatpp::DTO {
  DTO_INIT(MixedDto, DTO)
  DTO_FIELD(String, name);
  DTO_FIELD(Int32, age);
  DTO_FIELD(Float64, score);
  DTO_FIELD(Boolean, active);
  DTO_FIELD(Vector<String>, tags);
  DTO_FIELD(Enum<ColorEnum>::AsString, favoriteColor);
};

/**
 * Deeply nested DTO — Level 3 (leaf).
 */
class Level3Dto : public oatpp::DTO {
  DTO_INIT(Level3Dto, DTO)
  DTO_FIELD(String, value);
  DTO_FIELD(Int32, count);
};

/**
 * Deeply nested DTO — Level 2 (mid).
 */
class Level2Dto : public oatpp::DTO {
  DTO_INIT(Level2Dto, DTO)
  DTO_FIELD(String, name);
  DTO_FIELD(Object<Level3Dto>, child);
};

/**
 * Deeply nested DTO — Level 1 (root).
 */
class Level1Dto : public oatpp::DTO {
  DTO_INIT(Level1Dto, DTO)
  DTO_FIELD(String, title);
  DTO_FIELD(Object<Level2Dto>, nested);
};

/**
 * Item DTO used in the array-response scenario.
 */
class ItemDto : public oatpp::DTO {
  DTO_INIT(ItemDto, DTO)
  DTO_FIELD(Int32, id);
  DTO_FIELD(String, name);
  DTO_FIELD(Float64, price);
};

#include OATPP_CODEGEN_END(DTO)

} // namespace benchmark

#endif /* oatpp_benchmark_DTOs_hpp */
