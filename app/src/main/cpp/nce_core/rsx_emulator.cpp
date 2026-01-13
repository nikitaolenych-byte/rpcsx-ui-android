// ============================================================================
// RSX Emulator Implementation
// ============================================================================
// Reality Synthesizer GPU → Vulkan Backend
// ============================================================================

#include "rsx_emulator.h"
#include <android/log.h>
#include <cstring>
#include <sys/mman.h>

#define LOG_TAG "RSX-Emulator"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace rpcsx {
namespace rsx {

// ============================================================================
// RSX Vulkan Backend
// ============================================================================
RSXVulkanBackend::RSXVulkanBackend()
    : instance_(VK_NULL_HANDLE)
    , physical_device_(VK_NULL_HANDLE)
    , device_(VK_NULL_HANDLE)
    , graphics_queue_(VK_NULL_HANDLE)
    , command_pool_(VK_NULL_HANDLE)
    , render_pass_(VK_NULL_HANDLE)
    , current_pipeline_(VK_NULL_HANDLE)
    , pipeline_layout_(VK_NULL_HANDLE)
    , descriptor_pool_(VK_NULL_HANDLE)
    , current_framebuffer_(VK_NULL_HANDLE)
    , current_command_buffer_(VK_NULL_HANDLE)
    , graphics_queue_family_(0)
    , initialized_(false) {
}

RSXVulkanBackend::~RSXVulkanBackend() {
    Shutdown();
}

bool RSXVulkanBackend::Initialize(VkInstance instance, VkPhysicalDevice physical_device, VkDevice device) {
    if (initialized_) return true;
    
    instance_ = instance;
    physical_device_ = physical_device;
    device_ = device;
    
    // Get queue
    vkGetDeviceQueue(device_, 0, 0, &graphics_queue_);
    
    // Create command pool
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = graphics_queue_family_;
    
    if (vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_) != VK_SUCCESS) {
        LOGE("Failed to create command pool");
        return false;
    }
    
    // Allocate command buffers
    command_buffers_.resize(3);
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool_;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 3;
    
    if (vkAllocateCommandBuffers(device_, &alloc_info, command_buffers_.data()) != VK_SUCCESS) {
        LOGE("Failed to allocate command buffers");
        return false;
    }
    
    // Create descriptor pool
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 }
    };
    
    VkDescriptorPoolCreateInfo desc_pool_info = {};
    desc_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    desc_pool_info.poolSizeCount = 2;
    desc_pool_info.pPoolSizes = pool_sizes;
    desc_pool_info.maxSets = 100;
    
    if (vkCreateDescriptorPool(device_, &desc_pool_info, nullptr, &descriptor_pool_) != VK_SUCCESS) {
        LOGE("Failed to create descriptor pool");
        return false;
    }
    
    // Create default render pass
    CreateDefaultRenderPass();
    
    // Create pipeline layout
    CreatePipelineLayout();
    
    initialized_ = true;
    LOGI("RSX Vulkan Backend initialized");
    return true;
}

void RSXVulkanBackend::Shutdown() {
    if (!device_) return;
    
    vkDeviceWaitIdle(device_);
    
    // Destroy pipelines
    for (auto& [hash, pipeline] : pipeline_cache_) {
        vkDestroyPipeline(device_, pipeline, nullptr);
    }
    pipeline_cache_.clear();
    
    // Destroy shader modules
    for (auto& [hash, module] : shader_cache_) {
        vkDestroyShaderModule(device_, module, nullptr);
    }
    shader_cache_.clear();
    
    // Destroy textures
    for (auto& [addr, texture] : texture_cache_) {
        vkDestroyImageView(device_, texture.view, nullptr);
        vkDestroyImage(device_, texture.image, nullptr);
        vkFreeMemory(device_, texture.memory, nullptr);
    }
    texture_cache_.clear();
    
    // Destroy framebuffers
    for (auto& [addr, fb] : framebuffer_cache_) {
        vkDestroyFramebuffer(device_, fb.framebuffer, nullptr);
        vkDestroyImageView(device_, fb.color_view, nullptr);
        if (fb.depth_view) vkDestroyImageView(device_, fb.depth_view, nullptr);
        vkDestroyImage(device_, fb.color_image, nullptr);
        if (fb.depth_image) vkDestroyImage(device_, fb.depth_image, nullptr);
    }
    framebuffer_cache_.clear();
    
    if (pipeline_layout_) vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
    if (render_pass_) vkDestroyRenderPass(device_, render_pass_, nullptr);
    if (descriptor_pool_) vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
    if (command_pool_) vkDestroyCommandPool(device_, command_pool_, nullptr);
    
    initialized_ = false;
}

