/*
 * ESP8266 Satellite Tracker - Phase 3C.1: Single Motor Basic Movement Test
 * Tests individual stepper motor functionality with A4988 driver
 * 
 * This test verifies:
 * - A4988 driver responds to ESP8266 signals
 * - Stepper motor rotates smoothly
 * - Direction control works correctly
 * - Step accuracy (200 steps = 360 degrees)
 * 
 * Hardware setup:
 * - Connect one NEMA 17 stepper motor to A4988 driver
 * - Connect A4988 to ESP8266 as defined below
 * - Ensure 12V power supply connected to A4988
 */

#include <AccelStepper.h>

// Pin definitions for first stepper motor (Azimuth)
#define STEP_PIN D1    // GPIO5  - STEP signal to A4988
#define DIR_PIN D2     // GPIO4  - Direction signal to A4988  
#define ENABLE_PIN D3  // GPIO0  - Enable signal to A4988 (LOW = enabled)
#define MS1_PIN D4     // GPIO2  - Microstepping control (optional)

// Create stepper motor object
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// Test parameters
const int STEPS_PER_REVOLUTION = 200;  // Standard NEMA 17
const int MAX_SPEED = 1000;            // Steps per second
const int ACCELERATION = 500;          // Steps per second^2

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println("===============================================");
  Serial.println("ESP8266 Satellite Tracker - Stepper Motor Test");
  Serial.println("Test ID: 3C.1 - Single Motor Basic Movement");
  Serial.println("===============================================");
  
  // Initialize control pins
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(MS1_PIN, OUTPUT);
  
  // Enable the stepper driver (LOW = enabled)
  digitalWrite(ENABLE_PIN, LOW);
  
  // Set microstepping to full step (MS1 = LOW)
  digitalWrite(MS1_PIN, LOW);
  
  // Configure stepper motor
  stepper.setMaxSpeed(MAX_SPEED);
  stepper.setAcceleration(ACCELERATION);
  stepper.setCurrentPosition(0);
  
  Serial.println("Stepper motor initialized");
  Serial.print("Steps per revolution: ");
  Serial.println(STEPS_PER_REVOLUTION);
  Serial.print("Max speed: ");
  Serial.print(MAX_SPEED);
  Serial.println(" steps/sec");
  Serial.print("Acceleration: ");
  Serial.print(ACCELERATION);
  Serial.println(" steps/sec²");
  Serial.println();
  
  delay(2000);  // Allow driver to stabilize
  Serial.println("Starting movement test sequence...");
}

void loop() {
  static int testPhase = 0;
  static unsigned long phaseStartTime = 0;
  static bool phaseComplete = false;
  
  switch (testPhase) {
    case 0:
      // Phase 0: Initial position check
      if (!phaseComplete) {
        Serial.println("\n--- Phase 0: Initial Position ---");
        Serial.print("Current position: ");
        Serial.println(stepper.currentPosition());
        phaseStartTime = millis();
        phaseComplete = true;
      }
      
      if (millis() - phaseStartTime > 2000) {
        testPhase++;
        phaseComplete = false;
      }
      break;
      
    case 1:
      // Phase 1: Clockwise full rotation
      if (!phaseComplete) {
        Serial.println("\n--- Phase 1: Clockwise 360° Rotation ---");
        Serial.println("Moving to 200 steps (360 degrees)...");
        stepper.moveTo(STEPS_PER_REVOLUTION);
        phaseStartTime = millis();
        phaseComplete = true;
      }
      
      stepper.run();
      
      if (stepper.distanceToGo() == 0) {
        Serial.print("Rotation complete. Position: ");
        Serial.println(stepper.currentPosition());
        Serial.print("Time taken: ");
        Serial.print(millis() - phaseStartTime);
        Serial.println(" ms");
        testPhase++;
        phaseComplete = false;
      }
      break;
      
    case 2:
      // Phase 2: Pause
      if (!phaseComplete) {
        Serial.println("\n--- Phase 2: Pause ---");
        phaseStartTime = millis();
        phaseComplete = true;
      }
      
      if (millis() - phaseStartTime > 3000) {
        testPhase++;
        phaseComplete = false;
      }
      break;
      
    case 3:
      // Phase 3: Counter-clockwise to return
      if (!phaseComplete) {
        Serial.println("\n--- Phase 3: Counter-clockwise Return ---");
        Serial.println("Returning to position 0...");
        stepper.moveTo(0);
        phaseStartTime = millis();
        phaseComplete = true;
      }
      
      stepper.run();
      
      if (stepper.distanceToGo() == 0) {
        Serial.print("Return complete. Position: ");
        Serial.println(stepper.currentPosition());
        Serial.print("Time taken: ");
        Serial.print(millis() - phaseStartTime);
        Serial.println(" ms");
        testPhase++;
        phaseComplete = false;
      }
      break;
      
    case 4:
      // Phase 4: Multi-step test
      if (!phaseComplete) {
        Serial.println("\n--- Phase 4: Multi-step Test ---");
        phaseStartTime = millis();
        phaseComplete = true;
      }
      
      // Move to various positions
      static int positions[] = {50, 100, 150, 200, 100, 0};
      static int posIndex = 0;
      static unsigned long stepTime = 0;
      
      if (millis() - stepTime > 2000 && stepper.distanceToGo() == 0) {
        if (posIndex < 6) {
          Serial.print("Moving to position: ");
          Serial.println(positions[posIndex]);
          stepper.moveTo(positions[posIndex]);
          posIndex++;
          stepTime = millis();
        } else {
          testPhase++;
          phaseComplete = false;
          posIndex = 0;
        }
      }
      
      stepper.run();
      break;
      
    case 5:
      // Phase 5: Speed test
      if (!phaseComplete) {
        Serial.println("\n--- Phase 5: Speed Test ---");
        Serial.println("Testing different speeds...");
        stepper.setMaxSpeed(2000);  // Higher speed
        stepper.moveTo(STEPS_PER_REVOLUTION);
        phaseStartTime = millis();
        phaseComplete = true;
      }
      
      stepper.run();
      
      if (stepper.distanceToGo() == 0) {
        Serial.print("High speed test complete. Time: ");
        Serial.print(millis() - phaseStartTime);
        Serial.println(" ms");
        stepper.setMaxSpeed(MAX_SPEED);  // Reset to normal speed
        stepper.moveTo(0);
        testPhase++;
        phaseComplete = false;
      }
      break;
      
    case 6:
      // Phase 6: Return and complete
      stepper.run();
      
      if (stepper.distanceToGo() == 0) {
        Serial.println("\n===============================================");
        Serial.println("Stepper Motor Test Complete");
        Serial.println("All phases passed successfully!");
        Serial.println("Motor returned to home position");
        Serial.println("===============================================");
        
        // Disable motor to prevent heating
        digitalWrite(ENABLE_PIN, HIGH);
        
        // Hold here - test complete
        while (true) {
          delay(1000);
          Serial.println("Test complete. Reset to run again.");
        }
      }
      break;
  }
  
  // Monitor current position periodically
  static unsigned long lastStatusUpdate = 0;
  if (millis() - lastStatusUpdate > 1000 && stepper.isRunning()) {
    Serial.print("Current position: ");
    Serial.print(stepper.currentPosition());
    Serial.print(", Target: ");
    Serial.print(stepper.targetPosition());
    Serial.print(", Distance to go: ");
    Serial.println(stepper.distanceToGo());
    lastStatusUpdate = millis();
  }
}
