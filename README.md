## FFatSensor
ESP32 library for managing and logging of DS18B20 temperature sensors.

Scan, read and name sensors. Can log sensor values and errors to FATFS. Runs fine without FFat, but then you have no logging ofcourse.

#### Depends on:
- ESP32 [OneWire](https://github.com/stickbreaker/OneWire) library by stickbreaker.
- ESP32 [Task](https://github.com/CelliesProjects/Task) by Neil Kolban.
- ESP32 FFat library. Comes with ESP32 Arduino core. (only needed to log to file)

Install `OneWire` and `Task` in the esp32 libraries folder.

#### Example:
````c++
#include <OneWire.h>               /* https://github.com/stickbreaker/OneWire */
#include <FFat.h>                  // Include FFat to use the logging functions.
#include <FFatSensor.h>

/* Should be compiled with a FFat partition in Tools>Partition scheme */

FFatSensor sensor;                 // 1. Make an instance

void setup() {
  Serial.begin(115200);

  if ( !FFat.begin() )             // 2. Begin fatfs BEFORE starting the sensors.
    Serial.println( "Could not mount FFat.");

  sensor.startSensors();           // 3. Start the sensor task.


  Serial.println("Waiting for first sensor");   // 4. You can just wait until a particular sensor gives a valid reading.
  while ( sensor.error( 0 ) ) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();


  Serial.printf( "%i sensors found.\n", sensor.count() );  // 5. Or check how many sensors are found.


  Serial.print( "First sensor value: ");   // 6. Get a sensor reading.
  Serial.println( sensor.temp( 0 ) );

                                           // getting name and ID

  sensorId_t   id;                         // Make a id variable.
  sensorName_t name;                       // Make a name variable.

  Serial.printf( "First sensor id: %s\n", sensor.getId( 0, id ) );        // Get the id and print it in one go.
  Serial.printf( "First sensor name: %s\n", sensor.getName( 0, name ) );  // Get the name and print it in one go.
  Serial.println();


  if ( !sensor.setName( id, "thisnameistoolong" ) )   // Rename a sensor. The new name will be stored in NVS.
    sensor.setName( id, "Name is just fine!" );       // Will return true or false depending on the result of the operation.

  Serial.printf( "name from ID: %s is ", id);
  Serial.println( sensor.getName( id, name ) );

  Serial.print( "Name from first sensor is " );
  Serial.println( sensor.getName( 0, name ) );

  if ( FFat.totalBytes() ) {
    sensor.startErrorLogging();

    if ( sensor.isLogging() )              // You can check the current log state
      Serial.println("Logging already on.");

    else if ( sensor.startTemperatureLogging( 120 ) )   // If the FFat partition is mounted sensor values will be logged to this partition
      Serial.println( "Logging sensor values every 120 seconds." );
  }
  else
    Serial.println( "FFat not mounted so no logging enabled." );

  Serial.println( "Done with setup, starting loop." );
}

void loop() {
  Serial.printf( "%i sensors found.\n", sensor.count() );

  sensorId_t id;
  sensorName_t name;

  for ( uint8_t currentSensor = 0; currentSensor < sensor.count(); currentSensor++ ) {
    if ( !sensor.error( currentSensor ) )
      Serial.printf( "Sensor %i: %.4f '%15s' id: '%s'\n",
                     currentSensor,
                     sensor.temp( currentSensor ),
                     sensor.getName( currentSensor, name ),
                     sensor.getId( currentSensor, id ) );
    else
      Serial.printf( "Sensor %i: reports an error\n", currentSensor );
  }
  Serial.println("Waiting 5 seconds...");
  delay(5000);

}
````
