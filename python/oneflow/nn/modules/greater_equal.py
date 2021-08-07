"""
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
"""
import oneflow as flow
from oneflow.framework.tensor import register_tensor_op
from oneflow.nn.module import Module


class GreaterEqual(Module):
    def __init__(self) -> None:
        super().__init__()

    def forward(self, x, y):
        if x.dtype != flow.float32:
            x = flow.cast(x, flow.float32)
        if isinstance(y, int) or isinstance(y, float):
            return flow.F.scalar_logical_greater_equal(x, float(y))
        if y.dtype != flow.float32:
            y = flow.cast(y, flow.float32)
        return flow.F.broadcast_greater_equal(x, y)


def greater_equal_op(x, y):
    """Returns the truth value of :math:`x >= y` element-wise.

    Args:
        x (oneflow.Tensor): A Tensor
        y (oneflow.Tensor): A Tensor

    Returns:
        oneflow.Tensor: A Tensor with int8 type.

    For example:

    .. code-block:: python

        >>> import numpy as np
        >>> import oneflow as flow
        
        >>> input1 = flow.Tensor(np.array([1, 2, 3]).astype(np.float32), dtype=flow.float32)
        >>> input2 = flow.Tensor(np.array([1, 1, 4]).astype(np.float32), dtype=flow.float32)

        >>> out = flow.ge(input1, input2)
        >>> out
        tensor([1, 1, 0], dtype=oneflow.int8)

    """
    return GreaterEqual()(x, y)


@register_tensor_op("ge")
def greater_equal_op_tensor(x, y):
    """

    ge() -> Tensor

    See :func:`oneflow.ge`

    """
    return GreaterEqual()(x, y)


if __name__ == "__main__":
    import doctest

    doctest.testmod(raise_on_error=True)
