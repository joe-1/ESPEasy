#define _HEAD false
#define _TAIL true
#define CHUNKED_BUFFER_SIZE          400


#include "src/Globals/Device.h"
#include "src/Static/WebStaticData.h"

// ********************************************************************************
// Core part of WebServer, the chunked streaming buffer
// This must remain in the WebServer.ino file at the top.
// ********************************************************************************
void sendContentBlocking(String& data);
void sendHeaderBlocking(bool          json,
                        const String& origin = "");

class StreamingBuffer {
private:

  bool lowMemorySkip;

public:

  uint32_t initialRam;
  uint32_t beforeTXRam;
  uint32_t duringTXRam;
  uint32_t finalRam;
  uint32_t maxCoreUsage;
  uint32_t maxServerUsage;
  unsigned int sentBytes;
  uint32_t flashStringCalls;
  uint32_t flashStringData;

private:

  String buf;

public:

  StreamingBuffer(void) : lowMemorySkip(false),
    initialRam(0), beforeTXRam(0), duringTXRam(0), finalRam(0), maxCoreUsage(0),
    maxServerUsage(0), sentBytes(0), flashStringCalls(0), flashStringData(0)
  {
    buf.reserve(CHUNKED_BUFFER_SIZE + 50);
    buf = "";
  }

  StreamingBuffer operator=(String& a)                 {
    flush(); return addString(a);
  }

  StreamingBuffer operator=(const String& a)           {
    flush(); return addString(a);
  }

  StreamingBuffer operator+=(char a)                   {
    return addString(String(a));
  }

  StreamingBuffer operator+=(long unsigned int a)     {
    return addString(String(a));
  }

  StreamingBuffer operator+=(float a)                  {
    return addString(String(a));
  }

  StreamingBuffer operator+=(int a)                    {
    return addString(String(a));
  }

  StreamingBuffer operator+=(uint32_t a)               {
    return addString(String(a));
  }

  StreamingBuffer operator+=(const String& a)          {
    return addString(a);
  }

  StreamingBuffer operator+=(PGM_P str) {
    ++flashStringCalls;

    if (!str) { return *this; // return if the pointer is void
    }

    if (lowMemorySkip) { return *this; }
    int flush_step = CHUNKED_BUFFER_SIZE - this->buf.length();

    if (flush_step < 1) { flush_step = 0; }
    unsigned int pos          = 0;
    const unsigned int length = strlen_P((PGM_P)str);

    if (length == 0) { return *this; }
    flashStringData += length;

    while (pos < length) {
      if (flush_step == 0) {
        sendContentBlocking(this->buf);
        flush_step = CHUNKED_BUFFER_SIZE;
      }
      this->buf += (char)pgm_read_byte(&str[pos]);
      ++pos;
      --flush_step;
    }
    checkFull();
    return *this;
  }

  StreamingBuffer addString(const String& a) {
    if (lowMemorySkip) { return *this; }
    int flush_step = CHUNKED_BUFFER_SIZE - this->buf.length();

    if (flush_step < 1) { flush_step = 0; }
    int pos          = 0;
    const int length = a.length();

    while (pos < length) {
      if (flush_step == 0) {
        sendContentBlocking(this->buf);
        flush_step = CHUNKED_BUFFER_SIZE;
      }
      this->buf += a[pos];
      ++pos;
      --flush_step;
    }
    checkFull();
    return *this;
  }

  void flush() {
    if (lowMemorySkip) {
      this->buf = "";
    } else {
      sendContentBlocking(this->buf);
    }
  }

  void checkFull(void) {
    if (lowMemorySkip) { this->buf = ""; }

    if (this->buf.length() > CHUNKED_BUFFER_SIZE) {
      trackTotalMem();
      sendContentBlocking(this->buf);
    }
  }

  void startStream() {
    startStream(false, "");
  }

  void startStream(const String& origin) {
    startStream(false, origin);
  }

  void startJsonStream() {
    startStream(true, "*");
  }

private:

  void startStream(bool json, const String& origin) {
    maxCoreUsage = maxServerUsage = 0;
    initialRam   = ESP.getFreeHeap();
    beforeTXRam  = initialRam;
    sentBytes    = 0;
    buf          = "";

    if (beforeTXRam < 3000) {
      lowMemorySkip = true;
      WebServer.send(200, "text/plain", "Low memory. Cannot display webpage :-(");
       #if defined(ESP8266)
      tcpCleanup();
       #endif // if defined(ESP8266)
      return;
    } else {
      sendHeaderBlocking(json, origin);
    }
  }

  void trackTotalMem() {
    beforeTXRam = ESP.getFreeHeap();

    if ((initialRam - beforeTXRam) > maxServerUsage) {
      maxServerUsage = initialRam - beforeTXRam;
    }
  }

public:

  void trackCoreMem() {
    duringTXRam = ESP.getFreeHeap();

    if ((initialRam - duringTXRam) > maxCoreUsage) {
      maxCoreUsage = (initialRam - duringTXRam);
    }
  }

  void endStream(void) {
    if (!lowMemorySkip) {
      if (buf.length() > 0) { sendContentBlocking(buf); }
      buf = "";
      sendContentBlocking(buf);
      finalRam = ESP.getFreeHeap();

      /*
         if (loglevelActiveFor(LOG_LEVEL_DEBUG)) {
         String log = String("Ram usage: Webserver only: ") + maxServerUsage +
                     " including Core: " + maxCoreUsage +
                     " flashStringCalls: " + flashStringCalls +
                     " flashStringData: " + flashStringData;
         addLog(LOG_LEVEL_DEBUG, log);
         }
       */
    } else {
      addLog(LOG_LEVEL_ERROR, String("Webpage skipped: low memory: ") + finalRam);
      lowMemorySkip = false;
    }
  }
} TXBuffer;

void sendContentBlocking(String& data) {
  checkRAM(F("sendContentBlocking"));
  uint32_t freeBeforeSend = ESP.getFreeHeap();
  const uint32_t length   = data.length();
#ifndef BUILD_NO_DEBUG
  addLog(LOG_LEVEL_DEBUG_DEV, String("sendcontent free: ") + freeBeforeSend + " chunk size:" + length);
#endif // ifndef BUILD_NO_DEBUG
  freeBeforeSend = ESP.getFreeHeap();

  if (TXBuffer.beforeTXRam > freeBeforeSend) {
    TXBuffer.beforeTXRam = freeBeforeSend;
  }
  TXBuffer.duringTXRam = freeBeforeSend;
#if defined(ESP8266) && defined(ARDUINO_ESP8266_RELEASE_2_3_0)
  String size = formatToHex(length) + "\r\n";

  // do chunked transfer encoding ourselves (WebServer doesn't support it)
  WebServer.sendContent(size);

  if (length > 0) { WebServer.sendContent(data); }
  WebServer.sendContent("\r\n");
#else // ESP8266 2.4.0rc2 and higher and the ESP32 webserver supports chunked http transfer
  unsigned int timeout = 0;

  if (freeBeforeSend < 5000) { timeout = 100; }

  if (freeBeforeSend < 4000) { timeout = 1000; }
  const uint32_t beginWait = millis();
  WebServer.sendContent(data);

  while ((ESP.getFreeHeap() < freeBeforeSend) &&
         !timeOutReached(beginWait + timeout)) {
    if (ESP.getFreeHeap() < TXBuffer.duringTXRam) {
      TXBuffer.duringTXRam = ESP.getFreeHeap();
    }
    TXBuffer.trackCoreMem();
    checkRAM(F("duringDataTX"));
    delay(1);
  }
#endif // if defined(ESP8266) && defined(ARDUINO_ESP8266_RELEASE_2_3_0)

  TXBuffer.sentBytes += length;
  data                = "";
  delay(0);
}

void sendHeaderBlocking(bool json, const String& origin) {
  checkRAM(F("sendHeaderBlocking"));
  WebServer.client().flush();
  String contenttype;

  if (json) {
    contenttype = F("application/json");
  }
  else {
    contenttype = F("text/html");
  }

#if defined(ESP8266) && defined(ARDUINO_ESP8266_RELEASE_2_3_0)
  WebServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  WebServer.sendHeader(F("Accept-Ranges"),     F("none"));
  WebServer.sendHeader(F("Cache-Control"),     F("no-cache"));
  WebServer.sendHeader(F("Transfer-Encoding"), F("chunked"));

  if (json) {
    WebServer.sendHeader(F("Access-Control-Allow-Origin"), "*");
  }
  WebServer.send(200, contenttype, "");
#else // if defined(ESP8266) && defined(ARDUINO_ESP8266_RELEASE_2_3_0)
  unsigned int timeout        = 0;
  uint32_t     freeBeforeSend = ESP.getFreeHeap();

  if (freeBeforeSend < 5000) { timeout = 100; }

  if (freeBeforeSend < 4000) { timeout = 1000; }
  const uint32_t beginWait = millis();
  WebServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  WebServer.sendHeader(F("Cache-Control"), F("no-cache"));

  if (origin.length() > 0) {
    WebServer.sendHeader(F("Access-Control-Allow-Origin"), origin);
  }
  WebServer.send(200, contenttype, "");

  // dont wait on 2.3.0. Memory returns just too slow.
  while ((ESP.getFreeHeap() < freeBeforeSend) &&
         !timeOutReached(beginWait + timeout)) {
    checkRAM(F("duringHeaderTX"));
    delay(1);
  }
#endif // if defined(ESP8266) && defined(ARDUINO_ESP8266_RELEASE_2_3_0)
  delay(0);
}

void sendHeadandTail(const String& tmplName, boolean Tail = false, boolean rebooting = false) {
  String pageTemplate = "";
  String fileName     = tmplName;

  fileName += F(".htm");
  fs::File f = tryOpenFile(fileName, "r");

  if (f) {
    pageTemplate.reserve(f.size());

    while (f.available()) { pageTemplate += (char)f.read(); }
    f.close();
  } else {
    // TODO TD-er: Should send data directly to TXBuffer instead of using large strings.
    getWebPageTemplateDefault(tmplName, pageTemplate);
  }
  checkRAM(F("sendWebPage"));

  // web activity timer
  lastWeb = millis();

  if (Tail) {
    TXBuffer += pageTemplate.substring(
      11 +                                     // Size of "{{content}}"
      pageTemplate.indexOf(F("{{content}}"))); // advance beyond content key
  } else {
    int indexStart = 0;
    int indexEnd   = 0;
    int readPos    = 0; // Position of data sent to TXBuffer
    String varName;     // , varValue;
    String meta;

    if (rebooting) {
      meta = F("<meta http-equiv='refresh' content='10 url=/'>");
    }

    while ((indexStart = pageTemplate.indexOf(F("{{"), indexStart)) >= 0) {
      TXBuffer += pageTemplate.substring(readPos, indexStart);
      readPos   = indexStart;

      if ((indexEnd = pageTemplate.indexOf(F("}}"), indexStart)) > 0) {
        varName    = pageTemplate.substring(indexStart + 2, indexEnd);
        indexStart = indexEnd + 2;
        readPos    = indexEnd + 2;
        varName.toLowerCase();

        if (varName == F("content")) { // is var == page content?
          break;                       // send first part of result only
        } else if (varName == F("error")) {
          getErrorNotifications();
        }
        else if (varName == F("meta")) {
          TXBuffer += meta;
        }
        else {
          getWebPageTemplateVar(varName);
        }
      } else { // no closing "}}"
        // eat "{{"
        readPos    += 2;
        indexStart += 2;
      }
    }
  }

  if (shouldReboot) {
    // we only add this here as a seperate chunk to prevent using too much memory at once
    html_add_script(false);
    TXBuffer += DATA_REBOOT_JS;
    html_add_script_end();
  }
}

void sendHeadandTail_stdtemplate(boolean Tail = false, boolean rebooting = false) {
  sendHeadandTail(F("TmplStd"), Tail, rebooting);

  if (!Tail) {
    if (!clientIPinSubnet() && WifiIsAP(WiFi.getMode()) && (WiFi.softAPgetStationNum() > 0)) {
      addHtmlError(F("Warning: Connected via AP"));
    }
  }
}

// ********************************************************************************
// Web Interface init
// ********************************************************************************
// #include "core_version.h"
#define HTML_SYMBOL_WARNING "&#9888;"
#define HTML_SYMBOL_INPUT   "&#8656;"
#define HTML_SYMBOL_OUTPUT  "&#8658;"
#define HTML_SYMBOL_I_O     "&#8660;"


# define TASKS_PER_PAGE TASKS_MAX

#define strncpy_webserver_arg(D, N) safe_strncpy(D, WebServer.arg(N).c_str(), sizeof(D));
#define update_whenset_FormItemInt(K, V) { int tmpVal; \
                                           if (getCheckWebserverArg_int(K, tmpVal)) V = tmpVal; }


void WebServerInit()
{
  if (webserver_init) { return; }
  webserver_init = true;

  // Prepare webserver pages
  WebServer.on("/",                 handle_root);
  WebServer.on(F("/advanced"),      handle_advanced);
  WebServer.on(F("/config"),        handle_config);
  WebServer.on(F("/control"),       handle_control);
  WebServer.on(F("/controllers"),   handle_controllers);
  WebServer.on(F("/devices"),       handle_devices);
  WebServer.on(F("/download"),      handle_download);


  WebServer.on(F("/dumpcache"),     handle_dumpcache);  // C016 specific entrie
  WebServer.on(F("/cache_json"),    handle_cache_json); // C016 specific entrie
  WebServer.on(F("/cache_csv"),     handle_cache_csv);  // C016 specific entrie


  WebServer.on(F("/factoryreset"),  handle_factoryreset);
  #ifdef USE_SETTINGS_ARCHIVE
  WebServer.on(F("/settingsarchive"), handle_settingsarchive);
  #endif
  WebServer.on(F("/favicon.ico"),   handle_favicon);
  WebServer.on(F("/filelist"),      handle_filelist);
  WebServer.on(F("/hardware"),      handle_hardware);
  WebServer.on(F("/i2cscanner"),    handle_i2cscanner);
  WebServer.on(F("/json"),          handle_json);     // Also part of WEBSERVER_NEW_UI
  WebServer.on(F("/log"),           handle_log);
  WebServer.on(F("/login"),         handle_login);
  WebServer.on(F("/logjson"),       handle_log_JSON); // Also part of WEBSERVER_NEW_UI
#ifndef NOTIFIER_SET_NONE
  WebServer.on(F("/notifications"), handle_notifications);
#endif // ifndef NOTIFIER_SET_NONE
  WebServer.on(F("/pinstates"),     handle_pinstates);
  WebServer.on(F("/rules"),         handle_rules_new);
  WebServer.on(F("/rules/"),        Goto_Rules_Root);
  WebServer.on(F("/rules/add"),     []()
  {
    handle_rules_edit(WebServer.uri(), true);
  });
  WebServer.on(F("/rules/backup"),      handle_rules_backup);
  WebServer.on(F("/rules/delete"),      handle_rules_delete);
#ifdef FEATURE_SD
  WebServer.on(F("/SDfilelist"),        handle_SDfilelist);
#endif // ifdef FEATURE_SD
  WebServer.on(F("/setup"),             handle_setup);
  WebServer.on(F("/sysinfo"),           handle_sysinfo);
#ifdef WEBSERVER_SYSVARS
  WebServer.on(F("/sysvars"),           handle_sysvars);
#endif // WEBSERVER_SYSVARS
#ifdef WEBSERVER_TIMINGSTATS
  WebServer.on(F("/timingstats"),       handle_timingstats);
#endif // WEBSERVER_TIMINGSTATS
  WebServer.on(F("/tools"),             handle_tools);
  WebServer.on(F("/upload"),            HTTP_GET,  handle_upload);
  WebServer.on(F("/upload"),            HTTP_POST, handle_upload_post, handleFileUpload);
  WebServer.on(F("/wifiscanner"),       handle_wifiscanner);

#ifdef WEBSERVER_NEW_UI
  WebServer.on(F("/factoryreset_json"), handle_factoryreset_json);
  WebServer.on(F("/filelist_json"),     handle_filelist_json);
  WebServer.on(F("/i2cscanner_json"),   handle_i2cscanner_json);
  WebServer.on(F("/node_list_json"),    handle_nodes_list_json);
  WebServer.on(F("/pinstates_json"),    handle_pinstates_json);
  WebServer.on(F("/sysinfo_json"),      handle_sysinfo_json);
  WebServer.on(F("/timingstats_json"),  handle_timingstats_json);
  WebServer.on(F("/upload_json"),       HTTP_POST, handle_upload_json, handleFileUpload);
  WebServer.on(F("/wifiscanner_json"),  handle_wifiscanner_json);
#endif // WEBSERVER_NEW_UI

  WebServer.onNotFound(handleNotFound);

  #if defined(ESP8266)
  {
    uint32_t maxSketchSize;
    bool     use2step;

    if (OTA_possible(maxSketchSize, use2step)) {
      httpUpdater.setup(&WebServer);
    }
  }
  #endif // if defined(ESP8266)

  #if defined(ESP8266)

  if (Settings.UseSSDP)
  {
    WebServer.on(F("/ssdp.xml"), HTTP_GET, []() {
      WiFiClient client(WebServer.client());
      client.setTimeout(CONTROLLER_CLIENTTIMEOUT_DFLT);
      SSDP_schema(client);
    });
    SSDP_begin();
  }
  #endif // if defined(ESP8266)
}

void setWebserverRunning(bool state) {
  if (webserverRunning == state) {
    return;
  }

  if (state) {
    WebServerInit();
    WebServer.begin();
    addLog(LOG_LEVEL_INFO, F("Webserver: start"));
  } else {
    WebServer.stop();
    addLog(LOG_LEVEL_INFO, F("Webserver: stop"));
  }
  webserverRunning = state;
}

void getWebPageTemplateDefault(const String& tmplName, String& tmpl)
{
  tmpl.reserve(576);
  const bool addJS   = true;
  const bool addMeta = true;

  if (tmplName == F("TmplAP"))
  {
    getWebPageTemplateDefaultHead(tmpl, !addMeta, !addJS);
    tmpl += F("<body>"
              "<header class='apheader'>"
              "<h1>Welcome to ESP Easy Mega AP</h1>"
              "</header>");
    getWebPageTemplateDefaultContentSection(tmpl);
    getWebPageTemplateDefaultFooter(tmpl);
  }
  else if (tmplName == F("TmplMsg"))
  {
    getWebPageTemplateDefaultHead(tmpl, !addMeta, !addJS);
    tmpl += F("<body>");
    getWebPageTemplateDefaultHeader(tmpl, F("{{name}}"), false);
    getWebPageTemplateDefaultContentSection(tmpl);
    getWebPageTemplateDefaultFooter(tmpl);
  }
  else if (tmplName == F("TmplDsh"))
  {
    getWebPageTemplateDefaultHead(tmpl, !addMeta, addJS);
    tmpl += F(
      "<body>"
      "{{content}}"
      "</body></html>"
      );
  }
  else // all other template names e.g. TmplStd
  {
    getWebPageTemplateDefaultHead(tmpl, addMeta, addJS);
    tmpl += F("<body class='bodymenu'>"
              "<span class='message' id='rbtmsg'></span>");
    getWebPageTemplateDefaultHeader(tmpl, F("{{name}} {{logo}}"), true);
    getWebPageTemplateDefaultContentSection(tmpl);
    getWebPageTemplateDefaultFooter(tmpl);
  }
}

void getWebPageTemplateDefaultHead(String& tmpl, bool addMeta, bool addJS) {
  tmpl += F("<!DOCTYPE html><html lang='en'>"
            "<head>"
            "<meta charset='utf-8'/>"
            "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
            "<title>{{name}}</title>");

  if (addMeta) { tmpl += F("{{meta}}"); }

  if (addJS) { tmpl += F("{{js}}"); }

  tmpl += F("{{css}}"
            "</head>");
}

void getWebPageTemplateDefaultHeader(String& tmpl, const String& title, bool addMenu) {
  tmpl += F("<header class='headermenu'>"
            "<h1>ESP Easy Mega: ");
  tmpl += title;
  tmpl += F("</h1><BR>");

  if (addMenu) { tmpl += F("{{menu}}"); }
  tmpl += F("</header>");
}

void getWebPageTemplateDefaultContentSection(String& tmpl) {
  tmpl += F("<section>"
            "<span class='message error'>"
            "{{error}}"
            "</span>"
            "{{content}}"
            "</section>"
            );
}

void getWebPageTemplateDefaultFooter(String& tmpl) {
  tmpl += F("<footer>"
            "<br>"
            "<h6>Powered by <a href='http://www.letscontrolit.com' style='font-size: 15px; text-decoration: none'>Let's Control It</a> community</h6>"
            "</footer>"
            "</body></html>"
            );
}

void getErrorNotifications() {
  // Check number of MQTT controllers active.
  int nrMQTTenabled = 0;

  for (byte x = 0; x < CONTROLLER_MAX; x++) {
    if (Settings.Protocol[x] != 0) {
      byte ProtocolIndex = getProtocolIndex(Settings.Protocol[x]);

      if (Settings.ControllerEnabled[x] && Protocol[ProtocolIndex].usesMQTT) {
        ++nrMQTTenabled;
      }
    }
  }

  if (nrMQTTenabled > 1) {
    // Add warning, only one MQTT protocol should be used.
    addHtmlError(F("Only one MQTT controller should be active."));
  }

  // Check checksum of stored settings.
}

#define MENU_INDEX_MAIN          0
#define MENU_INDEX_CONFIG        1
#define MENU_INDEX_CONTROLLERS   2
#define MENU_INDEX_HARDWARE      3
#define MENU_INDEX_DEVICES       4
#define MENU_INDEX_RULES         5
#define MENU_INDEX_NOTIFICATIONS 6
#define MENU_INDEX_TOOLS         7
static byte navMenuIndex = MENU_INDEX_MAIN;


void getWebPageTemplateVar(const String& varName)
{
  // serialPrint(varName); serialPrint(" : free: "); serialPrint(ESP.getFreeHeap());   serialPrint("var len before:  "); serialPrint
  // (varValue.length()) ;serialPrint("after:  ");
  // varValue = "";

  if (varName == F("name"))
  {
    TXBuffer += Settings.Name;
  }

  else if (varName == F("unit"))
  {
    TXBuffer += String(Settings.Unit);
  }

  else if (varName == F("menu"))
  {
    static const __FlashStringHelper *gpMenu[8][3] = {
      // See https://github.com/letscontrolit/ESPEasy/issues/1650
      // Icon,        Full width label,   URL
      F("&#8962;"),   F("Main"),          F("/"),              // 0
      F("&#9881;"),   F("Config"),        F("/config"),        // 1
      F("&#128172;"), F("Controllers"),   F("/controllers"),   // 2
      F("&#128204;"), F("Hardware"),      F("/hardware"),      // 3
      F("&#128268;"), F("Devices"),       F("/devices"),       // 4
      F("&#10740;"),  F("Rules"),         F("/rules"),         // 5
      F("&#9993;"),   F("Notifications"), F("/notifications"), // 6
      F("&#128295;"), F("Tools"),         F("/tools"),         // 7
    };

    TXBuffer += F("<div class='menubar'>");

    for (byte i = 0; i < 8; i++)
    {
      if ((i == MENU_INDEX_RULES) && !Settings.UseRules) { // hide rules menu item
        continue;
      }
#ifdef NOTIFIER_SET_NONE

      if (i == MENU_INDEX_NOTIFICATIONS) { // hide notifications menu item
        continue;
      }
#endif // ifdef NOTIFIER_SET_NONE

      TXBuffer += F("<a class='menu");

      if (i == navMenuIndex) {
        TXBuffer += F(" active");
      }
      TXBuffer += F("' href='");
      TXBuffer += gpMenu[i][2];
      TXBuffer += "'>";
      TXBuffer += gpMenu[i][0];
      TXBuffer += F("<span class='showmenulabel'>");
      TXBuffer += gpMenu[i][1];
      TXBuffer += F("</span>");
      TXBuffer += F("</a>");
    }

    TXBuffer += F("</div>");
  }

  else if (varName == F("logo"))
  {
    if (SPIFFS.exists(F("esp.png")))
    {
      TXBuffer = F("<img src=\"esp.png\" width=48 height=48 align=right>");
    }
  }

  else if (varName == F("css"))
  {
    if (SPIFFS.exists(F("esp.css"))) // now css is written in writeDefaultCSS() to SPIFFS and always present
    // if (0) //TODO
    {
      TXBuffer = F("<link rel=\"stylesheet\" type=\"text/css\" href=\"esp.css\">");
    }
    else
    {
      TXBuffer += F("<style>");

      // Send CSS per chunk to avoid sending either too short or too large strings.
      TXBuffer += DATA_ESPEASY_DEFAULT_MIN_CSS;
      TXBuffer += F("</style>");
    }
  }


  else if (varName == F("js"))
  {
    html_add_autosubmit_form();
  }

  else if (varName == F("error"))
  {
    // print last error - not implemented yet
  }

  else if (varName == F("debug"))
  {
    // print debug messages - not implemented yet
  }

  else
  {
    if (loglevelActiveFor(LOG_LEVEL_ERROR)) {
      String log = F("Templ: Unknown Var : ");
      log += varName;
      addLog(LOG_LEVEL_ERROR, log);
    }

    // no return string - eat var name
  }
}

void writeDefaultCSS(void)
{
  return; // TODO

  if (!SPIFFS.exists(F("esp.css")))
  {
    String defaultCSS;

    fs::File f = tryOpenFile(F("esp.css"), "w");

    if (f)
    {
      if (loglevelActiveFor(LOG_LEVEL_INFO)) {
        String log = F("CSS  : Writing default CSS file to SPIFFS (");
        log += defaultCSS.length();
        log += F(" bytes)");
        addLog(LOG_LEVEL_INFO, log);
      }
      defaultCSS = PGMT(DATA_ESPEASY_DEFAULT_MIN_CSS);
      f.write((const unsigned char *)defaultCSS.c_str(), defaultCSS.length()); // note: content must be in RAM - a write of F("XXX") does
                                                                               // not work
      f.close();
    }
  }
}

