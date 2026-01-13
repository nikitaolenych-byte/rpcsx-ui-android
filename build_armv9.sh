#!/bin/bash

# RPCSX ARMv9 Build Script для Snapdragon 8s Gen 3
# Автоматична збірка з максимальними оптимізаціями

set -e  # Вийти при помилці
# Change to script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"
echo "=================================================="
echo "RPCSX ARMv9 Build Script"
echo "Target: Snapdragon 8s Gen 3 (ARMv9+SVE2)"
echo "=================================================="

# Кольори для виводу
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Перевірка наявності необхідних інструментів
echo -e "${YELLOW}Checking prerequisites...${NC}"

if ! command -v adb &> /dev/null; then
    echo -e "${RED}Error: adb not found. Please install Android SDK Platform Tools.${NC}"
    exit 1
fi

if ! [ -d "$ANDROID_HOME" ]; then
    echo -e "${RED}Error: ANDROID_HOME not set. Please set Android SDK path.${NC}"
    exit 1
fi

# Перевірка NDK
NDK_VERSION="29.0.13113456"
NDK_PATH="$ANDROID_HOME/ndk/$NDK_VERSION"

if ! [ -d "$NDK_PATH" ]; then
    echo -e "${RED}Error: NDK $NDK_VERSION not found.${NC}"
    echo "Installing NDK..."
    sdkmanager "ndk;$NDK_VERSION"
fi

echo -e "${GREEN}✓ Prerequisites OK${NC}"

# Очищення попередніх збірок
echo -e "${YELLOW}Cleaning previous builds...${NC}"
./gradlew clean

# Налаштування змінних середовища для оптимізацій
export CMAKE_BUILD_TYPE=Release
export ANDROID_ARM_NEON=ON
export ANDROID_STL=c++_shared

# Встановлення версії
if [ -z "$RX_VERSION" ]; then
    export RX_VERSION="armv9-optimized-$(date +%Y%m%d)"
fi

if [ -z "$RX_SHA" ]; then
    export RX_SHA=$(git rev-parse --short HEAD)
fi

echo -e "${YELLOW}Building RPCSX ARMv9 Optimized Edition${NC}"
echo "Version: $RX_VERSION-$RX_SHA"
echo "Build type: Release (ARMv9+SVE2 optimizations)"

# Збірка з оптимізаціями
echo -e "${YELLOW}Starting Gradle build...${NC}"
./gradlew assembleRelease \
    -Pandroid.native.buildOutput=verbose \
    -DCMAKE_VERBOSE_MAKEFILE=ON

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Build successful!${NC}"
else
    echo -e "${RED}✗ Build failed!${NC}"
    exit 1
fi

# Знаходимо APK
APK_PATH="app/build/outputs/apk/release/rpcsx-release.apk"

if [ ! -f "$APK_PATH" ]; then
    echo -e "${RED}Error: APK not found at $APK_PATH${NC}"
    exit 1
fi

# Виводимо інформацію про APK
echo -e "${GREEN}=================================================="
echo "Build completed successfully!"
echo "=================================================="
APK_SIZE=$(du -h "$APK_PATH" | cut -f1)
echo "APK: $APK_PATH"
echo "Size: $APK_SIZE"
echo "Version: $RX_VERSION-$RX_SHA"
echo "Optimizations: ARMv9, SVE2, LTO, Fastmem, FSR 3.1"
echo "=================================================="${NC}

# Питаємо чи встановлювати на пристрій
read -p "Install on connected device? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo -e "${YELLOW}Installing on device...${NC}"
    
    # Перевіряємо підключений пристрій
    if ! adb devices | grep -q "device$"; then
        echo -e "${RED}Error: No device connected${NC}"
        exit 1
    fi
    
    # Перевіряємо архітектуру пристрою
    DEVICE_ABI=$(adb shell getprop ro.product.cpu.abi)
    echo "Device ABI: $DEVICE_ABI"
    
    if [[ "$DEVICE_ABI" != "arm64-v8a" ]]; then
        echo -e "${RED}Warning: Device is not ARM64. This build is optimized for ARM64 only.${NC}"
        read -p "Continue anyway? (y/n) " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 0
        fi
    fi
    
    # Видаляємо стару версію
    adb uninstall net.rpcsx 2>/dev/null || true
    
    # Встановлюємо нову
    adb install -r "$APK_PATH"
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Installation successful!${NC}"
        
        # Виводимо інформацію про SoC пристрою
        SOC_MODEL=$(adb shell getprop ro.soc.model 2>/dev/null || echo "Unknown")
        echo "Device SoC: $SOC_MODEL"
        
        if [[ "$SOC_MODEL" == *"SM8635"* ]] || [[ "$SOC_MODEL" == *"8s Gen 3"* ]]; then
            echo -e "${GREEN}✓ Perfect! Device has Snapdragon 8s Gen 3 - all optimizations will work!${NC}"
        else
            echo -e "${YELLOW}⚠ Warning: Device SoC is not Snapdragon 8s Gen 3.${NC}"
            echo "Some ARMv9/SVE2 optimizations may not be available."
        fi
    else
        echo -e "${RED}✗ Installation failed!${NC}"
        exit 1
    fi
fi

# Генерація checksum
echo -e "${YELLOW}Generating checksums...${NC}"
sha256sum "$APK_PATH" > "${APK_PATH}.sha256"
echo -e "${GREEN}✓ Checksum saved to ${APK_PATH}.sha256${NC}"

echo ""
echo -e "${GREEN}All done! 🎮${NC}"
echo ""
echo "Next steps:"
echo "1. Launch RPCSX on your device"
echo "2. Install PS3 firmware"
echo "3. Load a game and enjoy 30-60 FPS!"
echo ""
echo "For optimal performance on Garden Warfare:"
echo "- Set resolution to 720p"
echo "- Enable FSR 3.1 upscaling"
echo "- Target 60 FPS with Frostbite hacks"
