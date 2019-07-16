#ifndef FFATSENSOR_H
#define FFATSENSOR_H

#ifndef ESP32
#warning FFatSensor will only work on ESP32 MCUs
#endif

#include <Task.h>

#define SAVED_LOGFILES          30
#define SENSOR_PIN              5
#define MAX_NUMBER_OF_SENSORS   3

#define VALID_ID_LENGTH         14

typedef char sensorId_t[VALID_ID_LENGTH + 1];
typedef char sensorName_t[15];
typedef byte sensorAddr_t[8];

class FFatSensor: public Task {

  public:

    struct sensorState_t {                  /* struct to keep track of Dallas DS18B20 sensors */
      sensorAddr_t     addr{0};
      float            tempCelsius = NAN;
      bool             error = true;
    };

    FFatSensor();
    virtual ~FFatSensor();

    bool                  startSensors(uint8_t pin);
    void                  rescan();
    uint8_t               count();
    float                 temp( const uint8_t num );
    bool                  error( const uint8_t num );
    const char *          getName( const uint8_t num, sensorName_t &name );
    const char *          getName( const sensorName_t &id, sensorName_t &name );
    const char *          getId( const uint8_t num, sensorId_t &id );
    bool                  setName( const sensorId_t &id, const char * name );
    bool                  isTempLogging();
    bool                  isErrorLogging();
    bool                  startTempLogging( const uint32_t seconds=180 );
    bool                  stopTempLogging();
    bool                  startErrorLogging();
    bool                  stopErrorLogging();

  private:
    void                  run( void * data );
    uint8_t               _count = 0;
    uint8_t               _scanSensors();
    sensorState_t         _state[MAX_NUMBER_OF_SENSORS];
    bool                  _errorlogging = false;
};

#endif //FFATSENSOR_H
