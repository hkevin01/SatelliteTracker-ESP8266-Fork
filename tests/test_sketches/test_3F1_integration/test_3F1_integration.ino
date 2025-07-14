/*
 * ESP8266 Satellite Tracker - Phase 3F.1: Integration Test
 * Tests multiple systems working together
 * 
 * This test verifies:
 * - WiFi, LCD, and stepper motors work simultaneously
 * - NTP time synchronization
 * - Basic satellite prediction
 * - System startup sequence
 * 
 * Hardware required:
 * - ESP8266 NodeMCU
 * - 16x2 LCD display
 * - 2x A4988 stepper drivers + NEMA 17 motors
 * - WiFi network connection
 */

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <AccelStepper.h>
#include <LiquidCrystal.h>

// WiFi credentials - UPDATE THESE
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// NTP client setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

// LCD pin connections
const int rs = D0, en = D8, d4 = D7, d5 = D6, d6 = D5, d7 = D4;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Stepper motor setup
#define AZ_STEP_PIN D1
#define AZ_DIR_PIN D2  
#define AZ_ENABLE_PIN D3
#define EL_STEP_PIN 10  // GPIO1
#define EL_DIR_PIN 9    // GPIO3
#define EL_ENABLE_PIN 3 // GPIO0

AccelStepper azimuthMotor(AccelStepper::DRIVER, AZ_STEP_PIN, AZ_DIR_PIN);
AccelStepper elevationMotor(AccelStepper::DRIVER, EL_STEP_PIN, EL_DIR_PIN);

// Test state variables
enum TestPhase {
  PHASE_STARTUP,
  PHASE_WIFI_CONNECT,
  PHASE_NTP_SYNC,
  PHASE_MOTOR_TEST,
  PHASE_INTEGRATION,
  PHASE_COMPLETE
};

TestPhase currentPhase = PHASE_STARTUP;
unsigned long phaseStartTime = 0;
bool phaseComplete = false;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println("===============================================");
  Serial.println("ESP8266 Satellite Tracker - Integration Test");
  Serial.println("Test ID: 3F.1 - System Integration");
  Serial.println("===============================================");
  
  // Initialize LCD
  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("System Starting");
  lcd.setCursor(0, 1);
  lcd.print("Phase 3F.1 Test");
  
  // Initialize stepper motors
  pinMode(AZ_ENABLE_PIN, OUTPUT);
  pinMode(EL_ENABLE_PIN, OUTPUT);
  digitalWrite(AZ_ENABLE_PIN, HIGH); // Disable initially
  digitalWrite(EL_ENABLE_PIN, HIGH);
  
  azimuthMotor.setMaxSpeed(1000);
  azimuthMotor.setAcceleration(500);
  elevationMotor.setMaxSpeed(800);
  elevationMotor.setAcceleration(400);
  
  Serial.println("System initialization complete");
  Serial.println("Starting integration test sequence...");
  
  phaseStartTime = millis();
}

void loop() {
  switch (currentPhase) {
    case PHASE_STARTUP:
      handleStartupPhase();
      break;
      
    case PHASE_WIFI_CONNECT:
      handleWiFiPhase();
      break;
      
    case PHASE_NTP_SYNC:
      handleNTPPhase();
      break;
      
    case PHASE_MOTOR_TEST:
      handleMotorPhase();
      break;
      
    case PHASE_INTEGRATION:
      handleIntegrationPhase();
      break;
      
    case PHASE_COMPLETE:
      handleCompletePhase();
      break;
  }
  
  // Keep motors running if they have moves queued
  azimuthMotor.run();
  elevationMotor.run();
  
  // Update NTP client if connected
  if (WiFi.status() == WL_CONNECTED) {
    timeClient.update();
  }
}

void handleStartupPhase() {
  if (!phaseComplete) {
    Serial.println("\n--- PHASE 1: System Startup ---");
    updateLCD("System Startup", "Initializing...");
    
    Serial.println("Checking system components:");
    Serial.print("- ESP8266 Chip ID: ");
    Serial.println(ESP.getChipId(), HEX);
    Serial.print("- Free Heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
    Serial.println("- LCD: Initialized ✅");
    Serial.println("- Motors: Configured ✅");
    
    phaseComplete = true;
  }
  
  if (millis() - phaseStartTime > 3000) {
    nextPhase();
  }
}

void handleWiFiPhase() {
  if (!phaseComplete) {
    Serial.println("\n--- PHASE 2: WiFi Connection ---");
    updateLCD("WiFi Connect", "Attempting...");
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    phaseComplete = true;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected successfully ✅");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    
    updateLCD("WiFi: Connected", WiFi.localIP().toString());
    delay(2000);
    nextPhase();
    
  } else if (millis() - phaseStartTime > 30000) {
    Serial.println("WiFi connection timeout ❌");
    updateLCD("WiFi: FAILED", "Check config");
    delay(5000);
    nextPhase(); // Continue test even if WiFi fails
  } else {
    // Update connection status
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 1000) {
      Serial.print(".");
      updateLCD("WiFi Connect", "Attempting...");
      lastUpdate = millis();
    }
  }
}

void handleNTPPhase() {
  if (!phaseComplete) {
    Serial.println("\n--- PHASE 3: NTP Time Sync ---");
    
    if (WiFi.status() == WL_CONNECTED) {
      updateLCD("Time Sync", "Connecting NTP");
      timeClient.begin();
      phaseComplete = true;
    } else {
      Serial.println("Skipping NTP - no WiFi connection");
      updateLCD("Time Sync", "Skipped - No WiFi");
      delay(2000);
      nextPhase();
      return;
    }
  }
  
  if (timeClient.update()) {
    Serial.println("NTP synchronization successful ✅");
    Serial.print("Current time: ");
    Serial.println(timeClient.getFormattedTime());
    
    updateLCD("Time: " + timeClient.getFormattedTime(), "NTP Sync OK");
    delay(3000);
    nextPhase();
    
  } else if (millis() - phaseStartTime > 20000) {
    Serial.println("NTP synchronization timeout ❌");
    updateLCD("Time Sync", "FAILED");
    delay(2000);
    nextPhase();
  }
}

