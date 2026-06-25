# Cloud_LLM_Call_from_ESP32

## What this is

ESP32 connects to WiFi, sends an HTTPS POST to the Google Gemini API with a text prompt, and prints whatever Gemini generates back. This is the second half of the AI-layers-in-isolation step, run independently of the STM32 side and independently of NanoEdge AI Studio. Goal is just proving the cloud-call round trip works on its own before it gets wired into the real anomaly-forwarding flow later.

## Why Gemini

Compared providers before starting:
- OpenAI and Anthropic require a card/deposit to use their APIs at all, no sustained no-card free tier
- A claude.ai or ChatGPT consumer subscription is completely separate from API access, paying for one does not grant the other
- Groq has a genuine no-card free tier but only serves open-source models (Llama, Mixtral, etc), not Gemini/GPT/Claude
- Gemini has the most generous no-card free tier among providers offering their own frontier model, comfortably enough quota for occasional calls

Model used: `gemini-2.5-flash`, called via the classic `generateContent` REST endpoint, not the newer Interactions API (overkill for a single-turn prompt-and-response use case).

## Secrets handling

WiFi SSID/password and the Gemini API key are defined in `main/Kconfig.projbuild` with placeholder defaults, then set to real values via `idf.py menuconfig`, which writes them into `sdkconfig`. The placeholder file is safe to commit, the real values in `sdkconfig` are not. Same pattern as `ESP32_to_Cloud_HTTP`.

Important gotcha caught mid-project: the repo's `.gitignore` only had STM32CubeIDE-template entries from initial setup, nothing ESP-IDF-specific. `sdkconfig` was never excluded, which means the previous project's real WiFi password had already been committed to git history before this was caught. Added `sdkconfig` to `.gitignore` going forward. Did not rewrite git history to scrub the already-exposed password since the repo is public and that's not worth the disruption, rotating the actual WiFi password is the simpler fix and is still pending.

## Request and response shape

Gemini expects:
```
{"contents": [{"parts": [{"text": "<prompt>"}]}]}
```

And replies with the generated text nested at:
```
candidates[0].content.parts[0].text
```

Built and parsed with `cJSON`, bundled with ESP-IDF, no extra dependency needed.

## Gotchas

Build failed on first attempt: `fatal error: esp_crt_bundle.h: No such file or directory`. The actual error output was unusually direct about the fix, it named the exact missing component and where to add it. `esp_crt_bundle.h` is provided by the `mbedtls` component, and that has to be explicitly listed in `main/CMakeLists.txt`'s `REQUIRES`, even though `esp_http_client` is also required. One component depending on another internally doesn't pull the second component's headers into a third component's visibility automatically.

`idf.py menuconfig` initially didn't show the custom "Cloud LLM Configuration" menu at all, even though `Kconfig.projbuild` was correct. Caused by `set-target` having run before the Kconfig file existed, leaving a stale cached config structure. Fixed with `idf.py fullclean` before reopening menuconfig.

## Outcome

```
wifi:connected with <network>, channel 1, BW20
esp_netif_handlers: sta ip: 192.168.0.195
llm_client: WiFi connected
esp-x509-crt-bundle: Certificate validated
llm_client: HTTP Status = 200
llm_client: Gemini says: Vibration-based predictive maintenance involves analyzing
machinery vibrations to detect anomalies and predict potential failures, enabling
proactive maintenance before breakdowns occur.
```

Full round trip confirmed: WiFi connect, TLS handshake, JSON request built and sent, JSON response received and parsed, generated text extracted and printed. Group E complete, both AI layers (NanoEdge AI Studio workflow, cloud LLM call) proven in isolation. Capstone integration is next: wiring the STM32-to-ESP32 UART bridge, the trained anomaly-detection library, and this LLM call together into one actual closed loop.

## Lessons learned

A component's header being includable doesn't mean its provider is automatically in scope. `esp_crt_bundle.h` only became visible once `mbedtls` was explicitly added to `main`'s `REQUIRES` list, the dependency has to be declared at the component level that actually uses the header, not inferred from what some other already-required component happens to depend on internally.

Good intentions in code (Kconfig placeholders, real values only in sdkconfig) don't protect anything if the toolchain's actual generated config file isn't excluded from version control. Check `.gitignore` covers the right files for whatever toolchain is in play before the first commit of a new kind of project, not after noticing a leak.

Comparing LLM API providers on cost and access before writing any code was worth the time, the actual constraints (which providers require a card, which subscriptions do and don't grant API access) aren't obvious from the model names alone and materially affect which one is usable for a side project with zero budget.
