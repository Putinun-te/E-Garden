//#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>

#include "secrets.h"
// Replace with your network credentials
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// Create AsyncWebServer object on port 80
AsyncWebServer server(8888);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<!DOCTYPE html PUBLIC "" ""><HTML lang="en"><HEAD><META content="IE=11.0000" 
    http-equiv="X-UA-Compatible">
    <head>   
    <META charset="UTF-8">   
    <META name="GENERATOR" content="MSHTML 11.00.10570.1001">   
    <META name="Author" content="">   
    <META name="Keywords" content="">   
        <META name="Description" content="">   
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <script src="https://kit.fontawesome.com/085527e9ea.js" crossorigin="anonymous"></script>

            <TITLE>E-Garden</TITLE>
            <link href="https://fonts.googleapis.com/icon?family=Material+Icons"rel="stylesheet">
            <link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Material+Symbols+Outlined:opsz,wght,FILL,GRAD@20..48,100..700,0..1,-50..200" />
            <link rel="stylesheet" href="stlye.css">
            <style>
@import url('https://fonts.googleapis.com/css2?family=Poppins:ital,wght@0,100;0,200;0,300;0,400;0,500;0,600;0,700;1,800;1,900&display=swap');

:root {
    --color-primary: #7380ec;
    --color-danger: #ff7782;
    --color-success: #41f1b6;
    --color-warning: #ffbb55;
    --color-white: : #fcfcfc;
    --color-info-dark: #afaeae;
    --color-info-light: #dce1eb;
    --color-dark: #474343;
    --color-light: rgba(132,139,200,0,18);
    --color-primary-variant: #3f85d2;
    --color-dark-variant: #0f6a1a;
    --color-background: #99ccff;

    --card-border-radius: 2rem;
    --border-radius-1: 0.4rem;
    --border-radius-2: 0.8rem;
    --border-radius-3: 1.2rem;

    --card-padding: 1.8rem;
    --padding-1: 1.2rem;

    --box-shadow:0.2rem;
}

/*
r{
   margin: 0; 
   padding: 0;
   list-style: none;
   text-decoration: none;
}
*/
html{
    font-size: 14px;
}

body{
    width: 100vw;
    height: 100vh;
    font-family: poppins, sans-serif;
    font-size: 0.88rem;
    background: var(--color-background);
    user-select: none;
    overflow-x: hidden;
    color: var(--color-dark);
}

a{
   color: var(--color-dark); 
}

h1 {
    font-weight: 400;
    font-size: 3rem;
  }

h2{
    text-align: center;
    font-size: 4rem;
    color: black;
}

h3{
    font-size: 2rem;
}

h4{
    font-size: 2.8rem;
    color:black;
}
h5{
    font-weight: 600;
    font-size: 3rem;
    color:black;
}
h6{
    font-size: 1.4rem; 
}
h7{
    font-size: 1.4rem;
    font-weight: 600;
    color: #474343; 
}
้้
p { font-size: 1.2rem; }
.units { font-size: 1.2rem; }
.readings { font-size: 2.0rem; }

small{
    font-size: 0.75rem;
}
/*============== Timezone ===============*/
.datetime{
    color: #474343;
    font-family: poppins, sans-serif;
    width: 340px;
    padding: 15px 10px;
    border: 3px solid ;
    border-radius: 5px;
    transition: 0.5s;
    transition-property: var(--color-background);
}

.datetime:hover{
    background: #afaeae;
    box-shadow: 0 0 30px #2E94E3;
}

.date{
    font-size: 20px;
    font-weight: 600;
    text-align: center;
    letter-spacing: 3px;
}
.time{
    font-size: 60px;
    display: flex;
    justify-content: center;
    align-items: center;
}
.time span:not(:last-child){
    position: relative;
    margin: 0 6px;
    font-weight: 600;
    text-align: center;
    letter-spacing: 3px;
}
.time span:last-child{
    background: #99ccff;
    font-size: 30px;
    font-weight: 600;
    text-transform: uppercase;
    margin-top: 10px;
    padding: 0 5px;
    border-radius: 3px;
}

/*============== MAIN ===============*/
main {
    margin-top: 1.4rem;
    display: grid;
    grid-template-columns: auto auto auto;
    gap: 10px;
       background: #dce1eb;
    padding: 1.8rem;
    border-radius: 2rem;
    margin-top: 1rem;
    box-shadow: 0.2rem;
    transition: all 300ms ease; 
    background-color: #dce1eb;
    border: 1px solid #474343;
    text-align: center;
    font-size: 30px;
    font-size: 1.2rem; 
    box-shadow: none;
}
     
/*============== On-Off ===============*/


