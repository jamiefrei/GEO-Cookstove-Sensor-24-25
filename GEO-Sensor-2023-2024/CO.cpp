// GEO Cookstove Sensor Code
// C.O.N.D.O.R.S. 2020-2021
// Arduino code for the cookstove sensor.
// CO.cpp
// This file contains function definitions for the CO sensor.
// We received most of this code for the CO sensor by an SPEC sensor representative


#include "CO.h"

double CO::measure() { //Returns concentration of CO in ppm
  Vavg = 0;
  for (int i = 0; i < 128; i++) {
    Vavg += analogRead(vgas);
    }
  result = Vavg / 128.0;
  return result;
}
