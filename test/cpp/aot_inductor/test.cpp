#include <gtest/gtest.h>
#include <string>
#include <vector>

#include <c10/cuda/CUDAStream.h>
#include <torch/csrc/inductor/aot_runtime/interface.h>
#include <torch/torch.h>

namespace torch {
namespace aot_inductor {

struct Net : torch::nn::Module {
  Net() : linear(register_module("linear", torch::nn::Linear(64, 10))) {}

  torch::Tensor forward(torch::Tensor x, torch::Tensor y) {
    return linear(torch::sin(x) + torch::cos(y));
  }
  torch::nn::Linear linear;
};

TEST(AotInductorTest, BasicTest) {
  torch::NoGradGuard no_grad;
  Net net;
  net.to(torch::kCUDA);

  // We should fix the weight over here.
  // This should match exactly with the one in test.py
  torch::Tensor weights =
      at::arange(640, at::dtype(at::kFloat).device(at::kCUDA));
  weights = at::reshape(weights, {10, 64});
  torch::Tensor bias = at::zeros({10}, at::dtype(at::kFloat).device(at::kCUDA));

  for (const auto& pair : net.named_parameters()) {
    if (pair.key().find("weight") != std::string::npos) {
      pair.value().copy_(weights);
    } else if (pair.key().find("bias") != std::string::npos) {
      pair.value().copy_(bias);
    }
  }

  torch::Tensor x =
      at::randn({32, 64}, at::dtype(at::kFloat).device(at::kCUDA));
  torch::Tensor y =
      at::randn({32, 64}, at::dtype(at::kFloat).device(at::kCUDA));
  torch::Tensor results_ref = net.forward(x, y);

  std::vector<torch::Tensor> inputs;
  inputs.push_back(x);
  inputs.push_back(y);

  AOTInductorModelContainerHandle container_handle;
  AOT_INDUCTOR_ERROR_CHECK(
      AOTInductorModelContainerCreate(&container_handle, 1 /*num_models*/))
  AOTInductorParamShape max_output_shape;
  const int64_t* max_output_sizes;
  int64_t max_output_dim;
  AOT_INDUCTOR_ERROR_CHECK(AOTInductorModelContainerGetMaxOutputShape(
      container_handle, 0 /*output_idx*/, &max_output_sizes, &max_output_dim));

  c10::IntArrayRef array_size(max_output_sizes, max_output_dim);
  torch::Tensor output_tensor =
      at::zeros(array_size, at::dtype(at::kFloat).device(at::kCUDA));
  std::vector<torch::Tensor> outputs;
  outputs.push_back(output_tensor);

  const auto& cuda_stream = at::cuda::getCurrentCUDAStream(0 /*device_index*/);
  const auto stream_id = cuda_stream.stream();
  AOTInductorStreamHandle stream_handle =
      reinterpret_cast<AOTInductorStreamHandle>(stream_id);
  AOTInductorTensorHandle inputs_handle =
      reinterpret_cast<AOTInductorTensorHandle>(inputs.data());
  AOTInductorTensorHandle outputs_handle =
      reinterpret_cast<AOTInductorTensorHandle>(outputs.data());

  std::vector<const int64_t*> output_sizes(outputs.size());
  std::vector<int64_t> output_ndims(outputs.size());

  AOTInductorProxyExecutorHandle proxy_executor_handle = nullptr;

  AOT_INDUCTOR_ERROR_CHECK(AOTInductorModelContainerRun(
      container_handle,
      inputs_handle,
      inputs.size(),
      outputs_handle,
      outputs.size(),
      stream_handle,
      proxy_executor_handle,
      output_sizes.data(),
      output_ndims.data()));

  ASSERT_EQ(output_shapes.size(), 1);
  ASSERT_EQ(output_shapes[0].ndim, 2);
  ASSERT_EQ(output_shapes[0].shape_data[0], 32);
  ASSERT_EQ(output_shapes[0].shape_data[1], 10);
  ASSERT_TRUE(torch::allclose(results_ref, outputs[0]));
  AOT_INDUCTOR_ERROR_CHECK(AOTInductorModelContainerDelete(container_handle));
}

} // namespace aot_inductor
} // namespace torch
