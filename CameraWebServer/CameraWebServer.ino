//
// WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
//            Ensure ESP32 Wrover Module or other board with PSRAM is selected
//            Partial images will be transmitted if image exceeds buffer size
//
//            You must select partition scheme from the board menu that has at least 3MB APP space.
//            Face Recognition is DISABLED for ESP32 and ESP32-S2, because it takes up from 15 
//            seconds to process single frame. Face Detection is ENABLED if PSRAM is enabled as well

/*
  Modified from https://github.com/edgeimpulse/example-esp32-cam.
  Modified from https://github.com/alankrantas/edge-impulse-esp32-cam-image-classification.

  Note: 
  The ST7735 TFT size has to be at least 120x120.
  Do not use Arduino IDE 2.0 or you won't be able to see the serial output!
*/

#define CAMERA_MODEL_AI_THINKER // Has PSRAM

#include <AWS_IOT.h>
#include <Arduino_JSON.h>
#include "camera_pins.h"
#include "esp_camera.h"
#include <WiFi.h>

/* ===========================
* // WiFi
* const char* ssid = "";
* const char* password = "";
*
* // IoT
* char HOST_ADDRESS[] = ""; 
   =========================== */


const char* ssid = "";
const char* password = "";

// AWS_IOT
char CLIENT_ID[] = "ESP32_CAM";
char HOST_ADDRESS[] = "";
char pTOPIC_NAME[] = "$aws/things/webserver_thing/shadow/update";  // publish topic name
char payload[512];
int statue_b = 1;

#include <esp32-cam-cat-dog_inferencing.h>  // replace with your deployed Edge Impulse library
#include "img_converters.h"
#include "image_util.h"

dl_matrix3du_t *resized_matrix = NULL;
ei_impulse_result_t result = {0};

void startCameraServer();
void setupLedFlash(int pin);

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(config.pixel_format == PIXFORMAT_JPEG){
    if(psramFound()){
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if(config.pixel_format == PIXFORMAT_JPEG){
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");

  AWS_IOT esp32_cam;

  // AWS에 연결을 해서 SubNet상에서 ESP32-CAM 웹서버의 할당받은 IP 주소를 알려줍니다.
  // 가능하다면 고정 IP를 할당받도록 설정해야겠습니다.
  if (esp32_cam.connect(HOST_ADDRESS, CLIENT_ID) == 0) {
    Serial.println("Connected to AWS");
    delay(1000);
  } else {
    Serial.println("AWS connection failed, Check the HOST Address");
    while (1)
      ;
  }

  // Get the local IP address
  // And Publish to AWS
  IPAddress localIP = WiFi.localIP();
  sprintf(payload, "{\"state\": { \"reported\": {\"espcam_ip\": \"%d.%d.%d.%d\", \"port\": 80}}}", localIP[0], localIP[1], localIP[2], localIP[3]);
  while(esp32_cam.publish(pTOPIC_NAME, payload) != 0){
    Serial.println("Feeding Done Publish failed");
  }
  Serial.print("Feeding Done Publish Message:");
  Serial.println(payload);

  // ESP32-CAM 웹서버에서 스트리밍과 AWS_IOT 연결이 동시에 이루어지지 않는다.
  // 이에 AWS_IOT 라이브러리를 수정해서 AWS_IOT와 연결을 끊도록 했다.
  disconnect();

  // ESP32-CAM 웹 서버 예제 실행
  startCameraServer();
}

void loop() {
  // Do nothing. Everything is done in another task by the web server
  delay(3000);

  // capture a image and classify it
  String result = classify();

  // display result
  Serial.printf("Result: %s\n", result);
}

// classify labels
String classify() {

  // run image capture once to force clear buffer
  // otherwise the captured image below would only show up next time you pressed the button!
  capture_quick();

  // capture image from camera
  if (!capture()) return "Error";

  Serial.println("Getting image...");
  signal_t signal;
  signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_WIDTH;
  signal.get_data = &raw_feature_get_data;

  Serial.println("Run classifier...");
  // Feed signal to the classifier
  EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false /* debug */);
  // --- Free memory ---
  dl_matrix3du_free(resized_matrix);

  if (res != 0) return "Error";

  int index;
  float score = 0.0;
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    // record the most possible label
    if (result.classification[ix].value > score) {
      score = result.classification[ix].value;
      index = ix;
    }
    Serial.printf("    %s: \t%f\r\n", result.classification[ix].label, result.classification[ix].value);
  }

#if EI_CLASSIFIER_HAS_ANOMALY == 1
  Serial.print("    anomaly score: %f\r\n", result.anomaly);
#endif
  
  // --- return the most possible label ---
  return String(result.classification[index].label);
}

// quick capture (to clear buffer)
void capture_quick() {
  camera_fb_t *fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) return;
  esp_camera_fb_return(fb);
}

// capture image from cam
bool capture() {

  Serial.println("Capture image...");
  esp_err_t res = ESP_OK;
  camera_fb_t *fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return false;
  }

  // --- Convert frame to RGB888  ---
  Serial.println("Converting to RGB888...");
  // Allocate rgb888_matrix buffer
  dl_matrix3du_t *rgb888_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);
  fmt2rgb888(fb->buf, fb->len, fb->format, rgb888_matrix->item);

  // --- Resize the RGB888 frame to 96x96 in this example ---
  Serial.println("Resizing the frame buffer...");
  resized_matrix = dl_matrix3du_alloc(1, EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT, 3);
  image_resize_linear(resized_matrix->item, rgb888_matrix->item, EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT, 3, fb->width, fb->height);

  // --- Convert frame to RGB565 ---
  Serial.println("Converting to RGB565");
  uint8_t *rgb565 = (uint8_t *) malloc(240 * 240 * 3);
  jpg2rgb565(fb->buf, fb->len, rgb565, JPG_SCALE_2X); // scale to half size

  // --- Free memory ---
  free(rgb565);
  rgb565 = NULL;
  dl_matrix3du_free(rgb888_matrix);
  esp_camera_fb_return(fb);

  return true;
}

int raw_feature_get_data(size_t offset, size_t out_len, float *signal_ptr) {

  size_t pixel_ix = offset * 3;
  size_t bytes_left = out_len;
  size_t out_ptr_ix = 0;

  // read byte for byte
  while (bytes_left != 0) {
    // grab the values and convert to r/g/b
    uint8_t r, g, b;
    r = resized_matrix->item[pixel_ix];
    g = resized_matrix->item[pixel_ix + 1];
    b = resized_matrix->item[pixel_ix + 2];

    // then convert to out_ptr format
    float pixel_f = (r << 16) + (g << 8) + b;
    signal_ptr[out_ptr_ix] = pixel_f;

    // and go to the next pixel
    out_ptr_ix++;
    pixel_ix += 3;
    bytes_left--;
  }

  return 0;
}
