/*
Copyright 2020 The OneFlow Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#ifndef ONEFLOW_CORE_FRAMEWORK_ND_SBP_H_
#define ONEFLOW_CORE_FRAMEWORK_ND_SBP_H_

#include <vector>
#include "oneflow/core/common/symbol.h"
#include "oneflow/core/common/maybe.h"
#include "oneflow/core/job/sbp_parallel.cfg.h"

namespace oneflow {

Maybe<Symbol<cfg::NdSbp>> GetDualNdSbp(Symbol<cfg::NdSbp> sbp_list);

Maybe<Symbol<cfg::NdSbp>> GetNdSbp(
    const std::vector<Symbol<cfg::SbpParallel>>& sbp_list);

Maybe<std::vector<std::string>> GetNdSbpStrList(
    const std::vector<Symbol<cfg::SbpParallel>>& sbp_list);

Maybe<std::vector<std::string>> GetNdSbpStrList(Symbol<cfg::NdSbp> nd_sbp);

Maybe<std::vector<std::string>> GetDualNdSbpStrList(Symbol<cfg::NdSbp> nd_sbp);
}  // namespace oneflow

#endif  // ONEFLOW_CORE_FRAMEWORK_ND_SBP_H_
