EESchema Schematic File Version 4
LIBS:FLipMouse_sensor-cache
EELAYER 26 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title "FLipMouse3 - Sensor PCB FSR"
Date "2018-11-08"
Rev "v3.1"
Comp "AsTeRICS Foundation"
Comment1 ""
Comment2 ""
Comment3 "beni@asterics-foundation.org"
Comment4 "(c) Benjamin Aigner, 2018"
$EndDescr
Text GLabel 6050 3100 0    60   Input ~ 0
3V3
Text Label 6050 3300 2    60   ~ 0
1
Text Label 6550 3300 0    60   ~ 0
2
Text Label 6050 3400 2    60   ~ 0
3
Text Label 6550 3400 0    60   ~ 0
4
Text GLabel 4000 2600 2    60   Input ~ 0
3V3
Text GLabel 4000 3000 2    60   Input ~ 0
3V3
Text GLabel 4000 3400 2    60   Input ~ 0
3V3
Text GLabel 4000 3800 2    60   Input ~ 0
3V3
Text Label 4000 2500 0    60   ~ 0
1
Text Label 4000 2900 0    60   ~ 0
2
Text Label 4000 3300 0    60   ~ 0
3
Text Label 4000 3700 0    60   ~ 0
4
$Comp
L Connector_Generic:Conn_02x05_Odd_Even J1
U 1 1 5BBE044D
P 6250 3300
F 0 "J1" H 6300 3717 50  0000 C CNN
F 1 "Conn_02x05_Odd_Even" H 6300 3626 50  0000 C CNN
F 2 "Connector_PinSocket_1.27mm:PinSocket_2x05_P1.27mm_Vertical_SMD" H 6250 3300 50  0001 C CNN
F 3 "~" H 6250 3300 50  0001 C CNN
	1    6250 3300
	1    0    0    -1  
$EndComp
$Comp
L power:GNDD #PWR0101
U 1 1 5BE452E0
P 6650 3100
F 0 "#PWR0101" H 6650 2850 50  0001 C CNN
F 1 "GNDD" V 6654 2990 50  0000 R CNN
F 2 "" H 6650 3100 50  0001 C CNN
F 3 "" H 6650 3100 50  0001 C CNN
	1    6650 3100
	0    -1   -1   0   
$EndComp
Wire Wire Line
	6650 3100 6550 3100
$Comp
L Connector:Conn_01x01_Female J2
U 1 1 5BE45829
P 3800 2500
F 0 "J2" H 3694 2275 50  0000 C CNN
F 1 "FSR1" H 3900 2550 50  0000 C CNN
F 2 "fsrpad:FSRPAD" H 3800 2500 50  0001 C CNN
F 3 "~" H 3800 2500 50  0001 C CNN
	1    3800 2500
	-1   0    0    1   
$EndComp
$Comp
L Connector:Conn_01x01_Female J3
U 1 1 5BE458E0
P 3800 2600
F 0 "J3" H 3694 2375 50  0000 C CNN
F 1 "FSR1" H 3900 2450 50  0000 C CNN
F 2 "fsrpad:FSRPAD" H 3800 2600 50  0001 C CNN
F 3 "~" H 3800 2600 50  0001 C CNN
	1    3800 2600
	-1   0    0    1   
$EndComp
$Comp
L Connector:Conn_01x01_Female J4
U 1 1 5BE4590E
P 3800 2900
F 0 "J4" H 3694 2675 50  0000 C CNN
F 1 "FSR2" H 3900 2850 50  0000 C CNN
F 2 "fsrpad:FSRPAD" H 3800 2900 50  0001 C CNN
F 3 "~" H 3800 2900 50  0001 C CNN
	1    3800 2900
	-1   0    0    1   
$EndComp
$Comp
L Connector:Conn_01x01_Female J5
U 1 1 5BE45932
P 3800 3000
F 0 "J5" H 3694 2775 50  0000 C CNN
F 1 "FSR2" H 3900 2950 50  0000 C CNN
F 2 "fsrpad:FSRPAD" H 3800 3000 50  0001 C CNN
F 3 "~" H 3800 3000 50  0001 C CNN
	1    3800 3000
	-1   0    0    1   
$EndComp
$Comp
L Connector:Conn_01x01_Female J6
U 1 1 5BE45957
P 3800 3300
F 0 "J6" H 3694 3075 50  0000 C CNN
F 1 "FSR3" H 3900 3250 50  0000 C CNN
F 2 "fsrpad:FSRPAD" H 3800 3300 50  0001 C CNN
F 3 "~" H 3800 3300 50  0001 C CNN
	1    3800 3300
	-1   0    0    1   
$EndComp
$Comp
L Connector:Conn_01x01_Female J7
U 1 1 5BE45981
P 3800 3400
F 0 "J7" H 3694 3175 50  0000 C CNN
F 1 "FSR3" H 3900 3350 50  0000 C CNN
F 2 "fsrpad:FSRPAD" H 3800 3400 50  0001 C CNN
F 3 "~" H 3800 3400 50  0001 C CNN
	1    3800 3400
	-1   0    0    1   
$EndComp
$Comp
L Connector:Conn_01x01_Female J8
U 1 1 5BE459C9
P 3800 3700
F 0 "J8" H 3694 3475 50  0000 C CNN
F 1 "FSR4" H 3900 3650 50  0000 C CNN
F 2 "fsrpad:FSRPAD" H 3800 3700 50  0001 C CNN
F 3 "~" H 3800 3700 50  0001 C CNN
	1    3800 3700
	-1   0    0    1   
$EndComp
$Comp
L Connector:Conn_01x01_Female J9
U 1 1 5BE459F1
P 3800 3800
F 0 "J9" H 3694 3575 50  0000 C CNN
F 1 "FSR4" H 3900 3750 50  0000 C CNN
F 2 "fsrpad:FSRPAD" H 3800 3800 50  0001 C CNN
F 3 "~" H 3800 3800 50  0001 C CNN
	1    3800 3800
	-1   0    0    1   
$EndComp
$EndSCHEMATC
