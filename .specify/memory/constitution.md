<!--
Sync Impact Report
- Version change: N/A (uninitialized template) → 1.0.0
- Modified principles:
  - [PRINCIPLE_1_NAME] → I. Strict Modular Architecture & Hardware Abstraction Layer (HAL)
  - [PRINCIPLE_2_NAME] → II. Resilient Network & State Machine Management
  - [PRINCIPLE_3_NAME] → III. Memory Discipline & Concurrency Safety (NON-NEGOTIABLE)
  - [PRINCIPLE_4_NAME] → IV. Safe Configuration & Robust NVS Persistence
  - [PRINCIPLE_5_NAME] → V. Testability, Observability & Diagnostics
- Added sections:
  - Technology Stack & Hardware Constraints (populated from [SECTION_2_NAME])
  - Quality Gates & Development Workflow (populated from [SECTION_3_NAME])
- Removed sections: None
- Templates requiring updates:
  - ✅ .specify/templates/plan-template.md (updated Constitution Check gates and embedded firmware project structure)
  - ✅ .specify/templates/spec-template.md (reviewed; aligned with functional/non-functional requirements)
  - ✅ .specify/templates/tasks-template.md (updated Path Conventions with embedded ESP-IDF layout)
  - ✅ .specify/templates/checklist-template.md (reviewed; no changes required)
- Follow-up TODOs: None
-->

# ESP-RLCD-Clock Constitution

## Core Principles

### I. Strict Modular Architecture & Hardware Abstraction Layer (HAL)

- Low-level hardware drivers (GPIO, power management, RLCD display, timers, peripherals) MUST be strictly decoupled from core networking, HTTP web serving, and application logic.
- Firmware source code MUST be organized into independent, loosely-coupled components (e.g., `hal`, `wifi_manager`, `web_server`, `config_store`, `app_core`) exposing explicit C/C++ header contracts.
- Core business and clock domain logic MUST be free of direct ESP-IDF/hardware peripheral calls so that it can be compiled and unit-tested off-target using mock interfaces.

**Rationale**: Decoupling hardware dependencies isolates peripheral changes specific to the ESP32-C6-DEV-KIT-N8 and enables rapid deterministic unit testing without physical hardware in the loop.

### II. Resilient Network & State Machine Management

- Wi-Fi connectivity MUST be governed by an explicit, event-driven Finite State Machine (FSM) supporting at minimum: `DISCONNECTED`, `CONNECTING`, `CONNECTED_STA`, `AP_PROVISIONING_MODE`, and `RECONNECT_BACKOFF`.
- Network unavailability, latency, or credential failures MUST NEVER block or degrade primary device operations (such as clock keeping, display refresh, local input handling, or watchdog feeding).
- Station (STA) connection failures MUST use exponential back-off and automatically transition to fallback SoftAP provisioning mode (with captive portal support) after a bounded retry threshold.

**Rationale**: IoT devices operate in unpredictable RF and router environments; deterministic state transitions prevent lockups and ensure the device remains recoverable without reflashing.

### III. Memory Discipline & Concurrency Safety (NON-NEGOTIABLE)

- Firmware MUST maintain zero memory leaks and prevent uncontrolled heap fragmentation across multi-week uptime cycles on the 512KB SRAM ESP32-C6 target.
- FreeRTOS task stacks MUST be sized deterministically and monitored via stack high-water mark checks (`uxTaskGetStackHighWaterMark`).
- Static or pre-allocated buffers SHOULD be preferred over dynamic heap allocations. Any dynamic memory allocated within HTTP web server request handlers MUST be freed on all execution paths, including timeouts and malformed request errors.
- Shared state accessed across FreeRTOS tasks or event callbacks (e.g., active configuration, network state, time sync status) MUST be synchronized using FreeRTOS mutexes, queues, or atomic operations. Unprotected global variables shared across tasks are strictly prohibited.
- Both Hardware Watchdog Timers (WDT) and the Task Watchdog Timer (TWDT) MUST remain enabled and actively subscribed across all critical execution tasks.

**Rationale**: Constrained SRAM and concurrent asynchronous network callbacks make race conditions and heap exhaustion the primary causes of field failures in embedded systems.

### IV. Safe Configuration & Robust NVS Persistence

