#include "../Commands/Timer.h"




#include "../../ESPEasy_common.h"


#include "../Commands/Common.h"

#include "../ESPEasyCore/ESPEasy_Log.h"
#include "../ESPEasyCore/ESPEasyRules.h"

#include "../Globals/ESPEasy_Scheduler.h"

#include "../Helpers/ESPEasy_time_calc.h"
#include "../Helpers/Misc.h"
#include "../Helpers/Scheduler.h"

const __FlashStringHelper * command_setRulesTimer(int msecFromNow, int timerIndex, int recurringCount, bool startImmediately = false) {
  if (msecFromNow < 0)
  {
    addLog(LOG_LEVEL_ERROR, F("TIMER: time must be positive"));
  } else {
    // start new timer when msecFromNow > 0
    // Clear timer when msecFromNow == 0
    if (Scheduler.setRulesTimer(msecFromNow, timerIndex, recurringCount, startImmediately))
    { 
      return return_command_success_flashstr();
    }
  }
  return return_command_failed_flashstr();
}

const __FlashStringHelper * Command_Timer_Set(struct EventStruct *event, const char *Line)
{
  return command_setRulesTimer(
    event->Par2 * 1000, // msec from now
    event->Par1,        // timer index
    0                   // recurringCount
    );
}

const __FlashStringHelper * Command_Timer_Set_ms (struct EventStruct *event, const char* Line)
{
  return command_setRulesTimer(
    event->Par2, // interval
    event->Par1, // timer index
    0            // recurringCount
    );
}

const __FlashStringHelper * Command_Loop_Timer_Set (struct EventStruct *event, const char* Line)
{
  int recurringCount = event->Par3;
  if (recurringCount == 0) {
    // if the optional 3rd parameter is not given, set it to "run always"
    recurringCount = -1;
  }
  return command_setRulesTimer(
    event->Par2 * 1000, // msec from now
    event->Par1,        // timer index
    recurringCount
    );
}

const __FlashStringHelper * Command_Loop_Timer_Set_ms (struct EventStruct *event, const char* Line)
{
  int recurringCount = event->Par3;
  if (recurringCount == 0) {
    // if the optional 3rd parameter is not given, set it to "run always"
    recurringCount = -1;
  }
  return command_setRulesTimer(
    event->Par2, // interval
    event->Par1, // timer index
    recurringCount
    );
}

const __FlashStringHelper * Command_Loop_Timer_SetAndRun (struct EventStruct *event, const char* Line)
{
  int recurringCount = event->Par3;
  if (recurringCount == 0) {
    // if the optional 3rd parameter is not given, set it to "run always"
    recurringCount = -1;
  }
  return command_setRulesTimer(
    event->Par2 * 1000, // msec from now
    event->Par1,        // timer index
    recurringCount,
    true);
}

const __FlashStringHelper * Command_Loop_Timer_SetAndRun_ms (struct EventStruct *event, const char* Line)
{
  int recurringCount = event->Par3;
  if (recurringCount == 0) {
    // if the optional 3rd parameter is not given, set it to "run always"
    recurringCount = -1;
  }
  return command_setRulesTimer(
    event->Par2, // interval
    event->Par1, // timer index
    recurringCount,
    true);
}

const __FlashStringHelper * Command_Timer_Pause(struct EventStruct *event, const char *Line)
{
  if (Scheduler.pause_rules_timer(event->Par1)) {
    String eventName = F("Rules#TimerPause=");
    eventName += event->Par1;
    rulesProcessing(eventName); // TD-er: Process right now
    return return_command_success_flashstr();
  }
  return return_command_failed_flashstr();
}

const __FlashStringHelper * Command_Timer_Resume(struct EventStruct *event, const char *Line)
{
  if (Scheduler.resume_rules_timer(event->Par1)) {
    String eventName = F("Rules#TimerResume=");
    eventName += event->Par1;
    rulesProcessing(eventName); // TD-er: Process right now
    return return_command_success_flashstr();
  }
  return return_command_failed_flashstr();
}

const __FlashStringHelper * Command_Delay(struct EventStruct *event, const char *Line)
{
  delayBackground(event->Par1);
  return return_command_success_flashstr();
}
