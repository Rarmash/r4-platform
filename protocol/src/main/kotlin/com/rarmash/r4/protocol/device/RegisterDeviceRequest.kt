package com.rarmash.r4.protocol.device

import jakarta.validation.constraints.NotBlank
import jakarta.validation.constraints.Size

data class RegisterDeviceRequest(

    @field:NotBlank
    @field:Size(max = 100)
    val name: String,

    @field:NotBlank
    @field:Size(max = 100)
    val platform: String,

    @field:NotBlank
    @field:Size(max = 50)
    val agentVersion: String,

    val capabilities: Set<String> = emptySet()
)