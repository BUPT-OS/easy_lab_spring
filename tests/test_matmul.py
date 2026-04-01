#!/usr/bin/env python3
"""
OS101 矩阵乘法实验 - 正确性测试

测试所有 matmul kernel 实现的正确性，以 PyTorch 的 torch.mm 作为参考。

用法:
    python -m pytest tests/test_matmul.py -v
    python tests/test_matmul.py
"""

import sys
import pytest
import torch

from mllm_kernel.cpu.jit.matmul import (
    matmul_naive,
    matmul_reorder,
    matmul_blocked,
    matmul_omp,
)

# Test matrix sizes: (M, N, K)
SMALL_SIZES = [
    (1, 1, 1),
    (2, 3, 4),
    (4, 4, 4),
    (8, 8, 8),
    (16, 16, 16),
]

MEDIUM_SIZES = [
    (32, 32, 32),
    (64, 64, 64),
    (128, 128, 128),
    (63, 65, 67),       # Non-power-of-2
    (100, 200, 150),     # Rectangular
]

LARGE_SIZES = [
    (256, 256, 256),
    (512, 512, 512),
]

ALL_SIZES = SMALL_SIZES + MEDIUM_SIZES

KERNELS = {
    "naive": matmul_naive,
    "reorder": matmul_reorder,
    "blocked": matmul_blocked,
    "omp": matmul_omp,
}


def _check_matmul(kernel_fn, M, N, K, atol=1e-4, rtol=1e-4):
    """Check that kernel_fn(A, B) matches torch.mm(A, B)."""
    A = torch.randn(M, K, dtype=torch.float32)
    B = torch.randn(K, N, dtype=torch.float32)

    C_ref = torch.mm(A, B)
    C_test = kernel_fn(A, B)

    assert C_test.shape == (M, N), f"Shape mismatch: {C_test.shape} vs {(M, N)}"
    if not torch.allclose(C_test, C_ref, atol=atol, rtol=rtol):
        max_diff = (C_test - C_ref).abs().max().item()
        pytest.fail(
            f"Results differ! max_diff={max_diff:.6f} "
            f"(atol={atol}, rtol={rtol})"
        )


class TestMatmulNaive:
    """Task 1: Naive matmul tests."""

    @pytest.mark.parametrize("M,N,K", SMALL_SIZES)
    def test_small(self, M, N, K):
        _check_matmul(matmul_naive, M, N, K)

    @pytest.mark.parametrize("M,N,K", MEDIUM_SIZES)
    def test_medium(self, M, N, K):
        _check_matmul(matmul_naive, M, N, K)

    def test_identity(self):
        """Multiply by identity matrix."""
        N = 32
        A = torch.randn(N, N)
        I = torch.eye(N)
        C = matmul_naive(A, I)
        assert torch.allclose(C, A, atol=1e-5)

    def test_zero(self):
        """Multiply by zero matrix."""
        C = matmul_naive(torch.randn(16, 32), torch.zeros(32, 8))
        assert torch.allclose(C, torch.zeros(16, 8), atol=1e-6)


class TestMatmulReorder:
    """Task 2: Loop-reorder matmul tests."""

    @pytest.mark.parametrize("M,N,K", ALL_SIZES)
    def test_correctness(self, M, N, K):
        _check_matmul(matmul_reorder, M, N, K)


class TestMatmulBlocked:
    """Task 3: Blocked matmul tests."""

    @pytest.mark.parametrize("M,N,K", ALL_SIZES)
    def test_correctness(self, M, N, K):
        _check_matmul(matmul_blocked, M, N, K)

    def test_non_block_aligned(self):
        """Matrix sizes that are not multiples of tile size (64)."""
        _check_matmul(matmul_blocked, 65, 33, 97)

    def test_smaller_than_block(self):
        """Matrix smaller than tile size."""
        _check_matmul(matmul_blocked, 3, 5, 7)


class TestMatmulOMP:
    """Task 4: OpenMP parallel matmul tests."""

    @pytest.mark.parametrize("M,N,K", ALL_SIZES)
    def test_correctness(self, M, N, K):
        _check_matmul(matmul_omp, M, N, K)

    @pytest.mark.parametrize("M,N,K", LARGE_SIZES)
    def test_large(self, M, N, K):
        _check_matmul(matmul_omp, M, N, K)

    def test_deterministic(self):
        """Multiple runs should give identical results."""
        A = torch.randn(128, 128)
        B = torch.randn(128, 128)
        C1 = matmul_omp(A, B)
        C2 = matmul_omp(A, B)
        assert torch.equal(C1, C2), "Results should be deterministic"


class TestInputValidation:
    """Input validation tests (applied to all kernels)."""

    def test_wrong_dtype(self):
        A = torch.randn(4, 4).double()
        B = torch.randn(4, 4)
        with pytest.raises(TypeError):
            matmul_naive(A, B)

    def test_dimension_mismatch(self):
        A = torch.randn(4, 5)
        B = torch.randn(3, 4)  # inner dims don't match
        with pytest.raises(ValueError):
            matmul_naive(A, B)

    def test_not_2d(self):
        A = torch.randn(4)
        B = torch.randn(4, 4)
        with pytest.raises(ValueError):
            matmul_naive(A, B)


def _run_all_tests():
    """Simple runner for quick testing without pytest."""
    print("=" * 60)
    print("OS101 MatMul Correctness Tests")
    print("=" * 60)

    passed = 0
    failed = 0

    for name, kernel_fn in KERNELS.items():
        for M, N, K in ALL_SIZES:
            try:
                _check_matmul(kernel_fn, M, N, K)
                print(f"  [PASS] {name} ({M}x{K}) @ ({K}x{N})")
                passed += 1
            except Exception as e:
                print(f"  [FAIL] {name} ({M}x{K}) @ ({K}x{N}): {e}")
                failed += 1

    print(f"\n{'=' * 60}")
    print(f"Results: {passed} passed, {failed} failed")
    if failed == 0:
        print("[SUCCESS] All tests passed!")
    else:
        print("[FAILURE] Some tests failed!")
        sys.exit(1)


if __name__ == "__main__":
    _run_all_tests()
