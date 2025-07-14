# ESP8266 Satellite Tracker Test Plan

## Overview
This test plan covers Phase 3 hardware interface testing for the ESP8266 satellite tracker system. The goal is to verify all hardware components function correctly with the migrated ESP8266 code before proceeding to full system integration.

## Test Environment Setup

### Required Hardware
- **ESP8266 NodeMCU v2** development board
- **2x A4988 Stepper Motor Drivers**
- **2x NEMA 17 Stepper Motors** (200 steps/revolution)
- **16x2 LCD Display** (HD44780 compatible)
- **Piezo Buzzer/Speaker**
- **12V Power Supply** (2A minimum for motors)
- **5V Power Supply** for ESP8266 and logic
- **Breadboard and jumper wires**
- **Multimeter**
- **Oscilloscope** (optional, for timing verification)

### Software Tools
- **Arduino CLI** (already installed)
- **Serial Monitor** for debugging
- **WiFi Network** with internet access
- **Celestrak.org** access for TLE testing

### Pin Configuration for Testing
```cpp
// Azimuth Stepper Motor (A4988)
#define AZmotorPin1 D1  // GPIO5  - STEP
#define AZmotorPin2 D2  // GPIO4  - DIR
#define AZmotorPin3 D3  // GPIO0  - ENABLE
#define AZmotorPin4 D4  // GPIO2  - MS1

// Elevation Stepper Motor (A4988)  
#define ELmotorPin1 D5  // GPIO14 - STEP
#define ELmotorPin2 D6  // GPIO12 - DIR
#define ELmotorPin3 D7  // GPIO13 - ENABLE
#define ELmotorPin4 D8  // GPIO15 - MS1

// LCD Display
#define LCD_RS D0    // GPIO16 - Register Select
#define LCD_EN A0    // ADC0   - Enable
#define LCD_D4 D9    // GPIO3  - Data 4
#define LCD_D5 D10   // GPIO1  - Data 5  
#define LCD_D6 9     // GPIO9  - Data 6
#define LCD_D7 10    // GPIO10 - Data 7

// Piezo Speaker
#define SPEAKER_PIN 3  // GPIO3
```

## Test Phases

## Phase 3A: Basic Hardware Tests (30 minutes)

### Test 3A.1: ESP8266 Basic Function Test
**Objective**: Verify ESP8266 boots and basic I/O works
**Duration**: 5 minutes

**Procedure**:
1. Flash simple blink sketch to ESP8266
2. Verify LED blinks on GPIO2 (built-in LED)
3. Check serial output at 115200 baud
4. Verify 3.3V and GND pins provide stable power

**Expected Results**:
- Built-in LED blinks every 1 second
- Serial output shows "Hello ESP8266" message
- 3.3V pin measures 3.3V ±0.1V
- No unexpected resets or brownouts

**Pass/Fail Criteria**: ✅ Pass if all outputs work correctly

---

### Test 3A.2: Power Supply Verification  
**Objective**: Verify power supplies are adequate
**Duration**: 10 minutes

**Procedure**:
1. Measure 12V supply under no load
2. Connect both A4988 drivers (no motors)
3. Measure 12V supply voltage drop
4. Connect motors and measure voltage under load
5. Check ESP8266 supply voltage stability

**Expected Results**:
- 12V supply: 12V ±0.5V under full load
- 5V supply: 5V ±0.2V 
- ESP8266 3.3V: 3.3V ±0.1V
- No voltage drops >5% during motor operation

**Pass/Fail Criteria**: ✅ Pass if all voltages within tolerance

---

### Test 3A.3: Pin Connectivity Test
**Objective**: Verify all GPIO pins respond correctly  
**Duration**: 15 minutes

**Procedure**:
1. Create test sketch that toggles each GPIO pin
2. Use multimeter to verify voltage changes
3. Test all defined pins in pin configuration
4. Check for any pin conflicts or shorts

**Expected Results**:
- Each GPIO pin toggles between 0V and 3.3V
- No unexpected voltage levels
- No shorts between pins
- All pins respond to digitalWrite() commands

**Pass/Fail Criteria**: ✅ Pass if all pins function correctly

