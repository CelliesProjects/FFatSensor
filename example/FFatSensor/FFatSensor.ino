#include <Task.h>
#include <FFat.h>
#include <FFatSensor.h>                                                        // 0. Include the library
/* Should be compiled with a FFat partition in Tools>Partition scheme */

FFatSensor sensor;                                                             // 1. Make an instance

void setup() {
  Serial.begin( 115200 );

  sensor.startSensors( 5 );                                                    // 3. Start the sensors on GPIO 5.

  Serial.println("Waiting for first sensor");                                  // 4. You can just wait until a particular sensor gives a valid reading.
  while ( sensor.error( 0 ) ) {
    Serial.print(".");
    delay( 100 );
  }
  Serial.println();

  Serial.printf( "%i sensors found.\n", sensor.count());                       // 5. Or check how many sensors are found.

  Serial.print( "First sensor value: ");                                       // 6. Get a sensor reading.
  Serial.println( sensor.temp( 0 ) );


  // how to get a name and ID
  sensorId_t   id;                                                             // Make a id variable.
  sensorName_t name;                                                           // Make a name variable.

  Serial.printf( "First sensor id: %s\n", sensor.getId( 0, id ) );             // Get the id and print it in one go.
  Serial.printf( "First sensor name: %s\n", sensor.getName( 0, name ) );       // Get the name and print it in one go.
  Serial.println();

  //setting a sensor name
  if ( !sensor.setName( id, "thisnameistoolong" ) )                            // Rename a sensor. The new name will be stored in NVS and be available after a reboot.
    sensor.setName( id, "Name is fine!" );                                     // Will return true or false depending on the result of the operation.

  Serial.printf( "name from ID: %s is ", id);
  Serial.println( sensor.getName( id, name ) );

  Serial.print( "Name from first sensor is " );
  Serial.println( sensor.getName( 0, name ) );

  // Logging to FFat

  if ( !FFat.begin() ) Serial.println( "Could not mount FFat.");                // Start FFat BEFORE any kind of logging.

  if ( FFat.totalBytes() ) {
    if ( !sensor.isErrorLogging() ) sensor.startErrorLogging();                 // Log sensor errors to FFat.

    if ( sensor.isTempLogging() ) Serial.println( "Logging already on." );      // You can check the current log state

    if ( !sensor.startTempLogging( 120 ) )                                      // If FFat is mounted sensor values will be logged
      Serial.println( "Logging already on." );
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
  Serial.println("Waiting 500ms...");

  delay(500);

}
