#!/bin/bash

# Script: build-armv9-local.sh
# Локальна збірка RPCSX ARMv9 з усіма оптимізаціями

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Кольори для виводу
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Функції логування
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Функція для перевірки наявності утиліт
check_prerequisites() {
    log_info "Checking prerequisites..."
    
    local MISSING_TOOLS=()
    
    # Перевірити наявність необхідних утиліт
    for TOOL in java gradle cmake ninja-build; do
        if ! command -v $TOOL &> /dev/null; then
            MISSING_TOOLS+=("$TOOL")
        fi
    done
    
    if [ ${#MISSING_TOOLS[@]} -gt 0 ]; then
        log_error "Missing required tools: ${MISSING_TOOLS[*]}"
        log_info "Please install: ${MISSING_TOOLS[*]}"
        return 1
    fi
    
    log_success "All prerequisites found"
    return 0
}

# Функція для налаштування LLVM
setup_llvm() {
    local LLVM_PATH=${1:-/opt/llvm}
    
    log_info "Setting up LLVM from ${LLVM_PATH}..."
    
    if [ ! -f "${LLVM_PATH}/bin/clang" ]; then
        log_error "LLVM not found at ${LLVM_PATH}"
        log_info "Please run: ./scripts/manage-llvm-and-patches.sh install-llvm"
        return 1
    fi
    
    export LLVM_PATH
    export CC="${LLVM_PATH}/bin/clang"
    export CXX="${LLVM_PATH}/bin/clang++"
    export LD="${LLVM_PATH}/bin/ld.lld"
    
    log_success "LLVM configured"
    $CC --version | head -3
    
    return 0
}

# Функція для налаштування флагів оптимізації
setup_optimization_flags() {
    log_info "Configuring ARMv9 optimization flags..."
    
    # Core optimization flags
    export CFLAGS="-O3 -march=armv9-a -mtune=cortex-x4 -ffast-math -ftree-vectorize -funroll-loops"
    export CXXFLAGS="-O3 -march=armv9-a -mtune=cortex-x4 -ffast-math -ftree-vectorize -funroll-loops"
    
    # Linker optimizations
    export LDFLAGS="-Wl,--gc-sections -Wl,--as-needed -Wl,-O2"
    
    # LTO flags
    export LTO_FLAGS="-flto=full -fuse-linker-plugin"
    
    # CMake variables
    export CMAKE_BUILD_TYPE="Release"
    export CMAKE_CXX_FLAGS_RELEASE="-O3 -march=armv9-a+sve2 -ffast-math -ftree-vectorize -funroll-loops -flto=full"
    export CMAKE_C_FLAGS_RELEASE="-O3 -march=armv9-a+sve2 -ffast-math -ftree-vectorize -flto=full"
    
    # Android-specific
    export ANDROID_ARM_NEON="ON"
    export ANDROID_STL="c++_shared"
    export ANDROID_NATIVE_API_LEVEL="29"
    
    log_success "Optimization flags configured"
}

# Функція для завантаження і застосування патчів
apply_project_patches() {
    local PATCH_DIR="${SCRIPT_DIR}/../patches"
    
    log_info "Checking patches..."
    
    if [ ! -d "$PATCH_DIR" ]; then
        log_warning "No patches directory found"
        return 0
    fi
    
    local PATCH_COUNT=0
    for PATCH_FILE in "$PATCH_DIR"/*.patch; do
        if [ ! -f "$PATCH_FILE" ]; then
            continue
        fi
        
        local PATCH_NAME=$(basename "$PATCH_FILE")
        log_info "Checking patch: $PATCH_NAME"
        
        # Dry-run check
        if patch -p1 --dry-run < "$PATCH_FILE" > /dev/null 2>&1; then
            log_info "Applying: $PATCH_NAME"
            if patch -p1 < "$PATCH_FILE"; then
                log_success "Applied: $PATCH_NAME"
                ((PATCH_COUNT++))
            else
                log_warning "Failed to apply: $PATCH_NAME"
            fi
        else
            log_warning "Patch already applied or incompatible: $PATCH_NAME"
        fi
    done
    
    if [ $PATCH_COUNT -gt 0 ]; then
        log_success "$PATCH_COUNT patch(es) applied"
    fi
}

# Функція для очищення попередніх збірок
clean_build() {
    log_info "Cleaning previous builds..."
    
    cd "$SCRIPT_DIR/.."
    
    if [ -f "gradlew" ]; then
        chmod +x gradlew
        ./gradlew clean --no-daemon || true
    fi
    
    log_success "Build directory cleaned"
}

# Функція для збірки Release APK
build_release_apk() {
    log_info "Building Release APK with ARMv9 optimizations..."
    
    cd "$SCRIPT_DIR/.."
    
    chmod +x gradlew
    
    ./gradlew assembleRelease \
        -Pandroid.native.buildOutput=verbose \
        -DCMAKE_VERBOSE_MAKEFILE=ON \
        -Dorg.gradle.parallel=true \
        -Dorg.gradle.workers.max=4 \
        -DCMAKE_BUILD_TYPE=Release \
        -DLLVM_PATH="${LLVM_PATH}" \
        --no-daemon \
        --stacktrace \
        --warning-mode all
    
    if [ $? -eq 0 ]; then
        log_success "Build completed successfully"
        return 0
    else
        log_error "Build failed"
        return 1
    fi
}

# Функція для вилучення .so файлів
extract_native_libraries() {
    log_info "Extracting native libraries..."
    
    cd "$SCRIPT_DIR/.."
    
    local APK_PATH="app/build/outputs/apk/release/app-release.apk"
    local EXTRACT_DIR="native-libs-armv9"
    
    if [ ! -f "$APK_PATH" ]; then
        log_error "APK not found: $APK_PATH"
        return 1
    fi
    
    mkdir -p "$EXTRACT_DIR"
    
    # Extract .so files
    unzip -o "$APK_PATH" "lib/arm64-v8a/*.so" -d "$EXTRACT_DIR" || true
    
    # Reorganize files
    if [ -d "$EXTRACT_DIR/lib/arm64-v8a" ]; then
        mv "$EXTRACT_DIR/lib/arm64-v8a"/*.so "$EXTRACT_DIR/" 2>/dev/null || true
        rm -rf "$EXTRACT_DIR/lib"
    fi
    
    # List extracted files
    log_success "Extracted .so files:"
    ls -lh "$EXTRACT_DIR"/*.so 2>/dev/null | awk '{print "  " $9, "(" $5 ")"}'
    
    return 0
}

# Функція для генерування звіту про збірку
generate_build_report() {
    log_info "Generating build report..."
    
    cd "$SCRIPT_DIR/.."
    
    local REPORT_FILE="build-report.txt"
    local BUILD_TIME=$(date -u '+%Y-%m-%d %H:%M:%S UTC')
    
    cat > "$REPORT_FILE" << EOF
RPCSX ARMv9 Build Report
=======================
Build Date: ${BUILD_TIME}
Build Type: Release

Build Environment:
- OS: $(uname -s)
- Architecture: $(uname -m)
- Kernel: $(uname -r)

Compiler Information:
- LLVM: $(${LLVM_PATH}/bin/clang --version | head -1)
- Java: $(java -version 2>&1 | head -1)

Build Configuration:
- API Level: 36
- Min SDK: 29
- Target SDK: 35
- NDK Version: 29.0.13113456
- ABI: arm64-v8a (ARMv9)

Optimization Flags:
- CFLAGS: ${CFLAGS}
- CXXFLAGS: ${CXXFLAGS}
- LDFLAGS: ${LDFLAGS}
- LTO: ${LTO_FLAGS}

Output Files:
EOF
    
    local APK_PATH="app/build/outputs/apk/release/app-release.apk"
    if [ -f "$APK_PATH" ]; then
        local APK_SIZE=$(du -h "$APK_PATH" | cut -f1)
        echo "- APK: $(basename "$APK_PATH") (${APK_SIZE})" >> "$REPORT_FILE"
        
        # Generate checksums
        sha256sum "$APK_PATH" > "${APK_PATH}.sha256"
        md5sum "$APK_PATH" > "${APK_PATH}.md5"
        echo "- SHA256: $(cut -d' ' -f1 "${APK_PATH}.sha256")" >> "$REPORT_FILE"
    fi
    
    local NATIVE_LIBS_DIR="native-libs-armv9"
    if [ -d "$NATIVE_LIBS_DIR" ]; then
        local SO_COUNT=$(ls -1 "$NATIVE_LIBS_DIR"/*.so 2>/dev/null | wc -l)
        local SO_SIZE=$(du -sh "$NATIVE_LIBS_DIR" | cut -f1)
        echo "- Native Libraries: ${SO_COUNT} files (${SO_SIZE})" >> "$REPORT_FILE"
    fi
    
    cat >> "$REPORT_FILE" << EOF

Target Device: Snapdragon 8s Gen 3 and newer
Optimization Features:
- ARMv9-A Architecture
- SVE2 Vector Extension Support
- Fastmem Mapper
- FSR 3.1 Upscaling
- Game Mode Integration
- LTO (Link Time Optimization)

Build Status: SUCCESS
EOF
    
    log_success "Report generated: $REPORT_FILE"
    cat "$REPORT_FILE"
}

# Функція для виведення допомоги
show_help() {
    cat << EOF
Usage: $(basename "$0") [OPTIONS]

Build RPCSX ARMv9 with full optimizations

Options:
  --clean                Clean build directory before building
  --llvm-path <PATH>    Custom LLVM path (default: /opt/llvm)
  --skip-patches        Skip patch application
  --help                Show this help message

Examples:
  # Build with default LLVM
  $(basename "$0")
  
  # Clean build with custom LLVM
  $(basename "$0") --clean --llvm-path /opt/llvm-19
  
  # Build without patches
  $(basename "$0") --skip-patches

Environment:
  LLVM_PATH             Custom LLVM installation path
  ANDROID_HOME          Android SDK home directory

EOF
}

# Main script logic
main() {
    log_info "Starting RPCSX ARMv9 build..."
    
    # Parse arguments
    local LLVM_PATH="/opt/llvm"
    local SKIP_PATCHES=false
    local DO_CLEAN=false
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --llvm-path)
                LLVM_PATH="$2"
                shift 2
                ;;
            --skip-patches)
                SKIP_PATCHES=true
                shift
                ;;
            --clean)
                DO_CLEAN=true
                shift
                ;;
            --help|-h)
                show_help
                exit 0
                ;;
            *)
                log_error "Unknown option: $1"
                show_help
                exit 1
                ;;
        esac
    done
    
    # Step 1: Check prerequisites
    if ! check_prerequisites; then
        exit 1
    fi
    
    # Step 2: Setup LLVM
    if ! setup_llvm "$LLVM_PATH"; then
        exit 1
    fi
    
    # Step 3: Setup optimization flags
    setup_optimization_flags
    
    # Step 4: Clean if requested
    if [ "$DO_CLEAN" = true ]; then
        clean_build
    fi
    
    # Step 5: Apply patches
    if [ "$SKIP_PATCHES" = false ]; then
        apply_project_patches
    else
        log_warning "Skipping patches"
    fi
    
    # Step 6: Build
    if ! build_release_apk; then
        log_error "Build failed!"
        exit 1
    fi
    
    # Step 7: Extract libraries
    if ! extract_native_libraries; then
        log_warning "Could not extract native libraries"
    fi
    
    # Step 8: Generate report
    generate_build_report
    
    log_success "Build completed successfully!"
    echo ""
    echo "Next steps:"
    echo "1. APK: app/build/outputs/apk/release/app-release.apk"
    echo "2. Native Libraries: native-libs-armv9/"
    echo "3. Report: build-report.txt"
    echo ""
    echo "To install on device:"
    echo "  adb install -r app/build/outputs/apk/release/app-release.apk"
}

# Run main function
main "$@"
