// Copyright (c) MLLM Team.
// Licensed under the MIT License.
//
// W4A8 Quantized matrix multiplication TVM FFI wrapper.

#include <mllm_kernel/cpu/quant_matmul.hpp>
#include <mllm_kernel/tensor.hpp>
#include <mllm_kernel/utils.hpp>
#include <tvm/ffi/container/tensor.h>

namespace mllm_kernel::cpu {

void quant_matmul_w4a8(tvm::ffi::TensorView C,
                       tvm::ffi::TensorView A_q8,
                       tvm::ffi::TensorView B_q4,
                       int64_t M, int64_t N, int64_t K) {
  using namespace mllm_kernel::host;  // NOLINT

  // Validate output tensor C: (M, N) float32
  SymbolicSize M_dim = {"M"}, N_dim = {"N"};
  (void)TensorMatcher({M_dim, N_dim})
      .with_dtype<float>()
      .with_device<kDLCPU>()
      .verify(C);

  RuntimeCheck(M_dim.unwrap() == M, "C rows mismatch");
  RuntimeCheck(N_dim.unwrap() == N, "C cols mismatch");
  RuntimeCheck(K > 0 && K % 64 == 0, "K must be positive and divisible by 64");

  // A_q8 and B_q4 are passed as 1D uint8 tensors (raw byte buffers)
  SymbolicSize A_bytes = {"A_bytes"};
  (void)TensorMatcher({A_bytes})
      .with_dtype<uint8_t>()
      .with_device<kDLCPU>()
      .verify(A_q8);

  SymbolicSize B_bytes = {"B_bytes"};
  (void)TensorMatcher({B_bytes})
      .with_dtype<uint8_t>()
      .with_device<kDLCPU>()
      .verify(B_q4);

  const auto num_groups = static_cast<std::size_t>(K) / QUANT_GROUP_SIZE;
  const auto expected_a_bytes = static_cast<int64_t>(
      static_cast<std::size_t>(M) * num_groups * sizeof(block_q8_64));
  const auto expected_b_bytes = static_cast<int64_t>(
      num_groups * static_cast<std::size_t>(N) * sizeof(block_q4_64));

  RuntimeCheck(A_bytes.unwrap() == expected_a_bytes,
               "A_q8 size mismatch: expected ", expected_a_bytes,
               " got ", A_bytes.unwrap());
  RuntimeCheck(B_bytes.unwrap() == expected_b_bytes,
               "B_q4 size mismatch: expected ", expected_b_bytes,
               " got ", B_bytes.unwrap());

  quant_matmul_w4a8_impl(
      static_cast<float*>(C.data_ptr()),
      static_cast<const uint8_t*>(A_q8.data_ptr()),
      static_cast<const uint8_t*>(B_q4.data_ptr()),
      static_cast<std::size_t>(M),
      static_cast<std::size_t>(N),
      static_cast<std::size_t>(K));
}

}  // namespace mllm_kernel::cpu
