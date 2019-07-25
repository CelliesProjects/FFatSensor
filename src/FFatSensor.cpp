#include <rom/rtc.h>
#include <list>
#include <FFat.h>
#include <OneWire.h>
#include <Preferences.h>

#include "FFatSensor.h"

static const char * SENSORERROR_FILENAME = "/sensor_error.txt";
static const char * UNKNOWN_SENSOR       = "unknown sensor";

static OneWire* _wire = nullptr;
static Preferences sensorPreferences;

static volatile bool tempLogTicker = false;
static hw_timer_t *   tempLogTimer = NULL;
static portMUX_TYPE       timerMux = portMUX_INITIALIZER_UNLOCKED;

static bool _rescan = false;

/* static functions */
static void IRAM_ATTR _onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  tempLogTicker = true;
  portEXIT_CRITICAL_ISR(&timerMux);
}

static void _deleteOldLogfiles( fs::FS &fs, const char * dirname, uint8_t levels ) {
  File root = FFat.open( dirname );
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
      if ( strstr( file.name(), ".log" ) ) {
        ESP_LOGD( TAG, "Pushing %s on stack.", file.name() );
        logFiles.push_back( file.name() );
      }
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
    ESP_LOGD( TAG, "Deleting oldest log file %s", filename.c_str() );
    FFat.remove( filename.c_str() );
    logFiles.erase( thisFile );
  }
}

static bool _saveTempLogStateToNVS( const bool state ) {
  return sensorPreferences.putBool( "logging", state );
};

/* FFatSensor member functions */
FFatSensor::FFatSensor() {}
FFatSensor::~FFatSensor() {}

bool FFatSensor::startSensors( const uint8_t num, const uint8_t pin ) {
  if ( nullptr != _wire ) {
    ESP_LOGE( TAG, "Sensors already running. Exiting." );
    return false;
  }
  _wire = new OneWire( pin );
  if ( nullptr == _wire ) {
    ESP_LOGE( TAG, "Could not allocate memory. OneWire not created." );
    return false;
  }
  _state = new sensorState_t[num];
  if ( nullptr == _state ) {
    ESP_LOGE( TAG, "Could not allocate primary buffer. No sensor objects created." );
    delete _wire;
    return false;
  }
  _tempState = new sensorState_t[num];
  if ( nullptr == _tempState ) {
    ESP_LOGE( TAG, "Could not allocate secondary buffer. No sensor objects created." );
    delete[] _state;
    delete _wire;
    return false;
  }
  _maxSensors = num;

  ESP_LOGD( TAG, "Created %i sensor objects.", num );
  sensorPreferences.begin( "FFatSensor", false );
  setStackSize(3500);
  setCore(1);
  setPriority(0);
  start();
  if ( sensorPreferences.getBool( "logging", false ) )
    startTempLogging();
  return true;
}

uint8_t FFatSensor::sensorCount() {
  return _count;
}

void FFatSensor::rescanSensors() {
  _rescan = true;
}

float FFatSensor::sensorTemp( const uint8_t num ) {
  return _state[num].tempCelsius;
}

bool FFatSensor::sensorError( const uint8_t num ) {
  return _state[num].error;
}

const char * FFatSensor::getSensorName( const uint8_t num, sensorName_t &name ) {
  sensorId_t id;
  getSensorId( num, id );
  return getSensorName( id, name );
}

const char * FFatSensor::getSensorName( const sensorId_t &id, sensorName_t &name ) {
  String result = sensorPreferences.getString( id, UNKNOWN_SENSOR );
  if ( result ) strncpy( name, result.c_str(), sizeof( sensorName_t ) );
  return name;
}

bool FFatSensor::setSensorName( const sensorId_t &id, const char * name ) {
  if ( 0 == strlen( name ) ) return sensorPreferences.remove( id );
  if ( strlen( name ) > sizeof( sensorName_t ) ) return false;
  return sensorPreferences.putString( id, name );
}

