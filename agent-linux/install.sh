#!/usr/bin/env bash
set -Eeuo pipefail

readonly REPOSITORY="Rarmash/r4-platform"
readonly SERVICE_NAME="r4-agent"
readonly SERVICE_USER="r4-agent"
readonly SERVICE_GROUP="r4-agent"
readonly INSTALL_DIR="/opt/r4-agent"
readonly CONFIG_DIR="/etc/r4-agent"
readonly DATA_DIR="/var/lib/r4-agent"
readonly UNIT_FILE="/etc/systemd/system/r4-agent.service"
readonly CONTROL_FILE="/usr/local/sbin/r4-agentctl"
readonly ENV_FILE="${CONFIG_DIR}/r4-agent.env"
readonly ASSET_NAME="r4-agent-linux.tar.gz"

COMMAND="install"
VERSION="latest"
HUB_URL=""
AGENT_NAME=""
PURGE=false
ASSUME_YES=false
TEMP_DIR=""
RELEASE_DIR=""

log() {
    printf 'r4-agent: %s\n' "$*" >&2
}

die() {
    printf 'r4-agent: error: %s\n' "$*" >&2
    exit 1
}

cleanup() {
    if [[ -n "${TEMP_DIR}" && -d "${TEMP_DIR}" ]]; then
        rm -rf -- "${TEMP_DIR}"
    fi
}
trap cleanup EXIT

usage() {
    cat <<'EOF'
Install and manage R4 Linux Agent.

Usage:
  install.sh [install] --hub-url URL [--name NAME] [--version VERSION]
  install.sh update [--hub-url URL] [--name NAME] [--version VERSION]
  install.sh status
  install.sh uninstall [--purge] [--yes]

Commands:
  install     Install or reinstall the agent (default).
  update      Download and install a new agent release.
  status      Show installation and service status.
  uninstall   Remove the application while preserving identity and configuration.

Options:
  --hub-url URL     R4 Hub base URL. Required for the first installation.
  --name NAME       Agent name. Defaults to the system hostname.
  --version VERSION Release version: latest, vX.Y.Z, or X.Y.Z.
  --purge           Also remove configuration, identity, user, and group.
  --yes             Confirm a non-interactive purge.
  -h, --help        Show this help.
EOF
}

parse_args() {
    if [[ $# -gt 0 ]]; then
        case "$1" in
            install|update|status|uninstall)
                COMMAND="$1"
                shift
                ;;
        esac
    fi

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --hub-url)
                [[ $# -ge 2 ]] || die "--hub-url requires a value"
                HUB_URL="$2"
                shift 2
                ;;
            --name)
                [[ $# -ge 2 ]] || die "--name requires a value"
                AGENT_NAME="$2"
                shift 2
                ;;
            --version)
                [[ $# -ge 2 ]] || die "--version requires a value"
                VERSION="$2"
                shift 2
                ;;
            --purge)
                PURGE=true
                shift
                ;;
            --yes)
                ASSUME_YES=true
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                die "unknown argument: $1 (run with --help for usage)"
                ;;
        esac
    done
}

require_root() {
    [[ "$(id -u)" -eq 0 ]] || die "this command requires root privileges; run it with sudo"
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || die "required command is missing: $1"
}

validate_platform() {
    [[ "$(uname -s)" == "Linux" ]] || die "only Linux is supported"
    [[ -d /run/systemd/system ]] || die "systemd is required"

    case "$(uname -m)" in
        x86_64|amd64|aarch64|arm64) ;;
        *) die "unsupported architecture: $(uname -m); supported: x86_64 and aarch64" ;;
    esac
}

validate_java() {
    require_command java
    local version_output major
    version_output="$(java -version 2>&1)" || die "Java could not be started"
    major="$(printf '%s\n' "${version_output}" | awk -F'"' '/version/ { print $2; exit }' | awk -F. '{ if ($1 == "1") print $2; else print $1 }')"
    [[ "${major}" =~ ^[0-9]+$ ]] || die "could not determine the installed Java version"
    (( major >= 21 )) || die "Java 21 or newer is required; found Java ${major}"
}

