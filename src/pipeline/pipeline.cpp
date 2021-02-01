//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include "pipeline.hpp"
#include "agent.hpp"
#include "config.hpp"
#include "shdr_tokenizer.hpp"
#include "shdr_token_mapper.hpp"
#include "duplicate_filter.hpp"
#include "rate_filter.hpp"
#include "timestamp_extractor.hpp"
#include "convert_sample.hpp"
#include "deliver.hpp"

using namespace std;

namespace mtconnect
{
  using namespace adapter;
  using namespace observation;
  using namespace entity;
  
  namespace pipeline
  {
    AdapterPipeline::AdapterPipeline(const ConfigOptions &options, PipelineContextPtr context)
    : Pipeline(options, context)
    {
    }
    
    std::unique_ptr<adapter::Handler> AdapterPipeline::makeHandler()
    {
      auto handler = make_unique<adapter::Handler>();
      
      // Build the pipeline for an adapter
      handler->m_connecting = [this](const Adapter &adapter) {
        auto entity = make_shared<Entity>("ConnectionStatus",
                                          Properties{
          { "VALUE", "CONNECTING" },
          { "id", adapter.getIdentity() }
        });
        run(entity);
      };
      handler->m_connected = [this](const Adapter &adapter) {
        auto entity = make_shared<Entity>("ConnectionStatus",
                                          Properties{
          { "VALUE", "CONNECTED" },
          { "id", adapter.getIdentity() }
        });
        run(entity);
      };
      handler->m_disconnected = [this](const Adapter &adapter) {
        auto entity = make_shared<Entity>("ConnectionStatus",
                                          Properties{
          { "VALUE", "DISCONNECTED" },
          { "id", adapter.getIdentity() }
        });
        run(entity);
      };
      handler->m_processData = [this](const std::string &data) {
        auto entity = make_shared<Entity>("Data", Properties{{"VALUE", data}});
        run(entity);
      };
      handler->m_protocolCommand = [this](const std::string &data) {
        auto entity = make_shared<Entity>("ProtocolCommand", Properties{{"VALUE", data}});
        run(entity);
      };
      
      return handler;
    }


    void AdapterPipeline::build()
    {
      TransformPtr next = bind(make_shared<ShdrTokenizer>());
      //bind(make_shared<DeliverConnectionStatus>());
      //bind(make_shared<DeliverAssetCommand>());
      //bind(make_shared<DeliverProcessCommand>());
      
      // Optional type based transforms
      if (IsOptionSet(m_options, "IgnoreTimestamps"))
        next = next->bind(make_shared<IgnoreTimestamp>());
      else
      {
        auto extract = make_shared<ExtractTimestamp>();
        if (IsOptionSet(m_options, "RelativeTime"))
          extract->m_relativeTime = true;
        next = next->bind(extract);
      }
      
      // Token mapping to data items and assets
      auto mapper = make_shared<ShdrTokenMapper>();
      next = next->bind(mapper);
      mapper->m_getDataItem = m_context->m_findDataItem;
      
      // Handle the observations and send to nowhere
      mapper->bind(make_shared<NullTransform>(TypeGuard<Observations>(RUN)));

      // Go directly to asset delivery
      auto asset = mapper->bind(make_shared<DeliverAsset>(m_context->m_deliverAsset));
            
      // Branched flow
      if (IsOptionSet(m_options, "UpcaseDataItemValue"))
        next = next->bind(make_shared<UpcaseValue>());
      
      if (IsOptionSet(m_options, "FilterDuplicates"))
      {
        auto state = m_context->getSharedState<DuplicateFilter::State>("DuplicationFilter");
        next = next->bind(make_shared<DuplicateFilter>(state));
      }
      {
        auto state = m_context->getSharedState<RateFilter::State>("RateFilter");
        auto rate = make_shared<RateFilter>(state, m_context->m_eachDataItem);
        next = next->bind(rate);
      }

      // Convert values
      if (IsOptionSet(m_options, "ConversionRequired"))
        next = next->bind(make_shared<ConvertSample>());
      
      // Deliver
      next->bind(make_shared<DeliverObservation>(m_context->m_deliverObservation));
    }

  }
}

 
