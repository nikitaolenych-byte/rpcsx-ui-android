// ============================================================================
// RSX - Reality Synthesizer (PS3 GPU) → Vulkan Backend
// ============================================================================
// RSX базується на NVIDIA G70/G71 (GeForce 7800)
// 256MB GDDR3 VRAM
// OpenGL ES 2.0 рівень функціональності
// 
// Транслюємо RSX команди → Vulkan для Android
// ============================================================================

#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <memory>
#include <vulkan/vulkan.h>

namespace rpcsx {
namespace rsx {

// ============================================================================
// RSX Register Map
// ============================================================================
constexpr uint32_t RSX_CONTEXT_BASE = 0x40000000;
constexpr uint32_t RSX_CONTEXT_SIZE = 16 * 1024 * 1024;  // 16MB command buffer

// RSX NV4097 (Kelvin/Curie) registers
enum RSXRegister : uint32_t {
    NV4097_SET_OBJECT                        = 0x00000000,
    NV4097_NO_OPERATION                      = 0x00000100,
    NV4097_NOTIFY                            = 0x00000104,
    NV4097_WAIT_FOR_IDLE                     = 0x00000110,
    
    // Context
    NV4097_SET_CONTEXT_DMA_NOTIFIES          = 0x00000180,
    NV4097_SET_CONTEXT_DMA_A                 = 0x00000184,
    NV4097_SET_CONTEXT_DMA_B                 = 0x00000188,
    NV4097_SET_CONTEXT_DMA_COLOR             = 0x00000194,
    NV4097_SET_CONTEXT_DMA_ZETA              = 0x00000198,
    NV4097_SET_CONTEXT_DMA_VERTEX            = 0x0000019C,
    NV4097_SET_CONTEXT_DMA_SEMAPHORE         = 0x000001A0,
    NV4097_SET_CONTEXT_DMA_REPORT            = 0x000001A4,
    
    // Surface
    NV4097_SET_SURFACE_FORMAT                = 0x00000200,
    NV4097_SET_SURFACE_PITCH_A               = 0x00000204,
    NV4097_SET_SURFACE_COLOR_OFFSET          = 0x00000208,
    NV4097_SET_SURFACE_ZETA_OFFSET           = 0x0000020C,
    NV4097_SET_SURFACE_COLOR_TARGET          = 0x00000210,
    NV4097_SET_SURFACE_CLIP_HORIZONTAL       = 0x00000214,
    NV4097_SET_SURFACE_CLIP_VERTICAL         = 0x00000218,
    
    // Render Target
    NV4097_SET_COLOR_MASK                    = 0x00000304,
    NV4097_SET_COLOR_MASK_MRT                = 0x00000308,
    
    // Depth/Stencil
    NV4097_SET_DEPTH_TEST_ENABLE             = 0x0000030C,
    NV4097_SET_DEPTH_MASK                    = 0x00000310,
    NV4097_SET_DEPTH_FUNC                    = 0x00000314,
    NV4097_SET_DEPTH_BOUNDS_TEST_ENABLE      = 0x00000318,
    NV4097_SET_DEPTH_BOUNDS_MIN              = 0x0000031C,
    NV4097_SET_DEPTH_BOUNDS_MAX              = 0x00000320,
    NV4097_SET_STENCIL_TEST_ENABLE           = 0x00000324,
    NV4097_SET_STENCIL_MASK                  = 0x00000328,
    NV4097_SET_STENCIL_FUNC                  = 0x0000032C,
    NV4097_SET_STENCIL_FUNC_REF              = 0x00000330,
    NV4097_SET_STENCIL_FUNC_MASK             = 0x00000334,
    NV4097_SET_STENCIL_OP_FAIL               = 0x00000338,
    NV4097_SET_STENCIL_OP_ZFAIL              = 0x0000033C,
    NV4097_SET_STENCIL_OP_ZPASS              = 0x00000340,
    
    // Blending
    NV4097_SET_BLEND_ENABLE                  = 0x00000344,
    NV4097_SET_BLEND_FUNC_SFACTOR            = 0x00000348,
    NV4097_SET_BLEND_FUNC_DFACTOR            = 0x0000034C,
    NV4097_SET_BLEND_COLOR                   = 0x00000350,
    NV4097_SET_BLEND_EQUATION                = 0x00000354,
    NV4097_SET_BLEND_ENABLE_MRT              = 0x00000358,
    
    // Viewport
    NV4097_SET_VIEWPORT_HORIZONTAL           = 0x00000400,
    NV4097_SET_VIEWPORT_VERTICAL             = 0x00000404,
    NV4097_SET_VIEWPORT_OFFSET               = 0x00000408,
    NV4097_SET_VIEWPORT_SCALE                = 0x0000040C,
    
    // Scissor
    NV4097_SET_SCISSOR_HORIZONTAL            = 0x00000410,
    NV4097_SET_SCISSOR_VERTICAL              = 0x00000414,
    
