#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h> 
#include <WiFi.h>
#include <WebServer.h>
#include <FluxGarage_RoboEyes.h> 

// =================================================================
// --- I. НАСТРОЙКИ СЕТИ И СЕРВЕРА ---
// =================================================================
const char* ssid = "Wir";          // !!! ЗАМЕНИТЕ НА ИМЯ СЕТИ !!!
const char* password = "12341234";  // !!! ЗАМЕНИТЕ НА ПАРОЛЬ СЕТИ !!!
WebServer server(80);

// =================================================================
// --- II. НАСТРОЙКИ ОБОРУДОВАНИЯ И ПИНЫ ---
// =================================================================

// --- OLED ДИСПЛЕЙ (I2C) ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C 
#define OLED_RESET -1  
const int OLED_SDA_PIN = 21; 
const int OLED_SCL_PIN = 22; 

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RoboEyes<Adafruit_SSD1306> roboEyes(display); 

// --- ПИНЫ УПРАВЛЕНИЯ МОТОРАМИ (L298N) ---
// ПИНЫ НЕ МЕНЯЛИСЬ
const int motorA_en = 32;  // ENA (ЛЕВЫЙ)
const int motorA_in1 = 33; 
const int motorA_in2 = 25; 
const int motorB_en = 23;  // ENB (ПРАВЫЙ)
const int motorB_in3 = 19; 
const int motorB_in4 = 18; 

// --- СКОРОСТИ И КАЛИБРОВКА ---
int currentBaseSpeed = 200;    
const int FOLLOW_SPEED = 120;  
const int BOOST_VALUE = 255;   
const int SLOW_TURN_SPEED = 120; // УВЕЛИЧЕНО до 120, чтобы гарантировать движение
const int TURN_TIME_90 = 350;    // Тайминг для поворота на 90 градусов (мc)

// --- ДАТЧИК ДИСТАНЦИИ (HC-SR04) ---
const int TRIG_PIN = 15; 
const int ECHO_PIN = 26; 

// --- КОНСТАНТЫ СЛЕДОВАНИЯ И БЕЗОПАСНОСТИ ---
const int DANGER_DISTANCE_CM = 5;      
const int REVERSE_START_DISTANCE = 13; 
const int DISTANCE_THRESHOLD = 25;     

// --- ПИНЫ BUZZER ---
#define BUZZER_PIN 4      

// --- ПИНЫ RGB LED ---
const int RGB_R_PIN = 14; 
const int RGB_G_PIN = 27; 
const int RGB_B_PIN = 12; 

// --- СОСТОЯНИЕ РОБОТА И УПРАВЛЕНИЕ LED ---
enum RobotMode {
    WEB_CONTROL,
    FOLLOW_HAND
};
RobotMode currentMode = WEB_CONTROL;
bool isLedEnabled = true;

// ПРОТОТИПЫ ФУНКЦИЙ (ОБЯЗАТЕЛЬНО!)
String handleRootHTML();
void stopMotors();
void updateEyeMood(long distance);
void followHandLogic(long distance);

// =================================================================
// --- III. ФУНКЦИИ УПРАВЛЕНИЯ ДВИЖЕНИЕМ И ДАТЧИКАМИ ---
// =================================================================

void setMotorSpeed(int pin, int speed) { analogWrite(pin, speed); }

void stopMotors() {
  setMotorSpeed(motorA_en, 0); setMotorSpeed(motorB_en, 0);
  digitalWrite(motorA_in1, LOW); digitalWrite(motorA_in2, LOW);
  digitalWrite(motorB_in3, LOW); digitalWrite(motorB_in4, LOW);
}

void moveForward(int speed) {
  // ИНВЕРСИЯ: Motor A (Левый) <-> Motor B (Правый)
  setMotorSpeed(motorA_en, speed); setMotorSpeed(motorB_en, speed);
  digitalWrite(motorA_in1, LOW); digitalWrite(motorA_in2, HIGH); // ИЗМЕНЕНО: Motor A
  digitalWrite(motorB_in3, HIGH); digitalWrite(motorB_in4, LOW);  // Motor B
}

