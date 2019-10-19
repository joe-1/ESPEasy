#ifndef ESPEASY_GLOBALS_H_
#define ESPEASY_GLOBALS_H_

#ifndef CORE_POST_2_5_0
  #define STR_HELPER(x) #x
  #define STR(x) STR_HELPER(x)
#endif

#ifdef __GCC__
#pragma GCC system_header
#endif

/*
    To modify the stock configuration without changing this repo file :
    - define USE_CUSTOM_H as a build flags. ie : export PLATFORMIO_BUILD_FLAGS="'-DUSE_CUSTOM_H'"
    - add a "Custom.h" file in this folder.

*/
#ifdef USE_CUSTOM_H
// make the compiler show a warning to confirm that this file is inlcuded
#warning "**** Using Settings from Custom.h File ***"
#include "Custom.h"
#endif



#include "ESPEasy_common.h"
#include "ESPEasy_fdwdecl.h"

#include "src/DataStructs/ESPEasyLimits.h"
#include "ESPEasy_plugindefs.h"







//#include <FS.h>



//enable reporting status to ESPEasy developers.
//this informs us of crashes and stability issues.
// not finished yet!
// #define FEATURE_REPORTING

//Select which plugin sets you want to build.
//These are normally automaticly set via the Platformio build environment.
//If you use ArduinoIDE you might need to uncomment some of them, depending on your needs
//If you dont select any, a version with a minimal number of plugins will be biult for 512k versions.
//(512k is NOT finsihed or tested yet as of v2.0.0-dev6)

//build all the normal stable plugins (on by default)
#define PLUGIN_BUILD_NORMAL

//build all plugins that are in test stadium
//#define PLUGIN_BUILD_TESTING

//build all plugins that still are being developed and are broken or incomplete
//#define PLUGIN_BUILD_DEV

//add this if you want SD support (add 10k flash)
//#define FEATURE_SD


// User configuration
#include "src/DataStructs/ESPEasyDefaults.h"

// Make sure to have this as early as possible in the build process.
#include "define_plugin_sets.h"


// ********************************************************************************
//   DO NOT CHANGE ANYTHING BELOW THIS LINE
// ********************************************************************************


#define CMD_REBOOT                         89
#define CMD_WIFI_DISCONNECT               135



/*
// TODO TD-er: Declare global variables as extern and construct them in the .cpp.
// Move all other defines in this file to separate .h files
// This file should only have the "extern" declared global variables so it can be included where they are needed.
//
// For a very good tutorial on how C++ handles global variables, see:
//    https://www.fluentcpp.com/2019/07/23/how-to-define-a-global-constant-in-cpp/
// For more information about the discussion which lead to this big change:
//    https://github.com/letscontrolit/ESPEasy/issues/2621#issuecomment-533673956
*/


#include "src/DataStructs/NotificationSettingsStruct.h"
#include "src/DataStructs/NotificationStruct.h"

<<<<<<< HEAD
#define CONTROLLER_MAX                      3 // max 4!
#define NOTIFICATION_MAX                    3 // max 4!
#define VARS_PER_TASK                       4
#define PLUGIN_MAX                DEVICES_MAX
#define PLUGIN_CONFIGVAR_MAX                8
#define PLUGIN_CONFIGFLOATVAR_MAX           4
#define PLUGIN_CONFIGLONGVAR_MAX            4
#define PLUGIN_EXTRACONFIGVAR_MAX          16
#define CPLUGIN_MAX                        16
#define NPLUGIN_MAX                         4
#define UNIT_MAX                          254 // unit 255 = broadcast
#define RULES_TIMER_MAX                     8
//#define PINSTATE_TABLE_MAX                 32
#define RULES_MAX_SIZE                   2048
#define RULES_MAX_NESTING_LEVEL             3
#define RULESETS_MAX                        4
#define RULES_BUFFER_SIZE                  64
#define NAME_FORMULA_LENGTH_MAX            40
#define RULES_IF_MAX_NESTING_LEVEL          4
#define CUSTOM_VARS_MAX                    16

#define UDP_PACKETSIZE_MAX               2048



