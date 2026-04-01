# Copyright (c) MLLM Team.
# Licensed under the MIT License.
#
# CPU JIT kernels with Highway SIMD support.

from .add_constant import add_constant, add_constant_runtime
from .matmul import matmul_naive, matmul_reorder, matmul_blocked, matmul_omp
from .quant_matmul import (
    quant_matmul_w4a8,
    quant_matmul_w4a8_raw,
    quantize_q4,
    quantize_q8,
    dequantize_q4,
    dequantize_q8,
)

__all__ = [
    "add_constant",
    "add_constant_runtime",
    "matmul_naive",
    "matmul_reorder",
    "matmul_blocked",
    "matmul_omp",
    "quant_matmul_w4a8",
    "quant_matmul_w4a8_raw",
    "quantize_q4",
    "quantize_q8",
    "dequantize_q4",
    "dequantize_q8",
]