input[type="checkbox"] {
    
    position: relative;
    width: 100px;
    height: 40px;
    background: #bdc3c7;
    -webkit-appearance: none;
    border-radius: 20px;
    
    transition: .4s;
    box-shadow: inset 0 0 5px rgba(0, 0, 0, 0.2);
    cursor: pointer;
}
input:checked[type="checkbox"]{
    background: #2c3e50;
}
input[type="checkbox"]::before{
    z-index: 2;
    position: absolute;
    content: "";
    left: 0;
    width: 40px;
    height: 40px;
    background: #fff;
    border-radius: 50%;
    transform: scale(1.1);
    box-shadow: 0 2px 5px rgba(0, 0, 0, 0.2);
    transition: .4s;
}
input:checked[type="checkbox"]::before{
    left: 60px;
}
.toggle{
    position: relative;
    display: inline;
}
label{
    position: absolute;
    color: #fff;
    font-weight: 600;
    font-size: 20px;
    pointer-events: none;
}
.onbtn{
    bottom: 15px;
    left: 15px;
}
.ofbtn{
    bottom: 15px;
    right: 14px;
    color: #34496e;
}
    </style>
   </HEAD>  
        <body onload="initClock()">
            <div class="container">
                <aside>
                    <div class="top">  
                            <h2>E-Garden</h2>
                    </div>
                </aside>
               <center> <div class="datetime">
                    <div class="date">
                        <span id="dayname">Day</span>
                        <span id="month">Month</span>
                        <span id="daynum">00</span>
                        <span id="year">Year</span>
                    </div>
                    <div class="time">
                        <span id="hour">00</span>:
                        <span id="minutes">00</span>:
                        <span id="seconds">00</span>
                        <span id="period">AM</span></center>
                    </div>
                </div>
                <script type="text/javascript">
                    function updateClock(){
                        var now = new Date();
                        var dname = now.getDay(),
                            mo = now.getMonth(),
                            dnum = now.getDate(),
                            yr = now.getFullYear(),
                            hou = now.getHours(),
                            min = now.getMinutes(),
                            sec = now.getSeconds(),
                            pe = "AM";

                            if(hou == 0){
                                hou = 12;
                            }
                            if(hou > 12){
                                hou = hou - 12;
                                pe = "PM";
                            }

                            Number.prototype.pad = function(digits){
                                for(var n = this.toString(); n.length < digits; n = 0 + n);
                                return n;
                            }

                            var months = ["January", "February", "March", "April", "May", "June", "July", "Augest", "September", "Octorber", "November", "December"];
                            var week = ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"];
                            var ids = ["dayname", "month", "daynum", "year", "hour", "minutes", "seconds", "period"];
                            var values = [week[dname], months[mo], dnum.pad(2), yr, hou.pad(2), min.pad(2), sec.pad(2), pe];
                            for(var i = 0; i < ids.length; i++)
                            document.getElementById(ids[i]).firstChild.nodeValue = values[i];
                    }
                    function initClock(){
                        updateClock();
                        window.setInterval("updateClock()",1);
                    }

                </script>
            </div> 
    <center> <btn>
        <div class="toggleBox">
            <h3>sprayer</h3>
            <h7>Manual</h7> 
            <br>
            <div class="toggle">
                <input type="checkbox">
                <label for="" class="onbtn">On</label>
                <label for="" class="ofbtn">Off</label>
            </div>
        </div>
        <br>
        <br>
        <h7>Automatic</h7>
        <br>
    </btn> </center>
    <br>
    <br>
    <center> 
    </center>
         <h4>Dashboard</h4>
         <center> <main>
            <p>
              <i class="fa fa-lightbulb-o" style="color:#059e8a;"></i> light: <span id="Light" class="readings">%Light%</span>
              <sup>㏓</sup>
            </p>
            <p>
              <i class="fas fa-tint" style="color:#00add6;"></i> humidity: <span id="Humid" class="readings">%Humid%</span>
              <sup>%</sup>
            </p>
            <p>
                <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> Temperature: <span id="temperature" class="readings">%Temp%</span>
                <sup>&deg;C</sup>
              </p>
          </main></center>
                
                <script>
                    setInterval(updateValues, 10000, "Light");
                    setInterval(updateValues, 10000, "Humid");
                    setInterval(updateValues, 10000, "Temp");
                    setInterval(updateValues, 10000, "rssi");
                     
                    function updateValues(value) {
                      var xhttp = new XMLHttpRequest();
                      xhttp.onreadystatechange = function() {
                        if (this.readyState == 4 && this.status == 200) {
                          document.getElementById(value).innerHTML = this.responseText;
                        }
                      };
                      xhttp.open("GET", "/" + value, true);
                      xhttp.send();
                    }
         </script> 

        </body>
    </html>
  </html>)rawliteral";

// Replaces placeholder with DHT values
String processor(const String& var){

  return String();
}

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);

  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  // Start server
  server.begin();
}
 
void loop(){
  
}
