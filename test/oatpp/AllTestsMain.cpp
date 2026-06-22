
#include "oatpp/web/ClientRetryTest.hpp"
#include "oatpp/web/FullTest.hpp"
#include "oatpp/web/FullAsyncTest.hpp"
#include "oatpp/web/FullAsyncClientTest.hpp"
#include "oatpp/web/PipelineTest.hpp"
#include "oatpp/web/PipelineAsyncTest.hpp"
#include "oatpp/web/protocol/http/encoding/ChunkedTest.hpp"
#include "oatpp/web/server/api/ApiControllerTest.hpp"
#include "oatpp/web/server/handler/AuthorizationHandlerTest.hpp"
#include "oatpp/web/server/HttpRouterTest.hpp"
#include "oatpp/web/server/ServerStopTest.hpp"
#include "oatpp/web/mime/multipart/StatefulParserTest.hpp"
#include "oatpp/web/mime/ContentMappersTest.hpp"

#include "oatpp/network/virtual_/PipeTest.hpp"
#include "oatpp/network/virtual_/InterfaceTest.hpp"
#include "oatpp/network/UrlTest.hpp"
#include "oatpp/network/ConnectionPoolTest.hpp"
#include "oatpp/network/monitor/ConnectionMonitorTest.hpp"

#include "oatpp/json/DeserializerTest.hpp"
#include "oatpp/json/DTOMapperPerfTest.hpp"
#include "oatpp/json/DTOMapperTest.hpp"
#include "oatpp/json/JsonTest.hpp"
#include "oatpp/json/EnumTest.hpp"
#include "oatpp/json/BooleanTest.hpp"
#include "oatpp/json/UnorderedSetTest.hpp"
#include "oatpp/json/JsonStressTest.hpp"

#include "oatpp/encoding/Base64Test.hpp"
#include "oatpp/encoding/HexTest.hpp"
#include "oatpp/encoding/UnicodeTest.hpp"
#include "oatpp/encoding/UrlTest.hpp"

#include "oatpp/utils/parser/CaretTest.hpp"
#include "oatpp/provider/PoolTest.hpp"
#include "oatpp/provider/PoolTemplateTest.hpp"
#include "oatpp/async/ConditionVariableTest.hpp"
#include "oatpp/async/LockTest.hpp"

#include "oatpp/data/type/UnorderedMapTest.hpp"
#include "oatpp/data/type/PairListTest.hpp"
#include "oatpp/data/type/VectorTest.hpp"
#include "oatpp/data/type/UnorderedSetTest.hpp"
#include "oatpp/data/type/ListTest.hpp"
#include "oatpp/data/type/ObjectTest.hpp"
#include "oatpp/data/type/StringTest.hpp"
#include "oatpp/data/type/PrimitiveTest.hpp"
#include "oatpp/data/type/ObjectWrapperTest.hpp"
#include "oatpp/data/type/TypeTest.hpp"
#include "oatpp/data/type/AnyTest.hpp"
#include "oatpp/data/type/EnumTest.hpp"
#include "oatpp/data/type/InterpretationTest.hpp"
#include "oatpp/data/mapping/TypeResolverTest.hpp"

#include "oatpp/data/resource/InMemoryDataTest.hpp"

#include "oatpp/data/stream/BufferStreamTest.hpp"

#include "oatpp/data/mapping/TreeTest.hpp"
#include "oatpp/data/mapping/ObjectToTreeMapperTest.hpp"
#include "oatpp/data/mapping/TreeToObjectMapperTest.hpp"
#include "oatpp/data/mapping/ObjectRemapperTest.hpp"

#include "oatpp/data/share/LazyStringMapTest.hpp"
#include "oatpp/data/share/StringTemplateTest.hpp"
#include "oatpp/data/share/MemoryLabelTest.hpp"
#include "oatpp/data/buffer/ProcessorTest.hpp"

#include "oatpp/base/CommandLineArgumentsTest.hpp"
#include "oatpp/base/LogTest.hpp"

#include "oatpp/LoggerTest.hpp"

#include "oatpp/async/Coroutine.hpp"

#include "oatpp/data/mapping/Tree.hpp"

#include "oatpp/Environment.hpp"

#include <iostream>
#include <mutex>
#include <cstring>