---

## Phase 3B: WiFi and Network Tests (20 minutes)

### Test 3B.1: WiFi Connection Test
**Objective**: Verify ESP8266 connects to WiFi reliably
**Duration**: 10 minutes

**Procedure**:
1. Upload WiFi connection test sketch
2. Monitor connection process via serial
3. Test connection to home/test network
4. Verify IP address assignment
5. Test connection stability over 5 minutes

**Expected Results**:
- Connects to WiFi within 30 seconds
- Receives valid IP address via DHCP
- Connection remains stable
- Signal strength >-70 dBm for reliable operation

**Test Code**:
```cpp
#include <ESP8266WiFi.h>

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi Connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Signal: ");
  Serial.println(WiFi.RSSI());
}
```

**Pass/Fail Criteria**: ✅ Pass if connection successful and stable

---

### Test 3B.2: HTTPS/TLE Download Test
**Objective**: Verify HTTPS connection to Celestrak works
**Duration**: 10 minutes

**Procedure**:
1. Upload TLE download test sketch
2. Attempt to connect to celestrak.org via HTTPS
3. Download sample TLE data
4. Verify SSL handshake succeeds
5. Parse and validate TLE format

**Expected Results**:
- SSL connection to celestrak.org succeeds
- TLE data downloads successfully
- Data format matches expected TLE structure
- No SSL certificate errors (with setInsecure mode)

**Test Code**:
```cpp
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

WiFiClientSecure client;

void testTLEDownload() {
  client.setInsecure();
  if (client.connect("celestrak.org", 443)) {
    Serial.println("HTTPS connection successful!");
    client.println("GET /satcat/tle.php?CATNR=25544 HTTP/1.1");
    client.println("Host: celestrak.org");
    client.println("Connection: close");
    client.println();
    
    // Read response...
  }
}
```

**Pass/Fail Criteria**: ✅ Pass if TLE data downloads successfully

---

## Phase 3C: Stepper Motor Tests (45 minutes)

### Test 3C.1: Single Motor Basic Movement
**Objective**: Verify individual stepper motors respond to commands
**Duration**: 15 minutes

**Procedure**:
1. Connect one A4988 driver and motor
2. Upload basic stepper test code
3. Test clockwise rotation (200 steps)
4. Test counter-clockwise rotation (200 steps)
5. Verify smooth movement without skipping

**Expected Results**:
- Motor rotates exactly 360° for 200 steps
- Movement is smooth without vibration
- No missed steps or irregular movement
- Motor can be stopped and started reliably

**Test Code**:
```cpp
#include <AccelStepper.h>

#define STEP_PIN D1
#define DIR_PIN D2
#define ENABLE_PIN D3

AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

void setup() {
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, LOW); // Enable driver
  
  stepper.setMaxSpeed(1000);
  stepper.setAcceleration(500);
}

void loop() {
  stepper.moveTo(200);   // 360 degrees
  stepper.runToPosition();
  delay(1000);
  
  stepper.moveTo(0);     // Return to start
  stepper.runToPosition();
  delay(1000);
}
```

**Pass/Fail Criteria**: ✅ Pass if motor rotates smoothly and accurately

---

### Test 3C.2: Dual Motor Coordination
**Objective**: Verify both motors can operate simultaneously  
**Duration**: 20 minutes

**Procedure**:
1. Connect both A4988 drivers and motors
2. Upload dual motor test code
3. Test simultaneous movement of both motors
4. Test independent movement control
5. Verify no interference between motors

**Expected Results**:
- Both motors operate simultaneously without issues
- Independent speed and direction control works
- No electrical interference between drivers
- Motors can handle coordinated movements

**Pass/Fail Criteria**: ✅ Pass if both motors work independently and together

---

### Test 3C.3: Load Testing with Saturn V Model
**Objective**: Verify motors can handle actual payload
**Duration**: 10 minutes

**Procedure**:
1. Attach 3D printed Saturn V model (or equivalent weight)
2. Test azimuth motor with rotational load
3. Test elevation motor with gravitational load
4. Verify tracking accuracy under load
5. Check for motor stalling or overheating

