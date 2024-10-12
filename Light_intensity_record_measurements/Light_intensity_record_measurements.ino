#include <Arduino_APDS9960.h>

#define AMOUNT_OF_MEASUREMENTS 10

void setup() {
  Serial.begin(9600);
  while (!Serial);

  if (!APDS.begin()) {
    Serial.println("Error initializing APDS9960 sensor!");
    while (1);
  }

  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);
  
  // CSV header
  Serial.println("Measurement, Timestamp (ms), Ambient Light Intensity");
}

void setLEDColor(int red, int green, int blue) {
  digitalWrite(LEDR, red);
  digitalWrite(LEDG, green);
  digitalWrite(LEDB, blue);
}

void loop() {
  for (int i = 0; i < AMOUNT_OF_MEASUREMENTS; i++) {
    // LED 5 Sekunden lang rot leuchten lassen
    // RED
    setLEDColor(LOW, HIGH, HIGH);
    delay(5000);

    // Switch to GREEN LED and start measurements
    setLEDColor(HIGH, LOW, HIGH);

     // Measure every 250 ms for 10 seconds
    for (int j = 0; j < 40;) {
      int r, g, b, a;
      if (APDS.colorAvailable()) {
        APDS.readColor(r, g, b, a);
        Serial.print(i + 1);  // measurement number
        Serial.print(", ");
        Serial.print(j * 250);  // Timestamp
        Serial.print(", ");
        Serial.println(a);  // brightness
        j++;
        delay(200); // Just 200 because the sensor needs also time so that we will have roughly 250ms +-25ms
      }
    }

    // Switch back to RED LED and wait 2 seconds
    setLEDColor(LOW, HIGH, HIGH);
    delay(2000);
  }

  Serial.println("All measurements completed.");
  while (1);
}
