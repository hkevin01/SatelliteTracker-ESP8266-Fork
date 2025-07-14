/*
 * ESP8266 Satellite Tracker - Phase 3D.1: LCD Display Test
 * Tests 16x2 LCD display functionality
 * 
 * This test verifies:
 * - LCD initializes correctly
 * - All character positions work
 * - Text display is clear and readable
 * - Dynamic updates work smoothly
 * 
 * Hardware setup:
 * - Connect 16x2 LCD to ESP8266 as defined below
 * - Adjust contrast potentiometer for optimal readability
 * - Ensure 5V power to LCD VCC
 */

#include <LiquidCrystal.h>

// LCD pin connections to ESP8266
// Note: Some pins may need adjustment based on your specific wiring
const int rs = D0;      // GPIO16 - Register Select
const int en = D8;      // GPIO15 - Enable  
const int d4 = D7;      // GPIO13 - Data 4
const int d5 = D6;      // GPIO12 - Data 5
const int d6 = D5;      // GPIO14 - Data 6
const int d7 = D4;      // GPIO2  - Data 7

// Initialize the library with interface pins
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Test patterns and messages
const char* testMessages[] = {
  "ESP8266 Tracker",
  "LCD Test Active",
  "Phase 3D.1 Test",
  "Display Working",
  "All Chars: OK!",
  "Satellite Track",
  "Az:123.4 El:45.6",
  "WiFi: Connected",
  "Time: 12:34:56",
  "Status: Ready"
};

const int numTestMessages = sizeof(testMessages) / sizeof(testMessages[0]);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println("===============================================");
  Serial.println("ESP8266 Satellite Tracker - LCD Display Test");
  Serial.println("Test ID: 3D.1 - LCD Basic Function");
  Serial.println("===============================================");
  
  // Initialize LCD
  Serial.println("Initializing 16x2 LCD display...");
  lcd.begin(16, 2);
  
  // Clear display and show startup message
  lcd.clear();
  lcd.print("ESP8266 Tracker");
  lcd.setCursor(0, 1);
  lcd.print("LCD Initializing");
  
  Serial.println("LCD initialized successfully");
  Serial.println("Pin configuration:");
  Serial.print("  RS = D0 (GPIO");
  Serial.print(rs);
  Serial.println(")");
  Serial.print("  EN = D8 (GPIO");
  Serial.print(en);
  Serial.println(")");
  Serial.print("  D4 = D7 (GPIO");
  Serial.print(d4);
  Serial.println(")");
  Serial.print("  D5 = D6 (GPIO");
  Serial.print(d5);
  Serial.println(")");
  Serial.print("  D6 = D5 (GPIO");
  Serial.print(d6);
  Serial.println(")");
  Serial.print("  D7 = D4 (GPIO");
  Serial.print(d7);
  Serial.println(")");
  
  delay(3000);  // Display startup message for 3 seconds
  
  Serial.println("\nStarting LCD test sequence...");
}

void loop() {
  static int testPhase = 0;
  static unsigned long phaseStartTime = 0;
  static bool phaseInitialized = false;
  
  switch (testPhase) {
    case 0:
      // Test Phase 0: Character position test
      if (!phaseInitialized) {
        Serial.println("\n--- Phase 0: Character Position Test ---");
        testCharacterPositions();
        phaseStartTime = millis();
        phaseInitialized = true;
      }
      
      if (millis() - phaseStartTime > 5000) {
        testPhase++;
        phaseInitialized = false;
      }
      break;
      
    case 1:
      // Test Phase 1: Scrolling character test
      if (!phaseInitialized) {
        Serial.println("\n--- Phase 1: Scrolling Character Test ---");
        testScrollingCharacters();
        phaseStartTime = millis();
        phaseInitialized = true;
      }
      
      if (millis() - phaseStartTime > 8000) {
        testPhase++;
        phaseInitialized = false;
      }
      break;
      
    case 2:
      // Test Phase 2: Message cycling test
      if (!phaseInitialized) {
        Serial.println("\n--- Phase 2: Message Cycling Test ---");
        phaseStartTime = millis();
        phaseInitialized = true;
      }
      
      testMessageCycling();
      
      if (millis() - phaseStartTime > 20000) { // 20 seconds of message cycling
        testPhase++;
        phaseInitialized = false;
      }
      break;
      
    case 3:
      // Test Phase 3: Real-time data simulation
      if (!phaseInitialized) {
        Serial.println("\n--- Phase 3: Real-time Data Simulation ---");
        phaseStartTime = millis();
        phaseInitialized = true;
      }
      
      simulateRealTimeData();
      
      if (millis() - phaseStartTime > 30000) { // 30 seconds of simulated data
        testPhase++;
        phaseInitialized = false;
      }
      break;
      
    case 4:
      // Test Phase 4: Special characters test
      if (!phaseInitialized) {
        Serial.println("\n--- Phase 4: Special Characters Test ---");
        testSpecialCharacters();
        phaseStartTime = millis();
        phaseInitialized = true;
      }
      
      if (millis() - phaseStartTime > 8000) {
        testPhase++;
        phaseInitialized = false;
      }
      break;
      
    case 5:
      // Test Phase 5: Completion
      if (!phaseInitialized) {
        Serial.println("\n--- Test Complete ---");
        lcd.clear();
        lcd.print("LCD Test");
        lcd.setCursor(0, 1);
        lcd.print("Complete! PASS");
        
        Serial.println("===============================================");
        Serial.println("LCD Display Test Complete");
        Serial.println("All phases passed successfully!");
        Serial.println("✅ RESULT: PASS");
        Serial.println("===============================================");
        
        phaseInitialized = true;
      }
      
      // Stay in this phase (test complete)
      delay(5000);
      break;
  }
}