#define DEVICE_TYPE_SINGLE                  1  // connected through 1 datapin
#define DEVICE_TYPE_DUAL                    2  // connected through 2 datapins
#define DEVICE_TYPE_TRIPLE                  3  // connected through 3 datapins
#define DEVICE_TYPE_ANALOG                 10  // AIN/tout pin
#define DEVICE_TYPE_I2C                    20  // connected through I2C
#define DEVICE_TYPE_DUMMY                  99  // Dummy device, has no physical connection

#define SENSOR_TYPE_NONE                    0
#define SENSOR_TYPE_SINGLE                  1
#define SENSOR_TYPE_TEMP_HUM                2
#define SENSOR_TYPE_TEMP_BARO               3
#define SENSOR_TYPE_TEMP_HUM_BARO           4
#define SENSOR_TYPE_DUAL                    5
#define SENSOR_TYPE_TRIPLE                  6
#define SENSOR_TYPE_QUAD                    7
#define SENSOR_TYPE_TEMP_EMPTY_BARO         8
#define SENSOR_TYPE_SWITCH                 10
#define SENSOR_TYPE_DIMMER                 11
#define SENSOR_TYPE_LONG                   20
#define SENSOR_TYPE_WIND                   21

#define UNIT_NUMBER_MAX                  9999  // Stored in Settings.Unit
#define DOMOTICZ_MAX_IDX            999999999  // Looks like it is an unsigned int, so could be up to 4 bln.

#define VALUE_SOURCE_SYSTEM                 1
#define VALUE_SOURCE_SERIAL                 2
#define VALUE_SOURCE_HTTP                   3
#define VALUE_SOURCE_MQTT                   4
#define VALUE_SOURCE_UDP                    5
#define VALUE_SOURCE_WEB_FRONTEND           6

#define BOOT_CAUSE_MANUAL_REBOOT            0
#define BOOT_CAUSE_COLD_BOOT                1
#define BOOT_CAUSE_DEEP_SLEEP               2
#define BOOT_CAUSE_EXT_WD                  10

#define DAT_TASKS_DISTANCE               2048  // DAT_TASKS_SIZE + DAT_TASKS_CUSTOM_SIZE
#define DAT_TASKS_SIZE                   1024
#define DAT_TASKS_CUSTOM_OFFSET          1024  // Equal to DAT_TASKS_SIZE
#define DAT_TASKS_CUSTOM_SIZE            1024
#define DAT_CUSTOM_CONTROLLER_SIZE       1024
#define DAT_CONTROLLER_SIZE              1024
#define DAT_NOTIFICATION_SIZE            1024

#define DAT_BASIC_SETTINGS_SIZE          4096
=======
extern NotificationStruct Notification[NPLUGIN_MAX];
>>>>>>> 79975d385bc2353f4f4297249fff1d850b962309








#include "ESPEasy_Log.h"
#include "ESPEasyTimeTypes.h"
#include "StringProviderTypes.h"
#include "ESPeasySerial.h"
#include "ESPEasy_fdwdecl.h"
#include "WebServer_fwddecl.h"
#include "I2CTypes.h"
#include <I2Cdev.h>


#define FS_NO_GLOBALS
#if defined(ESP8266)
  #include "core_version.h"
  #define NODE_TYPE_ID      NODE_TYPE_ID_ESP_EASYM_STD
  #define FILE_CONFIG       "config.dat"
  #define FILE_SECURITY     "security.dat"
  #define FILE_NOTIFICATION "notification.dat"
  #define FILE_RULES        "rules1.txt"
  #include <lwip/init.h>
  #ifndef LWIP_VERSION_MAJOR
    #error
  #endif
  #if LWIP_VERSION_MAJOR == 2
  //  #include <lwip/priv/tcp_priv.h>
  #else
    #include <lwip/tcp_impl.h>
  #endif
  #include <ESP8266WiFi.h>
  //#include <ESP8266Ping.h>
  #include <DNSServer.h>
  #include <Servo.h>
  #ifndef LWIP_OPEN_SRC
  #define LWIP_OPEN_SRC
  #endif
  #include "lwip/opt.h"
  #include "lwip/udp.h"
  #include "lwip/igmp.h"
  #include "include/UdpContext.h"
  #include "limits.h"
  extern "C" {
   #include "user_interface.h"
  }
  extern "C" {
  #include "spi_flash.h"
  }
  #ifdef CORE_POST_2_6_0
    extern "C" uint32_t _FS_start;
    extern "C" uint32_t _FS_end;
    extern "C" uint32_t _FS_page;
    extern "C" uint32_t _FS_block;
  #else
    extern "C" uint32_t _SPIFFS_start;
    extern "C" uint32_t _SPIFFS_end;
    extern "C" uint32_t _SPIFFS_page;
    extern "C" uint32_t _SPIFFS_block;
  #endif

  #ifdef FEATURE_MDNS
    #include <ESP8266mDNS.h>
  #endif
  #define SMALLEST_OTA_IMAGE 276848 // smallest known 2-step OTA image
  #define MAX_SKETCH_SIZE 1044464
  #define PIN_D_MAX        16
