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
#include "oneflow/core/operator/operator.h"

namespace oneflow {

REGISTER_USER_OP("hierarchical_parallel_cast")
    .Input("in")
    .Output("out")
    .Attr<std::vector<std::string>>("nd_sbp")
    .Attr<std::string>("grad_mode")
    .Attr<std::vector<std::string>>("grad_nd_sbp")
    .SetTensorDescInferFn([](user_op::InferContext* ctx) -> Maybe<void> {
      *ctx->OutputShape("out", 0) = ctx->InputShape("in", 0);
      *ctx->OutputIsDynamic("out", 0) = ctx->InputIsDynamic("in", 0);
      return Maybe<void>::Ok();
    })
    .SetNdSbpInferFn(
        [](user_op::InferNdSbpFnContext* ctx) -> Maybe<void> {
          cfg::NdSbp* in_distribution =
              ctx->NdSbp4ArgNameAndIndex("in", 0);
          cfg::NdSbp* out_distribution =
              ctx->NdSbp4ArgNameAndIndex("out", 0);
          const Shape& parallel_hierarchy = ctx->parallel_hierarchy();
          const auto& conf = ctx->user_op_conf().attr<std::vector<std::string>>("nd_sbp");
          CHECK_EQ_OR_RETURN(conf.size(), parallel_hierarchy.NumAxes());
          for (const std::string& sbp_str : conf) {
            cfg::SbpParallel sbp_parallel;
            CHECK_OR_RETURN(ParseSbpParallelFromString(sbp_str, &sbp_parallel));
            *in_distribution->add_sbp_parallel() = sbp_parallel;
            *out_distribution->add_sbp_parallel() = sbp_parallel;
          }
          return Maybe<void>::Ok();
        })
    .SetDataTypeInferFn([](user_op::InferContext* ctx) -> Maybe<void> {
      *ctx->OutputDType("out", 0) = ctx->InputDType("in", 0);
      return Maybe<void>::Ok();
    })
    .SetGetSbpFn(user_op::GetSbpFnUtil::DefaultBroadcastToBroadcast);

REGISTER_USER_OP("hierarchical_parallel_cast_like")
    .Input("in")
    .Input("like")
    .Output("out")
    .SetTensorDescInferFn([](user_op::InferContext* ctx) -> Maybe<void> {
      *ctx->OutputShape("out", 0) = ctx->InputShape("in", 0);
      *ctx->OutputIsDynamic("out", 0) = ctx->InputIsDynamic("in", 0);
      return Maybe<void>::Ok();
    })
    .SetNdSbpInferFn(
        [](user_op::InferNdSbpFnContext* ctx) -> Maybe<void> {
          cfg::NdSbp* in_distribution =
              ctx->NdSbp4ArgNameAndIndex("in", 0);
          cfg::NdSbp* out_distribution =
              ctx->NdSbp4ArgNameAndIndex("out", 0);
          cfg::NdSbp* like_distribution =
              ctx->NdSbp4ArgNameAndIndex("like", 0);
          const cfg::NdSbp& hint_distribution =
              ctx->NdSbpHint4InputArgNameAndIndex("like", 0);
          *in_distribution = hint_distribution;
          *out_distribution = hint_distribution;
          *like_distribution = hint_distribution;
          return Maybe<void>::Ok();
        })
    .SetDataTypeInferFn([](user_op::InferContext* ctx) -> Maybe<void> {
      *ctx->OutputDType("out", 0) = ctx->InputDType("in", 0);
      return Maybe<void>::Ok();
    })
    .SetGetSbpFn(user_op::GetSbpFnUtil::DefaultBroadcastToBroadcast);

REGISTER_USER_OP_GRAD("hierarchical_parallel_cast")
    .SetBackwardOpConfGenFn([](user_op::BackwardOpConfContext* ctx) -> Maybe<void> {
      if (ctx->FwOp().NeedGenGradTensor4OpInput("in", 0)) {
        const auto& grad_mode = ctx->FwOp().attr<std::string>("grad_mode");
        if (grad_mode == "identity") {
          ctx->FwOp().BindGradTensorWithOpInput(ctx->FwOp().GetGradTensorWithOpOutput("out", 0),
                                                "in", 0);
        } else if (grad_mode == "manual") {
          const std::string grad_op_name = "System-AutoGrad-" + ctx->FwOp().op_name();
          ctx->DefineOp(grad_op_name, [&](user_op::BackwardOpBuilder& builder) {
            return builder.OpTypeName("hierarchical_parallel_cast")
                .InputBind("in", ctx->FwOp().output_grad("out", 0))
                .Output("out")
                .Attr<std::vector<std::string>>(
                    "nd_sbp", ctx->FwOp().attr<std::vector<std::string>>("grad_nd_sbp"))
                .Attr<std::vector<std::string>>("grad_nd_sbp", std::vector<std::string>())
                .Build();
          });
          ctx->FwOp().InputGradBind(user_op::OpArg("in", 0), [&]() -> const std::string& {
            return ctx->GetOp(grad_op_name).output("out", 0);
          });
        } else if (grad_mode == "restore") {
          const std::string grad_op_name = "System-AutoGrad-" + ctx->FwOp().op_name();
          ctx->DefineOp(grad_op_name, [&](user_op::BackwardOpBuilder& builder) {
            return builder.OpTypeName("hierarchical_parallel_cast_like")
                .InputBind("in", ctx->FwOp().output_grad("out", 0))
                .InputBind("like", ctx->FwOp().input("in", 0))
                .Output("out")
                .Build();
          });
          ctx->FwOp().InputGradBind(user_op::OpArg("in", 0), [&]() -> const std::string& {
            return ctx->GetOp(grad_op_name).output("out", 0);
          });
        } else {
          UNIMPLEMENTED();
        }
      }
      return Maybe<void>::Ok();
    });

}  // namespace oneflow
