# Smart Pet Bowl

Smart Pet Bowl is an IoT system in which the pet bowl can automatically refill itself with food at certain scheduled times in the day. The bowl is mounted on a load sensor which can measure its weight. Above the bowl there is a pet food container that can be opened up by a servo motor arm. The user schedules the refill times via an Android app. When the scheduled refill time comes, the bowl weight is measured. If the bowl is not full, the servo motor opens the food container and lets the bowl get filled. All the measured weight data is uploaded onto an online database the app is connected to, in order to provide information to the user about whether, when and how much the pet ate.

Project delivered by Giulia Sellitto for the Lab of IoT exam - June 2020 - Università degli Studi di Salerno, Italy

For further information, please read the doc.

## How to replicate the project yourself

1. setup hardware as shown in the schema
2.	Setup a Firebase project and choose Realtime Database.
3.	Install the following libraries to your Arduino IDE:
    -	WiFiNINA 
    -	WiFiUdp
    -	Firebase_Arduino_WiFiNINA 
    -	TimeLib 
    -	HX711 
    -	Servo 
4.	Configure source code files sketch.ino and arduino_secrets.h as described in the files themselves.
5.	Open Firebase console and connect a web app to your project. You will obtain a configuration object that must be copied into chart.html.
6.	Import android_app.aia file to MIT App Inventor. Go to Screen3 and change Firebase configuration in the Design view.
7.	Import chart.html in your MIT App Inventor project.
