#include <rom/rtc.h>
#include <list>
#include <FFat.h>
#include <OneWire.h>
#include <Preferences.h>

#include "FFatSensor.h"

static const char * ERROR_LOG_NAME = "/sensor_error.txt";
static const char * UNKNOWN_SENSOR = "unknown sensor";

static OneWire* _wire = nullptr;
static Preferences sensorPreferences;

static volatile bool tempLogTicker = false;
static hw_timer_t * tempLogTimer = NULL;
static portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
static bool _rescan = false;

/* static functions */
static void IRAM_ATTR _onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  tempLogTicker = true;
  portEXIT_CRITICAL_ISR(&timerMux);
}

static bool _writelnFile( fs::FS &fs, const char * path, const char * message ) {
  File file = fs.open( path, FILE_APPEND );
  if ( !file ) return false;
  if ( !file.println( message ) ) {
    file.close();
    return false;
  }
  file.close();
  return true;
}

static void _deleteOldLogfiles( fs::FS &fs, const char * dirname, uint8_t levels ) {
  File root = fs.open( dirname );
  if ( !root ) {
    ESP_LOGE( TAG, "Failed to open %s", dirname );
    return;
  }
  if ( !root.isDirectory() ) {
    ESP_LOGE( TAG, "No root folder found." );
    return;
  }
  std::list<String> logFiles;
  File file = root.openNextFile();
  while ( file ) {
    if ( file.isDirectory() ) {
      if ( levels )
        _deleteOldLogfiles( fs, file.name(), levels - 1 );
    }
    else {
      if ( strstr( file.name(), ".log" ) )
        logFiles.push_back( file.name() );
    }
    file = root.openNextFile();
  }
  if ( logFiles.size() > SAVED_LOGFILES ) {
    logFiles.sort();
  }
  while ( logFiles.size() > SAVED_LOGFILES ) {
    std::list<String>::iterator thisFile;
    thisFile = logFiles.begin();
    String filename = *thisFile;
    ESP_LOGI( TAG, "Deleting oldest log file %s", filename.c_str() );
    fs.remove( filename.c_str() );
    logFiles.erase( thisFile );
  }
}

static bool _logError( const uint8_t num, const char * path, const char * message, const byte data[9] ) {
  File file = FFat.open( path, FILE_APPEND );
  if ( !file ) return false;
  time_t rawtime;
  struct tm * timeinfo;
  time ( &rawtime );
  timeinfo = localtime ( &rawtime );
  char timeBuff[20];
  strftime ( timeBuff, sizeof( timeBuff ), "%x %X", timeinfo );
  char buffer[100];
  snprintf( buffer, sizeof( buffer ), "%s - sensor:%i %s %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", timeBuff, num, message,
            data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8] );
  ESP_LOGE( TAG, "Writing sensor error: %s", buffer );
  if ( !file.println( buffer ) ) {
    file.close();
    return false;
  }
  file.close();
  return true;
}

static bool _saveTempStateToNVS( const bool state ) {
  return sensorPreferences.putBool( "logging", state );
};

/* FFatSensor member functions */
FFatSensor::FFatSensor() {}
FFatSensor::~FFatSensor() {}

bool FFatSensor::startSensors(uint8_t pin) {
  if ( nullptr != _wire ) {
    ESP_LOGE( TAG, "Sensors already running. Exiting." );
    return false;
  }
  _wire = new OneWire( pin );
  if ( nullptr == _wire ) {
    ESP_LOGE( TAG, "OneWire not created. (low mem?) Exiting." );
    return false;
  }
  sensorPreferences.begin( "FFatSensor", false );
  setStackSize(3500);
  setCore(1);
  setPriority(0);
  start();
  if ( sensorPreferences.getBool( "logging", false ) )
    startTempLogging();
  return true;
}

uint8_t FFatSensor::count() {
  return _count;
}

void FFatSensor::rescan() {
  _rescan = true;
}