void RSXVulkanBackend::CreateDefaultRenderPass() {
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = VK_FORMAT_R8G8B8A8_UNORM;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    VkAttachmentDescription depth_attachment = {};
    depth_attachment.format = VK_FORMAT_D24_UNORM_S8_UINT;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference color_ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference depth_ref = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
    
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;
    subpass.pDepthStencilAttachment = &depth_ref;
    
    VkAttachmentDescription attachments[] = { color_attachment, depth_attachment };
    
    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 2;
    render_pass_info.pAttachments = attachments;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    
    vkCreateRenderPass(device_, &render_pass_info, nullptr, &render_pass_);
}

void RSXVulkanBackend::CreatePipelineLayout() {
    VkDescriptorSetLayoutBinding bindings[] = {
        { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
        { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr }
    };
    
    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 2;
    layout_info.pBindings = bindings;
    
    VkDescriptorSetLayout set_layout;
    vkCreateDescriptorSetLayout(device_, &layout_info, nullptr, &set_layout);
    
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &set_layout;
    
    vkCreatePipelineLayout(device_, &pipeline_layout_info, nullptr, &pipeline_layout_);
}

void RSXVulkanBackend::BeginFrame() {
    current_command_buffer_ = command_buffers_[0];
    
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(current_command_buffer_, &begin_info);
}

void RSXVulkanBackend::EndFrame() {
    vkEndCommandBuffer(current_command_buffer_);
    
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &current_command_buffer_;
    
    vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue_);
}

void RSXVulkanBackend::SetSurface(const RSXState::Surface& surface) {
    current_surface_ = surface;
    // Create/get framebuffer for this surface
}

void RSXVulkanBackend::SetViewport(const RSXState::Viewport& viewport) {
    current_viewport_ = viewport;
}

void RSXVulkanBackend::SetScissor(const RSXState::Scissor& scissor) {
    current_scissor_ = scissor;
}

void RSXVulkanBackend::SetDepthStencil(const RSXState::DepthStencil& ds) {
    current_depth_stencil_ = ds;
}

void RSXVulkanBackend::SetBlend(const RSXState::Blend& blend) {
    current_blend_ = blend;
}

void RSXVulkanBackend::BindVertexShader(const uint8_t* program, size_t size) {
    // TODO: Translate RSX vertex shader → SPIR-V
    current_vertex_shader_ = program;
    current_vertex_shader_size_ = size;
}

void RSXVulkanBackend::BindFragmentShader(const uint8_t* program, size_t size) {
    // TODO: Translate RSX fragment shader → SPIR-V
    current_fragment_shader_ = program;
    current_fragment_shader_size_ = size;
}

void RSXVulkanBackend::BindTexture(uint32_t slot, const RSXState::Texture& texture) {
    if (slot < 16) {
        current_textures_[slot] = texture;
    }
}

void RSXVulkanBackend::SetVertexAttribute(uint32_t index, const RSXState::VertexAttribute& attr) {
    if (index < 16) {
        current_vertex_attributes_[index] = attr;
    }
}

void RSXVulkanBackend::DrawArrays(uint32_t mode, uint32_t first, uint32_t count) {
    if (!current_command_buffer_) return;
    
    VkViewport vk_viewport = {};
    vk_viewport.x = current_viewport_.x;
    vk_viewport.y = current_viewport_.y;
    vk_viewport.width = current_viewport_.width;
    vk_viewport.height = current_viewport_.height;
    vk_viewport.minDepth = current_viewport_.min_depth;
    vk_viewport.maxDepth = current_viewport_.max_depth;
    
    VkRect2D vk_scissor = {};
    vk_scissor.offset = { static_cast<int32_t>(current_scissor_.x), 
                          static_cast<int32_t>(current_scissor_.y) };
    vk_scissor.extent = { current_scissor_.width, current_scissor_.height };
    
    vkCmdSetViewport(current_command_buffer_, 0, 1, &vk_viewport);
    vkCmdSetScissor(current_command_buffer_, 0, 1, &vk_scissor);
    
    // Convert RSX primitive mode to Vulkan
    // mode: 0=points, 1=lines, 2=line_strip, 3=triangles, 4=triangle_strip, 5=triangle_fan
    
    vkCmdDraw(current_command_buffer_, count, 1, first, 0);
}

