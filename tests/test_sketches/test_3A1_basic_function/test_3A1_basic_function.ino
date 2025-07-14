/*
 * ESP8266 Satellite Tracker - Phase 3A.1: Basic Function Test
 * Tests basic ESP8266 functionality and I/O operations
 * 
 * This test verifies:
 * - ESP8266 boots correctly
 * - Built-in LED functions
 * - Serial communication works
 * - GPIO pins can be controlled
 * 
 * Expected results:
 * - Built-in LED blinks every 1 second
 * - Serial output shows status messages
 * - GPIO test pins toggle correctly
 */

#define BUILTIN_LED 2  // GPIO2 on ESP8266
#define TEST_PIN_1 D1  // GPIO5
#define TEST_PIN_2 D2  // GPIO4

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println("===============================================");
  Serial.println("ESP8266 Satellite Tracker - Basic Function Test");
  Serial.println("Test ID: 3A.1");
  Serial.println("===============================================");
  
  // Initialize GPIO pins
  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(TEST_PIN_1, OUTPUT);
  pinMode(TEST_PIN_2, OUTPUT);
  
  // Test initial states
  digitalWrite(BUILTIN_LED, HIGH);  // Turn off (inverted logic)
  digitalWrite(TEST_PIN_1, LOW);
  digitalWrite(TEST_PIN_2, LOW);
  
  Serial.println("GPIO pins initialized");
  Serial.print("ESP8266 Chip ID: ");
  Serial.println(ESP.getChipId(), HEX);
  Serial.print("Flash Chip Size: ");
  Serial.print(ESP.getFlashChipSize() / 1024 / 1024);
  Serial.println(" MB");
  Serial.print("Free Heap: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");
  Serial.println();
  Serial.println("Starting LED blink test...");
}

void loop() {
  static unsigned long lastBlink = 0;
  static unsigned long lastTest = 0;
  static bool ledState = false;
  static bool testState = false;
  static int testCount = 0;
  
  unsigned long currentTime = millis();
  
  // Blink built-in LED every 1 second
  if (currentTime - lastBlink >= 1000) {
    ledState = !ledState;
    digitalWrite(BUILTIN_LED, !ledState);  // Inverted logic
    
    Serial.print("LED State: ");
    Serial.print(ledState ? "ON" : "OFF");
    Serial.print(" | Uptime: ");
    Serial.print(currentTime / 1000);
    Serial.print("s | Free Heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
    
    lastBlink = currentTime;
  }
  
  // Toggle test pins every 500ms
  if (currentTime - lastTest >= 500) {
    testState = !testState;
    digitalWrite(TEST_PIN_1, testState);
    digitalWrite(TEST_PIN_2, !testState);  // Opposite phase
    
    testCount++;
    if (testCount % 10 == 0) {  // Every 5 seconds
      Serial.print("GPIO Test - Pin D1: ");
      Serial.print(testState ? "HIGH" : "LOW");
      Serial.print(", Pin D2: ");
      Serial.print(!testState ? "HIGH" : "LOW");
      Serial.print(" (Count: ");
      Serial.print(testCount);
      Serial.println(")");
    }
    
    lastTest = currentTime;
  }
  
  // Memory monitoring - report if heap drops below 10KB
  if (ESP.getFreeHeap() < 10000) {
    Serial.println("WARNING: Low memory detected!");
  }
  
  // Watchdog feed
  yield();
}
