package com.rarmash.r4.agent.linux.command

data class CommandExecutionResult(
    val success: Boolean,
    val result: String? = null,
    val error: String? = null
) {
    companion object {
        fun succeeded(result: String? = null): CommandExecutionResult {
            return CommandExecutionResult(success = true, result = result)
        }

        fun failed(error: String): CommandExecutionResult {
            return CommandExecutionResult(success = false, error = error)
        }
    }
}
