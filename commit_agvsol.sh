#!/bin/bash
cd /workspaces/rpcsx-ui-android
git add -A
git commit -m "Add AGVSOL - Automatic GPU Vendor-Specific Optimization Layer

Features:
- GPU auto-detection for Adreno/Mali/PowerVR GPUs
- Vendor-specific optimization profiles with tier-based settings
- Qualcomm Adreno support (610-830) with FlexRender/UBWC
- ARM Mali support (G51-Immortalis G925) with AFBC
- PowerVR support with TBDR/HSR optimizations
- Vendor-specific GLSL 450 shaders
- JSON profile system with tier overrides
- Compose UI for GPU settings
- JNI bridge with 18 native functions"
echo "Commit complete!"