void moveBackward(int speed) {
  // ИНВЕРСИЯ: Motor A (Левый) <-> Motor B (Правый)
  setMotorSpeed(motorA_en, speed); setMotorSpeed(motorB_en, speed);
  digitalWrite(motorA_in1, HIGH); digitalWrite(motorA_in2, LOW); // ИЗМЕНЕНО: Motor A
  digitalWrite(motorB_in3, LOW); digitalWrite(motorB_in4, HIGH);  // Motor B
}

void rotateRight(int speed) { 
  // Motor A - Вперед, Motor B - Назад (Поворот на месте)
  setMotorSpeed(motorA_en, speed); setMotorSpeed(motorB_en, speed); 
  digitalWrite(motorA_in1, LOW); digitalWrite(motorA_in2, HIGH); // ИЗМЕНЕНО: Motor A
  digitalWrite(motorB_in3, LOW); digitalWrite(motorB_in4, HIGH); // Motor B
}

void rotateLeft(int speed) { 
  // Motor A - Назад, Motor B - Вперед (Поворот на месте)
  setMotorSpeed(motorA_en, speed); setMotorSpeed(motorB_en, speed); 
  digitalWrite(motorA_in1, HIGH); digitalWrite(motorA_in2, LOW); // ИЗМЕНЕНО: Motor A
  digitalWrite(motorB_in3, HIGH); digitalWrite(motorB_in4, LOW); // Motor B
}

// Повороты с остановкой
void turn90DegreesRight() { stopMotors(); delay(100); rotateRight(currentBaseSpeed); delay(TURN_TIME_90); stopMotors(); }
void turn90DegreesLeft() { stopMotors(); delay(100); rotateLeft(currentBaseSpeed); delay(TURN_TIME_90); stopMotors(); }
void turn180Degrees() { stopMotors(); delay(100); rotateRight(currentBaseSpeed); delay(TURN_TIME_90 * 2); stopMotors(); }

long readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 60000); 
  
  if (duration == 0) { return 999; }
  
  long distance = duration * 0.034 / 2;
  
  if (distance > 400) { return 999; }
  
  return distance; 
}

// =================================================================
// --- IV. ФУНКЦИЯ УПРАВЛЕНИЯ RGB (ОСТАВЛЕНО БЕЗ ИЗМЕНЕНИЙ) ---
// =================================================================

void setRGBColor(int r, int g, int b) {
    if (!isLedEnabled) { 
        r = 0; g = 0; b = 0;
    }
    analogWrite(RGB_R_PIN, r);
    analogWrite(RGB_G_PIN, g);
    analogWrite(RGB_B_PIN, b);
}

// =================================================================
// --- V. ЛОГИКА РЕЖИМА СЛЕДОВАНИЯ (ОСТАВЛЕНО БЕЗ ИЗМЕНЕНИЙ) ---
// =================================================================

void followHandLogic(long distance) {
    if (distance > DISTANCE_THRESHOLD || distance == 999) {
        stopMotors();
        noTone(BUZZER_PIN); 
        roboEyes.setPosition(4);      
        setRGBColor(0, 0, 100);       
        return;
    } 
    
    else if (distance > REVERSE_START_DISTANCE && distance <= DISTANCE_THRESHOLD) {
        int speed = FOLLOW_SPEED; 
        moveForward(speed); 
        noTone(BUZZER_PIN); 
        roboEyes.setMood(2);      // 2 = HAPPY
        roboEyes.setPosition(1);  // 1 = UP
        setRGBColor(0, 255, 0);   // Зеленый: Вперед
    } 
    
    else if (distance > DANGER_DISTANCE_CM && distance <= REVERSE_START_DISTANCE) {
        int speed = FOLLOW_SPEED; 
        moveBackward(speed); 
        
        if (millis() % 400 < 200) { 
            tone(BUZZER_PIN, 500); 
            setRGBColor(255, 0, 0); // Красный
        } else {
            noTone(BUZZER_PIN); 
            setRGBColor(0, 0, 0); // Выключен
        }
        
        roboEyes.setMood(1);      // 1 = SCARED (Предупреждение)
        roboEyes.setPosition(7);  // 7 = DOWN
    } 
}