int8_t level     = 0;
int8_t lastLevel = -1;

void json_quote_name(const String& val) {
  if (lastLevel == level) { TXBuffer += ","; }

  if (val.length() > 0) {
    TXBuffer += '\"';
    TXBuffer += val;
    TXBuffer += '\"';
    TXBuffer += ':';
  }
}

void json_quote_val(const String& val) {
  TXBuffer += '\"';
  TXBuffer += val;
  TXBuffer += '\"';
}

void json_open(bool arr = false, const String& name = String()) {
  json_quote_name(name);
  TXBuffer += arr ? "[" : "{";
  lastLevel = level;
  level++;
}

void json_init() {
  level     = 0;
  lastLevel = -1;
}

void json_close(bool arr = false) {
  TXBuffer += arr ? "]" : "}";
  level--;
  lastLevel = level;
}

void json_number(const String& name, const String& value) {
  json_quote_name(name);
  json_quote_val(value);
  lastLevel = level;
}

void json_prop(const String& name, const String& value) {
  json_quote_name(name);
  json_quote_val(value);
  lastLevel = level;
}

// ********************************************************************************
// Add a task select dropdown list
// ********************************************************************************
void addTaskSelect(const String& name,  int choice)
{
  String deviceName;

  TXBuffer += F("<select id='selectwidth' name='");
  TXBuffer += name;
  TXBuffer += F("' onchange='return dept_onchange(frmselect)'>");

  for (byte x = 0; x < TASKS_MAX; x++)
  {
    deviceName = "";

    if (Settings.TaskDeviceNumber[x] != 0)
    {
      byte DeviceIndex = getDeviceIndex(Settings.TaskDeviceNumber[x]);

      if (Plugin_id[DeviceIndex] != 0) {
        deviceName = getPluginNameFromDeviceIndex(DeviceIndex);
      }
    }
    LoadTaskSettings(x);
    TXBuffer += F("<option value='");
    TXBuffer += x;
    TXBuffer += '\'';

    if (choice == x) {
      TXBuffer += F(" selected");
    }

    if (Settings.TaskDeviceNumber[x] == 0) {
      addDisabled();
    }
    TXBuffer += '>';
    TXBuffer += x + 1;
    TXBuffer += F(" - ");
    TXBuffer += deviceName;
    TXBuffer += F(" - ");
    TXBuffer += ExtraTaskSettings.TaskDeviceName;
    TXBuffer += F("</option>");
  }
}

// ********************************************************************************
// Add a Value select dropdown list, based on TaskIndex
// ********************************************************************************
void addTaskValueSelect(const String& name, int choice, byte TaskIndex)
{
  TXBuffer += F("<select id='selectwidth' name='");
  TXBuffer += name;
  TXBuffer += "'>";

  byte DeviceIndex = getDeviceIndex(Settings.TaskDeviceNumber[TaskIndex]);

  for (byte x = 0; x < Device[DeviceIndex].ValueCount; x++)
  {
    TXBuffer += F("<option value='");
    TXBuffer += x;
    TXBuffer += '\'';

    if (choice == x) {
      TXBuffer += F(" selected");
    }
<<<<<<< HEAD

    if (strcasecmp_P(sCommand.c_str(), PSTR("reboot")) == 0)
    {
      addLog(LOG_LEVEL_INFO, F("     : Rebooting..."));
      cmd_within_mainloop = CMD_REBOOT;
    }
   if (strcasecmp_P(sCommand.c_str(), PSTR("reset")) == 0)
    {
      addLog(LOG_LEVEL_INFO, F("     : factory reset..."));
      cmd_within_mainloop = CMD_REBOOT;
      TXBuffer += F("OK. Please wait > 1 min and connect to Acces point.<BR><BR>PW=configesp<BR>URL=<a href='http://192.168.4.1'>192.168.4.1</a>");
      TXBuffer.endStream();
      ExecuteCommand(VALUE_SOURCE_HTTP, sCommand.c_str());
    }

    TXBuffer += "OK";
    TXBuffer.endStream();

  }
}


//********************************************************************************
// Web Interface config page
//********************************************************************************
void handle_config() {

   checkRAM(F("handle_config"));
   if (!isLoggedIn()) return;

   navMenuIndex = MENU_INDEX_CONFIG;
   TXBuffer.startStream();
   sendHeadandTail_stdtemplate(_HEAD);

  if (timerAPoff)
    timerAPoff = millis() + 2000L;  //user has reached the main page - AP can be switched off in 2..3 sec


  String name = WebServer.arg(F("name"));
  //String password = WebServer.arg(F("password"));
  String iprangelow = WebServer.arg(F("iprangelow"));
  String iprangehigh = WebServer.arg(F("iprangehigh"));

  Settings.Delay = getFormItemInt(F("delay"), Settings.Delay);
  Settings.deepSleep = getFormItemInt(F("deepsleep"), Settings.deepSleep);
  String espip = WebServer.arg(F("espip"));
  String espgateway = WebServer.arg(F("espgateway"));
  String espsubnet = WebServer.arg(F("espsubnet"));
  String espdns = WebServer.arg(F("espdns"));
  Settings.Unit = getFormItemInt(F("unit"), Settings.Unit);
  //String apkey = WebServer.arg(F("apkey"));
  String ssid = WebServer.arg(F("ssid"));


  if (ssid[0] != 0)
  {
    if (strcmp(Settings.Name, name.c_str()) != 0) {
      addLog(LOG_LEVEL_INFO, F("Unit Name changed."));
      MQTTclient_should_reconnect = true;
    }
    // Unit name
    safe_strncpy(Settings.Name, name.c_str(), sizeof(Settings.Name));
    Settings.appendUnitToHostname(isFormItemChecked(F("appendunittohostname")));

    // Password
    copyFormPassword(F("password"), SecuritySettings.Password, sizeof(SecuritySettings.Password));

    // SSID 1
    safe_strncpy(SecuritySettings.WifiSSID, ssid.c_str(), sizeof(SecuritySettings.WifiSSID));
    copyFormPassword(F("key"), SecuritySettings.WifiKey, sizeof(SecuritySettings.WifiKey));

    // SSID 2
    strncpy_webserver_arg(SecuritySettings.WifiSSID2, F("ssid2"));
    copyFormPassword(F("key2"), SecuritySettings.WifiKey2, sizeof(SecuritySettings.WifiKey2));

    // Access point password.
    copyFormPassword(F("apkey"), SecuritySettings.WifiAPKey, sizeof(SecuritySettings.WifiAPKey));


    // TD-er Read access control from form.
    SecuritySettings.IPblockLevel = getFormItemInt(F("ipblocklevel"));
    switch (SecuritySettings.IPblockLevel) {
      case LOCAL_SUBNET_ALLOWED:
      {
        IPAddress low, high;
        getSubnetRange(low, high);
        for (byte i=0; i < 4; ++i) {
          SecuritySettings.AllowedIPrangeLow[i] = low[i];
          SecuritySettings.AllowedIPrangeHigh[i] = high[i];
        }
        break;
      }
      case ONLY_IP_RANGE_ALLOWED:
      case ALL_ALLOWED:
        // iprangelow.toCharArray(tmpString, 26);
        str2ip(iprangelow, SecuritySettings.AllowedIPrangeLow);
        // iprangehigh.toCharArray(tmpString, 26);
        str2ip(iprangehigh, SecuritySettings.AllowedIPrangeHigh);
        break;
    }

    Settings.deepSleepOnFail = isFormItemChecked(F("deepsleeponfail"));
    str2ip(espip, Settings.IP);
    str2ip(espgateway, Settings.Gateway);
    str2ip(espsubnet, Settings.Subnet);
    str2ip(espdns, Settings.DNS);
    addHtmlError(SaveSettings());
  }

  html_add_form();
  html_table_class_normal();

  addFormHeader(F("Main Settings"));

  Settings.Name[25] = 0;
  SecuritySettings.Password[25] = 0;
  addFormTextBox( F("Unit Name"), F("name"), Settings.Name, 25);
  addFormNumericBox( F("Unit Number"), F("unit"), Settings.Unit, 0, UNIT_NUMBER_MAX);
  addFormCheckBox(F("Append Unit Number to hostname"), F("appendunittohostname"), Settings.appendUnitToHostname());
  addFormPasswordBox(F("Admin Password"), F("password"), SecuritySettings.Password, 25);

  addFormSubHeader(F("Wifi Settings"));

  addFormTextBox( F("SSID"), F("ssid"), SecuritySettings.WifiSSID, 31);
  addFormPasswordBox(F("WPA Key"), F("key"), SecuritySettings.WifiKey, 63);
  addFormTextBox( F("Fallback SSID"), F("ssid2"), SecuritySettings.WifiSSID2, 31);
  addFormPasswordBox( F("Fallback WPA Key"), F("key2"), SecuritySettings.WifiKey2, 63);
  addFormSeparator(2);
  addFormPasswordBox(F("WPA AP Mode Key"), F("apkey"), SecuritySettings.WifiAPKey, 63);

  // TD-er add IP access box F("ipblocklevel")
  addFormSubHeader(F("Client IP filtering"));
  {
    IPAddress low, high;
    getIPallowedRange(low, high);
    byte iplow[4];
    byte iphigh[4];
    for (byte i = 0; i < 4; ++i) {
      iplow[i] = low[i];
      iphigh[i] = high[i];
    }
    addFormIPaccessControlSelect(F("Client IP block level"), F("ipblocklevel"), SecuritySettings.IPblockLevel);
    addFormIPBox(F("Access IP lower range"), F("iprangelow"), iplow);
    addFormIPBox(F("Access IP upper range"), F("iprangehigh"), iphigh);
  }

  addFormSubHeader(F("IP Settings"));

  addFormIPBox(F("ESP IP"), F("espip"), Settings.IP);
  addFormIPBox(F("ESP GW"), F("espgateway"), Settings.Gateway);
  addFormIPBox(F("ESP Subnet"), F("espsubnet"), Settings.Subnet);
  addFormIPBox(F("ESP DNS"), F("espdns"), Settings.DNS);
  addFormNote(F("Leave empty for DHCP"));


  addFormSubHeader(F("Sleep Mode"));

  addFormNumericBox( F("Sleep awake time"), F("deepsleep"), Settings.deepSleep, 0, 255);
  addUnit(F("sec"));
  addHelpButton(F("SleepMode"));
  addFormNote(F("0 = Sleep Disabled, else time awake from sleep"));

  addFormNumericBox( F("Sleep time"), F("delay"), Settings.Delay, 0, 4294);   //limited by hardware to ~1.2h
  addUnit(F("sec"));

  addFormCheckBox(F("Sleep on connection failure"), F("deepsleeponfail"), Settings.deepSleepOnFail);

  addFormSeparator(2);

  html_TR_TD();
  html_TD();
  addSubmitButton();
  html_end_table();
  html_end_form();

  sendHeadandTail_stdtemplate(_TAIL);
  TXBuffer.endStream();
}


//********************************************************************************
// Web Interface controller page
//********************************************************************************
void handle_controllers() {
  checkRAM(F("handle_controllers"));
  if (!isLoggedIn()) return;
  navMenuIndex = MENU_INDEX_CONTROLLERS;
  TXBuffer.startStream();
  sendHeadandTail_stdtemplate(_HEAD);

  struct EventStruct TempEvent;

  byte controllerindex = getFormItemInt(F("index"), 0);
  boolean controllerNotSet = controllerindex == 0;
  --controllerindex;

  String usedns = WebServer.arg(F("usedns"));
  String controllerip = WebServer.arg(F("controllerip"));
  const int controllerport = getFormItemInt(F("controllerport"), 0);
  const int protocol = getFormItemInt(F("protocol"), -1);
  const int minimumsendinterval = getFormItemInt(F("minimumsendinterval"), 100);
  const int maxqueuedepth = getFormItemInt(F("maxqueuedepth"), 10);
  const int maxretry = getFormItemInt(F("maxretry"), 10);
  const int clienttimeout = getFormItemInt(F("clienttimeout"), CONTROLLER_CLIENTTIMEOUT_DFLT);


  //submitted data
  if (protocol != -1 && !controllerNotSet)
  {
    MakeControllerSettings(ControllerSettings);
    //submitted changed protocol
    if (Settings.Protocol[controllerindex] != protocol)
    {

      Settings.Protocol[controllerindex] = protocol;

      //there is a protocol selected?
      if (Settings.Protocol[controllerindex]!=0)
      {
        //reset (some) default-settings
        byte ProtocolIndex = getProtocolIndex(Settings.Protocol[controllerindex]);
        ControllerSettings.Port = Protocol[ProtocolIndex].defaultPort;
        ControllerSettings.MinimalTimeBetweenMessages = CONTROLLER_DELAY_QUEUE_DELAY_DFLT;
        ControllerSettings.ClientTimeout = CONTROLLER_CLIENTTIMEOUT_DFLT;
//        ControllerSettings.MaxQueueDepth = 0;
        if (Protocol[ProtocolIndex].usesTemplate)
          CPlugin_ptr[ProtocolIndex](CPLUGIN_PROTOCOL_TEMPLATE, &TempEvent, dummyString);
        safe_strncpy(ControllerSettings.Subscribe, TempEvent.String1.c_str(), sizeof(ControllerSettings.Subscribe));
        safe_strncpy(ControllerSettings.Publish, TempEvent.String2.c_str(), sizeof(ControllerSettings.Publish));
        safe_strncpy(ControllerSettings.MQTTLwtTopic, TempEvent.String3.c_str(), sizeof(ControllerSettings.MQTTLwtTopic));
        safe_strncpy(ControllerSettings.LWTMessageConnect, TempEvent.String4.c_str(), sizeof(ControllerSettings.LWTMessageConnect));
        safe_strncpy(ControllerSettings.LWTMessageDisconnect, TempEvent.String5.c_str(), sizeof(ControllerSettings.LWTMessageDisconnect));
        TempEvent.String1 = "";
        TempEvent.String2 = "";
        TempEvent.String3 = "";
        TempEvent.String4 = "";
        TempEvent.String5 = "";
        //NOTE: do not enable controller by default, give user a change to enter sensible values first

        //not resetted to default (for convenience)
        //SecuritySettings.ControllerUser[controllerindex]
        //SecuritySettings.ControllerPassword[controllerindex]

        ClearCustomControllerSettings(controllerindex);
      }

    }

    //subitted same protocol
    else
    {
      //there is a protocol selected
      if (Settings.Protocol != 0)
      {
        //copy all settings to conroller settings struct
        byte ProtocolIndex = getProtocolIndex(Settings.Protocol[controllerindex]);
        TempEvent.ControllerIndex = controllerindex;
        TempEvent.ProtocolIndex = ProtocolIndex;
        CPlugin_ptr[ProtocolIndex](CPLUGIN_WEBFORM_SAVE, &TempEvent, dummyString);
        ControllerSettings.UseDNS = usedns.toInt();
        if (ControllerSettings.UseDNS)
        {
          strncpy_webserver_arg(ControllerSettings.HostName, F("controllerhostname"));
          IPAddress IP;
          resolveHostByName(ControllerSettings.HostName, IP);
          for (byte x = 0; x < 4; x++)
            ControllerSettings.IP[x] = IP[x];
        }
        //no protocol selected
        else
        {
          str2ip(controllerip, ControllerSettings.IP);
        }
        //copy settings to struct
        Settings.ControllerEnabled[controllerindex] = isFormItemChecked(F("controllerenabled"));
        ControllerSettings.Port = controllerport;
        strncpy_webserver_arg(SecuritySettings.ControllerUser[controllerindex], F("controlleruser"));
        //safe_strncpy(SecuritySettings.ControllerPassword[controllerindex], controllerpassword.c_str(), sizeof(SecuritySettings.ControllerPassword[0]));
        copyFormPassword(F("controllerpassword"), SecuritySettings.ControllerPassword[controllerindex], sizeof(SecuritySettings.ControllerPassword[0]));
        strncpy_webserver_arg(ControllerSettings.Subscribe, F("controllersubscribe"));
        strncpy_webserver_arg(ControllerSettings.Publish, F("controllerpublish"));
        strncpy_webserver_arg(ControllerSettings.MQTTLwtTopic, F("mqttlwttopic"));
        strncpy_webserver_arg(ControllerSettings.LWTMessageConnect, F("lwtmessageconnect"));
        strncpy_webserver_arg(ControllerSettings.LWTMessageDisconnect, F("lwtmessagedisconnect"));
        ControllerSettings.MinimalTimeBetweenMessages = minimumsendinterval;
        ControllerSettings.MaxQueueDepth = maxqueuedepth;
        ControllerSettings.MaxRetry = maxretry;
        ControllerSettings.DeleteOldest = getFormItemInt(F("deleteoldest"), ControllerSettings.DeleteOldest);
        ControllerSettings.MustCheckReply = getFormItemInt(F("mustcheckreply"), ControllerSettings.MustCheckReply);
        ControllerSettings.ClientTimeout = clienttimeout;


        CPlugin_ptr[ProtocolIndex](CPLUGIN_INIT, &TempEvent, dummyString);
      }
    }
    addHtmlError(SaveControllerSettings(controllerindex, ControllerSettings));
    addHtmlError(SaveSettings());
  }

  html_add_form();

  if (controllerNotSet)
  {
    html_table_class_multirow();
    html_TR();
    html_table_header("", 70);
    html_table_header("Nr", 50);
    html_table_header(F("Enabled"), 100);
    html_table_header(F("Protocol"));
    html_table_header("Host");
    html_table_header("Port");

    MakeControllerSettings(ControllerSettings);
    for (byte x = 0; x < CONTROLLER_MAX; x++)
    {
      LoadControllerSettings(x, ControllerSettings);
      html_TR_TD();
      html_add_button_prefix();
      TXBuffer += F("controllers?index=");
      TXBuffer += x + 1;
      TXBuffer += F("'>Edit</a>");
      html_TD();
      TXBuffer += getControllerSymbol(x);
      html_TD();
      if (Settings.Protocol[x] != 0)
      {
        addEnabled(Settings.ControllerEnabled[x]);

        html_TD();
        byte ProtocolIndex = getProtocolIndex(Settings.Protocol[x]);
        String ProtocolName = "";
        CPlugin_ptr[ProtocolIndex](CPLUGIN_GET_DEVICENAME, 0, ProtocolName);
        TXBuffer += ProtocolName;

        html_TD();
        TXBuffer += ControllerSettings.getHost();
        html_TD();
        TXBuffer += ControllerSettings.Port;
      }
      else {
        html_TD(3);
      }
    }
    html_end_table();
    html_end_form();
  }
  else
  {
    html_table_class_normal();
    addFormHeader(F("Controller Settings"));
    addRowLabel(F("Protocol"));
    byte choice = Settings.Protocol[controllerindex];
    addSelector_Head(F("protocol"), true);
    addSelector_Item(F("- Standalone -"), 0, false, false, "");
    for (byte x = 0; x <= protocolCount; x++)
    {
      String ProtocolName = "";
      CPlugin_ptr[x](CPLUGIN_GET_DEVICENAME, 0, ProtocolName);
      boolean disabled = false;// !((controllerindex == 0) || !Protocol[x].usesMQTT);
      addSelector_Item(ProtocolName,
                       Protocol[x].Number,
                       choice == Protocol[x].Number,
                       disabled,
                       "");
    }
    addSelector_Foot();

    addHelpButton(F("EasyProtocols"));
    if (Settings.Protocol[controllerindex])
    {
      MakeControllerSettings(ControllerSettings);
      LoadControllerSettings(controllerindex, ControllerSettings);
      byte choice = ControllerSettings.UseDNS;
      String options[2];
      options[0] = F("Use IP address");
      options[1] = F("Use Hostname");

      byte choice_delete_oldest = ControllerSettings.DeleteOldest;
      String options_delete_oldest[2];
      options_delete_oldest[0] = F("Ignore New");
      options_delete_oldest[1] = F("Delete Oldest");

      byte choice_mustcheckreply = ControllerSettings.MustCheckReply;
      String options_mustcheckreply[2];
      options_mustcheckreply[0] = F("Ignore Acknowledgement");
      options_mustcheckreply[1] = F("Check Acknowledgement");


      byte ProtocolIndex = getProtocolIndex(Settings.Protocol[controllerindex]);
      if (!Protocol[ProtocolIndex].Custom)
      {

        addFormSelector(F("Locate Controller"), F("usedns"), 2, options, NULL, NULL, choice, true);
        if (ControllerSettings.UseDNS)
        {
          addFormTextBox( F("Controller Hostname"), F("controllerhostname"), ControllerSettings.HostName, sizeof(ControllerSettings.HostName)-1);
        }
        else
        {
          addFormIPBox(F("Controller IP"), F("controllerip"), ControllerSettings.IP);
        }

        addFormNumericBox( F("Controller Port"), F("controllerport"), ControllerSettings.Port, 1, 65535);
        addFormNumericBox( F("Minimum Send Interval"), F("minimumsendinterval"), ControllerSettings.MinimalTimeBetweenMessages, 1, CONTROLLER_DELAY_QUEUE_DELAY_MAX);
        addUnit(F("ms"));
        addFormNumericBox( F("Max Queue Depth"), F("maxqueuedepth"), ControllerSettings.MaxQueueDepth, 1, CONTROLLER_DELAY_QUEUE_DEPTH_MAX);
        addFormNumericBox( F("Max Retries"), F("maxretry"), ControllerSettings.MaxRetry, 1, CONTROLLER_DELAY_QUEUE_RETRY_MAX);
        addFormSelector(F("Full Queue Action"), F("deleteoldest"), 2, options_delete_oldest, NULL, NULL, choice_delete_oldest, true);

        addFormSelector(F("Check Reply"), F("mustcheckreply"), 2, options_mustcheckreply, NULL, NULL, choice_mustcheckreply, true);
        addFormNumericBox( F("Client Timeout"), F("clienttimeout"), ControllerSettings.ClientTimeout, 10, CONTROLLER_CLIENTTIMEOUT_MAX);
        addUnit(F("ms"));

        if (Protocol[ProtocolIndex].usesAccount)
        {
          String protoDisplayName;
          if (!getControllerProtocolDisplayName(ProtocolIndex, CONTROLLER_USER, protoDisplayName)) {
            protoDisplayName = F("Controller User");
          }
          addFormTextBox(protoDisplayName, F("controlleruser"), SecuritySettings.ControllerUser[controllerindex], sizeof(SecuritySettings.ControllerUser[0])-1);
        }
        if (Protocol[ProtocolIndex].usesPassword)
        {
          String protoDisplayName;
          if (getControllerProtocolDisplayName(ProtocolIndex, CONTROLLER_PASS, protoDisplayName)) {
            // It is not a regular password, thus use normal text field.
            addFormTextBox(protoDisplayName, F("controllerpassword"), SecuritySettings.ControllerPassword[controllerindex], sizeof(SecuritySettings.ControllerPassword[0])-1);
          } else {
            addFormPasswordBox(F("Controller Password"), F("controllerpassword"), SecuritySettings.ControllerPassword[controllerindex], sizeof(SecuritySettings.ControllerPassword[0])-1);
          }
        }

        if (Protocol[ProtocolIndex].usesTemplate || Protocol[ProtocolIndex].usesMQTT)
        {
          String protoDisplayName;
          if (!getControllerProtocolDisplayName(ProtocolIndex, CONTROLLER_SUBSCRIBE, protoDisplayName)) {
            protoDisplayName = F("Controller Subscribe");
          }
          addFormTextBox(protoDisplayName, F("controllersubscribe"), ControllerSettings.Subscribe, sizeof(ControllerSettings.Subscribe)-1);
        }

        if (Protocol[ProtocolIndex].usesTemplate || Protocol[ProtocolIndex].usesMQTT)
        {
          String protoDisplayName;
          if (!getControllerProtocolDisplayName(ProtocolIndex, CONTROLLER_PUBLISH, protoDisplayName)) {
            protoDisplayName = F("Controller Publish");
          }
          addFormTextBox(protoDisplayName, F("controllerpublish"), ControllerSettings.Publish, sizeof(ControllerSettings.Publish)-1);
        }

        if (Protocol[ProtocolIndex].usesMQTT)
        {
          String protoDisplayName;
          if (!getControllerProtocolDisplayName(ProtocolIndex, CONTROLLER_LWT_TOPIC, protoDisplayName)) {
            protoDisplayName = F("Controller lwl topic");
          }
          addFormTextBox(protoDisplayName, F("mqttlwttopic"), ControllerSettings.MQTTLwtTopic, sizeof(ControllerSettings.MQTTLwtTopic)-1);
        }

        if (Protocol[ProtocolIndex].usesMQTT)
        {
          String protoDisplayName;
          if (!getControllerProtocolDisplayName(ProtocolIndex, CONTROLLER_LWT_CONNECT_MESSAGE, protoDisplayName)) {
            protoDisplayName = F("LWT Connect Message");
          }
          addFormTextBox(protoDisplayName, F("lwtmessageconnect"), ControllerSettings.LWTMessageConnect, sizeof(ControllerSettings.LWTMessageConnect)-1);
        }

        if (Protocol[ProtocolIndex].usesMQTT)
        {
          String protoDisplayName;
          if (!getControllerProtocolDisplayName(ProtocolIndex, CONTROLLER_LWT_DISCONNECT_MESSAGE, protoDisplayName)) {
            protoDisplayName = F("LWT Disconnect Message");
          }
          addFormTextBox(protoDisplayName, F("lwtmessagedisconnect"), ControllerSettings.LWTMessageDisconnect, sizeof(ControllerSettings.LWTMessageDisconnect)-1);
        }
      }

      addFormCheckBox(F("Enabled"), F("controllerenabled"), Settings.ControllerEnabled[controllerindex]);

      TempEvent.ControllerIndex = controllerindex;
      TempEvent.ProtocolIndex = ProtocolIndex;
      CPlugin_ptr[ProtocolIndex](CPLUGIN_WEBFORM_LOAD, &TempEvent,TXBuffer.buf);

    }

    addFormSeparator(2);
    html_TR_TD();
    html_TD();
    addButton(F("controllers"), F("Close"));
    addSubmitButton();
    html_end_table();
    html_end_form();
  }

  sendHeadandTail_stdtemplate(_TAIL);
  TXBuffer.endStream();
}

