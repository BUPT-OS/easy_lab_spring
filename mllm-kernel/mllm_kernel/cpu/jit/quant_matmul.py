# Copyright (c) MLLM Team.
# Licensed under the MIT License.
#
# W4A8 Quantized matrix multiplication JIT kernel for OS101 lab.

from __future__ import annotations

import ctypes
import platform
import struct

import numpy as np
import torch

from mllm_kernel.jit_utils import jit


def _get_omp_flags() -> tuple[list[str], list[str]]:
    """Get OpenMP compiler and linker flags for the current platform."""
    system = platform.system()
    if system == "Linux":
        return ["-fopenmp"], ["-fopenmp"]
    elif system == "Darwin":
        return ["-Xpreprocessor", "-fopenmp"], ["-lomp"]
    else:
        return ["-fopenmp"], ["-fopenmp"]


_omp_cxx, _omp_ld = _get_omp_flags()

# --- W4A8 quantized matmul ---
@jit(
    device="cpu",
    cpp_files=["quant_matmul.cpp"],
    cpp_wrappers=[("quant_matmul_w4a8", "mllm_kernel::cpu::quant_matmul_w4a8")],
    func_name="quant_matmul_w4a8",
    use_highway=False,
    extra_cxx_flags=_omp_cxx,
    extra_ld_flags=_omp_ld,
)
def _quant_matmul_w4a8(
    compiled_module,
    C: torch.Tensor,
    A_q8: torch.Tensor,
    B_q4: torch.Tensor,
    M: int,
    N: int,
    K: int,
) -> None:
    compiled_module.quant_matmul_w4a8(C, A_q8, B_q4, M, N, K)


# =============================================================================
# Quantization constants (must match C++ definitions)
# =============================================================================
QUANT_GROUP_SIZE = 64
BLOCK_Q4_64_SIZE = 36   # sizeof(block_q4_64): 4 (scale) + 32 (data)
BLOCK_Q8_64_SIZE = 68   # sizeof(block_q8_64): 4 (scale) + 64 (data)


def quantize_q8(tensor: torch.Tensor) -> torch.Tensor:
    """
    Quantize a float32 tensor to block_q8_64 format.

    Args:
        tensor: Float32 tensor of shape (rows, K) where K is divisible by 64.

    Returns:
        A uint8 tensor containing packed block_q8_64 data.
        Size: rows * (K / 64) * sizeof(block_q8_64) bytes.
    """
    assert tensor.dtype == torch.float32
    assert tensor.dim() == 2
    rows, K = tensor.shape
    assert K % QUANT_GROUP_SIZE == 0, f"K={K} must be divisible by {QUANT_GROUP_SIZE}"

    num_groups = K // QUANT_GROUP_SIZE
    total_blocks = rows * num_groups
    result = bytearray(total_blocks * BLOCK_Q8_64_SIZE)

    data = tensor.numpy()
    for i in range(rows):
        for g in range(num_groups):
            block_idx = i * num_groups + g
            offset = block_idx * BLOCK_Q8_64_SIZE

            group = data[i, g * QUANT_GROUP_SIZE : (g + 1) * QUANT_GROUP_SIZE]
            abs_max = np.max(np.abs(group))
            scale = abs_max / 127.0 if abs_max > 0 else 1.0

            # Write scale (float32, 4 bytes)
            struct.pack_into("f", result, offset, scale)

            # Quantize and write int8 data
            quantized = np.clip(np.round(group / scale), -128, 127).astype(np.int8)
            result[offset + 4 : offset + 4 + QUANT_GROUP_SIZE] = quantized.tobytes()

    return torch.tensor(np.frombuffer(bytes(result), dtype=np.uint8))


