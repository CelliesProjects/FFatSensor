[![Codacy Badge](https://api.codacy.com/project/badge/Grade/c452fe07342548edad149c309ca34e9a)](https://www.codacy.com/manual/CelliesProjects/FFatSensor?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=CelliesProjects/FFatSensor&amp;utm_campaign=Badge_Grade)

## FFatSensor
ESP32 Arduino IDE library for managing OneWire DS18B20 temperature sensors.
<br>Get easy temperature readings between -55° and +85° and log these to FFat.
<br>Just set the number of desired sensors and a GPIO pin number and you are good to go.

#### An simple interface for OneWire DS18B20 sensors.
- `logger.startSensors( 3, 5 )`
<br>Starts max 3 sensors on GPIO 5 that are scanned and updated every 750ms.
- `logger.sensorCount()`
<br>Get the number of sensors actually connected.
- `logger.getSensorTemp( 0 )`
<br>Get a temperature reading from the first sensor.

#### With easy temperature logging to FFat.
- `sensor.startTempLogging( 180 )`
<br>Starts temperature logging to FFat every 180 seconds.
<br>The interval and log state are saved in NVS. 
<br>If after a reboot `startSensors()` is called logging will resume with the specified interval. 
- `sensor.stopTempLogging()`
<br>Stops temperature logging to FFat. 
- `sensor.isTempLogging()`
<br>Gives the current temperature logging state.

Temperature logging writes to a csv file formatted as `1970-01-01.log` if no system time is set before.
<br>Logging is based on UTC. A typical log entry looks like:
````bash
1564356928,26.69,23.00,18.44
````
- Where `1564356928` is `Mon Jul 29 2019 01:35:28 GMT+0200 (Central European Summer Time)`
<br>To process these time values with JavaScript just take the value and multiply it with 1000 to get a valid JS time.
<br>In a console this would be: `new Date( 1564356928 * 1000 )`
- `26.69,23.00,18.44` are the logged sensor temperatures at that time.
#### Wait! There's more!
- `sensor.startErrorLogging()`
<br>Starts sensor error logging to FFat.
- `sensor.stopErrorLogging()`
<br>Stops sensor error logging to FFat.
- `sensor.isErrorLogging()`
<br>Gives the current error logging state.

Error logging writes to `sensor_error.txt`.
<br>Error logging is based on calender time.

FFatSensor runs fine without FFat partition mounted, but then you have no logging ofcourse.

#### Depends on:
- ESP32 FFat library. (only needed to log to file)
- ESP32 [OneWire](https://github.com/stickbreaker/OneWire) library by Chuck Todd.
<br>Use this library instead of the standard Arduino version which will not work for ESP32 MCUs.
- ESP32 [Task](https://github.com/CelliesProjects/Task) by Neil Kolban.

#### How to use:
Download and install `FFatSensor`, `OneWire` and `Task` in the esp32 libraries folder.

#### Example code:
````c++
#include <OneWire.h>
#include <Task.h>
#include <FFat.h>
#include <FFatSensor.h>  // 1. Include the libraries.

#define FORMAT_FS false /*WILL FORMAT FFAT! set to true to format FFat.*/

/* Should be compiled with a FFat partition in Tools>Partition scheme */

FFatSensor sensor;  // 2. Make an object.

void setup() {
  Serial.begin( 115200 );
  Serial.println();
  Serial.println();

  Serial.println("FFatSensor example sketch. Starting sensors.");

  if ( FORMAT_FS ) FFat.format();
  if ( !FFat.begin() ) Serial.println( "Could not mount FFat.");

  sensor.startSensors( 10, 5 );  // 3. Start max 10 DS18B20 sensors on GPIO 5.

  Serial.print("Waiting for sensors");  // 4. Wait for sensors to become available.
  while ( !sensor.sensorCount() ) {
    Serial.print(".");
    delay( 100 );
  }
  Serial.println();

  Serial.printf( "%i sensors found.\n", sensor.sensorCount());  // 5. Check how many sensors are found.

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