//********************************************************************************
// Web Interface notifcations page
//********************************************************************************
void handle_notifications() {
  checkRAM(F("handle_notifications"));
  if (!isLoggedIn()) return;
  navMenuIndex = MENU_INDEX_NOTIFICATIONS;
  TXBuffer.startStream();
  sendHeadandTail_stdtemplate(_HEAD);

  struct EventStruct TempEvent;
  // char tmpString[64];


  byte notificationindex = getFormItemInt(F("index"), 0);
  boolean notificationindexNotSet = notificationindex == 0;
  --notificationindex;

  const int notification = getFormItemInt(F("notification"), -1);

  if (notification != -1 && !notificationindexNotSet)
  {
    MakeNotificationSettings(NotificationSettings);
    if (Settings.Notification[notificationindex] != notification)
    {
      Settings.Notification[notificationindex] = notification;
    }
    else
    {
      if (Settings.Notification != 0)
      {
        byte NotificationProtocolIndex = getNotificationProtocolIndex(Settings.Notification[notificationindex]);
        if (NotificationProtocolIndex!=NPLUGIN_NOT_FOUND)
          NPlugin_ptr[NotificationProtocolIndex](NPLUGIN_WEBFORM_SAVE, 0, dummyString);
        NotificationSettings.Port = getFormItemInt(F("port"), 0);
        NotificationSettings.Pin1 = getFormItemInt(F("pin1"), 0);
        NotificationSettings.Pin2 = getFormItemInt(F("pin2"), 0);
        Settings.NotificationEnabled[notificationindex] = isFormItemChecked(F("notificationenabled"));
        strncpy_webserver_arg(NotificationSettings.Domain,   F("domain"));
        strncpy_webserver_arg(NotificationSettings.Server,   F("server"));
        strncpy_webserver_arg(NotificationSettings.Sender,   F("sender"));
        strncpy_webserver_arg(NotificationSettings.Receiver, F("receiver"));
        strncpy_webserver_arg(NotificationSettings.Subject,  F("subject"));
        strncpy_webserver_arg(NotificationSettings.User,     F("user"));
        strncpy_webserver_arg(NotificationSettings.Pass,     F("pass"));
        strncpy_webserver_arg(NotificationSettings.Body,     F("body"));

      }
    }
    // Save the settings.
    addHtmlError(SaveNotificationSettings(notificationindex, (byte*)&NotificationSettings, sizeof(NotificationSettingsStruct)));
    addHtmlError(SaveSettings());
    if (WebServer.hasArg(F("test"))) {
      // Perform tests with the settings in the form.
      byte NotificationProtocolIndex = getNotificationProtocolIndex(Settings.Notification[notificationindex]);
      if (NotificationProtocolIndex != NPLUGIN_NOT_FOUND)
      {
        // TempEvent.NotificationProtocolIndex = NotificationProtocolIndex;
        TempEvent.NotificationIndex = notificationindex;
        schedule_notification_event_timer(NotificationProtocolIndex, NPLUGIN_NOTIFY, &TempEvent);
      }
    }
  }

  html_add_form();

  if (notificationindexNotSet)
  {
    html_table_class_multirow();
    html_TR();
    html_table_header("", 70);
    html_table_header("Nr", 50);
    html_table_header(F("Enabled"), 100);
    html_table_header(F("Service"));
    html_table_header(F("Server"));
    html_table_header("Port");

    MakeNotificationSettings(NotificationSettings);
    for (byte x = 0; x < NOTIFICATION_MAX; x++)
    {
      LoadNotificationSettings(x, (byte*)&NotificationSettings, sizeof(NotificationSettingsStruct));
      html_TR_TD();
      html_add_button_prefix();
      TXBuffer += F("notifications?index=");
      TXBuffer += x + 1;
      TXBuffer += F("'>Edit</a>");
      html_TD();
      TXBuffer += x + 1;
      html_TD();
      if (Settings.Notification[x] != 0)
      {
        addEnabled(Settings.NotificationEnabled[x]);

        html_TD();
        byte NotificationProtocolIndex = getNotificationProtocolIndex(Settings.Notification[x]);
        String NotificationName = F("(plugin not found?)");
        if (NotificationProtocolIndex!=NPLUGIN_NOT_FOUND)
        {
          NPlugin_ptr[NotificationProtocolIndex](NPLUGIN_GET_DEVICENAME, 0, NotificationName);
        }
        TXBuffer += NotificationName;
        html_TD();
        TXBuffer += NotificationSettings.Server;
        html_TD();
        TXBuffer += NotificationSettings.Port;
      }
      else {
        html_TD(3);
      }
    }
    html_end_table();
    html_end_form();
  }
  else
  {
    html_table_class_normal();
    addFormHeader(F("Notification Settings"));
    addRowLabel(F("Notification"));
    byte choice = Settings.Notification[notificationindex];
    addSelector_Head(F("notification"), true);
    addSelector_Item(F("- None -"), 0, false, false, "");
    for (byte x = 0; x <= notificationCount; x++)
    {
      String NotificationName = "";
      NPlugin_ptr[x](NPLUGIN_GET_DEVICENAME, 0, NotificationName);
      addSelector_Item(NotificationName,
                       Notification[x].Number,
                       choice == Notification[x].Number,
                       false,
                       "");
    }
    addSelector_Foot();

    addHelpButton(F("EasyNotifications"));

    if (Settings.Notification[notificationindex])
    {
      MakeNotificationSettings(NotificationSettings);
      LoadNotificationSettings(notificationindex, (byte*)&NotificationSettings, sizeof(NotificationSettingsStruct));

      byte NotificationProtocolIndex = getNotificationProtocolIndex(Settings.Notification[notificationindex]);
      if (NotificationProtocolIndex!=NPLUGIN_NOT_FOUND)
      {

        if (Notification[NotificationProtocolIndex].usesMessaging)
        {
          addFormTextBox(F("Domain"), F("domain"), NotificationSettings.Domain, sizeof(NotificationSettings.Domain)-1);
          addFormTextBox(F("Server"), F("server"), NotificationSettings.Server, sizeof(NotificationSettings.Server)-1);
          addFormNumericBox(F("Port"), F("port"), NotificationSettings.Port, 1, 65535);

          addFormTextBox(F("Sender"), F("sender"), NotificationSettings.Sender, sizeof(NotificationSettings.Sender)-1);
          addFormTextBox(F("Receiver"), F("receiver"), NotificationSettings.Receiver, sizeof(NotificationSettings.Receiver)-1);
          addFormTextBox(F("Subject"), F("subject"), NotificationSettings.Subject, sizeof(NotificationSettings.Subject)-1);

          addFormTextBox(F("User"), F("user"), NotificationSettings.User, sizeof(NotificationSettings.User)-1);
          addFormTextBox(F("Pass"), F("pass"), NotificationSettings.Pass, sizeof(NotificationSettings.Pass)-1);

          addRowLabel(F("Body"));
          TXBuffer += F("<textarea name='body' rows='20' size=512 wrap='off'>");
          TXBuffer += NotificationSettings.Body;
          TXBuffer += F("</textarea>");
        }

        if (Notification[NotificationProtocolIndex].usesGPIO > 0)
        {
          addRowLabel(F("1st GPIO"));
          addPinSelect(false, "pin1", NotificationSettings.Pin1);
        }

        addRowLabel(F("Enabled"));
        addCheckBox(F("notificationenabled"), Settings.NotificationEnabled[notificationindex]);

        TempEvent.NotificationIndex = notificationindex;
        NPlugin_ptr[NotificationProtocolIndex](NPLUGIN_WEBFORM_LOAD, &TempEvent,TXBuffer.buf);
      }
    }

    addFormSeparator(2);

    html_TR_TD();
    html_TD();
    addButton(F("notifications"), F("Close"));
    addSubmitButton();
    addSubmitButton(F("Test"), F("test"));
    html_end_table();
    html_end_form();
  }
  sendHeadandTail_stdtemplate(_TAIL);
  TXBuffer.endStream();
}


//********************************************************************************
// Web Interface hardware page
//********************************************************************************
void handle_hardware() {
  checkRAM(F("handle_hardware"));
  if (!isLoggedIn()) return;
  navMenuIndex = MENU_INDEX_HARDWARE;
  TXBuffer.startStream();
  sendHeadandTail_stdtemplate(_HEAD);
  if (isFormItem(F("psda")))
  {
    Settings.Pin_status_led  = getFormItemInt(F("pled"));
    Settings.Pin_status_led_Inversed  = isFormItemChecked(F("pledi"));
    Settings.Pin_Reset  = getFormItemInt(F("pres"));
    Settings.Pin_i2c_sda     = getFormItemInt(F("psda"));
    Settings.Pin_i2c_scl     = getFormItemInt(F("pscl"));
    Settings.InitSPI = isFormItemChecked(F("initspi"));      // SPI Init
    Settings.Pin_sd_cs  = getFormItemInt(F("sd"));
    int gpio_pin = 0;
    // FIXME TD-er: Max of 17 is a limit in the Settings.PinBootStates array
    while (gpio_pin <= MAX_GPIO  && gpio_pin < 17) {
      if (Settings.UseSerial && (gpio_pin == 1 || gpio_pin == 3)) {
        // do not add the pin state select for these pins.
      } else {
        int nodemcu_pinnr = -1;
        bool input, output, warning;
        if (getGpioInfo(gpio_pin, nodemcu_pinnr, input, output, warning)) {
          String int_pinlabel = "p";
          int_pinlabel += gpio_pin;
          byte NewPinBootState = getFormItemInt(int_pinlabel);
          if (Settings.PinBootStates[gpio_pin] != NewPinBootState) {
            // Setting has changed, apply setting now.
            if (applyGpioPinBootState(gpio_pin, NewPinBootState)) {
              // Only store the settings when they are applicable.
              Settings.PinBootStates[gpio_pin] = NewPinBootState;
            }
          }
        }
      }
      ++gpio_pin;
    }
    addHtmlError(SaveSettings());
  }

  TXBuffer += F("<form  method='post'>");
  html_table_class_normal();
  addFormHeader(F("Hardware Settings"), F("ESPEasy#Hardware_page"));

  addFormSubHeader(F("Wifi Status LED"));
  addFormPinSelect(formatGpioName_output("LED"), "pled", Settings.Pin_status_led);
  addFormCheckBox(F("Inversed LED"), F("pledi"), Settings.Pin_status_led_Inversed);
  addFormNote(F("On most boards, use &rsquo;GPIO-2 (D4)&rsquo; with &rsquo;Inversed&rsquo; checked for onboard LED"));

  addFormSubHeader(F("Reset Pin"));
  addFormPinSelect(formatGpioName_input(F("Switch")), "pres", Settings.Pin_Reset);
  addFormNote(F("Press about 10s for factory reset"));

  addFormSubHeader(F("I2C Interface"));
  addFormPinSelectI2C(formatGpioName_bidirectional("SDA"), F("psda"), Settings.Pin_i2c_sda);
  addFormPinSelectI2C(formatGpioName_output("SCL"), F("pscl"), Settings.Pin_i2c_scl);

  // SPI Init
  addFormSubHeader(F("SPI Interface"));
  addFormCheckBox(F("Init SPI"), F("initspi"), Settings.InitSPI);
  addFormNote(F("CLK=GPIO-14 (D5), MISO=GPIO-12 (D6), MOSI=GPIO-13 (D7)"));
  addFormNote(F("Chip Select (CS) config must be done in the plugin"));
#ifdef FEATURE_SD
  addFormPinSelect(formatGpioName_output("SD Card CS"), "sd", Settings.Pin_sd_cs);
#endif

  addFormSubHeader(F("GPIO boot states"));
  int gpio_pin = 0;
  // FIXME TD-er: Max of 17 is a limit in the Settings.PinBootStates array
  while (gpio_pin <= MAX_GPIO  && gpio_pin < 17) {
    bool enabled = true;
    if (Settings.UseSerial && (gpio_pin == 1 || gpio_pin == 3)) {
      // do not add the pin state select for these pins.
      enabled = false;
    }
    int nodemcu_pinnr = -1;
    bool input, output, warning;
    if (getGpioInfo(gpio_pin, nodemcu_pinnr, input, output, warning)) {
      enabled = checkValidGpioBootStatePin(gpio_pin);
      String label;
      label.reserve(32);
      label = F("Pin mode ");
      label += createGPIO_label(gpio_pin, nodemcu_pinnr, input, output, warning);
      String int_pinlabel = "p";
      int_pinlabel += gpio_pin;
      // Add a GPIO pin select dropdown list
      addRowLabel(label);
      String options[4] = { F("Default (Input)"), F("Output Low"), F("Output High"), F("Input Pull-up") };
      addSelector(int_pinlabel, 4, options, NULL, NULL, Settings.PinBootStates[gpio_pin], false, enabled);
    }
    ++gpio_pin;
  }
  addFormSeparator(2);

  html_TR_TD();
  html_TD();
  addSubmitButton();
  html_TR_TD();
  html_end_table();
  html_end_form();

  sendHeadandTail_stdtemplate(_TAIL);
  TXBuffer.endStream();

}


//********************************************************************************
// Add a IP Access Control select dropdown list
//********************************************************************************
void addFormIPaccessControlSelect(const String& label, const String& id, int choice)
{
  addRowLabel(label);
  addIPaccessControlSelect(id, choice);
}

void addIPaccessControlSelect(const String& name, int choice)
{
  String options[3] = { F("Allow All"), F("Allow Local Subnet"), F("Allow IP range") };
  addSelector(name, 3, options, NULL, NULL, choice, false);
}




//********************************************************************************
// Web Interface device page
//********************************************************************************
//19480 (11128)

// change of device: cleanup old device and reset default settings
void setTaskDevice_to_TaskIndex(byte taskdevicenumber, byte taskIndex) {
  struct EventStruct TempEvent;
  TempEvent.TaskIndex = taskIndex;
  String dummy;

  //let the plugin do its cleanup by calling PLUGIN_EXIT with this TaskIndex
  PluginCall(PLUGIN_EXIT, &TempEvent, dummy);
  taskClear(taskIndex, false); // clear settings, but do not save
  ClearCustomTaskSettings(taskIndex);

  Settings.TaskDeviceNumber[taskIndex] = taskdevicenumber;
  if (taskdevicenumber != 0) // set default values if a new device has been selected
  {
    //NOTE: do not enable task by default. allow user to enter sensible valus first and let him enable it when ready.
    PluginCall(PLUGIN_GET_DEVICEVALUENAMES, &TempEvent, dummy); //the plugin should populate ExtraTaskSettings with its default values.
  } else {
    // New task is empty task, thus save config now.
    SaveTaskSettings(taskIndex);
    SaveSettings();
  }
}

void setBasicTaskValues(byte taskIndex, unsigned long taskdevicetimer,
                        bool enabled, const String& name, int pin1, int pin2, int pin3) {
    LoadTaskSettings(taskIndex); // Make sure ExtraTaskSettings are up-to-date
    byte DeviceIndex = getDeviceIndex(Settings.TaskDeviceNumber[taskIndex]);
    if (taskdevicetimer > 0) {
      Settings.TaskDeviceTimer[taskIndex] = taskdevicetimer;
    } else {
      if (!Device[DeviceIndex].TimerOptional) // Set default delay, unless it's optional...
        Settings.TaskDeviceTimer[taskIndex] = Settings.Delay;
      else
        Settings.TaskDeviceTimer[taskIndex] = 0;
    }
    Settings.TaskDeviceEnabled[taskIndex] = enabled;
    safe_strncpy(ExtraTaskSettings.TaskDeviceName, name.c_str(), sizeof(ExtraTaskSettings.TaskDeviceName));
    if (pin1 >= 0) Settings.TaskDevicePin1[taskIndex] = pin1;
    if (pin2 >= 0) Settings.TaskDevicePin2[taskIndex] = pin2;
    if (pin3 >= 0) Settings.TaskDevicePin3[taskIndex] = pin3;
    SaveTaskSettings(taskIndex);
}