#endif
#if defined(ESP32)

  // Temp fix for a missing core_version.h within ESP Arduino core. Wait until they actually have different releases
  #define ARDUINO_ESP8266_RELEASE "2_4_0"

  #define NODE_TYPE_ID                        NODE_TYPE_ID_ESP_EASY32_STD
  #define ICACHE_RAM_ATTR IRAM_ATTR
  #define FILE_CONFIG       "/config.dat"
  #define FILE_SECURITY     "/security.dat"
  #define FILE_NOTIFICATION "/notification.dat"
  #define FILE_RULES        "/rules1.txt"
  #include <WiFi.h>
//  #include  "esp32_ping.h"
  #include "SPIFFS.h"
  #include <rom/rtc.h>
  #include "esp_wifi.h" // Needed to call ESP-IDF functions like esp_wifi_....
  #ifdef FEATURE_MDNS
    #include <ESPmDNS.h>
  #endif
  #define PIN_D_MAX        39
  extern int8_t ledChannelPin[16];
#endif

#include <WiFiUdp.h>
#include <DNSServer.h>
#include <Wire.h>
#include <SPI.h>
#include <FS.h>
#ifdef FEATURE_SD
#include <SD.h>
#else
using namespace fs;
#endif
#include <base64.h>


extern I2Cdev i2cdev;





// Setup DNS, only used if the ESP has no valid WiFi config
extern const byte DNS_PORT;
extern IPAddress apIP;
extern DNSServer dnsServer;
extern bool dnsServerActive;

//NTP status
extern bool statusNTPInitialized;

// udp protocol stuff (syslog, global sync, node info list, ntp time)
extern WiFiUDP portUDP;


/*********************************************************************************************\
 * Custom Variables for usage in rules and http.
 * Syntax: %vX%
 * usage:
 * let,1,10
 * if %v1%=10 do ...
\*********************************************************************************************/
extern float customFloatVar[CUSTOM_VARS_MAX];

extern float UserVar[VARS_PER_TASK * TASKS_MAX];















enum gpio_direction {
  gpio_input,
  gpio_output,
  gpio_bidirectional
};


/*********************************************************************************************\
 * pinStatesStruct
\*********************************************************************************************/
/*
struct pinStatesStruct
{
  pinStatesStruct() : value(0), plugin(0), index(0), mode(0) {}
  uint16_t value;
  byte plugin;
  byte index;
  byte mode;
} pinStates[PINSTATE_TABLE_MAX];
*/


extern int deviceCount;
extern int protocolCount;
extern int notificationCount;

extern boolean printToWeb;
extern String printWebString;
extern boolean printToWebJSON;

/********************************************************************************************\
  RTC_cache_struct
\*********************************************************************************************/
struct RTC_cache_struct
{
  uint32_t checksumData = 0;
  uint16_t readFileNr = 0;       // File number used to read from.
  uint16_t writeFileNr = 0;      // File number to write to.
  uint16_t readPos = 0;          // Read position in file based cache
  uint16_t writePos = 0;         // Write position in the RTC memory
  uint32_t checksumMetadata = 0;
};

struct RTC_cache_handler_struct;


/*********************************************************************************************\
 * rulesTimerStruct
\*********************************************************************************************/
struct rulesTimerStatus
{
  rulesTimerStatus() : timestamp(0), interval(0), paused(false) {}

  unsigned long timestamp;
  unsigned int interval; //interval in milliseconds
  boolean paused;
};