float FFatSensor::temp( const uint8_t num ) {
  return _state[num].tempCelsius;
}

bool FFatSensor::error( const uint8_t num ) {
  return _state[num].error;
}

const char * FFatSensor::getName( const uint8_t num, sensorName_t &name ) {
  sensorId_t id;
  getId( num, id );
  return getName( id, name );
}

const char * FFatSensor::getName( const sensorName_t &id, sensorName_t &name ) {
  String result = sensorPreferences.getString( id, UNKNOWN_SENSOR );
  if ( result ) strncpy( name, result.c_str(), sizeof( sensorName_t ) );
  return name;
}

bool FFatSensor::setName( const sensorId_t &id, const char * name ) {
  if ( 0 == strlen( name ) ) return sensorPreferences.remove( id );
  if ( strlen( name ) > sizeof( sensorName_t ) ) return false;
  return sensorPreferences.putString( id, name );
}

const char * FFatSensor::getId( const uint8_t num, sensorId_t &id ) {
  snprintf( id, sizeof( sensorId_t ), "%02x%02x%02x%02x%02x%02x%02x",
            _state[num].addr[1], _state[num].addr[2], _state[num].addr[3], _state[num].addr[4],
            _state[num].addr[5], _state[num].addr[6], _state[num].addr[7] );
  return id;
}

bool FFatSensor::isTempLogging() {
  return ( NULL != tempLogTimer );
}

bool FFatSensor::isErrorLogging() {
  return _errorlogging;
}

bool FFatSensor::stopTempLogging() {
  if ( NULL == tempLogTimer ) return false;
  _saveTempStateToNVS( false );
  timerAlarmDisable(tempLogTimer);
  timerDetachInterrupt(tempLogTimer);
  timerEnd(tempLogTimer);
  tempLogTimer = NULL;
  return true;
}

bool FFatSensor::startTempLogging( const uint32_t seconds ) {
  if ( NULL != tempLogTimer ) return false;
  tempLogTimer = timerBegin(0, 80, true);
  timerAttachInterrupt(tempLogTimer, &_onTimer, true);
  timerAlarmWrite(tempLogTimer, seconds * 1000000, true);
  timerAlarmEnable(tempLogTimer);
  _saveTempStateToNVS( true );
  _onTimer();
  return true;
}

bool FFatSensor::startErrorLogging() {
  _errorlogging = true;
  return true;
}

bool FFatSensor::stopErrorLogging() {
  _errorlogging = false;
  return true;
}

