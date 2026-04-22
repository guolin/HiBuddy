# Buddy Product

## Positioning

Buddy V1 is a small networked desktop pet for AI coding agents.

## Current implementation status

The product direction is broader than the code that is wired up today.

Current implemented pieces:

- local CLI aggregation and local preview
- web auth and account session handling
- one-device binding with `device_id` + `device_key`
- CLI key issuance

Not yet wired into the main runtime loop:

- MQTT delivery
- real device rendering loop
- pet pack ownership / commerce

It is not:

- a full terminal
- a dense dashboard
- a chat client

It is:

- a glanceable state companion
- a lightweight reminder device
- a customizable pet display

## Fixed V1 hardware

Current reference hardware in this repository:

- `M5StickS3`

The UI is designed around a small screen and a 64x64 pet sprite.

## Fixed binding model

- One account drives one Buddy device in V1.
- One CLI installation logs into one account.
- One CLI installation binds to one device.
- One device binds to one account.
- One user keeps only one active pet in V1.

Planned but not fully implemented yet:

- One account can own multiple pet packs.

This is intentionally restrictive so the first version stays easy to reason about.

## Core user value

Buddy should let the user understand, in one glance:

- whether the agent is active
- whether something needs attention
- whether work just finished
- which pet is currently on duty

## Product principles

### Upstream computes, device renders

The device does not infer raw session truth.

The CLI computes:

- active sessions
- waiting sessions
- error sessions
- top session
- headline and subtitle

The device renders:

- pet state
- short text
- light summary
- temporary reminder animations

### The home screen shows only the most important layer

Home should not be a log window.

Home includes:

- one 64x64 pet
- one headline
- one subtitle
- one summary line

### Business state is translated into pet state

The pet should react emotionally, not mirror internal terms literally.

Recommended mapping:

- `active but calm` -> `thinking`
- `many tasks running` -> `busy`
- `needs attention` -> `waiting`
- `just finished` -> `done`
- `problem detected` -> `error`
- `offline or idle for long` -> `sleep`

## Main Views

The main screen uses three display modes:

### Image

Show:

- current pet sprite only

### Mixed

Show:

- current pet sprite
- one focus line
- one short status line

### Info

Show:

- current pet state
- focus summary
- Wi-Fi state
- MQTT state
- device id
- firmware version

## Interaction model

- `Short press`: next view / next item
- `Long press`: open menu / confirm / back
- `B`: reserved for system actions such as pairing

Additional rules:

- first press after screen-off only wakes
- important reminders may jump back to Home
- device returns to Home after transient reminder states

## Reminder model

Only important transitions should interrupt the user:

- entering `waiting`
- entering `error`
- entering `done`
- becoming stale after long silence

Each reminder should use:

- a one-shot pet animation
- optional tone or LED cue
- short temporary copy

## Pet commerce direction

Buddy sells complete pet packs.

A pet pack includes:

- character identity
- sprite states
- palette
- preview
- pricing metadata

It does not sell:

- single frames
- single expressions
- core product features

### Ownership rules

These are product-direction rules, not a fully implemented flow yet:

- new users receive one free random pack from the free base pool
- users may own multiple packs
- users may activate only one pet at a time on the device
- buying a pack unlocks the whole pack

For the V1 base release, only the first random pet grant is part of the required scope. Store purchase, extra pet ownership, voice, and TTS remain follow-up expansions.

## What V1 does not do

- approvals from hardware
- multiple simultaneous devices
- multi-account CLI switching
- complex pet progression systems
- large transcript browsing on device
