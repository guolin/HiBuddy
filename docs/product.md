# Buddy Product

## Positioning

Buddy V1 is a small AI companion device built for `M5StickS3`.

It is not:

- a terminal
- a chat client
- a dense dashboard

It is:

- a glanceable AI companion
- a pet-driven status display
- a lightweight reminder and ambient presence device

## Core Product Model

Buddy V1 uses a two-layer runtime:

- upstream `snapshot` describes AI work truth
- device-local pet logic turns that truth into animation and personality

The device should feel alive, but it should not invent fake work facts.

## Fixed V1 Hardware

Current reference hardware:

- `M5StickS3`

Hardware assumptions:

- `135x240` portrait screen
- two hardware buttons
- IMU available
- battery-powered handheld behavior

## Main User Value

The user should understand at a glance:

- whether the agent is active
- whether something needs attention
- whether work just finished
- how the pet feels right now

## Product Principles

### Upstream computes work truth

Upstream is responsible for:

- active sessions
- waiting sessions
- error state
- focus title
- token summary

### Device computes pet behavior

The device is responsible for:

- pet animation state
- `mood / energy / focus`
- shake response
- sleep / wake behavior
- local rendering and pacing

### Pet emotion should not mirror raw internal wording

Buddy should react emotionally, not look like a log viewer.

## Main Pages

Buddy V1 has three main pages.

### Pet

Shows:

- the pet as the main visual
- session status dots
- optional short state word when useful

Does not show:

- bars
- detailed device information

### Overview

Shows:

- `mood`
- `energy`
- `focus`

Representation:

- three compact bars
- no raw numeric values

This is the pet summary page.

### Info

Shows device information only:

- battery
- charging
- Wi‑Fi
- SSID
- firmware version

This is not the pet summary page.

## Pet Attributes

V1 uses three local pet attributes:

- `mood`
- `energy`
- `focus`

V1 does not include `affection` as a formal runtime stat.

### Mood

Represents recent emotional tone.

### Energy

Represents stamina and restfulness.

### Focus

Represents how concentrated the pet currently feels.

## Motion and Sleep

### Friendly shake

A light, infrequent shake can:

- slightly improve mood
- trigger a short positive animation

### Rough shake

A harsh or frequent shake should:

- trigger dizzy behavior
- not keep rewarding the pet

### Face-down nap

Face-down sleep is a core V1 behavior.

It should:

- put the device into pet sleep mode
- restore energy
- feel like a natural rest action for handheld hardware

## What V1 Does Not Do

- `affection` as a live system stat
- cloud-synced pet mood
- multiple pets at runtime
- large transcript browsing
- hardware approvals
- complex progression systems
