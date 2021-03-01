#define N_SLOTS 4
#define N_ZONES 4
#define N_TSTATS 10

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