// =================================================================
// --- VI. ГРАФИКА: ФУНКЦИИ ГЛАЗОК (И RGB) (ОСТАВЛЕНО БЕЗ ИЗМЕНЕНИЙ) ---
// =================================================================

void updateEyeMood(long distance) {
    if (distance <= DANGER_DISTANCE_CM) { 
        roboEyes.setMood(1); // 1 = SCARED 
        setRGBColor(255, 0, 0); 
        return;
    }

    if (currentMode == FOLLOW_HAND) {
        return;
    }
    
    // WEB_CONTROL
    roboEyes.setMood(0); // 0 = DEFAULT
    setRGBColor(0, 0, 100); 
}

// =================================================================
// --- VII. ОБРАБОТЧИКИ WEB SERVER (УПРАВЛЕНИЕ: ЕДИНЫЙ AJAX) ---
// =================================================================

// ЕДИНЫЙ ОБРАБОТЧИК ДЛЯ РЕЖИМА "УДЕРЖИВАЙ И ЕДЬ"
void handleMove() {
    if (currentMode != WEB_CONTROL) { 
        server.send(200, "text/plain", "Mode: Follow");
        return;
    }

    String direction = server.arg("dir"); 
    
    // ЛОГИКА ДВИЖЕНИЯ ПО УДЕРЖИВАНИЮ (currentBaseSpeed или SLOW_TURN_SPEED)
    if (direction == "fwd") {
        moveForward(currentBaseSpeed);
    } else if (direction == "rev") {
        moveBackward(currentBaseSpeed);
    } else if (direction == "rotL") {
        rotateLeft(currentBaseSpeed);
    } else if (direction == "rotR") {
        rotateRight(currentBaseSpeed);
    // ИСПРАВЛЕНО: Плавные повороты используют SLOW_TURN_SPEED (120)
    } else if (direction == "slowL") {
        rotateLeft(SLOW_TURN_SPEED);
    } else if (direction == "slowR") {
        rotateRight(SLOW_TURN_SPEED);
    } else {
        // Если команда не распознана (или это команда stop из JS)
        stopMotors(); 
    }

    server.send(200, "text/plain", "Moving: " + direction);
}

// Кнопки с фиксированным действием (поворот, буст, смена режима)

void handleStop() { stopMotors(); noTone(BUZZER_PIN); server.send(200, "text/html", "<h1>Stopped</h1>" + handleRootHTML()); }
void handleTurnRight90() { if (currentMode == WEB_CONTROL) turn90DegreesRight(); server.send(200, "text/html", "<h1>Turning 90 deg Right</h1>" + handleRootHTML()); }
void handleTurnLeft90() { if (currentMode == WEB_CONTROL) turn90DegreesLeft(); server.send(200, "text/html", "<h1>Turning 90 deg Left</h1>" + handleRootHTML()); }
void handleTurn180() { if (currentMode == WEB_CONTROL) turn180Degrees(); server.send(200, "text/html", "<h1>Turning 180 degrees</h1>" + handleRootHTML()); }
void handleFollow() { stopMotors(); noTone(BUZZER_PIN); currentMode = FOLLOW_HAND; server.send(200, "text/html", "<h1>Mode: FOLLOW HAND (Autonomous)</h1>" + handleRootHTML()); }
void handleWebControl() { currentMode = WEB_CONTROL; server.send(200, "text/html", "<h1>Mode: WEB CONTROL (Manual)</h1>" + handleRootHTML()); }
void handleRoot() { String html = handleRootHTML(); server.send(200, "text/html", html); }
void handleNotFound() { server.send(404, "text/plain", "404: Not Found"); }

// --- SPEED и BOOST ---
void handleSetSpeed(int speedValue) { 
    currentBaseSpeed = speedValue; 
    stopMotors(); // Остановка после смены скорости
    server.send(200, "text/html", "<h1>Speed set to: " + String(speedValue) + "</h1>" + handleRootHTML()); 
}

