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

#ifndef oatpp_benchmark_async_BenchController_hpp
#define oatpp_benchmark_async_BenchController_hpp

#include "../DTOs.hpp"

#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/utils/Conversion.hpp"
#include "oatpp/macro/codegen.hpp"
#include "oatpp/macro/component.hpp"

namespace benchmark { namespace async {

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
  // Scenario 1: Hello World — plain text response (async)
  // ========================================================================
  ENDPOINT_ASYNC("GET", "/hello", Hello) {
    ENDPOINT_ASYNC_INIT(Hello)
    Action act() override {
      return _return(controller->createResponse(Status::CODE_200, "Hello, Benchmark World!"));
    }
  };

  // ========================================================================
  // Scenario 2: Small JSON request/response (async)
  // ========================================================================
  ENDPOINT_ASYNC("POST", "/json", JsonSmall) {
    ENDPOINT_ASYNC_INIT(JsonSmall)
    Action act() override {
      return request->readBodyToStringAsync().callbackTo(&JsonSmall::onBodyRead);
    }
    Action onBodyRead(const String& body) {
      OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, mapper);
      auto dto = mapper->readFromString<oatpp::Object<SmallDto>>(body);
      return _return(controller->createDtoResponse(Status::CODE_200, dto));
    }
  };

  // ========================================================================
  // Scenario 3: Large JSON request/response (async, 15-field DTO)
  // ========================================================================
  ENDPOINT_ASYNC("POST", "/json/large", JsonLarge) {
    ENDPOINT_ASYNC_INIT(JsonLarge)
    Action act() override {
      return request->readBodyToStringAsync().callbackTo(&JsonLarge::onBodyRead);
    }
    Action onBodyRead(const String& body) {
      OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, mapper);
      auto dto = mapper->readFromString<oatpp::Object<LargeDto>>(body);
      return _return(controller->createDtoResponse(Status::CODE_200, dto));
    }
  };

  // ========================================================================
  // Scenario 4: Path parameter (async)
  // ========================================================================
  ENDPOINT_ASYNC("GET", "/params/{id}", PathParam) {
    ENDPOINT_ASYNC_INIT(PathParam)
    Action act() override {
      auto id = request->getPathVariable("id");
      auto dto = SmallDto::createShared();
      dto->message = "param=" + id;
      dto->value = static_cast<v_int32>(id->size());
      dto->number = 1.0;
      return _return(controller->createDtoResponse(Status::CODE_200, dto));
    }
  };

  // ========================================================================
  // Scenario 5: Query parameters (async)
  // ========================================================================
  ENDPOINT_ASYNC("GET", "/queries", QueryParams) {
    ENDPOINT_ASYNC_INIT(QueryParams)
    Action act() override {
      auto name = request->getQueryParameter("name");
      auto age = request->getQueryParameter("age");
      auto ageInt = oatpp::utils::Conversion::strToInt32(age->c_str());
      auto dto = SmallDto::createShared();
      dto->message = "name=" + name + "&age=" + oatpp::utils::Conversion::int32ToStr(ageInt);
      dto->value = ageInt;
      dto->number = 2.0;
      return _return(controller->createDtoResponse(Status::CODE_200, dto));
    }
  };

  // ========================================================================
  // Scenario 6: Plain text body echo (async)
  // ========================================================================
  ENDPOINT_ASYNC("POST", "/echo", EchoBody) {
    ENDPOINT_ASYNC_INIT(EchoBody)
    Action act() override {
      return request->readBodyToStringAsync().callbackTo(&EchoBody::onBodyRead);
    }
    Action onBodyRead(const String& body) {
      return _return(controller->createResponse(Status::CODE_200, body));
    }
  };

  // ========================================================================
  // Scenario 7: Multiple custom headers (async)
  // ========================================================================
  ENDPOINT_ASYNC("GET", "/headers", MultiHeaders) {
    ENDPOINT_ASYNC_INIT(MultiHeaders)
    Action act() override {
      auto h1 = request->getHeader("X-Header-1");
      auto h2 = request->getHeader("X-Header-2");
      auto h3 = request->getHeader("X-Header-3");
      auto dto = SmallDto::createShared();
      dto->message = h1;
      dto->value = oatpp::utils::Conversion::strToInt32(h2->c_str());
      dto->number = oatpp::utils::Conversion::strToFloat64(h3->c_str());
      return _return(controller->createDtoResponse(Status::CODE_200, dto));
    }
  };

  // ========================================================================
  // Scenario 8: Mixed-type DTO (async)
  // ========================================================================
  ENDPOINT_ASYNC("POST", "/mixed", MixedPayload) {
    ENDPOINT_ASYNC_INIT(MixedPayload)
    Action act() override {
      return request->readBodyToStringAsync().callbackTo(&MixedPayload::onBodyRead);
    }
    Action onBodyRead(const String& body) {
      OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, mapper);
      auto dto = mapper->readFromString<oatpp::Object<MixedDto>>(body);
      return _return(controller->createDtoResponse(Status::CODE_200, dto));
    }
  };

  // ========================================================================
  // Scenario 9: Deeply nested JSON (async, 3 levels)
  // ========================================================================
  ENDPOINT_ASYNC("POST", "/nested", NestedJson) {
    ENDPOINT_ASYNC_INIT(NestedJson)
    Action act() override {
      return request->readBodyToStringAsync().callbackTo(&NestedJson::onBodyRead);
    }
    Action onBodyRead(const String& body) {
      OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, mapper);
      auto dto = mapper->readFromString<oatpp::Object<Level1Dto>>(body);
      return _return(controller->createDtoResponse(Status::CODE_200, dto));
    }
  };

  // ========================================================================
  // Scenario 10: Array response — 100 items (async)
  // ========================================================================
  ENDPOINT_ASYNC("GET", "/array", ArrayResponse) {
    ENDPOINT_ASYNC_INIT(ArrayResponse)
    Action act() override {
      auto items = oatpp::Vector<oatpp::Object<ItemDto>>::createShared();
      for (v_int32 i = 0; i < 100; i++) {
        auto item = ItemDto::createShared();
        item->id = i;
        item->name = "Item-" + oatpp::utils::Conversion::int32ToStr(i);
        item->price = static_cast<v_float64>(i) * 1.5;
        items->push_back(item);
      }
      return _return(controller->createDtoResponse(Status::CODE_200, items));
    }
  };

#include OATPP_CODEGEN_END(ApiController)

};

}} // namespace benchmark::async

#endif /* oatpp_benchmark_async_BenchController_hpp */