void RSXVulkanBackend::DrawIndexed(uint32_t mode, uint32_t count, uint32_t type, const void* indices) {
    if (!current_command_buffer_) return;
    
    // TODO: Create index buffer and bind
    vkCmdDrawIndexed(current_command_buffer_, count, 1, 0, 0, 0);
}

void RSXVulkanBackend::Clear(uint32_t mask, float r, float g, float b, float a, float depth, uint8_t stencil) {
    if (!current_command_buffer_) return;
    
    VkClearAttachment attachments[2] = {};
    uint32_t attachment_count = 0;
    
    if (mask & 0xF0) {  // Color
        attachments[attachment_count].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        attachments[attachment_count].colorAttachment = 0;
        attachments[attachment_count].clearValue.color = {{ r, g, b, a }};
        attachment_count++;
    }
    
    if (mask & 0x03) {  // Depth/Stencil
        attachments[attachment_count].aspectMask = 0;
        if (mask & 0x01) attachments[attachment_count].aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
        if (mask & 0x02) attachments[attachment_count].aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        attachments[attachment_count].clearValue.depthStencil = { depth, stencil };
        attachment_count++;
    }
    
    VkClearRect clear_rect = {};
    clear_rect.rect.extent = { current_surface_.width, current_surface_.height };
    clear_rect.layerCount = 1;
    
    vkCmdClearAttachments(current_command_buffer_, attachment_count, attachments, 1, &clear_rect);
}

void RSXVulkanBackend::Flip(uint32_t buffer_id) {
    // Present frame
    EndFrame();
    LOGI("RSX Flip: buffer %d", buffer_id);
}

// ============================================================================
// RSX Emulator
// ============================================================================
RSXEmulator::RSXEmulator()
    : vram_(nullptr)
    , vram_size_(0)
    , fifo_base_(0)
    , fifo_size_(0)
    , put_offset_(0)
    , get_offset_(0)
    , running_(false) {
}

RSXEmulator::~RSXEmulator() {
    Shutdown();
}

