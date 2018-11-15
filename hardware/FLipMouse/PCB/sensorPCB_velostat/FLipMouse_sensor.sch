EESchema Schematic File Version 4
EELAYER 26 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title "FLipMouse3 - Sensor PCB Velostat"
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
$Comp
L Connector_Generic:Conn_01x02 J2
U 1 1 5A0E874D
P 3800 2600
F 0 "J2" H 3800 2700 50  0000 C CNN
F 1 "Conn_01x02" H 3800 2400 50  0000 C CNN
F 2 "respad:RESPAD" H 3800 2600 50  0001 C CNN
F 3 "" H 3800 2600 50  0001 C CNN
	1    3800 2600
	-1   0    0    1   
$EndComp
$Comp
L Connector_Generic:Conn_01x02 J3
U 1 1 5A0E87A6
P 3800 3000
F 0 "J3" H 3800 3100 50  0000 C CNN
F 1 "Conn_01x02" H 3800 2800 50  0000 C CNN
F 2 "respad:RESPAD" H 3800 3000 50  0001 C CNN
F 3 "" H 3800 3000 50  0001 C CNN
	1    3800 3000
	-1   0    0    1   
$EndComp
$Comp
L Connector_Generic:Conn_01x02 J4
U 1 1 5A0E87D3
P 3800 3400
F 0 "J4" H 3800 3500 50  0000 C CNN
F 1 "Conn_01x02" H 3800 3200 50  0000 C CNN
F 2 "respad:RESPAD" H 3800 3400 50  0001 C CNN
F 3 "" H 3800 3400 50  0001 C CNN
	1    3800 3400
	-1   0    0    1   
$EndComp
$Comp
L Connector_Generic:Conn_01x02 J5
U 1 1 5A0E87FF
P 3800 3800
F 0 "J5" H 3800 3900 50  0000 C CNN
F 1 "Conn_01x02" H 3800 3600 50  0000 C CNN
F 2 "respad:RESPAD" H 3800 3800 50  0001 C CNN
F 3 "" H 3800 3800 50  0001 C CNN
	1    3800 3800
	-1   0    0    1   
$EndComp
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
$EndSCHEMATC
