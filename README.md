# Running a machine learning model on an edge device (arduino) with TensorFlow Lite for microcontrollers

## Here you will find the jupyter notebook with the full description and code
**[View the full jupyter notebook](https://github.com/Dsite42/light_intesity_anomaly_detection_with_tflite-micro/blob/master/Light_intesity_anomaly_detection.ipynb)**



## General Target
With this project I want to figure out how it is possible to run a machine learning model on an microcontroller to get an general overview of the required steps and possibilities.

## Application Use case
Lets imagine the following use case. You are an automotive supplier and producing inerieur parts with light guides. Within the production you want to check if the brightness along the part is in the expected intesity or if you have maybe darker/ lighter or even black points due to problems during the injection molting like bubbles, cracks, compression or blowholes. Lets move the part across a brightness sensor and detect anomalies in the brightness curve.

Simulated situation:
Lets use a brightness sensor where we will take a paper in our hand 5cm above the sensor then slowly go down and up again to simulate a brightness curve which means the brightness of the part is higher at both ends and darker in the middle.

## Used Hardware and Libraries
Arduino Nano 33 Sense BLE rev.2 which has a brightness sensor on board.

Tensorflor Lite for microcontrollers libary for running the ML model on the arduino. (https://github.com/tensorflow/tflite-micro)

Install the following libaries from the Libary Manager in the Arduino IDE:
-  Arduino_APDS9960

![image](https://github.com/user-attachments/assets/77f98071-af6a-47cd-89f2-bf4d96373881)
