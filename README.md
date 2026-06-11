# Posture Pilot

## Overview

Posture Pilot is an embedded ergonomic monitoring system designed to help users maintain healthy sitting posture while studying, coding, gaming, or working at a desk.

The system mounts an ultrasonic distance sensor on the backrest of a chair and continuously measures the distance between the chair and the user's back. After a one-time calibration step, Posture Pilot tracks posture drift relative to the user's personal baseline and detects slouching behavior in real time.

When prolonged slouching is detected, the system provides visual feedback through an LCD display and audio reminders through an MP3-TF-16P audio playback module and speaker. The current firmware also filters multiple ultrasonic readings before making a posture decision, which helps reduce false alerts caused by noise or small body shifts.

> **Keywords:** Arduino Uno R3 · ATmega328P · HC-SR04 · Embedded Systems · Human Factors Engineering · Ergonomics · Real-Time Monitoring · Sensor Processing · MP3-TF-16P · C Programming · AVR Development · Assistive Technology

## Motivation

Many students and engineers spend hours sitting at a desk and gradually develop poor posture without realizing it. Existing posture-monitoring products are often expensive, require wearable hardware, or rely on cameras that introduce privacy concerns.

As an engineering student, I spend long hours sitting while studying and working on projects. Over time, this led to noticeable middle back pain and eventually required a doctor visit. That experience made posture feel less like a small habit issue and more like a real health problem worth addressing early.

Posture Pilot explores a low-cost embedded alternative using simple distance sensing and personalized calibration to provide posture awareness without requiring computer vision or wearable electronics.

## Features

- Real-time posture monitoring
- Personalized posture calibration
- Chair-mounted sensing system
- LCD status display
- Startup voice prompt
- Voice-based posture reminders
- Audio break reminder
- Filtered ultrasonic sensing
- Sustained-slouch detection
- Adjustable slouch threshold
- Fully standalone embedded system

## Hardware Components

### Core Components

- Arduino Uno R3
- LCD Keypad Shield
- HC-SR04 Ultrasonic Sensor
- MP3-TF-16P Audio Module
- Small Speaker
- Breadboard
- Jumper Wires

### Optional

- External battery pack
- Chair mounting bracket
- 3D printed enclosure

## System Architecture

```text
            User
              ^
              |
              |
    HC-SR04 Ultrasonic Sensor
       Mounted on Chair Back
              |
              v
         Arduino Uno
         /         \
        v           v
   LCD Shield    MP3-TF-16P
                      |
                      v
                   Speaker
```

## Wiring

### HC-SR04 Ultrasonic Sensor

| HC-SR04 | Arduino Uno |
|---|---|
| VCC | 5V |
| GND | GND |
| TRIG | D3 / PD3 |
| ECHO | D2 / PD2 |

### MP3-TF-16P

| MP3-TF-16P | Arduino Uno |
|---|---|
| VCC | 5V |
| GND | GND |
| RX | D11 / PB3 through 1k ohm resistor |

The current code only transmits commands from the Arduino to the MP3 module. The MP3 `TX` pin is not used right now.

Recommended support parts for the MP3 module:

- `1k ohm` resistor between Arduino `D11` and MP3 `RX`
- `100 uF` capacitor across MP3 `VCC` and `GND`
- `0.1 uF` ceramic bypass capacitor near the module power pins
- FAT32-formatted microSD / TF card

### Speaker

| Speaker | MP3-TF-16P |
|---|---|
| Positive | SPK_1 |
| Negative | SPK_2 |

### LCD Shield

The LCD Keypad Shield plugs directly onto the Arduino Uno and provides:

- 16x2 LCD display
- `SELECT` button used for calibration
- Built-in navigation buttons

LCD wiring follows the pin mapping used by [lcd.c](lcd.c), and the `SELECT` button is read on `A0 / ADC0`.

## Calibration Procedure

