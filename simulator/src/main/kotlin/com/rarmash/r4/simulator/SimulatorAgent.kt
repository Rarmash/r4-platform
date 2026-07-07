package com.rarmash.r4.simulator

import com.rarmash.r4.protocol.device.DeviceResponse
import com.rarmash.r4.protocol.device.RegisterDeviceRequest
import org.slf4j.LoggerFactory
import org.springframework.scheduling.annotation.Scheduled
import org.springframework.stereotype.Component
import org.springframework.web.client.HttpClientErrorException
import org.springframework.web.client.RestClient
import org.springframework.web.client.RestClientException
import java.util.UUID

@Component
class SimulatorAgent(
    private val hubRestClient: RestClient,
    private val identityProvider: AgentIdentityProvider
) {

    private val logger = LoggerFactory.getLogger(javaClass)

    @Volatile
    private var deviceId: UUID? = null

    @Scheduled(
        initialDelayString = "\${r4.initial-delay-ms:1000}",
        fixedDelayString = "\${r4.heartbeat-interval-ms:10000}"
    )
    fun tick() {
        val currentDeviceId = deviceId ?: register() ?: return

        sendHeartbeat(currentDeviceId)
    }

    private fun register(): UUID? {
        logger.info(
            "Registering simulator in R4 Hub with agent ID {}",
            identityProvider.agentId
        )

        val request = RegisterDeviceRequest(
            agentId = identityProvider.agentId,
            name = "Local simulator",
            platform = "windows-amd64",
            agentVersion = "0.1.0",
            capabilities = setOf(
                "system-info",
                "command.echo"
            )
        )

        return try {
            val response = hubRestClient
                .post()
                .uri("/api/v1/devices/register")
                .body(request)
                .retrieve()
                .body(DeviceResponse::class.java)
                ?: error("Hub returned an empty registration response")

            deviceId = response.id

            logger.info(
                "Simulator registered with device ID {}",
                response.id
            )

            response.id
        } catch (exception: RestClientException) {
            logger.warn(
                "Could not register simulator: {}",
                exception.message
            )

            null
        }
    }

    private fun sendHeartbeat(deviceId: UUID) {
        try {
            val response = hubRestClient
                .post()
                .uri(
                    "/api/v1/devices/{deviceId}/heartbeat",
                    deviceId
                )
                .retrieve()
                .body(DeviceResponse::class.java)
                ?: error("Hub returned an empty heartbeat response")

            logger.info(
                "Heartbeat sent, lastSeenAt={}",
                response.lastSeenAt
            )
        } catch (exception: HttpClientErrorException.NotFound) {
            logger.warn(
                "Hub no longer knows device {}, registering again",
                deviceId
            )

            this.deviceId = null
        } catch (exception: RestClientException) {
            logger.warn(
                "Could not send heartbeat: {}",
                exception.message
            )
        }
    }
}