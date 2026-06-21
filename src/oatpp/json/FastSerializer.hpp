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

#ifndef oatpp_json_FastSerializer_hpp
#define oatpp_json_FastSerializer_hpp

#include "./Serializer.hpp"

#include "oatpp/data/type/Type.hpp"
#include "oatpp/data/mapping/ObjectToTreeMapper.hpp"
#include "oatpp/data/stream/Stream.hpp"
#include "oatpp/Types.hpp"

namespace oatpp { namespace json {

/**
 * FastSerializer walks oatpp DTO objects and writes JSON directly to a stream,
 * bypassing the Tree intermediate representation entirely.
 *
 * For complex types (Any with typeSelector / polymorphic), falls back to
 * the standard Tree-based serializer path.
 */
class FastSerializer {
public:

  /**
   * Serialize any oatpp typed value as JSON to a stream.
   * Recursively walks the type hierarchy.
   *
   * @param stream - output stream.
   * @param variant - the value to serialize (can be any ObjectWrapper type).
   * @param errorStack - error stack for error reporting.
   * @param mapperConfig - object-to-tree mapper configuration.
   * @param jsonConfig - json serializer configuration.
   * @return true on success.
   */
  static bool serialize(
    data::stream::ConsistentOutputStream* stream,
    const oatpp::Void& variant,
    data::mapping::ErrorStack& errorStack,
    const data::mapping::ObjectToTreeMapper::Config& mapperConfig,
    const Serializer::Config& jsonConfig
  );

};

}} // namespace oatpp::json

#endif // oatpp_json_FastSerializer_hpp
