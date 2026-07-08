package com.rarmash.r4.protocol.command

import jakarta.validation.constraints.Size
import java.util.UUID

data class CompleteCommandRequest(
    val leaseToken: UUID,

    val success: Boolean,

    @field:Size(max = 10_000)
    val result: String? = null,

    @field:Size(max = 10_000)
    val error: String? = null
)
