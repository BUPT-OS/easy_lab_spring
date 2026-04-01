#!/usr/bin/env python3
"""
OS101 矩阵乘法实验 - W4A8 量化矩阵乘法正确性测试

测试 W4A8 quantized matmul kernel 的正确性。
由于量化会引入误差，测试使用较宽松的容差，并额外验证量化/反量化流程。

用法:
    python -m pytest tests/test_quant_matmul.py -v
    python tests/test_quant_matmul.py
"""

import sys
import pytest
import torch
import numpy as np

from mllm_kernel.cpu.jit.quant_matmul import (
    quant_matmul_w4a8,
    quant_matmul_w4a8_raw,
    quantize_q4,
    quantize_q8,
    dequantize_q4,
    dequantize_q8,
    QUANT_GROUP_SIZE,
)


# Test sizes: (M, N, K) where K must be divisible by 64
SMALL_SIZES = [
    (1, 1, 64),
    (4, 4, 64),
    (8, 8, 128),
    (16, 16, 64),
]

MEDIUM_SIZES = [
    (32, 32, 128),
    (64, 64, 256),
    (32, 64, 192),
    (128, 128, 128),
]


class TestQuantization:
    """Test quantization and dequantization routines."""

    def test_q8_roundtrip(self):
        """Quantize to q8 and dequantize; check reasonable error."""
        A = torch.randn(16, 128)
        A_q8 = quantize_q8(A)
        A_deq = dequantize_q8(A_q8, 16, 128)
        # Q8 should be fairly accurate
        max_diff = (A - A_deq).abs().max().item()
        assert max_diff < 0.1, f"Q8 roundtrip error too large: {max_diff}"

    def test_q4_roundtrip(self):
        """Quantize to q4 and dequantize; check reasonable error."""
        B = torch.randn(128, 16)
        B_q4 = quantize_q4(B)
        B_deq = dequantize_q4(B_q4, 128, 16)
        # Q4 has larger quantization error
        max_diff = (B - B_deq).abs().max().item()
        assert max_diff < 1.0, f"Q4 roundtrip error too large: {max_diff}"

    def test_q8_zero(self):
        """Zero tensor should quantize cleanly."""
        A = torch.zeros(4, 64)
        A_q8 = quantize_q8(A)
        A_deq = dequantize_q8(A_q8, 4, 64)
        assert torch.allclose(A_deq, torch.zeros_like(A_deq), atol=1e-6)

    def test_q4_constant(self):
        """Constant tensor should be preserved after roundtrip."""
        B = torch.ones(64, 4) * 3.0
        B_q4 = quantize_q4(B)
        B_deq = dequantize_q4(B_q4, 64, 4)
        # Should be reasonably close
        max_diff = (B - B_deq).abs().max().item()
        assert max_diff < 1.0, f"Q4 constant roundtrip error: {max_diff}"

    def test_q8_size(self):
        """Check that quantized tensor has correct byte size."""
        M, K = 8, 128
        A = torch.randn(M, K)
        A_q8 = quantize_q8(A)
        expected_size = M * (K // QUANT_GROUP_SIZE) * 68  # sizeof(block_q8_64)
        assert A_q8.numel() == expected_size

    def test_q4_size(self):
        """Check that quantized tensor has correct byte size."""
        K, N = 128, 8
        B = torch.randn(K, N)
        B_q4 = quantize_q4(B)
        expected_size = (K // QUANT_GROUP_SIZE) * N * 36  # sizeof(block_q4_64)
        assert B_q4.numel() == expected_size


class TestQuantMatmulW4A8:
    """Test the full W4A8 quantized matmul pipeline."""

    @pytest.mark.parametrize("M,N,K", SMALL_SIZES)
    def test_small(self, M, N, K):
        A = torch.randn(M, K) * 0.5
        B = torch.randn(K, N) * 0.5

        C_ref = torch.mm(A, B)
        C_quant = quant_matmul_w4a8(A, B)

        assert C_quant.shape == (M, N)
        # Quantization introduces error; use relative tolerance
        # The key check is that the result is in the right ballpark
        rel_error = (C_quant - C_ref).norm() / (C_ref.norm() + 1e-6)
        assert rel_error < 0.5, (
            f"Relative error too large: {rel_error:.4f}. "
            f"This likely means the kernel is computing incorrect results."
        )

    @pytest.mark.parametrize("M,N,K", MEDIUM_SIZES)
    def test_medium(self, M, N, K):
        A = torch.randn(M, K) * 0.3
        B = torch.randn(K, N) * 0.3

        C_ref = torch.mm(A, B)
        C_quant = quant_matmul_w4a8(A, B)

        rel_error = (C_quant - C_ref).norm() / (C_ref.norm() + 1e-6)
        assert rel_error < 0.5, f"Relative error: {rel_error:.4f}"

    def test_identity_like(self):
        """Multiply by near-identity (scaled eye quantizes well)."""
        N, K = 8, 64
        A = torch.eye(N, K) * 5.0
        B = torch.eye(K, N) * 5.0

        C_quant = quant_matmul_w4a8(A, B)
        C_ref = torch.mm(A, B)

        # This should be reasonably accurate due to sparse structure
        max_diff = (C_quant - C_ref).abs().max().item()
        assert max_diff < 10.0, f"Max difference: {max_diff}"

    def test_raw_api(self):
        """Test the low-level raw API with pre-quantized data."""
        M, N, K = 8, 4, 64
        A = torch.randn(M, K) * 0.5
        B = torch.randn(K, N) * 0.5

        A_q8 = quantize_q8(A)
        B_q4 = quantize_q4(B)
        C = torch.empty(M, N, dtype=torch.float32)

        quant_matmul_w4a8_raw(C, A_q8, B_q4, M, N, K)

        # Compare with full pipeline
        C2 = quant_matmul_w4a8(A, B)
        assert torch.allclose(C, C2, atol=1e-5)


class TestQuantInputValidation:
    """Input validation tests."""

    def test_k_not_divisible(self):
        A = torch.randn(4, 65)
        B = torch.randn(65, 4)
        with pytest.raises(ValueError):
            quant_matmul_w4a8(A, B)

    def test_wrong_dtype(self):
        A = torch.randn(4, 64).double()
        B = torch.randn(64, 4)
        with pytest.raises(TypeError):
            quant_matmul_w4a8(A, B)


def _run_all_tests():
    """Simple runner for quick testing without pytest."""
    print("=" * 60)
    print("OS101 W4A8 Quantized MatMul Tests")
    print("=" * 60)

    passed = 0
    failed = 0

    # Test quantization roundtrips
    for name, fn in [
        ("Q8 roundtrip", lambda: TestQuantization().test_q8_roundtrip()),
        ("Q4 roundtrip", lambda: TestQuantization().test_q4_roundtrip()),
        ("Q8 size", lambda: TestQuantization().test_q8_size()),
        ("Q4 size", lambda: TestQuantization().test_q4_size()),
    ]:
        try:
            fn()
            print(f"  [PASS] {name}")
            passed += 1
        except Exception as e:
            print(f"  [FAIL] {name}: {e}")
            failed += 1

    # Test matmul
    for M, N, K in SMALL_SIZES + MEDIUM_SIZES:
        try:
            A = torch.randn(M, K) * 0.5
            B = torch.randn(K, N) * 0.5
            C_ref = torch.mm(A, B)
            C_quant = quant_matmul_w4a8(A, B)
            rel_error = (C_quant - C_ref).norm() / (C_ref.norm() + 1e-6)
            assert rel_error < 0.5, f"rel_error={rel_error:.4f}"
            print(f"  [PASS] quant_matmul ({M}x{K}) @ ({K}x{N}), rel_error={rel_error:.4f}")
            passed += 1
        except Exception as e:
            print(f"  [FAIL] quant_matmul ({M}x{K}) @ ({K}x{N}): {e}")
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