void handle_devices() {
  checkRAM(F("handle_devices"));
  if (!isLoggedIn()) return;
  navMenuIndex = MENU_INDEX_DEVICES;
  TXBuffer.startStream();
  sendHeadandTail_stdtemplate(_HEAD);


  // char tmpString[41];
  struct EventStruct TempEvent;

  // String taskindex = WebServer.arg(F("index"));

  byte taskdevicenumber;
  if (WebServer.hasArg(F("del")))
    taskdevicenumber=0;
  else
    taskdevicenumber = getFormItemInt(F("TDNUM"), 0);


  unsigned long taskdevicetimer = getFormItemInt(F("TDT"),0);
  // String taskdeviceid[CONTROLLER_MAX];
  // String taskdevicepin1 = WebServer.arg(F("taskdevicepin1"));   // "taskdevicepin*" should not be changed because it is uses by plugins and expected to be saved by this code
  // String taskdevicepin2 = WebServer.arg(F("taskdevicepin2"));
  // String taskdevicepin3 = WebServer.arg(F("taskdevicepin3"));
  // String taskdevicepin1pullup = WebServer.arg(F("TDPPU"));
  // String taskdevicepin1inversed = WebServer.arg(F("TDPI"));
  // String taskdevicename = WebServer.arg(F("TDN"));
  // String taskdeviceport = WebServer.arg(F("TDP"));
  // String taskdeviceformula[VARS_PER_TASK];
  // String taskdevicevaluename[VARS_PER_TASK];
  // String taskdevicevaluedecimals[VARS_PER_TASK];
  // String taskdevicesenddata[CONTROLLER_MAX];
  // String taskdeviceglobalsync = WebServer.arg(F("TDGS"));
  // String taskdeviceenabled = WebServer.arg(F("TDE"));

  // for (byte varNr = 0; varNr < VARS_PER_TASK; varNr++)
  // {
  //   char argc[25];
  //   String arg = F("TDF");
  //   arg += varNr + 1;
  //   arg.toCharArray(argc, 25);
  //   taskdeviceformula[varNr] = WebServer.arg(argc);
  //
  //   arg = F("TDVN");
  //   arg += varNr + 1;
  //   arg.toCharArray(argc, 25);
  //   taskdevicevaluename[varNr] = WebServer.arg(argc);
  //
  //   arg = F("TDVD");
  //   arg += varNr + 1;
  //   arg.toCharArray(argc, 25);
  //   taskdevicevaluedecimals[varNr] = WebServer.arg(argc);
  // }

  // for (byte controllerNr = 0; controllerNr < CONTROLLER_MAX; controllerNr++)
  // {
  //   char argc[25];
  //   String arg = F("TDID");
  //   arg += controllerNr + 1;
  //   arg.toCharArray(argc, 25);
  //   taskdeviceid[controllerNr] = WebServer.arg(argc);
  //
  //   arg = F("TDSD");
  //   arg += controllerNr + 1;
  //   arg.toCharArray(argc, 25);
  //   taskdevicesenddata[controllerNr] = WebServer.arg(argc);
  // }

  byte page = getFormItemInt(F("page"), 0);
  if (page == 0)
    page = 1;
  byte setpage = getFormItemInt(F("setpage"), 0);
  if (setpage > 0)
  {
    if (setpage <= (TASKS_MAX / TASKS_PER_PAGE))
      page = setpage;
    else
      page = TASKS_MAX / TASKS_PER_PAGE;
  }
  const int edit = getFormItemInt(F("edit"), 0);

  // taskIndex in the URL is 1 ... TASKS_MAX
  // For use in other functions, set it to 0 ... (TASKS_MAX - 1)
  byte taskIndex = getFormItemInt(F("index"), 0);
  boolean taskIndexNotSet = taskIndex == 0;
  --taskIndex;

  byte DeviceIndex = 0;
  LoadTaskSettings(taskIndex); // Make sure ExtraTaskSettings are up-to-date
  // FIXME TD-er: Might have to clear any caches here.
  if (edit != 0  && !taskIndexNotSet) // when form submitted
  {
    if (Settings.TaskDeviceNumber[taskIndex] != taskdevicenumber)
    {
      // change of device: cleanup old device and reset default settings
      setTaskDevice_to_TaskIndex(taskdevicenumber, taskIndex);
    }
    else if (taskdevicenumber != 0) //save settings
    {
      Settings.TaskDeviceNumber[taskIndex] = taskdevicenumber;
      int pin1 = -1;
      int pin2 = -1;
      int pin3 = -1;
      update_whenset_FormItemInt(F("taskdevicepin1"), pin1);
      update_whenset_FormItemInt(F("taskdevicepin2"), pin2);
      update_whenset_FormItemInt(F("taskdevicepin3"), pin3);
      setBasicTaskValues(taskIndex, taskdevicetimer,
                         isFormItemChecked(F("TDE")), WebServer.arg(F("TDN")),
                         pin1, pin2, pin3);
      Settings.TaskDevicePort[taskIndex] = getFormItemInt(F("TDP"), 0);

      for (byte controllerNr = 0; controllerNr < CONTROLLER_MAX; controllerNr++)
      {
        Settings.TaskDeviceID[controllerNr][taskIndex] = getFormItemInt(String(F("TDID")) + (controllerNr + 1));
        Settings.TaskDeviceSendData[controllerNr][taskIndex] = isFormItemChecked(String(F("TDSD")) + (controllerNr + 1));
      }

      DeviceIndex = getDeviceIndex(Settings.TaskDeviceNumber[taskIndex]);
      if (Device[DeviceIndex].PullUpOption)
        Settings.TaskDevicePin1PullUp[taskIndex] = isFormItemChecked(F("TDPPU"));

      if (Device[DeviceIndex].InverseLogicOption)
        Settings.TaskDevicePin1Inversed[taskIndex] = isFormItemChecked(F("TDPI"));

      for (byte varNr = 0; varNr < Device[DeviceIndex].ValueCount; varNr++)
      {
        strncpy_webserver_arg(ExtraTaskSettings.TaskDeviceFormula[varNr], String(F("TDF")) + (varNr + 1));
        ExtraTaskSettings.TaskDeviceValueDecimals[varNr] = getFormItemInt(String(F("TDVD")) + (varNr + 1));
        strncpy_webserver_arg(ExtraTaskSettings.TaskDeviceValueNames[varNr], String(F("TDVN")) + (varNr + 1));

        // taskdeviceformula[varNr].toCharArray(tmpString, 41);
        // strcpy(ExtraTaskSettings.TaskDeviceFormula[varNr], tmpString);
        // ExtraTaskSettings.TaskDeviceValueDecimals[varNr] = taskdevicevaluedecimals[varNr].toInt();
        // taskdevicevaluename[varNr].toCharArray(tmpString, 41);

      }

      // // task value names handling.
      // for (byte varNr = 0; varNr < Device[DeviceIndex].ValueCount; varNr++)
      // {
      //   taskdevicevaluename[varNr].toCharArray(tmpString, 41);
      //   strcpy(ExtraTaskSettings.TaskDeviceValueNames[varNr], tmpString);
      // }

      TempEvent.TaskIndex = taskIndex;
      if (ExtraTaskSettings.TaskIndex != TempEvent.TaskIndex) // if field set empty, reload defaults
        PluginCall(PLUGIN_GET_DEVICEVALUENAMES, &TempEvent, dummyString);

      //allow the plugin to save plugin-specific form settings.
      PluginCall(PLUGIN_WEBFORM_SAVE, &TempEvent, dummyString);

      // notify controllers: CPLUGIN_TASK_CHANGE_NOTIFICATION
      for (byte x=0; x < CONTROLLER_MAX; x++)
        {
          TempEvent.ControllerIndex = x;
          if (Settings.TaskDeviceSendData[TempEvent.ControllerIndex][TempEvent.TaskIndex] &&
            Settings.ControllerEnabled[TempEvent.ControllerIndex] && Settings.Protocol[TempEvent.ControllerIndex])
            {
              TempEvent.ProtocolIndex = getProtocolIndex(Settings.Protocol[TempEvent.ControllerIndex]);
              CPlugin_ptr[TempEvent.ProtocolIndex](CPLUGIN_TASK_CHANGE_NOTIFICATION, &TempEvent, dummyString);
            }
        }
    }
    addHtmlError(SaveTaskSettings(taskIndex));

    addHtmlError(SaveSettings());

    if (taskdevicenumber != 0 && Settings.TaskDeviceEnabled[taskIndex])
      PluginCall(PLUGIN_INIT, &TempEvent, dummyString);
  }

  // show all tasks as table
  if (taskIndexNotSet)
  {
    html_add_script(true);
    TXBuffer += DATA_UPDATE_SENSOR_VALUES_DEVICE_PAGE_JS;
    html_add_script_end();
    html_table_class_multirow();
    html_TR();
    html_table_header("", 70);

    if (TASKS_MAX != TASKS_PER_PAGE)
    {
      html_add_button_prefix();
      TXBuffer += F("devices?setpage=");
      if (page > 1)
        TXBuffer += page - 1;
      else
        TXBuffer += page;
      TXBuffer += F("'>&lt;</a>");
      html_add_button_prefix();
      TXBuffer += F("devices?setpage=");
      if (page < (TASKS_MAX / TASKS_PER_PAGE))
        TXBuffer += page + 1;
      else
        TXBuffer += page;
      TXBuffer += F("'>&gt;</a>");
    }

    html_table_header("Task", 50);
    html_table_header(F("Enabled"), 100);
    html_table_header(F("Device"));
    html_table_header("Name");
    html_table_header("Port");
    html_table_header(F("Ctr (IDX)"), 100);
    html_table_header("GPIO", 70);
    html_table_header(F("Values"));

    String deviceName;

    for (byte x = (page - 1) * TASKS_PER_PAGE; x < ((page) * TASKS_PER_PAGE); x++)
    {
      html_TR_TD();
      html_add_button_prefix();
      TXBuffer += F("devices?index=");
      TXBuffer += x + 1;
      TXBuffer += F("&page=");
      TXBuffer += page;
      TXBuffer += F("'>Edit</a>");
      html_TD();
      TXBuffer += x + 1;
      html_TD();

      if (Settings.TaskDeviceNumber[x] != 0)
      {
        LoadTaskSettings(x);
        DeviceIndex = getDeviceIndex(Settings.TaskDeviceNumber[x]);
        TempEvent.TaskIndex = x;
        addEnabled( Settings.TaskDeviceEnabled[x]);

        html_TD();
        TXBuffer += getPluginNameFromDeviceIndex(DeviceIndex);
        html_TD();
        TXBuffer += ExtraTaskSettings.TaskDeviceName;
        html_TD();

        byte customConfig = false;
        customConfig = PluginCall(PLUGIN_WEBFORM_SHOW_CONFIG, &TempEvent,TXBuffer.buf);
        if (!customConfig)
          if (Device[DeviceIndex].Ports != 0)
            TXBuffer += formatToHex_decimal(Settings.TaskDevicePort[x]);

        html_TD();

        if (Device[DeviceIndex].SendDataOption)
        {
          boolean doBR = false;
          for (byte controllerNr = 0; controllerNr < CONTROLLER_MAX; controllerNr++)
          {
            byte ProtocolIndex = getProtocolIndex(Settings.Protocol[controllerNr]);
            if (Settings.TaskDeviceSendData[controllerNr][x])
            {
              if (doBR)
                TXBuffer += F("<BR>");
              TXBuffer += getControllerSymbol(controllerNr);
              if (Protocol[ProtocolIndex].usesID && Settings.Protocol[controllerNr] != 0)
              {
                TXBuffer += " (";
                TXBuffer += Settings.TaskDeviceID[controllerNr][x];
                TXBuffer += ')';
                if (Settings.TaskDeviceID[controllerNr][x] == 0)
                  TXBuffer += F(" " HTML_SYMBOL_WARNING);
              }
              doBR = true;
            }
          }
        }

        html_TD();

        if (Settings.TaskDeviceDataFeed[x] == 0)
        {
          if (Device[DeviceIndex].Type == DEVICE_TYPE_I2C)
          {
            TXBuffer += F("GPIO-");
            TXBuffer += Settings.Pin_i2c_sda;
            TXBuffer += F("<BR>GPIO-");
            TXBuffer += Settings.Pin_i2c_scl;
          }
          if (Device[DeviceIndex].Type == DEVICE_TYPE_ANALOG)
            TXBuffer += F("ADC (TOUT)");

          if (Settings.TaskDevicePin1[x] != -1)
          {
            TXBuffer += F("GPIO-");
            TXBuffer += Settings.TaskDevicePin1[x];
          }

          if (Settings.TaskDevicePin2[x] != -1)
          {
            TXBuffer += F("<BR>GPIO-");
            TXBuffer += Settings.TaskDevicePin2[x];
          }

          if (Settings.TaskDevicePin3[x] != -1)
          {
            TXBuffer += F("<BR>GPIO-");
            TXBuffer += Settings.TaskDevicePin3[x];
          }
        }

        html_TD();
        byte customValues = false;
        customValues = PluginCall(PLUGIN_WEBFORM_SHOW_VALUES, &TempEvent,TXBuffer.buf);
        if (!customValues)
        {
          for (byte varNr = 0; varNr < Device[DeviceIndex].ValueCount; varNr++)
          {
            if (Settings.TaskDeviceNumber[x] != 0)
            {
              if (varNr > 0)
                TXBuffer += F("<div class='div_br'></div>");
              TXBuffer += F("<div class='div_l' id='valuename_");
              TXBuffer  += x;
              TXBuffer  += '_';
              TXBuffer  += varNr;
              TXBuffer  += "'>";
              TXBuffer += ExtraTaskSettings.TaskDeviceValueNames[varNr];
              TXBuffer += F(":</div><div class='div_r' id='value_");
              TXBuffer  += x;
              TXBuffer  += '_';
              TXBuffer  += varNr;
              TXBuffer  += "'>";
              TXBuffer += formatUserVarNoCheck(x, varNr);
              TXBuffer += "</div>";
            }
          }
        }
      }
      else {
        html_TD(6);
      }

    } // next
    html_end_table();
    html_end_form();

  }
  // Show edit form if a specific entry is chosen with the edit button
  else
  {
    LoadTaskSettings(taskIndex);
    DeviceIndex = getDeviceIndex(Settings.TaskDeviceNumber[taskIndex]);
    TempEvent.TaskIndex = taskIndex;

    html_add_form();
    html_table_class_normal();
    addFormHeader(F("Task Settings"));


    TXBuffer += F("<TR><TD style='width:150px;' align='left'>Device:<TD>");

    //no device selected
    if (Settings.TaskDeviceNumber[taskIndex] == 0 )
    {
      //takes lots of memory/time so call this only when needed.
      addDeviceSelect("TDNUM", Settings.TaskDeviceNumber[taskIndex]);   //="taskdevicenumber"

    }
    // device selected
    else
    {
      //remember selected device number
      TXBuffer += F("<input type='hidden' name='TDNUM' value='");
      TXBuffer += Settings.TaskDeviceNumber[taskIndex];
      TXBuffer += "'>";

      //show selected device name and delete button
      TXBuffer += getPluginNameFromDeviceIndex(DeviceIndex);

      addHelpButton(String(F("Plugin")) + Settings.TaskDeviceNumber[taskIndex]);
      addRTDPluginButton(Settings.TaskDeviceNumber[taskIndex]);


      if (Device[DeviceIndex].Number == 3 && taskIndex >= 4) // Number == 3 = PulseCounter Plugin
        {
          addFormNote(F("This plugin is only supported on task 1-4 for now"));
        }

      addFormTextBox( F("Name"), F("TDN"), ExtraTaskSettings.TaskDeviceName, NAME_FORMULA_LENGTH_MAX);   //="taskdevicename"

      addFormCheckBox(F("Enabled"), F("TDE"), Settings.TaskDeviceEnabled[taskIndex]);   //="taskdeviceenabled"

      // section: Sensor / Actuator
      if (!Device[DeviceIndex].Custom && Settings.TaskDeviceDataFeed[taskIndex] == 0 &&
          ((Device[DeviceIndex].Ports != 0) ||
           (Device[DeviceIndex].PullUpOption) ||
           (Device[DeviceIndex].InverseLogicOption) ||
           (Device[DeviceIndex].connectedToGPIOpins())) )
      {
        addFormSubHeader((Device[DeviceIndex].SendDataOption) ? F("Sensor") : F("Actuator"));

        if (Device[DeviceIndex].Ports != 0)
          addFormNumericBox(F("Port"), F("TDP"), Settings.TaskDevicePort[taskIndex]);   //="taskdeviceport"

        if (Device[DeviceIndex].PullUpOption)
        {
          addFormCheckBox(F("Internal PullUp"), F("TDPPU"), Settings.TaskDevicePin1PullUp[taskIndex]);   //="taskdevicepin1pullup"
          if ((Settings.TaskDevicePin1[taskIndex] == 16) || (Settings.TaskDevicePin2[taskIndex] == 16) || (Settings.TaskDevicePin3[taskIndex] == 16))
            addFormNote(F("GPIO-16 (D0) does not support PullUp"));
        }

        if (Device[DeviceIndex].InverseLogicOption)
        {
          addFormCheckBox(F("Inversed Logic"), F("TDPI"), Settings.TaskDevicePin1Inversed[taskIndex]);   //="taskdevicepin1inversed"
          addFormNote(F("Will go into effect on next input change."));
        }

        //get descriptive GPIO-names from plugin
        TempEvent.String1 = F("1st GPIO");
        TempEvent.String2 = F("2nd GPIO");
        TempEvent.String3 = F("3rd GPIO");
        PluginCall(PLUGIN_GET_DEVICEGPIONAMES, &TempEvent, dummyString);

        if (Device[DeviceIndex].connectedToGPIOpins()) {
          if (Device[DeviceIndex].Type >= DEVICE_TYPE_SINGLE)
            addFormPinSelect(TempEvent.String1, F("taskdevicepin1"), Settings.TaskDevicePin1[taskIndex]);
          if (Device[DeviceIndex].Type >= DEVICE_TYPE_DUAL)
            addFormPinSelect(TempEvent.String2, F("taskdevicepin2"), Settings.TaskDevicePin2[taskIndex]);
          if (Device[DeviceIndex].Type == DEVICE_TYPE_TRIPLE)
            addFormPinSelect(TempEvent.String3, F("taskdevicepin3"), Settings.TaskDevicePin3[taskIndex]);
        }
      }

      //add plugins content
      if (Settings.TaskDeviceDataFeed[taskIndex] == 0) { // only show additional config for local connected sensors
        String webformLoadString;
        PluginCall(PLUGIN_WEBFORM_LOAD, &TempEvent,webformLoadString);
        if (webformLoadString.length() > 0) {
          String errorMessage;
          PluginCall(PLUGIN_GET_DEVICENAME, &TempEvent, errorMessage);
          errorMessage += F(": Bug in PLUGIN_WEBFORM_LOAD, should not append to string, use addHtml() instead");
          addHtmlError(errorMessage);
        }
      }

      //section: Data Acquisition
      if (Device[DeviceIndex].SendDataOption)
      {
        addFormSubHeader(F("Data Acquisition"));

        for (byte controllerNr = 0; controllerNr < CONTROLLER_MAX; controllerNr++)
        {
          if (Settings.Protocol[controllerNr] != 0)
          {
            String id = F("TDSD");   //="taskdevicesenddata"
            id += controllerNr + 1;

            html_TR_TD(); TXBuffer += F("Send to Controller ");
            TXBuffer += getControllerSymbol(controllerNr);
            html_TD();
            addCheckBox(id, Settings.TaskDeviceSendData[controllerNr][taskIndex]);

            byte ProtocolIndex = getProtocolIndex(Settings.Protocol[controllerNr]);
            if (Protocol[ProtocolIndex].usesID && Settings.Protocol[controllerNr] != 0)
            {
              addRowLabel(F("IDX"));
              id = F("TDID");   //="taskdeviceid"
              id += controllerNr + 1;
              addNumericBox(id, Settings.TaskDeviceID[controllerNr][taskIndex], 0, DOMOTICZ_MAX_IDX);
            }
          }
        }
      }

      addFormSeparator(2);

      if (Device[DeviceIndex].TimerOption)
      {
        //FIXME: shoudn't the max be ULONG_MAX because Settings.TaskDeviceTimer is an unsigned long? addFormNumericBox only supports ints for min and max specification
        addFormNumericBox( F("Interval"), F("TDT"), Settings.TaskDeviceTimer[taskIndex], 0, 65535);   //="taskdevicetimer"
        addUnit(F("sec"));
        if (Device[DeviceIndex].TimerOptional)
          TXBuffer += F(" (Optional for this Device)");
      }

      //section: Values
      if (!Device[DeviceIndex].Custom && Device[DeviceIndex].ValueCount > 0)
      {
        addFormSubHeader(F("Values"));
        html_end_table();
        html_table_class_normal();

        //table header
        TXBuffer += F("<TR><TH style='width:30px;' align='center'>#");
        html_table_header("Name");

        if (Device[DeviceIndex].FormulaOption)
        {
          html_table_header(F("Formula"), F("EasyFormula"), 0);
        }

        if (Device[DeviceIndex].FormulaOption || Device[DeviceIndex].DecimalsOnly)
        {
          html_table_header(F("Decimals"), 30);
        }

        //table body
        for (byte varNr = 0; varNr < Device[DeviceIndex].ValueCount; varNr++)
        {
          html_TR_TD();
          TXBuffer += varNr + 1;
          html_TD();
          String id = F("TDVN");   //="taskdevicevaluename"
          id += (varNr + 1);
          addTextBox(id, ExtraTaskSettings.TaskDeviceValueNames[varNr], NAME_FORMULA_LENGTH_MAX);

          if (Device[DeviceIndex].FormulaOption)
          {
            html_TD();
            String id = F("TDF");   //="taskdeviceformula"
            id += (varNr + 1);
            addTextBox(id, ExtraTaskSettings.TaskDeviceFormula[varNr], NAME_FORMULA_LENGTH_MAX);
          }

          if (Device[DeviceIndex].FormulaOption || Device[DeviceIndex].DecimalsOnly)
          {
            html_TD();
            String id = F("TDVD");   //="taskdevicevaluedecimals"
            id += (varNr + 1);
            addNumericBox(id, ExtraTaskSettings.TaskDeviceValueDecimals[varNr], 0, 6);
          }
        }
      }
    }

    addFormSeparator(4);

    html_TR_TD();
    TXBuffer += F("<TD colspan='3'>");
    html_add_button_prefix();
    TXBuffer += F("devices?setpage=");
    TXBuffer += page;
    TXBuffer += F("'>Close</a>");
    addSubmitButton();
    TXBuffer += F("<input type='hidden' name='edit' value='1'>");
    TXBuffer += F("<input type='hidden' name='page' value='1'>");

    //if user selected a device, add the delete button
    if (Settings.TaskDeviceNumber[taskIndex] != 0 )
      addSubmitButton(F("Delete"), F("del"));

    html_end_table();
    html_end_form();
  }


  checkRAM(F("handle_devices"));
  if (loglevelActiveFor(LOG_LEVEL_DEBUG_DEV)) {
    String log = F("DEBUG: String size:");
    log += String(TXBuffer.sentBytes);
    addLog(LOG_LEVEL_DEBUG_DEV, log);
  }
  sendHeadandTail_stdtemplate(_TAIL);
  TXBuffer.endStream();
}


byte sortedIndex[DEVICES_MAX + 1];
//********************************************************************************
// Add a device select dropdown list
//********************************************************************************
void addDeviceSelect(const String& name,  int choice)
{
  // first get the list in alphabetic order
  for (byte x = 0; x <= deviceCount; x++)
    sortedIndex[x] = x;
  sortDeviceArray();

  String deviceName;

  addSelector_Head(name, true);
  addSelector_Item(F("- None -"), 0, false, false, "");
  for (byte x = 0; x <= deviceCount; x++)
  {
    byte deviceIndex = sortedIndex[x];
    if (Plugin_id[deviceIndex] != 0)
      deviceName = getPluginNameFromDeviceIndex(deviceIndex);

#ifdef PLUGIN_BUILD_DEV
    int num = Plugin_id[deviceIndex];
    String plugin = "P";
    if (num < 10) plugin += '0';
    if (num < 100) plugin += '0';
    plugin += num;
    plugin += F(" - ");
    deviceName = plugin + deviceName;
#endif

    addSelector_Item(deviceName,
                     Device[deviceIndex].Number,
                     choice == Device[deviceIndex].Number,
                     false,
                     "");
  }
  addSelector_Foot();
}

//********************************************************************************
// Device Sort routine, switch array entries
//********************************************************************************
void switchArray(byte value)
{
  byte temp;
  temp = sortedIndex[value - 1];
  sortedIndex[value - 1] = sortedIndex[value];
  sortedIndex[value] = temp;
}


//********************************************************************************
// Device Sort routine, compare two array entries
//********************************************************************************
boolean arrayLessThan(const String& ptr_1, const String& ptr_2)
{
  unsigned int i = 0;
  while (i < ptr_1.length())    // For each character in string 1, starting with the first:
  {
    if (ptr_2.length() < i)    // If string 2 is shorter, then switch them
    {
      return true;
    }
    else
    {
      const char check1 = (char)ptr_1[i];  // get the same char from string 1 and string 2
      const char check2 = (char)ptr_2[i];
      if (check1 == check2) {
        // they're equal so far; check the next char !!
        i++;
      } else {
        return (check2 > check1);
      }
    }
  }
  return false;
}


//********************************************************************************
// Device Sort routine, actual sorting
//********************************************************************************
void sortDeviceArray()
{
  int innerLoop ;
  int mainLoop ;
  for ( mainLoop = 1; mainLoop <= deviceCount; mainLoop++)
  {
    innerLoop = mainLoop;
    while (innerLoop  >= 1)
    {
      if (arrayLessThan(
        getPluginNameFromDeviceIndex(sortedIndex[innerLoop]),
        getPluginNameFromDeviceIndex(sortedIndex[innerLoop - 1])))
      {
        switchArray(innerLoop);
      }
      innerLoop--;
    }
  }
}

void addFormPinSelect(const String& label, const String& id, int choice)
{
  addRowLabel(label);
  addPinSelect(false, id, choice);
}


void addFormPinSelectI2C(const String& label, const String& id, int choice)
{
  addRowLabel(label);
  addPinSelect(true, id, choice);
}


//********************************************************************************
// Add a GPIO pin select dropdown list for 8266, 8285 or ESP32
//********************************************************************************
String createGPIO_label(int gpio_pin, int nodemcu_pinnr, bool input, bool output, bool warning) {
  if (gpio_pin < 0) return F("- None -");
  String result;
  result.reserve(24);
  result = F("GPIO-");
  result += gpio_pin;
  if (nodemcu_pinnr >= 0) {
    result += F(" (D");
    result += nodemcu_pinnr;
    result += ')';
  }
  if (input != output) {
    result += ' ';
    result += input ? F(HTML_SYMBOL_INPUT) : F(HTML_SYMBOL_OUTPUT);
  }
  if (warning) {
    result += ' ';
    result += F(HTML_SYMBOL_WARNING);
  }
  bool serialPinConflict = (Settings.UseSerial && (gpio_pin == 1 || gpio_pin == 3));
  if (serialPinConflict) {
    if (gpio_pin == 1) { result += F(" TX0"); }
    if (gpio_pin == 3) { result += F(" RX0"); }
  }
  return result;
}

void addPinSelect(boolean forI2C, String name,  int choice)
{
  #ifdef ESP32
    #define NR_ITEMS_PIN_DROPDOWN  35 // 34 GPIO + 1
  #else
    #define NR_ITEMS_PIN_DROPDOWN  14 // 13 GPIO + 1
  #endif

  String * gpio_labels = new String[NR_ITEMS_PIN_DROPDOWN];
  int * gpio_numbers = new int[NR_ITEMS_PIN_DROPDOWN];

  // At i == 0 && gpio_pin == -1, add the "- None -" option first
  int i = 0;
  int gpio_pin = -1;
  while (i < NR_ITEMS_PIN_DROPDOWN && gpio_pin <= MAX_GPIO) {
    int nodemcu_pinnr = -1;
    bool input, output, warning;
    if (getGpioInfo(gpio_pin, nodemcu_pinnr, input, output, warning) || i == 0) {
      gpio_labels[i] = createGPIO_label(gpio_pin, nodemcu_pinnr, input, output, warning);
      gpio_numbers[i] = gpio_pin;
      ++i;
    }
    ++gpio_pin;
  }
  renderHTMLForPinSelect(gpio_labels, gpio_numbers, forI2C, name, choice, NR_ITEMS_PIN_DROPDOWN);
  delete[] gpio_numbers;
  delete[] gpio_labels;
  #undef NR_ITEMS_PIN_DROPDOWN
}


//********************************************************************************
// Helper function actually rendering dropdown list for addPinSelect()
//********************************************************************************
void renderHTMLForPinSelect(String options[], int optionValues[], boolean forI2C, const String& name,  int choice, int count) {
  addSelector_Head(name, false);
  for (byte x = 0; x < count; x++)
  {
    boolean disabled = false;

    if (optionValues[x] != -1) // empty selection can never be disabled...
    {
      if (!forI2C && ((optionValues[x] == Settings.Pin_i2c_sda) || (optionValues[x] == Settings.Pin_i2c_scl)))
        disabled = true;
      if (Settings.UseSerial && ((optionValues[x] == 1) || (optionValues[x] == 3)))
        disabled = true;
    }
    addSelector_Item(options[x],
                     optionValues[x],
                     choice == optionValues[x],
                     disabled,
                     "");
  }
  addSelector_Foot();
}


void addFormSelectorI2C(const String& id, int addressCount, const int addresses[], int selectedIndex)
{
  String options[addressCount];
  for (byte x = 0; x < addressCount; x++)
  {
    options[x] = formatToHex_decimal(addresses[x]);
    if (x == 0)
      options[x] += F(" - (default)");
  }
  addFormSelector(F("I2C Address"), id, addressCount, options, addresses, NULL, selectedIndex, false);
}

void addFormSelector(const String& label, const String& id, int optionCount, const String options[], const int indices[], int selectedIndex)
{
  addFormSelector(label, id, optionCount, options, indices, NULL, selectedIndex, false);
}

void addFormSelector(const String& label, const String& id, int optionCount, const String options[], const int indices[], const String attr[], int selectedIndex, boolean reloadonchange)
{
  addRowLabel(label);
  addSelector(id, optionCount, options, indices, attr, selectedIndex, reloadonchange);
}

void addSelector(const String& id, int optionCount, const String options[], const int indices[], const String attr[], int selectedIndex, boolean reloadonchange) {
  addSelector(id, optionCount, options, indices, attr, selectedIndex, reloadonchange, true);
}

void addSelector(const String& id, int optionCount, const String options[], const int indices[], const String attr[], int selectedIndex, boolean reloadonchange, bool enabled)
{
  int index;
  // FIXME TD-er Change boolean to disabled
  addSelector_Head(id, reloadonchange, !enabled);

  for (byte x = 0; x < optionCount; x++)
  {
    if (indices)
      index = indices[x];
    else
      index = x;
    TXBuffer += F("<option value=");
    TXBuffer += index;
    if (selectedIndex == index)
      TXBuffer += F(" selected");
    if (attr)
    {
      TXBuffer += ' ';
      TXBuffer += attr[x];
    }
    TXBuffer += '>';
    TXBuffer += options[x];
    TXBuffer += F("</option>");
  }
  TXBuffer += F("</select>");
}

void addSelector_Head(const String& id, boolean reloadonchange) {
  addSelector_Head(id, reloadonchange, false);
}

void addSelector_Head(const String& id, boolean reloadonchange, bool disabled)
{
  TXBuffer += F("<select class='wide' name='");
  TXBuffer += id;
  TXBuffer += '\'';
  if (disabled) {
    addDisabled();
  }
  if (reloadonchange)
    TXBuffer += F(" onchange='return dept_onchange(frmselect)'");
  TXBuffer += '>';
}

void addSelector_Item(const String& option, int index, boolean selected, boolean disabled, const String& attr)
{
  TXBuffer += F("<option value=");
  TXBuffer += index;
  if (selected)
    TXBuffer += F(" selected");
  if (disabled)
    addDisabled();
  if (attr && attr.length() > 0)
  {
    TXBuffer += ' ';
    TXBuffer += attr;
  }
  TXBuffer += '>';
  TXBuffer += option;
  TXBuffer += F("</option>");
}


void addSelector_Foot()
{
  TXBuffer += F("</select>");
}


void addUnit(const String& unit)
{
  TXBuffer += F(" [");
  TXBuffer += unit;
  TXBuffer += "]";
}


void addRowLabel(const String& label)
{
  html_TR_TD();
  TXBuffer += label;
  TXBuffer += ':';
  html_TD();
}

// Add a row label and mark it with copy markers to copy it to clipboard.
void addRowLabel_copy(const String& label) {
  TXBuffer += F("<TR>");
  html_copyText_TD();
  TXBuffer += label;
  TXBuffer += ':';
  html_copyText_marker();
  html_copyText_TD();
}

void addButton(const String &url, const String &label) {
  addButton(url, label, "");
}

void addButton(const String &url, const String &label, const String& classes)
{
  html_add_button_prefix(classes);
  TXBuffer += url;
  TXBuffer += "'>";
  TXBuffer += label;
  TXBuffer += F("</a>");
}

void addButton(class StreamingBuffer &buffer, const String &url, const String &label)
{
  buffer += F("<a class='button link' href='");
  buffer += url;
  buffer += F("'>");
  buffer += label;
  buffer += F("</a>");
}

void addSaveButton(const String &url, const String &label)
{
  addSaveButton(TXBuffer, url, label);
}

void addSaveButton(class StreamingBuffer &buffer, const String &url, const String &label)
{
  buffer += F("<a class='button link' href='");
  buffer += url;
  buffer += F("' alt='");
  buffer += label;
  buffer += F("'>");
  buffer += F("<svg width='24' height='24' viewBox='-1 -1 26 26' style='position: relative; top: 5px;'>");
  buffer += F("<path d='M19 12v7H5v-7H3v7c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2v-7h-2zm-6 .67l2.59-2.58L17 11.5l-5 5-5-5 1.41-1.41L11 12.67V3h2v9.67z'  stroke='white' fill='white' ></path>");
  buffer += F("</svg>");
  buffer += F("</a>");
}

void addDeleteButton(const String &url, const String &label)
{
  addSaveButton(TXBuffer, url, label);
}

void addDeleteButton(class StreamingBuffer &buffer, const String &url, const String &label)
{
  buffer += F("<a class='button link' href='");
  buffer += url;
  buffer += F("' alt='");
  buffer += label;
  buffer += F("' onclick='return confirm(\"Are you sure?\")'>");
  buffer += F("<svg width='24' height='24' viewBox='-1 -1 26 26' style='position: relative; top: 5px;'>");
  buffer += F("<path fill='none' d='M0 0h24v24H0V0z'></path>");
  buffer += F("<path d='M6 19c0 1.1.9 2 2 2h8c1.1 0 2-.9 2-2V7H6v12zM8 9h8v10H8V9zm7.5-5l-1-1h-5l-1 1H5v2h14V4h-3.5z' stroke='white' fill='white' ></path>");
  buffer += F("</svg>");
  buffer += F("</a>");
}

