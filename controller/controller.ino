#define MINUTESECS 60
#define SAMPLE_INTERVAL 250   // 1/4 second = 250 milliseconds
#define TEMP_PAD 0.3          // pad = 0.3 degrees which creates a temperature target band.
#define FLIP_FLOP_MINUTES 5   // minutes a zone must stay in a state to prevent rapid flipping on and off

#include <string.h> 
#include <time.h> 
#include <stdint.h>

#include "vars.h"

#if defined(__arm__)
#include "arduino_util.h"
#else
#include "util.h"
#endif

// Declare main data sructures
Cfg_t cfg;
Data_t data;
WiFiClient client;
float aref_volts = 3.3;

void run_tests();
void log_data(unsigned short hr_min, int i, float deg, float tgt, bool is_on, bool call);
void checkTempsAgainstSchedule();
void toggleHeat(int zone_idx);
void periodicFuncs(unsigned long ms);
bool isWorkDay();

void setArefVoltage() {
    analogReference(AR_INTERNAL1V0);
    aref_volts = 1.0;
}
unsigned short getHrMin() {
    int hr = timer.getHours();
    int min = timer.getMinutes();
    return (hr * 60 + min);
}

void initSchedEntry(SchedEntry_t &entry) {
    entry.n_slots = 4;
    entry.slot_time[0] =  7 * 60;
    entry.slot_time[1] = 12 * 60;
    entry.slot_time[2] = 17 * 60;
    entry.slot_time[3] = 23 * 60;
}

void initSched(Sched_t &sched) {
    initSchedEntry(sched.work);
    initSchedEntry(sched.non_work);
}

void initData() {
    memset(&cfg, 0, sizeof(cfg));
    memset(&data, 0, sizeof(data));
    strcpy(cfg.town, "Fairfield, CT");
    strcpy(cfg.ssid, "kimberly2.4g");
    strcpy(cfg.pass, "GetYour$h*t");
    cfg.lat= 41.1408;
    cfg.lon = -73.2613;
    cfg.time_zone_offset = -5;

    cfg.n_tstats = 1;
    cfg.tstats[0].active = true;
    cfg.tstats[0].input_port = A0;

    cfg.n_zones = 1;
    strcpy(cfg.zones[0].name, "FAMILY RM");
    memset(cfg.zones[0].tgt_temp, 70, N_SLOTS);
    cfg.zones[0].tgt_temp[N_SLOTS - 1] = 63;

    initSched(cfg.default_sched);
}

void setup() {
    //run_tests();

    Serial.begin(115200);
    
    // Figure out time zone
    initData();

    setArefVoltage();
    
    processWiFi(cfg);   // first time through initializes stuff that requires a connection

    // Set on/off times after timer initialized.
    unsigned long tm = timer.time();
    data.zones[0].on_off_time = tm;
    
    // get weather prediction

}

// Stubbed
bool isWorkDay() {
    return true;
}

    // TMP36 calibrated to 10mV / deg C and 750mV at 25C (= 77F)
    //
    // If AREF = 3.3v, range is 0 - 3.3V divided by 1024 (10 bit) digital units
    // 1 unit = 0.0032226 V = 3.2226 mV = 0.32226 deg C = 0.58 deg F
    // 1C = 3.1031 units
    // 1F = 1.724 units
    // 25C = 77F = 750mV ~= 232.73 units
    float TempStat_t::getTemp(float aref_volts) {
        if (count <= 0)
            return (0.0);
        float x = float(sensor_sum) / float(count); // average reading
        float units_per_volt = 1024.0 / aref_volts;
        float degF_per_volt = 100.0 * 1.8;        
        float degF_per_unit = degF_per_volt / units_per_volt;
        float units_at_77F = 0.75 * units_per_volt; // 075 volts = 77 deg F
        x -= units_at_77F;                          // just the difference
        x = 77.0 + (x * degF_per_unit);;
        last_temp = x;                              // save most recent reading
        sensor_sum = count = 0;
        return x;
    }


unsigned char Zone_t::getTgtTemp(unsigned short hr_min) {
    Sched_t &sched = use_custom_schedule ? custom_sched : cfg.default_sched;
    SchedEntry_t entry = isWorkDay() ? sched.work : sched.non_work;
    short i;
    for (i=0; (i < entry.n_slots) && entry.slot_time[i] <= hr_min; i++) {
        ;
    }
    i = (i > 0) ? i-1 : entry.n_slots - 1;
    return tgt_temp[i];
}