void RSXEmulator::Initialize() {
    // Allocate 256MB VRAM
    vram_size_ = 256 * 1024 * 1024;
    vram_ = mmap(nullptr, vram_size_, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (vram_ == MAP_FAILED) {
        LOGE("Failed to allocate VRAM");
        vram_ = nullptr;
        return;
    }
    
    memset(vram_, 0, vram_size_);
    
    // Initialize state
    memset(&state_, 0, sizeof(state_));
    state_.surface.width = 1280;
    state_.surface.height = 720;
    state_.surface.format = 0x85;  // ARGB8888
    
    state_.viewport.width = 1280;
    state_.viewport.height = 720;
    state_.viewport.max_depth = 1.0f;
    
    state_.scissor.width = 1280;
    state_.scissor.height = 720;
    
    state_.depth_stencil.depth_test = true;
    state_.depth_stencil.depth_write = true;
    state_.depth_stencil.depth_func = 0x0201;  // GL_LESS
    
    LOGI("RSX Emulator initialized: %zu MB VRAM", vram_size_ / (1024 * 1024));
}

void RSXEmulator::Shutdown() {
    Stop();
    
    if (vram_) {
        munmap(vram_, vram_size_);
        vram_ = nullptr;
    }
    
    backend_.Shutdown();
}

bool RSXEmulator::InitializeVulkan(VkInstance instance, VkPhysicalDevice physical_device, VkDevice device) {
    return backend_.Initialize(instance, physical_device, device);
}

// ============================================================================
// Memory Management
// ============================================================================
uint32_t RSXEmulator::AllocateContext(uint32_t size) {
    // Allocate command buffer at end of VRAM
    fifo_size_ = size;
    fifo_base_ = vram_size_ - fifo_size_;
    put_offset_ = 0;
    get_offset_ = 0;
    
    LOGI("RSX Context: base=0x%08x, size=%u", fifo_base_, fifo_size_);
    return fifo_base_;
}

void RSXEmulator::Put(uint32_t offset) {
    put_offset_ = offset;
}

uint32_t RSXEmulator::Get() {
    return get_offset_;
}

void RSXEmulator::Kick() {
    // Process commands from GET to PUT
    while (get_offset_ < put_offset_) {
        ProcessCommands();
    }
}

void* RSXEmulator::MemoryAllocate(uint32_t size, uint32_t alignment) {
    // Simple bump allocator
    static uint32_t alloc_offset = 0;
    
    alloc_offset = (alloc_offset + alignment - 1) & ~(alignment - 1);
    
    if (alloc_offset + size > fifo_base_) {
        LOGE("VRAM exhausted");
        return nullptr;
    }
    
    void* ptr = static_cast<uint8_t*>(vram_) + alloc_offset;
    alloc_offset += size;
    
    return ptr;
}

void RSXEmulator::MemoryFree(void* ptr) {
    // TODO: Proper memory management
}

// ============================================================================
// Command Processing
// ============================================================================
void RSXEmulator::ProcessCommands() {
    if (!vram_) return;
    
    uint32_t* fifo = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(vram_) + fifo_base_);
    
    while (get_offset_ < put_offset_) {
        uint32_t cmd = fifo[get_offset_ / 4];
        get_offset_ += 4;
        
        // RSX command format: [method:13][subchannel:3][count:13][inc:1][type:2]
        // or NV signature commands
        
        if (cmd == 0) continue;  // NOP
        
        uint32_t method = (cmd >> 2) & 0x7FF;
        uint32_t count = (cmd >> 18) & 0x7FF;
        bool inc = (cmd >> 29) & 1;
        
        for (uint32_t i = 0; i < count; i++) {
            if (get_offset_ >= put_offset_) break;
            
            uint32_t arg = fifo[get_offset_ / 4];
            get_offset_ += 4;
            
            ExecuteMethod(method, arg);
            
            if (inc) method++;
        }
    }
}