void addWideButton(const String &url, const String &label, const String &classes)
{
  html_add_wide_button_prefix(classes);
  TXBuffer += url;
  TXBuffer += "'>";
  TXBuffer += label;
  TXBuffer += F("</a>");
}

void addSubmitButton()
{
  addSubmitButton(F("Submit"), "");
}

//add submit button with different label and name
void addSubmitButton(const String &value, const String &name) {
  addSubmitButton(value, name, "");
}

void addSubmitButton(const String &value, const String &name, const String &classes)
{
  TXBuffer += F("<input class='button link");
  if (classes.length() > 0) {
    TXBuffer += ' ';
    TXBuffer += classes;
  }
  TXBuffer += F("' type='submit' value='");
  TXBuffer += value;
  if (name.length() > 0) {
    TXBuffer += F("' name='");
    TXBuffer += name;
  }
  TXBuffer += F("'><div id='toastmessage'></div><script type='text/javascript'>toasting();</script>");
}

// add copy to clipboard button
void addCopyButton(const String &value, const String &delimiter, const String &name)
{
  TXBuffer += jsClipboardCopyPart1;
  TXBuffer += value;
  TXBuffer += jsClipboardCopyPart2;
  TXBuffer += delimiter;
  TXBuffer += jsClipboardCopyPart3;
  //Fix HTML
  TXBuffer += F("<button class='button link' onclick='setClipboard()'>");
  TXBuffer += name;
  TXBuffer += " (";
  html_copyText_marker();
  TXBuffer += ')';
  TXBuffer += F("</button>");
}


//********************************************************************************
// Add a header
//********************************************************************************
void addTableSeparator(const String& label, int colspan, int h_size) {
  addTableSeparator(label, colspan, h_size, "");
}

void addTableSeparator(const String& label, int colspan, int h_size, const String& helpButton) {
  TXBuffer += F("<TR><TD colspan=");
  TXBuffer += colspan;
  TXBuffer += "><H";
  TXBuffer += h_size;
  TXBuffer += '>';
  TXBuffer += label;
  if (helpButton.length() > 0)
    addHelpButton(helpButton);
  TXBuffer += "</H";
  TXBuffer += h_size;
  TXBuffer += F("></TD></TR>");
}

void addFormHeader(const String& header, const String& helpButton)
{
  html_TR();
  html_table_header(header, helpButton, 225);
  html_table_header("");
}

void addFormHeader(const String& header)
{
  addFormHeader(header, "");
}


//********************************************************************************
// Add a sub header
//********************************************************************************
void addFormSubHeader(const String& header)
{
  addTableSeparator(header, 2, 3);
}


//********************************************************************************
// Add a note as row start
//********************************************************************************
void addFormNote(const String& text)
{
  html_TR_TD();
  html_TD();
  TXBuffer += F("<div class='note'>Note: ");
  TXBuffer += text;
  TXBuffer += F("</div>");
}


//********************************************************************************
// Add a separator as row start
//********************************************************************************
void addFormSeparator(int clspan)
{
 TXBuffer += F("<TR><TD colspan='");
 TXBuffer += clspan;
 TXBuffer += F("'><hr>");
}


//********************************************************************************
// Add a checkbox
//********************************************************************************
void addCheckBox(const String& id, boolean checked) {
  addCheckBox(id, checked, false);
}

void addCheckBox(const String& id, boolean checked, bool disabled)
{
  TXBuffer += F("<label class='container'>&nbsp;");
  TXBuffer += F("<input type='checkbox' id='");
  TXBuffer += id;
  TXBuffer += F("' name='");
  TXBuffer += id;
  TXBuffer += '\'';
  if (checked)
    TXBuffer += F(" checked");
  if (disabled) addDisabled();
  TXBuffer += F("><span class='checkmark");
  if (disabled) addDisabled();
  TXBuffer += F("'></span></label>");
}

void addFormCheckBox(const String& label, const String& id, boolean checked) {
  addFormCheckBox(label, id, checked, false);
}

void addFormCheckBox_disabled(const String& label, const String& id, boolean checked) {
  addFormCheckBox(label, id, checked, true);
}

void addFormCheckBox(const String& label, const String& id, boolean checked, bool disabled)
{
  addRowLabel(label);
  addCheckBox(id, checked, disabled);
}


//********************************************************************************
// Add a numeric box
//********************************************************************************
void addNumericBox(const String& id, int value, int min, int max)
{
  TXBuffer += F("<input class='widenumber' type='number' name='");
  TXBuffer += id;
  TXBuffer += '\'';
  if (min != INT_MIN)
  {
    TXBuffer += F(" min=");
    TXBuffer += min;
  }
  if (max != INT_MAX)
  {
    TXBuffer += F(" max=");
    TXBuffer += max;
  }
  TXBuffer += F(" value=");
  TXBuffer += value;
  TXBuffer += '>';
}

void addNumericBox(const String& id, int value)
{
  addNumericBox(id, value, INT_MIN, INT_MAX);
}

void addFormNumericBox(const String& label, const String& id, int value, int min, int max)
{
  addRowLabel(label);
  addNumericBox(id, value, min, max);
}

void addFormNumericBox(const String& label, const String& id, int value)
{
  addFormNumericBox(label, id, value, INT_MIN, INT_MAX);
}

void addFloatNumberBox(const String& id, float value, float min, float max)
{
  TXBuffer += F("<input type='number' name='");
  TXBuffer += id;
  TXBuffer += '\'';
  TXBuffer += F(" min=");
  TXBuffer += min;
  TXBuffer += F(" max=");
  TXBuffer += max;
  TXBuffer += F(" step=0.01");
  TXBuffer += F(" style='width:5em;' value=");
  TXBuffer += value;
  TXBuffer += '>';
}

void addFormFloatNumberBox(const String& label, const String& id, float value, float min, float max)
{
  addRowLabel(label);
  addFloatNumberBox(id, value, min, max);
}


void addTextBox(const String& id, const String&  value, int maxlength)
{
  addTextBox(id, value, maxlength, false);
}

void addTextBox(const String& id, const String&  value, int maxlength, bool readonly)
{
  addTextBox(id, value, maxlength, false, false, "");
}

void addTextBox(const String& id, const String&  value, int maxlength, bool readonly, bool required)
{
  addTextBox(id, value, maxlength, false, false, "");
}

void addTextBox(const String& id, const String&  value, int maxlength, bool readonly, bool required, const String& pattern)
{
  TXBuffer += F("<input class='wide' type='text' name='");
  TXBuffer += id;
  TXBuffer += F("' maxlength=");
  TXBuffer += maxlength;
  TXBuffer += F(" value='");
  TXBuffer += value;
  TXBuffer += '\'';
  if(readonly){
    TXBuffer += F(" readonly ");
  }
  if(required){
    TXBuffer += F(" required ");
  }
  if(pattern.length()>0){
    TXBuffer += F("pattern = '");
    TXBuffer += pattern;
    TXBuffer += '\'';
  }
  TXBuffer += '>';
}

void addFormTextBox(const String& label, const String& id, const String&  value, int maxlength)
{
  addRowLabel(label);
  addTextBox(id, value, maxlength);
}

void addFormTextBox(const String& label, const String& id, const String&  value, int maxlength, bool readonly)
{
  addRowLabel(label);
  addTextBox(id, value, maxlength, readonly);
}

void addFormTextBox(const String& label, const String& id, const String&  value, int maxlength, bool readonly, bool required)
{
  addRowLabel(label);
  addTextBox(id, value, maxlength, readonly, required);
}

void addFormTextBox(const String& label, const String& id, const String&  value, int maxlength, bool readonly, bool required, const String& pattern)
{
  addRowLabel(label);
  addTextBox(id, value, maxlength, readonly, required, pattern);
}


void addFormPasswordBox(const String& label, const String& id, const String& password, int maxlength)
{
  addRowLabel(label);
  TXBuffer += F("<input class='wide' type='password' name='");
  TXBuffer += id;
  TXBuffer += F("' maxlength=");
  TXBuffer += maxlength;
  TXBuffer += F(" value='");
  if (password != "")   //no password?
    TXBuffer += F("*****");
  //TXBuffer += password;   //password will not published over HTTP
  TXBuffer += "'>";
}

void copyFormPassword(const String& id, char* pPassword, int maxlength)
{
  String password = WebServer.arg(id);
  if (password == F("*****"))   //no change?
    return;
  safe_strncpy(pPassword, password.c_str(), maxlength);
}

void addFormIPBox(const String& label, const String& id, const byte ip[4])
{
  bool empty_IP =(ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0);

  addRowLabel(label);
  TXBuffer += F("<input class='wide' type='text' name='");
  TXBuffer += id;
  TXBuffer += F("' value='");
  if (!empty_IP){
    TXBuffer += formatIP(ip);
  }
  TXBuffer += "'>";
}

// adds a Help Button with points to the the given Wiki Subpage
void addHelpButton(const String& url)
{
  TXBuffer += F(" <a class='button help' href='");
  if (!url.startsWith(F("http"))) {
    TXBuffer += F("http://www.letscontrolit.com/wiki/index.php/");
  }
  TXBuffer += url;
  TXBuffer += F("' target='_blank'>&#10068;</a>");
}

void addRTDPluginButton(int taskDeviceNumber) {
  TXBuffer += F(" <a class='button help' href='");
  TXBuffer += F("https://espeasy.readthedocs.io/en/latest/Plugin/P");
  if (taskDeviceNumber < 100) TXBuffer += '0';
  if (taskDeviceNumber < 10) TXBuffer += '0';
  TXBuffer += String(taskDeviceNumber);
  TXBuffer += F(".html' target='_blank'>&#8505;</a>");
}

void addEnabled(boolean enabled)
{
  TXBuffer += F("<span class='enabled ");
  if (enabled)
    TXBuffer += F("on'>&#10004;");
  else
    TXBuffer += F("off'>&#10060;");
  TXBuffer += F("</span>");
}


//********************************************************************************
// HTML string re-use to keep the executable smaller
// Flash strings are not checked for duplication.
//********************************************************************************
void wrap_html_tag(const String& tag, const String& text) {
  TXBuffer += '<';
  TXBuffer += tag;
  TXBuffer += '>';
  TXBuffer += text;
  TXBuffer += "</";
  TXBuffer += tag;
  TXBuffer += '>';
}

void html_B(const String& text) {
  wrap_html_tag("b", text);
}

void html_U(const String& text) {
  wrap_html_tag("u", text);
}

void html_TR_TD_highlight() {
  TXBuffer += F("<TR class=\"highlight\">");
  html_TD();
}

void html_TR_TD() {
  html_TR();
  html_TD();
}

void html_BR() {
  TXBuffer += F("<BR>");
}

void html_TR() {
  TXBuffer += F("<TR>");
}

void html_TR_TD_height(int height) {
  html_TR();
  TXBuffer += F("<TD HEIGHT=\"");
  TXBuffer += height;
  TXBuffer += "\">";
}

void html_TD() {
  html_TD(1);
}

void html_TD(int td_cnt) {
  for (int i = 0; i < td_cnt; ++i) {
    TXBuffer += F("<TD>");
  }
}

static int copyTextCounter = 0;

void html_reset_copyTextCounter() {
  copyTextCounter = 0;
}

void html_copyText_TD() {
  ++copyTextCounter;
  TXBuffer += F("<TD id='copyText_");
  TXBuffer += copyTextCounter;
  TXBuffer += "'>";
}

// Add some recognizable token to show which parts will be copied.
void html_copyText_marker() {
  TXBuffer += F("&#x022C4;"); //   &diam; &diamond; &Diamond; &#x022C4; &#8900;
}

void html_add_estimate_symbol() {
  TXBuffer += F(" &#8793; "); //   &#8793;  &#x2259;  &wedgeq;
}

void html_table_class_normal() {
  html_table(F("normal"));
}

void html_table_class_multirow() {
  html_table(F("multirow"), true);
}

void html_table_class_multirow_noborder() {
  html_table(F("multirow"), false);
}

void html_table(const String& tableclass) {
  html_table(tableclass, false);
}

void html_table(const String& tableclass, bool boxed) {
  TXBuffer += F("<table class='");
  TXBuffer += tableclass;
  TXBuffer += '\'';
  if (boxed) {
    TXBuffer += F("' border=1px frame='box' rules='all'");
  }
  TXBuffer += '>';
}

void html_table_header(const String& label) {
  html_table_header(label, 0);
}

void html_table_header(const String& label, int width) {
  html_table_header(label, "", width);
}

void html_table_header(const String& label, const String& helpButton, int width) {
  TXBuffer += F("<TH");
  if (width > 0) {
    TXBuffer += F(" style='width:");
    TXBuffer += String(width);
    TXBuffer += F("px;'");
  }
  TXBuffer += '>';
  TXBuffer += label;
  if (helpButton.length() > 0)
    addHelpButton(helpButton);
  TXBuffer += F("</TH>");
}

void html_end_table() {
  TXBuffer += F("</table>");
}

void html_end_form() {
  TXBuffer += F("</form>");
}

void html_add_button_prefix() {
  html_add_button_prefix("");
}

void html_add_button_prefix(const String& classes) {
  TXBuffer += F(" <a class='button link");
  if (classes.length() > 0) {
    TXBuffer += ' ';
    TXBuffer += classes;
  }
  TXBuffer += F("' href='");
}

void html_add_wide_button_prefix() {
  html_add_wide_button_prefix("");
}

void html_add_wide_button_prefix(const String& classes) {
  String wide_classes;
  wide_classes.reserve(classes.length() + 5);
  wide_classes = F("wide ");
  wide_classes += classes;
  html_add_button_prefix(wide_classes);
}

void html_add_form() {
  TXBuffer += F("<form name='frmselect' method='post'>");
}

void html_add_autosubmit_form() {
  TXBuffer += F("<script><!--\n"
           "function dept_onchange(frmselect) {frmselect.submit();}"
           "\n//--></script>");
}

void html_add_script(bool defer) {
  TXBuffer += F("<script");
  if (defer) {
    TXBuffer += F(" defer");
  }
  TXBuffer += F(" type='text/JavaScript'>");
}

void html_add_script_end() {
  TXBuffer += F("</script>");
}


//********************************************************************************
// Add a task select dropdown list
//********************************************************************************
void addTaskSelect(const String& name,  int choice)
{
  String deviceName;

  TXBuffer += F("<select id='selectwidth' name='");
  TXBuffer += name;
  TXBuffer += F("' onchange='return dept_onchange(frmselect)'>");

  for (byte x = 0; x < TASKS_MAX; x++)
  {
    deviceName = "";
    if (Settings.TaskDeviceNumber[x] != 0 )
    {
      byte DeviceIndex = getDeviceIndex(Settings.TaskDeviceNumber[x]);

      if (Plugin_id[DeviceIndex] != 0)
        deviceName = getPluginNameFromDeviceIndex(DeviceIndex);
    }
    LoadTaskSettings(x);
    TXBuffer += F("<option value='");
    TXBuffer += x;
    TXBuffer += '\'';
    if (choice == x)
      TXBuffer += F(" selected");
    if (Settings.TaskDeviceNumber[x] == 0)
      addDisabled();
    TXBuffer += '>';
    TXBuffer += x + 1;
    TXBuffer += F(" - ");
    TXBuffer += deviceName;
    TXBuffer += F(" - ");
    TXBuffer += ExtraTaskSettings.TaskDeviceName;
    TXBuffer += F("</option>");
  }
}



//********************************************************************************
// Add a Value select dropdown list, based on TaskIndex
//********************************************************************************
void addTaskValueSelect(const String& name, int choice, byte TaskIndex)
{
  TXBuffer += F("<select id='selectwidth' name='");
  TXBuffer += name;
  TXBuffer += "'>";

  byte DeviceIndex = getDeviceIndex(Settings.TaskDeviceNumber[TaskIndex]);

  for (byte x = 0; x < Device[DeviceIndex].ValueCount; x++)
  {
    TXBuffer += F("<option value='");
    TXBuffer += x;
    TXBuffer += '\'';
    if (choice == x)
      TXBuffer += F(" selected");
    TXBuffer += '>';
    TXBuffer += ExtraTaskSettings.TaskDeviceValueNames[x];
    TXBuffer += F("</option>");
  }
}



//********************************************************************************
// Web Interface log page
//********************************************************************************
void handle_log() {
  if (!isLoggedIn()) return;
  navMenuIndex = MENU_INDEX_TOOLS;
  TXBuffer.startStream();
  sendHeadandTail_stdtemplate(_HEAD);

  html_table_class_normal();
  TXBuffer += F("<TR><TH id=\"headline\" align=\"left\">Log");
  addCopyButton(F("copyText"), "", F("Copy log to clipboard"));
  TXBuffer += F("</TR></table><div  id='current_loglevel' style='font-weight: bold;'>Logging: </div><div class='logviewer' id='copyText_1'></div>");
  TXBuffer += F("Autoscroll: ");
  addCheckBox(F("autoscroll"), true);
  TXBuffer += F("<BR></body>");

  html_add_script(true);
  TXBuffer += DATA_FETCH_AND_PARSE_LOG_JS;
  html_add_script_end();

  sendHeadandTail_stdtemplate(_TAIL);
  TXBuffer.endStream();
  }

//********************************************************************************
// Web Interface JSON log page
//********************************************************************************
void handle_log_JSON() {
  if (!isLoggedIn()) return;
  TXBuffer.startJsonStream();
  String webrequest = WebServer.arg(F("view"));
  TXBuffer += F("{\"Log\": {");
  if (webrequest == F("legend")) {
    TXBuffer += F("\"Legend\": [");
    for (byte i = 0; i < LOG_LEVEL_NRELEMENTS; ++i) {
      if (i != 0)
        TXBuffer += ',';
      TXBuffer += '{';
      int loglevel;
      stream_next_json_object_value(F("label"), getLogLevelDisplayStringFromIndex(i, loglevel));
      stream_last_json_object_value(F("loglevel"), String(loglevel));
    }
    TXBuffer += F("],\n");
  }
  TXBuffer += F("\"Entries\": [");
  bool logLinesAvailable = true;
  int nrEntries = 0;
  unsigned long firstTimeStamp = 0;
  unsigned long lastTimeStamp = 0;
  while (logLinesAvailable) {
    String reply = Logging.get_logjson_formatted(logLinesAvailable, lastTimeStamp);
    if (reply.length() > 0) {
      TXBuffer += reply;
      if (nrEntries == 0) {
        firstTimeStamp = lastTimeStamp;
      }
      ++nrEntries;
    }
    // Do we need to do something here and maybe limit number of lines at once?
  }
  TXBuffer += F("],\n");
  long logTimeSpan = timeDiff(firstTimeStamp, lastTimeStamp);
  long refreshSuggestion = 1000;
  long newOptimum = 1000;
  if (nrEntries > 2 && logTimeSpan > 1) {
    // May need to lower the TTL for refresh when time needed
    // to fill half the log is lower than current TTL
    newOptimum = logTimeSpan * (LOG_STRUCT_MESSAGE_LINES / 2);
    newOptimum = newOptimum / (nrEntries - 1);
  }
  if (newOptimum < refreshSuggestion) refreshSuggestion = newOptimum;
  if (refreshSuggestion < 100) {
    // Reload times no lower than 100 msec.
    refreshSuggestion = 100;
  }
  stream_next_json_object_value(F("TTL"), String(refreshSuggestion));
  stream_next_json_object_value(F("timeHalfBuffer"), String(newOptimum));
  stream_next_json_object_value(F("nrEntries"), String(nrEntries));
  stream_next_json_object_value(F("SettingsWebLogLevel"), String(Settings.WebLogLevel));
  stream_last_json_object_value(F("logTimeSpan"), String(logTimeSpan));
  TXBuffer += F("}\n");
  TXBuffer.endStream();
  updateLogLevelCache();
}

//********************************************************************************
// Web Interface debug page
//********************************************************************************
void handle_tools() {
  if (!isLoggedIn()) return;
  navMenuIndex = MENU_INDEX_TOOLS;
  TXBuffer.startStream();
  sendHeadandTail_stdtemplate(_HEAD);

  String webrequest = WebServer.arg(F("cmd"));

  TXBuffer += F("<form>");
  html_table_class_normal();

  addFormHeader(F("Tools"));

  addFormSubHeader(F("Command"));
  html_TR_TD();
  TXBuffer += F("<TR><TD style='width: 180px'>");
  TXBuffer += F("<input class='wide' type='text' name='cmd' value='");
  TXBuffer += webrequest;
  TXBuffer += "'>";
  html_TD();
  addSubmitButton();
  addHelpButton(F("ESPEasy_Command_Reference"));
  html_TR_TD();

  printToWeb = true;
  printWebString = "";

  if (webrequest.length() > 0)
  {
    struct EventStruct TempEvent;
    webrequest=parseTemplate(webrequest,webrequest.length());  //@giig1967g: parseTemplate before executing the command
    parseCommandString(&TempEvent, webrequest);
    TempEvent.Source = VALUE_SOURCE_WEB_FRONTEND;
    if (!PluginCall(PLUGIN_WRITE, &TempEvent, webrequest))
      ExecuteCommand(VALUE_SOURCE_WEB_FRONTEND, webrequest.c_str());
  }

  if (printWebString.length() > 0)
  {
    TXBuffer += F("<TR><TD colspan='2'>Command Output<BR><textarea readonly rows='10' wrap='on'>");
    TXBuffer += printWebString;
    TXBuffer += F("</textarea>");
  }

  addFormSubHeader(F("System"));

  html_TR_TD_height(30);
  addWideButton(F("/?cmd=reboot"), F("Reboot"), "");
  html_TD();
  TXBuffer += F("Reboots ESP");

  html_TR_TD_height(30);
  addWideButton(F("log"), F("Log"), "");
  html_TD();
  TXBuffer += F("Open log output");

  html_TR_TD_height(30);
  addWideButton(F("sysinfo"), F("Info"), "");
  html_TD();
  TXBuffer += F("Open system info page");

  html_TR_TD_height(30);
  addWideButton(F("advanced"), F("Advanced"), "");
  html_TD();
  TXBuffer += F("Open advanced settings");

  html_TR_TD_height(30);
  addWideButton(F("json"), F("Show JSON"), "");
  html_TD();
  TXBuffer += F("Open JSON output");

  html_TR_TD_height(30);
  addWideButton(F("timingstats"), F("Timing stats"), "");
  html_TD();
  TXBuffer += F("Open timing statistics of system");

  html_TR_TD_height(30);
  addWideButton(F("pinstates"), F("Pin state buffer"), "");
  html_TD();
  TXBuffer += F("Show Pin state buffer");

  html_TR_TD_height(30);
  addWideButton(F("sysvars"), F("System Variables"), "");
  html_TD();
  TXBuffer += F("Show all system variables and conversions");

  addFormSubHeader(F("Wifi"));

  html_TR_TD_height(30);
  addWideButton(F("/?cmd=wificonnect"), F("Connect"), "");
  html_TD();
  TXBuffer += F("Connects to known Wifi network");

  html_TR_TD_height(30);
  addWideButton(F("/?cmd=wifidisconnect"), F("Disconnect"), "");
  html_TD();
  TXBuffer += F("Disconnect from wifi network");

  html_TR_TD_height(30);
  addWideButton(F("wifiscanner"), F("Scan"), "");
  html_TD();
  TXBuffer += F("Scan for wifi networks");

  addFormSubHeader(F("Interfaces"));

  html_TR_TD_height(30);
  addWideButton(F("i2cscanner"), F("I2C Scan"), "");
  html_TD();
  TXBuffer += F("Scan for I2C devices");

  addFormSubHeader(F("Settings"));

  html_TR_TD_height(30);
  addWideButton(F("upload"), F("Load"), "");
  html_TD();
  TXBuffer += F("Loads a settings file");
  addFormNote(F("(File MUST be renamed to \"config.dat\" before upload!)"));

  html_TR_TD_height(30);
  addWideButton(F("download"), F("Save"), "");
  html_TD();
  TXBuffer += F("Saves a settings file");

#if defined(ESP8266)
  {
    {
      uint32_t maxSketchSize;
      bool use2step;
      if (OTA_possible(maxSketchSize, use2step)) {
        addFormSubHeader(F("Firmware"));
        html_TR_TD_height(30);
        addWideButton(F("update"), F("Load"), "");
        addHelpButton(F("EasyOTA"));
        html_TD();
        TXBuffer += F("Load a new firmware");
        if (use2step) {
          TXBuffer += F(" <b>WARNING</b> only use 2-step OTA update and sketch < ");
        } else {
          TXBuffer += F(" Max sketch size: ");
        }
        TXBuffer += maxSketchSize / 1024;
        TXBuffer += F(" kB");
      }
    }
  }
#endif

  addFormSubHeader(F("Filesystem"));

  html_TR_TD_height(30);
  addWideButton(F("filelist"), F("Flash"), "");
  html_TD();
  TXBuffer += F("Show files on internal flash");

  html_TR_TD_height(30);
  addWideButton(F("/factoryreset"), F("Factory Reset"), "");
  html_TD();
  TXBuffer += F("Select pre-defined configuration or full erase of settings");

#ifdef FEATURE_SD
  html_TR_TD_height(30);
  addWideButton(F("SDfilelist"), F("SD Card"), "");
  html_TD();
  TXBuffer += F("Show files on SD-Card");
#endif

  html_end_table();
  html_end_form();
  sendHeadandTail_stdtemplate(_TAIL);
  TXBuffer.endStream();
  printWebString = "";
  printToWeb = false;
}


