// Mimic Arduino classes
#include <iostream> 
#include <unistd.h> 
#include <fcntl.h>


using namespace std;

class Serial_t {
    public:
    static void begin(int speed) { }
};
Serial_t Serial;


#define PRINT(v) ( std::cout << v )
#define PRINTLN(v) ( std::cout << v << "\n" )
#define PRINT_IP(v) ( std::cout << "local ip goes here" << "\n" )
#define CPRINT(v) ( std::cout << v )

//----------------------------------------------------------------------------------
// Timer class
// Stubs out begin() and update() functions, since PC already has valid time
// and no need to use NTS (network time service) 

class Timer_t {
public:
    Timer_t() {}
    bool begin(int time_zone_offset) { return true; }
    bool update() { return true; }
    unsigned long time() { return (::time(0) - (5 * 3600)); }
    int getHours() {
        time_t now = time();
        struct tm *t = gmtime(&now);
        return t->tm_hour;
    }
    int getMinutes() {
        time_t now = time();
        struct tm *t = gmtime(&now);
        return t->tm_min;
    }

};

Timer_t timer;


//----------------------------------------------------------------------------------
// WiFi classes

//----------------------------------------------------------------------------------

#define WL_IDLE_STATUS 0
#define WL_CONNECTED 1

class WiFi_t {
public:
    string _ssid;
    string _pwd;
    int _status;

    WiFi_t() { _status = WL_IDLE_STATUS; }
    int begin(const char *ssid, const char *pwd) {
        _ssid = ssid;
        _pwd = pwd;
        _status = WL_CONNECTED;
        return _status;
    }
    int status() { return _status; }
    const char *SSID() { return _ssid.c_str(); }
    const char *RSSI() { return _pwd.c_str(); }
};
WiFi_t WiFi;
/*
class WiFiClient {
public:
    int _fd;

    WiFiClient(int fd) { _fd = fd; }  // fd opened by server, uses it
    WiFiClient() { _fd = 0; }
    // Destructor does not close file so object may be copied/assigned easily
    // You must call stop() to explicitly close the file
    ~WiFiClient() { }

    bool connected() { return (_fd > 0); }

    bool connect(const char *server, int port) {
        struct addrinfo hints;
        struct addrinfo *result, *rp;
        int s;
        char port_str[20];

        sprintf(port_str, "%d", port);
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        //hints.ai_flags = AI_PASSIVE;     //For server: wildcard IP address

        s = getaddrinfo(server, port_str, &hints, &result);
        if (s != 0) {
            PRINTLN(gai_strerror(s));
            return(false);
        }

       // getaddrinfo() returns a list of address structures.
        for (rp = result; rp != NULL; rp = rp->ai_next) {
            _fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (_fd == -1)
                continue;

            if (::connect(_fd, rp->ai_addr, rp->ai_addrlen) == 0)
                break;                  // Success
            close(_fd);
            _fd = 0;
        }

        if (rp == NULL) {
            PRINT("Could not connect\n");
            return(false);
        }

        freeaddrinfo(result);
        return(true);
    }

    void stop() {
        if (_fd > 0) {
            close(_fd);
            _fd = 0;
        }
    }

    bool available() { char c; return (1 == recv( _fd , &c, 1, MSG_PEEK | MSG_DONTWAIT)); }

    char read() {
        char c;
        if (1 == recv( _fd , &c, 1, 0))
            return c;
        return 0;
    }
    void print(const char*v) {
        if (_fd > 0) send(_fd , v, strlen(v) , 0 ); 
    }
    void print(char v) {
        if (_fd > 0) send(_fd , &v, 1 , 0 ); 
    }
    void print(uint8_t v) { print((int)v);}
    void print(int v) {
        char s[20];
        sprintf(s, "%d", v);
        print(s);
    }
    void print(float v) {
        char s[20];
        sprintf(s, "%.2f", v);
        print(s);
    }

}; */

//----------------------------------------------------------------------------------
void errexit(const char *errmsg) {
    std::cout << errmsg << "\n";
    exit(1);
}

void processClient(WiFiClient &client);


//----------------------------------------------------------------------------------
// Misc functions + main()

#define A0 0

int analogRead(int port) {
    return 200;
}
void delay (time_t millisecs) {
    usleep(1000 * millisecs);
}
unsigned long millis(){
    struct timespec _t;
    clock_gettime(CLOCK_REALTIME, &_t);
    unsigned long tm = _t.tv_sec*1000 + _t.tv_nsec/1.0e6;
    return tm;
}
bool setRelayState(uint8_t zone_idx, bool tgt) {
    static bool relay_state[4] = {0,0,0,0};
    if (zone_idx > 3)
        return false;
    if (relay_state[zone_idx] == tgt)
        return true;
    relay_state[zone_idx] = tgt;
    printf("setting relay %s\n", tgt ? "ON" : "OFF");
    return true;
}

void setup();
void loop();

int
main(int argc, char *argv[])
{
    setup();
    while(1)
        loop();
    return(0);
}

#ifdef __WIN32__
#include "sockw32.h"
#else
#include "socknix.h"
#endif 
