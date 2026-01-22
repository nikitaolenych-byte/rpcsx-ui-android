#!/bin/bash

# Script: manage-llvm-and-patches.sh
# Управління версіями LLVM та патчами для RPCSX ARMv9 збірки

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Кольори для виводу
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Функція для логування
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

# Функція для отримання останної версії LLVM
get_latest_llvm_version() {
    log_info "Fetching latest LLVM version..."
    
    LATEST=$(curl -s https://api.github.com/repos/llvm/llvm-project/releases | \
        grep '"tag_name":' | \
        grep -oE 'llvmorg-[0-9]+\.[0-9]+\.[0-9]+' | \
        head -1 | \
        sed 's/llvmorg-//')
    
    if [ -z "$LATEST" ]; then
        log_warning "Could not fetch latest version, defaulting to 19.1.0"
        echo "19.1.0"
    else
        echo "$LATEST"
    fi
}

# Функція для завантаження та встановлення LLVM
install_llvm() {
    local VERSION=$1
    local INSTALL_PATH=${2:-/opt/llvm}
    
    log_info "Installing LLVM ${VERSION}..."
    
    # Проверка наличия LLVM уже установленной
    if [ -f "${INSTALL_PATH}/bin/clang" ]; then
        CURRENT_VERSION=$(${INSTALL_PATH}/bin/clang --version | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
        if [ "$CURRENT_VERSION" = "$VERSION" ]; then
            log_success "LLVM ${VERSION} is already installed"
            return 0
        fi
    fi
    
    # URL для завантаження
    DOWNLOAD_URL="https://github.com/llvm/llvm-project/releases/download/llvmorg-${VERSION}/clang+llvm-${VERSION}-x86_64-linux-gnu-ubuntu-22.04.tar.xz"
    
    # Створити тимчасову директорію
    TEMP_DIR=$(mktemp -d)
    cd "$TEMP_DIR"
    
    log_info "Downloading LLVM ${VERSION} from ${DOWNLOAD_URL}..."
    
    if ! wget -q --show-progress "${DOWNLOAD_URL}" -O llvm.tar.xz 2>/dev/null; then
        log_error "Failed to download LLVM ${VERSION}"
        rm -rf "$TEMP_DIR"
        return 1
    fi
    
    log_info "Extracting LLVM..."
    mkdir -p "$INSTALL_PATH"
    tar -xf llvm.tar.xz -C "$INSTALL_PATH" --strip-components=1
    
    # Перевірка успішного встановлення
    if [ -f "${INSTALL_PATH}/bin/clang" ]; then
        log_success "LLVM ${VERSION} installed successfully"
        INSTALLED_VERSION=$(${INSTALL_PATH}/bin/clang --version | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
        echo "${INSTALL_PATH}/bin/clang --version:"
        ${INSTALL_PATH}/bin/clang --version
    else
        log_error "LLVM installation verification failed"
        rm -rf "$TEMP_DIR"
        return 1
    fi
    
    # Очищення
    rm -rf "$TEMP_DIR"
    cd "$SCRIPT_DIR"
    
    return 0
}

# Функція для завантаження патчів
download_patches() {
    local PATCH_DIR="${SCRIPT_DIR}/patches"
    
    log_info "Checking and downloading patches..."
    
    mkdir -p "$PATCH_DIR"
    
    # Завантажити patch.yml з віддаленого сховища (якщо доступний)
    # Це приклад - замініть на реальну URL
    local PATCH_SOURCES=(
        "https://raw.githubusercontent.com/RPCSX/rpcsx-nce-arm64-jit/master/rpcsx-nce-arm64-jit.patch"
    )
    
    for PATCH_URL in "${PATCH_SOURCES[@]}"; do
        PATCH_FILE="${PATCH_DIR}/$(basename "$PATCH_URL")"
        
        if [ -f "$PATCH_FILE" ]; then
            log_info "Patch file exists: $PATCH_FILE"
            log_info "Checking if update is available..."
        fi
        
        # Спробувати завантажити патч
        if curl -s -f -o "${PATCH_FILE}.new" "$PATCH_URL" 2>/dev/null; then
            if [ -f "$PATCH_FILE" ]; then
                # Порівняти з існуючим файлом
                if ! diff -q "$PATCH_FILE" "${PATCH_FILE}.new" > /dev/null 2>&1; then
                    log_warning "Patch has been updated"
                    mv "${PATCH_FILE}.new" "$PATCH_FILE"
                    log_success "Patch updated: $PATCH_FILE"
                else
                    log_info "Patch is up to date"
                    rm -f "${PATCH_FILE}.new"
                fi
            else
                mv "${PATCH_FILE}.new" "$PATCH_FILE"
                log_success "New patch downloaded: $PATCH_FILE"
            fi
        else
            log_warning "Could not download patch from: $PATCH_URL"
            rm -f "${PATCH_FILE}.new" 2>/dev/null || true
        fi
    done
}

# Функція для застосування патчів
apply_patches() {
    local PATCH_DIR="${SCRIPT_DIR}/patches"
    local PATCH_COUNT=0
    
    if [ ! -d "$PATCH_DIR" ]; then
        log_warning "No patches directory found"
        return 0
    fi
    
    log_info "Applying patches from ${PATCH_DIR}..."
    
    for PATCH_FILE in "$PATCH_DIR"/*.patch; do
        if [ ! -f "$PATCH_FILE" ]; then
            continue
        fi
        
        PATCH_NAME=$(basename "$PATCH_FILE")
        log_info "Processing: $PATCH_NAME"
        
        # Dry-run для перевірки
        if patch -p1 --dry-run < "$PATCH_FILE" > /dev/null 2>&1; then
            log_info "Applying patch: $PATCH_NAME"
            if patch -p1 < "$PATCH_FILE"; then
                log_success "Patch applied: $PATCH_NAME"
                ((PATCH_COUNT++))
            else
                log_warning "Failed to apply patch: $PATCH_NAME"
            fi
        else
            log_warning "Patch check failed (may already be applied): $PATCH_NAME"
        fi
    done
    
    if [ $PATCH_COUNT -gt 0 ]; then
        log_success "$PATCH_COUNT patch(es) applied"
    else
        log_info "No patches were applied"
    fi
}

# Функція для валідації LLVM
validate_llvm() {
    local LLVM_PATH=${1:-/opt/llvm}
    
    if [ ! -f "${LLVM_PATH}/bin/clang" ]; then
        log_error "LLVM not found at ${LLVM_PATH}"
        return 1
    fi
    
    log_info "LLVM Information:"
    ${LLVM_PATH}/bin/clang --version
    
    # Перевірка наявності необхідних компонентів
    local REQUIRED_TOOLS=("clang" "clang++" "llvm-config" "llc")
    local ALL_FOUND=true
    
    for TOOL in "${REQUIRED_TOOLS[@]}"; do
        if [ ! -f "${LLVM_PATH}/bin/${TOOL}" ]; then
            log_warning "Missing: ${TOOL}"
            ALL_FOUND=false
        fi
    done
    
    if [ "$ALL_FOUND" = true ]; then
        log_success "All required LLVM tools found"
        return 0
    else
        log_warning "Some LLVM tools are missing"
        return 1
    fi
}

# Функція для формування звіту
generate_report() {
    local REPORT_FILE="${SCRIPT_DIR}/build-environment-report.txt"
    
    log_info "Generating environment report..."
    
    cat > "$REPORT_FILE" << EOF
RPCSX ARMv9 Build Environment Report
====================================
Generated: $(date -u '+%Y-%m-%d %H:%M:%S UTC')

System Information:
- OS: $(uname -s)
- Architecture: $(uname -m)
- Kernel: $(uname -r)

LLVM Configuration:
- LLVM Path: /opt/llvm
EOF
    
    if [ -f "/opt/llvm/bin/clang" ]; then
        cat >> "$REPORT_FILE" << EOF
- LLVM Version: $(/opt/llvm/bin/clang --version | head -1)
- Clang: $(/opt/llvm/bin/clang --version | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
EOF
    fi
    
    cat >> "$REPORT_FILE" << EOF

Build Tools:
- CMake: $(cmake --version | head -1)
- Java: $(java -version 2>&1 | head -1)
- Gradle: $(gradle --version 2>/dev/null | head -1 || echo "Not found")

Android SDK:
- SDK Root: ${ANDROID_HOME:-Not set}
- NDK Version: 29.0.13113456

Patches:
EOF
    
    if [ -d "${SCRIPT_DIR}/patches" ]; then
        ls -1 "${SCRIPT_DIR}/patches"/*.patch 2>/dev/null | sed 's/^/- /' >> "$REPORT_FILE" || echo "- None" >> "$REPORT_FILE"
    else
        echo "- No patches directory" >> "$REPORT_FILE"
    fi
    
    log_success "Report generated: $REPORT_FILE"
    cat "$REPORT_FILE"
}

# Функція помощи
show_help() {
    cat << EOF
Usage: $(basename "$0") [COMMAND] [OPTIONS]

Commands:
  install-llvm [VERSION]      Install specific LLVM version (default: latest stable)
  list-llvm-versions          List available LLVM versions
  validate-llvm [PATH]        Validate LLVM installation
  download-patches            Download latest patches
  apply-patches               Apply patches to the project
  generate-report             Generate build environment report
  full-setup [VERSION]        Full setup (install LLVM, download & apply patches)
  help                        Show this help message

Options:
  --llvm-path <PATH>          Custom LLVM installation path (default: /opt/llvm)
  --patch-dir <PATH>          Custom patches directory (default: ./patches)
  
Examples:
  # Install latest LLVM
  $(basename "$0") install-llvm
  
  # Install specific LLVM version
  $(basename "$0") install-llvm 19.1.0
  
  # Full setup with latest LLVM
  $(basename "$0") full-setup
  
  # Validate existing LLVM
  $(basename "$0") validate-llvm /opt/llvm
  
  # Download and apply patches
  $(basename "$0") download-patches
  $(basename "$0") apply-patches

EOF
}

# Main script logic
main() {
    local COMMAND=$1
    shift || true
    
    case "$COMMAND" in
        install-llvm)
            local VERSION=${1:-$(get_latest_llvm_version)}
            install_llvm "$VERSION"
            ;;
        list-llvm-versions)
            log_info "Fetching available LLVM versions..."
            curl -s https://api.github.com/repos/llvm/llvm-project/releases | \
                grep '"tag_name":' | \
                grep -oE 'llvmorg-[0-9]+\.[0-9]+\.[0-9]+' | \
                sed 's/llvmorg-//' | \
                sort -V | \
                tail -20
            ;;
        validate-llvm)
            validate_llvm "${1:-/opt/llvm}"
            ;;
        download-patches)
            download_patches
            ;;
        apply-patches)
            apply_patches
            ;;
        generate-report)
            generate_report
            ;;
        full-setup)
            local VERSION=${1:-$(get_latest_llvm_version)}
            log_info "Starting full setup..."
            install_llvm "$VERSION"
            download_patches
            apply_patches
            validate_llvm
            generate_report
            log_success "Full setup completed!"
            ;;
        help|--help|-h)
            show_help
            ;;
        *)
            if [ -n "$COMMAND" ]; then
                log_error "Unknown command: $COMMAND"
                echo ""
            fi
            show_help
            exit 1
            ;;
    esac
}

# Run main function
main "$@"
