// Copyright (c) MLLM Team.
// Licensed under the MIT License.
//
// Matrix multiplication TVM FFI wrappers for JIT compilation.

#include <mllm_kernel/cpu/matmul.hpp>
#include <mllm_kernel/tensor.hpp>
#include <mllm_kernel/utils.hpp>
#include <tvm/ffi/container/tensor.h>

namespace mllm_kernel::cpu {

namespace detail {

inline auto prepare_matmul_args(tvm::ffi::TensorView C,
                                tvm::ffi::TensorView A,
                                tvm::ffi::TensorView B)
    -> std::tuple<float*, const float*, const float*,
                  std::size_t, std::size_t, std::size_t> {
  using namespace mllm_kernel::host;  // NOLINT

  SymbolicSize M = {"M"}, N = {"N"}, K1 = {"K"}, K2 = {"K"};

  // C: (M, N)
  (void)TensorMatcher({M, N})
      .with_dtype<float>()
      .with_device<kDLCPU>()
      .verify(C);

  // A: (M, K)
  SymbolicSize M2 = {"M"};
  (void)TensorMatcher({M2, K1})
      .with_dtype<float>()
      .with_device<kDLCPU>()
      .verify(A);

  // B: (K, N)
  SymbolicSize N2 = {"N"};
  (void)TensorMatcher({K2, N2})
      .with_dtype<float>()
      .with_device<kDLCPU>()
      .verify(B);

  const auto m = static_cast<std::size_t>(M.unwrap());
  const auto n = static_cast<std::size_t>(N.unwrap());
  const auto k = static_cast<std::size_t>(K1.unwrap());

  // Verify dimension consistency
  RuntimeCheck(M2.unwrap() == static_cast<int64_t>(m),
               "A rows (", M2.unwrap(), ") must match C rows (", m, ")");
  RuntimeCheck(K2.unwrap() == static_cast<int64_t>(k),
               "B rows (", K2.unwrap(), ") must match A cols (", k, ")");
  RuntimeCheck(N2.unwrap() == static_cast<int64_t>(n),
               "B cols (", N2.unwrap(), ") must match C cols (", n, ")");

  auto* c_ptr = static_cast<float*>(C.data_ptr());
  const auto* a_ptr = static_cast<const float*>(A.data_ptr());
  const auto* b_ptr = static_cast<const float*>(B.data_ptr());
  return {c_ptr, a_ptr, b_ptr, m, n, k};
}

}  // namespace detail

void matmul_naive(tvm::ffi::TensorView C,
                  tvm::ffi::TensorView A,
                  tvm::ffi::TensorView B) {
  auto [c, a, b, m, n, k] = detail::prepare_matmul_args(C, A, B);
  matmul_naive_impl(c, a, b, m, n, k);
}

void matmul_reorder(tvm::ffi::TensorView C,
                    tvm::ffi::TensorView A,
                    tvm::ffi::TensorView B) {
  auto [c, a, b, m, n, k] = detail::prepare_matmul_args(C, A, B);
  matmul_reorder_impl(c, a, b, m, n, k);
}

void matmul_blocked(tvm::ffi::TensorView C,
                    tvm::ffi::TensorView A,
                    tvm::ffi::TensorView B) {
  auto [c, a, b, m, n, k] = detail::prepare_matmul_args(C, A, B);
  matmul_blocked_impl(c, a, b, m, n, k);
}

void matmul_omp(tvm::ffi::TensorView C,
                tvm::ffi::TensorView A,
                tvm::ffi::TensorView B) {
  auto [c, a, b, m, n, k] = detail::prepare_matmul_args(C, A, B);
  matmul_omp_impl(c, a, b, m, n, k);
}

}  // namespace mllm_kernel::cpu
