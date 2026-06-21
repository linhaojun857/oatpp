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

#ifndef oatpp_json_FastDeserializer_hpp
#define oatpp_json_FastDeserializer_hpp

#include "./Deserializer.hpp"

#include "oatpp/data/type/Type.hpp"
#include "oatpp/data/mapping/TreeToObjectMapper.hpp"
#include "oatpp/utils/parser/Caret.hpp"
#include "oatpp/Types.hpp"

namespace oatpp { namespace json {

/**
 * FastDeserializer: JSON text → DTO, bypassing the Tree intermediate representation.
 *
 * Falls back to the standard Tree-based Deserializer for:
 *   - Any type (requires runtime type guessing)
 *   - Polymorphic fields (typeSelector)
 *   - Small documents (< 64 bytes)
 */
class FastDeserializer {
public:
  /**
   * Deserialize JSON text directly into an oatpp typed result.
   * Recursively walks the type hierarchy.
   * @param caret - parser caret positioned at the start of JSON text.
   * @param type - expected result type (must be a DTO/object type for fast path).
   * @param errorStack - error stack for error reporting.
   * @param mapperConfig - tree-to-object mapper configuration.
   * @param jsonConfig - json deserializer configuration.
   * @return parsed value on success, nullptr on failure.
   */
  static oatpp::Void deserialize(
    oatpp::utils::parser::Caret& caret,
    const data::type::Type* type,
    data::mapping::ErrorStack& errorStack,
    const data::mapping::TreeToObjectMapper::Config& mapperConfig,
    const Deserializer::Config& jsonConfig
  );

};

}} // namespace oatpp::json

#endif // oatpp_json_FastDeserializer_hpp