extern rulesTimerStatus RulesTimer[RULES_TIMER_MAX];

extern msecTimerHandlerStruct msecTimerHandler;

extern unsigned long timer_gratuitous_arp_interval;
extern unsigned long timermqtt_interval;
extern unsigned long lastSend;
extern unsigned long lastWeb;
extern byte cmd_within_mainloop;
extern unsigned long wdcounter;
extern unsigned long timerAPoff;    // Timer to check whether the AP mode should be disabled (0 = disabled)
extern unsigned long timerAPstart;  // Timer to start AP mode, started when no valid network is detected.
extern unsigned long timerAwakeFromDeepSleep;
extern unsigned long last_system_event_run;

#if FEATURE_ADC_VCC
extern float vcc;
#endif

extern boolean WebLoggedIn;
extern int WebLoggedInTimer;


extern bool (*CPlugin_ptr[CPLUGIN_MAX])(byte, struct EventStruct*, String&);
extern byte CPlugin_id[CPLUGIN_MAX];

extern boolean (*NPlugin_ptr[NPLUGIN_MAX])(byte, struct EventStruct*, String&);
extern byte NPlugin_id[NPLUGIN_MAX];

extern String dummyString;  // FIXME @TD-er  This may take a lot of memory over time, since long-lived Strings only tend to grow.

enum PluginPtrType {
  TaskPluginEnum,
  ControllerPluginEnum,
  NotificationPluginEnum,
  CommandTimerEnum
};
void schedule_event_timer(PluginPtrType ptr_type, byte Index, byte Function, struct EventStruct* event);
unsigned long createSystemEventMixedId(PluginPtrType ptr_type, byte Index, byte Function);
unsigned long createSystemEventMixedId(PluginPtrType ptr_type, uint16_t crc16);







extern bool webserverRunning;
extern bool webserver_init;


extern String eventBuffer;


extern bool shouldReboot;
extern bool firstLoop;

extern boolean activeRuleSets[RULESETS_MAX];

extern boolean UseRTOSMultitasking;


// void (*MainLoopCall_ptr)(void); //FIXME TD-er: No idea what this does.


/*
String getLogLine(const TimingStats& stats) {
    unsigned long minVal, maxVal;
    unsigned int c = stats.getMinMax(minVal, maxVal);
    String log;
    log.reserve(64);
    log += F("Count: ");
    log += c;
    log += F(" Avg/min/max ");
    log += stats.getAvg();
    log += '/';
    log += minVal;
    log += '/';
    log += maxVal;
    log += F(" usec");
    return log;
}
*/

<<<<<<< HEAD
String getPluginFunctionName(int function) {
    switch(function) {
        case PLUGIN_INIT_ALL:              return F("INIT_ALL");
        case PLUGIN_INIT:                  return F("INIT");
        case PLUGIN_READ:                  return F("READ");
        case PLUGIN_ONCE_A_SECOND:         return F("ONCE_A_SECOND");
        case PLUGIN_TEN_PER_SECOND:        return F("TEN_PER_SECOND");
        case PLUGIN_DEVICE_ADD:            return F("DEVICE_ADD");
        case PLUGIN_EVENTLIST_ADD:         return F("EVENTLIST_ADD");
        case PLUGIN_WEBFORM_SAVE:          return F("WEBFORM_SAVE");
        case PLUGIN_WEBFORM_LOAD:          return F("WEBFORM_LOAD");
        case PLUGIN_WEBFORM_SHOW_VALUES:   return F("WEBFORM_SHOW_VALUES");
        case PLUGIN_GET_DEVICENAME:        return F("GET_DEVICENAME");
        case PLUGIN_GET_DEVICEVALUENAMES:  return F("GET_DEVICEVALUENAMES");
        case PLUGIN_WRITE:                 return F("WRITE");
        case PLUGIN_EVENT_OUT:             return F("EVENT_OUT");
        case PLUGIN_WEBFORM_SHOW_CONFIG:   return F("WEBFORM_SHOW_CONFIG");
        case PLUGIN_SERIAL_IN:             return F("SERIAL_IN");
        case PLUGIN_UDP_IN:                return F("UDP_IN");
        case PLUGIN_CLOCK_IN:              return F("CLOCK_IN");
        case PLUGIN_TIMER_IN:              return F("TIMER_IN");
        case PLUGIN_FIFTY_PER_SECOND:      return F("FIFTY_PER_SECOND");
        case PLUGIN_SET_CONFIG:            return F("SET_CONFIG");
        case PLUGIN_GET_DEVICEGPIONAMES:   return F("GET_DEVICEGPIONAMES");
        case PLUGIN_EXIT:                  return F("EXIT");
        case PLUGIN_GET_CONFIG:            return F("GET_CONFIG");
        case PLUGIN_UNCONDITIONAL_POLL:    return F("UNCONDITIONAL_POLL");
        case PLUGIN_REQUEST:               return F("REQUEST");
    }
    return F("Unknown");
}

