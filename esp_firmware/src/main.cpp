#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include "img_converters.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "driver/rtc_io.h"
#include "density.h"          // the vision algorithm (lib/density)

// ============================================================
// Flow (every wake):
//   ATmega pulls the shared line LOW -> ESP32 wakes (ext0)
//   -> camera photo -> Wi-Fi -> HTTP POST of the JPEG to the laptop
//   -> I2C slave up -> ESP32 pulses the shared line to wake the ATmega
//   -> ATmega sends GET_DATA and reads the 1-byte density (0..3)
//   -> deep sleep
//
// Shared wake line (open-drain style, active LOW):
//   ESP32 GPIO13  <->  ATmega pin 15 (D9)
//   OUTPUT LOW = assert, INPUT = release (external 3.3 V pull-up).
//   NEVER drive this pin HIGH.
//
// I2C slave: SDA = GPIO15, SCL = GPIO14 (SD-card pins: no SD card inserted)
// ============================================================

// ---- Wi-Fi / laptop: set WIFI_SSID, WIFI_PASSWORD and UPLOAD_URL in include/secrets.h
//      (secrets.h is ignored by git, so the password never reaches the repository)
#include "secrets.h"
#define WIFI_TIMEOUT_MS   15000
#define HTTP_TIMEOUT_MS   8000
#define JPEG_QUALITY      80      // 0-100, for frame2jpg

#define I2C_SLAVE_ADDRESS   0x08
#define I2C_SDA             15
#define I2C_SCL             14

#define SHARED_WAKE_GPIO      13
#define SHARED_WAKE_RTC_GPIO  GPIO_NUM_13

#define GET_DATA_COMMAND    1
#define WAKE_PULSE_MS       100
#define ATMEGA_REQUEST_TIMEOUT_MS 10000

// Hard limit for one whole wake (photo + Wi-Fi + I2C). If anything hangs the
// chip restarts; a restart is not an ext0 wake, so setup() goes to deep sleep.
// Must stay below the ATmega's own timeout (6 x 8 s watchdog ticks).
#define WAKE_WATCHDOG_MS    40000

// ---- AI Thinker ESP32-CAM (same pin map as the CameraWebServer example)
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

volatile bool getDataCommandReceived = false;
volatile bool dataWasSent            = false;

// Traffic density sent to the ATmega: 0 = low, 1 = medium, 2 = high, 3 = full
volatile uint8_t density = 0;

unsigned long waitingSince = 0;


// ============================================================
// I2C callbacks
// ============================================================

void onReceive(int numberOfBytes)
{
  while (Wire.available())
  {
    byte command = Wire.read();
    if (command == GET_DATA_COMMAND)
    {
      getDataCommandReceived = true;
    }
  }
}

void onRequest()
{
  Wire.write((byte)density);

  dataWasSent = true;
}


// ============================================================
// Shared line helpers
// ============================================================

bool waitForLineHigh(uint16_t timeoutMs)
{
  unsigned long start = millis();
  while (digitalRead(SHARED_WAKE_GPIO) == LOW)
  {
    if (millis() - start > timeoutMs) return false;
    delay(1);
  }
  return true;
}

// ESP32 -> ATmega signal: pull the line LOW briefly, then release
void wakeATmega()
{
  Serial.println("Waking ATmega...");

  digitalWrite(SHARED_WAKE_GPIO, LOW);   // latch LOW
  pinMode(SHARED_WAKE_GPIO, OUTPUT);     // pull line LOW

  delay(WAKE_PULSE_MS);

  pinMode(SHARED_WAKE_GPIO, INPUT);      // release

  waitForLineHigh(500);
}

void onWakeWatchdog(void *arg)
{
  esp_restart();
}

void startWakeWatchdog()
{
  static esp_timer_handle_t timer;
  esp_timer_create_args_t args = {};
  args.callback = &onWakeWatchdog;
  args.name     = "wake_wdt";
  esp_timer_create(&args, &timer);
  esp_timer_start_once(timer, (uint64_t)WAKE_WATCHDOG_MS * 1000);
}

void goToDeepSleep()
{
  Serial.println("Preparing for deep sleep...");

  // Release the shared line and make sure it's HIGH,
  // otherwise the LOW-level wake source would fire immediately.
  pinMode(SHARED_WAKE_GPIO, INPUT);
  waitForLineHigh(2000);

  esp_sleep_enable_ext0_wakeup(SHARED_WAKE_RTC_GPIO, 0);   // wake on LOW

  rtc_gpio_pullup_dis(SHARED_WAKE_RTC_GPIO);
  rtc_gpio_pulldown_dis(SHARED_WAKE_RTC_GPIO);

  Serial.println("ESP32 entering deep sleep.");
  Serial.flush();
  delay(20);

  esp_deep_sleep_start();
}


// ============================================================
// Camera + Wi-Fi upload
// ============================================================

