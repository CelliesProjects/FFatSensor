## FFatSensor
ESP32 Arduino IDE library for managing OneWire DS18B20 temperature sensors.

#### An easy interface for OneWire DS18B20 sensors.

- `logger.startSensors( 3, 5 )` and 3 sensors on GPIO 5 are scanned and running.
- `logger.sensorCount()` gives the number of sensors connected.
- `logger.getSensorTemp( 0 )` gives a temperature reading from the first sensor.

Runs fine without FFat, but then you have no logging ofcourse.

#### Depends on:
- ESP32 FFat library. (only needed to log to file)
- ESP32 [OneWire](https://github.com/stickbreaker/OneWire) library by stickbreaker.
<br>Use this library instead of the standard Arduino version which will not work for ESP32 MCUs.
- ESP32 [Task](https://github.com/CelliesProjects/Task) by Neil Kolban.

#### How to use:
Download and install `FFatSensor`, `OneWire` and `Task` in the esp32 libraries folder.

#### Example code:
````c++
#include <OneWire.h>
#include <Task.h>
#include <FFat.h>
#include <FFatSensor.h>  // 0. Include the libraries.

#define FORMAT_FS false /*WILL FORMAT FFAT! set to true to format FFat.*/

/* Should be compiled with a FFat partition in Tools>Partition scheme */

FFatSensor sensor;  // 1. Make an instance.

void setup() {
  Serial.begin( 115200 );
  Serial.println();
  Serial.println();

  Serial.println("FFatSensor example sketch. Starting sensors.");

  // Logging to FFat

  if ( FORMAT_FS ) FFat.format();
  if ( !FFat.begin() ) Serial.println( "Could not mount FFat.");

  sensor.startSensors( 10, 5 );  // 3. Start max 10 DS18B20 sensors on GPIO 5.

  Serial.print("Waiting for sensors");  // 4. You can just wait until a particular sensor gives a valid reading.
  while ( !sensor.sensorCount() ) {
    Serial.print(".");
    delay( 100 );
  }
  Serial.println();

  Serial.printf( "%i sensors found.\n", sensor.sensorCount());  // 5. Or check how many sensors are found.

  Serial.print( "First sensor value: ");  // 6. Get a sensor reading.
  Serial.println( sensor.sensorTemp( 0 ) );

  // how to get a name and ID
  sensorId_t   id;    // Make a id variable.
  sensorName_t name;  // Make a name variable.

  for ( uint8_t num = 0; num < sensor.sensorCount(); num++ ) {
    // Get the id and print it in one go.
    Serial.printf( "sensor %i id: %s\n", num, sensor.getSensorId( num, id ) );

    // Get the name and print it in one go.
    Serial.printf( "sensor %i name: %s\n", num, sensor.getSensorName( num, name ) );
  }

  Serial.println();

  //setting a sensor name
  if ( !sensor.setSensorName( id, "thisnameistoolong" ) )  // Rename a sensor. The new name will be stored in NVS and be available after a reboot.
    sensor.setSensorName( id, "FFatSensor" );  // Will return true or false depending on the result of the operation.

  Serial.printf( "name from ID: %s is ", id);
  Serial.println( sensor.getSensorName( id, name ) );

  Serial.print( "Name from first sensor is " );
  Serial.println( sensor.getSensorName( 0, name ) );

  if ( FFat.totalBytes() ) {
    if ( !sensor.isErrorLogging() ) sensor.startErrorLogging();  // Log sensor errors to FFat.

    if ( sensor.isTempLogging() ) Serial.println( "Logging already on." );  // You can check the current log state.

    if ( !sensor.startTempLogging() ) Serial.println( "Logging already on. (again)" );  // If FFat is mounted sensor values will be logged every 180 seconds.
  }
  else
    Serial.println( "FFat not mounted so no logging enabled." );

  Serial.println( "Done with setup, waiting 5 seconds before starting loop..." );
  delay( 5000 );
}

void loop() {
  // How to use time stamps
  timeStampBuffer_t tsb;
  Serial.printf( "Human timestamp:  %s\n", sensor.timeStamp( HUMAN_TIME, tsb ) );
  Serial.printf( "Unix timestamp:   %s\n", sensor.timeStamp( UNIX_TIME, tsb ) );
  Serial.printf( "Millis timestamp: %s\n", sensor.timeStamp( MILLIS_TIME, tsb ) );

  Serial.printf( "%i sensors found.\n", sensor.sensorCount() );

  sensorId_t id;
  sensorName_t name;
  for ( uint8_t num = 0; num < sensor.sensorCount(); num++ ) {
    if ( !sensor.sensorError( num ) )
      Serial.printf( "Sensor %i: %.4f '%15s' id: '%s'\n",
                     num,
                     sensor.sensorTemp( num ),
                     sensor.getSensorName( num, name ),
                     sensor.getSensorId( num, id ) );
    else
      Serial.printf( "Sensor %i: reports an error\n", num );
  }
  Serial.println("Waiting 500ms...");
  Serial.println();
  delay(500);
}
````
