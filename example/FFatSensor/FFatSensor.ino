#include <OneWire.h>
#include <Task.h>
#include <FFat.h>
#include <FFatSensor.h>                                        // 0. Include the libraries.

#define FORMAT_FS true /*WILL FORMAT FFAT! set to false to just mount.*/

/* Should be compiled with a FFat partition in Tools>Partition scheme */

FFatSensor sensor;                                             // 1. Make an instance.

void setup() {
  Serial.begin( 115200 );
  Serial.println();
  Serial.println();

  Serial.println("FFatSensor example sketch. Starting sensors.");

  // Logging to FFat

  if ( !FFat.begin( FORMAT_FS ) ) Serial.println( "Could not mount FFat.");

  sensor.startSensors( 10, 5 );                                      // 3. Start max 10 DS18B20 sensors on GPIO 5.

  Serial.print("Waiting for sensors");  // 4. You can just wait until a particular sensor gives a valid reading.
  while ( !sensor.sensorCount() ) {
    Serial.print(".");
    delay( 100 );
  }
  Serial.println();

  Serial.printf( "%i sensors found.\n", sensor.sensorCount());   // 5. Or check how many sensors are found.

  //Serial.print( "First sensor value: ");                         // 6. Get a sensor reading.
  //Serial.println( sensor.sensorTemp( 0 ) );


  // how to get a name and ID
  sensorId_t   id;                   // Make a variable to hold the id.
  sensorName_t name;                 // Make a variable to hold the name.

  for ( uint8_t num = 0; num < sensor.sensorCount(); num++ ) {
    // Get the id and print it in one go.
    Serial.printf( "sensor %i id: %s\n", num, sensor.getSensorId( num, id ) );

    // Get the name and print it in one go.
    Serial.printf( "sensor %i name: %s\n", num, sensor.getSensorName( num, name ) );
  }

  Serial.println();

  //setting a sensor name
  if ( !sensor.setSensorName( id, "thisnameistoolong" ) )        // Rename a sensor. The new name will be stored in NVS and be available after a reboot.
    sensor.setSensorName( id, "FFatSensor" );                    // Will return true or false depending on the result of the operation.

  Serial.printf( "name from ID: %s is ", id);
  Serial.println( sensor.getSensorName( id, name ) );

  Serial.print( "Name from first sensor is " );
  Serial.println( sensor.getSensorName( 0, name ) );

  if ( FFat.totalBytes() ) {
    if ( !sensor.isErrorLogging() ) sensor.startErrorLogging();                 // Log sensor errors to FFat.

    if ( sensor.isTempLogging() ) Serial.println( "Logging already on." );      // You can check the current log state.

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
