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
#include "oneflow/core/framework/framework.h"
#include "oneflow/user/kernels/copy_data_content_kernel.h"

namespace oneflow {

#define REGISTER_SQUEEZE_KERNEL(D)                                                              \
  REGISTER_USER_KERNEL("squeeze")                                                               \
      .SetCreateFn<CopyDataContentKernel<DeviceType::D>>()                                      \
      .SetIsMatchedHob(user_op::HobDeviceTag() == DeviceType::D)                                \
      .SetInplaceProposalFn([](const user_op::InferContext&,                                    \
                               user_op::AddInplaceArgPair AddInplaceArgPairFn) -> Maybe<void> { \
        OF_RETURN_IF_ERROR(AddInplaceArgPairFn("out", 0, "in", 0, false));                      \
        return Maybe<void>::Ok();                                                               \
      });

REGISTER_SQUEEZE_KERNEL(kCPU)
#if defined(WITH_CUDA) || defined(WITH_ROCM)
REGISTER_SQUEEZE_KERNEL(kGPU)
#endif

}  // namespace oneflow
