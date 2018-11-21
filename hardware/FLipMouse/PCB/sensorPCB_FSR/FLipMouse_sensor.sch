EESchema Schematic File Version 4
LIBS:FLipMouse_sensor-cache
EELAYER 26 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title "FLipMouse3 - Sensor PCB FSR"
Date "2018-11-21"
Rev "v3.1"
Comp "AsTeRICS Foundation"
Comment1 ""
Comment2 "Art. Number: 30310201"
Comment3 "beni@asterics-foundation.org"
Comment4 "(c) Benjamin Aigner, 2018"
$EndDescr
Text GLabel 3750 1600 0    60   Input ~ 0
3V3
Text Label 4000 1800 2    60   ~ 0
1
Text Label 4500 1800 0    60   ~ 0
2
Text Label 4000 1900 2    60   ~ 0
3
Text Label 4500 1900 0    60   ~ 0
4
Text GLabel 2250 1800 2    60   Input ~ 0
3V3
Text GLabel 2250 2200 2    60   Input ~ 0
3V3
Text GLabel 2250 2600 2    60   Input ~ 0
3V3
Text GLabel 2250 3000 2    60   Input ~ 0
3V3
Text Label 2250 1700 0    60   ~ 0
1
Text Label 2250 2100 0    60   ~ 0
2
Text Label 2250 2500 0    60   ~ 0
3
Text Label 2250 2900 0    60   ~ 0
4
$Comp
L Connector_Generic:Conn_02x05_Odd_Even J1
U 1 1 5BBE044D
P 4200 1800
F 0 "J1" H 4250 2217 50  0000 C CNN
F 1 "Conn_02x05_Odd_Even" H 4250 2126 50  0000 C CNN
F 2 "Connector_PinSocket_1.27mm:PinSocket_2x05_P1.27mm_Vertical_SMD" H 4200 1800 50  0001 C CNN
F 3 "~" H 4200 1800 50  0001 C CNN
	1    4200 1800
	1    0    0    -1  
$EndComp
$Comp
L power:GNDD #PWR0101
U 1 1 5BE452E0
P 5000 1600
F 0 "#PWR0101" H 5000 1350 50  0001 C CNN
F 1 "GNDD" V 5004 1490 50  0000 R CNN
F 2 "" H 5000 1600 50  0001 C CNN
F 3 "" H 5000 1600 50  0001 C CNN
	1    5000 1600
	0    -1   -1   0   
$EndComp
$Comp
L Connector:Conn_01x01_Female J2
U 1 1 5BE45829
P 2050 1700
F 0 "J2" H 1944 1475 50  0000 C CNN
F 1 "FSR1" H 2150 1750 50  0000 C CNN
F 2 "fsrpad:FSRPAD" H 2050 1700 50  0001 C CNN
F 3 "~" H 2050 1700 50  0001 C CNN
	1    2050 1700
	-1   0    0    1   
$EndComp
$Comp
L Connector:Conn_01x01_Female J3
U 1 1 5BE458E0
P 2050 1800
F 0 "J3" H 1944 1575 50  0000 C CNN
F 1 "FSR1" H 2150 1650 50  0000 C CNN
F 2 "fsrpad:FSRPAD" H 2050 1800 50  0001 C CNN
F 3 "~" H 2050 1800 50  0001 C CNN
	1    2050 1800
	-1   0    0    1   
$EndComp
$Comp
L Connector:Conn_01x01_Female J4
U 1 1 5BE4590E
P 2050 2100
F 0 "J4" H 1944 1875 50  0000 C CNN
F 1 "FSR2" H 2150 2050 50  0000 C CNN
F 2 "fsrpad:FSRPAD" H 2050 2100 50  0001 C CNN
F 3 "~" H 2050 2100 50  0001 C CNN
	1    2050 2100
	-1   0    0    1   
