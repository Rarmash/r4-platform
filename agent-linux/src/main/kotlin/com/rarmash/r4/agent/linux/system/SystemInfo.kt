package com.rarmash.r4.agent.linux.system

data class SystemInfo(
    val hostname: String,
    val osName: String,
    val osVersion: String?,
    val kernelVersion: String,
    val architecture: String,
    val availableProcessors: Int,
    val totalMemoryBytes: Long?,
    val uptimeSeconds: Long?,
    val agentVersion: String
)
