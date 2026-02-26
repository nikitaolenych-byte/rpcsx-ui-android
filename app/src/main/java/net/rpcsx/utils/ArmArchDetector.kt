package net.rpcsx.utils

import android.os.Build
import android.util.Log
import java.io.File

/**
 * Auto-detects the best ARM architecture level for the device.
 * Checks CPU features from /proc/cpuinfo and known SoC mappings.
 * 
 * ARMv9 features: SVE, SVE2, BF16, I8MM, etc.
 * ARMv9 cores: Cortex-X3, X4, A715, A720, etc.
 */
object ArmArchDetector {

    private const val TAG = "ArmArchDetector"

    data class DetectionResult(
        val arch: String,           // e.g. "armv9-a", "armv8.2-a"
        val reason: String,         // human-readable reason
        val supportsArm9: Boolean,
        val features: Set<String>   // detected CPU features
    )

    /**
     * Detect the best architecture for this device and return it.
     * Safe to call from any thread.
     */
    fun detect(): DetectionResult {
        try {
            val cpuInfo = readCpuInfo()
            val features = parseCpuFeatures(cpuInfo)
            val cpuParts = parseCpuParts(cpuInfo)
            val implementers = parseCpuImplementers(cpuInfo)

            // Check for ARMv9 indicators
            // SVE2 is the defining feature of ARMv9
            if ("sve2" in features) {
                val subArch = if ("sme" in features || "sme2" in features) "armv9.2-a"
                else if ("bf16" in features && "i8mm" in features) "armv9.1-a"
                else "armv9-a"
                return DetectionResult(
                    arch = subArch,
                    reason = "Detected SVE2 feature (ARMv9 mandatory)",
                    supportsArm9 = true,
                    features = features
                )
            }

            // Check for SVE (ARMv9 precursor, some ARMv8.6+ also have it)
            if ("sve" in features) {
                return DetectionResult(
                    arch = "armv9-a",
                    reason = "Detected SVE feature",
                    supportsArm9 = true,
                    features = features
                )
            }

            // Check known ARMv9 CPU part numbers
            val armv9Parts = setOf(
                "d46", // Cortex-A510 (ARMv9)
                "d47", // Cortex-A710 (ARMv9)
                "d48", // Cortex-X2 (ARMv9)
                "d4d", // Cortex-A715 (ARMv9)
                "d4e", // Cortex-X3 (ARMv9)
                "d4f", // Neoverse-V2 (ARMv9)
                "d80", // Cortex-A520 (ARMv9.2)
                "d81", // Cortex-A720 (ARMv9.2)
                "d82", // Cortex-X4 (ARMv9.2)
                "d84", // Neoverse-V3 (ARMv9.2)
                "d85", // Cortex-X925 (ARMv9.2)
                "d87", // Cortex-A725 (ARMv9.2)
                "d89", // Cortex-A520AE (ARMv9.2)
            )

            for (part in cpuParts) {
                val normalized = part.lowercase().removePrefix("0x")
                if (normalized in armv9Parts) {
                    return DetectionResult(
                        arch = "armv9-a",
                        reason = "Detected ARMv9 CPU part: 0x$normalized",
                        supportsArm9 = true,
                        features = features
                    )
                }
            }

            // Check known ARMv9 SoC names from Build.HARDWARE / Build.SOC_MODEL
            if (isKnownArmv9Soc()) {
                return DetectionResult(
                    arch = "armv9-a",
                    reason = "Known ARMv9 SoC: ${getSocInfo()}",
                    supportsArm9 = true,
                    features = features
                )
            }

            // Determine ARMv8 sub-level based on features
            val archLevel = detectArmv8Level(features)
            return DetectionResult(
                arch = archLevel,
                reason = "ARMv8 features detected",
                supportsArm9 = false,
                features = features
            )
        } catch (e: Throwable) {
            Log.e(TAG, "Detection failed, defaulting to armv8-a", e)
            return DetectionResult(
                arch = "armv8-a",
                reason = "Detection failed: ${e.message}",
                supportsArm9 = false,
                features = emptySet()
            )
        }
    }

