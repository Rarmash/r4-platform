package com.rarmash.r4.hub.persistence

import org.springframework.stereotype.Component
import tools.jackson.databind.ObjectMapper
import tools.jackson.module.kotlin.readValue

@Component
class JsonCodec(
    private val objectMapper: ObjectMapper
) {

    fun write(value: Any): String {
        return objectMapper.writeValueAsString(value)
    }

    fun readStringSet(value: String): Set<String> {
        return objectMapper.readValue(value)
    }

    fun readStringMap(value: String): Map<String, String> {
        return objectMapper.readValue(value)
    }
}
