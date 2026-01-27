package net.rpcsx.utils

/**
 * Helpers to normalise CPU display names into native LLVM CPU tokens
 */
fun mapCpuPartToName(partHex: String): String? {
    return when (partHex.lowercase().removePrefix("0x")) {
        "d03" -> "Cortex-A53"
        "d04" -> "Cortex-A35"
        "d05" -> "Cortex-A55"
        "d07" -> "Cortex-A57"
        "d08" -> "Cortex-A72"
        "d09" -> "Cortex-A73"
        "d0a" -> "Cortex-A75"
        "d0b" -> "Cortex-A76"
        "d0d" -> "Cortex-A77"
        "d0e" -> "Cortex-A78"
        "d40" -> "Cortex-A78C"
        "d41" -> "Neoverse-N1"
        "c0f" -> "Cortex-X1"
        "d4a" -> "Cortex-X2"
        "d4b" -> "Cortex-A710"
        "d4c" -> "Cortex-A715"
        else -> null
    }
}

fun displayToCpuToken(label: String): String {
    // If label already contains a Cortex-like name, normalise to e.g. "cortex-x1"
    val cortexRegex = Regex("(?i)(cortex[- ]?[xA-Za-z0-9]+|neoverse[- ]?[A-Za-z0-9]+)")
    cortexRegex.find(label)?.let { m ->
        return m.value.replace(" ", "-").lowercase()
    }

    // If label contains an explicit CPU# like CPU0
    val cpuNumRegex = Regex("CPU(\\d+)", RegexOption.IGNORE_CASE)
    cpuNumRegex.find(label)?.let { m ->
        return "cpu${m.groupValues[1]}"
    }

    // If it's a cluster/frequency fallback (e.g. CoreCluster@3014 MHz x4), convert to cluster token
    val mhzRegex = Regex("(\\d{3,5})\\s*MHz", RegexOption.IGNORE_CASE)
    mhzRegex.find(label)?.let { m ->
        val mhz = m.groupValues[1]
        return "cluster-${mhz}mhz"
    }

    // Generic fallback: strip counts, replace non-alnum with '-', lowercase
    return label.replace(Regex("\\s+x\\d+$"), "")
        .replace(Regex("[^A-Za-z0-9_-]"), "-")
        .trim('-')
        .lowercase()
}
