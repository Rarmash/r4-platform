package com.rarmash.r4.agent.linux.system

import java.lang.management.ManagementFactory
import java.net.InetAddress
import java.nio.file.Files
import java.nio.file.Path

class DefaultSystemInfoCollector(
    private val agentVersion: String = "0.1.0",
    private val osReleasePath: Path = Path.of("/etc/os-release"),
    private val memInfoPath: Path = Path.of("/proc/meminfo"),
    private val uptimePath: Path = Path.of("/proc/uptime")
) : SystemInfoCollector {

    override fun collect(): SystemInfo {
        val osRelease = readOsRelease()

        return SystemInfo(
            hostname = hostname(),
            osName = osRelease["PRETTY_NAME"]
                ?: osRelease["NAME"]
                ?: System.getProperty("os.name"),
            osVersion = osRelease["VERSION_ID"]
                ?: System.getProperty("os.version"),
            kernelVersion = System.getProperty("os.version"),
            architecture = System.getProperty("os.arch"),
            availableProcessors = Runtime.getRuntime().availableProcessors(),
            totalMemoryBytes = totalMemoryBytes(),
            uptimeSeconds = uptimeSeconds(),
            agentVersion = agentVersion
        )
    }

    private fun hostname(): String {
        return runCatching { InetAddress.getLocalHost().hostName }
            .getOrDefault("unknown")
    }

    private fun readOsRelease(): Map<String, String> {
        if (!Files.isRegularFile(osReleasePath)) {
            return emptyMap()
        }

        return Files.readAllLines(osReleasePath)
            .asSequence()
            .mapNotNull { line ->
                val delimiter = line.indexOf('=')
                if (delimiter <= 0) {
                    null
                } else {
                    val key = line.substring(0, delimiter)
                    val value = line.substring(delimiter + 1)
                        .trim()
                        .removeSurrounding("\"")
                    key to value
                }
            }
            .toMap()
    }

    private fun totalMemoryBytes(): Long? {
        if (Files.isRegularFile(memInfoPath)) {
            return Files.readAllLines(memInfoPath)
                .firstOrNull { it.startsWith("MemTotal:") }
                ?.split(Regex("\\s+"))
                ?.getOrNull(1)
                ?.toLongOrNull()
                ?.times(1024)
        }

        return runCatching {
            (ManagementFactory.getOperatingSystemMXBean() as com.sun.management.OperatingSystemMXBean)
                .totalMemorySize
        }.getOrNull()
    }

    private fun uptimeSeconds(): Long? {
        if (!Files.isRegularFile(uptimePath)) {
            return ManagementFactory.getRuntimeMXBean().uptime / 1000
        }

        return Files.readString(uptimePath)
            .trim()
            .substringBefore(' ')
            .substringBefore('.')
            .toLongOrNull()
    }
}
