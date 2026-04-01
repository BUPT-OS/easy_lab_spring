#!/usr/bin/env python3
"""
OS101 矩阵乘法实验 - 性能基准测试

对比不同 matmul 实现的性能：
  1. torch.mm (PyTorch 参考实现)
  2. matmul_naive (朴素三重循环)
  3. matmul_reorder (循环重排)
  4. matmul_blocked (分块/分片)
  5. matmul_omp (OpenMP 并行分块)
  6. quant_matmul_w4a8 (W4A8 量化矩阵乘法) [可选]

用法:
    python benchmarks/bench_matmul.py [--sizes S1,S2,...] [--iters N] [--warmup N] [--no-quant]

示例:
    python benchmarks/bench_matmul.py
    python benchmarks/bench_matmul.py --sizes 128,256,512,1024 --iters 10
    python benchmarks/bench_matmul.py --sizes 256 --no-quant
"""

import argparse
import sys
import time

import torch

from mllm_kernel.cpu.jit.matmul import (
    matmul_naive,
    matmul_reorder,
    matmul_blocked,
    matmul_omp,
)


def bench_one(fn, A, B, warmup=3, iters=5):
    """Benchmark a single matmul function, return median time in ms."""
    # Warmup
    for _ in range(warmup):
        fn(A, B)

    times = []
    for _ in range(iters):
        start = time.perf_counter()
        fn(A, B)
        elapsed = (time.perf_counter() - start) * 1000  # ms
        times.append(elapsed)

    times.sort()
    return times[len(times) // 2]  # median


def bench_torch_mm(A, B, warmup=3, iters=5):
    """Benchmark PyTorch's built-in matmul."""
    for _ in range(warmup):
        torch.mm(A, B)

    times = []
    for _ in range(iters):
        start = time.perf_counter()
        torch.mm(A, B)
        elapsed = (time.perf_counter() - start) * 1000
        times.append(elapsed)

    times.sort()
    return times[len(times) // 2]


def run_benchmark(sizes, warmup, iters, include_quant):
    """Run the full benchmark suite."""
    kernels = [
        ("torch.mm", lambda A, B: torch.mm(A, B)),
        ("naive", matmul_naive),
        ("reorder", matmul_reorder),
        ("blocked", matmul_blocked),
        ("omp", matmul_omp),
    ]

    if include_quant:
        try:
            from mllm_kernel.cpu.jit.quant_matmul import quant_matmul_w4a8
            kernels.append(("w4a8_quant", quant_matmul_w4a8))
        except ImportError:
            print("Warning: quant_matmul not available, skipping")
            include_quant = False

    # Print header
    print("=" * 80)
    print("OS101 MatMul Benchmark")
    print(f"  Warmup: {warmup}, Iterations: {iters}")
    print(f"  Sizes: {sizes}")
    print("=" * 80)
    print()

    # Column headers
    header = f"{'Size':>12s}"
    for name, _ in kernels:
        header += f" | {name:>12s}"
    print(header)
    print("-" * len(header))

    for N in sizes:
        A = torch.randn(N, N, dtype=torch.float32)
        B = torch.randn(N, N, dtype=torch.float32)

        row = f"{f'{N}x{N}':>12s}"
        ref_time = None

        for name, fn in kernels:
            # Skip naive for large sizes (too slow)
            if name == "naive" and N > 512:
                row += f" | {'skip':>12s}"
                continue

            # For quant, K must be divisible by 64
            if name == "w4a8_quant" and N % 64 != 0:
                row += f" | {'N/A':>12s}"
                continue

            try:
                t = bench_one(fn, A, B, warmup=warmup, iters=iters)
                if ref_time is None:
                    ref_time = t
                    row += f" | {t:>9.2f} ms"
                else:
                    speedup = ref_time / t if t > 0 else float("inf")
                    row += f" | {t:>6.2f}({speedup:>4.1f}x)"
            except Exception as e:
                row += f" | {'err':>12s}"
                print(f"  Warning: {name} failed for N={N}: {e}", file=sys.stderr)

        print(row)

    print()

    # GFLOPS analysis
    print("=" * 80)
    print("GFLOPS Analysis (2*M*N*K / time)")
    print("=" * 80)
    header2 = f"{'Size':>12s}"
    for name, _ in kernels:
        header2 += f" | {name:>12s}"
    print(header2)
    print("-" * len(header2))

    for N in sizes:
        A = torch.randn(N, N, dtype=torch.float32)
        B = torch.randn(N, N, dtype=torch.float32)
        flops = 2.0 * N * N * N

        row = f"{f'{N}x{N}':>12s}"
        for name, fn in kernels:
            if name == "naive" and N > 512:
                row += f" | {'skip':>12s}"
                continue
            if name == "w4a8_quant" and N % 64 != 0:
                row += f" | {'N/A':>12s}"
                continue

            try:
                t_ms = bench_one(fn, A, B, warmup=warmup, iters=iters)
                gflops = flops / (t_ms * 1e-3) / 1e9
                row += f" | {gflops:>9.2f} GF"
            except Exception:
                row += f" | {'err':>12s}"

        print(row)

    print()


def main():
    parser = argparse.ArgumentParser(description="OS101 MatMul Benchmark")
    parser.add_argument(
        "--sizes",
        type=str,
        default="64,128,256,512,1024",
        help="Comma-separated list of matrix sizes (default: 64,128,256,512,1024)",
    )
    parser.add_argument(
        "--warmup",
        type=int,
        default=3,
        help="Number of warmup iterations (default: 3)",
    )
    parser.add_argument(
        "--iters",
        type=int,
        default=5,
        help="Number of benchmark iterations (default: 5)",
    )
    parser.add_argument(
        "--no-quant",
        action="store_true",
        help="Skip the W4A8 quantized matmul benchmark",
    )

    args = parser.parse_args()
    sizes = [int(s.strip()) for s in args.sizes.split(",")]

    run_benchmark(sizes, args.warmup, args.iters, include_quant=not args.no_quant)


if __name__ == "__main__":
    main()
