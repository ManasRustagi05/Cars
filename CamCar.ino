/* 
  ESP32-CAM 4WD Robot Car with L298N Motor Driver
  ------------------------------------------------
  - Live MJPEG video stream (http://<ip>:81/stream)
  - Web-based touch/click D-pad remote control with Speed Slider (http://<ip>/)
  - Integrated Flashlight Toggle switch (GPIO 4)
  - Fail-safe auto-stop if connection drops
  - Brownout workaround for motor current spikes

  WIRING MAP:
    ESP32-CAM GPIO12 -> L298N IN1
    ESP32-CAM GPIO13 -> L298N IN2
    ESP32-CAM GPIO14 -> L298N IN3
    ESP32-CAM GPIO15 -> L298N IN4
    ESP32-CAM GPIO2  -> L298N ENA and ENB bridged (PWM speed)
    ESP32-CAM GPIO4  -> Onboard High-Power Flash LED (Flashlight)
*/

#include "esp_camera.h"
#include "esp_http_server.h"
#include <WiFi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_arduino_version.h"

// ---------- WiFi Access Point credentials ----------
const char* AP_SSID = "ESP32-CAM-CAR";
const char* AP_PASS = "12345678";

// ---------- Motor pins ----------
#define IN1 12
#define IN2 13
#define IN3 14
#define IN4 15

// ---------- Speed (PWM on bridged ENA+ENB) ----------
#define PWM_PIN 2        
#define PWM_CHANNEL 0
#define PWM_FREQ 5000    
#define PWM_RES 8        
volatile uint8_t currentSpeed = 200; 

// ---------- Flashlight Pin ----------
#define FLASH_PIN 4

// ---------- Failsafe ----------
volatile unsigned long lastCommandMillis = 0;
const unsigned long CMD_TIMEOUT_MS = 400; 

// ---------- AI-Thinker ESP32-CAM pin map ----------
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;

void applySpeedPWM(uint8_t speed) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(PWM_PIN, speed);
#else
    ledcWrite(PWM_CHANNEL, speed);
#endif
}

// =====================================================
// Motor primitives
// =====================================================
void leftForward()   { digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);  }
void leftBackward()  { digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); }
void leftStop()      { digitalWrite(IN1, LOW);  digitalWrite(IN2, LOW);  }

void rightForward()  { digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);  }
void rightBackward() { digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); }
void rightStop()     { digitalWrite(IN3, LOW);  digitalWrite(IN4, LOW);  }

void carStop()       { leftStop(); rightStop(); }
void carForward()    { leftForward(); rightForward(); }
void carBackward()   { leftBackward(); rightBackward(); }
void carLeft()       { leftBackward(); rightForward(); } 
void carRight()      { leftForward(); rightBackward();  } 

void applyCommand(const String& dir) {
    lastCommandMillis = millis();
    if (dir == "F")      carForward();
    else if (dir == "B") carBackward();
    else if (dir == "L") carLeft();
    else if (dir == "R") carRight();
    else if (dir == "S") carStop();
}