**Expected Results**:
- Motors can move payload without stalling
- Positioning accuracy maintained under load
- No excessive motor heating (>50°C)
- Smooth movement throughout full range

**Pass/Fail Criteria**: ✅ Pass if motors handle load without performance degradation

---

## Phase 3D: LCD Display Tests (20 minutes)

### Test 3D.1: LCD Basic Function Test
**Objective**: Verify LCD displays text correctly
**Duration**: 10 minutes

**Procedure**:
1. Connect 16x2 LCD to ESP8266
2. Upload LCD test sketch
3. Display test patterns and text
4. Verify all character positions work
5. Test contrast and readability

**Expected Results**:
- LCD initializes without errors
- All 32 character positions (16x2) display correctly
- Text is clear and readable
- No missing segments or garbled characters

**Test Code**:
```cpp
#include <LiquidCrystal.h>

LiquidCrystal lcd(D0, A0, D9, D10, 9, 10);

void setup() {
  lcd.begin(16, 2);
  lcd.print("ESP8266 Tracker");
  lcd.setCursor(0, 1);
  lcd.print("LCD Test OK!");
}

void loop() {
  // Cycle through test patterns
  for (int i = 0; i < 16; i++) {
    lcd.setCursor(i, 0);
    lcd.print("*");
    delay(200);
  }
}
```

**Pass/Fail Criteria**: ✅ Pass if all characters display correctly

---

### Test 3D.2: Dynamic Information Display
**Objective**: Test real-time data display updates
**Duration**: 10 minutes

**Procedure**:
1. Display simulated satellite tracking data
2. Update display every second with new values
3. Test azimuth and elevation value display
4. Verify no display flickering or corruption
5. Test special characters and degree symbols

**Expected Results**:
- Display updates smoothly without flickering
- Numbers and symbols display correctly
- Real-time updates work reliably
- Display remains readable during updates

**Pass/Fail Criteria**: ✅ Pass if dynamic updates work smoothly

---

## Phase 3E: Audio System Tests (15 minutes)

### Test 3E.1: Piezo Speaker Basic Test
**Objective**: Verify audio feedback system works
**Duration**: 10 minutes

**Procedure**:
1. Connect piezo speaker to designated pin
2. Upload audio test sketch
3. Test different frequencies (500Hz, 1000Hz, 2000Hz)
4. Test tone duration control
5. Verify audio levels are appropriate

**Expected Results**:
- Clear tones generated at all test frequencies
- Tone duration matches programmed values
- Audio level sufficient but not excessive
- No distortion or unwanted harmonics

**Test Code**:
```cpp
#define SPEAKER_PIN 3

void setup() {
  Serial.begin(115200);
}

void loop() {
  Serial.println("Testing 1000Hz tone");
  tone(SPEAKER_PIN, 1000, 500);
  delay(1000);
  
  Serial.println("Testing 2000Hz tone");
  tone(SPEAKER_PIN, 2000, 300);
  delay(1000);
  
  Serial.println("Testing startup melody");
  playStartupMelody();
  delay(3000);
}

void playStartupMelody() {
  tone(SPEAKER_PIN, 1000, 200);
  delay(250);
  tone(SPEAKER_PIN, 1500, 200);
  delay(250);
  tone(SPEAKER_PIN, 2000, 300);
}
```

**Pass/Fail Criteria**: ✅ Pass if all tones are clear and audible

---

### Test 3E.2: Event Notification Sounds
**Objective**: Test context-appropriate audio feedback
**Duration**: 5 minutes

**Procedure**:
1. Test startup confirmation sound
2. Test WiFi connection success sound
3. Test satellite pass notification sound
4. Test error notification sound
5. Verify sounds are distinguishable

**Expected Results**:
- Each event has distinct sound pattern
- Sounds are appropriate length (not annoying)
- Audio clearly indicates system status
- Sounds can be heard in typical operating environment

**Pass/Fail Criteria**: ✅ Pass if all notification sounds work correctly

---

## Phase 3F: Integration Tests (30 minutes)

### Test 3F.1: NTP Time Synchronization Test
**Objective**: Verify accurate time synchronization
**Duration**: 10 minutes