bool mustLogFunction(int function) {
    switch(function) {
        case PLUGIN_INIT_ALL:              return false;
        case PLUGIN_INIT:                  return false;
        case PLUGIN_READ:                  return true;
        case PLUGIN_ONCE_A_SECOND:         return true;
        case PLUGIN_TEN_PER_SECOND:        return true;
        case PLUGIN_DEVICE_ADD:            return false;
        case PLUGIN_EVENTLIST_ADD:         return false;
        case PLUGIN_WEBFORM_SAVE:          return false;
        case PLUGIN_WEBFORM_LOAD:          return false;
        case PLUGIN_WEBFORM_SHOW_VALUES:   return false;
        case PLUGIN_GET_DEVICENAME:        return false;
        case PLUGIN_GET_DEVICEVALUENAMES:  return false;
        case PLUGIN_WRITE:                 return true;
        case PLUGIN_EVENT_OUT:             return true;
        case PLUGIN_WEBFORM_SHOW_CONFIG:   return false;
        case PLUGIN_SERIAL_IN:             return true;
        case PLUGIN_UDP_IN:                return true;
        case PLUGIN_CLOCK_IN:              return false;
        case PLUGIN_TIMER_IN:              return true;
        case PLUGIN_FIFTY_PER_SECOND:      return true;
        case PLUGIN_SET_CONFIG:            return false;
        case PLUGIN_GET_DEVICEGPIONAMES:   return false;
        case PLUGIN_EXIT:                  return false;
        case PLUGIN_GET_CONFIG:            return false;
        case PLUGIN_UNCONDITIONAL_POLL:    return false;
        case PLUGIN_REQUEST:               return true;
    }
    return false;
}

std::map<int,TimingStats> pluginStats;
std::map<int,TimingStats> miscStats;
unsigned long timediff_calls = 0;
unsigned long timediff_cpu_cycles_total = 0;
unsigned long timingstats_last_reset = 0;

#define LOADFILE_STATS        0
#define SAVEFILE_STATS        1
#define LOOP_STATS            2
#define PLUGIN_CALL_50PS      3
#define PLUGIN_CALL_10PS      4
#define PLUGIN_CALL_10PSU     5
#define PLUGIN_CALL_1PS       6
#define SENSOR_SEND_TASK      7
#define SEND_DATA_STATS       8
#define COMPUTE_FORMULA_STATS 9
#define PROC_SYS_TIMER       10
#define SET_NEW_TIMER        11
#define TIME_DIFF_COMPUTE    12
#define MQTT_DELAY_QUEUE     13
#define C001_DELAY_QUEUE     14
#define C002_DELAY_QUEUE     15
#define C003_DELAY_QUEUE     16
#define C004_DELAY_QUEUE     17
#define C005_DELAY_QUEUE     18
#define C006_DELAY_QUEUE     19
#define C007_DELAY_QUEUE     20
#define C008_DELAY_QUEUE     21
#define C009_DELAY_QUEUE     22
#define C010_DELAY_QUEUE     23
#define C011_DELAY_QUEUE     24
#define C012_DELAY_QUEUE     25
#define C013_DELAY_QUEUE     26
#define TRY_CONNECT_HOST_TCP 27
#define TRY_CONNECT_HOST_UDP 28
#define HOST_BY_NAME_STATS   29
#define CONNECT_CLIENT_STATS 30
#define LOAD_CUSTOM_TASK_STATS 31
#define WIFI_ISCONNECTED_STATS 32




