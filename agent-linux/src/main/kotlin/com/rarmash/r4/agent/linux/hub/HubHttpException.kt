package com.rarmash.r4.agent.linux.hub

class HubHttpException(
    val statusCode: Int
) : RuntimeException("Hub request failed with HTTP status $statusCode")