const char * FFatSensor::getSensorId( const uint8_t num, sensorId_t &id ) {
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

bool FFatSensor::startTempLogging( const uint32_t seconds ) {
  if ( NULL != tempLogTimer ) return false;
  tempLogTimer = timerBegin(0, 80, true);
  timerAttachInterrupt(tempLogTimer, &_onTimer, true);
  timerAlarmWrite(tempLogTimer, seconds * 1000000, true);
  timerAlarmEnable(tempLogTimer);
  _onTimer();
  _saveTempLogStateToNVS( TEMPLOG_ON );
  return true;
}

bool FFatSensor::stopTempLogging() {
  if ( NULL == tempLogTimer ) return false;
  timerAlarmDisable(tempLogTimer);
  timerDetachInterrupt(tempLogTimer);
  timerEnd(tempLogTimer);
  tempLogTimer = NULL;
  _saveTempLogStateToNVS( TEMPLOG_OFF );
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

bool FFatSensor::appendToFile( const char * path, const timeStamp_t type, const char * message ) {
  char buffer[100];
  timeStampBuffer_t tsb;
  snprintf( buffer, sizeof( buffer ), "%s%s", timeStamp( type, tsb ), message );
  return _writelnFile( path, buffer );
}

const char * FFatSensor::timeStamp( const timeStamp_t type , timeStampBuffer_t &buf ) {
  switch ( type ) {
   case UNIX_TIME: {
      time_t t = time(NULL);
      snprintf( buf , sizeof( timeStampBuffer_t ), "%i", t );
      break;
    }
    case HUMAN_TIME: {
      struct tm timeinfo = {0};
      getLocalTime( &timeinfo, 0 );
      strftime( buf , sizeof( timeStampBuffer_t ), "%x-%X", &timeinfo );
      break;
    }
    case MILLIS_TIME: {
      snprintf( buf , sizeof( timeStampBuffer_t ), "%i", millis() );
      break;
    }
    default: break;
  }
  return buf;
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
        case 0x10: {
          ESP_LOGD( TAG, "Dallas sensor type : DS18S20" );  /* or old DS1820 */
          type_s = 1;
          break;
        }
        case 0x28: {
          ESP_LOGD( TAG, "Dallas sensor type : DS18B20");
          type_s = 0;
          break;
        }
        case 0x22: {
          ESP_LOGD( TAG, "Dallas sensor type : DS1822");
          type_s = 0;
          break;
        }
        default:
          ESP_LOGE( TAG, "OneWire device is not a DS18x20 family device.");
      }

      if ( OneWire::crc8( data, 8 ) != data[8] ) {
        _tempState[num].error = true;
        _tempState[num].tempCelsius  = NAN;
        if ( _errorlogging && !_logError( num, SENSORERROR_FILENAME, "BAD_CRC", data ) )
          ESP_LOGE( TAG, "Error writing to '%s' (disk full/not mounted?)", SENSORERROR_FILENAME );
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
          if ( _errorlogging && !_logError( num, SENSORERROR_FILENAME, "BAD_TMP", data ) )
            ESP_LOGE( TAG, "Error writing to '%s' (disk full/not mounted?)", SENSORERROR_FILENAME );
        }
      }
      ESP_LOGD( TAG, "sensor %i: %.1f %s", num, _tempState[num].tempCelsius, _tempState[num].error ? "invalid" : "valid" );
      num++;
    }
    _state = _tempState;
    _count = loopCounter;

    if ( tempLogTicker ) {
      _deleteOldLogfiles( FFat, "/", 0 );

      struct tm timeinfo;
      getLocalTime( &timeinfo );
      char fileName[17];
      strftime( fileName , sizeof( fileName ), "/%F.log", &timeinfo );

      char content[35];
      uint8_t used = 0;
      if ( loopCounter ) {
        timeStampBuffer_t tsb;
        used += snprintf( content, sizeof( content ), "%s,%3.2f",timeStamp( UNIX_TIME, tsb ), _tempState[0].tempCelsius );
        for ( uint8_t num = 1; num < loopCounter; num++ )
          used += snprintf( content + used, sizeof( content ) - used, ",%3.2f", _tempState[num].tempCelsius  );
        if ( !_writelnFile( fileName, content ) )
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
  _wire->reset_search();
  _wire->target_search(0x28);
  uint8_t num(0);
  sensorAddr_t addr;
  while ( _wire->search( addr ) && ( num < _maxSensors ) ) {
    memcpy( _tempState[num].addr, addr, sizeof( sensorAddr_t ) );
    num++;
  }
  _rescan = false;
  return num;
}

bool FFatSensor::_writelnFile( const char * path, const char * message ) {
  File file = FFat.open( path, FILE_APPEND );
  if ( !file ) return false;
  if ( !file.println( message ) ) {
    file.close();
    return false;
  }
  file.close();
  return true;
}

bool FFatSensor::_logError( const uint8_t num, const char * path, const char * message, const byte data[9] ) {
  char content[100];
  snprintf( content, sizeof( content ), " - sensor:%i %s %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", num, message,
            data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8] );
  ESP_LOGE( TAG, "Writing sensor error: %s", content );
  return appendToFile( path, HUMAN_TIME, content );
}