1. Sit upright in your chair.
2. Press the LCD shield `SELECT` button.
3. Posture Pilot stores the current distance measurement as the baseline posture.
4. The system announces: `Calibration complete.`
5. Continuous posture monitoring begins.

The baseline is captured from several filtered distance readings instead of a single raw measurement, which makes calibration more stable.

## Detection Logic

The HC-SR04 continuously measures the distance between the chair back and the user's back.

The current firmware improves reliability in two ways:

- It filters several ultrasonic samples and uses a trimmed average before updating the posture state.
- It requires several consecutive slouch-like readings before triggering the slouch condition.

### Good Posture

`Current Distance ~= Baseline Distance`

LCD:

`Posture Good`

`Score: 95%`

### Slouching

`Current Distance > Baseline Distance`

When the distance exceeds the configured threshold for several consecutive filtered readings:

LCD:

`SLOUCHING!`

`Score: 42%`

Audio:

`Please sit upright.`

## Audio Prompts

Stored on the microSD card connected to the MP3-TF-16P module.

```text
/mp3/0001.mp3  Please sit upright.
/mp3/0002.mp3  Calibration complete.
/mp3/0003.mp3  Time to take a break.
/mp3/0004.mp3  System starting.
```

These prompts match the current code:

- Track `1`: slouch warning
- Track `2`: calibration complete
- Track `3`: break reminder
- Track `4`: startup prompt

## Example LCD Screens

### Startup

```text
Posture Pilot
Press SELECT
```

### Calibrated

```text
Calibrated!
Base: 14 cm
```

### Good Posture

```text
Posture Good
Score: 94%
```

### Slouching

```text
SLOUCHING!
Score: 51%
```

## Current Progress

The initial design is finished. I used a massage back pad and a hair clip to secure the ultrasonic sensor to the chair back, and the breadboard and Arduino are mounted on the chair arm. I am still waiting for the speaker and TF card to arrive.

### Ultrasonic Sensor Mounted on the Back Pad

<img src="asset/Ultrasonic_Sensor.jpg" alt="Ultrasonic sensor mounted on massage back pad" width="320" />

### Full Chair Setup

<img src="asset/Arm_chair.jpg" alt="Full chair setup with mounted sensor and electronics" width="320" />

### Breadboard and Arduino on Chair Arm

<img src="asset/Breadboard.jpg" alt="Breadboard and Arduino mounted on chair arm" width="320" />

## Cost Table

