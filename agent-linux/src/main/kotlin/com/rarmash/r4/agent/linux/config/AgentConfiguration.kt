package com.rarmash.r4.agent.linux.config

import java.net.URI
import java.nio.file.Path
import java.time.Duration

data class AgentConfiguration(
    val hubUrl: URI,
    val agentName: String,
    val dataDir: Path,
    val heartbeatInterval: Duration,
    val commandPollInterval: Duration,
    val httpConnectTimeout: Duration,
    val httpRequestTimeout: Duration,
    val agentVersion: String = "0.1.0"
) {
    companion object {
        fun fromEnvironment(environment: Map<String, String> = System.getenv()): AgentConfiguration {
            val hubUrl = environment["R4_HUB_URL"]
                ?.takeIf { it.isNotBlank() }
                ?: "http://localhost:8080"

            return AgentConfiguration(
                hubUrl = URI.create(hubUrl).normalize(),
                agentName = environment["R4_AGENT_NAME"]
                    ?.takeIf { it.isNotBlank() }
                    ?: localHostname(),
                dataDir = Path.of(
                    environment["R4_AGENT_DATA_DIR"]
                        ?.takeIf { it.isNotBlank() }
                        ?: defaultDataDir()
                ),
                heartbeatInterval = parseDuration(
                    environment["R4_HEARTBEAT_INTERVAL"],
                    Duration.ofSeconds(10)
                ),
                commandPollInterval = parseDuration(
                    environment["R4_COMMAND_POLL_INTERVAL"],
                    Duration.ofSeconds(5)
                ),
                httpConnectTimeout = parseDuration(
                    environment["R4_HTTP_CONNECT_TIMEOUT"],
                    Duration.ofSeconds(5)
                ),
                httpRequestTimeout = parseDuration(
                    environment["R4_HTTP_REQUEST_TIMEOUT"],
                    Duration.ofSeconds(10)
                )
            ).validate()
        }

        private fun parseDuration(value: String?, default: Duration): Duration {
            if (value.isNullOrBlank()) {
                return default
            }

            return value.toLongOrNull()
                ?.let { Duration.ofMillis(it) }
                ?: Duration.parse(value)
        }

        private fun defaultDataDir(): String {
            if (System.getProperty("os.name").contains("Linux", ignoreCase = true)) {
                return "/var/lib/r4-agent"
            }

            return Path.of(System.getProperty("user.home"), ".r4-agent").toString()
        }

        private fun localHostname(): String {
            return runCatching { java.net.InetAddress.getLocalHost().hostName }
                .getOrDefault("r4-linux-agent")
        }
    }

    private fun validate(): AgentConfiguration {
        require(hubUrl.scheme == "http" || hubUrl.scheme == "https") {
            "R4_HUB_URL must use http or https"
        }
        require(!heartbeatInterval.isNegative && !heartbeatInterval.isZero) {
            "R4_HEARTBEAT_INTERVAL must be positive"
        }
        require(!commandPollInterval.isNegative && !commandPollInterval.isZero) {
            "R4_COMMAND_POLL_INTERVAL must be positive"
        }
        require(!httpConnectTimeout.isNegative && !httpConnectTimeout.isZero) {
            "R4_HTTP_CONNECT_TIMEOUT must be positive"
        }
        require(!httpRequestTimeout.isNegative && !httpRequestTimeout.isZero) {
            "R4_HTTP_REQUEST_TIMEOUT must be positive"
        }

        return this
    }
}