void checkTempsAgainstSchedule() {
    unsigned short hr_min = getHrMin();
    int i;
    // Set all zones not to call for heat
    for (i = 0; i < cfg.n_zones; i++)
        data.zones[i].call_for_heat = false;

    // Test all thermostats against schedule.
    //  If heat is off and temp < tgt - pad, set call_for_heat flag for the zone
    //  If heat is on and temp < tgt + pad, set call_for_heat flag for the zone

    for (i=0; i < cfg.n_tstats; i++) {
        Tstat_t &tstat = cfg.tstats[i];
        if (tstat.active) {
            //printf("hr=%d, min=%d, samples=%ld ", hr_min/60, hr_min%60, data.tstats[i].stat_1min.count);
            Tdata_t &tdata = data.tstats[i];
            float deg = tdata.stat_1min.getTemp(aref_volts);
            tdata.tgt = cfg.zones[tstat.zone].getTgtTemp(hr_min);
            float tgt = (float) tdata.tgt;
            Zdata_t &zone = data.zones[tstat.zone];
            bool call = false;
            tgt = zone.is_on ? tgt + TEMP_PAD : tgt - TEMP_PAD;
            PRINT(tgt);
            PRINT(",");
            PRINTLN(deg);

            if (deg < tgt) {
                call = true;
                data.zones[tstat.zone].call_for_heat = true;
            }
            //printf("tstat_i=%d, zone=%d, deg=%f, tgt=%f, \n", i, tstat.zone, deg, tgt);
        }
    }

    // process each zone, turning heat on or off as needed
    for (i = 0; i < cfg.n_zones; i++) {
        Zdata_t &zone = data.zones[i];
        
        unsigned long secs = timer.time();
        if (zone.on_off_time == 0)
          zone.on_off_time = secs;
        else {
            secs -= zone.on_off_time;  // seconds in current state
            //PRINTLN(secs_in_state);
            if ((zone.call_for_heat != zone.is_on) && (secs >= FLIP_FLOP_MINUTES * MINUTESECS))
                toggleHeat(i);
        }
    }
}

void toggleHeat(int zone_idx) {
    Zdata_t &zone = data.zones[zone_idx];
    zone.on_off_time = timer.time();
    zone.is_on = !zone.is_on;              // toggle state
    setRelayState(zone_idx, zone.is_on);    // send command to relay control
}

void sendTime(WiFiClient &client) {
    client.print(timer.getHours());
    client.print(":");
    int m = timer.getMinutes();
    if (m <= 9)
        client.print("0");
    client.print(m);
}

void sendZoneStatus(WiFiClient &client, int zone_idx) {
    Zdata_t &zone = data.zones[zone_idx];
    client.print("<title>THERMOSTATUS</title>");
    client.print("<h1>");
    client.print("Thermostat as of ");
    sendTime(client);
    client.print("<br>Zone: ");
    client.print(cfg.zones[zone_idx].name);
    if (zone.is_on)
        client.print(" ON ");
    else
        client.print(" OFF ");
    client.print(" for ");

    int minutes = (timer.time() - zone.on_off_time) / 60;
    client.print(minutes);
    client.print(" minutes<br>\n");

    int i;
    for (i=0; i < cfg.n_tstats; i++) {
        Tstat_t &tstat = cfg.tstats[i];
        if (tstat.active && tstat.zone == zone_idx) {
            Tdata_t &tdata = data.tstats[i];
            client.print(" Tstat: ");
            client.print(i);
            client.print(" Tgt: ");
            client.print(tdata.tgt);

            client.print(" Temp: ");
            client.print(tdata.stat_1min.last_temp);
            client.print("  5min: ");
            client.print(tdata.stat_5min.last_temp);
            client.print("<br />\n");
        }
    }  
    client.print("</h1>");
}

void sendFrontPage(WiFiClient &client) {
    // send a standard http response header
    client.print("HTTP/1.1 200 OK\n");
    client.print("Content-Type: text/html\n");
    client.print("Connection: close\n");  // the connection will be closed after completion of the response
    client.print("Refresh: 5\n");         // refresh the page automatically every 5 sec
    client.print("\n");
    client.print("<!DOCTYPE HTML>");
    client.print("<html>");
    client.print("<body>");

    sendZoneStatus(client, 0);
    
    client.print("</body>");
    client.print("</html>");
  
}

