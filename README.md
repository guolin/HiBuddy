# HiBuddy

HiBuddy is an open-source AI companion firmware project for `M5StickS3`.

It turns a small ESP32-S3 device into a glanceable pet display that can:

- provision Wi-Fi on-device
- request a pairing code from a control plane API
- fetch runtime configuration after pairing
- connect to MQTT and consume live `snapshot` updates
- render a pet-driven UI with local motion, sleep, and mood logic

## Hardware

Current reference target:

- `M5StickS3`
- `ESP32-S3`
- `135x240` portrait display
- IMU, battery, Wi-Fi

The repository currently uses PlatformIO and Arduino framework.

## Main Features

- Wi-Fi setup portal on the device
- Pairing flow with control plane API
- MQTT runtime bootstrap and live snapshot subscription
- Three main screens:
  - `Pet`
  - `Overview`
  - `Info`
- Local pet attributes:
  - `mood`
  - `energy`
  - `focus`
- Procedural pet motion:
  - micro movement
  - random pauses
  - state variants
  - shake and face-down sleep behavior
- Demo pet pack and pet pack tooling

## Repository Layout

- `src`
  Firmware source code
- `include`
  Shared firmware types and config
- `docs`
  Product, protocol, UI, and animation docs
- `pet_pack_builder`
  Python tooling for generating pet pack outputs
- `tools/pet_pack_builder`
  Tool-specific README and requirements

## Build

1. Install PlatformIO.
2. Open the repo on Windows.
3. Build:

```powershell
& "$env:USERPROFILE\\.platformio\\penv\\Scripts\\platformio.exe" run
```

4. Upload:

```powershell
& "$env:USERPROFILE\\.platformio\\penv\\Scripts\\platformio.exe" run --target upload
```

5. Monitor:

```powershell
& "$env:USERPROFILE\\.platformio\\penv\\Scripts\\platformio.exe" device monitor
```

## Local Configuration

The committed `platformio.ini` uses a safe placeholder API base URL:

- `https://example.supabase.co/functions/v1`

For local development, create an untracked `platformio.local.ini` and override build flags there. Example:

```ini
[env:m5stack-sticks3]
build_flags =
    ${env:m5stack-sticks3.build_flags}
    -D BUDDY_API_BASE_URL=\"https://your-project-ref.supabase.co/functions/v1\"
```

Do not commit real project URLs, device tokens, MQTT credentials, or private API keys.

## Docs

Recommended starting points:

- `docs/product.md`
- `docs/boundaries.md`
- `docs/device-client/pet-animation.md`
- `docs/device-api.md`

## Pet Pack Tooling

The repo includes a Python-based `pet_pack_builder` for generating pet pack artifacts and preview outputs.

See `tools/pet_pack_builder/README.md`.

## Open-Source Notes

This repository is intended to be published publicly.

Before pushing new changes:

- keep service endpoints as placeholders unless they are intentionally public
- never commit personal API keys or bearer tokens
- avoid committing generated `output/` artifacts unless they are part of project docs
- keep local machine config in ignored files
