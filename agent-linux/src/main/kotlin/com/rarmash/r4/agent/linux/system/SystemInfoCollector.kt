package com.rarmash.r4.agent.linux.system

fun interface SystemInfoCollector {
    fun collect(): SystemInfo
}