#define START_TIMER const unsigned statisticsTimerStart(micros());
#define STOP_TIMER_TASK(T,F)  if (mustLogFunction(F)) pluginStats[T*32 + F].add(usecPassedSince(statisticsTimerStart));
//#define STOP_TIMER_LOADFILE miscStats[LOADFILE_STATS].add(usecPassedSince(statisticsTimerStart));
#define STOP_TIMER(L)       miscStats[L].add(usecPassedSince(statisticsTimerStart));


String getMiscStatsName(int stat) {
    switch (stat) {
        case LOADFILE_STATS:        return F("Load File");
        case SAVEFILE_STATS:        return F("Save File");
        case LOOP_STATS:            return F("Loop");
        case PLUGIN_CALL_50PS:      return F("Plugin call 50 p/s");
        case PLUGIN_CALL_10PS:      return F("Plugin call 10 p/s");
        case PLUGIN_CALL_10PSU:     return F("Plugin call 10 p/s U");
        case PLUGIN_CALL_1PS:       return F("Plugin call  1 p/s");
        case SENSOR_SEND_TASK:      return F("SensorSendTask()");
        case SEND_DATA_STATS:       return F("sendData()");
        case COMPUTE_FORMULA_STATS: return F("Compute formula");
        case PROC_SYS_TIMER:        return F("proc_system_timer()");
        case SET_NEW_TIMER:         return F("setNewTimerAt()");
        case TIME_DIFF_COMPUTE:     return F("timeDiff()");
        case MQTT_DELAY_QUEUE:      return F("Delay queue MQTT");
        case TRY_CONNECT_HOST_TCP:  return F("try_connect_host() (TCP)");
        case TRY_CONNECT_HOST_UDP:  return F("try_connect_host() (UDP)");
        case HOST_BY_NAME_STATS:    return F("hostByName()");
        case CONNECT_CLIENT_STATS:  return F("connectClient()");
        case LOAD_CUSTOM_TASK_STATS: return F("LoadCustomTaskSettings()");
        case WIFI_ISCONNECTED_STATS: return F("WiFi.isConnected()");
        case C001_DELAY_QUEUE:
        case C002_DELAY_QUEUE:
        case C003_DELAY_QUEUE:
        case C004_DELAY_QUEUE:
        case C005_DELAY_QUEUE:
        case C006_DELAY_QUEUE:
        case C007_DELAY_QUEUE:
        case C008_DELAY_QUEUE:
        case C009_DELAY_QUEUE:
        case C010_DELAY_QUEUE:
        case C011_DELAY_QUEUE:
        case C012_DELAY_QUEUE:
        case C013_DELAY_QUEUE:
        {
          String result;
          result.reserve(16);
          result = F("Delay queue ");
          result += get_formatted_Controller_number(static_cast<int>(stat - C001_DELAY_QUEUE + 1));
          return result;
        }
    }
    return F("Unknown");
}


/********************************************************************************************\
  Pre defined settings for off-the-shelf hardware
  \*********************************************************************************************/

// This enum will be stored, so do not change order or at least the values.
enum DeviceModel {
  DeviceModel_default = 0,
  DeviceModel_Sonoff_Basic,
  DeviceModel_Sonoff_TH1x,
  DeviceModel_Sonoff_S2x,
  DeviceModel_Sonoff_TouchT1,
  DeviceModel_Sonoff_TouchT2,
  DeviceModel_Sonoff_TouchT3,
  DeviceModel_Sonoff_4ch,
  DeviceModel_Sonoff_POW,
  DeviceModel_Sonoff_POWr2,
  DeviceModel_Shelly1,

  DeviceModel_MAX
};
=======


>>>>>>> 79975d385bc2353f4f4297249fff1d850b962309











#include "src/DataStructs/DeviceModel.h"

