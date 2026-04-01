// Copyright (c) MLLM Team.
// Licensed under the MIT License.
//
// Matrix multiplication kernels for OS101 lab.
// Implementations: naive, loop-reorder, blocked, OpenMP parallel.
//
// ====================================================================
// 学生需要实现本文件中标注的所有填写区域。
// 请仔细阅读每个任务的注释说明和提示。
// ====================================================================

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstring>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace mllm_kernel::cpu {

// =============================================================================
// Task 1: Naive matrix multiplication (朴素矩阵乘法)
//
// 计算 C(M,N) = A(M,K) * B(K,N)，所有矩阵为 row-major 存储。
//
// Row-major 存储意味着：
//   A[i][k] 存储在 A[i * K + k]
//   B[k][j] 存储在 B[k * N + j]
//   C[i][j] 存储在 C[i * N + j]
//
// 矩阵乘法公式：
//   C[i][j] = sum_{k=0}^{K-1} A[i][k] * B[k][j]
//
// 提示：使用三重嵌套循环 (i, j, k) 实现。
// =============================================================================
inline void matmul_naive_impl(float* __restrict__ C,
                              const float* __restrict__ A,
                              const float* __restrict__ B,
                              std::size_t M, std::size_t N, std::size_t K) {
  // ==================== 学生填写区域 开始 ====================
  // 提示：
  // 1. 外层循环遍历 C 的行 i (0 到 M-1)
  // 2. 中间循环遍历 C 的列 j (0 到 N-1)
  // 3. 内层循环遍历 k (0 到 K-1)，累加 A[i][k] * B[k][j]
  // 4. 将累加结果写入 C[i * N + j]

  // （在此处填写你的代码）

  // ==================== 学生填写区域 结束 ====================
}

// =============================================================================
// Task 2: Cache-friendly matrix multiplication (缓存友好的矩阵乘法)
//
// 通过循环重排 (ikj 顺序) 来提高缓存命中率。
//
// 分析 naive 版本的问题：
//   在内层循环 k 中，B[k * N + j] 的 k 在变化，导致 B 按列访问，
//   每次访问跳跃 N 个元素，缓存不友好。
//
// ikj 重排后：
//   内层循环 j 中，B[k * N + j] 的 j 在变化，B 按行连续访问，
//   C[i * N + j] 也按行连续访问，两者都是缓存友好的。
//
// 注意：由于 C 是累加的，需要先用 memset 将 C 清零。
//
// 提示：
//   for i: for k: a_ik = A[i*K+k]; for j: C[i*N+j] += a_ik * B[k*N+j]
// =============================================================================
inline void matmul_reorder_impl(float* __restrict__ C,
                                const float* __restrict__ A,
                                const float* __restrict__ B,
                                std::size_t M, std::size_t N, std::size_t K) {
  // ==================== 学生填写区域 开始 ====================
  // 提示：
  // 1. 先用 std::memset(C, 0, M * N * sizeof(float)) 将 C 清零
  // 2. 外层循环 i (行)
  // 3. 中间循环 k
  // 4. 提取 a_ik = A[i * K + k] 到局部变量，避免重复访存
  // 5. 内层循环 j：C[i * N + j] += a_ik * B[k * N + j]

  // （在此处填写你的代码）

  // ==================== 学生填写区域 结束 ====================
}

// =============================================================================
// Task 3: Blocked (tiled) matrix multiplication (分块矩阵乘法)
//
// 将矩阵分成 BLOCK x BLOCK 大小的子块，使得每个子块能放进 L1/L2 缓存。
//
// 思路：
//   外层三重循环按 BLOCK 步长遍历 (i0, k0, j0)
//   内层三重循环在子块内遍历 (i, k, j)
//
// 需要用 std::min 处理边界情况（矩阵大小不是 BLOCK 整数倍）。
//
// 建议 BLOCK 大小：64（典型 L1 缓存行为 64B，64x64 float 子块 = 16KB）
// =============================================================================
inline void matmul_blocked_impl(float* __restrict__ C,
                                const float* __restrict__ A,
                                const float* __restrict__ B,
                                std::size_t M, std::size_t N, std::size_t K) {
  constexpr std::size_t BLOCK = 64;

  // ==================== 学生填写区域 开始 ====================
  // 提示：
  // 1. 先用 std::memset(C, 0, M * N * sizeof(float)) 将 C 清零
  // 2. 外层分块循环：for i0 = 0; i0 < M; i0 += BLOCK
  // 3. 中间分块循环：for k0 = 0; k0 < K; k0 += BLOCK
  // 4. 内层分块循环：for j0 = 0; j0 < N; j0 += BLOCK
  // 5. 计算块边界：i_end = min(i0 + BLOCK, M)，类似处理 k_end, j_end
  // 6. 在块内使用 ikj 顺序循环：
  //    for i in [i0, i_end):
  //      for k in [k0, k_end):
  //        a_ik = A[i * K + k]
  //        for j in [j0, j_end):
  //          C[i * N + j] += a_ik * B[k * N + j]

  // （在此处填写你的代码）

  // ==================== 学生填写区域 结束 ====================
}

// =============================================================================
// Task 4: OpenMP parallel blocked matrix multiplication (OpenMP 并行分块矩阵乘法)
//
// 在分块矩阵乘法的基础上，使用 OpenMP 并行化外层循环。
//
// 关键点：
//   - 按行块 (i0) 并行化，不同线程处理不同行块，无写冲突
//   - 使用 #pragma omp parallel for schedule(dynamic)
//   - k0 循环不能并行化（不同 k0 会写同一个 C[i][j]，存在竞争）
//
// 注意：需要在编译时链接 OpenMP (-fopenmp 或 -lomp)
// =============================================================================
inline void matmul_omp_impl(float* __restrict__ C,
                            const float* __restrict__ A,
                            const float* __restrict__ B,
                            std::size_t M, std::size_t N, std::size_t K) {
  constexpr std::size_t BLOCK = 64;

  // ==================== 学生填写区域 开始 ====================
  // 提示：
  // 1. 先用 std::memset(C, 0, M * N * sizeof(float)) 将 C 清零
  // 2. 在外层 i0 循环前添加：#pragma omp parallel for schedule(dynamic)
  // 3. 外层分块循环 i0：并行化此循环
  // 4. 内部与 matmul_blocked_impl 相同：嵌套 k0, j0 分块循环
  // 5. 块内使用 ikj 顺序遍历
  //
  // 思考：为什么只并行化 i0 而不是 j0 或 k0？
  //   - i0 并行化：不同线程写 C 的不同行，无竞争
  //   - k0 并行化：不同线程写 C 的相同位置，有数据竞争
  //   - j0 可以并行化，但通常 i0 并行已经足够

  // （在此处填写你的代码）

  // ==================== 学生填写区域 结束 ====================
}

}  // namespace mllm_kernel::cpu
