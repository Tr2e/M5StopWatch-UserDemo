# RuView Sentinel

RuView Sentinel is an experimental, camera-free room activity sensor for a single M5StopWatch and an existing 2.4 GHz Wi-Fi router. It uses the ESP32-S3 Channel State Information (CSI) receiver; it does not inspect network payloads and it does not identify people.

## Use

1. Configure Wi-Fi in the StopWatch Setup app and connect to a 2.4 GHz access point.
2. Open **RuView** and place the watch on a stable, non-metallic surface.
3. Leave the watch and room still while the 30-second calibration completes.
4. Read the center value as an experimental RF activity score, not a person count or medical measurement.
5. Press **A** to recalibrate, press **B** to cycle LOW/MED/HIGH sensitivity, and hold **A+B** to exit.

Calibration pauses whenever the IMU detects that the watch itself is moving. The app vibrates once when sustained RF activity crosses the current threshold.

During an activity event, the center detail shows its peak score, elapsed duration, and event number. After the room returns to stillness, the same line retains the last event age and peak until recalibration.

If a high activity score persists for at least 12 seconds and then remains stable at the new RF level for 6 seconds, RuView treats it as a lasting room change rather than endless motion. It closes the activity event, shows **ADAPTING BASELINE**, and learns the new stable environment for 15 seconds. Brief motion never enters this recovery path.

## What this first version measures

The app enables ESP-IDF CSI capture while the station remains connected, temporarily enables promiscuous reception, and filters CSI frames to the associated access point BSSID. Each callback reduces the I/Q subcarriers to an eight-band log-power fingerprint. The foreground state machine then:

- smooths the signal;
- learns a stationary mean and variance for 30 seconds;
- compares new samples to that local baseline;
- requires multiple samples to enter and leave the activity state;
- learns an activity threshold from the observed calibration noise;
- suppresses classification while the watch is moving.

While RuView is open, the watch sends a 16-byte UDP probe to the local gateway every 100 ms. The resulting Wi-Fi acknowledgements provide a steadier local CSI sample cadence without sending payload data to the internet.

This is deliberately a scalar presence/activity experiment. It does not implement RuView's server, neural pose model, multi-node localization, heart-rate estimation, or through-wall body reconstruction.

## Known limits

- CSI frame rate depends on access-point traffic and firmware behavior. `WAITING FOR SIGNAL` means the watch is connected but is not receiving usable CSI frames from the AP.
- A single antenna and a moving wearable cannot separate people, position, furniture changes, pets, doors, or interference reliably.
- Every room, access point, placement, and watch unit may require different thresholds.
- Metal surfaces, charging cables, hand contact, and access-point channel changes invalidate calibration.
- Promiscuous reception and continuous Wi-Fi increase power consumption; CSI is disabled when the app closes.

## Hardware validation checklist

- Confirm calibration reaches 100% with the watch untouched.
- Record idle frame rate, RSSI, and false alerts for at least 10 minutes.
- Walk into and out of the room ten times and record detections.
- Repeat with the watch moved to three placements.
- Confirm picking up the watch shows `WATCH MOVING`, not `ACTIVITY`.
- Confirm Wi-Fi, time sync, and other apps still work after leaving RuView.

The activity threshold and motion threshold are first-pass engineering defaults. Tune them from captured hardware observations rather than treating the current values as accuracy claims.
