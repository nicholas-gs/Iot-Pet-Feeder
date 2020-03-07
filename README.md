# IoT Pet Feeder

Based off [this](https://www.hackster.io/circuito-io-team/iot-pet-feeder-10a4f3#toc-assembly-2) project. The pet feeder is controlled through the [Blynk App](https://blynk.io/).

## Components

1. Arduino Nano
2. ESP8266 WIFI Module
3. HC-SR501 PIR Sensor
4. MG995 Servo Motor
5. Thin Speaker

## Blynk App

<img src="https://user-images.githubusercontent.com/39665412/76133786-da0c9b00-6054-11ea-8270-200d723d4f89.jpg" width="40%">

## Functionality

### __Automatic Mode__

1. Enter the feed time through the Blynk App. (Give a buffer period of 1-2 minutes)
2. The pet feeder will dispense food during the feed time and the "Food Dispense" led will light up.
3. Once the PIR sensor has detected the pet, it display the time stamp on the LCD in the Blynk App. The "Pet Eaten" led will also light up.

Remember to press the "Reset" button to have the pet feeder dispense food the next day.

### __Manual Mode__

1. Press the "Feed Pet" button at the bottom of the Blynk App to manually dispense food. (This will NOT interfere with the automatic mode)