// =====================================================
// Web control page
// =====================================================
const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-CAM Car</title>
<style>
 body{margin:0;background:#111;color:#fff;font-family:sans-serif;text-align:center}
 img{width:100%;max-width:480px;border-bottom:2px solid #333;min-height:240px;background:#222}
 .pad{display:grid;grid-template-columns:80px 80px 80px;grid-gap:10px;justify-content:center;margin-top:15px}
 button{font-size:24px;padding:20px 0;border-radius:12px;border:none;background:#2b6cff;color:#fff;user-select:none;-webkit-user-select:none}
 button:active{background:#1a4fce}
 .stopBtn{background:#d92626}
 .slider-container{margin:20px auto; max-width:300px; background:#222; padding:15px; border-radius:12px; box-sizing:border-box}
 .slider{width:100%; margin-top:10px; accent-color:#2b6cff}
 .flashBtn{width:100%; margin-top:15px; font-size:16px; padding:12px 0; background:#2b6cff; font-weight:bold; cursor:pointer}
</style></head><body>
 <img src="" id="cam">
 
 <div class="slider-container">
   <label style="font-size:18px; font-weight:bold;">Speed: <span id="speedVal">200</span></label>
   <input type="range" min="100" max="255" value="200" class="slider" id="speedSlider" 
          oninput="document.getElementById('speedVal').innerText=this.value" 
          onchange="setSpeed(this.value)">
   
   <button class="flashBtn" id="flashBtn" onclick="toggleFlash()">Flashlight: OFF</button>
 </div>

 <div class="pad">
   <div></div><button ontouchstart="go('F')" ontouchend="go('S')" onmousedown="go('F')" onmouseup="go('S')">&#8593;</button><div></div>
   <button ontouchstart="go('L')" ontouchend="go('S')" onmousedown="go('L')" onmouseup="go('S')">&#8592;</button>
   <button class="stopBtn" onclick="go('S')">STOP</button>
   <button ontouchstart="go('R')" ontouchend="go('S')" onmousedown="go('R')" onmouseup="go('S')">&#8594;</button>
   <div></div><button ontouchstart="go('B')" ontouchend="go('S')" onmousedown="go('B')" onmouseup="go('S')">&#8595;</button><div></div>
 </div>
 <script>
  let iv=null;
  let currentDir='S';
  let flashOn=false;

  window.onload = function() {
    document.getElementById('cam').src = window.location.protocol + '//' + window.location.hostname + ':81/stream';
  };

  function go(d){
    currentDir=d;
    fetch('/control?dir='+d).catch(()=>{});
    if(iv) clearInterval(iv);
    if(d!=='S'){
      iv=setInterval(()=>fetch('/control?dir='+currentDir).catch(()=>{}),200);
    }
  }
  function setSpeed(v){
    fetch('/control?speed='+v).catch(()=>{});
  }
  function toggleFlash(){
    flashOn = !flashOn;
    const btn = document.getElementById('flashBtn');
    if(flashOn) {
      btn.innerText = "Flashlight: ON";
      btn.style.background = "#ffcc00";
      btn.style.color = "#000";
    } else {
      btn.innerText = "Flashlight: OFF";
      btn.style.background = "#2b6cff";
      btn.style.color = "#fff";
    }
    fetch('/control?flash=' + (flashOn ? '1' : '0')).catch(()=>{});
  }
  window.addEventListener('blur', ()=>go('S'));
  document.addEventListener('contextmenu', e=>e.preventDefault());
 </script></body></html>
)HTML";

// =====================================================
// HTTP handlers
// =====================================================
static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

static esp_err_t control_handler(httpd_req_t *req) {
    char query[64];
    char dir[4] = "";
    char speedStr[8] = "";
    char flashStr[4] = "";

    if (httpd_req_get_url_query_len(req) > 0 && httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        // Handle directions
        if (httpd_query_key_value(query, "dir", dir, sizeof(dir)) == ESP_OK) {
            applyCommand(String(dir));
        }
        // Handle speed slider changes
        if (httpd_query_key_value(query, "speed", speedStr, sizeof(speedStr)) == ESP_OK) {
            int speedVal = atoi(speedStr);
            if (speedVal >= 0 && speedVal <= 255) {
                currentSpeed = speedVal;
                applySpeedPWM(currentSpeed);
            }
        }
        // Handle flashlight state updates
        if (httpd_query_key_value(query, "flash", flashStr, sizeof(flashStr)) == ESP_OK) {
            if (strcmp(flashStr, "1") == 0) {
                digitalWrite(FLASH_PIN, HIGH);
            } else if (strcmp(flashStr, "0") == 0) {
                digitalWrite(FLASH_PIN, LOW);
            }
        }
    }
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "OK", 2);
}

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    char part_buf[64];
    res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    if (res != ESP_OK) return res;

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed, retrying...");
            vTaskDelay(50 / portTICK_PERIOD_MS);
            continue;
        }
        if (fb->format != PIXFORMAT_JPEG) {
            Serial.println("Non-JPEG frame skipped");
            esp_camera_fb_return(fb);
            continue;
        }
        if (res == ESP_OK) res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
        if (res == ESP_OK) {
            size_t hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, fb->len);
            res = httpd_resp_send_chunk(req, part_buf, hlen);
        }
        if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
        esp_camera_fb_return(fb);
        fb = NULL;
        if (res != ESP_OK) break; 
    }
    return res;
}

void startServers() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 8;

    httpd_uri_t index_uri =   { "/",        HTTP_GET, index_handler,   NULL };
    httpd_uri_t control_uri = { "/control", HTTP_GET, control_handler, NULL };

    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &control_uri);
    }

    config.server_port = 81;
    config.ctrl_port = 81;
    httpd_uri_t stream_uri = { "/stream", HTTP_GET, stream_handler, NULL };
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
    }
}

bool initCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    
    if (psramFound()) {
        config.frame_size = FRAMESIZE_VGA; 
        config.jpeg_quality = 12;
        config.fb_count = 2;
    } else {
        config.frame_size = FRAMESIZE_QVGA; 
        config.jpeg_quality = 15;
        config.fb_count = 1;
    }

    for (int attempt = 1; attempt <= 3; attempt++) {
        esp_err_t err = esp_camera_init(&config);
        if (err == ESP_OK) return true;
        Serial.printf("Camera init failed (0x%x), attempt %d/3\n", err, attempt);
        delay(300);
    }
    return false;
}

void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
    Serial.begin(115200);

    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    pinMode(IN3, OUTPUT);
    pinMode(IN4, OUTPUT);
    carStop();

    // Initialize the Flashlight LED pin and keep it turned off initially
    pinMode(FLASH_PIN, OUTPUT);
    digitalWrite(FLASH_PIN, LOW);

    // Initialize ESP32 hardware PWM parameters on the motor pin (GPIO 2)
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(PWM_PIN, PWM_FREQ, PWM_RES);
#else
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RES);
    ledcAttachPin(PWM_PIN, PWM_CHANNEL);
#endif
    applySpeedPWM(currentSpeed); 

    if (!initCamera()) {
        Serial.println("Camera init failed after 3 attempts. Restarting...");
        delay(1000);
        ESP.restart();
    }

    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    startServers();
    lastCommandMillis = millis();
}

void loop() {
    if (millis() - lastCommandMillis > CMD_TIMEOUT_MS) {
        carStop();
    }
    delay(50);
}
