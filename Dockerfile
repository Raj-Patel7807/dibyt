# Multi-stage production build for Dibyt Server Daemon
# Stage 1: Build environment
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    gcc-12 \
    g++-12 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

COPY . .

RUN cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=gcc-12 \
    -DCMAKE_CXX_COMPILER=g++-12 \
    -DDIBYT_BUILD_TESTS=OFF \
    -DDIBYT_BUILD_BENCHMARKS=OFF \
    && cmake --build build --target dibyt-server dibyt-cli

# Stage 2: Runtime environment
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Create dedicated non-root system user and group (UID 10001)
RUN groupadd --system --gid 10001 dibyt && \
    useradd --system --uid 10001 --gid dibyt --create-home --home-dir /var/lib/dibyt dibyt

# Prepare persistent state directories with non-root ownership
RUN mkdir -p /var/lib/dibyt/data /var/lib/dibyt/staging /etc/dibyt && \
    chown -R dibyt:dibyt /var/lib/dibyt

# Copy compiled binaries from builder stage
COPY --from=builder /build/build/apps/dibyt-server/dibyt-server /usr/local/bin/dibyt-server
COPY --from=builder /build/build/apps/dibyt-cli/dibyt /usr/local/bin/dibyt

USER dibyt:dibyt
WORKDIR /var/lib/dibyt

# Expose Control Plane (HTTP API / Health / Metrics) and Data Plane (Raw TCP Transfer)
EXPOSE 8080 9080

VOLUME ["/var/lib/dibyt"]

HEALTHCHECK --interval=15s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

ENTRYPOINT ["/usr/local/bin/dibyt-server"]
CMD ["--config", "/etc/dibyt/dibyt.env"]
