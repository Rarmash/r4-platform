FROM eclipse-temurin:21-jdk-jammy AS build

WORKDIR /workspace

COPY . .

RUN chmod +x gradlew \
    && ./gradlew :hub:bootJar --no-daemon


FROM eclipse-temurin:21-jre-jammy

WORKDIR /app

RUN useradd \
        --system \
        --uid 10001 \
        --home-dir /app \
        r4

COPY --from=build \
    /workspace/hub/build/libs/r4-hub.jar \
    /app/r4-hub.jar

RUN chown -R r4:r4 /app

USER r4

EXPOSE 8080

ENTRYPOINT ["sh", "-c", "exec java $JAVA_OPTS -jar /app/r4-hub.jar"]
