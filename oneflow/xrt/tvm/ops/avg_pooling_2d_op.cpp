#include "oneflow/xrt/tvm/ops/op_kernel.h"
#include <tvm/relay/attrs/nn.h>
#include "oneflow/xrt/tvm/ops/nn_util.h"

namespace oneflow {
namespace xrt {
namespace of_tvm {

class AveragePooling2DOp final : public TVMOpKernel {
 public:
  void Compile(TVMOpContext* ctx) override {
    LOG(WARNING) << ctx->DebugStr();
    tvm::Array<tvm::relay::Expr> node_inputs;
    node_inputs.push_back(ctx->GetExpr4InputName("x_0"));

    auto attrs = tvm::runtime::make_object<tvm::relay::AvgPool2DAttrs>();
    {
      std::string data_format = ctx->Attr<std::string>("data_format");
      CHECK(data_format == "channels_last" || data_format == "channels_first")
        << "Wrong data_format: " << data_format;
      if (data_format == "channels_first") {
        data_format = "NCHW";
      } else {
        data_format = "NHWC";
      }
      attrs->layout = data_format;
      attrs->ceil_mode = false;
      // corresponding to CUDNN_POOLING_AVERAGE_COUNT_EXCLUDE_PADDING
      attrs->count_include_pad = false;

      std::vector<int32_t> strides = ctx->Attr<std::vector<int32_t>>("strides");
      CHECK_EQ(2, strides.size());
      attrs->strides = tvm::Array<tvm::relay::IndexExpr>({strides.at(0), strides.at(1)});

      std::vector<int32_t> pool_size = ctx->Attr<std::vector<int32_t>>("pool_size");
      CHECK_EQ(2, pool_size.size());
      attrs->pool_size = tvm::Array<tvm::relay::IndexExpr>({pool_size.at(0), pool_size.at(1)});

      attrs->padding = Calc2DPadding4Pool(data_format, ctx->Attr<std::string>("padding"),
          ctx->GetShape4InputName("x_0"), pool_size, strides);
    }

    auto op = tvm::relay::Op::Get("nn.avg_pool2d");
    auto expr = tvm::relay::Call(op, node_inputs, tvm::Attrs(attrs), {});
    ctx->SetExpr4OutputName("y_0", std::move(expr));
  }
};

REGISTER_TVM_OP_KERNEL(AveragePooling2D, AveragePooling2DOp).Finalize();

}  // namespace of_tvm
}  // namespace xrt
}  // namespace oneflow
