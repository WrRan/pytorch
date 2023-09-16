#include <ATen/core/dispatch/Dispatcher.h>
#include <torch/csrc/inductor/aot_runtime/interface.h>
#include <torch/csrc/inductor/aot_runtime/model_container.h>
#include <torch/csrc/inductor/aot_runtime/proxy_executor.h>

#include <iostream>
#include <stdexcept>
#include <vector>

#define CONVERT_EXCEPTION_TO_ERROR_CODE(...)                 \
  try {                                                      \
    __VA_ARGS__                                              \
  } catch (const std::exception& e) {                        \
    std::cerr << "Error: " << e.what() << std::endl;         \
    return AOTI_RUNTIME_FAILURE;                             \
  } catch (...) {                                            \
    std::cerr << "Unknown exception occurred." << std::endl; \
    return AOTI_RUNTIME_FAILURE;                             \
  }                                                          \
  return AOTI_RUNTIME_SUCCESS;

extern "C" {

AOTInductorError AOTInductorModelContainerCreate(
    AOTInductorModelContainerHandle* container_handle,
    size_t num_models,
    bool is_cpu) {
  if (num_models == 0) {
    LOG(ERROR) << "num_models must be positive, but got 0";
    return AOTI_RUNTIME_FAILURE;
  }
  CONVERT_EXCEPTION_TO_ERROR_CODE({
    auto* container =
        new torch::aot_inductor::AOTInductorModelContainer(num_models, is_cpu);
    *container_handle =
        reinterpret_cast<AOTInductorModelContainerHandle>(container);
  })
}

AOTInductorError AOTInductorModelContainerDelete(
    AOTInductorModelContainerHandle container_handle) {
  CONVERT_EXCEPTION_TO_ERROR_CODE({
    auto* container =
        reinterpret_cast<torch::aot_inductor::AOTInductorModelContainer*>(
            container_handle);
    delete container;
  });
}

AOTInductorError AOTInductorModelContainerRun(
    AOTInductorModelContainerHandle container_handle,
    AOTInductorTensorHandle inputs_handle,
    size_t num_inputs,
    AOTInductorTensorHandle outputs_handle,
    size_t num_outputs,
    AOTInductorStreamHandle stream_handle,
    AOTInductorProxyExecutorHandle proxy_executor_handle,
    const int64_t** ret_output_sizes,
    int64_t* ret_output_ndims) {
  auto* container =
      reinterpret_cast<torch::aot_inductor::AOTInductorModelContainer*>(
          container_handle);

  const auto* inputs = reinterpret_cast<const at::Tensor*>(inputs_handle);
  std::vector<at::Tensor> input_tensors;
  input_tensors.reserve(num_inputs);
  for (size_t i = 0; i < num_inputs; i++) {
    input_tensors.push_back(inputs[i]);
  }

  auto* outputs = reinterpret_cast<at::Tensor*>(outputs_handle);
  std::vector<at::Tensor> output_tensors;
  output_tensors.reserve(num_outputs);
  for (size_t i = 0; i < num_outputs; i++) {
    output_tensors.push_back(outputs[i]);
  }

  auto stream = reinterpret_cast<cudaStream_t>(stream_handle);

  torch::aot_inductor::ProxyExecutor* proxy_executor =
      reinterpret_cast<torch::aot_inductor::ProxyExecutor*>(
          proxy_executor_handle);

  CONVERT_EXCEPTION_TO_ERROR_CODE({
    std::vector<std::vector<int64_t>>* shapes;
    container->run(input_tensors, output_tensors, &shapes, stream, proxy_executor);
    for (size_t i = 0; i < num_outputs; i++) {
      ret_output_sizes[i] = shapes->at(i).data();
      ret_output_ndims[i] = shapes->at(i).size();
    }
  })
}

AOTInductorError AOTInductorModelContainerGetNumInputs(
    AOTInductorModelContainerHandle container_handle,
    size_t* ret_num_inputs) {
  auto* container =
      reinterpret_cast<torch::aot_inductor::AOTInductorModelContainer*>(
          container_handle);
  CONVERT_EXCEPTION_TO_ERROR_CODE(
      { *ret_num_inputs = container->num_inputs(); })
}

AOTInductorError AOTInductorModelContainerGetInputName(
    AOTInductorModelContainerHandle container_handle,
    size_t input_idx,
    const char** ret_input_names) {
  auto* container =
      reinterpret_cast<torch::aot_inductor::AOTInductorModelContainer*>(
          container_handle);
  CONVERT_EXCEPTION_TO_ERROR_CODE(
      { *ret_input_names = container->input_name(input_idx); })
}

AOTInductorError AOTInductorModelContainerGetInputDtype(
    AOTInductorModelContainerHandle container_handle,
    size_t input_idx,
    const char** ret_input_dtypes) {
  auto* container =
      reinterpret_cast<torch::aot_inductor::AOTInductorModelContainer*>(
          container_handle);
  CONVERT_EXCEPTION_TO_ERROR_CODE(
      { *ret_input_dtypes = container->get_input_dtype(input_idx); })
}

AOTInductorError AOTInductorModelContainerGetNumOutputs(
    AOTInductorModelContainerHandle container_handle,
    size_t* ret_num_outputs) {
  auto* container =
      reinterpret_cast<torch::aot_inductor::AOTInductorModelContainer*>(
          container_handle);
  CONVERT_EXCEPTION_TO_ERROR_CODE(
      { *ret_num_outputs = container->num_outputs(); })
}

AOTInductorError AOTInductorModelContainerGetOutputName(
    AOTInductorModelContainerHandle container_handle,
    size_t output_idx,
    const char** ret_output_names) {
  auto* container =
      reinterpret_cast<torch::aot_inductor::AOTInductorModelContainer*>(
          container_handle);
  CONVERT_EXCEPTION_TO_ERROR_CODE(
      { *ret_output_names = container->output_name(output_idx); })
}

AOTInductorError AOTInductorModelContainerGetOutputDtype(
    AOTInductorModelContainerHandle container_handle,
    size_t output_idx,
    const char** ret_output_dtypes) {
  auto* container =
      reinterpret_cast<torch::aot_inductor::AOTInductorModelContainer*>(
          container_handle);
  CONVERT_EXCEPTION_TO_ERROR_CODE(
      { *ret_output_dtypes = container->get_output_dtype(output_idx); })
}

AOTInductorError AOTInductorModelContainerGetMaxInputShape(
    AOTInductorModelContainerHandle container_handle,
    size_t input_idx,
    const int64_t** ret_input_sizes,
    int64_t* ret_input_ndim) {
  auto* container =
      reinterpret_cast<torch::aot_inductor::AOTInductorModelContainer*>(
          container_handle);
  CONVERT_EXCEPTION_TO_ERROR_CODE({
    const std::vector<int64_t>& max_input_shape =
        container->max_input_shape(input_idx);
    *ret_input_sizes = max_input_shape.data();
    *ret_input_ndim = max_input_shape.size();
  })
}

AOTInductorError AOTInductorModelContainerGetMaxOutputShape(
    AOTInductorModelContainerHandle container_handle,
    size_t output_idx,
    const int64_t** ret_output_sizes,
    int64_t* ret_output_ndim) {
  auto* container =
      reinterpret_cast<torch::aot_inductor::AOTInductorModelContainer*>(
          container_handle);
  CONVERT_EXCEPTION_TO_ERROR_CODE({
    const std::vector<int64_t>& max_output_shape =
        container->max_output_shape(output_idx);
    *ret_output_sizes = max_output_shape.data();
    *ret_output_ndim = max_output_shape.size();
  })
}

} // extern "C"
