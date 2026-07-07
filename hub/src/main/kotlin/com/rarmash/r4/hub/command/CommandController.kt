package com.rarmash.r4.hub.command

import com.rarmash.r4.protocol.command.CommandResponse
import com.rarmash.r4.protocol.command.CompleteCommandRequest
import com.rarmash.r4.protocol.command.CreateCommandRequest
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
@RequestMapping("/api/v1/devices/{deviceId}/commands")
class CommandController(
    private val commandService: CommandService
) {

    @PostMapping
    fun create(
        @PathVariable deviceId: UUID,
        @Valid @RequestBody request: CreateCommandRequest
    ): ResponseEntity<CommandResponse> {
        return ResponseEntity
            .status(HttpStatus.CREATED)
            .body(commandService.create(deviceId, request))
    }

    @GetMapping("/next")
    fun next(
        @PathVariable deviceId: UUID
    ): ResponseEntity<CommandResponse> {
        val command = commandService.claimNext(deviceId)

        return if (command == null) {
            ResponseEntity.noContent().build()
        } else {
            ResponseEntity.ok(command)
        }
    }

    @PostMapping("/{commandId}/result")
    fun complete(
        @PathVariable deviceId: UUID,
        @PathVariable commandId: UUID,
        @Valid @RequestBody request: CompleteCommandRequest
    ): CommandResponse {
        return commandService.complete(
            deviceId = deviceId,
            commandId = commandId,
            request = request
        )
    }

    @GetMapping
    fun getAll(
        @PathVariable deviceId: UUID
    ): List<CommandResponse> {
        return commandService.getAll(deviceId)
    }
}