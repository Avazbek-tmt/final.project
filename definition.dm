Section,Function,Variable (in Code),ESP32 GPIO Pin
I2C/OLED Display,SDA,OLED_SDA_PIN,21
,SCL,OLED_SCL_PIN,22
Ultrasonic Sensor (HC-SR04),TRIG,TRIG_PIN,15
,ECHO,ECHO_PIN,26
Motor Driver (L298N),ENA (Left Speed/PWM),motorA_en,32
,IN1 (Motor A Direction),motorA_in1,33
,IN2 (Motor A Direction),motorA_in2,25
,ENB (Right Speed/PWM),motorB_en,23
,IN3 (Motor B Direction),motorB_in3,19
,IN4 (Motor B Direction),motorB_in4,18
RGB LED,Red Component,RGB_R_PIN,14
,Green Component,RGB_G_PIN,27
,Blue Component,`RGB_B_PIN**,12
Buzzer/Piezo,Signal,BUZZER_PIN,4


1) ESP32 (Main Controller)

The brain of the robot.
It reads sensors, processes data, and controls the motors.

2) Ultrasonic Sensor

Measures distance using sound waves.
Helps the robot detect obstacles.

3) Motor Driver

Takes control signals from the ESP32 and delivers enough power to the motors.
Controls speed and rotation direction.

4) DC Gear Motors

Provide mechanical power to move the tracks.
Different rotation of left/right motors allows turning.

5) Tracks (Treads)

Allow the robot to move on different surfaces with strong grip.

Power Components

6) 18650 Batteries

Main power source for the robot.
Provide high current for motors and electronics.

7) Step-Down Converter (Buck Converter)

Red board with capacitors and an inductor.
It reduces the battery voltage to a stable voltage for the ESP32 and other electronics.

8) ON/OFF Switch

Turns the entire robot’s power on or off.
Controls the battery connection to the system.


🔌 Other

9) Wires & Breadboard

Used to connect power lines, sensors, motors, and the ESP32.
