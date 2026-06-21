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

#include "./AppComponent.hpp"
#include "./BenchController.hpp"

#include "oatpp/network/Server.hpp"
#include "oatpp/Environment.hpp"

#include <iostream>
#include <csignal>
#include <atomic>

namespace {

std::atomic<bool> RUNNING{true};

void signalHandler(int /*signal*/) {
  RUNNING.store(false);
}

void run(v_uint16 port) {

  /* Register components in scope of run() method */
  benchmark::sync::AppComponent components(port);

  /* Inject router and add controllers */
  OATPP_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, router);
  router->addController(benchmark::sync::BenchController::createShared());

  /* Inject connection provider and connection handler */
  OATPP_COMPONENT(std::shared_ptr<oatpp::network::ServerConnectionProvider>, connectionProvider);
  OATPP_COMPONENT(std::shared_ptr<oatpp::network::ConnectionHandler>, connectionHandler);

  /* Create server */
  oatpp::network::Server server(connectionProvider, connectionHandler);

  /* Print server info */
  OATPP_LOGd("BenchSync", "Server running on http://{}:{}",
    connectionProvider->getProperty("host").toString(),
    connectionProvider->getProperty("port").toString()
  )

  /* Run server with stop condition */
  std::function<bool()> condition = []() { return RUNNING.load(); };
  server.run(condition);

  /* Stop connection handler after server stops */
  connectionHandler->stop();

  OATPP_LOGd("BenchSync", "Server stopped")
}

} // anonymous namespace

int main(int argc, char* argv[]) {

  oatpp::Environment::init();

  v_uint16 port = 8000;
  if (argc > 1) {
    port = static_cast<v_uint16>(std::atoi(argv[1]));
  }

  std::signal(SIGINT,  signalHandler);
  std::signal(SIGTERM, signalHandler);

  std::cout << "============================================================" << std::endl;
  std::cout << "  oatpp Sync Benchmark Server" << std::endl;
  std::cout << "  Listening on port: " << port << std::endl;
  std::cout << "  Press Ctrl+C to stop." << std::endl;
  std::cout << "============================================================" << std::endl;

  run(port);

  std::cout << std::endl;
  std::cout << "Objects count:   " << oatpp::Environment::getObjectsCount() << std::endl;
  std::cout << "Objects created: " << oatpp::Environment::getObjectsCreated() << std::endl;

  oatpp::Environment::destroy();

  return 0;
}
