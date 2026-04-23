# Buddy Device Control Plane API

This document describes the device-facing control plane endpoints used by the Buddy firmware.

It is written for:

- firmware developers
- embedded client developers
- device-side debugging tools

## 1. Base URL

All endpoints are expected to be exposed through Supabase Edge Functions or an equivalent HTTP control plane.

Production-style shape:

```text
https://<your-project-ref>.supabase.co/functions/v1/<function-name>
```

Implemented device endpoints:

- `POST /functions/v1/device-pair-code`
- `POST /functions/v1/device-bootstrap`
- `POST /functions/v1/device-pet-sync`
- `POST /functions/v1/device-heartbeat`

Implemented CLI/control-plane endpoints:

- `POST /functions/v1/issue-cli-key`
- `POST /functions/v1/cli-bootstrap`

## 2. Device Identity Model

The device currently uses two identity fields:

1. `device_id`
   Public long-lived device identifier, for example `demo`
2. `device_token`
   Control-plane-issued device access token

Rules:

- the device can request a pairing code with only `device_id`
- the control plane returns `device_token` during pairing code creation
- later `bootstrap`, `pet-sync`, and `heartbeat` calls should include `device_token`
- requesting a new pair code invalidates the previous pairing relationship

## 3. Typical Device Flow

Recommended startup flow:

1. Request `device-pair-code`
2. Show the `pairCode` on the device
3. User completes binding on the web side
4. Poll `device-bootstrap`
5. Once `paired=true`, read MQTT and pet configuration
6. Connect MQTT
7. Periodically call `device-pet-sync`
8. Periodically call `device-heartbeat`

## 4. API: `device-pair-code`

Request a short-lived pairing code for a device.

### Request

`POST /functions/v1/device-pair-code`

```json
{
  "device_id": "demo"
}
```

Optional existing token form:

```json
{
  "device_id": "demo",
  "device_token": "dev_xxx"
}
```

### Response

```json
{
  "ok": true,
  "deviceId": "demo",
  "deviceToken": "dev_xxx",
  "pairCode": "038271",
  "pairUrl": "https://app.example.com/auth?pair_code=038271&device_id=demo",
  "expiresAt": "2026-04-21T01:58:51.980035+00:00"
}
```

## 5. API: `device-bootstrap`

Fetch the current runtime configuration after Wi-Fi is ready.

### Request

`POST /functions/v1/device-bootstrap`

```json
{
  "device_id": "demo",
  "device_token": "dev_xxx"
}
```

### Unpaired Response

```json
{
  "ok": true,
  "deviceId": "demo",
  "deviceType": "demo-simulator",
  "paired": false,
  "lastSeenAt": null,
  "lastPetSyncAt": null,
  "mqtt": null,
  "config": null,
  "activePet": null
}
```

### Paired Response

```json
{
  "ok": true,
  "deviceId": "demo",
  "deviceType": "demo-simulator",
  "paired": true,
  "lastSeenAt": null,
  "lastPetSyncAt": null,
  "mqtt": {
    "mqttBrokerUrl": "mqtts://mqtt.example.com:8883",
    "mqttUsername": "buddy",
    "mqttToken": "mqtt_token_xxx",
    "mqttClientId": "buddy-cli-demo",
    "mqttTopic": "buddy/v1/device/demo"
  },
  "config": {
    "petName": "Buddy",
    "displayProfile": "rich",
    "soundEnabled": true,
    "assetAutoUpdate": true
  },
  "activePet": {
    "petPackId": "chief-v1",
    "name": "Chief",
    "slug": "chief-v1",
    "assetVersion": "1.0.0",
    "assetManifestUrl": "https://assets.example.com/packs/chief-v1/manifest.json",
    "coverImageUrl": "https://assets.example.com/packs/chief-v1/cover.png",
    "petName": "Buddy"
  }
}
```

## 6. API: `device-pet-sync`

Report the currently installed pet pack and ask whether an update exists.

### Request

`POST /functions/v1/device-pet-sync`

```json
{
  "device_id": "demo",
  "device_token": "dev_xxx",
  "current_pet_pack_id": "chief-v1",
  "current_pet_version": "0.9.0"
}
```

### Response

```json
{
  "ok": true,
  "paired": true,
  "hasUpdate": true,
  "syncedAt": "2026-04-21T01:53:54.747Z",
  "nextSyncAfterSeconds": 600,
  "activePet": {
    "petPackId": "chief-v1",
    "name": "Chief",
    "slug": "chief-v1",
    "assetVersion": "1.0.0",
    "assetManifestUrl": "https://assets.example.com/packs/chief-v1/manifest.json",
    "coverImageUrl": "https://assets.example.com/packs/chief-v1/cover.png",
    "petName": "Buddy"
  }
}
```

## 7. API: `device-heartbeat`

Report device liveness and current runtime summary.

### Request

`POST /functions/v1/device-heartbeat`

```json
{
  "device_id": "demo",
  "device_token": "dev_xxx",
  "firmware_version": "fw-1.0.0",
  "mqtt_connected": true
}
```

### Response

```json
{
  "ok": true,
  "accepted": true,
  "receivedAt": "2026-04-21T01:53:54.747Z"
}
```

## 8. Security Notes

- Never commit real `device_token` values.
- Never commit real MQTT usernames, passwords, or tokens.
- Keep project-specific base URLs out of committed firmware defaults unless they are intentionally public.
- Use placeholders in docs and examples.