void RSXEmulator::ExecuteMethod(uint32_t method, uint32_t arg) {
    // NV4097 (main 3D class) methods
    switch (method) {
        // Surface
        case NV4097_SET_SURFACE_COLOR_TARGET:
            state_.surface.color_target = arg;
            break;
        case NV4097_SET_SURFACE_PITCH_A:
            state_.surface.pitch = arg;
            break;
        case NV4097_SET_SURFACE_COLOR_AOFFSET:
            state_.surface.color_offset[0] = arg;
            break;
        case NV4097_SET_SURFACE_COLOR_BOFFSET:
            state_.surface.color_offset[1] = arg;
            break;
        case NV4097_SET_SURFACE_ZETA_OFFSET:
            state_.surface.depth_offset = arg;
            break;
            
        // Viewport
        case NV4097_SET_VIEWPORT_HORIZONTAL:
            state_.viewport.x = arg & 0xFFFF;
            state_.viewport.width = (arg >> 16) & 0xFFFF;
            backend_.SetViewport(state_.viewport);
            break;
        case NV4097_SET_VIEWPORT_VERTICAL:
            state_.viewport.y = arg & 0xFFFF;
            state_.viewport.height = (arg >> 16) & 0xFFFF;
            backend_.SetViewport(state_.viewport);
            break;
            
        // Scissor
        case NV4097_SET_SCISSOR_HORIZONTAL:
            state_.scissor.x = arg & 0xFFFF;
            state_.scissor.width = (arg >> 16) & 0xFFFF;
            backend_.SetScissor(state_.scissor);
            break;
        case NV4097_SET_SCISSOR_VERTICAL:
            state_.scissor.y = arg & 0xFFFF;
            state_.scissor.height = (arg >> 16) & 0xFFFF;
            backend_.SetScissor(state_.scissor);
            break;
            
        // Depth/Stencil
        case NV4097_SET_DEPTH_TEST_ENABLE:
            state_.depth_stencil.depth_test = arg != 0;
            backend_.SetDepthStencil(state_.depth_stencil);
            break;
        case NV4097_SET_DEPTH_FUNC:
            state_.depth_stencil.depth_func = arg;
            backend_.SetDepthStencil(state_.depth_stencil);
            break;
        case NV4097_SET_DEPTH_MASK:
            state_.depth_stencil.depth_write = arg != 0;
            backend_.SetDepthStencil(state_.depth_stencil);
            break;
        case NV4097_SET_STENCIL_TEST_ENABLE:
            state_.depth_stencil.stencil_test = arg != 0;
            backend_.SetDepthStencil(state_.depth_stencil);
            break;
            
        // Blend
        case NV4097_SET_BLEND_ENABLE:
            state_.blend.enable = arg != 0;
            backend_.SetBlend(state_.blend);
            break;
        case NV4097_SET_BLEND_FUNC_SFACTOR:
            state_.blend.src_factor = arg & 0xFFFF;
            state_.blend.dst_factor = (arg >> 16) & 0xFFFF;
            backend_.SetBlend(state_.blend);
            break;
        case NV4097_SET_BLEND_EQUATION:
            state_.blend.equation = arg;
            backend_.SetBlend(state_.blend);
            break;
            
        // Draw
        case NV4097_SET_BEGIN_END:
            if (arg) {
                current_primitive_mode_ = arg;
                current_draw_first_ = 0;
                current_draw_count_ = 0;
            } else {
                // End - execute draw
                backend_.DrawArrays(current_primitive_mode_, 
                                   current_draw_first_, 
                                   current_draw_count_);
            }
            break;
            
        case NV4097_DRAW_ARRAYS:
            current_draw_first_ = arg & 0xFFFFFF;
            current_draw_count_ = (arg >> 24) + 1;
            break;
            
        case NV4097_CLEAR_SURFACE:
            {
                float r = state_.clear_color[0];
                float g = state_.clear_color[1];
                float b = state_.clear_color[2];
                float a = state_.clear_color[3];
                backend_.Clear(arg, r, g, b, a, state_.clear_depth, state_.clear_stencil);
            }
            break;
            
        // Vertex attributes
        case NV4097_SET_VERTEX_DATA_ARRAY_FORMAT:
            {
                uint32_t index = (method - NV4097_SET_VERTEX_DATA_ARRAY_FORMAT) >> 2;
                if (index < 16) {
                    state_.vertex_attribs[index].type = arg & 0xF;
                    state_.vertex_attribs[index].size = (arg >> 4) & 0xF;
                    state_.vertex_attribs[index].stride = (arg >> 8) & 0xFF;
                    backend_.SetVertexAttribute(index, state_.vertex_attribs[index]);
                }
            }
            break;
            
        // Textures
        case NV4097_SET_TEXTURE_ADDRESS:
        case NV4097_SET_TEXTURE_FORMAT:
        case NV4097_SET_TEXTURE_CONTROL0:
        case NV4097_SET_TEXTURE_CONTROL1:
        case NV4097_SET_TEXTURE_FILTER:
        case NV4097_SET_TEXTURE_IMAGE_RECT:
            // TODO: Handle texture state
            break;
            
        // Shaders
        case NV4097_SET_TRANSFORM_PROGRAM_LOAD:
            state_.vertex_shader.load_offset = arg;
            break;
        case NV4097_SET_SHADER_PROGRAM:
            state_.fragment_shader.offset = arg;
            break;
            
        default:
            // Unknown method - ignore
            break;
    }
}

// ============================================================================
// Flip (Present Frame)
// ============================================================================
void RSXEmulator::Flip(uint32_t buffer_id) {
    frame_count_++;
    backend_.Flip(buffer_id);
}

// ============================================================================
// Execution Control
// ============================================================================
void RSXEmulator::Run() {
    running_ = true;
    LOGI("RSX: Starting execution");
        profiler_.Start("RSX_Run");
    
    backend_.BeginFrame();
    
    while (running_) {
        if (get_offset_ < put_offset_) {
            ProcessCommands();
        }
        // In real emulator: sync with VSync or sleep
    }
        profiler_.Stop("RSX_Run");
}

void RSXEmulator::Stop() {
    running_ = false;
    LOGI("RSX: Stopping");
}

RSXState* RSXEmulator::GetState() {
    return &state_;
}

uint64_t RSXEmulator::GetFrameCount() const {
    return frame_count_;
}

} // namespace rsx
} // namespace rpcsx
