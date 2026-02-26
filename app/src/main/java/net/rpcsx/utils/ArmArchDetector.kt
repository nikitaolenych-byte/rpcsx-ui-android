package net.rpcsx.utils

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

            // CONSERVATIVE: Only trust actual CPU feature flags from /proc/cpuinfo.
            // Do NOT guess based on SoC names or CPU part numbers —
            // the kernel may not expose SVE2 even on ARMv9 hardware,
            // and loading an ARMv9 library will cause a native crash (SIGILL)
            // that cannot be caught by Java try-catch.

            // SVE2 is the defining mandatory feature of ARMv9
            if ("sve2" in features) {
                val subArch = if ("sme" in features || "sme2" in features) "armv9.2-a"
                else if ("bf16" in features && "i8mm" in features) "armv9.1-a"
                else "armv9-a"
                return DetectionResult(
                    arch = subArch,
                    reason = "Detected SVE2 in /proc/cpuinfo",
                    supportsArm9 = true,
                    features = features
                )
            }

            // SVE alone is NOT enough for ARMv9 — some ARMv8.6+ chips have SVE
            // but the library may use SVE2 instructions. Stay on ARMv8.

            // Determine ARMv8 sub-level based on features
            val archLevel = detectArmv8Level(features)
            return DetectionResult(
                arch = archLevel,
                reason = "ARMv8 features detected (sve2 not in cpuinfo)",
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

    /**
     * Force ARMv8 detection only (used after ARMv9 library crash).
     * Returns the best ARMv8 sub-level without considering ARMv9.
     */
    fun detectArmv8Only(): String {
        return try {
            val features = parseCpuFeatures(readCpuInfo())
            detectArmv8Level(features)
        } catch (e: Throwable) {
            "armv8-a"
        }
    }

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
