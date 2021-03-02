import socketserver
import time
import http.client
import json

weather_last_time = 0.0
weather_temp = 0.0

class MyTCPHandler(socketserver.StreamRequestHandler):
    def setup(self):
        super(MyTCPHandler, self).setup()
        self.weather_conn = http.client.HTTPSConnection("climacell-microweather-v1.p.rapidapi.com")
        self.weather_cmd = "/weather/realtime?lat=41.1408&lon=-73.2613&fields=temp"
        self.weather_headers = {
            'x-rapidapi-key': "9533dd54bamsh149c4fd1bc505aap12a50bjsn96145216d517",
            'x-rapidapi-host': "climacell-microweather-v1.p.rapidapi.com"
        }

    def handle(self):
        global weather_last_time
        global weather_temp        
# Need to check if remote closed connection and/or whether readline() returned any data
        raw = self.rfile.readline()
        temp = str(raw.strip(), encoding='utf-8')
        tm = time.time();
        # Update outdoor temp every 20 minutes
        if (tm > (1200 + weather_last_time)) :
            weather_last_time = tm
            self.weather_conn.request("GET", self.weather_cmd, headers=self.weather_headers)
            res = self.weather_conn.getresponse()
            myjson = res.read().decode('utf8')
            #print(myjson)
            weather_temp = (float(json.loads(myjson)['temp']['value'])*9/5)+32

        s = time.strftime("%Y/%m/%d %H:%M:%S", time.localtime()) + "," +str(weather_temp)+ ","+temp+"\n"
        print(s)
        log = open("./tempdata.txt", 'a')
        log.write(s)
        log.close()


if __name__ == "__main__":
    HOST, PORT = "0.0.0.0", 3001
    with socketserver.TCPServer((HOST, PORT), MyTCPHandler) as server:
        server.serve_forever()
