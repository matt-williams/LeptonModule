#!/bin/bash
g++ -ggdb -I /usr/include/opencv main.cpp Lepton_I2C.cpp SPI.cpp -lopencv_core -lopencv_imgproc -lopencv_highgui -LleptonSDKEmb32PUB/Debug -lLEPTON_SDK