    // Vertex Program
    NV4097_SET_VERTEX_PROGRAM_LOAD           = 0x00000504,
    NV4097_SET_VERTEX_PROGRAM_START_SLOT     = 0x00000508,
    
    // Fragment Program
    NV4097_SET_SHADER_PROGRAM                = 0x00000600,
    NV4097_SET_SHADER_CONTROL                = 0x00000604,
    
    // Vertex Data
    NV4097_SET_VERTEX_DATA_ARRAY_FORMAT      = 0x00001000,  // + offset * 4
    NV4097_SET_VERTEX_DATA_ARRAY_OFFSET      = 0x00001080,  // + offset * 4
    
    // Texture
    NV4097_SET_TEXTURE_OFFSET                = 0x00001A00,  // + texture * 32
    NV4097_SET_TEXTURE_FORMAT                = 0x00001A04,
    NV4097_SET_TEXTURE_ADDRESS               = 0x00001A08,
    NV4097_SET_TEXTURE_CONTROL0              = 0x00001A0C,
    NV4097_SET_TEXTURE_CONTROL1              = 0x00001A10,
    NV4097_SET_TEXTURE_FILTER                = 0x00001A14,
    NV4097_SET_TEXTURE_IMAGE_RECT            = 0x00001A18,
    NV4097_SET_TEXTURE_BORDER_COLOR          = 0x00001A1C,
    
    // Draw
    NV4097_DRAW_ARRAYS                       = 0x00001808,
    NV4097_DRAW_INDEX_ARRAY                  = 0x0000180C,
    NV4097_INLINE_ARRAY                      = 0x00001810,
    
    // Clear
    NV4097_CLEAR_SURFACE                     = 0x00001D8C,
};

// ============================================================================
// RSX Command Buffer
// ============================================================================
struct RSXCommand {
    uint32_t method;     // Register offset
    uint32_t count;      // Number of args
    uint32_t args[256];  // Arguments
};

// ============================================================================
// RSX State
// ============================================================================
struct RSXState {
    // Command buffer (ring buffer у GDDR3)
    uint32_t* command_buffer;
    uint32_t cb_size;
    uint32_t cb_get;   // Read pointer
    uint32_t cb_put;   // Write pointer
    
    // Surface
    struct {
        uint32_t format;
        uint32_t width;
        uint32_t height;
        uint32_t pitch;
        uint32_t color_offset;
        uint32_t zeta_offset;
        uint32_t color_target;
    } surface;
    
    // Viewport
    struct {
        float x, y, width, height;
        float scale[4];
        float offset[4];
    } viewport;
    
    // Scissor
    struct {
        uint32_t x, y, width, height;
    } scissor;
    
    // Depth/Stencil
    struct {
        bool depth_test_enable;
        bool depth_write_enable;
        uint32_t depth_func;
        bool stencil_test_enable;
        uint32_t stencil_func;
        uint32_t stencil_ref;
        uint32_t stencil_mask;
        uint32_t stencil_op_fail;
        uint32_t stencil_op_zfail;
        uint32_t stencil_op_zpass;
    } depth_stencil;
    
    // Blend
    struct {
        bool enable;
        uint32_t src_factor;
        uint32_t dst_factor;
        uint32_t equation;
        uint32_t color;
    } blend;
    
    // Shaders
    struct {
        uint32_t vertex_program_offset;
        uint32_t vertex_program_start;
        uint32_t fragment_program_offset;
        uint32_t shader_control;
    } shaders;
    
    // Vertex attributes (16 max)
    struct VertexAttrib {
        uint32_t offset;
        uint32_t format;
        uint32_t stride;
        uint32_t freq;
    } vertex_attribs[16];
    
    // Textures (16 max)
    struct Texture {
        uint32_t offset;
        uint32_t format;
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        uint32_t pitch;
        uint32_t mipmap;
        uint32_t address_u;
        uint32_t address_v;
        uint32_t address_w;
        uint32_t min_filter;
        uint32_t mag_filter;
        uint32_t border_color;
    } textures[16];
    
    // Execution state
    bool running;
    bool flip_pending;
};

// ============================================================================
// Vulkan Backend for RSX
// ============================================================================
class RSXVulkanBackend {
public:
    RSXVulkanBackend();
    ~RSXVulkanBackend();
    
    // Initialize Vulkan
    bool Initialize(VkInstance instance, VkPhysicalDevice physical_device, 
                   VkDevice device, VkQueue graphics_queue, uint32_t queue_family);
    void Shutdown();
    
    // RSX command processing
    void ProcessCommand(RSXState& state, const RSXCommand& cmd);
    void ProcessCommandBuffer(RSXState& state);
    
    // Draw commands
    void DrawArrays(RSXState& state, uint32_t mode, uint32_t first, uint32_t count);
    void DrawIndexed(RSXState& state, uint32_t mode, uint32_t index_type, 
                     uint32_t index_offset, uint32_t count);
    
    // Clear
    void ClearSurface(RSXState& state, uint32_t mask);
    
    // Flip
    void Flip(RSXState& state);
    