| Item | Unit Cost | Notes | Link |
|---|---:|---|---|
| Ultrasonic Distance Sensor - 5V (HC-SR04) | $5.25 | Single sensor | [SparkFun](https://www.sparkfun.com/ultrasonic-distance-sensor-hc-sr04.html) |
| MP3-TF-16P | $1.75 | Calculated from $13.99 for 8 pieces | [Amazon](https://www.amazon.com/Yuuhseel-Player-Support-Compatible-DFPlayer/dp/B0G133NWK3/ref=sr_1_1_sspa?crid=LT0E1SVX54BU&dib=eyJ2IjoiMSJ9.H0jpaE0ezUCMJNWoRgfomFCbnXaH14IgV1fY5wK6_3gEullOGD34a0hsuDnfRww46G5lbXlxsXnub8Tar6oA5QjrJAw4n0BB9_jbHabY1N5jCYA3zNQ8-ZnKliPtOSNHd7n_a7gm7CVLo11NCWqsQrXpwiciMUMqrATHGz77Jarr2RFBgtxGwUUh5F4kkV_m488hJ2SB2yDmdYnKpJn3fPEv58UoXlCHm214um-d8H8.sKYE_tEpR9mL1udf5tpWVm9iruptCX2BJWyZTqRHs8&dib_tag=se&keywords=MP3-TF-16P&qid=1780804720&sprefix=%2Caps%2C172&sr=8-1-spons&sp_csd=d2lkZ2V0TmFtZT1zcF9hdGY&psc=1) |
| Small speaker | $1.50 | Calculated from $8.99 for 6 pieces | [Amazon](https://www.amazon.com/dp/B09MRK24PP?ref=ppx_yo2ov_dt_b_fed_asin_title&th=1) |
| TF card | $9.50 | Single card | N/A |
| 1602 LCD Keypad Shield for Arduino | $9.90 | Single unit | [DigiKey](https://www.digikey.com/en/products/detail/dfrobot/DFR0009/7597118?so=98720144&content=productdetail_row&mkt_tok=MDI4LVNYSy01MDcAAAGhPfLEqpM2lLR6KASRUN78ibixmfFmJAGMbjnvwCN-sx2UC8WJq3Vyp58bAfi6u29iFtyrukjS05FqgW1wGS4EKDaJKCcMvChPn558nLkDb9g) |
| Arduino Uno R3 ATmega328P board | $27.60 | Single board | [DigiKey](https://www.digikey.com/en/products/detail/arduino/A000066/2784006?so=98720144&content=productdetail_row&mkt_tok=MDI4LVNYSy01MDcAAAGhPfLEqjvMaCWBQzWS6pyKdkj6A046_sR2P6MLYOB1pscW2D-1i5ohHERtZRA0Piug_8kmyllGEOJZGrbs2klqcQNpykopjkmMgRxSeQ3mSBg) |

**Estimated total cost:** `$55.50`

## Hardware Integration Notes

- Keep the ultrasonic sensor aimed as squarely as possible toward the user's mid-back so the reflected signal stays consistent.
- Make sure the Arduino, MP3 module, and ultrasonic sensor all share the same ground.
- Place the `100 uF` and `0.1 uF` capacitors close to the MP3-TF-16P power pins.
- Use short jumper wires around the MP3 module and speaker to reduce noise and accidental disconnects.
- If MP3 playback causes resets or unstable distance readings, recheck grounding and consider a cleaner 5V supply path.
- For a cleaner prototype, replace loose chair-arm wiring with zip ties, adhesive cable clips, or a small mounting plate.

## Demo Script

1. Power on the system and wait for the startup screen and startup audio.
2. Press the LCD shield `SELECT` button while sitting upright.
3. Show the calibration complete screen and audio prompt.
4. Sit with good posture and show the stable `Posture Good` status.
5. Deliberately slouch for several seconds to demonstrate sustained-slouch detection.
6. Let the system play the posture warning audio.
7. Explain that the code filters multiple sensor readings before deciding whether the user is slouching.

## Testing Notes

- Verify that calibration can be repeated multiple times without resetting power.
- Confirm that the HC-SR04 returns stable readings at the expected back-to-chair distance.
- Check that the TF card is readable and formatted correctly before final assembly.
- Confirm that the audio files are named exactly `0001.mp3`, `0002.mp3`, `0003.mp3`, and `0004.mp3`.
- Test both brief posture shifts and sustained slouching so the threshold can be tuned if needed.

## Technologies

- Embedded C
- ATmega328P
- Arduino Uno R3
- Ultrasonic sensing
- Serial communication
- MP3 audio playback
- Real-time monitoring
- Human factors engineering

## Acknowledgment

I learned much of the low-level embedded programming style used in this project through USC's EE109 course, especially topics such as LCD interfacing and pin change interrupts. If you are interested in the course material and lab structure, you can read through the EE109 lab page here:

[USC EE109 Labs](https://bytes.usc.edu/ee109/labs/)

## Current Code Wiring Summary

- LCD: uses the existing [lcd.c](/Users/harvardsummer/Library/Mobile%20Documents/com~apple~CloudDocs/GitHub/PosturePilot/lcd.c) shield wiring
- LCD `SELECT` button: `A0 / ADC0`
- HC-SR04 `TRIG`: `D3 / PD3`
- HC-SR04 `ECHO`: `D2 / PD2`
- MP3-TF-16P `RX`: `D11 / PB3` through `1k` resistor
- MP3-TF-16P `TX`: not used in the current code
- Speaker: `SPK_1` and `SPK_2`

2026 copyright wuisabel-gif