$EndComp
$Comp
L Connector:Conn_01x01_Female J5
U 1 1 5BE45932
P 2050 2200
F 0 "J5" H 1944 1975 50  0000 C CNN
F 1 "FSR2" H 2150 2150 50  0000 C CNN
F 2 "fsrpad:FSRPAD" H 2050 2200 50  0001 C CNN
F 3 "~" H 2050 2200 50  0001 C CNN
	1    2050 2200
	-1   0    0    1   
$EndComp
$Comp
L Connector:Conn_01x01_Female J6
U 1 1 5BE45957
P 2050 2500
F 0 "J6" H 1944 2275 50  0000 C CNN
F 1 "FSR3" H 2150 2450 50  0000 C CNN
F 2 "fsrpad:FSRPAD" H 2050 2500 50  0001 C CNN
F 3 "~" H 2050 2500 50  0001 C CNN
	1    2050 2500
	-1   0    0    1   
$EndComp
$Comp
L Connector:Conn_01x01_Female J7
U 1 1 5BE45981
P 2050 2600
F 0 "J7" H 1944 2375 50  0000 C CNN
F 1 "FSR3" H 2150 2550 50  0000 C CNN
F 2 "fsrpad:FSRPAD" H 2050 2600 50  0001 C CNN
F 3 "~" H 2050 2600 50  0001 C CNN
	1    2050 2600
	-1   0    0    1   
$EndComp
$Comp
L Connector:Conn_01x01_Female J8
U 1 1 5BE459C9
P 2050 2900
F 0 "J8" H 1944 2675 50  0000 C CNN
F 1 "FSR4" H 2150 2850 50  0000 C CNN
F 2 "fsrpad:FSRPAD" H 2050 2900 50  0001 C CNN
F 3 "~" H 2050 2900 50  0001 C CNN
	1    2050 2900
	-1   0    0    1   
$EndComp
$Comp
L Connector:Conn_01x01_Female J9
U 1 1 5BE459F1
P 2050 3000
F 0 "J9" H 1944 2775 50  0000 C CNN
F 1 "FSR4" H 2150 2950 50  0000 C CNN
F 2 "fsrpad:FSRPAD" H 2050 3000 50  0001 C CNN
F 3 "~" H 2050 3000 50  0001 C CNN
	1    2050 3000
	-1   0    0    1   
$EndComp
Text Label 4000 1700 2    50   ~ 0
VUSB
Text Label 4500 1700 0    50   ~ 0
GND_USB
Text Label 4000 2000 2    50   ~ 0
EXT1
Text Label 4500 2000 0    50   ~ 0
NEOPIXEL
Wire Wire Line
	3750 1600 4000 1600
Wire Wire Line
	4500 1600 5000 1600
Text Notes 3550 2600 0    50   ~ 0
Note:\n-) VUSB is directly from USB port, take care of sleep mode, power consumption and USB specs\n-) GNDD is switched off while in sleep mode, use GND for permanent power supply, but this one \n   needs to have less than 500uA (in total!) in sleep mode!\n-) NEOPIXEL is the output signal of the onboard neopixel\n-) EXT1 is an analog/digital I/O pin
Wire Notes Line
	1400 3200 3250 3200
Wire Notes Line
	3250 3200 3250 1050
Wire Notes Line
	3250 1050 1400 1050
Wire Notes Line
	1400 1050 1400 3200
Text Notes 1450 1200 0    50   ~ 0
FSR sensor landing pads
Text Notes 3500 1200 0    50   ~ 0
Connector to mainboard
Wire Notes Line
	3450 1050 7400 1050
Wire Notes Line
	7400 1050 7400 2700
Wire Notes Line
	7400 2700 3450 2700
Wire Notes Line
	3450 2700 3450 1050
Text Notes 1450 3500 0    50   ~ 0
MEMS microphone + OPAMP
$Comp
L SPU0410:SPU0410LR5H MK1
U 1 1 5BEEFC73
P 1950 4300
F 0 "MK1" H 2150 4600 50  0000 R CNN
F 1 "SPU0410LR5H" H 2550 4000 50  0000 R CNN
F 2 "SPU0410:SPU0410LR5H_6_3.76x3mm" H 1400 3650 50  0001 L CIN
F 3 "https://www.knowles.com/docs/default-source/model-downloads/spu0410lr5h-qb-revh32421a731dff6ddbb37cff0000940c19.pdf?" H 1950 4300 50  0001 C CNN
	1    1950 4300
	1    0    0    -1  