void processClient(WiFiClient &client) {
    PRINT("new client\n");

    // an http request ends with a blank line
    bool currentLineIsBlank = true;

    while (client.connected() && client.available()) {
        char c = client.read();
//        Serial.write(c);

        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply

        if (c == '\n' && currentLineIsBlank) {
            sendFrontPage(client);
            break;
        }

        if (c == '\n') {
          currentLineIsBlank = true;          // you're starting a new line
        }
        else
          if (c != '\r') {
            // you've gotten a character on the current line
            currentLineIsBlank = false;
          }

    }
}

void test_sched() {
    for (int i=0; i < cfg.n_zones; i++) {
        printf("zone: %d\n", i);
        for (short hr = 0; hr < 24; hr++) {
            unsigned short hr_min = hr*60;
            int tgt = cfg.zones[i].getTgtTemp(hr_min);
            printf(" %d -> %d\n", hr, tgt);
        }
    }
}

void test_process_temp() {
    for (long ms=1000; ms < 70000; ms++) {
        periodicFuncs(ms);
    }
}
void run_tests() {
    //test_sched();
    test_process_temp();
    exit(0);
}


void send_server() {
    Tstat_t &tcfg = cfg.tstats[0];
    Tdata_t &tstat = data.tstats[0];
    Zdata_t &zone = data.zones[tcfg.zone];
    WiFiClient client;

    if (!tcfg.active)
        return;

    if ( WiFi.status() != WL_CONNECTED) {
        return;
    }
    const char *server = "192.168.1.2";  // Server

    if (client.connect(server, 3001)){
        sendTime(client);
        client.print(",");
        client.print(tstat.tgt);
        client.print(",");
        client.print(tstat.stat_1min.last_temp);
        client.print(",");
        client.print(tstat.stat_5min.last_temp);
        client.print(",");
        client.print(tstat.prev_5min);
        client.print(",");
        client.print(zone.is_on ? "ON" : "OFF");
        client.print(",");
        int m = zone.on_off_time ? ((timer.time() - zone.on_off_time)/MINUTESECS) : 0;
        client.print(m);
        client.print("\n");

        client.stop();
    }
}

void oneMinuteUpdate() {
    checkTempsAgainstSchedule();
    send_server();
}

void fiveMinuteUpdate() {
    int i;

    for (i=0; i < cfg.n_tstats; i++) {
        Tstat_t &tstat = cfg.tstats[i];
        if (tstat.active) {
            Tdata_t &tdata = data.tstats[i];
            tdata.prev_5min = tdata.stat_5min.last_temp;
            tdata.stat_5min.getTemp(aref_volts);
        }
    }
}

void periodicFuncs(unsigned long ms) {
    static unsigned long next_sample_time = 0;
    static unsigned long next_1_min = 0;
    static unsigned long next_5_min = 0;
    static unsigned long next_wifi_time = 0;
    
    if (ms >= next_sample_time) {
        for (int i=0; i < cfg.n_tstats; i++) {
            Tstat_t &tstat = cfg.tstats[i];
            if ((tstat.active) && (tstat.input_port >= A0)) {
                Tdata_t &tdata = data.tstats[i];
                int val = analogRead(tstat.input_port);
                tdata.stat_1min.addSample(val);
                tdata.stat_5min.addSample(val);
            }
        }
        next_sample_time = ms + SAMPLE_INTERVAL;
    }

    if (ms >= next_wifi_time) {
        processWiFi(cfg);
        next_wifi_time = ms + 1000;
    }

    if (ms >= next_1_min) {
        if (next_1_min) {
            oneMinuteUpdate();
        }
        next_1_min = ms + (MINUTESECS * 1000);
    }

    if (ms >= next_5_min) {
        if (next_5_min) {
            fiveMinuteUpdate();
        }
        next_5_min = ms + (5 * MINUTESECS * 1000);
    }
}

void loop() {
    unsigned long ms = millis();
    periodicFuncs(ms);
    
    // Must call to periodically update time server
    timer.update();
//    delay(1000);    
}
