#include <Arduino.h>
#include "BLEUlmernest.h"

void setup ()
{
  Serial.begin(115200);
  BLEUlmernest::init("nest_esp32");
}

void loop ()
{
  // do stuff!
}