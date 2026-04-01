// Copyright (c) MLLM Team.
// Licensed under the MIT License.
//
// W4A8 Quantized matrix multiplication kernel for OS101 lab.
// Weights: Per-64-Group symmetric quantized int4
// Activations: Per-64-Group symmetric quantized int8
//
// ====================================================================
// 学生需要实现本文件中标注的所有填写区域。
// 请仔细阅读量化数据结构的定义和函数说明。
// ====================================================================

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace mllm_kernel::cpu {

// =============================================================================
// 量化块定义 (GGUF 风格)
// =============================================================================

/// 量化组大小
inline constexpr std::size_t QUANT_GROUP_SIZE = 64;

/// Int4 权重量化块：64 个元素一组
///
/// 对称量化：real_value = scale * int4_value
/// int4 范围：[-8, 7]
///
/// 数据打包方式：每个 uint8 存储 2 个 int4 值
///   - 低 4 位 (nibble) 存偶数位置的值
///   - 高 4 位 (nibble) 存奇数位置的值
///
/// 例如：data[0] 的低 4 位是第 0 个值，高 4 位是第 1 个值
struct block_q4_64 {
  float scale;           ///< 对称量化缩放因子
  uint8_t data[32];      ///< 64 个 4-bit 有符号整数，打包为 32 字节
};
static_assert(sizeof(block_q4_64) == 36, "block_q4_64 must be 36 bytes");

/// Int8 激活量化块：64 个元素一组
///
/// 对称量化：real_value = scale * int8_value
/// int8 范围：[-128, 127]
struct block_q8_64 {
  float scale;           ///< 对称量化缩放因子
  int8_t data[64];       ///< 64 个 8-bit 有符号整数
};
static_assert(sizeof(block_q8_64) == 68, "block_q8_64 must be 68 bytes");

// =============================================================================
// Task A1-a: 从打包数据中提取 int4 值
//
// 打包格式：每个 uint8 存储两个 int4 值
//   - idx 为偶数：取 byte 的低 4 位 (byte & 0x0F)
//   - idx 为奇数：取 byte 的高 4 位 ((byte >> 4) & 0x0F)
//
// 符号扩展：4-bit 二进制补码范围是 [-8, 7]
//   - 如果提取的 nibble >= 8，则实际值 = nibble - 16
//     例如：0xF (=15) 表示 -1，0x8 (=8) 表示 -8
//   - 如果 nibble < 8，则实际值 = nibble
//     例如：0x0 (=0) 表示 0，0x7 (=7) 表示 7
// =============================================================================
inline int8_t extract_q4(const uint8_t* data, std::size_t idx) {
  // ==================== 学生填写区域 开始 ====================
  // 提示：
  // 1. 获取包含目标值的字节：uint8_t byte = data[idx / 2]
  // 2. 根据 idx 的奇偶性提取 nibble：
  //    - 偶数：nibble = byte & 0x0F
  //    - 奇数：nibble = (byte >> 4) & 0x0F
  // 3. 符号扩展：if (nibble >= 8) return nibble - 16; else return nibble;

  // （在此处填写你的代码）
  return 0;  // placeholder

  // ==================== 学生填写区域 结束 ====================
}

/// 将一个 int4 值打包写入数据（供测试使用，已实现）
inline void pack_q4(uint8_t* data, std::size_t idx, int8_t val) {
  uint8_t nibble = static_cast<uint8_t>(val & 0x0F);
  if (idx % 2 == 0) {
    data[idx / 2] = (data[idx / 2] & 0xF0) | nibble;
  } else {
    data[idx / 2] = (data[idx / 2] & 0x0F) | (nibble << 4);
  }
}

// =============================================================================
// Task A1-b (Advanced): W4A8 量化矩阵乘法
//
// 计算 C(M, N) = dequant(A_q8) * dequant(B_q4)
//
// 数据布局：
//   A_q8: M * (K/64) 个 block_q8_64 块，行主序
//         第 [i][g] 个块覆盖第 i 行、第 [g*64, (g+1)*64) 列
//   B_q4: (K/64) * N 个 block_q4_64 块
//         第 [g][j] 个块覆盖第 j 列、第 [g*64, (g+1)*64) 行
//
// 计算公式（对于每个输出元素 C[i][j]）：
//   C[i][j] = sum_{g=0}^{num_groups-1} (
//     A_blocks[i][g].scale * B_blocks[g][j].scale *
//     sum_{k=0}^{63} A_blocks[i][g].data[k] * extract_q4(B_blocks[g][j].data, k)
//   )
//
// 实现步骤：
//   1. 将 raw uint8 指针转换为对应 block 结构体指针
//   2. 三重循环遍历 (i, j, g)
//   3. 对每个 group，计算 int32 点积，再乘以两个 scale
//   4. 使用 OpenMP 并行化外层 i 循环
// =============================================================================
inline void quant_matmul_w4a8_impl(
    float* __restrict__ C,
    const uint8_t* __restrict__ A_q8_raw,  // M * num_groups 个 block_q8_64
    const uint8_t* __restrict__ B_q4_raw,  // num_groups * N 个 block_q4_64
    std::size_t M, std::size_t N, std::size_t K) {
  // ==================== 学生填写区域 开始 ====================
  // 提示：
  // 1. 计算 group 数量：num_groups = K / QUANT_GROUP_SIZE
  // 2. 将 raw 指针转换为 block 指针：
  //    const auto* A_blocks = reinterpret_cast<const block_q8_64*>(A_q8_raw);
  //    const auto* B_blocks = reinterpret_cast<const block_q4_64*>(B_q4_raw);
  // 3. 添加 OpenMP 并行化：#pragma omp parallel for schedule(dynamic)
  // 4. 遍历输出矩阵的每个元素 (i, j)：
  //    float sum = 0.0f;
  //    for each group g:
  //      a. 获取 A 块和 B 块：
  //         const auto& a_blk = A_blocks[i * num_groups + g];
  //         const auto& b_blk = B_blocks[g * N + j];
  //      b. 计算组合缩放因子：group_scale = a_blk.scale * b_blk.scale
  //      c. 计算 int32 点积：
  //         int32_t dot = 0;
  //         for k in [0, 64):
  //           dot += (int32_t)a_blk.data[k] * (int32_t)extract_q4(b_blk.data, k)
  //      d. 累加：sum += group_scale * (float)dot
  //    C[i * N + j] = sum;

  // （在此处填写你的代码）

  // ==================== 学生填写区域 结束 ====================
}

}  // namespace mllm_kernel::cpu
