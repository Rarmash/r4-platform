package com.rarmash.r4.agent.linux.lifecycle

import com.rarmash.r4.agent.linux.command.CommandDispatcher
import com.rarmash.r4.agent.linux.config.AgentConfiguration
import com.rarmash.r4.agent.linux.hub.HubClient
import com.rarmash.r4.agent.linux.hub.HubHttpException
import com.rarmash.r4.agent.linux.identity.AgentIdentity
import com.rarmash.r4.protocol.command.CommandResponse
import com.rarmash.r4.protocol.device.RegisterDeviceRequest
import org.slf4j.LoggerFactory
import java.time.Duration
import java.util.Collections
import java.util.UUID
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledExecutorService
import java.util.concurrent.TimeUnit
import kotlin.math.min

class LinuxAgent(
    private val configuration: AgentConfiguration,
    private val identity: AgentIdentity,
    private val hubClient: HubClient,
    private val commandDispatcher: CommandDispatcher,
    private val hubBackoff: HubBackoff = HubBackoff(),
    private val scheduler: ScheduledExecutorService = Executors.newScheduledThreadPool(2)
) {

    private val logger = LoggerFactory.getLogger(javaClass)

    @Volatile
    private var deviceId: UUID? = null

    private val completedCommandIds = Collections.synchronizedSet(mutableSetOf<UUID>())

    fun start() {
        logger.info("Starting R4 Linux Agent with agentId={}", identity.agentId)
        registerIfNeeded()

        scheduler.scheduleWithFixedDelay(
            ::heartbeatSafely,
            configuration.heartbeatInterval.toMillis(),
            configuration.heartbeatInterval.toMillis(),
            TimeUnit.MILLISECONDS
        )
        scheduler.scheduleWithFixedDelay(
            ::pollCommandSafely,
            0,
            configuration.commandPollInterval.toMillis(),
            TimeUnit.MILLISECONDS
        )
    }

    fun stop() {
        logger.info("Stopping R4 Linux Agent with agentId={}", identity.agentId)
        scheduler.shutdown()
        if (!scheduler.awaitTermination(Duration.ofSeconds(5).toMillis(), TimeUnit.MILLISECONDS)) {
            scheduler.shutdownNow()
        }
    }

    internal fun registerIfNeeded(): UUID? {
        deviceId?.let {
            return it
        }

        if (!hubBackoff.canAttempt()) {
            return null
        }

        return runCatching {
            val response = hubClient.register(
                RegisterDeviceRequest(
                    agentId = identity.agentId,
                    name = configuration.agentName,
                    platform = platform(),
                    agentVersion = configuration.agentVersion,
                    capabilities = commandDispatcher.capabilities
                )
            )

            deviceId = response.id
            hubBackoff.recordSuccess()
            logger.info("Registered Linux agent with deviceId={}", response.id)
            response.id
        }.onFailure { exception ->
            val delay = hubBackoff.recordFailure()
            logger.warn(
                "Could not register Linux agent with agentId={}: {}; next attempt in {} ms",
                identity.agentId,
                exception.message,
                delay.toMillis()
            )
        }.getOrNull()
    }

    internal fun heartbeatSafely() {
        val currentDeviceId = registerIfNeeded() ?: return
        if (!hubBackoff.canAttempt()) {
            return
        }

        runCatching {
            hubClient.heartbeat(currentDeviceId)
        }.onSuccess { response ->
            hubBackoff.recordSuccess()
            logger.info("Heartbeat sent for deviceId={}, lastSeenAt={}", currentDeviceId, response.lastSeenAt)
        }.onFailure { exception ->
            if (exception is HubHttpException && exception.statusCode == 404) {
                deviceId = null
            }

            val delay = hubBackoff.recordFailure()
            logger.warn(
                "Could not send heartbeat for agentId={}, deviceId={}: {}; next attempt in {} ms",
                identity.agentId,
                currentDeviceId,
                exception.message,
                delay.toMillis()
            )
        }
    }

    internal fun pollCommandSafely() {
        val currentDeviceId = registerIfNeeded() ?: return
        if (!hubBackoff.canAttempt()) {
            return
        }

        runCatching {
            hubClient.fetchNextCommand(currentDeviceId)
        }.onSuccess { command ->
            hubBackoff.recordSuccess()
            if (command != null) {
                executeCommand(currentDeviceId, command)
            }
        }.onFailure { exception ->
            val delay = hubBackoff.recordFailure()
            logger.warn(
                "Could not fetch command for agentId={}, deviceId={}: {}; next attempt in {} ms",
                identity.agentId,
                currentDeviceId,
                exception.message,
                delay.toMillis()
            )
        }
    }

    private fun executeCommand(deviceId: UUID, command: CommandResponse) {
        if (command.id in completedCommandIds) {
            logger.info("Command {} was already completed by this process, skipping duplicate", command.id)
            return
        }

        logger.info(
            "Executing command {}, type={}, attempt={}",
            command.id,
            command.type,
            command.attemptCount
        )

        val result = commandDispatcher.dispatch(command)

        runCatching {
            hubClient.completeCommand(
                deviceId = deviceId,
                command = command,
                success = result.success,
                result = result.result?.take(MAX_RESULT_LENGTH),
                error = result.error?.take(MAX_RESULT_LENGTH)
            )
        }.onSuccess {
            completedCommandIds.add(command.id)
            hubBackoff.recordSuccess()
            logger.info("Command {} completed, success={}", command.id, result.success)
        }.onFailure { exception ->
            val delay = hubBackoff.recordFailure()
            logger.warn(
                "Could not complete command {} for agentId={}: {}; next Hub attempt in {} ms",
                command.id,
                identity.agentId,
                exception.message,
                delay.toMillis()
            )
        }
    }

    private fun String.take(maxLength: Int): String {
        return substring(0, min(length, maxLength))
    }

    private fun platform(): String {
        return "${System.getProperty("os.name")}-${System.getProperty("os.arch")}"
            .lowercase()
            .replace(Regex("\\s+"), "-")
    }

    companion object {
        private const val MAX_RESULT_LENGTH = 10_000
    }
}