$EndComp
$Comp
L Device:R R4
U 1 1 5BEEFE65
P 4050 4950
F 0 "R4" V 3843 4950 50  0000 C CNN
F 1 "100k" V 3934 4950 50  0000 C CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 3980 4950 50  0001 C CNN
F 3 "~" H 4050 4950 50  0001 C CNN
	1    4050 4950
	0    1    1    0   
$EndComp
$Comp
L Device:R R2
U 1 1 5BEEFED3
P 3650 3850
F 0 "R2" H 3581 3804 50  0000 R CNN
F 1 "100k" H 3581 3895 50  0000 R CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 3580 3850 50  0001 C CNN
F 3 "~" H 3650 3850 50  0001 C CNN
	1    3650 3850
	1    0    0    1   
$EndComp
$Comp
L Device:R R1
U 1 1 5BEEFEFD
P 3450 4250
F 0 "R1" V 3243 4250 50  0000 C CNN
F 1 "10k" V 3334 4250 50  0000 C CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 3380 4250 50  0001 C CNN
F 3 "~" H 3450 4250 50  0001 C CNN
	1    3450 4250
	0    1    1    0   
$EndComp
$Comp
L Device:R R3
U 1 1 5BEEFF29
P 3650 4550
F 0 "R3" H 3581 4504 50  0000 R CNN
F 1 "100k" H 3581 4595 50  0000 R CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 3580 4550 50  0001 C CNN
F 3 "~" H 3650 4550 50  0001 C CNN
	1    3650 4550
	1    0    0    1   
$EndComp
Text Notes 2950 4900 0    50   ~ 0
Gain set to x10\n-> should be 2Vpp.
$Comp
L Device:C C2
U 1 1 5BEF0271
P 2850 4250
F 0 "C2" V 2598 4250 50  0000 C CNN
F 1 "4.7uF" V 2689 4250 50  0000 C CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 2888 4100 50  0001 C CNN
F 3 "~" H 2850 4250 50  0001 C CNN
	1    2850 4250
	0    1    1    0   
$EndComp
$Comp
L Amplifier_Operational:LMV321 U1
U 1 1 5BEF068E
P 4100 4150
F 0 "U1" H 4200 4250 50  0000 L CNN
F 1 "LMV321" H 4100 3950 50  0000 L CNN
F 2 "Package_TO_SOT_SMD:SOT-23-5" H 4100 4150 50  0001 L CNN
F 3 "http://www.ti.com/lit/ds/symlink/lmv324.pdf" H 4100 4150 50  0001 C CNN
	1    4100 4150
	1    0    0    -1  
$EndComp
Wire Wire Line
	3600 4250 3800 4250
Wire Wire Line
	3800 4250 3800 4950
Wire Wire Line
	3800 4950 3900 4950
Connection ~ 3800 4250
Wire Wire Line
	4200 4950 4450 4950
Wire Wire Line
	4450 4950 4450 4150
Wire Wire Line
	4450 4150 4400 4150
Wire Wire Line
	3650 4000 3650 4050
Wire Wire Line
	3800 4050 3650 4050
Connection ~ 3650 4050
Wire Wire Line
	3650 4050 3650 4400
Wire Wire Line
	3650 3700 4000 3700
Wire Wire Line
	4000 3700 4000 3850
Wire Wire Line
	3650 4700 4000 4700
Wire Wire Line
	4000 4700 4000 4600
$Comp
L power:GNDD #PWR0102
U 1 1 5BEF11CA
P 4100 4600
F 0 "#PWR0102" H 4100 4350 50  0001 C CNN
F 1 "GNDD" V 4104 4490 50  0000 R CNN
F 2 "" H 4100 4600 50  0001 C CNN
F 3 "" H 4100 4600 50  0001 C CNN
	1    4100 4600
	0    -1   -1   0   