def quantize_q4(tensor: torch.Tensor) -> torch.Tensor:
    """
    Quantize a float32 tensor to block_q4_64 format.

    The weight matrix B has shape (K, N).
    Quantization is per-column-group: block [g][j] covers column j, rows [g*64, (g+1)*64).

    Args:
        tensor: Float32 tensor of shape (K, N) where K is divisible by 64.

    Returns:
        A uint8 tensor containing packed block_q4_64 data.
        Size: (K/64) * N * sizeof(block_q4_64) bytes.
    """
    assert tensor.dtype == torch.float32
    assert tensor.dim() == 2
    K, N = tensor.shape
    assert K % QUANT_GROUP_SIZE == 0, f"K={K} must be divisible by {QUANT_GROUP_SIZE}"

    num_groups = K // QUANT_GROUP_SIZE
    total_blocks = num_groups * N
    result = bytearray(total_blocks * BLOCK_Q4_64_SIZE)

    data = tensor.numpy()
    for g in range(num_groups):
        for j in range(N):
            block_idx = g * N + j
            offset = block_idx * BLOCK_Q4_64_SIZE

            group = data[g * QUANT_GROUP_SIZE : (g + 1) * QUANT_GROUP_SIZE, j]
            abs_max = np.max(np.abs(group))
            scale = abs_max / 7.0 if abs_max > 0 else 1.0

            # Write scale (float32, 4 bytes)
            struct.pack_into("f", result, offset, scale)

            # Quantize to int4 [-8, 7] and pack into uint8 (2 per byte)
            quantized = np.clip(np.round(group / scale), -8, 7).astype(np.int8)
            packed = bytearray(32)
            for k in range(QUANT_GROUP_SIZE):
                nibble = int(quantized[k]) & 0x0F
                if k % 2 == 0:
                    packed[k // 2] = nibble
                else:
                    packed[k // 2] |= (nibble << 4)

            result[offset + 4 : offset + 4 + 32] = packed

    return torch.tensor(np.frombuffer(bytes(result), dtype=np.uint8))


def dequantize_q8(q8_tensor: torch.Tensor, rows: int, K: int) -> torch.Tensor:
    """Dequantize block_q8_64 data back to float32 for validation."""
    num_groups = K // QUANT_GROUP_SIZE
    raw = q8_tensor.numpy().tobytes()
    result = np.zeros((rows, K), dtype=np.float32)

    for i in range(rows):
        for g in range(num_groups):
            offset = (i * num_groups + g) * BLOCK_Q8_64_SIZE
            scale = struct.unpack_from("f", raw, offset)[0]
            data = np.frombuffer(raw, dtype=np.int8, count=QUANT_GROUP_SIZE, offset=offset + 4)
            result[i, g * QUANT_GROUP_SIZE : (g + 1) * QUANT_GROUP_SIZE] = (
                data.astype(np.float32) * scale
            )

    return torch.from_numpy(result)


def dequantize_q4(q4_tensor: torch.Tensor, K: int, N: int) -> torch.Tensor:
    """Dequantize block_q4_64 data back to float32 for validation."""
    num_groups = K // QUANT_GROUP_SIZE
    raw = q4_tensor.numpy().tobytes()
    result = np.zeros((K, N), dtype=np.float32)

    for g in range(num_groups):
        for j in range(N):
            offset = (g * N + j) * BLOCK_Q4_64_SIZE
            scale = struct.unpack_from("f", raw, offset)[0]
            packed = raw[offset + 4 : offset + 4 + 32]

            for k in range(QUANT_GROUP_SIZE):
                byte_val = packed[k // 2]
                if k % 2 == 0:
                    nibble = byte_val & 0x0F
                else:
                    nibble = (byte_val >> 4) & 0x0F
                # Sign extend from 4 bits
                val = nibble - 16 if nibble >= 8 else nibble
                result[g * QUANT_GROUP_SIZE + k, j] = val * scale

    return torch.from_numpy(result)


def quant_matmul_w4a8(
    A: torch.Tensor,
    B: torch.Tensor,
) -> torch.Tensor:
    """
    W4A8 quantized matrix multiplication: C = A @ B

    Quantizes A to int8 (per-64-group symmetric) and B to int4
    (per-64-group symmetric), then computes the matmul in quantized domain.

    Args:
        A: Float32 tensor of shape (M, K), K must be divisible by 64.
        B: Float32 tensor of shape (K, N), K must be divisible by 64.

    Returns:
        Float32 tensor of shape (M, N).
    """
    if A.dtype != torch.float32 or B.dtype != torch.float32:
        raise TypeError("Expected float32 tensors")
    if A.dim() != 2 or B.dim() != 2:
        raise ValueError("Expected 2D tensors")
    if A.size(1) != B.size(0):
        raise ValueError(f"Inner dimensions must match: A({A.size(0)},{A.size(1)}) B({B.size(0)},{B.size(1)})")
    if A.size(1) % QUANT_GROUP_SIZE != 0:
        raise ValueError(f"K={A.size(1)} must be divisible by {QUANT_GROUP_SIZE}")

    M, K = A.shape
    _, N = B.shape

    if not A.is_contiguous():
        A = A.contiguous()
    if not B.is_contiguous():
        B = B.contiguous()

    # Quantize
    A_q8 = quantize_q8(A)
    B_q4 = quantize_q4(B)

    # Allocate output
    C = torch.empty(M, N, dtype=torch.float32, device="cpu")

    # Call JIT kernel
    _quant_matmul_w4a8(C, A_q8, B_q4, M, N, K)

    return C


def quant_matmul_w4a8_raw(
    C: torch.Tensor,
    A_q8: torch.Tensor,
    B_q4: torch.Tensor,
    M: int,
    N: int,
    K: int,
) -> None:
    """
    Low-level W4A8 quantized matmul with pre-quantized inputs.

    Args:
        C: Output float32 tensor of shape (M, N)
        A_q8: Pre-quantized uint8 tensor (block_q8_64 format)
        B_q4: Pre-quantized uint8 tensor (block_q4_64 format)
        M, N, K: Matrix dimensions
    """
    _quant_matmul_w4a8(C, A_q8, B_q4, M, N, K)
