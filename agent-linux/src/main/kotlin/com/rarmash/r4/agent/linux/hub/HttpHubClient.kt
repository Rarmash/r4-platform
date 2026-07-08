package com.rarmash.r4.agent.linux.hub

import com.fasterxml.jackson.databind.DeserializationFeature
import com.fasterxml.jackson.databind.ObjectMapper
import com.fasterxml.jackson.datatype.jsr310.JavaTimeModule
import com.fasterxml.jackson.module.kotlin.KotlinModule
import com.rarmash.r4.agent.linux.config.AgentConfiguration
import com.rarmash.r4.protocol.command.CommandResponse
import com.rarmash.r4.protocol.command.CompleteCommandRequest
import com.rarmash.r4.protocol.device.DeviceResponse
import com.rarmash.r4.protocol.device.RegisterDeviceRequest
import java.net.URI
import java.net.http.HttpClient
import java.net.http.HttpRequest
import java.net.http.HttpResponse
import java.util.UUID

class HttpHubClient(
    private val configuration: AgentConfiguration,
    private val httpClient: HttpClient = HttpClient.newBuilder()
        .connectTimeout(configuration.httpConnectTimeout)
        .build(),
    private val objectMapper: ObjectMapper = defaultObjectMapper()
) : HubClient {

    override fun register(request: RegisterDeviceRequest): DeviceResponse {
        return sendJson(
            request = post("/api/v1/devices/register", request),
            responseType = DeviceResponse::class.java
        )
    }

    override fun heartbeat(deviceId: UUID): DeviceResponse {
        return sendJson(
            request = post("/api/v1/devices/$deviceId/heartbeat", null),
            responseType = DeviceResponse::class.java
        )
    }

    override fun fetchNextCommand(deviceId: UUID): CommandResponse? {
        val response = httpClient.send(
            request("/api/v1/devices/$deviceId/commands/next")
                .GET()
                .build(),
            HttpResponse.BodyHandlers.ofString()
        )

        if (response.statusCode() == 204) {
            return null
        }

        ensureSuccess(response)
        return objectMapper.readValue(response.body(), CommandResponse::class.java)
    }

    override fun completeCommand(
        deviceId: UUID,
        command: CommandResponse,
        success: Boolean,
        result: String?,
        error: String?
    ) {
        val leaseToken = command.leaseToken
            ?: error("Running command ${command.id} has no lease token")

        val request = CompleteCommandRequest(
            leaseToken = leaseToken,
            success = success,
            result = result,
            error = error
        )

        val response = httpClient.send(
            post(
                "/api/v1/devices/$deviceId/commands/${command.id}/result",
                request
            ),
            HttpResponse.BodyHandlers.discarding()
        )

        ensureSuccess(response)
    }

    private fun <T> sendJson(request: HttpRequest, responseType: Class<T>): T {
        val response = httpClient.send(
            request,
            HttpResponse.BodyHandlers.ofString()
        )

        ensureSuccess(response)
        return objectMapper.readValue(response.body(), responseType)
    }

    private fun post(path: String, body: Any?): HttpRequest {
        val publisher = body?.let {
            HttpRequest.BodyPublishers.ofString(objectMapper.writeValueAsString(it))
        } ?: HttpRequest.BodyPublishers.noBody()

        return request(path)
            .POST(publisher)
            .header("Content-Type", "application/json")
            .build()
    }

    private fun request(path: String): HttpRequest.Builder {
        return HttpRequest.newBuilder(resolve(path))
            .timeout(configuration.httpRequestTimeout)
            .header("Accept", "application/json")
    }

    private fun resolve(path: String): URI {
        return configuration.hubUrl.resolve(path)
    }

    private fun ensureSuccess(response: HttpResponse<*>) {
        if (response.statusCode() !in 200..299) {
            throw HubHttpException(response.statusCode())
        }
    }

    companion object {
        fun defaultObjectMapper(): ObjectMapper {
            return ObjectMapper()
                .registerModule(JavaTimeModule())
                .registerModule(KotlinModule.Builder().build())
                .disable(DeserializationFeature.FAIL_ON_UNKNOWN_PROPERTIES)
        }
    }
}