//********************************************************************************
// Web Interface pin state list
//********************************************************************************
void handle_pinstates() {
  checkRAM(F("handle_pinstates"));
  if (!isLoggedIn()) return;
  navMenuIndex = MENU_INDEX_TOOLS;
  TXBuffer.startStream();
  sendHeadandTail_stdtemplate(_HEAD);

  //addFormSubHeader(F("Pin state table<TR>"));

  html_table_class_multirow();
  html_TR();
  html_table_header(F("Plugin"), F("Official_plugin_list"), 0);
  html_table_header("GPIO");
  html_table_header("Mode");
  html_table_header(F("Value/State"));
  html_table_header(F("Task"));
  html_table_header(F("Monitor"));
  html_table_header(F("Command"));
  html_table_header("Init");
  for (auto it=globalMapPortStatus.begin(); it!=globalMapPortStatus.end(); ++it)
  {
    html_TR_TD(); TXBuffer += "P";
    const uint16_t plugin = getPluginFromKey(it->first);
    const uint16_t port = getPortFromKey(it->first);

    if (plugin < 100)
    {
      TXBuffer += '0';
    }
    if (plugin < 10)
    {
      TXBuffer += '0';
    }
    TXBuffer += plugin;
    html_TD();
    TXBuffer += port;
    html_TD();
    byte mode = it->second.mode;
    TXBuffer += getPinModeString(mode);
    html_TD();
    TXBuffer += it->second.state;
    html_TD();
    TXBuffer += it->second.task;
    html_TD();
    TXBuffer += it->second.monitor;
    html_TD();
    TXBuffer += it->second.command;
    html_TD();
    TXBuffer += it->second.portstatus_init;
  }


/*
  html_table_header(F("Plugin"), F("Official_plugin_list"), 0);
  html_table_header("GPIO");
  html_table_header("Mode");
  html_table_header(F("Value/State"));
  for (byte x = 0; x < PINSTATE_TABLE_MAX; x++)
    if (pinStates[x].plugin != 0)
    {
      html_TR_TD(); TXBuffer += "P";
      if (pinStates[x].plugin < 100)
      {
        TXBuffer += '0';
      }
      if (pinStates[x].plugin < 10)
      {
        TXBuffer += '0';
      }
      TXBuffer += pinStates[x].plugin;
      html_TD();
      TXBuffer += pinStates[x].index;
      html_TD();
      byte mode = pinStates[x].mode;
      TXBuffer += getPinModeString(mode);
      html_TD();
      TXBuffer += pinStates[x].value;
    }
*/
    html_end_table();
    sendHeadandTail_stdtemplate(_TAIL);
    TXBuffer.endStream();
}


//********************************************************************************
// Web Interface I2C scanner
//********************************************************************************
void handle_i2cscanner() {
  checkRAM(F("handle_i2cscanner"));
  if (!isLoggedIn()) return;
  navMenuIndex = MENU_INDEX_TOOLS;
  TXBuffer.startStream();
  sendHeadandTail_stdtemplate(_HEAD);

  char *TempString = (char*)malloc(80);

  html_table_class_multirow();
  html_table_header(F("I2C Addresses in use"));
  html_table_header(F("Supported devices"));

  byte error, address;
  int nDevices;
  nDevices = 0;
  for (address = 1; address <= 127; address++ )
  {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0)
    {
      TXBuffer += "<TR><TD>";
      TXBuffer += formatToHex(address);
      TXBuffer += "<TD>";
      switch (address)
      {
        case 0x20:
        case 0x21:
        case 0x22:
        case 0x25:
        case 0x26:
        case 0x27:
          TXBuffer += F("PCF8574<BR>MCP23017<BR>LCD");
          break;
        case 0x23:
          TXBuffer += F("PCF8574<BR>MCP23017<BR>LCD<BR>BH1750");
          break;
        case 0x24:
          TXBuffer += F("PCF8574<BR>MCP23017<BR>LCD<BR>PN532");
          break;
        case 0x29:
          TXBuffer += F("TSL2561");
          break;
        case 0x38:
        case 0x3A:
        case 0x3B:
        case 0x3E:
        case 0x3F:
          TXBuffer += F("PCF8574A");
          break;
        case 0x39:
          TXBuffer += F("PCF8574A<BR>TSL2561<BR>APDS9960");
          break;
        case 0x3C:
        case 0x3D:
          TXBuffer += F("PCF8574A<BR>OLED");
          break;
        case 0x40:
          TXBuffer += F("SI7021<BR>HTU21D<BR>INA219<BR>PCA9685");
          break;
        case 0x41:
        case 0x42:
        case 0x43:
          TXBuffer += F("INA219");
          break;
        case 0x44:
        case 0x45:
          TXBuffer += F("SHT30/31/35");
          break;
        case 0x48:
        case 0x4A:
        case 0x4B:
          TXBuffer += F("PCF8591<BR>ADS1115<BR>LM75A");
          break;
        case 0x49:
          TXBuffer += F("PCF8591<BR>ADS1115<BR>TSL2561<BR>LM75A");
          break;
        case 0x4C:
        case 0x4E:
        case 0x4F:
          TXBuffer += F("PCF8591<BR>LM75A");
          break;
        case 0x4D:
          TXBuffer += F("PCF8591<BR>MCP3221<BR>LM75A");
          break;
        case 0x5A:
          TXBuffer += F("MLX90614<BR>MPR121");
          break;
        case 0x5B:
          TXBuffer += F("MPR121");
          break;
        case 0x5C:
          TXBuffer += F("DHT12<BR>AM2320<BR>BH1750<BR>MPR121");
          break;
        case 0x5D:
          TXBuffer += F("MPR121");
          break;
        case 0x60:
          TXBuffer += F("Adafruit Motorshield v2<BR>SI1145");
          break;
        case 0x70:
          TXBuffer += F("Adafruit Motorshield v2 (Catchall)<BR>HT16K33");
          break;
        case 0x71:
        case 0x72:
        case 0x73:
        case 0x74:
        case 0x75:
          TXBuffer += F("HT16K33");
          break;
        case 0x76:
          TXBuffer += F("BME280<BR>BMP280<BR>MS5607<BR>MS5611<BR>HT16K33");
          break;
        case 0x77:
          TXBuffer += F("BMP085<BR>BMP180<BR>BME280<BR>BMP280<BR>MS5607<BR>MS5611<BR>HT16K33");
          break;
        case 0x7f:
          TXBuffer += F("Arduino PME");
          break;
      }
      nDevices++;
    }
    else if (error == 4)
    {
      html_TR_TD(); TXBuffer += F("Unknown error at address ");
      TXBuffer += formatToHex(address);
    }
  }

  if (nDevices == 0)
    TXBuffer += F("<TR>No I2C devices found");

  html_end_table();
  sendHeadandTail_stdtemplate(_TAIL);
  TXBuffer.endStream();
  free(TempString);
}


//********************************************************************************
// Web Interface Wifi scanner
//********************************************************************************
void handle_wifiscanner() {
  checkRAM(F("handle_wifiscanner"));
  if (!isLoggedIn()) return;
  navMenuIndex = MENU_INDEX_TOOLS;
  TXBuffer.startStream();
  sendHeadandTail_stdtemplate(_HEAD);
  html_table_class_multirow();
  html_TR();
  html_table_header("SSID");
  html_table_header(F("BSSID"));
  html_table_header("info");

  int n = WiFi.scanNetworks(false, true);
  if (n == 0)
    TXBuffer += F("No Access Points found");
  else
  {
    for (int i = 0; i < n; ++i)
    {
      html_TR_TD();
      TXBuffer += formatScanResult(i, "<TD>");
    }
  }

  html_end_table();
  sendHeadandTail_stdtemplate(_TAIL);
  TXBuffer.endStream();
}


//********************************************************************************
// Web Interface login page
//********************************************************************************
void handle_login() {
  checkRAM(F("handle_login"));
  if (!clientIPallowed()) return;
  TXBuffer.startStream();
  sendHeadandTail_stdtemplate(_HEAD);

  String webrequest = WebServer.arg(F("password"));
  TXBuffer += F("<form method='post'>");
  html_table_class_normal();
  TXBuffer += F("<TR><TD>Password<TD>");
  TXBuffer += F("<input class='wide' type='password' name='password' value='");
  TXBuffer += webrequest;
  TXBuffer += "'>";
  html_TR_TD();
  html_TD();
  addSubmitButton();
  html_TR_TD();
  html_end_table();
  html_end_form();

  if (webrequest.length() != 0)
  {
    char command[80];
    command[0] = 0;
    webrequest.toCharArray(command, 80);

    // compare with stored password and set timer if there's a match
    if ((strcasecmp(command, SecuritySettings.Password) == 0) || (SecuritySettings.Password[0] == 0))
    {
      WebLoggedIn = true;
      WebLoggedInTimer = 0;
      TXBuffer = F("<script>window.location = '.'</script>");
    }
    else
    {
      TXBuffer += F("Invalid password!");
      if (Settings.UseRules)
      {
        String event = F("Login#Failed");
        rulesProcessing(event);
      }
    }
  }

  sendHeadandTail_stdtemplate(_TAIL);
  TXBuffer.endStream();
  printWebString = "";
  printToWeb = false;
}


//********************************************************************************
// Web Interface control page (no password!)
//********************************************************************************
void handle_control() {
  checkRAM(F("handle_control"));
  if (!clientIPallowed()) return;
  //TXBuffer.startStream(true); // true= json
  // sendHeadandTail_stdtemplate(_HEAD);
  String webrequest = WebServer.arg(F("cmd"));

  // in case of event, store to buffer and return...
  String command = parseString(webrequest, 1);
  addLog(LOG_LEVEL_INFO,String(F("HTTP: ")) + webrequest);
  webrequest=parseTemplate(webrequest,webrequest.length());
  addLog(LOG_LEVEL_DEBUG,String(F("HTTP after parseTemplate: ")) + webrequest);

  bool handledCmd = false;
  if (command == F("event"))
  {
    eventBuffer = webrequest.substring(6);
    handledCmd = true;
  }
  else if (command.equalsIgnoreCase(F("taskrun")) ||
           command.equalsIgnoreCase(F("taskvalueset")) ||
           command.equalsIgnoreCase(F("taskvaluetoggle")) ||
           command.equalsIgnoreCase(F("let")) ||
           command.equalsIgnoreCase(F("logPortStatus")) ||
           command.equalsIgnoreCase(F("jsonportstatus")) ||
           command.equalsIgnoreCase(F("rules"))) {
    ExecuteCommand(VALUE_SOURCE_HTTP,webrequest.c_str());
    handledCmd = true;
  }

  if (handledCmd) {
    TXBuffer.startStream("*");
    TXBuffer += "OK";
    TXBuffer.endStream();
	  return;
  }

  struct EventStruct TempEvent;
  parseCommandString(&TempEvent, webrequest);
  TempEvent.Source = VALUE_SOURCE_HTTP;

  printToWeb = true;
  printWebString = "";

  bool unknownCmd = false;
  if (PluginCall(PLUGIN_WRITE, &TempEvent, webrequest));
  else if (remoteConfig(&TempEvent, webrequest));
  else unknownCmd = true;

  if (printToWebJSON) // it is setted in PLUGIN_WRITE (SendStatus)
    TXBuffer.startJsonStream();
  else
    TXBuffer.startStream();

  if (unknownCmd)
	TXBuffer += F("Unknown or restricted command!");
  else
	TXBuffer += printWebString;

  TXBuffer.endStream();

  printWebString = "";
  printToWeb = false;
  printToWebJSON = false;
}

/*********************************************************************************************\
   Streaming versions directly to TXBuffer
  \*********************************************************************************************/

void stream_to_json_object_value(const String& object, const String& value) {
  TXBuffer += '\"';
  TXBuffer += object;
  TXBuffer += "\":";
  if (value.length() == 0 || !isFloat(value)) {
    TXBuffer += '\"';
    TXBuffer += value;
    TXBuffer += '\"';
  } else {
    TXBuffer += value;
  }
}

String jsonBool(bool value) {
  return toString(value);
}

// Add JSON formatted data directly to the TXbuffer, including a trailing comma.
void stream_next_json_object_value(const String& object, const String& value) {
  TXBuffer += to_json_object_value(object, value);
  TXBuffer += ",\n";
}

// Add JSON formatted data directly to the TXbuffer, including a closing '}'
void stream_last_json_object_value(const String& object, const String& value) {
  TXBuffer += to_json_object_value(object, value);
  TXBuffer += "\n}";
}


//********************************************************************************
// Web Interface JSON page (no password!)
//********************************************************************************
void handle_json()
{
  const int taskNr = getFormItemInt(F("tasknr"), -1);
  const bool showSpecificTask = taskNr > 0;
  bool showSystem = true;
  bool showWifi = true;
  bool showDataAcquisition = true;
  bool showTaskDetails = true;
  bool showNodes = true;
  {
    String view = WebServer.arg("view");
    if (view.length() != 0) {
      if (view == F("sensorupdate")) {
        showSystem = false;
        showWifi = false;
        showDataAcquisition = false;
        showTaskDetails = false;
        showNodes =false;
      }
    }
  }
  TXBuffer.startJsonStream();
  if (!showSpecificTask)
  {
    TXBuffer += '{';
    if (showSystem) {
      TXBuffer += F("\"System\":{\n");
      stream_next_json_object_value(F("Build"), String(BUILD));
      stream_next_json_object_value(F("Git Build"), String(BUILD_GIT));
      stream_next_json_object_value(F("System libraries"), getSystemLibraryString());
      stream_next_json_object_value(F("Plugins"), String(deviceCount + 1));
      stream_next_json_object_value(F("Plugin description"), getPluginDescriptionString());
      stream_next_json_object_value(F("Local time"), getDateTimeString('-',':',' '));
      stream_next_json_object_value(F("Unit"), String(Settings.Unit));
      stream_next_json_object_value(F("Name"), String(Settings.Name));
      stream_next_json_object_value(F("Uptime"), String(wdcounter / 2));
      stream_next_json_object_value(F("Last boot cause"), getLastBootCauseString());
      stream_next_json_object_value(F("Reset Reason"), getResetReasonString());

      if (wdcounter > 0)
      {
          stream_next_json_object_value(F("Load"), String(getCPUload()));
          stream_next_json_object_value(F("Load LC"), String(getLoopCountPerSec()));
      }

      stream_last_json_object_value(F("Free RAM"), String(ESP.getFreeHeap()));
      TXBuffer += ",\n";
    }
    if (showWifi) {
      TXBuffer += F("\"WiFi\":{\n");
      #if defined(ESP8266)
        stream_next_json_object_value(F("Hostname"), WiFi.hostname());
      #endif
      stream_next_json_object_value(F("IP config"), useStaticIP() ? F("Static") : F("DHCP"));
      stream_next_json_object_value(F("IP"), WiFi.localIP().toString());
      stream_next_json_object_value(F("Subnet Mask"), WiFi.subnetMask().toString());
      stream_next_json_object_value(F("Gateway IP"), WiFi.gatewayIP().toString());
      stream_next_json_object_value(F("MAC address"), WiFi.macAddress());
      stream_next_json_object_value(F("DNS 1"), WiFi.dnsIP(0).toString());
      stream_next_json_object_value(F("DNS 2"), WiFi.dnsIP(1).toString());
      stream_next_json_object_value(F("SSID"), WiFi.SSID());
      stream_next_json_object_value(F("BSSID"), WiFi.BSSIDstr());
      stream_next_json_object_value(F("Channel"), String(WiFi.channel()));
      stream_next_json_object_value(F("Connected msec"), String(timeDiff(lastConnectMoment, millis())));
      stream_next_json_object_value(F("Last Disconnect Reason"), String(lastDisconnectReason));
      stream_next_json_object_value(F("Last Disconnect Reason str"), getLastDisconnectReason());
      stream_next_json_object_value(F("Number reconnects"), String(wifi_reconnects));
      stream_last_json_object_value(F("RSSI"), String(WiFi.RSSI()));
      TXBuffer += ",\n";
    }
    if(showNodes) {
      bool comma_between=false;
      for (NodesMap::iterator it = Nodes.begin(); it != Nodes.end(); ++it)
      {
        if (it->second.ip[0] != 0)
        {
          if( comma_between ) {
            TXBuffer += ',';
          } else {
            comma_between=true;
            TXBuffer += F("\"nodes\":[\n"); // open json array if >0 nodes
          }

          TXBuffer += '{';
          stream_next_json_object_value(F("nr"), String(it->first));
          stream_next_json_object_value(F("name"),
              (it->first != Settings.Unit) ? it->second.nodeName : Settings.Name);

          if (it->second.build) {
            stream_next_json_object_value(F("build"), String(it->second.build));
          }

          if (it->second.nodeType) {
            String platform;
            switch (it->second.nodeType)
            {
              case NODE_TYPE_ID_ESP_EASY_STD:     platform = F("ESP Easy");      break;
              case NODE_TYPE_ID_ESP_EASYM_STD:    platform = F("ESP Easy Mega"); break;
              case NODE_TYPE_ID_ESP_EASY32_STD:   platform = F("ESP Easy 32");   break;
              case NODE_TYPE_ID_ARDUINO_EASY_STD: platform = F("Arduino Easy");  break;
              case NODE_TYPE_ID_NANO_EASY_STD:    platform = F("Nano Easy");     break;
            }
            if (platform.length() > 0)
              stream_next_json_object_value(F("platform"), platform);
          }
          stream_next_json_object_value(F("ip"), it->second.ip.toString());
          stream_last_json_object_value(F("age"),  String( it->second.age ));
        } // if node info exists
      } // for loop
      if(comma_between) {
        TXBuffer += F("],\n"); // close array if >0 nodes
      }
    }
  }

  byte firstTaskIndex = 0;
  byte lastTaskIndex = TASKS_MAX - 1;
  if (showSpecificTask)
  {
    firstTaskIndex = taskNr - 1;
    lastTaskIndex = taskNr - 1;
  }
  byte lastActiveTaskIndex = 0;
  for (byte TaskIndex = firstTaskIndex; TaskIndex <= lastTaskIndex; TaskIndex++) {
    if (Settings.TaskDeviceNumber[TaskIndex])
      lastActiveTaskIndex = TaskIndex;
  }

  if (!showSpecificTask) TXBuffer += F("\"Sensors\":[\n");
  unsigned long ttl_json = 60; // The shortest interval per enabled task (with output values) in seconds
  for (byte TaskIndex = firstTaskIndex; TaskIndex <= lastActiveTaskIndex; TaskIndex++)
  {
    if (Settings.TaskDeviceNumber[TaskIndex])
    {
      byte DeviceIndex = getDeviceIndex(Settings.TaskDeviceNumber[TaskIndex]);
      const unsigned long taskInterval = Settings.TaskDeviceTimer[TaskIndex];
      LoadTaskSettings(TaskIndex);
      TXBuffer += F("{\n");
      // For simplicity, do the optional values first.
      if (Device[DeviceIndex].ValueCount != 0) {
        if (ttl_json > taskInterval && taskInterval > 0 && Settings.TaskDeviceEnabled[TaskIndex]) {
          ttl_json = taskInterval;
        }
        TXBuffer += F("\"TaskValues\": [\n");
        for (byte x = 0; x < Device[DeviceIndex].ValueCount; x++)
        {
          TXBuffer += '{';
          stream_next_json_object_value(F("ValueNumber"), String(x + 1));
          stream_next_json_object_value(F("Name"), String(ExtraTaskSettings.TaskDeviceValueNames[x]));
          stream_next_json_object_value(F("NrDecimals"), String(ExtraTaskSettings.TaskDeviceValueDecimals[x]));
          stream_last_json_object_value(F("Value"), formatUserVarNoCheck(TaskIndex, x));
          if (x < (Device[DeviceIndex].ValueCount - 1))
            TXBuffer += ",\n";
        }
        TXBuffer += F("],\n");
      }
      if (showSpecificTask) {
        stream_next_json_object_value(F("TTL"), String(ttl_json * 1000));
      }
      if (showDataAcquisition) {
        TXBuffer += F("\"DataAcquisition\": [\n");
        for (byte x = 0; x < CONTROLLER_MAX; x++)
        {
          TXBuffer += '{';
          stream_next_json_object_value(F("Controller"), String(x + 1));
          stream_next_json_object_value(F("IDX"), String(Settings.TaskDeviceID[x][TaskIndex]));
          stream_last_json_object_value(F("Enabled"), jsonBool(Settings.TaskDeviceSendData[x][TaskIndex]));
          if (x < (CONTROLLER_MAX - 1))
            TXBuffer += ",\n";
        }
        TXBuffer += F("],\n");
      }
      if (showTaskDetails) {
        stream_next_json_object_value(F("TaskInterval"), String(taskInterval));
        stream_next_json_object_value(F("Type"), getPluginNameFromDeviceIndex(DeviceIndex));
        stream_next_json_object_value(F("TaskName"), String(ExtraTaskSettings.TaskDeviceName));
      }
      stream_next_json_object_value(F("TaskEnabled"), jsonBool(Settings.TaskDeviceEnabled[TaskIndex]));
      stream_last_json_object_value(F("TaskNumber"), String(TaskIndex + 1));
      if (TaskIndex != lastActiveTaskIndex)
        TXBuffer += ',';
      TXBuffer += '\n';
    }
  }
  if (!showSpecificTask) {
    TXBuffer += F("],\n");
    stream_last_json_object_value(F("TTL"), String(ttl_json * 1000));
  }

  TXBuffer.endStream();
}

//********************************************************************************
// JSON formatted timing statistics
//********************************************************************************

void stream_timing_stats_json(unsigned long count, unsigned long minVal, unsigned long maxVal, float avg) {
  stream_next_json_object_value(F("count"), String(count));
  stream_next_json_object_value(F("min"), String(minVal));
  stream_next_json_object_value(F("max"), String(maxVal));
  stream_next_json_object_value(F("avg"), String(avg));
}

void stream_plugin_function_timing_stats_json(
      const String& object,
      unsigned long count, unsigned long minVal, unsigned long maxVal, float avg) {
  TXBuffer += "{\"";
  TXBuffer += object;
  TXBuffer += "\":{";
  stream_timing_stats_json(count, minVal, maxVal, avg);
  stream_last_json_object_value(F("unit"), F("usec"));
}

void stream_plugin_timing_stats_json(int pluginId) {
  String P_name = "";
  Plugin_ptr[pluginId](PLUGIN_GET_DEVICENAME, NULL, P_name);
  TXBuffer += '{';
  stream_next_json_object_value(F("name"), P_name);
  stream_next_json_object_value(F("id"), String(pluginId));
  stream_json_start_array(F("function"));
}

void stream_json_start_array(const String& label) {
  TXBuffer += '\"';
  TXBuffer += label;
  TXBuffer += F("\": [\n");
}

void stream_json_end_array_element(bool isLast) {
  if (isLast) {
    TXBuffer += "]\n";
  } else {
    TXBuffer += ",\n";
  }
}

void stream_json_end_object_element(bool isLast) {
  TXBuffer += '}';
  if (!isLast) {
    TXBuffer += ',';
  }
  TXBuffer += '\n';
}


void handle_timingstats_json() {
  TXBuffer.startJsonStream();
  TXBuffer += '{';
  jsonStatistics(false);
  TXBuffer += '}';
  TXBuffer.endStream();
}

//********************************************************************************
// HTML table formatted timing statistics
//********************************************************************************
void format_using_threshhold(unsigned long value) {
  float value_msec = value / 1000.0;
  if (value > TIMING_STATS_THRESHOLD) {
    html_B(String(value_msec, 3));
  } else {
    TXBuffer += String(value_msec, 3);
  }
}

void stream_html_timing_stats(const TimingStats& stats, long timeSinceLastReset) {
    unsigned long minVal, maxVal;
    unsigned int c = stats.getMinMax(minVal, maxVal);

    html_TD();
    TXBuffer += c;
    html_TD();
    float call_per_sec = static_cast<float>(c) / static_cast<float>(timeSinceLastReset) * 1000.0;
    TXBuffer += call_per_sec;
    html_TD();
    format_using_threshhold(minVal);
    html_TD();
    format_using_threshhold(stats.getAvg());
    html_TD();
    format_using_threshhold(maxVal);
}



long stream_timing_statistics(bool clearStats) {
  long timeSinceLastReset = timePassedSince(timingstats_last_reset);
  for (auto& x: pluginStats) {
      if (!x.second.isEmpty()) {
          const int pluginId = x.first/32;
          String P_name = "";
          Plugin_ptr[pluginId](PLUGIN_GET_DEVICENAME, NULL, P_name);
          if (x.second.thresholdExceeded(TIMING_STATS_THRESHOLD)) {
            html_TR_TD_highlight();
          } else {
            html_TR_TD();
          }
          TXBuffer += F("P_");
          TXBuffer += pluginId + 1;
          TXBuffer += '_';
          TXBuffer += P_name;
          html_TD();
          TXBuffer += getPluginFunctionName(x.first%32);
          stream_html_timing_stats(x.second, timeSinceLastReset);
          if (clearStats) x.second.reset();
      }
  }
  for (auto& x: miscStats) {
      if (!x.second.isEmpty()) {
          if (x.second.thresholdExceeded(TIMING_STATS_THRESHOLD)) {
            html_TR_TD_highlight();
          } else {
            html_TR_TD();
          }
          TXBuffer += getMiscStatsName(x.first);
          html_TD();
          stream_html_timing_stats(x.second, timeSinceLastReset);
          if (clearStats) x.second.reset();
      }
  }
  if (clearStats) {
    timediff_calls = 0;
    timediff_cpu_cycles_total = 0;
    timingstats_last_reset = millis();
  }
  return timeSinceLastReset;
}

