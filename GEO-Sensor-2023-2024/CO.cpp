// GEO Cookstove Sensor Code
// C.O.N.D.O.R.S. 2020-2021
// Arduino code for the cookstove sensor.
// CO.cpp
// This file contains function definitions for the CO sensor.
// We received most of this code for the CO sensor by an SPEC sensor representative


#include "CO.h"

// Returns analogRead() value from the CO sensor
// 128 is just an arbitrary number that we picked. It could be 50, 100, 200 or any number in between. We just wanted 
// to collect enough readings to average them, in case one random reading was a malfunction it won't affect everything.
double CO::measure() {
  Vavg = 0;
  for (int i = 0; i < 128; i++) {
    Vavg += analogRead(vgas);
    }
  result = Vavg / 128.0;
  return result; //  this return value is considered one CO measurement that will be saved as the coRaw value
}
