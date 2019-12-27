#!/bin/sh

scp main.cpp pi@192.168.1.150:source/monitor_433

ssh pi@192.168.1.150 "gcc -l stdc++ -l wiringPi ~/source/monitor_433/main.cpp -o ~/a.out"