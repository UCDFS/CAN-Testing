# Teensy Motor Controller — Startup Procedure

## Hardware checklist before powering on
- SD card inserted (optional — system runs without it, logging disabled if absent)
- Nextion display connected to TX1/RX1 (pins 1/0), powered
- Button wired between GND and pin 2
- APPS1 connected to A0, APPS2 connected to A1
- CAN bus connected, 120Ω termination resistors in place
- BAMOCAR powered and on the CAN bus
- Pedal at physical rest (not pressed)

---

## Startup sequence

Power is applied. No button press required until Step 1.

### Boot screen (Page 0)

| # | Status line | Detail line | Action required |
|---|---|---|---|
| 1 | `INITIALISING` | — | None — automatic |
| 2a | `SD: OK` | e.g. `CAN_log_0001.csv` | None — automatic |
| 2b | `SD: NONE` | `logging disabled` | None — system continues without logging |
| 2c | `SD: ERROR` | `file open failed` | None — system continues without logging |
| 3 | `STEP 1: START` | `press to continue` | **Press button** to begin BAMOCAR bring-up |
| 4 | `WAITING BAMOCAR` | — | None — polls until BAMOCAR responds on CAN |
| 5 | `BAMOCAR ONLINE` | — | None — automatic, brief pause |
| 6 | `STEP 2: DC BUS` | `press to continue` | **Press button** to request DC bus voltage |
| 7 | `STEP 3: CLR ERR` | `press to continue` | **Press button** to clear BAMOCAR error flags |
| 8 | `STEP 4: CAN TMO` | `press to continue` | **Press button** to configure CAN timeout (2000 ms) |
| 9 | `STEP 5: ENABLE` | `hold 3s to enable` | **Hold button for 3 seconds** to enable drive |
| 10 | `ENABLING DRIVE` | — | None — sends enable command and zero torque |
| 11 | `RELEASE PEDAL` | — | None — waits automatically until pedal is at rest |
| 12 | `STEP 7: DRIVE` | `press to continue` | **Press button** to enter torque control |

Display switches to drive screen (Page 1). System is now live.

---

## Drive screen (Page 1)

Updates every 500 ms.

| Component | Label | Values |
|---|---|---|
| `n_speed` | km/h | Integer vehicle speed |
| `n_rpm` | RPM | Motor RPM from BAMOCAR |
| `n_torque` | % | Torque command, 0–100% |
| `n_dcbus` | V | DC bus voltage, whole volts |
| `t_fault` | — | `OK` or `FAULT` |
| `t_drive` | — | `ON`, `OFF`, or `RELEASE PEDAL` |

### `t_fault` values
| Text | Meaning |
|---|---|
| `OK` | Both APPS sensors agree, no plausibility fault |
| `FAULT` | APPS1 and APPS2 disagree by more than 10% — torque zeroed, latched until pedal released |

### `t_drive` values
| Text | Meaning |
|---|---|
| `ON` | Drive enabled, torque control active |
| `OFF` | Drive disabled via button press |
| `RELEASE PEDAL` | Button pressed to re-enable but pedal was not at rest — release pedal and press again |

---

## Drive enable/disable toggle (Page 1)

Button press while on the drive screen:

- **Drive ON → press button** — disables drive, zeros torque, display shows `OFF`
- **Drive OFF → hold button 3 s (pedal at rest)** — clears errors, re-enables drive, display shows `ON`
- **Drive OFF → hold button 3 s (pedal pressed)** — display shows `RELEASE PEDAL`, nothing happens; release pedal and hold again

---

## Notes
- If `WAITING BAMOCAR` hangs indefinitely: check CAN wiring, termination, and that BAMOCAR CAN IDs match `0x181` / `0x201`
- If `FAULT` appears immediately on pressing pedal: APPS sensors not calibrated or plausibility check failing — check `APPS1_REST/FULL` and `APPS2_REST/FULL` values in `config.h`
- The system will not send torque commands until the drive screen is active and drive is `ON`
- If drive is disabled and left idle, the BAMOCAR CAN timeout (2000 ms) will flag a timeout fault internally; this is cleared automatically by the `clearErrors()` call on the next re-enable
