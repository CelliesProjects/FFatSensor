#ifndef FFATSENSOR_H
#define FFATSENSOR_H

#ifndef ESP32
#warning FFatSensor will only work on ESP32 MCUs
#endif

#include <Task.h>

#define SAVED_LOGFILES          30
#define VALID_ID_LENGTH         14

#define TEMPLOG_ON  true
#define TEMPLOG_OFF false

typedef char sensorId_t[VALID_ID_LENGTH + 1];
typedef char sensorName_t[15];
typedef byte sensorAddr_t[8];

enum         timeStamp_t { NO_TIME, UNIX_TIME, HUMAN_TIME, MILLIS_TIME };
typedef char timeStampBuffer_t[20];

class FFatSensor: public Task {
public:
  struct sensorState_t {                  /* struct to keep track of Dallas DS18B20 sensors */
    sensorAddr_t     addr{0};
    float            tempCelsius = NAN;
    bool             error = true;
  };
  FFatSensor();
  virtual ~FFatSensor();
  bool                  startSensors( const uint8_t pin, uint8_t maxsensors );
  void                  rescanSensors();
  uint8_t               sensorCount();
  float                 sensorTemp( const uint8_t num );
  bool                  sensorError( const uint8_t num );
  const char *          getSensorName( const uint8_t num, sensorName_t &name );
  const char *          getSensorName( const sensorId_t &id, sensorName_t &name );
  const char *          getSensorId( const uint8_t num, sensorId_t &id );
  bool                  setSensorName( const sensorId_t &id, const char * name );
  bool                  isTempLogging();
  bool                  isErrorLogging();
  bool                  startTempLogging( const uint32_t seconds = 180 );
  bool                  stopTempLogging();
  bool                  startErrorLogging();
  bool                  stopErrorLogging();
  bool                  appendToFile( const char * path, timeStamp_t ts, const char * message );
  const char *          timeStamp( const timeStamp_t type , timeStampBuffer_t &buf );

private:
  uint8_t               _maxSensors = 0;
  uint8_t               _count = 0;
  sensorState_t *       _state = nullptr;
  sensorState_t *       _tempState = nullptr;
  bool                  _errorlogging = false;
  void                  run( void * data );
  uint8_t               _scanSensors();
  bool                  _writelnFile( const char * path, const char * message );
  bool                  _logError( const uint8_t num, const char * path, const char * message, const byte data[9] );
};

#endif //FFATSENSOR_H