namespace {

const char* g_testFilter = nullptr;

inline bool testEnabled(const char* name) {
  return !g_testFilter || std::strstr(name, g_testFilter);
}
#define RUN_TEST_IF_0(TEST) \
  do { if (testEnabled(#TEST)) oatpp::test::UnitTest::runTest<TEST>(1); } while(0)
#define RUN_TEST_IF_1(TEST, N) \
  do { if (testEnabled(#TEST)) oatpp::test::UnitTest::runTest<TEST>(N); } while(0)

void runTests() {

  oatpp::Environment::printCompilationConfig();

  OATPP_LOGd("Tests", "oatpp::String size={}", sizeof(oatpp::String))

  OATPP_LOGd("Tests", "oatpp::String size={}", sizeof(oatpp::String))
  OATPP_LOGd("Tests", "std::string size={}", sizeof(std::string))
  OATPP_LOGd("Tests", "Vector size={}", sizeof(std::vector<int>))
  OATPP_LOGd("Tests", "Map size={}", sizeof(std::unordered_map<oatpp::String, oatpp::String>))
  OATPP_LOGd("Tests", "Tree size={}", sizeof(oatpp::data::mapping::Tree))

  OATPP_LOGd("Tests", "coroutine handle size={}", sizeof(oatpp::async::CoroutineHandle))
  OATPP_LOGd("Tests", "coroutine size={}", sizeof(oatpp::async::AbstractCoroutine))
  OATPP_LOGd("Tests", "action size={}", sizeof(oatpp::async::Action))
  OATPP_LOGd("Tests", "class count={}", oatpp::data::type::ClassId::getClassCount())

  auto names = oatpp::data::type::ClassId::getRegisteredClassNames();
  v_int32 i = 0;
  for(auto& name : names) {
    OATPP_LOGd("CLASS", "{} --> '{}'", i, name)
    i ++;
  }

  RUN_TEST_IF_0(oatpp::test::LoggerTest);
  RUN_TEST_IF_0(oatpp::base::CommandLineArgumentsTest);
  RUN_TEST_IF_0(oatpp::base::LogTest);

  RUN_TEST_IF_0(oatpp::data::share::MemoryLabelTest);
  RUN_TEST_IF_0(oatpp::data::share::LazyStringMapTest);
  RUN_TEST_IF_0(oatpp::data::share::StringTemplateTest);

  RUN_TEST_IF_0(oatpp::data::buffer::ProcessorTest);
  RUN_TEST_IF_0(oatpp::data::stream::BufferStreamTest);

  RUN_TEST_IF_0(oatpp::data::mapping::TreeTest);
  RUN_TEST_IF_0(oatpp::data::mapping::ObjectToTreeMapperTest);
  RUN_TEST_IF_0(oatpp::data::mapping::TreeToObjectMapperTest);
  RUN_TEST_IF_0(oatpp::data::mapping::ObjectRemapperTest);

  RUN_TEST_IF_0(oatpp::data::type::ObjectWrapperTest);
  RUN_TEST_IF_0(oatpp::data::type::TypeTest);

  RUN_TEST_IF_0(oatpp::data::type::StringTest);

  RUN_TEST_IF_0(oatpp::data::type::PrimitiveTest);
  RUN_TEST_IF_0(oatpp::data::type::ListTest);
  RUN_TEST_IF_0(oatpp::data::type::VectorTest);
  RUN_TEST_IF_0(oatpp::data::type::UnorderedSetTest);
  RUN_TEST_IF_0(oatpp::data::type::PairListTest);
  RUN_TEST_IF_0(oatpp::data::type::UnorderedMapTest);
  RUN_TEST_IF_0(oatpp::data::type::AnyTest);
  RUN_TEST_IF_0(oatpp::data::type::EnumTest);

  RUN_TEST_IF_0(oatpp::data::type::ObjectTest);

  RUN_TEST_IF_0(oatpp::data::type::InterpretationTest);
  RUN_TEST_IF_0(oatpp::data::mapping::TypeResolverTest);

  RUN_TEST_IF_0(oatpp::data::resource::InMemoryDataTest);

  RUN_TEST_IF_0(oatpp::async::ConditionVariableTest);
  RUN_TEST_IF_0(oatpp::async::LockTest);

  RUN_TEST_IF_0(oatpp::utils::parser::CaretTest);

  RUN_TEST_IF_0(oatpp::provider::PoolTest);
  RUN_TEST_IF_0(oatpp::provider::PoolTemplateTest);

  RUN_TEST_IF_0(oatpp::json::EnumTest);
  RUN_TEST_IF_0(oatpp::json::BooleanTest);

  RUN_TEST_IF_0(oatpp::json::JsonTest);

  RUN_TEST_IF_0(oatpp::json::UnorderedSetTest);

  RUN_TEST_IF_0(oatpp::json::DeserializerTest);

  RUN_TEST_IF_0(oatpp::json::DTOMapperPerfTest);

  RUN_TEST_IF_0(oatpp::json::DTOMapperTest);

  RUN_TEST_IF_0(oatpp::test::json::JsonStressTest);

  RUN_TEST_IF_0(oatpp::test::encoding::Base64Test);
  RUN_TEST_IF_0(oatpp::encoding::HexTest);
  RUN_TEST_IF_0(oatpp::test::encoding::UnicodeTest);
  RUN_TEST_IF_0(oatpp::test::encoding::UrlTest);

  RUN_TEST_IF_0(oatpp::test::network::UrlTest);
  RUN_TEST_IF_0(oatpp::test::network::ConnectionPoolTest);
  RUN_TEST_IF_0(oatpp::test::network::monitor::ConnectionMonitorTest);
  RUN_TEST_IF_0(oatpp::test::network::virtual_::PipeTest);
  RUN_TEST_IF_0(oatpp::test::network::virtual_::InterfaceTest);

  RUN_TEST_IF_0(oatpp::test::web::protocol::http::encoding::ChunkedTest);

  RUN_TEST_IF_0(oatpp::test::web::mime::multipart::StatefulParserTest);
  RUN_TEST_IF_0(oatpp::web::mime::ContentMappersTest);

  RUN_TEST_IF_0(oatpp::test::web::server::HttpRouterTest);
  RUN_TEST_IF_0(oatpp::test::web::server::api::ApiControllerTest);
  RUN_TEST_IF_0(oatpp::test::web::server::handler::AuthorizationHandlerTest);

  if (testEnabled("web::ServerStopTest")) {
    oatpp::test::web::server::ServerStopTest test_virtual(0);
    test_virtual.run();
    oatpp::test::web::server::ServerStopTest test_port(8000);
    test_port.run();
  }

  if (testEnabled("web::PipelineTest")) {
    oatpp::test::web::PipelineTest test_virtual(0, 3000);
    test_virtual.run();
    oatpp::test::web::PipelineTest test_port(8000, 3000);
    test_port.run();
  }

  if (testEnabled("web::PipelineAsyncTest")) {
    oatpp::test::web::PipelineAsyncTest test_virtual(0, 3000);
    test_virtual.run();
    oatpp::test::web::PipelineAsyncTest test_port(8000, 3000);
    test_port.run();
  }

  if (testEnabled("web::FullTest")) {
    oatpp::test::web::FullTest test_virtual(0, 1000);
    test_virtual.run();
    oatpp::test::web::FullTest test_port(8000, 5);
    test_port.run();
  }

  if (testEnabled("web::FullAsyncTest")) {
    oatpp::test::web::FullAsyncTest test_virtual(0, 1000);
    test_virtual.run();
    oatpp::test::web::FullAsyncTest test_port(8000, 5);
    test_port.run();
  }

  if (testEnabled("web::FullAsyncClientTest")) {
    oatpp::test::web::FullAsyncClientTest test_virtual(0, 1000);
    test_virtual.run(20);
    oatpp::test::web::FullAsyncClientTest test_port(8000, 5);
    test_port.run(1);
  }

  if (testEnabled("web::ClientRetryTest")) {
    oatpp::test::web::ClientRetryTest test_virtual(0);
    test_virtual.run();
    oatpp::test::web::ClientRetryTest test_port(8000);
    test_port.run();
  }

}

}

int main(int argc, char* argv[]) {

  oatpp::Environment::init();

  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--filter") == 0 && i + 1 < argc) {
      g_testFilter = argv[++i];
    }
  }

  if (g_testFilter) {
    OATPP_LOGi("TestRunner", "Filter enabled: {}", g_testFilter)
  } else {
    OATPP_LOGi("TestRunner", "No filter set, running all tests")
  }

  runTests();

  /* Print how much objects were created during app running, and what have left-probably leaked */
  /* Disable object counting for release builds using '-D OATPP_DISABLE_ENV_OBJECT_COUNTERS' flag for better performance */
  std::cout << "\nEnvironment:\n";
  std::cout << "objectsCount = " << oatpp::Environment::getObjectsCount() << "\n";
  std::cout << "objectsCreated = " << oatpp::Environment::getObjectsCreated() << "\n\n";

  OATPP_ASSERT(oatpp::Environment::getObjectsCount() == 0)

  oatpp::Environment::destroy();

  return 0;
}

