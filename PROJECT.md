# MakeSense IoT Kit (firmware examples)

- **Owner:** Own
- **Status:** Paused
- **Stack:** Arduino firmware for ESP32 / ESP8266 (Blynk, MQTT)
- **Hosting / deploy:** n/a (firmware examples)
- **Template:** none
- **Last updated:** 2026-10-26

## Goal
Arduino firmware examples for the MakeSense IoT education kit (Roboshop Technologies), used for IoT teaching and prototyping. Related to the MakeSense Cloud platform in the iot-roboshoptech repo.

## Current state
- 10 numbered examples: 3 Blynk 2.0 (gas, soil moisture) and 7 MQTT (relay, gas, pH, soil, smart home with DHT11/OLED).
- FarmSense nodes included: ESP8266 4-channel MQTT relay (04) and ESP32 MQTT soil node (07), added 2026-03-16.
- README and kit documentation PDF in `00-Documentation/`.
- Last commit on main: 2026-03-20 (repo reorganization). No activity since.
- Local branch `code` has 1 unmerged commit ("Update device info and relay pin mapping", 2026-03-20); its remote branch is gone.

## Next steps
1. Decide whether to push/merge or drop the local `code` branch commit.
2. ?
3. ?

## Customer commitments

## Decisions log
- 2026-10-26: PROJECT.md added as the source of truth for status and next steps.
- 2026-03-20: Repo reorganized into numbered example folders with headers and a README.