bool cameraInit()
{
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  // Settings taken from the working CameraWebServer on this module: the OV2640
  // clone cannot do JPEG, so capture RGB565 at 160x120 from DRAM with a slower
  // XCLK (avoids FB-OVF), and convert to JPEG on the ESP32 before sending.
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size   = FRAMESIZE_QQVGA;
  config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location  = CAMERA_FB_IN_DRAM;
  config.jpeg_quality = 12;
  config.fb_count     = 1;

  if (esp_camera_init(&config) != ESP_OK) return false;

  sensor_t *s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QQVGA);
  s->set_vflip(s, 0);
  s->set_hmirror(s, 0);
  return true;
}

bool wifiConnect()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    if (millis() - start > WIFI_TIMEOUT_MS) return false;
    delay(100);
  }
  Serial.print("Wi-Fi connected, IP ");
  Serial.println(WiFi.localIP());
  return true;
}

// Take one photo and POST it to the laptop. Returns true if the laptop got it.
bool takeAndSendPhoto()
{
  if (!cameraInit())
  {
    Serial.println("Camera init failed");
    return false;
  }

  // First frames after power-up have bad exposure: throw a few away
  for (int i = 0; i < 3; i++)
  {
    camera_fb_t *warm = esp_camera_fb_get();
    if (warm) esp_camera_fb_return(warm);
    delay(100);
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb)
  {
    Serial.println("Capture failed");
    esp_camera_deinit();
    return false;
  }
  Serial.printf("Photo taken: %ux%u, %u raw bytes\n",
                (unsigned)fb->width, (unsigned)fb->height, (unsigned)fb->len);

  // ---- vision: traffic density from this frame (0 LOW, 1 MEDIUM, 2 HIGH, 3 FULL) ----
  // The frame is RGB565, which is exactly what lib/density expects.
  DensityResult result;
  if (density_from_rgb565(fb->buf, (int)fb->width, (int)fb->height, &result))
  {
    density = result.level;
    Serial.printf("meanGrad = %u -> density level %u\n",
                  (unsigned)result.meanGrad, (unsigned)result.level);
  }
  else
  {
    Serial.println("Density estimation failed");
  }

  // RGB565 frame -> JPEG (quality 0-100), buffer allocated by the converter
  uint8_t *jpg = NULL;
  size_t jpgLen = 0;
  bool converted = frame2jpg(fb, JPEG_QUALITY, &jpg, &jpgLen);
  esp_camera_fb_return(fb);
  esp_camera_deinit();

  if (!converted)
  {
    Serial.println("JPEG conversion failed");
    return false;
  }
  Serial.printf("JPEG: %u bytes\n", (unsigned)jpgLen);

  bool ok = false;
  if (wifiConnect())
  {
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.begin(UPLOAD_URL);
    http.addHeader("Content-Type", "image/jpeg");
    int code = http.POST(jpg, jpgLen);
    Serial.printf("Upload HTTP code: %d\n", code);
    ok = (code == 200);
    http.end();
  }
  else
  {
    Serial.println("Wi-Fi connect failed");
  }

  free(jpg);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  return ok;
}


// ============================================================
// SETUP  (runs after every wake, because deep sleep resets the chip)
// ============================================================

void setup()
{
  Serial.begin(115200);
  delay(100);

  pinMode(SHARED_WAKE_GPIO, INPUT);

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

  Serial.println();
  Serial.println("==============================");
  Serial.println("ESP32-CAM START");
  Serial.println("==============================");

  // Power-on / reset: don't work, just go to sleep and wait for ATmega
  if (cause != ESP_SLEEP_WAKEUP_EXT0)
  {
    Serial.println("Initial power-on/reset -> deep sleep.");
    goToDeepSleep();
  }

  Serial.println("Wake signal received from ATmega!");
  startWakeWatchdog();
  waitForLineHigh(1000);

  // ---- work: photo -> laptop ----
  takeAndSendPhoto();

  // density was computed from the photo inside takeAndSendPhoto()
  Serial.print("Density prepared = ");
  Serial.println(density);

  // ---- I2C slave must be ready BEFORE the ATmega is woken ----
  if (!Wire.begin(I2C_SLAVE_ADDRESS, I2C_SDA, I2C_SCL, 100000))
  {
    Serial.println("ERROR: I2C failed to start!");
    delay(1000);
    goToDeepSleep();
  }
  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);

  // The original ESP32 runs onRequest() too late: the master's read would get
  // stale fill bytes (0xFE/0xFF). Preload the transmit buffer with the answer
  // so it is already there when the ATmega reads.
  uint8_t answer = density;
  Wire.slaveWrite(&answer, 1);

  wakeATmega();

  waitingSince = millis();
  Serial.println("Waiting for ATmega I2C request...");
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
  if (getDataCommandReceived)
  {
    Serial.println("GET_DATA command received from ATmega.");
    getDataCommandReceived = false;
  }

  // Number has been read by ATmega -> sleep
  if (dataWasSent)
  {
    dataWasSent = false;

    Serial.print("Density sent to ATmega: ");
    Serial.println(density);

    delay(100);            // let the I2C transfer finish
    goToDeepSleep();
  }

  // Safety: never stay awake forever (we'd miss the next wake pulse)
  if (millis() - waitingSince > ATMEGA_REQUEST_TIMEOUT_MS)
  {
    Serial.println("Timeout waiting for ATmega request.");
    goToDeepSleep();
  }

  delay(10);
}
