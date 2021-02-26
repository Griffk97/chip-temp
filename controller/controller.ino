#include <string.h> 

#define MINUTESECS 10
#define N_SLOTS 4
#define N_ZONES 4
#define N_TSTATS 10

#if defined(__arm__)
#include "arduino_util.h"
#else
#include "util.h"
#endif

void run_tests();
void log_data(unsigned short hr_min, int i, float deg, float tgt, bool is_on, bool call);
void checkTempsAgainstSchedule();
void toggleHeat(int zone_idx);
void processTemps(unsigned long ms);
bool isWorkDay();

Timer_t timer;

struct Tstat_t {
    bool active;
    short zone;
    short input_port;  // Analog port on chip.  -1 for wifi
    char ssid[12];     // wifi id
};

struct SchedEntry_t {
    short n_slots;
    unsigned short slot_time[N_SLOTS];
};

struct Sched_t {
    struct SchedEntry_t work;
    struct SchedEntry_t non_work;
};

struct Zone_t {
    char name[12];
    bool use_custom_schedule;
    struct Sched_t custom_sched;
    unsigned char tgt_temp[N_SLOTS];    
    bool be_there[N_SLOTS];

    unsigned char getTgtTemp(unsigned short hr_min);
};

struct Cfg_t {
    char town[20];
    char ssid[20];
    char pass[20];
    float lat;
    float lon;
    char time_zone_offset;
    short n_tstats;
    short n_zones;
    struct Tstat_t tstats[N_TSTATS];
    struct Zone_t zones[N_ZONES];
    struct Sched_t default_sched;

};

struct TempStat_t {
    unsigned long sensor_sum;
    unsigned long count;
    float last_temp;
    
    void addSample(int sample) {
        sensor_sum += sample;
        count++;
    }

    // TMP36 calibrated to 10mV / deg C and 750mV at 25C (= 77F)
    // With 0 - 3.3V input voltage and 1024 (10 bit) digital units
    // 1 unit = 0.0032226 V = 3.2226 mV = 0.32226 deg C = 0.58 deg F
    // 1C = 3.1031 units
    // 1F = 1.724 units
    // 25C = 77F = 750mV ~= 232.73 units

    float getTemp() {
        if (count <= 0)
            return (0.0);
        float x = float(sensor_sum) / float(count); // average reading
        x = 77.0 + (0.58 * (x - 232.73));           // temp in Faranheit
        last_temp = x;                              // save most recent reading
        sensor_sum = count = 0;
        return x;
    }
};

struct Cfg_t cfg;

// dtemp = change in inside temp per hour
// dtemp_off = dtemp with heat off.  Best samples when has been off a while
// factor = dtemp_off / (inside temp - outside temp)
// heat_on_dtemp = peak change in dtemp when heat is on
struct Tdata_t {
    struct TempStat_t stat_1min;
    struct TempStat_t stat_5min;
    float prev_5min;
    float factor;
    float heat_on_dtemp;  
    uint8_t tgt;
};

struct Zdata_t {
    bool call_for_heat;
    bool is_on;
    unsigned long on_off_time;
};

// Working data
struct Data_t {
    unsigned char my_ip[4];
    struct Tdata_t tstats[N_TSTATS];
    Zdata_t zones[N_ZONES];
};

Data_t data;
WiFiClient client;

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
    initData();
    //run_tests();

    Serial.begin(115200);
    
    // Figure out time zone

    // Find out what time it is
    timer.begin(cfg.time_zone_offset);
    PRINT(" Secs since 1970: ");
    PRINTLN(timer.time());
    
    // get weather prediction

}