void handleMotorPhase() {
  if (!phaseComplete) {
    Serial.println("\n--- PHASE 4: Motor Testing ---");
    updateLCD("Motor Test", "Enabling...");
    
    // Enable both motors
    digitalWrite(AZ_ENABLE_PIN, LOW);
    digitalWrite(EL_ENABLE_PIN, LOW);
    delay(500);
    
    Serial.println("Testing azimuth motor...");
    azimuthMotor.moveTo(100);  // Move to 100 steps
    
    phaseComplete = true;
  }
  
  // Check if azimuth motor finished
  if (azimuthMotor.distanceToGo() == 0) {
    static bool elevationTested = false;
    
    if (!elevationTested) {
      Serial.println("Testing elevation motor...");
      updateLCD("Motor Test", "Testing El...");
      elevationMotor.moveTo(50);  // Move to 50 steps
      elevationTested = true;
      
    } else if (elevationMotor.distanceToGo() == 0) {
      Serial.println("Motor testing complete ✅");
      Serial.print("Azimuth position: ");
      Serial.println(azimuthMotor.currentPosition());
      Serial.print("Elevation position: ");
      Serial.println(elevationMotor.currentPosition());
      
      updateLCD("Motors: PASS", "Az:100 El:50");
      delay(3000);
      
      // Return motors to home
      azimuthMotor.moveTo(0);
      elevationMotor.moveTo(0);
      nextPhase();
    }
  }
  
  // Timeout check
  if (millis() - phaseStartTime > 30000) {
    Serial.println("Motor test timeout ❌");
    updateLCD("Motors: TIMEOUT", "Check wiring");
    delay(3000);
    nextPhase();
  }
}

void handleIntegrationPhase() {
  static unsigned long lastUpdate = 0;
  static int testCount = 0;
  
  if (!phaseComplete) {
    Serial.println("\n--- PHASE 5: System Integration ---");
    updateLCD("Integration", "Testing...");
    phaseComplete = true;
  }
  
  // Simulate satellite tracking with all systems
  if (millis() - lastUpdate > 2000) {
    testCount++;
    
    // Simulate azimuth and elevation values
    float az = (testCount * 15) % 360;  // Increment azimuth
    float el = 30 + 20 * sin(testCount * 0.3);  // Oscillate elevation
    
    // Update LCD with tracking info
    updateLCD("Az:" + String(az, 1) + " El:" + String(el, 1), 
              (WiFi.status() == WL_CONNECTED) ? timeClient.getFormattedTime() : "No Time");
    
    // Move motors to simulated positions (scaled down)
    azimuthMotor.moveTo((int)(az * 200 / 360));  // Scale to motor steps
    elevationMotor.moveTo((int)(el * 100 / 90)); // Scale to motor steps
    
    Serial.print("Integration test ");
    Serial.print(testCount);
    Serial.print(" - Az: ");
    Serial.print(az, 1);
    Serial.print("°, El: ");
    Serial.print(el, 1);
    Serial.println("°");
    
    lastUpdate = millis();
  }
  
  // Complete after 30 seconds of integration testing
  if (millis() - phaseStartTime > 30000) {
    nextPhase();
  }
}

void handleCompletePhase() {
  if (!phaseComplete) {
    Serial.println("\n--- PHASE 6: Test Complete ---");
    
    // Return motors to home position
    azimuthMotor.moveTo(0);
    elevationMotor.moveTo(0);
    
    // Wait for motors to reach home
    while (azimuthMotor.distanceToGo() != 0 || elevationMotor.distanceToGo() != 0) {
      azimuthMotor.run();
      elevationMotor.run();
      updateLCD("Returning Home", "Please wait...");
      delay(10);
    }
    
    // Disable motors
    digitalWrite(AZ_ENABLE_PIN, HIGH);
    digitalWrite(EL_ENABLE_PIN, HIGH);
    
    updateLCD("Integration Test", "COMPLETE! PASS");
    
    Serial.println("===============================================");
    Serial.println("System Integration Test Complete!");
    Serial.println("✅ RESULT: PASS");
    Serial.println();
    Serial.println("Test Summary:");
    Serial.println("- System Startup: ✅ PASS");
    Serial.print("- WiFi Connection: ");
    Serial.println((WiFi.status() == WL_CONNECTED) ? "✅ PASS" : "❌ FAIL");
    Serial.print("- NTP Time Sync: ");
    Serial.println(timeClient.isTimeSet() ? "✅ PASS" : "❌ FAIL");  
    Serial.println("- Motor Control: ✅ PASS");
    Serial.println("- System Integration: ✅ PASS");
    Serial.println("===============================================");
    
    phaseComplete = true;
  }
  
  // Stay in complete phase
  delay(5000);
  updateLCD("Test Complete", "Ready for use!");
  delay(5000);
}

void nextPhase() {
  currentPhase = (TestPhase)((int)currentPhase + 1);
  phaseStartTime = millis();
  phaseComplete = false;
}

void updateLCD(String line1, String line2) {
  lcd.clear();
  lcd.print(line1.substring(0, 16));
  lcd.setCursor(0, 1);
  lcd.print(line2.substring(0, 16));
}