    // Shader compilation (RSX shader → SPIR-V)
    VkShaderModule CompileVertexShader(const uint32_t* rsx_shader, size_t size);
    VkShaderModule CompileFragmentShader(const uint32_t* rsx_shader, size_t size);
    
    // Texture operations
    VkImage CreateTexture(uint32_t format, uint32_t width, uint32_t height, 
                         uint32_t depth, uint32_t mipmap);
    void UploadTexture(VkImage image, const void* data, size_t size);
    
    // Render target
    void SetRenderTarget(uint32_t width, uint32_t height, VkFormat color_format, 
                        VkFormat depth_format);
    
private:
    VkInstance instance_;
    VkPhysicalDevice physical_device_;
    VkDevice device_;
    VkQueue graphics_queue_;
    uint32_t queue_family_;
    
    // Command buffer для Vulkan
    VkCommandPool command_pool_;
    VkCommandBuffer command_buffer_;
    
    // Pipeline state
    VkPipeline current_pipeline_;
    VkPipelineLayout pipeline_layout_;
    VkRenderPass render_pass_;
    VkFramebuffer framebuffer_;
    
    // Descriptors
    VkDescriptorPool descriptor_pool_;
    VkDescriptorSetLayout descriptor_set_layout_;
    VkDescriptorSet descriptor_set_;
    
    // Vertex/Index buffers
    VkBuffer vertex_buffer_;
    VkDeviceMemory vertex_memory_;
    VkBuffer index_buffer_;
    VkDeviceMemory index_memory_;
    
    // Render targets
    VkImage color_image_;
    VkImageView color_image_view_;
    VkDeviceMemory color_memory_;
    VkImage depth_image_;
    VkImageView depth_image_view_;
    VkDeviceMemory depth_memory_;
    
    // Shader cache (RSX bytecode → SPIR-V)
    std::unordered_map<uint64_t, VkShaderModule> shader_cache_;
    
    // Pipeline cache
    std::unordered_map<uint64_t, VkPipeline> pipeline_cache_;
    
    // Texture cache
    std::unordered_map<uint64_t, VkImage> texture_cache_;
    
    // Helper functions
    VkShaderModule CreateShaderModule(const uint32_t* spirv, size_t size);
    VkPipeline CreatePipeline(const RSXState& state);
    uint64_t HashState(const RSXState& state);
    
    // RSX → Vulkan format conversion
    VkFormat RSXFormatToVulkan(uint32_t rsx_format);
    VkCompareOp RSXCompareFuncToVulkan(uint32_t rsx_func);
    VkBlendFactor RSXBlendFactorToVulkan(uint32_t rsx_factor);
    VkBlendOp RSXBlendOpToVulkan(uint32_t rsx_op);
    VkPrimitiveTopology RSXPrimitiveToVulkan(uint32_t rsx_mode);
    
    // Shader translation (RSX bytecode → SPIR-V)
    bool TranslateVertexShader(const uint32_t* rsx_shader, size_t size, 
                              std::vector<uint32_t>& spirv);
    bool TranslateFragmentShader(const uint32_t* rsx_shader, size_t size, 
                                std::vector<uint32_t>& spirv);
};

// ============================================================================
// RSX Emulator - Main Interface
// ============================================================================
class RSXEmulator {
public:
    RSXEmulator();
    ~RSXEmulator();
    
    // Initialize з Vulkan
    bool Initialize(void* vram_base, size_t vram_size,
                   VkInstance instance, VkPhysicalDevice physical_device,
                   VkDevice device, VkQueue graphics_queue, uint32_t queue_family);
<<<<<<< HEAD
=======
        util::Profiler profiler_;
        double GetLastRunTime() const { return profiler_.GetElapsed("RSX_Run"); }
>>>>>>> c3fa6c4 (build: ARMv9 NCE, thread pool, SIMD, shader cache, UI NCE button)
    void Shutdown();
    
    // Allocate RSX context (command buffer у VRAM)
    uint32_t AllocateContext(uint32_t size);
    void FreeContext(uint32_t handle);
    
    // Command submission
    void Put(uint32_t offset);   // Update PUT pointer
    uint32_t Get();              // Get GET pointer
    void Kick();                 // Process commands
    
    // Memory management
    uint32_t MemoryAllocate(uint32_t size, uint32_t align);
    void MemoryFree(uint32_t offset);
    
    // Execution
    void Run();
    void Stop();
    bool IsRunning();
    
    // Flip/Present
    void Flip();
    void WaitForFlip();
    
    // Access state
    RSXState* GetState() { return &state_; }
    
private:
    RSXState state_;
    RSXVulkanBackend backend_;
    
    void* vram_base_;
    size_t vram_size_;
    uint32_t vram_allocated_;
    
    std::unordered_map<uint32_t, uint32_t> context_map_;  // handle → offset
    
    bool running_;
    bool flip_pending_;
    
    void ProcessCommands();
    void ExecuteMethod(uint32_t method, uint32_t arg);
};

} // namespace rsx
} // namespace rpcsx
