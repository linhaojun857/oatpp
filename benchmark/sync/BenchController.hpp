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

#ifndef oatpp_benchmark_sync_BenchController_hpp
#define oatpp_benchmark_sync_BenchController_hpp

#include "../DTOs.hpp"

#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/utils/Conversion.hpp"
#include "oatpp/macro/codegen.hpp"
#include "oatpp/macro/component.hpp"

namespace benchmark { namespace sync {

class BenchController : public oatpp::web::server::api::ApiController {
public:
  BenchController(const std::shared_ptr<ObjectMapper>& objectMapper)
    : oatpp::web::server::api::ApiController(objectMapper)
  {}

  static std::shared_ptr<BenchController> createShared(
      const std::shared_ptr<ObjectMapper>& objectMapper =
        OATPP_GET_COMPONENT(std::shared_ptr<ObjectMapper>))
  {
    return std::make_shared<BenchController>(objectMapper);
  }

#include OATPP_CODEGEN_BEGIN(ApiController)

  // ========================================================================
  // Scenario 1: Hello World — plain text response
  // ========================================================================
  ENDPOINT("GET", "/hello", hello) {
    return createResponse(Status::CODE_200, "Hello, Benchmark World!");
  }

  // ========================================================================
  // Scenario 2: Small JSON request/response
  // ========================================================================
  ENDPOINT("POST", "/json", jsonSmall,
           BODY_DTO(Object<SmallDto>, body))
  {
    return createDtoResponse(Status::CODE_200, body);
  }

  // ========================================================================
  // Scenario 3: Large JSON request/response (15-field DTO)
  // ========================================================================
  ENDPOINT("POST", "/json/large", jsonLarge,
           BODY_DTO(Object<LargeDto>, body))
  {
    return createDtoResponse(Status::CODE_200, body);
  }

  // ========================================================================
  // Scenario 4: Path parameter
  // ========================================================================
  ENDPOINT("GET", "/params/{id}", pathParam,
           PATH(String, id))
  {
    auto dto = SmallDto::createShared();
    dto->message = "param=" + id;
    dto->value = static_cast<v_int32>(id->size());
    dto->number = 1.0;
    return createDtoResponse(Status::CODE_200, dto);
  }

  // ========================================================================
  // Scenario 5: Query parameters
  // ========================================================================
  ENDPOINT("GET", "/queries", queryParams,
           QUERY(String, name),
           QUERY(Int32, age))
  {
    auto dto = SmallDto::createShared();
    dto->message = "name=" + name + "&age=" + oatpp::utils::Conversion::int32ToStr(age);
    dto->value = age;
    dto->number = 2.0;
    return createDtoResponse(Status::CODE_200, dto);
  }

  // ========================================================================
  // Scenario 6: Plain text body echo
  // ========================================================================
  ENDPOINT("POST", "/echo", echoBody,
           BODY_STRING(String, body))
  {
    return createResponse(Status::CODE_200, body);
  }

  // ========================================================================
  // Scenario 7: Multiple custom headers
  // ========================================================================
  ENDPOINT("GET", "/headers", multiHeaders,
           HEADER(String, h1, "X-Header-1"),
           HEADER(String, h2, "X-Header-2"),
           HEADER(String, h3, "X-Header-3"))
  {
    auto dto = SmallDto::createShared();
    dto->message = h1;
    dto->value = oatpp::utils::Conversion::strToInt32(h2->c_str());
    dto->number = oatpp::utils::Conversion::strToFloat64(h3->c_str());
    return createDtoResponse(Status::CODE_200, dto);
  }

  // ========================================================================
  // Scenario 8: Mixed-type DTO (String, Int, Bool, Vector, Enum)
  // ========================================================================
  ENDPOINT("POST", "/mixed", mixedPayload,
           BODY_DTO(Object<MixedDto>, body))
  {
    return createDtoResponse(Status::CODE_200, body);
  }

  // ========================================================================
  // Scenario 9: Deeply nested JSON (3 levels)
  // ========================================================================
  ENDPOINT("POST", "/nested", nestedJson,
           BODY_DTO(Object<Level1Dto>, body))
  {
    return createDtoResponse(Status::CODE_200, body);
  }

  // ========================================================================
  // Scenario 10: Array response — 100 items
  // ========================================================================
  ENDPOINT("GET", "/array", arrayResponse) {
    auto items = oatpp::Vector<oatpp::Object<ItemDto>>::createShared();
    for (v_int32 i = 0; i < 100; i++) {
      auto item = ItemDto::createShared();
      item->id = i;
      item->name = "Item-" + oatpp::utils::Conversion::int32ToStr(i);
      item->price = static_cast<v_float64>(i) * 1.5;
      items->push_back(item);
    }
    return createDtoResponse(Status::CODE_200, items);
  }

#include OATPP_CODEGEN_END(ApiController)

};

}} // namespace benchmark::sync

#endif /* oatpp_benchmark_sync_BenchController_hpp */