- All runtime configuration parameters (Wi-Fi SSID/password, device settings, network parameters, time zone) MUST be validated for type, length, and domain constraints prior to storage in Non-Volatile Storage (NVS).
- Configuration storage MUST implement atomic commit semantics, explicit schema versioning for forward migration, and a deterministic fallback to factory-default parameters if NVS partitions are uninitialized or corrupted.
- All user-supplied payloads received via the built-in HTTP web server MUST be sanitized and bounds-checked before parsing to prevent buffer overflows, injection flaws, or NVS corruption.

**Rationale**: Untrusted HTTP inputs over a provisioning SoftAP directly interface with persistent flash storage; strict validation prevents bricking the device via malformed configuration payloads.

### V. Testability, Observability & Diagnostics

- Every firmware module MUST emit structured logs via `ESP_LOGx` macros (`ESP_LOGE`, `ESP_LOGW`, `ESP_LOGI`, `ESP_LOGD`) using distinct module tags and configurable runtime log levels.
- The built-in HTTP web server MUST expose a diagnostic health endpoint (`/api/status` or `/api/health`) reporting system uptime, current free heap, minimum free heap watermark, Wi-Fi mode/RSSI, and task health metrics.
- State machine transitions, NVS configuration validation, and HTTP payload/query parsers MUST be covered by automated unit tests using the Unity test framework (or equivalent host/target runner).

**Rationale**: Field debugging on headless or minimal-display embedded devices requires runtime telemetry over HTTP and serial logs alongside repeatable automated tests for state and parser edge cases.

## Technology Stack & Hardware Constraints

- **Target Hardware**: ESP32-C6-DEV-KIT-N8 (32-bit RISC-V single-core processor @ up to 160 MHz, 8MB Quad SPI Flash, 512KB HP SRAM, 2.4 GHz Wi-Fi 6 / 802.11ax).
- **Primary Framework**: ESP-IDF v6.1 (or Arduino-ESP32 core v3.x built on ESP-IDF v6.1).
- **Networking & Web Server**: LwIP TCP/IP stack with non-blocking `esp_event` Wi-Fi callbacks; embedded asynchronous/lightweight HTTP server (`esp_http_server` or `ESPAsyncWebServer`) supporting STA and SoftAP interfaces concurrently.
- **Persistent Storage**: ESP-IDF NVS (`nvs_flash`) / Preferences API with dedicated partition layout supporting 8MB flash geometry.
- **Test Framework**: Unity unit testing framework (`esp-idf` unit test app or host-based C/C++ test runner).

## Quality Gates & Development Workflow

- **Compiler Strictness**: All project components MUST compile cleanly with `-Wall -Wextra` enabled and zero unhandled compiler warnings in project code.
- **Error Handling**: Every ESP-IDF API call returning `esp_err_t` MUST be explicitly checked via `ESP_ERROR_CHECK` (during initialization where fatal) or explicit graceful error handling paths (during runtime operations such as HTTP handling and NVS writes).
- **Non-Blocking Execution**: All pull requests and task implementations MUST preserve non-blocking execution in the main loop and event handlers; blocking delays (`vTaskDelay`) are forbidden inside event callbacks or HTTP request handlers.
- **Pre-Merge Verification**: Feature branches MUST pass build verification, static/warning checks, and unit test suites for modified state machines or parsers prior to merge.

## Governance

- This Constitution supersedes ad-hoc implementation shortcuts across all specifications, implementation plans, and task breakdowns.
- Any architectural deviation (such as introducing third-party dynamic allocation libraries or bypassing the HAL) MUST be explicitly documented and justified in the `Complexity Tracking` section of the feature's `plan.md`.
- Amendments to this Constitution require updating `.specify/memory/constitution.md`, synchronizing affected templates in `.specify/templates/`, and incrementing the version according to Semantic Versioning:
  - **MAJOR**: Removal or backward-incompatible redefinition of a core principle or non-negotiable safety rule.
  - **MINOR**: Addition of a new principle, hardware constraint section, or mandatory quality gate.
  - **PATCH**: Wording clarifications, formatting improvements, or non-semantic refinements.

**Version**: 1.0.0 | **Ratified**: 2026-11-05 | **Last Amended**: 2026-11-05