struct GpioFactorySettingsStruct {
  GpioFactorySettingsStruct(DeviceModel model = DeviceModel_default) {
    for (int i = 0; i < 4; ++i) {
      button[i] = -1;
      relais[i] = -1;
    }
    switch (model) {
      case DeviceModel_Sonoff_Basic:
      case DeviceModel_Sonoff_TH1x:
      case DeviceModel_Sonoff_S2x:
      case DeviceModel_Sonoff_TouchT1:
      case DeviceModel_Sonoff_POWr2:
        button[0] = 0;   // Single Button
        relais[0] = 12;  // Red Led and Relay (0 = Off, 1 = On)
        status_led = 13; // Green/Blue Led (0 = On, 1 = Off)
        i2c_sda = -1;
        i2c_scl = -1;
        break;
      case DeviceModel_Sonoff_POW:
        button[0] = 0;   // Single Button
        relais[0] = 12;  // Red Led and Relay (0 = Off, 1 = On)
        status_led = 15; // Blue Led (0 = On, 1 = Off)
        i2c_sda = -1;
        i2c_scl = -1;    // GPIO5 conflicts with HLW8012 Sel output
        break;
      case DeviceModel_Sonoff_TouchT2:
        button[0] = 0;   // Button 1
        button[1] = 9;   // Button 2
        relais[0] = 12;  // Led and Relay1 (0 = Off, 1 = On)
        relais[1] = 4;   // Led and Relay2 (0 = Off, 1 = On)
        status_led = 13; // Blue Led (0 = On, 1 = Off)
        i2c_sda = -1;    // GPIO4 conflicts with GPIO_REL3
        i2c_scl = -1;    // GPIO5 conflicts with GPIO_REL2
        break;
      case DeviceModel_Sonoff_TouchT3:
        button[0] = 0;   // Button 1
        button[1] = 10;  // Button 2
        button[2] = 9;   // Button 3
        relais[0] = 12;  // Led and Relay1 (0 = Off, 1 = On)
        relais[1] = 5;   // Led and Relay2 (0 = Off, 1 = On)
        relais[2] = 4;   // Led and Relay3 (0 = Off, 1 = On)
        status_led = 13; // Blue Led (0 = On, 1 = Off)
        i2c_sda = -1;    // GPIO4 conflicts with GPIO_REL3
        i2c_scl = -1;    // GPIO5 conflicts with GPIO_REL2
        break;

      case DeviceModel_Sonoff_4ch:
        button[0] = 0;   // Button 1
        button[1] = 9;   // Button 2
        button[2] = 10;  // Button 3
        button[3] = 14;  // Button 4
        relais[0] = 12;  // Red Led and Relay1 (0 = Off, 1 = On)
        relais[1] = 5;   // Red Led and Relay2 (0 = Off, 1 = On)
        relais[2] = 4;   // Red Led and Relay3 (0 = Off, 1 = On)
        relais[3] = 15;  // Red Led and Relay4 (0 = Off, 1 = On)
        status_led = 13; // Blue Led (0 = On, 1 = Off)
        i2c_sda = -1;    // GPIO4 conflicts with GPIO_REL3
        i2c_scl = -1;    // GPIO5 conflicts with GPIO_REL2
        break;
      case DeviceModel_Shelly1:
        button[0] = 5;   // Single Button
        relais[0] = 4;   // Red Led and Relay (0 = Off, 1 = On)
        status_led = 15; // Blue Led (0 = On, 1 = Off)
        i2c_sda = -1;    // GPIO4 conflicts with relay control.
        i2c_scl = -1;    // GPIO5 conflicts with SW input
        break;

      // case DeviceModel_default: break;
      default: break;
    }
  }

  int8_t button[4];
  int8_t relais[4];
  int8_t status_led = DEFAULT_PIN_STATUS_LED;
  int8_t i2c_sda = DEFAULT_PIN_I2C_SDA;
  int8_t i2c_scl = DEFAULT_PIN_I2C_SCL;
};

void addPredefinedPlugins(const GpioFactorySettingsStruct& gpio_settings);
void addPredefinedRules(const GpioFactorySettingsStruct& gpio_settings);



// These wifi event functions must be in a .h-file because otherwise the preprocessor
// may not filter the ifdef checks properly.
// Also the functions use a lot of global defined variables, so include at the end of this file.
#include "ESPEasyWiFiEvent.h"
#define SPIFFS_CHECK(result, fname) if (!(result)) { return(FileError(__LINE__, fname)); }
#include "WebServer_Rules.h"
#include "ESPEasy-GPIO.h"




#endif /* ESPEASY_GLOBALS_H_ */