void FFatSensor::run( void * data ) {
  uint8_t loopCounter = _scanSensors();
  while (1) {
    ESP_LOGV( TAG, "Stack left: %i", uxTaskGetStackHighWaterMark( NULL ) );
    if ( _rescan ) loopCounter = _scanSensors();
    _wire->reset();
    _wire->write( 0xCC, 0); /* Skip ROM - All sensors */
    _wire->write( 0x44, 0); /* start conversion, with parasite power off at the end */
    vTaskDelay( 750 ); //wait for conversion ready
    uint8_t num = 0;
    while ( num < loopCounter ) {
      byte data[12];
      _tempState[num].error = true; /* we start with an error, which will be cleared if the CRC checks out. */
      _wire->reset();
      _wire->select( _tempState[num].addr );
      _wire->write( 0xBE );         /* Read Scratchpad */
      for ( byte i = 0; i < 9; i++ ) data[i] = _wire->read(); // we need 9 bytes
      ESP_LOGD( TAG, "Sensor %i data=%02x%02x%02x%02x%02x%02x%02x%02x%02x", num,
                     data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8] );
      byte type_s;
      // the first ROM byte indicates which chip
      switch ( _tempState[num].addr[0] ) {
        case 0x10:
          ESP_LOGD( TAG, "Dallas sensor type : DS18S20" );  /* or old DS1820 */
          type_s = 1;
          break;
        case 0x28:
          ESP_LOGD( TAG, "Dallas sensor type : DS18B20");
          type_s = 0;
          break;
        case 0x22:
          ESP_LOGD( TAG, "Dallas sensor type : DS1822");
          type_s = 0;
          break;
        default:
          ESP_LOGE( TAG, "OneWire device is not a DS18x20 family device.");
      }

      if ( OneWire::crc8( data, 8 ) != data[8] ) {
        _tempState[num].error = true;
        _tempState[num].tempCelsius  = NAN;
        if ( _errorlogging && !_logError( num, ERROR_LOG_NAME, "BAD_CRC", data ) )
          ESP_LOGE( TAG, "Error writing to '%s' (disk full/not mounted?)" );
      }
      else {
        int16_t raw = (data[1] << 8) | data[0];
        if (type_s) {
          raw = raw << 3; // 9 bit resolution default
          if (data[7] == 0x10) {
            // "count remain" gives full 12 bit resolution
            raw = (raw & 0xFFF0) + 12 - data[6];
          }
        }
        else {
          byte cfg = (data[4] & 0x60);
          // at lower res, the low bits are undefined, so let's zero them
          if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
          else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
          else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
          //// default is 12 bit resolution, 750 ms conversion time
        }
        _tempState[num].tempCelsius = raw / 16.0;

        if ( _tempState[num].tempCelsius > -55.0 && _tempState[num].tempCelsius  < 85.0 ) {
          _tempState[num].error = false;
        }
        else {
          _tempState[num].error = true;
          _tempState[num].tempCelsius  = NAN;
          if ( _errorlogging && !_logError( num, ERROR_LOG_NAME, "BAD_TMP", data ) )
            ESP_LOGE( TAG, "Error writing to '%s' (disk full/not mounted?)" );
        }
      }
      ESP_LOGD( TAG, "sensor %i: %.1f %s", num, _tempState[num].tempCelsius, _tempState[num].error ? "invalid" : "valid" );
      num++;
    }
    memcpy( &_state, &_tempState, sizeof( sensorState_t[ MAX_NUMBER_OF_SENSORS ] ) );
    _count = loopCounter;

    if ( tempLogTicker ) {
      time_t                  now;
      struct tm          timeinfo;
      char           fileName[17];

      time( &now );
      localtime_r( &now, &timeinfo );
      strftime( fileName , sizeof( fileName ), "/%F.log", &timeinfo );

      _deleteOldLogfiles( FFat, "/", 0 );

      char content[60];
      uint8_t charCount = 0;
      if ( loopCounter ) {
        charCount += snprintf( content, sizeof( content ), "%i,", now );
        charCount += snprintf( content + charCount, sizeof( content ) - charCount, "%3.2f", _tempState[0].tempCelsius  );
        for  ( uint8_t sensorNumber = 1; sensorNumber < loopCounter; sensorNumber++ )
          charCount += snprintf( content + charCount, sizeof( content ) - charCount, ",%3.2f", _tempState[sensorNumber].tempCelsius  );
        if ( !_writelnFile( FFat, fileName, content ) )
          ESP_LOGE( TAG, "%s", "Failed to write to log file." );
      }
      ESP_LOGD( TAG, "Sensor values '%s' logged to '%s'", content, fileName );

      portENTER_CRITICAL(&timerMux);
      tempLogTicker = false;
      portEXIT_CRITICAL(&timerMux);
    }
  }
}

uint8_t FFatSensor::_scanSensors() {
  uint8_t num = 0;
  sensorAddr_t currentAddr;
  _wire->reset_search();
  _wire->target_search(0x28);
  vTaskPrioritySet( NULL, 10 );
  while ( _wire->search( currentAddr ) && ( num < MAX_NUMBER_OF_SENSORS ) ) {
    _tempState[num].error = true;
    _tempState[num].tempCelsius = NAN;
    memcpy( _tempState[num].addr, currentAddr, sizeof( sensorState_t::addr ) );
    num++;
  }
  vTaskPrioritySet( NULL, 0);
  _rescan = false;
  return num;
}