**Procedure**:
1. Upload NTP test sketch
2. Connect to NTP server
3. Display synchronized time on LCD
4. Compare with known accurate time source
5. Test timezone correction

**Expected Results**:
- NTP synchronization completes within 10 seconds
- Time accuracy within ±1 second of reference
- Timezone offset applied correctly
- Time updates periodically without issues

**Pass/Fail Criteria**: ✅ Pass if time synchronization is accurate

---

### Test 3F.2: Satellite Prediction Test
**Objective**: Verify SGP4 library calculates positions correctly
**Duration**: 10 minutes

**Procedure**:
1. Download fresh TLE for ISS
2. Calculate current satellite position
3. Display azimuth and elevation values
4. Compare with online satellite tracker
5. Verify predictions update correctly

**Expected Results**:
- Satellite positions calculate without errors
- Values match online trackers within ±2°
- Predictions update in real-time
- No mathematical errors or overflows

**Pass/Fail Criteria**: ✅ Pass if predictions are accurate

---

### Test 3F.3: Full System Startup Test
**Objective**: Test complete system initialization sequence
**Duration**: 10 minutes

**Procedure**:
1. Power cycle entire system
2. Monitor full startup sequence
3. Verify all components initialize correctly
4. Check LCD status display during startup
5. Confirm audio feedback at appropriate times

**Expected Results**:
- System boots within 60 seconds
- All hardware components respond
- LCD shows meaningful status messages
- Audio provides startup confirmation
- No hangs or error conditions

**Pass/Fail Criteria**: ✅ Pass if complete startup succeeds

---

## Test Results Documentation

### Test Result Format
For each test, record:
- **Test ID**: (e.g., 3A.1)
- **Date/Time**: When test was performed
- **Result**: ✅ PASS / ❌ FAIL / ⚠️ PARTIAL
- **Notes**: Any observations or issues
- **Action Items**: Required fixes or improvements

### Sample Test Log Entry
```
Test ID: 3C.1
Date: 2025-07-14 14:30
Result: ✅ PASS
Notes: Motor rotates smoothly, exactly 360° per 200 steps
Measured: 1.8° per step (expected: 1.8°)
Action Items: None
```

## Success Criteria for Phase 3

### Must Pass (Critical)
- ✅ ESP8266 basic functionality (3A.1)
- ✅ WiFi connection reliability (3B.1)
- ✅ Stepper motor basic movement (3C.1)
- ✅ LCD display functionality (3D.1)
- ✅ System startup sequence (3F.3)

### Should Pass (Important)
- ✅ HTTPS TLE downloads (3B.2)
- ✅ Dual motor coordination (3C.2)
- ✅ Dynamic LCD updates (3D.2)
- ✅ Audio notifications (3E.1)
- ✅ Time synchronization (3F.1)

### Nice to Pass (Desirable)
- ✅ Load testing with payload (3C.3)
- ✅ Event notification sounds (3E.2)
- ✅ Satellite prediction accuracy (3F.2)

## Risk Mitigation

### Common Issues and Solutions

**Issue**: Motors not responding
- **Check**: Power supply connections
- **Check**: A4988 driver configuration
- **Solution**: Verify enable pin is LOW

**Issue**: LCD not displaying
- **Check**: Contrast adjustment
- **Check**: Power connections (5V)
- **Solution**: Verify pin connections match code

**Issue**: WiFi connection fails
- **Check**: SSID and password
- **Check**: Signal strength
- **Solution**: Move closer to router or use different network

**Issue**: HTTPS connection fails
- **Check**: Internet connectivity
- **Check**: DNS resolution
- **Solution**: Try HTTP fallback for testing

## Next Steps After Phase 3

Upon successful completion of Phase 3 tests:
1. **Document all test results**
2. **Fix any failed tests before proceeding**
3. **Proceed to Phase 4: Enhanced Features & Reliability**
4. **Begin end-to-end satellite tracking tests**

---

**Total Estimated Test Time**: 2.5 hours
**Required for Phase 4**: All critical tests must pass
**Equipment Setup Time**: +30 minutes additional