void handle_timingstats() {
  checkRAM(F("handle_timingstats"));
  navMenuIndex = MENU_INDEX_TOOLS;
  TXBuffer.startStream();
  sendHeadandTail_stdtemplate(_HEAD);
  html_table_class_multirow();
  html_TR();
  html_table_header(F("Description"));
  html_table_header(F("Function"));
  html_table_header(F("#calls"));
  html_table_header(F("call/sec"));
  html_table_header(F("min (ms)"));
  html_table_header(F("Avg (ms)"));
  html_table_header(F("max (ms)"));

  long timeSinceLastReset = stream_timing_statistics(true);
  html_end_table();

  html_table_class_normal();
  const float timespan = timeSinceLastReset / 1000.0;
  addFormHeader(F("Statistics"));
  addRowLabel(F("Start Period"));
  struct tm startPeriod = addSeconds(tm, -1.0 * timespan, false);
  TXBuffer += getDateTimeString(startPeriod, '-', ':', ' ', false);
  addRowLabel(F("Local Time"));
  TXBuffer += getDateTimeString('-', ':', ' ');
  addRowLabel(F("Time span"));
  TXBuffer += String(timespan);
  TXBuffer += " sec";
  html_end_table();

  sendHeadandTail_stdtemplate(_TAIL);
  TXBuffer.endStream();
}

//********************************************************************************
// Web Interface config page
//********************************************************************************
void handle_advanced() {
  checkRAM(F("handle_advanced"));
  if (!isLoggedIn()) return;
  navMenuIndex = MENU_INDEX_TOOLS;
  TXBuffer.startStream();
  sendHeadandTail_stdtemplate();

  int timezone = getFormItemInt(F("timezone"));
  int dststartweek = getFormItemInt(F("dststartweek"));
  int dststartdow = getFormItemInt(F("dststartdow"));
  int dststartmonth = getFormItemInt(F("dststartmonth"));
  int dststarthour = getFormItemInt(F("dststarthour"));
  int dstendweek = getFormItemInt(F("dstendweek"));
  int dstenddow = getFormItemInt(F("dstenddow"));
  int dstendmonth = getFormItemInt(F("dstendmonth"));
  int dstendhour = getFormItemInt(F("dstendhour"));
  String edit = WebServer.arg(F("edit"));


  if (edit.length() != 0)
  {
    Settings.MessageDelay = getFormItemInt(F("messagedelay"));
    Settings.IP_Octet = WebServer.arg(F("ip")).toInt();
    strncpy_webserver_arg(Settings.NTPHost, F("ntphost"));
    Settings.TimeZone = timezone;
    TimeChangeRule dst_start(dststartweek, dststartdow, dststartmonth, dststarthour, timezone);
    if (dst_start.isValid()) { Settings.DST_Start = dst_start.toFlashStoredValue(); }
    TimeChangeRule dst_end(dstendweek, dstenddow, dstendmonth, dstendhour, timezone);
    if (dst_end.isValid()) { Settings.DST_End = dst_end.toFlashStoredValue(); }
    str2ip(WebServer.arg(F("syslogip")).c_str(), Settings.Syslog_IP);
    Settings.UDPPort = getFormItemInt(F("udpport"));

    Settings.SyslogFacility = getFormItemInt(F("syslogfacility"));
    Settings.UseSerial = isFormItemChecked(F("useserial"));
    setLogLevelFor(LOG_TO_SYSLOG, getFormItemInt(F("sysloglevel")));
    setLogLevelFor(LOG_TO_SERIAL, getFormItemInt(F("serialloglevel")));
    setLogLevelFor(LOG_TO_WEBLOG, getFormItemInt(F("webloglevel")));
    setLogLevelFor(LOG_TO_SDCARD, getFormItemInt(F("sdloglevel")));
    Settings.UseValueLogger = isFormItemChecked(F("valuelogger"));
    Settings.BaudRate = getFormItemInt(F("baudrate"));
    Settings.UseNTP = isFormItemChecked(F("usentp"));
    Settings.DST = isFormItemChecked(F("dst"));
    Settings.WDI2CAddress = getFormItemInt(F("wdi2caddress"));
    Settings.UseSSDP = isFormItemChecked(F("usessdp"));
    Settings.WireClockStretchLimit = getFormItemInt(F("wireclockstretchlimit"));
    Settings.UseRules = isFormItemChecked(F("userules"));
    Settings.ConnectionFailuresThreshold = getFormItemInt(F("cft"));
    Settings.MQTTRetainFlag = isFormItemChecked(F("mqttretainflag"));
    Settings.ArduinoOTAEnable = isFormItemChecked(F("arduinootaenable"));
    Settings.UseRTOSMultitasking = isFormItemChecked(F("usertosmultitasking"));
    Settings.MQTTUseUnitNameAsClientId = isFormItemChecked(F("mqttuseunitnameasclientid"));
    Settings.uniqueMQTTclientIdReconnect(isFormItemChecked(F("uniquemqttclientidreconnect")));
    Settings.Latitude = getFormItemFloat(F("latitude"));
    Settings.Longitude = getFormItemFloat(F("longitude"));
    Settings.OldRulesEngine(isFormItemChecked(F("oldrulesengine")));

    addHtmlError(SaveSettings());
    if (Settings.UseNTP)
      initTime();
  }

  TXBuffer += F("<form  method='post'>");
  html_table_class_normal();

  addFormHeader(F("Advanced Settings"));

  addFormSubHeader(F("Rules Settings"));

  addFormCheckBox(F("Rules"), F("userules"), Settings.UseRules);
  addFormCheckBox(F("Old Engine"), F("oldrulesengine"), Settings.OldRulesEngine());

  addFormSubHeader(F("Controller Settings"));

  addFormCheckBox(F("MQTT Retain Msg"), F("mqttretainflag"), Settings.MQTTRetainFlag);
  addFormNumericBox( F("Message Interval"), F("messagedelay"), Settings.MessageDelay, 0, INT_MAX);
  addUnit(F("ms"));
  addFormCheckBox(F("MQTT use unit name as ClientId"), F("mqttuseunitnameasclientid"), Settings.MQTTUseUnitNameAsClientId);
  addFormCheckBox(F("MQTT change ClientId at reconnect"), F("uniquemqttclientidreconnect"), Settings.uniqueMQTTclientIdReconnect());

  addFormSubHeader(F("NTP Settings"));

  addFormCheckBox(F("Use NTP"), F("usentp"), Settings.UseNTP);
  addFormTextBox( F("NTP Hostname"), F("ntphost"), Settings.NTPHost, 63);

  addFormSubHeader(F("DST Settings"));
  addFormDstSelect(true, Settings.DST_Start);
  addFormDstSelect(false, Settings.DST_End);
  addFormNumericBox(F("Timezone Offset (UTC +)"), F("timezone"), Settings.TimeZone, -720, 840);   // UTC-12H ... UTC+14h
  addUnit(F("minutes"));
  addFormCheckBox(F("DST"), F("dst"), Settings.DST);

  addFormSubHeader(F("Location Settings"));
  addFormFloatNumberBox(F("Latitude"), F("latitude"), Settings.Latitude, -90.0, 90.0);
  addUnit(F("&deg;"));
  addFormFloatNumberBox(F("Longitude"), F("longitude"), Settings.Longitude, -180.0, 180.0);
  addUnit(F("&deg;"));

  addFormSubHeader(F("Log Settings"));

  addFormIPBox(F("Syslog IP"), F("syslogip"), Settings.Syslog_IP);
  addFormLogLevelSelect(F("Syslog Level"),      F("sysloglevel"),    Settings.SyslogLevel);
  addFormLogFacilitySelect(F("Syslog Facility"),F("syslogfacility"), Settings.SyslogFacility);
  addFormLogLevelSelect(F("Serial log Level"),  F("serialloglevel"), Settings.SerialLogLevel);
  addFormLogLevelSelect(F("Web log Level"),     F("webloglevel"),    Settings.WebLogLevel);

#ifdef FEATURE_SD
  addFormLogLevelSelect(F("SD Card log Level"), F("sdloglevel"),     Settings.SDLogLevel);

  addFormCheckBox(F("SD Card Value Logger"), F("valuelogger"), Settings.UseValueLogger);
#endif


  addFormSubHeader(F("Serial Settings"));

  addFormCheckBox(F("Enable Serial port"), F("useserial"), Settings.UseSerial);
  addFormNumericBox(F("Baud Rate"), F("baudrate"), Settings.BaudRate, 0, 1000000);


  addFormSubHeader(F("Inter-ESPEasy Network"));

  addFormNumericBox(F("UDP port"), F("udpport"), Settings.UDPPort, 0, 65535);


  //TODO sort settings in groups or move to other pages/groups
  addFormSubHeader(F("Special and Experimental Settings"));

  addFormNumericBox(F("Fixed IP Octet"), F("ip"), Settings.IP_Octet, 0, 255);

  addFormNumericBox(F("WD I2C Address"), F("wdi2caddress"), Settings.WDI2CAddress, 0, 127);
  TXBuffer += F(" (decimal)");

  addFormCheckBox_disabled(F("Use SSDP"), F("usessdp"), Settings.UseSSDP);

  addFormNumericBox(F("Connection Failure Threshold"), F("cft"), Settings.ConnectionFailuresThreshold, 0, 100);

  addFormNumericBox(F("I2C ClockStretchLimit"), F("wireclockstretchlimit"), Settings.WireClockStretchLimit);   //TODO define limits
  #if defined(FEATURE_ARDUINO_OTA)
  addFormCheckBox(F("Enable Arduino OTA"), F("arduinootaenable"), Settings.ArduinoOTAEnable);
  #endif
  #if defined(ESP32)
    addFormCheckBox_disabled(F("Enable RTOS Multitasking"), F("usertosmultitasking"), Settings.UseRTOSMultitasking);
  #endif

  addFormSeparator(2);

  html_TR_TD();
  html_TD();
  addSubmitButton();
  TXBuffer += F("<input type='hidden' name='edit' value='1'>");
  html_end_table();
  html_end_form();
  sendHeadandTail_stdtemplate(true);
  TXBuffer.endStream();
}

