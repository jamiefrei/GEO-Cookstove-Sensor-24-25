// GEO Cookstove Sensor Code
// C.O.N.D.O.R.S. 2020-2021
// Arduino code for the cookstove sensor.
// CO2.h
// This file contains pin numbers for the CO2 sensor, other values for the CO2 sensor,
// and function declarations for the CO2 sensor.


#ifndef CO2_H
#define CO2_H

#include <Arduino.h>
#include <String.h>
#include <Adafruit_SCD30.h>

class CO2 {
  public:
    Adafruit_SCD30  scd30;
              
         // Adam added this in so that it doesn't do a self calibration
         // We want to do our own forced calibration
         // parameter is 'uint16_t reference'

    CO2() { // Constructor
//        if (!scd30.selfCalibrationEnabled(false)){
//          Serial.println("Failed to disable self calibration");
//          while(1) { delay(10); }
//        }
//        if (scd30.selfCalibrationEnabled()) {
//          Serial.print("Self calibration enabled");
//        } else {
//          Serial.print("Self calibration disabled");
//        }
//
//        scd30.forceRecalibrationWithReference(550);
    }

    
    double measure() {
        double ppm;
        if (scd30.dataReady()) {
          if (!scd30.read()) return 0.00;
          ppm = scd30.CO2;
        }
      return ppm;
    }
};

#endif //CO2_H
