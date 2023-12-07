# CameraWebServer
ESP32_CAM 웹 서버 예제를 이용해서 외부에서도 Streaming 서비스를 제공하는 ESP32-CAM 보드 코드입니다.  


사용자가 외부에서도 접근하기 위해서는 고정 IP를 할당받고 포트포워딩을 진행해야겠습니다.


ESP32-CAM 보드 코드내에서 MobileNetV1 모델을 이용해 Cat-Dog classfication을 진행합니다.


## 본 프로젝트에서는 사용되지 않았습니다!

## 참고사항

### 추가해야 할 라이브러리
```
AWS_IOT
Arduino_JSON
```

### AWS_IOT 라이브러리에 추가해야할 내용
```
AWS_IOT에 disconnect 함수 선언

1. AWS_IOT.h 파일에 disconnect 함수 선언

/**
 * @brief Disconnects the ESP32 from AWS IoT using MQTT.
 * 
 * This function gracefully disconnects the ESP32 from the AWS IoT broker
 * by using aws_iot_mqtt_disconnect function in "aws_iot_mqtt_client_interface.h". 
 * It should be called when the device needs to terminate its connection with AWS IoT.
 * 
 */
void disconnect();

2. AWS_IOT.cpp 파일에서 disconnect 함수 정의

void disconnect()
{
    aws_iot_mqtt_disconnect(&client);
}

```

### 새롭게 Define 해야하는 변수
```
// Wifi
const char* ssid = "Your WiFi ID";
const char* password = "Your WiFi Password";

// AWS IoT
char HOST_ADDRESS[] = "Your AWS IoT End point";
```