void addFormDstSelect(bool isStart, uint16_t choice) {
  String weekid  = isStart ? F("dststartweek")  : F("dstendweek");
  String dowid   = isStart ? F("dststartdow")   : F("dstenddow");
  String monthid = isStart ? F("dststartmonth") : F("dstendmonth");
  String hourid  = isStart ? F("dststarthour")  : F("dstendhour");

  String weeklabel  = isStart ? F("Start (week, dow, month)")  : F("End (week, dow, month)");
  String hourlabel  = isStart ? F("Start (localtime, e.g. 2h&rarr;3h)")  : F("End (localtime, e.g. 3h&rarr;2h)");

  String week[5] = {F("Last"), F("1st"), F("2nd"), F("3rd"), F("4th")};
  int weekValues[5] = {0, 1, 2, 3, 4};
  String dow[7] = {F("Sun"), F("Mon"), F("Tue"), F("Wed"), F("Thu"), F("Fri"), F("Sat")};
  int dowValues[7] = {1, 2, 3, 4, 5, 6, 7};
  String month[12] = {F("Jan"), F("Feb"), F("Mar"), F("Apr"), F("May"), F("Jun"), F("Jul"), F("Aug"), F("Sep"), F("Oct"), F("Nov"), F("Dec")};
  int monthValues[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

  uint16_t tmpstart(choice);
  uint16_t tmpend(choice);
  if (!TimeChangeRule(choice, 0).isValid()) {
    getDefaultDst_flash_values(tmpstart, tmpend);
  }
  TimeChangeRule rule(isStart ? tmpstart : tmpend, 0);
  addRowLabel(weeklabel);
  addSelector(weekid, 5, week, weekValues, NULL, rule.week, false);
  TXBuffer += F("<BR>");
  addSelector(dowid, 7, dow, dowValues, NULL, rule.dow, false);
  TXBuffer += F("<BR>");
  addSelector(monthid, 12, month, monthValues, NULL, rule.month, false);

  addFormNumericBox(hourlabel, hourid, rule.hour, 0, 23);
  addUnit(isStart ? F("hour &#x21b7;") : F("hour &#x21b6;"));
}

void addFormLogLevelSelect(const String& label, const String& id, int choice)
{
  addRowLabel(label);
  addLogLevelSelect(id, choice);
}

void addLogLevelSelect(const String& name, int choice)
{
  String options[LOG_LEVEL_NRELEMENTS + 1];
  int optionValues[LOG_LEVEL_NRELEMENTS + 1] = {0};
  options[0] = getLogLevelDisplayString(0);
  optionValues[0] = 0;
  for (int i = 0; i < LOG_LEVEL_NRELEMENTS; ++i) {
    options[i + 1] = getLogLevelDisplayStringFromIndex(i, optionValues[i + 1]);
  }
  addSelector(name, LOG_LEVEL_NRELEMENTS + 1, options, optionValues, NULL, choice, false);
}

void addFormLogFacilitySelect(const String& label, const String& id, int choice)
{
  addRowLabel(label);
  addLogFacilitySelect(id, choice);
}

void addLogFacilitySelect(const String& name, int choice)
{
  String options[12] = { F("Kernel"), F("User"), F("Daemon"), F("Message"), F("Local0"), F("Local1"), F("Local2"), F("Local3"), F("Local4"), F("Local5"), F("Local6"), F("Local7")};
  int optionValues[12] = { 0, 1, 3, 5, 16, 17, 18, 19, 20, 21, 22, 23 };
  addSelector(name, 12, options, optionValues, NULL, choice, false);
}


//********************************************************************************
// Login state check
//********************************************************************************
boolean isLoggedIn()
{
  if (!clientIPallowed()) return false;
  if (SecuritySettings.Password[0] == 0)
    WebLoggedIn = true;

  if (!WebLoggedIn)
  {
    WebServer.sendContent(F("HTTP/1.1 302 \r\nLocation: /login\r\n"));
  }
  else
  {
    WebLoggedInTimer = 0;
  }

  return WebLoggedIn;
}


//********************************************************************************
// Web Interface download page
//********************************************************************************
void handle_download()
{
  checkRAM(F("handle_download"));
  if (!isLoggedIn()) return;
  navMenuIndex = MENU_INDEX_TOOLS;
//  TXBuffer.startStream();
//  sendHeadandTail_stdtemplate();


  fs::File dataFile = SPIFFS.open(F(FILE_CONFIG), "r");
  if (!dataFile)
    return;

  String str = F("attachment; filename=config_");
  str += Settings.Name;
  str += "_U";
  str += Settings.Unit;
  str += F("_Build");
  str += BUILD;
  str += '_';
  if (Settings.UseNTP)
  {
    str += getDateTimeString('\0', '\0', '\0');
  }
  str += F(".dat");

  WebServer.sendHeader(F("Content-Disposition"), str);
  WebServer.streamFile(dataFile, F("application/octet-stream"));
  dataFile.close();
}


//********************************************************************************
// Web Interface upload page
//********************************************************************************
byte uploadResult = 0;
void handle_upload() {
  if (!isLoggedIn()) return;
  navMenuIndex = MENU_INDEX_TOOLS;
  TXBuffer.startStream();
  sendHeadandTail_stdtemplate();

  TXBuffer += F("<form enctype='multipart/form-data' method='post'><p>Upload settings file:<br><input type='file' name='datafile' size='40'></p><div><input class='button link' type='submit' value='Upload'></div><input type='hidden' name='edit' value='1'></form>");
  sendHeadandTail_stdtemplate(true);
  TXBuffer.endStream();
  printWebString = "";
  printToWeb = false;
}


//********************************************************************************
// Web Interface upload page
//********************************************************************************
void handle_upload_post() {
  checkRAM(F("handle_upload_post"));
  if (!isLoggedIn()) return;

  navMenuIndex = MENU_INDEX_TOOLS;
  TXBuffer.startStream();
  sendHeadandTail_stdtemplate();



  if (uploadResult == 1)
  {
    TXBuffer += F("Upload OK!<BR>You may need to reboot to apply all settings...");
    LoadSettings();
  }

  if (uploadResult == 2)
    TXBuffer += F("<font color=\"red\">Upload file invalid!</font>");

  if (uploadResult == 3)
    TXBuffer += F("<font color=\"red\">No filename!</font>");


  TXBuffer += F("Upload finished");
  sendHeadandTail_stdtemplate(true);
  TXBuffer.endStream();
  printWebString = "";
  printToWeb = false;
}


//********************************************************************************
// Web Interface upload handler
//********************************************************************************
fs::File uploadFile;
void handleFileUpload() {
  checkRAM(F("handleFileUpload"));
  if (!isLoggedIn()) return;

  static boolean valid = false;

  HTTPUpload& upload = WebServer.upload();

  if (upload.filename.c_str()[0] == 0)
  {
    uploadResult = 3;
    return;
  }

  if (upload.status == UPLOAD_FILE_START)
  {
    if (loglevelActiveFor(LOG_LEVEL_INFO)) {
      String log = F("Upload: START, filename: ");
      log += upload.filename;
      addLog(LOG_LEVEL_INFO, log);
    }
    valid = false;
    uploadResult = 0;
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    // first data block, if this is the config file, check PID/Version
    if (upload.totalSize == 0)
    {
      if (strcasecmp(upload.filename.c_str(), FILE_CONFIG) == 0)
      {
        struct TempStruct {
          unsigned long PID;
          int Version;
        } Temp;
        for (unsigned int x = 0; x < sizeof(struct TempStruct); x++)
        {
          byte b = upload.buf[x];
          memcpy((byte*)&Temp + x, &b, 1);
        }
        if (Temp.Version == VERSION && Temp.PID == ESP_PROJECT_PID)
          valid = true;
      }
      else
      {
        // other files are always valid...
        valid = true;
      }
      if (valid)
      {
        // once we're safe, remove file and create empty one...
        SPIFFS.remove((char *)upload.filename.c_str());
        uploadFile = SPIFFS.open(upload.filename.c_str(), "w");
        // dont count manual uploads: flashCount();
      }
    }
    if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
    if (loglevelActiveFor(LOG_LEVEL_INFO)) {
      String log = F("Upload: WRITE, Bytes: ");
      log += upload.currentSize;
      addLog(LOG_LEVEL_INFO, log);
    }
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (uploadFile) uploadFile.close();
    if (loglevelActiveFor(LOG_LEVEL_INFO)) {
      String log = F("Upload: END, Size: ");
      log += upload.totalSize;
      addLog(LOG_LEVEL_INFO, log);
    }
  }

  if (valid)
    uploadResult = 1;
  else
    uploadResult = 2;

}


//********************************************************************************
// Web Interface server web file from SPIFFS
//********************************************************************************
bool loadFromFS(boolean spiffs, String path) {
  // path is a deepcopy, since it will be changed here.
  checkRAM(F("loadFromFS"));
  if (!isLoggedIn()) return false;

  statusLED(true);

  String dataType = F("text/plain");
  if (path.endsWith("/")) path += F("index.htm");

  if (path.endsWith(F(".src"))) path = path.substring(0, path.lastIndexOf("."));
  else if (path.endsWith(F(".htm"))) dataType = F("text/html");
  else if (path.endsWith(F(".css"))) dataType = F("text/css");
  else if (path.endsWith(F(".js"))) dataType = F("application/javascript");
  else if (path.endsWith(F(".png"))) dataType = F("image/png");
  else if (path.endsWith(F(".gif"))) dataType = F("image/gif");
  else if (path.endsWith(F(".jpg"))) dataType = F("image/jpeg");
  else if (path.endsWith(F(".ico"))) dataType = F("image/x-icon");
  else if (path.endsWith(F(".txt"))) dataType = F("application/octet-stream");
  else if (path.endsWith(F(".dat"))) dataType = F("application/octet-stream");
  else if (path.endsWith(F(".esp"))) return handle_custom(path);
  if (loglevelActiveFor(LOG_LEVEL_DEBUG)) {
    String log = F("HTML : Request file ");
    log += path;
    addLog(LOG_LEVEL_DEBUG, log);
  }

  path = path.substring(1);
  if (spiffs)
  {
    fs::File dataFile = SPIFFS.open(path.c_str(), "r");
    if (!dataFile)
      return false;

    //prevent reloading stuff on every click
    WebServer.sendHeader(F("Cache-Control"), F("max-age=3600, public"));
    WebServer.sendHeader(F("Vary"),"*");
    WebServer.sendHeader(F("ETag"), F("\"2.0.0\""));

    if (path.endsWith(F(".dat")))
      WebServer.sendHeader(F("Content-Disposition"), F("attachment;"));
    WebServer.streamFile(dataFile, dataType);
    dataFile.close();
  }
  else
  {
#ifdef FEATURE_SD
    File dataFile = SD.open(path.c_str());
    if (!dataFile)
      return false;
    if (path.endsWith(F(".DAT")))
      WebServer.sendHeader(F("Content-Disposition"), F("attachment;"));
    WebServer.streamFile(dataFile, dataType);
    dataFile.close();
#endif
  }
  statusLED(true);
  return true;
}

//********************************************************************************
// Web Interface custom page handler
//********************************************************************************
boolean handle_custom(String path) {
  // path is a deepcopy, since it will be changed.
  checkRAM(F("handle_custom"));
  if (!clientIPallowed()) return false;
  path = path.substring(1);

  // create a dynamic custom page, parsing task values into [<taskname>#<taskvalue>] placeholders and parsing %xx% system variables
  fs::File dataFile = SPIFFS.open(path.c_str(), "r");
  const bool dashboardPage = path.startsWith(F("dashboard"));
  if (!dataFile && !dashboardPage) {
    return false; // unknown file that does not exist...
  }

  if (dashboardPage) // for the dashboard page, create a default unit dropdown selector
  {
    // handle page redirects to other unit's as requested by the unit dropdown selector
    byte unit = getFormItemInt(F("unit"));
    byte btnunit = getFormItemInt(F("btnunit"));
    if(!unit) unit = btnunit; // unit element prevails, if not used then set to btnunit
    if (unit && unit != Settings.Unit)
    {
      NodesMap::iterator it = Nodes.find(unit);
      if (it != Nodes.end()) {
        TXBuffer.startStream();
        sendHeadandTail(F("TmplDsh"),_HEAD);
        TXBuffer += F("<meta http-equiv=\"refresh\" content=\"0; URL=http://");
        TXBuffer += it->second.ip.toString();
        TXBuffer += F("/dashboard.esp\">");
        sendHeadandTail(F("TmplDsh"),_TAIL);
        TXBuffer.endStream();
        return true;
      }
    }

    TXBuffer.startStream();
    sendHeadandTail(F("TmplDsh"),_HEAD);
    html_add_autosubmit_form();
    html_add_form();

    // create unit selector dropdown
    addSelector_Head(F("unit"), true);
    byte choice = Settings.Unit;
    for (NodesMap::iterator it = Nodes.begin(); it != Nodes.end(); ++it)
    {
      if (it->second.ip[0] != 0 || it->first == Settings.Unit)
      {
        String name = String(it->first) + F(" - ");
        if (it->first != Settings.Unit)
          name += it->second.nodeName;
        else
          name += Settings.Name;
        addSelector_Item(name, it->first, choice == it->first, false, "");
      }
    }
    addSelector_Foot();

    // create <> navigation buttons
    byte prev=Settings.Unit;
    byte next=Settings.Unit;
    NodesMap::iterator it;
    for (byte x = Settings.Unit-1; x > 0; x--) {
      it = Nodes.find(x);
      if (it != Nodes.end()) {
        if (it->second.ip[0] != 0) {prev = x; break;}
      }
    }
    for (byte x = Settings.Unit+1; x < UNIT_MAX; x++) {
      it = Nodes.find(x);
      if (it != Nodes.end()) {
        if (it->second.ip[0] != 0) {next = x; break;}
      }
    }

    html_add_button_prefix();
    TXBuffer += path;
    TXBuffer += F("?btnunit=");
    TXBuffer += prev;
    TXBuffer += F("'>&lt;</a>");
    html_add_button_prefix();
    TXBuffer += path;
    TXBuffer += F("?btnunit=");
    TXBuffer += next;
    TXBuffer += F("'>&gt;</a>");
  }

  // handle commands from a custom page
  String webrequest = WebServer.arg(F("cmd"));
  if (webrequest.length() > 0 ){
    struct EventStruct TempEvent;
    parseCommandString(&TempEvent, webrequest);
    TempEvent.Source = VALUE_SOURCE_HTTP;

    if (PluginCall(PLUGIN_WRITE, &TempEvent, webrequest));
    else if (remoteConfig(&TempEvent, webrequest));
    else if (webrequest.startsWith(F("event")))
      ExecuteCommand(VALUE_SOURCE_HTTP, webrequest.c_str());

    // handle some update processes first, before returning page update...
    PluginCall(PLUGIN_TEN_PER_SECOND, 0, dummyString);
  }


  if (dataFile)
  {
    String page = "";
    page.reserve(dataFile.size());
    while (dataFile.available())
      page += ((char)dataFile.read());

    TXBuffer += parseTemplate(page,0);
    dataFile.close();
  }
  else // if the requestef file does not exist, create a default action in case the page is named "dashboard*"
  {
    if (dashboardPage)
    {
      // if the custom page does not exist, create a basic task value overview page in case of dashboard request...
      TXBuffer += F("<meta name='viewport' content='width=width=device-width, initial-scale=1'><STYLE>* {font-family:sans-serif; font-size:16pt;}.button {margin:4px; padding:4px 16px; background-color:#07D; color:#FFF; text-decoration:none; border-radius:4px}</STYLE>");
      html_table_class_normal();
      for (byte x = 0; x < TASKS_MAX; x++)
      {
        if (Settings.TaskDeviceNumber[x] != 0)
          {
            LoadTaskSettings(x);
            byte DeviceIndex = getDeviceIndex(Settings.TaskDeviceNumber[x]);
            html_TR_TD();
            TXBuffer += ExtraTaskSettings.TaskDeviceName;
            for (byte varNr = 0; varNr < VARS_PER_TASK; varNr++)
              {
                if ((Settings.TaskDeviceNumber[x] != 0) && (varNr < Device[DeviceIndex].ValueCount) && ExtraTaskSettings.TaskDeviceValueNames[varNr][0] !=0)
                {
                  if (varNr > 0)
                    html_TR_TD();
                  html_TD();
                  TXBuffer += ExtraTaskSettings.TaskDeviceValueNames[varNr];
                  html_TD();
                  TXBuffer += String(UserVar[x * VARS_PER_TASK + varNr], ExtraTaskSettings.TaskDeviceValueDecimals[varNr]);
                }
              }
          }
      }
    }
  }
  sendHeadandTail(F("TmplDsh"),_TAIL);
  TXBuffer.endStream();
  return true;
}



//********************************************************************************
// Web Interface file list
//********************************************************************************
void handle_filelist() {
  checkRAM(F("handle_filelist"));
  if (!clientIPallowed()) return;
  navMenuIndex = MENU_INDEX_TOOLS;
  TXBuffer.startStream();
  sendHeadandTail_stdtemplate();

#if defined(ESP8266)

  String fdelete = WebServer.arg(F("delete"));

  if (fdelete.length() > 0)
  {
    SPIFFS.remove(fdelete);
    checkRuleSets();
  }

  const int pageSize = 25;
  int startIdx = 0;

  String fstart = WebServer.arg(F("start"));
  if (fstart.length() > 0)
  {
    startIdx = atoi(fstart.c_str());
  }
  int endIdx = startIdx + pageSize - 1;

  html_table_class_multirow();
  html_table_header("", 50);
  html_table_header(F("Filename"));
  html_table_header(F("Size"), 80);

  fs::Dir dir = SPIFFS.openDir("");

  int count = -1;
  while (dir.next())
  {
    ++count;

    if (count < startIdx)
    {
      continue;
    }

    html_TR_TD();
    if (dir.fileName() != F(FILE_CONFIG) && dir.fileName() != F(FILE_SECURITY) && dir.fileName() != F(FILE_NOTIFICATION))
    {
      html_add_button_prefix();
      TXBuffer += F("filelist?delete=");
      TXBuffer += dir.fileName();
      if (startIdx > 0)
      {
        TXBuffer += F("&start=");
        TXBuffer += startIdx;
      }
      TXBuffer += F("'>Del</a>");
    }

    TXBuffer += F("<TD><a href=\"");
    TXBuffer += dir.fileName();
    TXBuffer += "\">";
    TXBuffer += dir.fileName();
    TXBuffer += F("</a>");
    fs::File f = dir.openFile("r");
    html_TD();
    if (f) {
      TXBuffer += f.size();
      f.close();
    }
    if (count >= endIdx)
    {
      break;
    }
  }
  html_end_table();
  html_end_form();
  html_BR();
  addButton(F("/upload"), F("Upload"));
  if (startIdx > 0)
  {
    html_add_button_prefix();
    TXBuffer += F("/filelist?start=");
    TXBuffer += max(0, startIdx - pageSize);
    TXBuffer += F("'>Previous</a>");
  }
  if (count >= endIdx and dir.next())
  {
    html_add_button_prefix();
    TXBuffer += F("/filelist?start=");
    TXBuffer += endIdx + 1;
    TXBuffer += F("'>Next</a>");
  }
  TXBuffer += F("<BR><BR>");
  sendHeadandTail_stdtemplate(true);
  TXBuffer.endStream();
#endif
#if defined(ESP32)
  String fdelete = WebServer.arg(F("delete"));

  if (fdelete.length() > 0)
  {
    SPIFFS.remove(fdelete);
    // flashCount();
  }

  const int pageSize = 25;
  int startIdx = 0;

  String fstart = WebServer.arg(F("start"));
  if (fstart.length() > 0)
  {
    startIdx = atoi(fstart.c_str());
  }
  int endIdx = startIdx + pageSize - 1;

  html_table_class_multirow();
  html_table_header("");
  html_table_header(F("Filename"));
  html_table_header("Size");

  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  int count = -1;
  while (file and count < endIdx)
  {
    if(!file.isDirectory()){
      ++count;

      if (count >= startIdx)
      {
        html_TR_TD();
        if (strcmp(file.name(), FILE_CONFIG) != 0 && strcmp(file.name(), FILE_SECURITY) != 0 && strcmp(file.name(), FILE_NOTIFICATION) != 0)
        {
          html_add_button_prefix();
          TXBuffer += F("filelist?delete=");
          TXBuffer += file.name();
          if (startIdx > 0)
          {
            TXBuffer += F("&start=");
            TXBuffer += startIdx;
          }
          TXBuffer += F("'>Del</a>");
        }

        TXBuffer += F("<TD><a href=\"");
        TXBuffer += file.name();
        TXBuffer += "\">";
        TXBuffer += file.name();
        TXBuffer += F("</a>");
        html_TD();
        TXBuffer += file.size();
      }
    }
    file = root.openNextFile();
  }
  html_end_table();
  html_end_form();
  html_BR();
  addButton(F("/upload"), F("Upload"));
  if (startIdx > 0)
  {
    html_add_button_prefix();
    TXBuffer += F("/filelist?start=");
    TXBuffer += startIdx < pageSize ? 0 : startIdx - pageSize;
    TXBuffer += F("'>Previous</a>");
  }
  if (count >= endIdx and file)
  {
    html_add_button_prefix();
    TXBuffer += F("/filelist?start=");
    TXBuffer += endIdx + 1;
    TXBuffer += F("'>Next</a>");
  }
  TXBuffer += F("<BR><BR>");
    sendHeadandTail_stdtemplate(true);
    TXBuffer.endStream();
#endif
}


//********************************************************************************
// Web Interface SD card file and directory list
//********************************************************************************
#ifdef FEATURE_SD
void handle_SDfilelist() {
  checkRAM(F("handle_SDfilelist"));
  if (!clientIPallowed()) return;
  navMenuIndex = MENU_INDEX_TOOLS;
  TXBuffer.startStream();
  sendHeadandTail_stdtemplate();


  String fdelete = "";
  String ddelete = "";
  String change_to_dir = "";
  String current_dir = "";
  String parent_dir = "";

  for (uint8_t i = 0; i < WebServer.args(); i++) {
    if (WebServer.argName(i) == F("delete"))
    {
      fdelete = WebServer.arg(i);
    }
    if (WebServer.argName(i) == F("deletedir"))
    {
      ddelete = WebServer.arg(i);
    }
    if (WebServer.argName(i) == F("chgto"))
    {
      change_to_dir = WebServer.arg(i);
    }
  }

  if (fdelete.length() > 0)
  {
    SD.remove((char*)fdelete.c_str());
  }
  if (ddelete.length() > 0)
  {
    SD.rmdir((char*)ddelete.c_str());
  }
  if (change_to_dir.length() > 0)
  {
    current_dir = change_to_dir;
  }
  else
  {
    current_dir = "/";
  }

  File root = SD.open(current_dir.c_str());
  root.rewindDirectory();
  File entry = root.openNextFile();
  parent_dir = current_dir;
  if (!current_dir.equals("/"))
  {
    /* calculate the position to remove
    /
    / current_dir = /dir1/dir2/   =>   parent_dir = /dir1/
    /                     ^ position to remove, second last index of "/" + 1
    /
    / current_dir = /dir1/   =>   parent_dir = /
    /                ^ position to remove, second last index of "/" + 1
    */
    parent_dir.remove(parent_dir.lastIndexOf("/", parent_dir.lastIndexOf("/") - 1) + 1);
  }



  String subheader = "SD Card: " + current_dir;
  addFormSubHeader(subheader);
  html_BR();
  html_table_class_multirow();
  html_table_header("", 50);
  html_table_header("Name");
  html_table_header("Size");
  html_TR_TD();
  TXBuffer += F("<TD><a href=\"SDfilelist?chgto=");
  TXBuffer += parent_dir;
  TXBuffer += F("\">..");
  TXBuffer += F("</a>");
  html_TD();
  while (entry)
  {
    if (entry.isDirectory())
    {
      char SDcardChildDir[80];
      html_TR_TD();
      // take a look in the directory for entries
      String child_dir = current_dir + entry.name();
      child_dir.toCharArray(SDcardChildDir, child_dir.length()+1);
      File child = SD.open(SDcardChildDir);
      File dir_has_entry = child.openNextFile();
      // when the directory is empty, display the button to delete them
      if (!dir_has_entry)
      {
        TXBuffer += F("<a class='button link' onclick=\"return confirm('Delete this directory?')\" href=\"SDfilelist?deletedir=");
        TXBuffer += current_dir;
        TXBuffer += entry.name();
        TXBuffer += '/';
        TXBuffer += F("&chgto=");
        TXBuffer += current_dir;
        TXBuffer += F("\">Del</a>");
      }
      TXBuffer += F("<TD><a href=\"SDfilelist?chgto=");
      TXBuffer += current_dir;
      TXBuffer += entry.name();
      TXBuffer += '/';
      TXBuffer += "\">";
      TXBuffer += entry.name();
      TXBuffer += F("</a>");
      html_TD();
      TXBuffer += F("dir");
      dir_has_entry.close();
    }
    else
    {
      html_TR_TD();
      if (entry.name() != String(F(FILE_CONFIG)).c_str() && entry.name() != String(F(FILE_SECURITY)).c_str())
      {
        TXBuffer += F("<a class='button link' onclick=\"return confirm('Delete this file?')\" href=\"SDfilelist?delete=");
        TXBuffer += current_dir;
        TXBuffer += entry.name();
        TXBuffer += F("&chgto=");
        TXBuffer += current_dir;
        TXBuffer += F("\">Del</a>");
      }
      TXBuffer += F("<TD><a href=\"");
      TXBuffer += current_dir;
      TXBuffer += entry.name();
      TXBuffer += "\">";
      TXBuffer += entry.name();
      TXBuffer += F("</a>");
      html_TD();
      TXBuffer += entry.size();
    }
    entry.close();
    entry = root.openNextFile();
  }
  root.close();
  html_end_table();
  html_end_form();
  //TXBuffer += F("<BR><a class='button link' href=\"/upload\">Upload</a>");
  sendHeadandTail_stdtemplate(true);
  TXBuffer.endStream();
}
#endif


//********************************************************************************
// Web Interface handle other requests
//********************************************************************************
void handleNotFound() {
  checkRAM(F("handleNotFound"));

  if (wifiSetup)
  {
    WebServer.send(200, F("text/html"), F("<meta HTTP-EQUIV='REFRESH' content='0; url=/setup'>"));
    return;
  }

  if (!isLoggedIn()) return;
  if (handle_rules_edit(WebServer.uri())) return;
  if (loadFromFS(true, WebServer.uri())) return;
  if (loadFromFS(false, WebServer.uri())) return;
  String message = F("URI: ");
  message += WebServer.uri();
  message += F("\nMethod: ");
  message += (WebServer.method() == HTTP_GET) ? F("GET") : F("POST");
  message += F("\nArguments: ");
  message += WebServer.args();
  message += "\n";
  for (uint8_t i = 0; i < WebServer.args(); i++) {
    message += F(" NAME:");
    message += WebServer.argName(i);
    message += F("\n VALUE:");
    message += WebServer.arg(i);
    message += '\n';
=======
    TXBuffer += '>';
    TXBuffer += ExtraTaskSettings.TaskDeviceValueNames[x];
    TXBuffer += F("</option>");
>>>>>>> 79975d385bc2353f4f4297249fff1d850b962309
  }
}

// ********************************************************************************
// Login state check
// ********************************************************************************
boolean isLoggedIn()
{
  String www_username = F(DEFAULT_ADMIN_USERNAME);

  if (!clientIPallowed()) { return false; }

  if (SecuritySettings.Password[0] == 0) { return true; }

  if (!WebServer.authenticate(www_username.c_str(), SecuritySettings.Password))

  // Basic Auth Method with Custom realm and Failure Response
  // return server.requestAuthentication(BASIC_AUTH, www_realm, authFailResponse);
  // Digest Auth Method with realm="Login Required" and empty Failure Response
  // return server.requestAuthentication(DIGEST_AUTH);
  // Digest Auth Method with Custom realm and empty Failure Response
  // return server.requestAuthentication(DIGEST_AUTH, www_realm);
  // Digest Auth Method with Custom realm and Failure Response
  {
#ifdef CORE_PRE_2_5_0

    // See https://github.com/esp8266/Arduino/issues/4717
    HTTPAuthMethod mode = BASIC_AUTH;
#else // ifdef CORE_PRE_2_5_0
    HTTPAuthMethod mode = DIGEST_AUTH;
#endif // ifdef CORE_PRE_2_5_0
    String message = F("Login Required (default user: ");
    message += www_username;
    message += ')';
    WebServer.requestAuthentication(mode, message.c_str());
    return false;
  }
  return true;
}

String getControllerSymbol(byte index)
{
  String ret = F("<p style='font-size:20px'>&#");

  ret += 10102 + index;
  ret += F(";</p>");
  return ret;
}

/*
   String getValueSymbol(byte index)
   {
   String ret = F("&#");
   ret += 10112 + index;
   ret += ';';
   return ret;
   }
 */

void addSVG_param(const String& key, float value) {
  String value_str = String(value, 2);
  addSVG_param(key, value_str);
}

void addSVG_param(const String& key, const String& value) {
  TXBuffer += ' ';
  TXBuffer += key;
  TXBuffer += '=';
  TXBuffer += '\"';
  TXBuffer += value;
  TXBuffer += '\"';
}

void createSvgRect_noStroke(unsigned int fillColor, float xoffset, float yoffset, float width, float height, float rx, float ry) {
  createSvgRect(fillColor, fillColor, xoffset, yoffset, width, height, 0, rx, ry);
}

void createSvgRect(unsigned int fillColor, unsigned int strokeColor, float xoffset, float yoffset, float width, float height, float strokeWidth, float rx, float ry) {
  TXBuffer += F("<rect");
  addSVG_param(F("fill"), formatToHex(fillColor, F("#")));
  if (strokeWidth != 0) {
    addSVG_param(F("stroke"), formatToHex(strokeColor, F("#")));
    addSVG_param(F("stroke-width"), strokeWidth);
  }
  addSVG_param("x", xoffset);
  addSVG_param("y", yoffset);
  addSVG_param(F("width"), width);
  addSVG_param(F("height"), height);
  addSVG_param(F("rx"), rx);
  addSVG_param(F("ry"), ry);
  TXBuffer += F("/>");
}

void createSvgHorRectPath(unsigned int color, int xoffset, int yoffset, int size, int height, int range, float SVG_BAR_WIDTH) {
  float width = SVG_BAR_WIDTH * size / range;

  if (width < 2) { width = 2; }
  TXBuffer += formatToHex(color, F("<path fill=\"#"));
  TXBuffer += F("\" d=\"M");
  TXBuffer += toString(SVG_BAR_WIDTH * xoffset / range, 2);
  TXBuffer += ' ';
  TXBuffer += yoffset;
  TXBuffer += 'h';
  TXBuffer += toString(width, 2);
  TXBuffer += 'v';
  TXBuffer += height;
  TXBuffer += 'H';
  TXBuffer += toString(SVG_BAR_WIDTH * xoffset / range, 2);
  TXBuffer += F("z\"/>\n");
}

void createSvgTextElement(const String& text, float textXoffset, float textYoffset) {
  TXBuffer += F("<text style=\"line-height:1.25\" x=\"");
  TXBuffer += toString(textXoffset, 2);
  TXBuffer += F("\" y=\"");
  TXBuffer += toString(textYoffset, 2);
  TXBuffer += F("\" stroke-width=\".3\" font-family=\"sans-serif\" font-size=\"8\" letter-spacing=\"0\" word-spacing=\"0\">\n");
  TXBuffer += F("<tspan x=\"");
  TXBuffer += toString(textXoffset, 2);
  TXBuffer += F("\" y=\"");
  TXBuffer += toString(textYoffset, 2);
  TXBuffer += "\">";
  TXBuffer += text;
  TXBuffer += F("</tspan>\n</text>");
}

unsigned int getSettingsTypeColor(SettingsType settingsType) {
  switch (settingsType) {
    case BasicSettings_Type:
      return 0x5F0A87;
    case TaskSettings_Type:
      return 0xEE6352;
    case CustomTaskSettings_Type:
      return 0x59CD90;
    case ControllerSettings_Type:
      return 0x3FA7D6;
    case CustomControllerSettings_Type:
      return 0xFAC05E;
    case NotificationSettings_Type:
      return 0xF79D84;
    default:
      break;
  }
  return 0;
}

#define SVG_BAR_HEIGHT 16
#define SVG_BAR_WIDTH 400

void write_SVG_image_header(int width, int height) {
  write_SVG_image_header(width, height, false);
}

void write_SVG_image_header(int width, int height, bool useViewbox) {
  TXBuffer += F("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"");
  TXBuffer += width;
  TXBuffer += F("\" height=\"");
  TXBuffer += height;
  TXBuffer += F("\" version=\"1.1\"");

  if (useViewbox) {
    TXBuffer += F(" viewBox=\"0 0 100 100\"");
  }
  TXBuffer += '>';
}

/*
void getESPeasyLogo(int width_pixels) {
  write_SVG_image_header(width_pixels, width_pixels, true);
  TXBuffer += F("<g transform=\"translate(-33.686 -7.8142)\">");
  TXBuffer += F("<rect x=\"49\" y=\"23.1\" width=\"69.3\" height=\"69.3\" fill=\"#2c72da\" stroke=\"#2c72da\" stroke-linecap=\"round\"stroke-linejoin=\"round\" stroke-width=\"30.7\"/>");
  TXBuffer += F("<g transform=\"matrix(3.3092 0 0 3.3092 -77.788 -248.96)\">");
  TXBuffer += F("<path d=\"m37.4 89 7.5-7.5M37.4 96.5l15-15M37.4 96.5l15-15M37.4 104l22.5-22.5M44.9 104l15-15\" fill=\"none\"stroke=\"#fff\" stroke-linecap=\"round\" stroke-width=\"2.6\"/>");
  TXBuffer += F("<circle cx=\"58\" cy=\"102.1\" r=\"3\" fill=\"#fff\"/>");
  TXBuffer += F("</g></g></svg>");
}
*/

void getWiFi_RSSI_icon(int rssi, int width_pixels)
{
  const int nbars_filled = (rssi + 100) / 8;
  int nbars = 5;
  int white_between_bar = (static_cast<float>(width_pixels) / nbars) * 0.2;
  if (white_between_bar < 1) { white_between_bar = 1; }
  const int barWidth = (width_pixels - (nbars - 1) * white_between_bar) / nbars;
  int svg_width_pixels = nbars * barWidth + (nbars - 1) * white_between_bar;
  write_SVG_image_header(svg_width_pixels, svg_width_pixels, true);
  float scale = 100 / svg_width_pixels;
  const int bar_height_step = 100 / nbars;
  for (int i = 0; i < nbars; ++i) {
    unsigned int color = i < nbars_filled ? 0x0 : 0xa1a1a1;  // Black/Grey
    int barHeight = (i + 1) * bar_height_step;
    createSvgRect_noStroke(color, i * (barWidth + white_between_bar) * scale, 100 - barHeight, barWidth, barHeight, 0, 0);
  }
  TXBuffer += F("</svg>\n");
}


#ifndef BUILD_MINIMAL_OTA
void getConfig_dat_file_layout() {
  const int shiftY  = 2;
  float     yOffset = shiftY;

  write_SVG_image_header(SVG_BAR_WIDTH + 250, SVG_BAR_HEIGHT + shiftY);

  int max_index, offset, max_size;
  int struct_size = 0;

  // background
  const uint32_t realSize = getFileSize(TaskSettings_Type);
  createSvgHorRectPath(0xcdcdcd, 0, yOffset, realSize, SVG_BAR_HEIGHT - 2, realSize, SVG_BAR_WIDTH);

  for (int st = 0; st < SettingsType_MAX; ++st) {
    SettingsType settingsType = static_cast<SettingsType>(st);

    if (settingsType != NotificationSettings_Type) {
      unsigned int color = getSettingsTypeColor(settingsType);
      getSettingsParameters(settingsType, 0, max_index, offset, max_size, struct_size);

      for (int i = 0; i < max_index; ++i) {
        getSettingsParameters(settingsType, i, offset, max_size);

        // Struct position
        createSvgHorRectPath(color, offset, yOffset, max_size, SVG_BAR_HEIGHT - 2, realSize, SVG_BAR_WIDTH);
      }
    }
  }

  // Text labels
  float textXoffset = SVG_BAR_WIDTH + 2;
  float textYoffset = yOffset + 0.9 * SVG_BAR_HEIGHT;
  createSvgTextElement(F("Config.dat"), textXoffset, textYoffset);
  TXBuffer += F("</svg>\n");
}

void getStorageTableSVG(SettingsType settingsType) {
  uint32_t realSize   = getFileSize(settingsType);
  unsigned int color  = getSettingsTypeColor(settingsType);
  const int    shiftY = 2;

  int max_index, offset, max_size;
  int struct_size = 0;

  getSettingsParameters(settingsType, 0, max_index, offset, max_size, struct_size);

  if (max_index == 0) { return; }

  // One more to add bar indicating struct size vs. reserved space.
  write_SVG_image_header(SVG_BAR_WIDTH + 250, (max_index + 1) * SVG_BAR_HEIGHT + shiftY);
  float yOffset = shiftY;

  for (int i = 0; i < max_index; ++i) {
    getSettingsParameters(settingsType, i, offset, max_size);

    // background
    createSvgHorRectPath(0xcdcdcd, 0,      yOffset, realSize, SVG_BAR_HEIGHT - 2, realSize, SVG_BAR_WIDTH);

    // Struct position
    createSvgHorRectPath(color,    offset, yOffset, max_size, SVG_BAR_HEIGHT - 2, realSize, SVG_BAR_WIDTH);

    // Text labels
    float textXoffset = SVG_BAR_WIDTH + 2;
    float textYoffset = yOffset + 0.9 * SVG_BAR_HEIGHT;
    createSvgTextElement(formatHumanReadable(offset, 1024),   textXoffset, textYoffset);
    textXoffset = SVG_BAR_WIDTH + 60;
    createSvgTextElement(formatHumanReadable(max_size, 1024), textXoffset, textYoffset);
    textXoffset = SVG_BAR_WIDTH + 130;
    createSvgTextElement(String(i),                           textXoffset, textYoffset);
    yOffset += SVG_BAR_HEIGHT;
  }

  // usage
  createSvgHorRectPath(0xcdcdcd, 0, yOffset, max_size, SVG_BAR_HEIGHT - 2, max_size, SVG_BAR_WIDTH);

  // Struct size (used part of the reserved space)
  if (struct_size != 0) {
    createSvgHorRectPath(color, 0, yOffset, struct_size, SVG_BAR_HEIGHT - 2, max_size, SVG_BAR_WIDTH);
  }

  // Text labels
  float textXoffset = SVG_BAR_WIDTH + 2;
  float textYoffset = yOffset + 0.9 * SVG_BAR_HEIGHT;

  if (struct_size != 0) {
    String text = formatHumanReadable(struct_size, 1024);
    text += '/';
    text += formatHumanReadable(max_size, 1024);
    text += F(" per item");
    createSvgTextElement(text, textXoffset, textYoffset);
  } else {
    createSvgTextElement(F("Variable size"), textXoffset, textYoffset);
  }
  TXBuffer += F("</svg>\n");
}

#endif // ifndef BUILD_MINIMAL_OTA

#ifdef ESP32


int getPartionCount(byte pType) {
  esp_partition_type_t partitionType       = static_cast<esp_partition_type_t>(pType);
  esp_partition_iterator_t _mypartiterator = esp_partition_find(partitionType, ESP_PARTITION_SUBTYPE_ANY, NULL);
  int nrPartitions                         = 0;

  if (_mypartiterator) {
    do {
      ++nrPartitions;
    } while ((_mypartiterator = esp_partition_next(_mypartiterator)) != NULL);
  }
  esp_partition_iterator_release(_mypartiterator);
  return nrPartitions;
}

void getPartitionTableSVG(byte pType, unsigned int partitionColor) {
  int nrPartitions = getPartionCount(pType);

  if (nrPartitions == 0) { return; }
  const int shiftY = 2;

  uint32_t realSize                      = getFlashRealSizeInBytes();
  esp_partition_type_t     partitionType = static_cast<esp_partition_type_t>(pType);
  const esp_partition_t   *_mypart;
  esp_partition_iterator_t _mypartiterator = esp_partition_find(partitionType, ESP_PARTITION_SUBTYPE_ANY, NULL);
  write_SVG_image_header(SVG_BAR_WIDTH + 250, nrPartitions * SVG_BAR_HEIGHT + shiftY);
  float yOffset = shiftY;

  if (_mypartiterator) {
    do {
      _mypart = esp_partition_get(_mypartiterator);
      createSvgHorRectPath(0xcdcdcd,       0,                yOffset, realSize,      SVG_BAR_HEIGHT - 2, realSize, SVG_BAR_WIDTH);
      createSvgHorRectPath(partitionColor, _mypart->address, yOffset, _mypart->size, SVG_BAR_HEIGHT - 2, realSize, SVG_BAR_WIDTH);
      float textXoffset = SVG_BAR_WIDTH + 2;
      float textYoffset = yOffset + 0.9 * SVG_BAR_HEIGHT;
      createSvgTextElement(formatHumanReadable(_mypart->size, 1024),          textXoffset, textYoffset);
      textXoffset = SVG_BAR_WIDTH + 60;
      createSvgTextElement(_mypart->label,                                    textXoffset, textYoffset);
      textXoffset = SVG_BAR_WIDTH + 130;
      createSvgTextElement(getPartitionType(_mypart->type, _mypart->subtype), textXoffset, textYoffset);
      yOffset += SVG_BAR_HEIGHT;
    } while ((_mypartiterator = esp_partition_next(_mypartiterator)) != NULL);
  }
  TXBuffer += F("</svg>\n");
  esp_partition_iterator_release(_mypartiterator);
}

#endif // ifdef ESP32