normalize_version() {
    if [[ "${VERSION}" == "latest" ]]; then
        return
    fi
    if [[ "${VERSION}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
        VERSION="v${VERSION}"
    fi
    [[ "${VERSION}" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]] ||
        die "invalid version '${VERSION}'; use latest, vX.Y.Z, or X.Y.Z"
}

validate_hub_url() {
    [[ "$1" =~ ^https?://[^[:space:]]+$ ]] ||
        die "Hub URL must start with http:// or https:// and contain no spaces"
}

read_env_value() {
    local key="$1"
    [[ -f "${ENV_FILE}" ]] || return 0
    awk -v key="${key}" '
        $0 ~ "^[[:space:]]*" key "=" {
            sub("^[[:space:]]*" key "=", "")
            value = $0
        }
        END { if (value != "") print value }
    ' "${ENV_FILE}"
}

set_env_value() {
    local key="$1" value="$2" temp_file
    temp_file="$(mktemp "${CONFIG_DIR}/.r4-agent.env.XXXXXX")"
    awk -v key="${key}" -v value="${value}" '
        BEGIN { found = 0 }
        $0 ~ "^[[:space:]]*" key "=" {
            if (!found) print key "=" value
            found = 1
            next
        }
        { print }
        END { if (!found) print key "=" value }
    ' "${ENV_FILE}" > "${temp_file}"
    chown root:"${SERVICE_GROUP}" "${temp_file}"
    chmod 0640 "${temp_file}"
    mv -f -- "${temp_file}" "${ENV_FILE}"
}

release_base_url() {
    if [[ "${VERSION}" == "latest" ]]; then
        printf 'https://github.com/%s/releases/latest/download' "${REPOSITORY}"
    else
        printf 'https://github.com/%s/releases/download/%s' "${REPOSITORY}" "${VERSION}"
    fi
}

download_release() {
    require_command curl
    require_command sha256sum
    require_command tar

    TEMP_DIR="$(mktemp -d)"
    local base_url archive checksum expected_name
    base_url="$(release_base_url)"
    archive="${TEMP_DIR}/${ASSET_NAME}"
    checksum="${archive}.sha256"

    log "downloading ${VERSION} release"
    curl --fail --silent --show-error --location --proto '=https' \
        --output "${archive}" "${base_url}/${ASSET_NAME}" ||
        die "failed to download ${base_url}/${ASSET_NAME}"
    curl --fail --silent --show-error --location --proto '=https' \
        --output "${checksum}" "${base_url}/${ASSET_NAME}.sha256" ||
        die "failed to download the SHA-256 checksum"

    expected_name="$(awk 'NF >= 2 { name=$2; sub(/^\\*/, "", name); print name; exit }' "${checksum}")"
    [[ "${expected_name}" == "${ASSET_NAME}" ]] ||
        die "checksum file does not refer to ${ASSET_NAME}"
    (cd "${TEMP_DIR}" && sha256sum --check --strict "${ASSET_NAME}.sha256") ||
        die "release checksum verification failed"

    validate_archive "${archive}"
    tar -xzf "${archive}" -C "${TEMP_DIR}"
    RELEASE_DIR="${TEMP_DIR}/r4-agent-linux"
}

validate_archive() {
    local archive="$1" entry
    while IFS= read -r entry; do
        [[ "${entry}" == r4-agent-linux/* ]] ||
            die "release archive has an unexpected top-level entry: ${entry}"
        [[ "${entry}" != *"/../"* && "${entry}" != ../* && "${entry}" != /* ]] ||
            die "release archive contains an unsafe path: ${entry}"
    done < <(tar -tzf "${archive}")

    tar -tzf "${archive}" | grep -qx 'r4-agent-linux/app/bin/agent-linux' ||
        die "release archive is missing app/bin/agent-linux"
    tar -tzf "${archive}" | grep -qx 'r4-agent-linux/deploy/r4-agent.service' ||
        die "release archive is missing deploy/r4-agent.service"
    tar -tzf "${archive}" | grep -qx 'r4-agent-linux/deploy/r4-agent.env.example' ||
        die "release archive is missing deploy/r4-agent.env.example"
    tar -tzf "${archive}" | grep -qx 'r4-agent-linux/install.sh' ||
        die "release archive is missing install.sh"
    tar -tzf "${archive}" | grep -qx 'r4-agent-linux/VERSION' ||
        die "release archive is missing VERSION"
}

ensure_account_and_directories() {
    if ! getent group "${SERVICE_GROUP}" >/dev/null; then
        groupadd --system "${SERVICE_GROUP}"
    fi
    if ! id "${SERVICE_USER}" >/dev/null 2>&1; then
        useradd --system --gid "${SERVICE_GROUP}" --home-dir "${DATA_DIR}" \
            --shell /usr/sbin/nologin "${SERVICE_USER}"
    fi

    install -d -o root -g root -m 0755 "${CONFIG_DIR}"
    install -d -o "${SERVICE_USER}" -g "${SERVICE_GROUP}" -m 0750 "${DATA_DIR}"
}

configure_agent() {
    local release_dir="$1" had_config="$2" existing_hub existing_name
    if [[ ! -f "${ENV_FILE}" ]]; then
        install -o root -g "${SERVICE_GROUP}" -m 0640 \
            "${release_dir}/deploy/r4-agent.env.example" "${ENV_FILE}"
    fi

    existing_hub="$(read_env_value R4_HUB_URL)"
    existing_name="$(read_env_value R4_AGENT_NAME)"

    if [[ -n "${HUB_URL}" ]]; then
        validate_hub_url "${HUB_URL}"
        set_env_value R4_HUB_URL "${HUB_URL}"
    elif [[ -z "${existing_hub}" ]]; then
        die "--hub-url is required for the first installation"
    else
        validate_hub_url "${existing_hub}"
    fi

    if [[ -n "${AGENT_NAME}" ]]; then
        set_env_value R4_AGENT_NAME "${AGENT_NAME}"
    elif [[ "${had_config}" == false || -z "${existing_name}" ]]; then
        set_env_value R4_AGENT_NAME "$(hostname)"
    fi
    set_env_value R4_AGENT_DATA_DIR "${DATA_DIR}"
}

show_failure_logs() {
    log "service failed to start; recent logs follow"
    journalctl -u "${SERVICE_NAME}" --no-pager -n 30 >&2 || true
}

activate_release() {
    local release_dir="$1" staging="${INSTALL_DIR}.new.$$" backup="${INSTALL_DIR}.previous.$$"
    local unit_backup="${TEMP_DIR}/r4-agent.service.previous" had_unit=false
    rm -rf -- "${staging}"
    install -d -o root -g root -m 0755 "${staging}"
    cp -a "${release_dir}/app/." "${staging}/"
    install -o root -g root -m 0644 "${release_dir}/VERSION" "${staging}/VERSION"
    chown -R root:root "${staging}"
    chmod 0755 "${staging}/bin/agent-linux"

    if [[ -f "${UNIT_FILE}" ]]; then
        had_unit=true
        cp -a -- "${UNIT_FILE}" "${unit_backup}"
    fi
    install -o root -g root -m 0644 "${release_dir}/deploy/r4-agent.service" "${UNIT_FILE}"
    install -o root -g root -m 0755 "${release_dir}/install.sh" "${CONTROL_FILE}"
    systemctl daemon-reload

    local had_previous=false
    if [[ -d "${INSTALL_DIR}" ]]; then
        had_previous=true
        systemctl stop "${SERVICE_NAME}" || true
        mv -- "${INSTALL_DIR}" "${backup}"
    fi

    mv -- "${staging}" "${INSTALL_DIR}"
    systemctl enable "${SERVICE_NAME}" >/dev/null
    if systemctl restart "${SERVICE_NAME}" && systemctl is-active --quiet "${SERVICE_NAME}"; then
        rm -rf -- "${backup}"
        log "installed version $(tr -d '\r\n' < "${INSTALL_DIR}/VERSION")"
        return
    fi

    show_failure_logs
    systemctl stop "${SERVICE_NAME}" || true
    rm -rf -- "${INSTALL_DIR}"
    if [[ "${had_unit}" == true ]]; then
        install -o root -g root -m 0644 "${unit_backup}" "${UNIT_FILE}"
    else
        rm -f -- "${UNIT_FILE}"
    fi
    systemctl daemon-reload
    if [[ "${had_previous}" == true ]]; then
        mv -- "${backup}" "${INSTALL_DIR}"
        if systemctl restart "${SERVICE_NAME}" && systemctl is-active --quiet "${SERVICE_NAME}"; then
            die "new version failed to start; the previous version was restored"
        fi
        die "new version failed to start and the previous service could not be restored"
    fi
    systemctl disable "${SERVICE_NAME}" >/dev/null 2>&1 || true
    die "agent installation failed because the service did not become active"
}

install_or_update() {
    require_root
    validate_platform
    validate_java
    normalize_version
    require_command awk
    require_command getent
    require_command install
    require_command systemctl

    local had_config=false
    if [[ -f "${ENV_FILE}" ]]; then
        had_config=true
    elif [[ -z "${HUB_URL}" ]]; then
        die "--hub-url is required for the first installation"
    fi

    download_release
    ensure_account_and_directories
    configure_agent "${RELEASE_DIR}" "${had_config}"
    activate_release "${RELEASE_DIR}"
    log "Hub URL: $(read_env_value R4_HUB_URL)"
    log "agent name: $(read_env_value R4_AGENT_NAME)"
    log "run 'sudo r4-agentctl status' to inspect the service"
}

show_status() {
    require_root
    local installed="no" version="not installed" hub_url="not configured"
    local agent_name="not configured" identity="absent" enabled="disabled" active="inactive"

    if [[ -x "${INSTALL_DIR}/bin/agent-linux" ]]; then
        installed="yes"
    fi
    if [[ -f "${INSTALL_DIR}/VERSION" ]]; then
        version="$(tr -d '\r\n' < "${INSTALL_DIR}/VERSION")"
    fi
    if [[ -f "${ENV_FILE}" ]]; then
        hub_url="$(read_env_value R4_HUB_URL)"
        agent_name="$(read_env_value R4_AGENT_NAME)"
    fi
    if [[ -f "${DATA_DIR}/agent-id" ]]; then
        identity="present"
    fi
    if systemctl is-enabled --quiet "${SERVICE_NAME}" 2>/dev/null; then
        enabled="enabled"
    fi
    active="$(systemctl is-active "${SERVICE_NAME}" 2>/dev/null || true)"

    printf '%-18s %s\n' \
        "Installed:" "${installed}" \
        "Version:" "${version}" \
        "Hub URL:" "${hub_url:-not configured}" \
        "Agent name:" "${agent_name:-not configured}" \
        "Persistent ID:" "${identity}" \
        "Service enabled:" "${enabled}" \
        "Service state:" "${active:-unknown}"
    printf '\nLogs: journalctl -u %s -f\n' "${SERVICE_NAME}"
}

confirm_purge() {
    [[ "${ASSUME_YES}" == true ]] && return
    [[ -t 0 ]] || die "purge requires confirmation; add --yes for non-interactive use"
    printf 'Purge removes configuration and identity. The next install registers a new device. Continue? [y/N] '
    local answer
    read -r answer
    [[ "${answer}" =~ ^[Yy]$ ]] || die "purge cancelled"
}

uninstall_agent() {
    require_root
    if [[ "${PURGE}" == true ]]; then
        confirm_purge
    fi

    systemctl disable --now "${SERVICE_NAME}" >/dev/null 2>&1 || true
    rm -f -- "${UNIT_FILE}"
    systemctl daemon-reload
    rm -rf -- "${INSTALL_DIR}"

    if [[ "${PURGE}" == true ]]; then
        rm -rf -- "${CONFIG_DIR}" "${DATA_DIR}"
        if id "${SERVICE_USER}" >/dev/null 2>&1; then
            userdel "${SERVICE_USER}" || die "could not remove user ${SERVICE_USER}"
        fi
        if getent group "${SERVICE_GROUP}" >/dev/null; then
            groupdel "${SERVICE_GROUP}" || die "could not remove group ${SERVICE_GROUP}"
        fi
        log "agent, configuration, and identity removed"
    else
        log "agent removed; configuration and identity were preserved"
    fi

    rm -f -- "${CONTROL_FILE}"
}

main() {
    parse_args "$@"
    case "${COMMAND}" in
        install|update) install_or_update ;;
        status) show_status ;;
        uninstall) uninstall_agent ;;
    esac
}

main "$@"
