/**
 * RPCSX Shader Compiler with Vendor-Specific Optimizations
 * 
 * Compiles GLSL/SPIR-V shaders with GPU-specific optimizations.
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <vulkan/vulkan.h>

#include "gpu_detector.h"

namespace rpcsx {
namespace shaders {

// =============================================================================
// Shader Stage
// =============================================================================

enum class ShaderStage {
    VERTEX,
    FRAGMENT,
    COMPUTE,
    GEOMETRY,
    TESSELLATION_CONTROL,
    TESSELLATION_EVALUATION,
};

// =============================================================================
// Shader Optimization Flags
// =============================================================================

struct ShaderOptimizationFlags {
    // Precision settings
    bool use_half_precision = true;        // mediump/float16
    bool use_relaxed_precision = true;     // RelaxedPrecision decoration
    bool force_full_precision = false;     // Always use highp
    
    // Optimization levels
    bool optimize_for_performance = true;  // -O2 equivalent
    bool optimize_for_size = false;        // -Os equivalent
    bool enable_dead_code_elimination = true;
    bool enable_constant_folding = true;
    bool enable_loop_unrolling = true;
    
    // Vendor-specific
    bool enable_vendor_extensions = true;
    bool use_vendor_intrinsics = true;
    
    // Adreno-specific
    bool adreno_use_gmem_hints = true;
    bool adreno_optimize_binning = true;
    
    // Mali-specific
    bool mali_use_fma = true;
    bool mali_vectorize_fp16 = true;
    
    // PowerVR-specific
    bool powervr_optimize_tbdr = true;
    bool powervr_use_usc_hints = true;
    
    // Debug
    bool generate_debug_info = false;
    bool validate_spirv = true;
};

// =============================================================================
// Compiled Shader
// =============================================================================

struct CompiledShader {
    std::vector<uint32_t> spirv_code;
    ShaderStage stage = ShaderStage::VERTEX;
    std::string entry_point = "main";
    
    // Reflection info
    struct InputAttribute {
        uint32_t location;
        uint32_t binding;
        VkFormat format;
        std::string name;
    };
    std::vector<InputAttribute> input_attributes;
    
    struct UniformBuffer {
        uint32_t set;
        uint32_t binding;
        size_t size;
        std::string name;
    };
    std::vector<UniformBuffer> uniform_buffers;
    
    struct PushConstant {
        size_t offset;
        size_t size;
        std::string name;
    };
    std::vector<PushConstant> push_constants;
    
    struct Sampler {
        uint32_t set;
        uint32_t binding;
        std::string name;
    };
    std::vector<Sampler> samplers;
    
    // Optimization info
    bool is_optimized = false;
    gpu::GPUVendor target_vendor = gpu::GPUVendor::UNKNOWN;
    std::string optimization_notes;
    
    // Validation
    bool is_valid = false;
    std::string error_message;
};

// =============================================================================
// Shader Source
// =============================================================================

struct ShaderSource {
    std::string code;
    ShaderStage stage;
    std::string filename;  // For error messages
    
    // Preprocessor defines
    std::unordered_map<std::string, std::string> defines;
    
    // Include paths
    std::vector<std::string> include_paths;
};

// =============================================================================
// Shader Compiler Interface
// =============================================================================

class ShaderCompiler {
public:
    static ShaderCompiler& Instance();
    
    // Initialization
    bool Initialize();
    void Shutdown();
    bool IsInitialized() const { return m_initialized; }
    
    // Set target GPU
    void SetTargetGPU(gpu::GPUVendor vendor, gpu::GPUTier tier);
    void SetTargetGPU(const gpu::GPUInfo& gpu_info);
    
    // Compilation
    CompiledShader CompileGLSL(const ShaderSource& source, 
                                const ShaderOptimizationFlags& flags = {});
    
    CompiledShader CompileGLSL(const std::string& code, ShaderStage stage,
                                const ShaderOptimizationFlags& flags = {});
    
    // Load pre-compiled SPIR-V
    CompiledShader LoadSPIRV(const std::vector<uint32_t>& spirv_code, ShaderStage stage);
    CompiledShader LoadSPIRV(const std::string& filepath, ShaderStage stage);
    
    // Optimize existing SPIR-V
    CompiledShader OptimizeSPIRV(const CompiledShader& shader,
                                  const ShaderOptimizationFlags& flags = {});
    
    // Create Vulkan shader module
    VkShaderModule CreateShaderModule(VkDevice device, const CompiledShader& shader);
    
    // Get vendor-specific shader variant
    std::string GetVendorShaderPath(const std::string& base_path, 
                                     gpu::GPUVendor vendor,
                                     ShaderStage stage);
    
    // Preprocessing
    std::string PreprocessGLSL(const ShaderSource& source);
    void AddGlobalDefine(const std::string& name, const std::string& value);
    void ClearGlobalDefines();
    
    // Validation
    bool ValidateSPIRV(const std::vector<uint32_t>& spirv_code, std::string& error_out);
    
    // Cache management
    bool SaveToCache(const CompiledShader& shader, const std::string& cache_key);
    CompiledShader LoadFromCache(const std::string& cache_key);
    void ClearCache();
    
    // Statistics
    struct Stats {
        uint64_t shaders_compiled = 0;
        uint64_t cache_hits = 0;
        uint64_t cache_misses = 0;
        uint64_t compilation_time_ms = 0;
        uint64_t optimization_time_ms = 0;
    };
    const Stats& GetStats() const { return m_stats; }

private:
    ShaderCompiler() = default;
    ~ShaderCompiler() = default;
    
    // Internal compilation
    bool CompileGLSLToSPIRV(const std::string& source, ShaderStage stage,
                             std::vector<uint32_t>& spirv_out, std::string& error_out);
    
    // Vendor-specific optimization passes
    void ApplyAdrenoOptimizations(std::vector<uint32_t>& spirv);
    void ApplyMaliOptimizations(std::vector<uint32_t>& spirv);
    void ApplyPowerVROptimizations(std::vector<uint32_t>& spirv);
    
    // Add vendor-specific decorations
    void AddHalfPrecisionDecorations(std::vector<uint32_t>& spirv);
    void AddRelaxedPrecisionDecorations(std::vector<uint32_t>& spirv);
    
    // Inject vendor-specific GLSL modifications
    std::string InjectVendorPragmas(const std::string& source, gpu::GPUVendor vendor);
    std::string InjectPrecisionQualifiers(const std::string& source, 
                                           const ShaderOptimizationFlags& flags);
    
    bool m_initialized = false;
    gpu::GPUInfo m_target_gpu;
    ShaderOptimizationFlags m_default_flags;
    std::unordered_map<std::string, std::string> m_global_defines;
    
    // Cache
    std::string m_cache_path;
    std::unordered_map<std::string, CompiledShader> m_shader_cache;
    
    Stats m_stats;
};

// =============================================================================
// Shader Variant Manager
// =============================================================================

class ShaderVariantManager {
public:
    static ShaderVariantManager& Instance();
    
    // Register shader variants
    void RegisterVariant(const std::string& shader_name, 
                         gpu::GPUVendor vendor,
                         const std::string& shader_path);
    
    // Get best shader for current GPU
    std::string GetBestVariant(const std::string& shader_name);
    
    // Load all registered variants for current GPU
    bool LoadVariantsForGPU(gpu::GPUVendor vendor);
    
    // Get compiled shader
    const CompiledShader* GetCompiledShader(const std::string& shader_name);

private:
    ShaderVariantManager() = default;
    
    struct VariantInfo {
        std::string shader_name;
        gpu::GPUVendor vendor;
        std::string shader_path;
        CompiledShader compiled;
    };
    
    std::unordered_map<std::string, std::vector<VariantInfo>> m_variants;
    std::unordered_map<std::string, CompiledShader> m_active_shaders;
};

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * Get VkShaderStageFlagBits from ShaderStage
 */
VkShaderStageFlagBits ShaderStageToVulkan(ShaderStage stage);

/**
 * Get shader stage from file extension
 */
ShaderStage ShaderStageFromExtension(const std::string& extension);

/**
 * Generate cache key for shader
 */
std::string GenerateShaderCacheKey(const std::string& source, 
                                    ShaderStage stage,
                                    const ShaderOptimizationFlags& flags,
                                    gpu::GPUVendor vendor);

} // namespace shaders
} // namespace rpcsx