// Stubbed
bool isWorkDay() {
    return true;
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
    // pad = 0.3 degrees which creates a temperature target band.
    //  If heat is off and temp < tgt - pad, set call_for_heat flag for the zone
    //  If heat is on and temp < tgt + pad, set call_for_heat flag for the zone

    for (i=0; i < cfg.n_tstats; i++) {
        Tstat_t &tstat = cfg.tstats[i];
        if (tstat.active) {
            //printf("hr=%d, min=%d, samples=%ld ", hr_min/60, hr_min%60, data.tstats[i].stat_1min.count);
            Tdata_t tdata= data.tstats[i];
            float deg = tdata.stat_1min.getTemp();
            PRINTLN(deg);
            tdata.tgt = cfg.zones[tstat.zone].getTgtTemp(hr_min);
            float tgt = (float) tdata.tgt;
            Zdata_t &zone = data.zones[tstat.zone];
            float pad = 0.3;
            bool call = false;
            tgt = zone.is_on ? tgt + pad : tgt - pad;
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
        if (zone.call_for_heat != zone.is_on)
            toggleHeat(i);
    }
}

void toggleHeat(int zone_idx) {
    Zdata_t &zone = data.zones[zone_idx];
    zone.on_off_time = timer.time();
    zone.is_on = !zone.is_on;              // toggle state
    setRelayState(zone_idx, zone.is_on);    // send command to relay control
}


#define SAMPLE_INTERVAL 250   // 1/4 second = 250 milliseconds

void processTemps(unsigned long ms) {
    static unsigned long next_sample_time = 0;
    static unsigned long next_1_min = 0;
    static unsigned long next_5_min = 0;

    struct TempStat_t stat_1min;
    struct TempStat_t stat_5min;
    float prev_5min;

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

    if (ms >= next_1_min) {
        if (next_1_min) {
            checkTempsAgainstSchedule();
        }
        next_1_min = ms + (MINUTESECS * 1000);
    }

    if (ms >= next_5_min) {
        if (next_5_min) {
        }
        next_5_min = ms + (5 * MINUTESECS * 1000);
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
        processTemps(ms);
    }
}
void run_tests() {
    //test_sched();
    test_process_temp();
    exit(0);
}


void sendZoneStatus(WiFiClient &client, int zone_idx) {
    Zdata_t &zone = data.zones[zone_idx];
    client.print("Zone: ");
    client.print(zone_idx);
    if (zone.is_on)
        client.print(" ON ");
    else
        client.print(" OFF ");
    client.print(" for ");
    client.print(zone.on_off_time);
    client.print(" minutes<br />\n");

    int i;
    for (i=0; i < cfg.n_tstats; i++) {
        Tstat_t &tstat = cfg.tstats[i];
        if (tstat.active && tstat.zone == zone_idx) {
            Tdata_t &tdata = data.tstats[i];
            client.print(" Tstat: ");
            client.print(i);
            client.print(" Tgt: ");
            client.print(tdata.tgt);

            client.print(" Temp1: ");
            client.print(tdata.stat_1min.last_temp);
            client.print(" Temp5: ");
            client.print(tdata.stat_5min.last_temp);
            client.print("<br />\n");
        }
    }  
}

void sendFrontPage(WiFiClient &client) {
    // send a standard http response header
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Connection: close");  // the connection will be closed after completion of the response
    client.println("Refresh: 5");  // refresh the page automatically every 5 sec
    client.println();
    client.println("<!DOCTYPE HTML>");
    client.println("<html>");

    sendZoneStatus(client, 0);
    
    client.println("</html>");
  
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
    
void processWiFi(unsigned long ms) {
    static unsigned long next_check_time = 0;
    static WiFiServer server(80);
    
    if (ms >= next_check_time) {
        
        if ( WiFi.status() != WL_CONNECTED) {
            PRINT("Attempting to connect to WPA network...\n");
            if (WiFi.begin(cfg.ssid, cfg.pass) != WL_CONNECTED) {
                PRINT("Couldn't get a wifi connection\n");
                return;
            }
            IPAddress ip = WiFi.localIP();
            PRINT("IP Address: ");
            PRINTLN(ip);

            server.begin();
        }
          
        WiFiClient client = server.available();        
        if (client)
          processClient(client);
        
        next_check_time = ms + 1000;
    }
}

void send_server() {
    int data = 0;
    WiFiClient client;
    if ( WiFi.status() != WL_CONNECTED) {
        return;
    }
    const char *server = "192,168,1,22";  // Server

    if (client.connect(server, 3001)){
        client.print(data);
        client.stop();
    }
}

void loop() {
    unsigned long ms = millis();
    processTemps(ms);
    processWiFi(ms);
    
    // Must call to periodically update time server
    timer.update();
//    delay(1000);    
}
