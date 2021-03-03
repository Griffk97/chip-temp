#include <winsock2.h>
#include <ws2def.h>
#include <wtsapi32.h>
#include <ws2tcpip.h>
#include <iostream> 

#define PORT 80  // http port

void errexit(const char *errmsg) {
    std::cout << errmsg << "\n";
    exit(1);
}

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
        WSADATA wsadata;
        SOCKET ClientSocket = INVALID_SOCKET;
        int iResult;
        char recvbuf[512];
        int recvbuflen = 512;
        struct addrinfo hints;
        struct addrinfo *result, *rp;
        char port_str[20];
        sprintf(port_str, "%d", port);

        iResult = WSAStartup(MAKEWORD(2,2), &wsadata);
         if (iResult != 0) {
            PRINTLN("WSAStartup failed with error: %d\n", iResult);
            return(false);
        }

        ZeroMemory(&hints,sizeof(hints));   
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        //hints.ai_flags = AI_PASSIVE;     //For server: wildcard IP address

        iResult = getaddrinfo(server, port_str, &hints, &result);
        if (iResult != 0) {
            PRINTLN(gai_strerror(s));
            WSACleanup();
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

};

void processWiFi(Cfg_t &cfg) {
    static int server_fd = 0;
    struct sockaddr_in address; 
    struct sockaddr *addrptr = (struct sockaddr*) &address;
    int opt = 1; 
    socklen_t addrlen = sizeof(address); 

    if ( WiFi.status() != WL_CONNECTED)
        WiFi.begin(cfg.ssid, cfg.pass);

    if (server_fd == 0) {

        // Creating socket file descriptor 
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { 
            errexit("socket()"); 
        } 

        // Forcefully attaching socket to the port 8080 
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 
                                                      &opt, sizeof(opt))) { 
            errexit("setsockopt"); 
        } 
        address.sin_family = AF_INET; 
        address.sin_addr.s_addr = INADDR_ANY; 
        address.sin_port = htons( PORT );           // 80 is http port

        // Forcefully attaching socket to the port 8080 
        if (bind(server_fd, addrptr, sizeof(address))<0) { 
            errexit("bind"); 
        } 
        int flags = fcntl(server_fd, F_GETFL);
        fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);
  
        if (listen(server_fd, 3) < 0) { 
            errexit("listen"); 
        } 
    }
    int client_fd = accept(server_fd, addrptr, &addrlen);

    if (client_fd > 0) {
        WiFiClient client(client_fd);
        processClient(client);
        close(client_fd);
    }
}
