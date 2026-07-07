package com.rarmash.r4.simulator

import org.springframework.beans.factory.annotation.Value
import org.springframework.context.annotation.Bean
import org.springframework.context.annotation.Configuration
import org.springframework.web.client.RestClient

@Configuration
class SimulatorConfiguration {

    @Bean
    fun hubRestClient(
        @Value("\${r4.hub-url}") hubUrl: String
    ): RestClient {
        return RestClient.builder()
            .baseUrl(hubUrl)
            .build()
    }
}