$EndComp
Wire Wire Line
	4100 4600 4000 4600
Connection ~ 4000 4600
Wire Wire Line
	4000 4600 4000 4450
Text GLabel 4000 3700 1    60   Input ~ 0
3V3
Text Label 4450 4150 0    50   ~ 0
EXT1
$Comp
L Device:C C1
U 1 1 5BEF16B7
P 5500 4150
F 0 "C1" H 5615 4196 50  0000 L CNN
F 1 "100nF" H 5615 4105 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 5538 4000 50  0001 C CNN
F 3 "~" H 5500 4150 50  0001 C CNN
	1    5500 4150
	1    0    0    -1  
$EndComp
Text GLabel 5750 3950 1    60   Input ~ 0
3V3
Wire Wire Line
	5750 3950 5750 4000
Wire Wire Line
	5750 4000 5500 4000
$Comp
L power:GNDD #PWR0103
U 1 1 5BEF1D83
P 5750 4400
F 0 "#PWR0103" H 5750 4150 50  0001 C CNN
F 1 "GNDD" V 5754 4290 50  0000 R CNN
F 2 "" H 5750 4400 50  0001 C CNN
F 3 "" H 5750 4400 50  0001 C CNN
	1    5750 4400
	1    0    0    -1  
$EndComp
Wire Wire Line
	5750 4400 5750 4300
Wire Wire Line
	5750 4300 5500 4300
Wire Wire Line
	3000 4250 3300 4250
Wire Wire Line
	2350 4250 2700 4250
Text GLabel 1950 3950 1    60   Input ~ 0
3V3
Wire Wire Line
	1950 3950 1950 4000
$Comp
L power:GNDD #PWR0104
U 1 1 5BEF2D19
P 1950 4750
F 0 "#PWR0104" H 1950 4500 50  0001 C CNN
F 1 "GNDD" V 1954 4640 50  0000 R CNN
F 2 "" H 1950 4750 50  0001 C CNN
F 3 "" H 1950 4750 50  0001 C CNN
	1    1950 4750
	1    0    0    -1  
$EndComp
Wire Wire Line
	1950 4750 1950 4600
Wire Notes Line
	4750 3350 4750 5150
Wire Notes Line
	4750 5150 1400 5150
Wire Notes Line
	1400 5150 1400 3350
Wire Notes Line
	1400 3350 4750 3350
Text Notes 5000 3450 0    50   ~ 0
Decoupling for MEMS/OPAMP
Wire Notes Line
	4950 3350 4950 5150
Wire Notes Line
	4950 5150 6500 5150
Wire Notes Line
	6500 5150 6500 3350
Wire Notes Line
	6500 3350 4950 3350
NoConn ~ 4500 1700
NoConn ~ 4000 1700
NoConn ~ 4500 2000
$Comp
L Mechanical:MountingHole FID1
U 1 1 5BEFE497
P 7200 4000
F 0 "FID1" H 7300 4046 50  0000 L CNN
F 1 "Fiducial" H 7300 3955 50  0000 L CNN
F 2 "Fiducial:Fiducial_0.5mm_Dia_1mm_Outer" H 7200 4000 50  0001 C CNN
F 3 "~" H 7200 4000 50  0001 C CNN
	1    7200 4000
	1    0    0    -1  
$EndComp
$Comp
L Mechanical:MountingHole FID2
U 1 1 5BEFE6A7
P 7200 4200
F 0 "FID2" H 7300 4246 50  0000 L CNN
F 1 "Fiducial" H 7300 4155 50  0000 L CNN
F 2 "Fiducial:Fiducial_0.5mm_Dia_1mm_Outer" H 7200 4200 50  0001 C CNN
F 3 "~" H 7200 4200 50  0001 C CNN
	1    7200 4200
	1    0    0    -1  
$EndComp
$EndSCHEMATC