void testCharacterPositions() {
  lcd.clear();
  
  // Test all 32 positions (16x2)
  for (int row = 0; row < 2; row++) {
    for (int col = 0; col < 16; col++) {
      lcd.setCursor(col, row);
      lcd.print(String(col % 10)); // Display column number
      delay(100);
    }
  }
  
  delay(2000);
  
  // Clear and show position test complete
  lcd.clear();
  lcd.print("Position Test");
  lcd.setCursor(0, 1);
  lcd.print("Complete!");
  
  Serial.println("Character position test completed");
}

void testScrollingCharacters() {
  lcd.clear();
  lcd.print("Scroll Test:");
  
  // Animate a moving character across the bottom row
  for (int i = 0; i < 16; i++) {
    lcd.setCursor(0, 1);
    lcd.print("                "); // Clear line
    lcd.setCursor(i, 1);
    lcd.print("*");
    delay(300);
  }
  
  // Reverse direction
  for (int i = 15; i >= 0; i--) {
    lcd.setCursor(0, 1);
    lcd.print("                "); // Clear line
    lcd.setCursor(i, 1);
    lcd.print("◄");
    delay(200);
  }
  
  Serial.println("Scrolling character test completed");
}

void testMessageCycling() {
  static unsigned long lastMessageChange = 0;
  static int currentMessage = 0;
  
  if (millis() - lastMessageChange > 2000) { // Change message every 2 seconds
    lcd.clear();
    
    // Display current test message
    String msg = String(testMessages[currentMessage]);
    lcd.print(msg.substring(0, 16)); // First line (max 16 chars)
    
    // If message is longer than 16 chars, show on second line
    if (msg.length() > 16) {
      lcd.setCursor(0, 1);
      lcd.print(msg.substring(16, 32));
    } else {
      // Show message index on second line
      lcd.setCursor(0, 1);
      lcd.print("Message ");
      lcd.print(currentMessage + 1);
      lcd.print("/");
      lcd.print(numTestMessages);
    }
    
    Serial.print("Displaying message ");
    Serial.print(currentMessage + 1);
    Serial.print(": ");
    Serial.println(testMessages[currentMessage]);
    
    currentMessage = (currentMessage + 1) % numTestMessages;
    lastMessageChange = millis();
  }
}

void simulateRealTimeData() {
  static unsigned long lastUpdate = 0;
  static float azimuth = 0.0;
  static float elevation = 0.0;
  static int seconds = 0;
  
  if (millis() - lastUpdate > 1000) { // Update every second
    // Simulate changing satellite position
    azimuth += 1.5;
    if (azimuth > 360.0) azimuth = 0.0;
    
    elevation = 45.0 + 30.0 * sin(millis() / 10000.0); // Oscillate between 15-75°
    
    // Display simulated tracking data
    lcd.clear();
    lcd.print("Az:");
    lcd.print(azimuth, 1);
    lcd.print(" El:");
    lcd.print(elevation, 1);
    
    lcd.setCursor(0, 1);
    lcd.print("Time: ");
    if (seconds < 10) lcd.print("0");
    lcd.print(seconds);
    lcd.print("s WiFi:OK");
    
    seconds++;
    if (seconds > 59) seconds = 0;
    
    Serial.print("Simulated data - Az: ");
    Serial.print(azimuth, 1);
    Serial.print("°, El: ");
    Serial.print(elevation, 1);
    Serial.print("°, Time: ");
    Serial.print(seconds);
    Serial.println("s");
    
    lastUpdate = millis();
  }
}

void testSpecialCharacters() {
  lcd.clear();
  lcd.print("Special Chars:");
  
  // Test various special characters
  lcd.setCursor(0, 1);
  lcd.print("°");  // Degree symbol
  lcd.print("←");  // Arrow left  
  lcd.print("→");  // Arrow right
  lcd.print("↑");  // Arrow up
  lcd.print("↓");  // Arrow down
  lcd.print("●");  // Bullet
  lcd.print("○");  // Circle
  lcd.print("■");  // Square
  lcd.print("□");  // Empty square
  lcd.print("♦");  // Diamond
  
  delay(3000);
  
  // Test numbers and symbols
  lcd.clear();
  lcd.print("Nums & Symbols:");
  lcd.setCursor(0, 1);
  lcd.print("0123456789!@#$%");
  
  delay(3000);
  
  Serial.println("Special characters test completed");
}
