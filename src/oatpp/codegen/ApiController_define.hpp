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

/**[info]
 * This file contains "defines" for ApiController code generating macro. <br>
 * Usage:<br>
 *
 * ```cpp
 * #include OATPP_CODEGEN_BEGIN(ApiController)
 * ...
 * // Generated Endpoints.
 * ...
 * #include OATPP_CODEGEN_END(ApiController)
 * ```
 *
 *
 * *For details see:*
 * <ul>
 *   <li>[ApiController component](https://oatpp.io/docs/components/api-controller/)</li>
 *   <li>&id:oatpp::web::server::api::ApiController;</li>
 * </ul>
 */

#include "oatpp/macro/basic.hpp"
#include "oatpp/macro/codegen.hpp"

#include "./api_controller/base_define.hpp"
#include "./api_controller/auth_define.hpp"
#include "./api_controller/bundle_define.hpp"
#include "./api_controller/cors_define.hpp"
