# Copyright (c) MLLM Team.
# Licensed under the MIT License.
#
# Matrix multiplication JIT kernels for OS101 lab.

from __future__ import annotations

import platform
import sys

import torch

from mllm_kernel.jit_utils import jit


def _get_omp_flags() -> tuple[list[str], list[str]]:
    """Get OpenMP compiler and linker flags for the current platform."""
    system = platform.system()
    if system == "Linux":
        return ["-fopenmp"], ["-fopenmp"]
    elif system == "Darwin":
        # macOS: Apple Clang needs -Xpreprocessor -fopenmp and -lomp
        return ["-Xpreprocessor", "-fopenmp"], ["-lomp"]
    else:
        # Fallback
        return ["-fopenmp"], ["-fopenmp"]


_omp_cxx, _omp_ld = _get_omp_flags()


# --- Naive matmul (no OpenMP, no Highway) ---
@jit(
    device="cpu",
    cpp_files=["matmul.cpp"],
    cpp_wrappers=[("matmul_naive", "mllm_kernel::cpu::matmul_naive")],
    func_name="matmul_naive",
    use_highway=False,
)
def _matmul_naive(compiled_module, C: torch.Tensor, A: torch.Tensor, B: torch.Tensor) -> None:
    compiled_module.matmul_naive(C, A, B)


# --- Loop-reorder matmul (no OpenMP, no Highway) ---
@jit(
    device="cpu",
    cpp_files=["matmul.cpp"],
    cpp_wrappers=[("matmul_reorder", "mllm_kernel::cpu::matmul_reorder")],
    func_name="matmul_reorder",
    use_highway=False,
)
def _matmul_reorder(compiled_module, C: torch.Tensor, A: torch.Tensor, B: torch.Tensor) -> None:
    compiled_module.matmul_reorder(C, A, B)


# --- Blocked matmul (no OpenMP, no Highway) ---
@jit(
    device="cpu",
    cpp_files=["matmul.cpp"],
    cpp_wrappers=[("matmul_blocked", "mllm_kernel::cpu::matmul_blocked")],
    func_name="matmul_blocked",
    use_highway=False,
)
def _matmul_blocked(compiled_module, C: torch.Tensor, A: torch.Tensor, B: torch.Tensor) -> None:
    compiled_module.matmul_blocked(C, A, B)


# --- OpenMP parallel blocked matmul ---
@jit(
    device="cpu",
    cpp_files=["matmul.cpp"],
    cpp_wrappers=[("matmul_omp", "mllm_kernel::cpu::matmul_omp")],
    func_name="matmul_omp",
    use_highway=False,
    extra_cxx_flags=_omp_cxx,
    extra_ld_flags=_omp_ld,
)
def _matmul_omp(compiled_module, C: torch.Tensor, A: torch.Tensor, B: torch.Tensor) -> None:
    compiled_module.matmul_omp(C, A, B)


def _validate_matmul_inputs(A: torch.Tensor, B: torch.Tensor) -> torch.Tensor:
    """Validate inputs and allocate output tensor."""
    if A.dtype != torch.float32:
        raise TypeError(f"Expected float32 tensor for A, got {A.dtype}")
    if B.dtype != torch.float32:
        raise TypeError(f"Expected float32 tensor for B, got {B.dtype}")
    if A.dim() != 2 or B.dim() != 2:
        raise ValueError(f"Expected 2D tensors, got A.dim()={A.dim()}, B.dim()={B.dim()}")
    if A.size(1) != B.size(0):
        raise ValueError(
            f"Inner dimensions must match: A is {A.size(0)}x{A.size(1)}, "
            f"B is {B.size(0)}x{B.size(1)}"
        )
    if not A.is_contiguous():
        A = A.contiguous()
    if not B.is_contiguous():
        B = B.contiguous()
    C = torch.empty(A.size(0), B.size(1), dtype=torch.float32, device="cpu")
    return C


def matmul_naive(A: torch.Tensor, B: torch.Tensor) -> torch.Tensor:
    """Naive matrix multiplication: C = A @ B using triple nested loop (ijk)."""
    C = _validate_matmul_inputs(A, B)
    _matmul_naive(C, A, B)
    return C


def matmul_reorder(A: torch.Tensor, B: torch.Tensor) -> torch.Tensor:
    """Cache-friendly matrix multiplication with loop reorder (ikj)."""
    C = _validate_matmul_inputs(A, B)
    _matmul_reorder(C, A, B)
    return C


def matmul_blocked(A: torch.Tensor, B: torch.Tensor) -> torch.Tensor:
    """Blocked (tiled) matrix multiplication for better cache utilization."""
    C = _validate_matmul_inputs(A, B)
    _matmul_blocked(C, A, B)
    return C


def matmul_omp(A: torch.Tensor, B: torch.Tensor) -> torch.Tensor:
    """OpenMP parallel blocked matrix multiplication."""
    C = _validate_matmul_inputs(A, B)
    _matmul_omp(C, A, B)
    return C
