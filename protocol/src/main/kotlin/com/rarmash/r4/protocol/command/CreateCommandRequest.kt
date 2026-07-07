package com.rarmash.r4.protocol.command

import jakarta.validation.constraints.NotBlank
import jakarta.validation.constraints.Size

data class CreateCommandRequest(
    @field:NotBlank
    @field:Size(max = 100)
    val type: String,

    val parameters: Map<String, String> = emptyMap()
)