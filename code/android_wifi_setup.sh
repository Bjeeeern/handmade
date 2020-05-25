#!/bin/bash

adb=~/Library/Android/sdk/platform-tools/adb

$adb tcpip 5555
$adb connect 192.168.11.23:5555 

