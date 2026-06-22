# ESP32_to_Cloud_HTTP

ESP32-WROOM-32D connects to Wi-Fi as a station, waits for an actual IP (not a guessed delay), then does an HTTPS GET to httpbin.org and logs the JSON response. First time this board has talked to the open internet rather than just the local network.

## What it does

- Boots, initializes NVS (required by the Wi-Fi driver for calibration data)
- Connects to Wi-Fi in station mode, retrying on disconnect
- Blocks on a FreeRTOS event group bit until `IP_EVENT_STA_GOT_IP` actually fires
- Fires an HTTPS GET to `https://httpbin.org/get` using `esp_http_client`
- Validates the TLS cert using ESP-IDF's bundled root CA list (`esp_crt_bundle_attach`) — no manual cert management
- Logs status code, content length, and the response body

This is the de-risking step before any cloud LLM call gets layered on top for the capstone — confirms DNS, TLS, and HTTP all work cleanly on this board before adding API auth, JSON payloads, or anything capstone-specific.

## Key config decisions

**Own code, not the stock example.** Last project (`ESP32_WiFi_HelloWorld`) used Espressif's example verbatim purely to validate the toolchain. This one is hand-written — Wi-Fi connect logic, event handling, and the HTTP call are all mine, not copied.

**Event-driven Wi-Fi connect, not the example's built-in retry wrapper.** Registered handlers for `WIFI_EVENT_STA_START`, `WIFI_EVENT_STA_DISCONNECTED`, and `IP_EVENT_STA_GOT_IP` directly instead of using the example's retry-count wrapper. This is the actual underlying mechanism ESP-IDF examples wrap — same event-driven pattern as the FreeRTOS event-flag AND-logic session, just IDF's own event loop instead of a hand-rolled one.

**`xEventGroupWaitBits` instead of a fixed delay.** First version used `vTaskDelay(5000ms)` before firing the HTTP request, gambling that DHCP would finish in 5 seconds. It didn't always — see Gotchas. Replaced with a proper blocking wait on a `WIFI_CONNECTED_BIT`, set only inside the `IP_EVENT_STA_GOT_IP` handler. No more racing against DHCP timing.

**`esp_crt_bundle_attach` for TLS.** ESP-IDF ships a bundled list of trusted root CAs (`Component config → mbedTLS → Certificate Bundle`). Passing `crt_bundle_attach = esp_crt_bundle_attach` in the HTTP client config lets it validate any standard HTTPS cert without manually embedding a specific cert for one endpoint.

**SSID/password via Kconfig, not hardcoded.** Same pattern as the Wi-Fi hello-world project: `main/Kconfig.projbuild` defines `WIFI_SSID`/`WIFI_PASSWORD` as menuconfig options, set via `idf.py menuconfig`, stored in the gitignored `sdkconfig`. Never touches source or git history.

## Gotchas

- **Fixed-delay race condition.** `vTaskDelay(5000ms)` before the HTTP call fired before Wi-Fi had actually gotten an IP on a slower DHCP handshake — the log showed `Disconnected, retrying...` at the 3s and 5s marks, then the HTTP call ran anyway and DNS resolution failed (`getaddrinfo() returns 202`) because the network stack wasn't actually up yet. Looked like a DNS bug; was actually a timing bug one layer up. Fixed by blocking on the actual `got ip` event instead of guessing a duration.
- **`sdkconfig` is per-project, not global.** After fixing the delay bug, rebuilt and got an infinite `Disconnected, retrying...` loop with no `wifi:connected` line at all — looked like a fresh, worse failure. Turned out `idf.py menuconfig` had never actually been (re)run for this project folder, so `sdkconfig` still held the Kconfig defaults (`"myssid"` / `"mypassword"`) instead of the real network credentials. The board was retrying auth against a network that doesn't exist. `grep WIFI_SSID sdkconfig` after every menuconfig run is now standard procedure — don't trust the UI exit alone.
- **No auto-reset circuit on this breakout** (carried over from the previous project, still applies): manual BOOT-then-EN-then-release sequence required before every flash. `esptool`'s automatic bootloader entry hangs silently otherwise ("chip stopped responding").
- RF calibration failed to load (`0x1102`) and fell back to full calibration on first boot of a fresh flash — expected, not a fault.

## Lessons learned

**A timing race and a missing config can produce nearly identical-looking failures.** Both showed up as Wi-Fi-layer symptoms (disconnect/retry, no IP, downstream failure) even though one was a logic bug in my code and the other was a stale config file. The actual signal that distinguished them: the first attempt did get as far as `wifi:connected` and `got ip:` before failing later at DNS; the second never got past the auth/retry loop at all. Worth checking *how far* a failure gets before assuming it's the same root cause as last time.

**Blocking on the real event beats guessing a delay, every time.** This is the same principle as the FreeRTOS sessions on binary semaphores and event flags — block on the condition you actually care about, not on an estimate of how long it should take. A delay that "usually works" is a race condition that hasn't bitten you yet.

**Config files don't carry over between project folders.** Each ESP-IDF project gets its own `sdkconfig`. Migrating values mentally from a previous project's menuconfig session means nothing — every new project starts back at Kconfig defaults until menuconfig is actually run again for that folder.

**This is the proof point before going anywhere near the capstone's cloud LLM call.** DNS, TLS handshake, and HTTP request/response are now all confirmed working independently. Adding API authentication, JSON request bodies, or anything STM32-bridge-specific next builds on a known-good HTTP foundation instead of debugging networking and API logic at the same time.
