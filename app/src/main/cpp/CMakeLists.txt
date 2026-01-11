cmake_minimum_required(VERSION 3.14) # Bump for FetchContent
project("rpcsx-android")

include(FetchContent)

# Fetch Zstd for Shader Cache compression
FetchContent_Declare(
  zstd
  GIT_REPOSITORY https://github.com/facebook/zstd.git
  GIT_TAG v1.5.5
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR build/cmake 
)
# Disable Zstd legacy support and binaries to save space
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zstd)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE on)

# ARMv9 + SVE2 оптимізації для Snapdragon 8s Gen 3
if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    include(CheckCXXCompilerFlag)
    
    # Check for Cortex-X4 support
    check_cxx_compiler_flag("-mcpu=cortex-x4" HAVE_CORTEX_X4)
    if(HAVE_CORTEX_X4)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=cortex-x4")
    else()
        # Fallback to generic ARMv9-A if X4 is not recognized
        check_cxx_compiler_flag("-march=armv9-a+sve2" HAVE_ARMV9_SVE2)
        if(HAVE_ARMV9_SVE2)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv9-a+sve2")
        endif()
    endif()

	# Векторизація та агресивні оптимізації
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ftree-vectorize -ffast-math")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -funroll-loops -finline-functions")
	# LTO для зменшення розміру та покращення швидкості
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto=thin")
    # Suppress unused argument warnings if any flags are ignored
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-command-line-argument")
	
	# Оптимізації пам'яті для LPDDR5X
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdata-sections -ffunction-sections")
	
    # Fetch libadrenotools
    FetchContent_Declare(
      libadrenotools
      GIT_REPOSITORY https://github.com/bylaws/libadrenotools.git
      GIT_TAG main
      GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(libadrenotools)
    
    # Fix target name mismatch if any
    if(TARGET libadrenotools AND NOT TARGET adrenotools)
        add_library(adrenotools ALIAS libadrenotools)
    endif()

else()
	add_library(adrenotools INTERFACE)
endif()

add_library(${CMAKE_PROJECT_NAME} SHARED 
    native-lib.cpp
    nce_engine.cpp
    fastmem_mapper.cpp
    shader_cache_manager.cpp
    thread_scheduler.cpp
    frostbite_hacks.cpp
    vulkan_renderer.cpp
    fsr31/fsr31.cpp
)

# Підключення Mesa Turnip для Adreno 735
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # ${CMAKE_CURRENT_SOURCE_DIR}/mesa_turnip/include
    # ${CMAKE_CURRENT_SOURCE_DIR}/fsr31/include
)

target_link_libraries(${CMAKE_PROJECT_NAME}
    android
    log
    adrenotools
    z
    libzstd_static
)
