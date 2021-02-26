#include <iostream> 
#include <string>
#include <string.h> 
#define PORT 80

// Mimic Arduino classes
#include <unistd.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 

class Serial_t {
    public:
    static void begin(int) { }
    static void print(const char*v) { std::cout << v; }
    static void print(int v) { std::cout << v; }
    static void print(unsigned char v) { std::cout << v; }
    static void println(const char*v) { std::cout << v << std::endl; }
    static void println(int v) { std::cout << v << std::endl; }
    static void println(unsigned char v) { std::cout << v << std::endl; }
};
Serial_t Serial;

struct MsgBlock {
    char *cmd;
    char *source;
    char *protocol;
    int content_length;
    char *data;
};

char empty_string[1] = {0};
const char *html_ok = "HTTP/1.1 200 OK\n";

const char *front_page = 
"<!DOCTYPE html>\n\
<html>\n\
  <head>\n\
    <meta charset='utf-8'>\n\
    <title>Post method example</title>\n\
    <style>\n\
      div {\n\
        margin-bottom: 20px;\n\
      }\n\
\n\
      label {\n\
        display: inline-block;\n\
        text-align: right;\n\
        padding-right: 10px;\n\
      }\n\
\n\
    </style>\n\
  </head>\n\
  <body>\n\
<form  method='POST' action='/foo'>\n\
  <div>\n\
    <label for='say'>What greeting do you want to say?</label>\n\
    <input name='say' id='say' value='Hi'>\n\
  </div>\n\
  <div>\n\
    <label for='to'>Who do you want to say it to?</label>\n\
    <input name='to' id='to' value='Mom'>\n\
  </div>\n\
  <div>\n\
    <button>Send my greetings</button>\n\
  </div>\n\
</form>\n\
  </body>\n\
</html>\n\
";

void errexit(const char *errmsg) {
    Serial.println(errmsg);
    exit(1);
}
void send_str(int client, const char*s) {
    send(client , s, strlen(s) , 0 ); 
}

int parse_eol(char *nextline, char **fields, int line, int nfields, struct MsgBlock *msg) {
    if (line == 0) {
        if (nfields < 3) {
            Serial.print("First line does not have 3 fields\n");
            return 1;
        }
        msg->cmd = fields[0];
        msg->source = fields[1];
        msg->protocol = fields[2];
        return 0;
    }
    int linelen = nextline - fields[0] - 1;

    if (linelen == 1) {
        msg->data = nextline;
        Serial.print("data=");
        Serial.println(msg->data);
        return 0;
    }
    if (nfields == 2 && 0 == strcmp(fields[0], "Content-Length:")) {
        msg->content_length = atoi(fields[1]);
        return 0;
    }   
    return 0;
}

// return 0 on success, 1 on error
int parse_buffer(char *buff, int len, struct MsgBlock *msg) {
    int line = 0;
    int nfields = 0;
    char *fields[3] = {empty_string, empty_string, empty_string};

    Serial.print("Received ");
    Serial.print(len);
    Serial.print(",");
    Serial.println(buff);

    char *endbuff = buff+len;
    fields[0] = buff;
    for(; buff <= endbuff; buff++) {
        char c = *buff;
        if (c == ' ' || c == '\n'|| c == 0) {
            // end of field 
            *buff = 0;
            if (nfields < 3)
                nfields++;
            // end of line
            if (c == '\n' || c == 0) {
                if (parse_eol(buff+1, fields, line, nfields, msg))
                    return 1;
                nfields = 0;
                line++;
            }
            if (nfields < 3)
                fields[nfields] = buff + 1;
        }
    }
    Serial.print("returning from parse\n");
    return 0;
}

void process_client(int client) {
    struct MsgBlock msg;
    char buffer[1024]; 
    int n;

    msg.cmd = msg.source = msg.protocol = msg.data = empty_string;
    msg.content_length = 0;

    Serial.print("Connection: ");
    Serial.println(client);

    n = read( client , buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        if (n < 0)
            perror("read()");
        else
            Serial.print("Client closed connection\n");
        return;
    }
    buffer[n] = 0;  // terminate string

    if (n == sizeof(buffer)) {
        Serial.print("Buffer overflowed\n");
        return;
    }

    if (parse_buffer(buffer, n, &msg)) {
        return;
    }
    if (strcmp(msg.cmd, "GET") == 0) {
        if (strcmp(msg.source, "/") == 0) {
            Serial.print("Sending front page ...\n");
            send_str(client, front_page); 
            return;
        }
    }
    if (strcmp(msg.cmd, "POST") == 0) {
    }
    send_str(client , html_ok);
}

void run_server() {
    int server_fd, client; 
    struct sockaddr_in address; 
    struct sockaddr *addrptr = (struct sockaddr*) &address;
    int opt = 1; 
    socklen_t addrlen = sizeof(address); 
       
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
    address.sin_port = htons( PORT ); 
       
    // Forcefully attaching socket to the port 8080 
    if (bind(server_fd, addrptr,  
                                 sizeof(address))<0) { 
        errexit("bind"); 
    } 

    if (listen(server_fd, 3) < 0) { 
        errexit("listen"); 
    } 
    while (0 < (client = accept(server_fd, addrptr, &addrlen))) { 
        process_client(client);
        close(client);
    }
} 

int main(int argc, char const *argv[]) 
{
    run_server();
    return 0; 
}