void handleSpeedNormal() { handleSetSpeed(200); } 
void handleSpeedMedium() { handleSetSpeed(220); } 
void handleSpeedFast() { handleSetSpeed(255); } 

void handleBoost() { 
    if (currentMode == WEB_CONTROL) {
        moveForward(BOOST_VALUE); 
        delay(300);
        stopMotors(); 
    }
    server.send(200, "text/html", "<h1>Boost Activated!</h1>" + handleRootHTML()); 
}

// --- LED ---
void handleDisableLed() { 
    isLedEnabled = false; 
    setRGBColor(0, 0, 0); 
    server.send(200, "text/html", "<h1>LED Disabled</h1>" + handleRootHTML()); 
}
void handleEnableLed() { 
    isLedEnabled = true; 
    setRGBColor(0, 0, 100); 
    server.send(200, "text/html", "<h1>LED Enabled</h1>" + handleRootHTML()); 
}


// =================================================================
// --- VIII. HTML INTERFACE (Режим УДЕРЖИВАЙ И ЕДЬ) ---
// =================================================================

String handleRootHTML() {
  // Добавлены скрипты для AJAX-управления "Удерживай и Едь"
  String html = R"raw(<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Robot Control</title>
<style>
      body{font-family: 'Segoe UI', Arial, sans-serif; text-align: center; margin: 0; padding: 20px; background: #222; color: #eee;}
      .status{margin-bottom: 20px; padding: 10px; background: #333; border-radius: 12px; box-shadow: 0 0 10px rgba(0,0,0,0.5);}
      h2{color: #00bcd4;}
      .joystick-grid {
        display: grid;
        grid-template-columns: repeat(3, 100px);
        grid-template-rows: repeat(3, 100px);
        gap: 8px;
        margin: 0 auto;
        width: 324px;
        background: #111;
        padding: 15px;
        border-radius: 20px;
        box-shadow: 0 8px 15px rgba(0,0,0,0.7);
      }
      .ctrl-btn {
        display: flex; align-items: center; justify-content: center; 
        text-decoration: none; color: white; background-color: #444; 
        border: 2px solid #555; border-radius: 10px; font-size: 16px; 
        font-weight: bold; transition: background-color 0.1s, transform 0.1s; 
        height: 100%;
        box-shadow: 0 4px 6px rgba(0,0,0,0.5);
        user-select: none; 
        touch-action: manipulation; 
      }
      .ctrl-btn:hover {background-color: #666;}
      .ctrl-btn:active {transform: translateY(1px); box-shadow: inset 0 0 10px #000;}

      .stop-btn {background-color: #dc3545;}
      .forward-btn {grid-column: 2; grid-row: 1;}
      .backward-btn {grid-column: 2; grid-row: 3;}
      .left-btn {grid-column: 1; grid-row: 2;}
      .right-btn {grid-column: 3; grid-row: 2;}

      .slow-turn {background-color: #00bcd4; font-size: 14px;}
      .boost-btn {background-color: #ff9800;}
      .turn-btn {background-color: #4caf50;}

      .speed-group {
        margin-top: 25px; 
        display: flex; justify-content: center;
        padding: 10px; background: #333; border-radius: 10px;
      }
      .speed-btn {
        width: 90px; height: 40px; margin: 0 5px;
        background-color: #9e9e9e; color: #333;
      }
      .mode-btn {background-color: #03a9f4; margin-top: 20px; padding: 10px 20px; border-radius: 10px; text-decoration: none; display: inline-block;}
      .led-toggle-group {margin-top: 15px; display: flex; justify-content: center;}
</style>
<script>
    // AJAX для управления "Удерживай и Едь"
    function sendCommand(url) {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", url, true);
        xhr.send();
    }

    // НАЧАЛО ДВИЖЕНИЯ: отправляет команду /move?dir=X
    function startMove(direction) {
        sendCommand("/move?dir=" + direction);
    }

    // ОСТАНОВКА: отправляет команду /move (без dir), которая вызывает stopMotors()
    function stopMove() {
        sendCommand("/move"); 
    }
</script>
</head>
<body>
    <div class="status">
      <h2>ROBOT COMMAND CENTER</h2>
      <p>IP: )raw" + WiFi.localIP().toString() + R"raw(</p>
      <p>Current Speed: <b style="color:#ff9800;">)raw" + String(currentBaseSpeed) + R"raw(</b> | Mode: <b style="color:#03a9f4;">)raw";
  
  if (currentMode == WEB_CONTROL) {
      html += "MANUAL (Web Control)</b></p>";
      
      html += R"raw(
      <p style='color:#4caf50;'>УПРАВЛЕНИЕ: **УДЕРЖИВАЙ И ЕДЬ**</p>
      <div class="speed-group">
        <a class="ctrl-btn speed-btn" href='/speed_n'>NORMAL (200)</a>
        <a class="ctrl-btn speed-btn" href='/speed_m'>MEDIUM (220)</a>
        <a class="ctrl-btn speed-btn" href='/speed_f'>FAST (255)</a>
      </div>
      <div class="joystick-grid">
        <a class="ctrl-btn slow-turn" 
            onmousedown="startMove('slowL')" ontouchstart="startMove('slowL')" 
            onmouseup="stopMove()" ontouchend="stopMove()" 
            href="javascript:void(0)">SLOW L</a>
        
        <a class="ctrl-btn forward-btn" 
            onmousedown="startMove('fwd')" ontouchstart="startMove('fwd')"
            onmouseup="stopMove()" ontouchend="stopMove()" 
            href="javascript:void(0)">FWD</a>
        
        <a class="ctrl-btn slow-turn" 
            onmousedown="startMove('slowR')" ontouchstart="startMove('slowR')" 
            onmouseup="stopMove()" ontouchend="stopMove()" 
            href="javascript:void(0)">SLOW R</a>
        
        <a class="ctrl-btn left-btn" 
            onmousedown="startMove('rotL')" ontouchstart="startMove('rotL')"
            onmouseup="stopMove()" ontouchend="stopMove()" 
            href="javascript:void(0)">LEFT</a>
            
        <a class="ctrl-btn stop-btn" href='/stop'>STOP</a> <a class="ctrl-btn right-btn" 
            onmousedown="startMove('rotR')" ontouchstart="startMove('rotR')"
            onmouseup="stopMove()" ontouchend="stopMove()" 
            href="javascript:void(0)">RIGHT</a>
        
        <a class="ctrl-btn boost-btn" href='/boost'>BOOST!</a>
        
        <a class="ctrl-btn backward-btn" 
            onmousedown="startMove('rev')" ontouchstart="startMove('rev')"
            onmouseup="stopMove()" ontouchend="stopMove()" 
            href="javascript:void(0)">REV</a>
            
        <a class="ctrl-btn turn-btn" href='/turn180'>180 ROT</a>
      </div>

      <div class="speed-group">
        <a class="ctrl-btn turn-btn" style="width: 80px; height: 40px; margin-right: 5px;" href='/turn90L'>90 L</a>
        <a class="ctrl-btn turn-btn" style="width: 80px; height: 40px;" href='/turn90R'>90 R</a>
      </div>

      <a class="mode-btn" href='/follow'>Switch to FOLLOW HAND MODE</a>
      
      <div class="led-toggle-group">)raw";
        if (isLedEnabled) {
            html += "<a class=\"ctrl-btn stop-btn\" style=\"width: 250px; height: 40px;\" href='/disable_led'>🔴 Turn LED OFF</a>";
        } else {
            html += "<a class=\"ctrl-btn turn-btn\" style=\"width: 250px; height: 40px;\" href='/enable_led'>🟢 Turn LED ON</a>";
        }
        html += R"raw(</div>)raw";
  } else if (currentMode == FOLLOW_HAND) {
      html += "FOLLOW HAND (Autonomous)</b></p>";
      html += "<p style='color:#4caf50;'>Robot is autonomously following your hand. (Range: 15-25cm)</p>";
      html += "<a class=\"mode-btn\" href='/web_control'>Switch to WEB CONTROL MODE</a>";
      
      html += R"raw(<div class="led-toggle-group">)raw";
        if (isLedEnabled) {
            html += "<a class=\"ctrl-btn stop-btn\" style=\"width: 250px; height: 40px;\" href='/disable_led'>🔴 Turn LED OFF</a>";
        } else {
            html += "<a class=\"ctrl-btn turn-btn\" style=\"width: 250px; height: 40px;\" href='/enable_led'>🟢 Turn LED ON</a>";
        }
        html += R"raw(</div>)raw";
  }

  html += R"raw(</div></body></html>)raw";
  return html;
}

// =================================================================
// --- IX. SETUP (ОСТАВЛЕНО БЕЗ ИЗМЕНЕНИЙ) ---
// =================================================================

void setup() {
  Serial.begin(115200);
  
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(motorA_en, OUTPUT); pinMode(motorB_en, OUTPUT); 
  pinMode(motorA_in1, OUTPUT); pinMode(motorA_in2, OUTPUT);
  pinMode(motorB_in3, OUTPUT); pinMode(motorB_in4, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);
  
  pinMode(RGB_R_PIN, OUTPUT); 
  pinMode(RGB_G_PIN, OUTPUT);
  pinMode(RGB_B_PIN, OUTPUT);

  // Инициализация OLED
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN); 
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("OLED FAILED. Halting.")); for(;;); 
  }
  
  setRGBColor(0, 0, 0); 

  roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 100); 
  roboEyes.setMood(0); 
  roboEyes.setPosition(4); 
  roboEyes.setAutoblinker(1, 3, 2); 
  roboEyes.setIdleMode(1, 2, 2); 
  roboEyes.close(); 
  display.setRotation(2); 

  // НАСТРОЙКА WI-FI
  WiFi.begin(ssid, password);
  
  display.clearDisplay(); display.setTextSize(1); display.setCursor(0, 0);
  display.println("Connecting to WiFi..."); display.display();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  display.clearDisplay(); display.setTextSize(1); display.setCursor(0, 0);
  display.println("Connected!"); display.println(WiFi.localIP()); display.display();

  Serial.println("\nWiFi connected. IP: "); Serial.println(WiFi.localIP());

  // НАСТРОЙКА WEB SERVER (Изменен маршрут /move)
  server.on("/", handleRoot); 
  server.on("/move", handleMove); // ЕДИНЫЙ ОБРАБОТЧИК ДЛЯ УПРАВЛЕНИЯ

  server.on("/stop", handleStop); 
  server.on("/turn90R", handleTurnRight90); server.on("/turn90L", handleTurnLeft90);  server.on("/turn180", handleTurn180);      
  server.on("/follow", handleFollow); server.on("/web_control", handleWebControl);
  server.on("/speed_n", handleSpeedNormal); server.on("/speed_m", handleSpeedMedium); server.on("/speed_f", handleSpeedFast);
  server.on("/boost", handleBoost);
  server.on("/disable_led", handleDisableLed); server.on("/enable_led", handleEnableLed);

  server.onNotFound(handleNotFound);

  server.begin(); 
  stopMotors(); 
  delay(1000);
}

// =================================================================
// --- X. ОСНОВНАЯ ЛОГИКА LOOP (ОСТАВЛЕНО БЕЗ ИЗМЕНЕНИЙ) ---
// =================================================================

void loop() {
  server.handleClient(); 
  
  long currentDistance = readDistanceCM();
  
  // 1. SAFETY OVERRIDE
  if (currentDistance <= DANGER_DISTANCE_CM) {
    stopMotors();
    noTone(BUZZER_PIN);
    updateEyeMood(currentDistance); 
    roboEyes.update();
    return; 
  }
  
  // 2. MODE LOGIC
  
  if (currentMode == FOLLOW_HAND) {
    followHandLogic(currentDistance);
  } else {  
    // WEB_CONTROL: Моторы не останавливаются, если они были запущены через /move (AJAX)
    noTone(BUZZER_PIN);
    roboEyes.setPosition(4); 
    updateEyeMood(currentDistance); 
  }
  
  roboEyes.update(); 
  
  delay(50); 
}
