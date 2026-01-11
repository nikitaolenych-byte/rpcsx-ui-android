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
	# Максимальні оптимізації для ARMv9 з підтримкою SVE2
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv9-a+sve2 -O3")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mtune=cortex-x4 -mcpu=cortex-x4")
	# Векторизація та агресивні оптимізації
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftree-vectorize -ffast-math")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -funroll-loops -finline-functions")
	# LTO для зменшення розміру та покращення швидкості
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto=thin")
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
