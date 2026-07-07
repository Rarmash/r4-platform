package com.rarmash.r4.hub.device

import com.rarmash.r4.protocol.device.DeviceResponse
import com.rarmash.r4.protocol.device.RegisterDeviceRequest
import jakarta.validation.Valid
import org.springframework.http.HttpStatus
import org.springframework.http.ResponseEntity
import org.springframework.web.bind.annotation.GetMapping
import org.springframework.web.bind.annotation.PathVariable
import org.springframework.web.bind.annotation.PostMapping
import org.springframework.web.bind.annotation.RequestBody
import org.springframework.web.bind.annotation.RequestMapping
import org.springframework.web.bind.annotation.RestController
import java.util.UUID

@RestController
@RequestMapping("/api/v1/devices")
class DeviceController(
    private val deviceService: DeviceService
) {

    @PostMapping("/register")
    fun register(
        @Valid @RequestBody request: RegisterDeviceRequest
    ): ResponseEntity<DeviceResponse> {
        val device = deviceService.register(request)

        return ResponseEntity
            .status(HttpStatus.CREATED)
            .body(device)
    }

    @PostMapping("/{deviceId}/heartbeat")
    fun heartbeat(
        @PathVariable deviceId: UUID
    ): DeviceResponse {
        return deviceService.heartbeat(deviceId)
    }

    @GetMapping
    fun getAll(): List<DeviceResponse> {
        return deviceService.getAll()
    }
}