//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//

// Ensure that gtest is the first header otherwise Windows raises an error
#include <gtest/gtest.h>
// Keep this comment to keep gtest.h above. (clang-format off/on is not working here!)

#include "adapter.hpp"
#include "agent.hpp"
#include "asset_buffer.hpp"
#include "agent_test_helper.hpp"
#include "test_globals.hpp"
#include "xml_printer.hpp"
#include "entity.hpp"
#include <dlib/server.h>

#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>

#if defined(WIN32) && _MSC_VER < 1500
typedef __int64 int64_t;
#endif

// Registers the fixture into the 'registry'
using namespace std;
using namespace std::chrono;
using namespace mtconnect;
using namespace mtconnect::entity;

class AssetBufferTest : public testing::Test
{
 public:
  typedef dlib::map<std::string, std::string>::kernel_1a_c map_type;
  using queue_type = dlib::queue<std::string>::kernel_1a_c;

 protected:
  void SetUp() override
  {
    m_agent = make_unique<Agent>(PROJECT_ROOT_DIR "/samples/test_config.xml", 8, 4, "1.7", 25);
    m_agentId = intToString(getCurrentTimeInSec());
    m_adapter = nullptr;
    m_assetBuffer = make_unique<AssetBuffer>(10);

    m_agentTestHelper = make_unique<AgentTestHelper>();
    m_agentTestHelper->m_agent = m_agent.get();
    m_agentTestHelper->m_queries.clear();
  }

  void TearDown() override
  {
    m_agent.reset();
    m_adapter = nullptr;
    m_agentTestHelper.reset();
    m_assetBuffer.reset();
  }

  void addAdapter()
  {
    ASSERT_FALSE(m_adapter);
    m_adapter = new Adapter("LinuxCNC", "server", 7878);
    m_agent->addAdapter(m_adapter);
    ASSERT_TRUE(m_adapter);
  }
  
  AssetPtr makeAsset(const string &type,
                      const string &uuid, const string &device,
                      const string &ts, ErrorList &errors)
  {
    Properties props {
      {"assetId", uuid},
      {"deviceUuid", device },
      {"timestamp", ts }
    };
    auto asset = Asset::getFactory()->make(type, props , errors);
    return dynamic_pointer_cast<Asset>(asset);
  }

 public:
  std::unique_ptr<Agent> m_agent;
  Adapter *m_adapter{nullptr};
  std::string m_agentId;
  std::unique_ptr<AgentTestHelper> m_agentTestHelper;
  std::unique_ptr<AssetBuffer> m_assetBuffer;

  std::chrono::milliseconds m_delay{};
};

TEST_F(AssetBufferTest, AddAsset)
{
  ErrorList errors;
  auto asset = makeAsset("Asset", "A1", "D1", "2020-12-01T12:00:00Z", errors);
  ASSERT_EQ(0, errors.size());
  
  m_assetBuffer->addAsset(asset);
  ASSERT_EQ(1, m_assetBuffer->getCount());
  ASSERT_EQ(1, m_assetBuffer->getCountForType("Asset"));
  ASSERT_EQ(1, m_assetBuffer->getCountForDevice("D1"));
}

TEST_F(AssetBufferTest, ReplaceAsset)
{
  ErrorList errors;
  auto asset1 = makeAsset("Asset", "A1", "D1", "2020-12-01T12:00:00Z", errors);
  ASSERT_EQ(0, errors.size());
  
  m_assetBuffer->addAsset(asset1);
  ASSERT_EQ(1, m_assetBuffer->getCount());
  
  auto asset2 = makeAsset("Asset", "A1", "D2", "2020-12-01T12:00:00Z", errors);
  ASSERT_EQ(0, errors.size());

  m_assetBuffer->addAsset(asset2);
  ASSERT_EQ(1, m_assetBuffer->getCount());
  ASSERT_EQ(1, m_assetBuffer->getCountForType("Asset"));
  ASSERT_EQ(0, m_assetBuffer->getCountForDevice("D1"));
  ASSERT_EQ(1, m_assetBuffer->getCountForDevice("D2"));
}

TEST_F(AssetBufferTest, TestOverflow)
{
  ErrorList errors;
  for (int i = 0; i < 10; i++)
  {
    auto asset = makeAsset("Asset", "A" + to_string(i),
                            "D" + to_string(i % 3), "2020-12-01T12:00:00Z",
                            errors);
    ASSERT_EQ(0, errors.size());
    m_assetBuffer->addAsset(asset);
  }
  
  ASSERT_EQ(10, m_assetBuffer->getCount());
  ASSERT_EQ(10, m_assetBuffer->getCountForType("Asset"));
  ASSERT_EQ(4, m_assetBuffer->getCountForDevice("D0"));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D1"));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D2"));

  auto asset = makeAsset("Asset", "A11", "D3",
                         "2020-12-01T12:00:00Z", errors);
  ASSERT_EQ(0, errors.size());
  m_assetBuffer->addAsset(asset);
  ASSERT_EQ(10, m_assetBuffer->getCount());
  ASSERT_EQ(10, m_assetBuffer->getCountForType("Asset"));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D0"));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D1"));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D2"));
  ASSERT_EQ(1, m_assetBuffer->getCountForDevice("D3"));
}

TEST_F(AssetBufferTest, RemovedAsset)
{
  ErrorList errors;
  for (int i = 0; i < 10; i++)
  {
    auto asset = makeAsset("Asset", "A" + to_string(i),
                            "D" + to_string(i % 3), "2020-12-01T12:00:00Z",
                            errors);
    ASSERT_EQ(0, errors.size());
    m_assetBuffer->addAsset(asset);
  }
  
  ASSERT_EQ(10, m_assetBuffer->getCount());
  ASSERT_EQ(0, m_assetBuffer->getIndex("A0"));
  ASSERT_EQ(10, m_assetBuffer->getCountForType("Asset"));
  ASSERT_EQ(4, m_assetBuffer->getCountForDevice("D0"));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D1"));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D2"));

  auto a0 = m_assetBuffer->getAsset("A0");
  m_assetBuffer->removeAsset(a0);
  ASSERT_EQ(0, m_assetBuffer->getIndex("A0"));
  
    
  ASSERT_EQ(10, m_assetBuffer->getCount());
  ASSERT_EQ(9, m_assetBuffer->getActiveCount());
  ASSERT_EQ(9, m_assetBuffer->getCountForType("Asset"));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D0"));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D1"));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D2"));

  auto asset = makeAsset("Asset", "A11", "D3",
                         "2020-12-01T12:00:00Z", errors);
  ASSERT_EQ(0, errors.size());
  m_assetBuffer->addAsset(asset);
  
  ASSERT_EQ(-1, m_assetBuffer->getIndex("A0"));
  
  ASSERT_EQ(10, m_assetBuffer->getCount());
  ASSERT_EQ(10, m_assetBuffer->getActiveCount());
  ASSERT_EQ(10, m_assetBuffer->getCountForType("Asset"));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D0"));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D1"));
  ASSERT_EQ(3, m_assetBuffer->getCountForDevice("D2"));
  ASSERT_EQ(1, m_assetBuffer->getCountForDevice("D3"));
}
