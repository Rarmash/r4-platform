package com.rarmash.r4.agent.linux

import com.rarmash.r4.agent.linux.command.CommandDispatcher
import com.rarmash.r4.agent.linux.command.EchoCommandHandler
import com.rarmash.r4.agent.linux.command.SystemInfoCommandHandler
import com.rarmash.r4.agent.linux.config.AgentConfiguration
import com.rarmash.r4.agent.linux.hub.HttpHubClient
import com.rarmash.r4.agent.linux.identity.PersistentAgentIdentity
import com.rarmash.r4.agent.linux.lifecycle.LinuxAgent
import com.rarmash.r4.agent.linux.system.DefaultSystemInfoCollector
import java.util.concurrent.CountDownLatch

fun main() {
    val configuration = AgentConfiguration.fromEnvironment()
    val identity = PersistentAgentIdentity(configuration.dataDir)
    val dispatcher = CommandDispatcher(
        listOf(
            EchoCommandHandler(),
            SystemInfoCommandHandler(DefaultSystemInfoCollector())
        )
    )

    val agent = LinuxAgent(
        configuration = configuration,
        identity = identity,
        hubClient = HttpHubClient(configuration),
        commandDispatcher = dispatcher
    )

    val stopSignal = CountDownLatch(1)
    Runtime.getRuntime().addShutdownHook(
        Thread {
            agent.stop()
            stopSignal.countDown()
        }
    )

    agent.start()
    stopSignal.await()
}