    /**
     * Quick check: does this device support ARMv9?
     */
    fun supportsArmv9(): Boolean = detect().supportsArm9

    /**
     * Get the best arch string for downloads.
     */
    fun getBestArch(): String = detect().arch

    // ---- private implementation ----

    private fun readCpuInfo(): String {
        return try {
            File("/proc/cpuinfo").readText()
        } catch (e: Throwable) {
            Log.w(TAG, "Cannot read /proc/cpuinfo: ${e.message}")
            ""
        }
    }

    private fun parseCpuFeatures(cpuInfo: String): Set<String> {
        val features = mutableSetOf<String>()
        for (line in cpuInfo.lines()) {
            if (line.startsWith("Features", ignoreCase = true)) {
                val parts = line.substringAfter(":").trim().split("\\s+".toRegex())
                features.addAll(parts.map { it.lowercase() })
            }
        }
        return features
    }

    private fun parseCpuParts(cpuInfo: String): Set<String> {
        val parts = mutableSetOf<String>()
        for (line in cpuInfo.lines()) {
            if (line.startsWith("CPU part", ignoreCase = true)) {
                val value = line.substringAfter(":").trim().lowercase().removePrefix("0x")
                parts.add(value)
            }
        }
        return parts
    }

    private fun parseCpuImplementers(cpuInfo: String): Set<String> {
        val impls = mutableSetOf<String>()
        for (line in cpuInfo.lines()) {
            if (line.startsWith("CPU implementer", ignoreCase = true)) {
                val value = line.substringAfter(":").trim().lowercase()
                impls.add(value)
            }
        }
        return impls
    }

    private fun isKnownArmv9Soc(): Boolean {
        val socModel = try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                Build.SOC_MODEL.lowercase()
            } else ""
        } catch (_: Throwable) { "" }

        val hardware = Build.HARDWARE.lowercase()
        val board = Build.BOARD.lowercase()
        val combined = "$socModel $hardware $board"

        // Known ARMv9 SoCs (Snapdragon 8 Gen 1+, Dimensity 9000+, Exynos 2200+)
        val armv9Patterns = listOf(
            // Qualcomm Snapdragon 8 Gen 1 and newer
            "sm8450", "sm8475", "sm8550", "sm8650", "sm8750",  // SD 8 Gen 1/1+/2/3/4
            "sm7550", "sm7675",  // SD 7+ Gen 2, SD 7s Gen 3
            "taro", "kalama", "pineapple", "sun",  // Qualcomm codenames
            // MediaTek Dimensity 9000+
            "mt6983", "mt6985", "mt6989", "mt6990",  // D9000/9200/9300/9400
            // Samsung Exynos with ARMv9
            "exynos2200", "exynos2300", "exynos2400", "exynos2500",
            "s5e9925", "s5e9935", "s5e9945", "s5e9955",
            // Google Tensor G2+
            "gs201", "zuma", "zumapro",  // Tensor G2, G3, G4
        )

        for (pattern in armv9Patterns) {
            if (combined.contains(pattern)) return true
        }

        return false
    }

    private fun getSocInfo(): String {
        val parts = mutableListOf<String>()
        parts.add("HW=${Build.HARDWARE}")
        parts.add("Board=${Build.BOARD}")
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                parts.add("SoC=${Build.SOC_MODEL}")
            }
        } catch (_: Throwable) {}
        return parts.joinToString(", ")
    }

    private fun detectArmv8Level(features: Set<String>): String {
        // Check from highest to lowest
        // ARMv8.5-a: BTI, MTE (memory tagging)
        if ("bti" in features || "mte" in features) return "armv8.5-a"

        // ARMv8.4-a: FLAGM, DIT, TLB range
        if ("flagm" in features || "dit" in features) return "armv8.4-a"

        // ARMv8.2-a: FP16, DotProd, RAS
        if ("fphp" in features || "asimdhp" in features || "asimddp" in features) return "armv8.2-a"

        // ARMv8.1-a: atomics (LSE), RDMA
        if ("atomics" in features || "asimdrdm" in features) return "armv8.1-a"

        return "armv8-a"
    }
}
