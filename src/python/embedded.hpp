//
// Copyright Copyright 2009-2022, AMT – The Association For Manufacturing Technology (“AMT”)
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

#include <boost/python.hpp>

#include <string>

#include "utilities.hpp"

namespace mtconnect {
  class Agent;

  namespace python {
    struct Context;
    class Embedded
    {
    public:
      Embedded(Agent *agent, const ConfigOptions &options);
      ~Embedded();

    protected:
      Agent *m_agent;
      Context *m_context;
      ConfigOptions m_options;
    };
  }  // namespace python
}  // namespace mtconnect
