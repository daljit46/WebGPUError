#include "utils.h"
#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#include <array>
#include <exception>
#include <iostream>
#include <math.h>
#include <vector>

WGPUSwapChain buildSwapchain(WGPUDevice device, WGPUSurface surface)
{
    std::cout << "Building swapchain..." << std::endl;
    WGPUTextureFormat swapChainFormat = WGPUTextureFormat_BGRA8Unorm;

    WGPUSwapChainDescriptor swapChainDesc {};
    swapChainDesc.nextInChain = nullptr;
    swapChainDesc.label = "My Swapchain";
    swapChainDesc.width = 800;
    swapChainDesc.height = 600;
    swapChainDesc.format = swapChainFormat;
    swapChainDesc.presentMode = WGPUPresentMode_Fifo;
    swapChainDesc.usage = WGPUTextureUsage_RenderAttachment;

    WGPUSwapChain swapChain = wgpuDeviceCreateSwapChain(device, surface, &swapChainDesc);
    if(!swapChain){
        throw std::runtime_error("Failed to create swapchain");
    }
    std::cout << "Got swapchain: " << swapChain << std::endl;
    return swapChain;
}

int main()
{
    // Create WebGPU instance
    WGPUInstanceDescriptor instanceDesc {};
    instanceDesc.nextInChain = nullptr;

    WGPUInstance instance = wgpuCreateInstance(&instanceDesc);

    // Initialise GLFW window
    if(!glfwInit()){
        throw std::runtime_error("Failed to initialise GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(800, 600, "WebGPU", nullptr, nullptr);
    if(!window){
        throw std::runtime_error("Failed to create GLFW window");
    }

    std::cout << "Requesting adapter..." << std::endl;
    WGPUSurface surface = glfwGetWGPUSurface(instance, window);
    if(!surface){
        throw std::runtime_error("Failed to create surface");
    }
    WGPURequestAdapterOptions options {};
    options.nextInChain = nullptr;
    options.compatibleSurface = surface;
    WGPUAdapter adapter = Utils::requestAdapter(instance, &options);
    if(!adapter){
        throw std::runtime_error("Failed to find suitable adapter");
    }
    std::cout << "Got adapter: " << adapter << std::endl;

    WGPUSupportedLimits supportedLimits {};
    wgpuAdapterGetLimits(adapter, &supportedLimits);

    std::cout << "Requesting device..." << std::endl;
    WGPURequiredLimits requiredLimits {};
    requiredLimits.limits.maxVertexAttributes = 2;
    requiredLimits.limits.maxVertexBuffers = 1;
    requiredLimits.limits.maxBufferSize = 4 * 5 * sizeof(float);
	requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;
	requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
	requiredLimits.limits.maxInterStageShaderComponents = 3;
    requiredLimits.limits.maxBindGroups = 1;
    requiredLimits.limits.maxUniformBuffersPerShaderStage = 1;
    requiredLimits.limits.maxUniformBufferBindingSize = 16 * 4;

    WGPUDeviceDescriptor deviceDesc {};
    deviceDesc.nextInChain = nullptr;
    deviceDesc.label = "My Device";
    deviceDesc.requiredFeatureCount = 0;
    deviceDesc.requiredLimits = &requiredLimits;
    deviceDesc.defaultQueue.label = "The default queue";
    WGPUDevice device = Utils::requestDevice(adapter, &deviceDesc);
    if(!device){
        throw std::runtime_error("Failed to create device");
    }
    std::cout << "Got device: " << device << std::endl;

    auto onDeviceErrorCallback = [](WGPUErrorType type, const char* message, void*) {
        std::cout << "Device error type: " << type;
        if(message){
            std::cout << " message: " << message;
        }
        std::cout << std::endl;
    };
    wgpuDeviceSetUncapturedErrorCallback(device, onDeviceErrorCallback, nullptr);

    WGPUQueue queue = wgpuDeviceGetQueue(device);

    WGPUSwapChain swapChain = buildSwapchain(device, surface);

    std::cout << "Creating shader module" << std::endl;
    WGPUShaderModule shaderModule = Utils::loadShaderModule("shaders.wgsl", device);
    std::cout << "Got shader module: " << shaderModule << std::endl;

    std::cout << "Creating render pipeline" << std::endl;
    WGPURenderPipelineDescriptor renderPipelineDesc {};

    std::vector<WGPUVertexAttribute> vertexAttribs(2);
    vertexAttribs[0].format = WGPUVertexFormat_Float32x2;
    vertexAttribs[0].offset = 0;

    vertexAttribs[1].format = WGPUVertexFormat_Float32x3;
    vertexAttribs[1].offset = 2 * sizeof(float);

    WGPUVertexBufferLayout vertexBufferLayout {};
    vertexBufferLayout.attributeCount = static_cast<uint32_t>(vertexAttribs.size());
    vertexBufferLayout.attributes = vertexAttribs.data();
    vertexBufferLayout.arrayStride = 5 * sizeof(float);
    vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;

    renderPipelineDesc.vertex.bufferCount = 1;
    renderPipelineDesc.vertex.buffers = &vertexBufferLayout;
    renderPipelineDesc.vertex.entryPoint = "main";
    renderPipelineDesc.vertex.module = shaderModule;
    renderPipelineDesc.vertex.constantCount = 0;
    renderPipelineDesc.vertex.constants = nullptr;

    renderPipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    renderPipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    renderPipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
    renderPipelineDesc.primitive.cullMode = WGPUCullMode_None;

    WGPUFragmentState fragmentState {};
    renderPipelineDesc.fragment = &fragmentState;
    fragmentState.entryPoint = "fs_main";
    fragmentState.module = shaderModule;
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;

    WGPUBlendState blendState {};
    blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blendState.color.operation = WGPUBlendOperation_Add;
    blendState.alpha.srcFactor = WGPUBlendFactor_Zero;
    blendState.alpha.dstFactor = WGPUBlendFactor_One;
    blendState.alpha.operation = WGPUBlendOperation_Add;

    WGPUColorTargetState colorTarget {};
    colorTarget.nextInChain = nullptr;
    colorTarget.format = WGPUTextureFormat_BGRA8Unorm;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    renderPipelineDesc.depthStencil = nullptr;
    renderPipelineDesc.multisample.count = 1;
    renderPipelineDesc.multisample.mask = ~0u;
    renderPipelineDesc.multisample.alphaToCoverageEnabled = false;

    WGPUBindGroupLayoutEntry bindingLayout = Utils::createDefaultBindingLayout();
    bindingLayout.binding = 0;
    bindingLayout.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    bindingLayout.buffer.type = WGPUBufferBindingType_Uniform;
    bindingLayout.buffer.type = WGPUBufferBindingType_Uniform;

    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc {};
    bindGroupLayoutDesc.nextInChain = nullptr;
    bindGroupLayoutDesc.label = "My Bind Group Layout";
    bindGroupLayoutDesc.entryCount = 1;
    bindGroupLayoutDesc.entries = &bindingLayout;
    // ERROR
    WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);
}