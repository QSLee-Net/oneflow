#ifndef ONEFLOW_CORE_AUTO_PLACEMNENT_DF_FUNC_H_
#define ONEFLOW_CORE_AUTO_PLACEMNENT_DF_FUNC_H_

#include "oneflow/core/auto_placement/tensor.h"

namespace oneflow {

namespace df {

Tensor ColIndexReduce(const Tensor& input,
                      const std::vector<std::vector<int64_t>>& reduce_indexes);

Tensor IndexReduce(const Tensor& input,
                   const std::vector<std::vector<int64_t>>& reduce_indexes);

Tensor Update(Tensor* var, double lr);

std::vector<Tensor> Clone(const Tensor& input, size_t n);

Tensor Reshape(const Tensor& input, const Shape& shape);

Tensor Minus(const Tensor& input);

Tensor Abs(const Tensor& input);

Tensor Exp(const Tensor& input);

Tensor Tanh(const Tensor& input);

Tensor Tee(const Tensor& input, Tensor* out);

Tensor Add(const Tensor& a, const Tensor& b);

Tensor Sub(const Tensor& a, const Tensor& b);

Tensor ElemWiseMul(const Tensor& a, const Tensor& b);

Tensor ElemWiseDiv(const Tensor& a, const Tensor& b);

Tensor Mul(const Tensor& a, const Tensor& b);

Tensor Reciprocal(const Tensor& input);

Tensor Max(const Tensor& a, const Tensor& b);

Tensor Min(const Tensor& a, const Tensor& b);

Tensor MaxElem(const Tensor& a);

Tensor Relu(const Tensor& input);

Tensor MinElem(const Tensor& a);

Tensor Sum(const Tensor& a);

Tensor Avg(const Tensor& a);

Tensor Variance(const Tensor& a);

Tensor StandardDeviation(const Tensor& a);

Tensor AvgAbsDeviation(const Tensor& a);

Tensor GeAvg(const Tensor& input);

Tensor LeAvg(const Tensor& input);

Tensor DoubleVariance(const Tensor& input);

Tensor Square(const Tensor& input);

Tensor Sqrt(const Tensor& input);

Tensor MatrixRowSum(const Tensor& input);

Tensor MatrixColSum(const Tensor& input);

Tensor MatrixColMax(const Tensor& input);

Tensor TensorProduct(const Tensor& a, const Tensor& b);

Tensor FixedExpectation(const Tensor& a, double e);

Tensor Backward(const Tensor& loss);

}  // namespace df

}  // namespace oneflow

#endif  // ONEFLOW_CORE_AUTO_PLACEMNENT_DF_FUNC_H_
