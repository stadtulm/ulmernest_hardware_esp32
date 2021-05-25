# Installation

Das Projekt verwendet [platformio](https://platformio.org).

## Git Submodule

Teil des Projekts ist in anderen Git Repositorien zu finden:

*BLEUlmernest* Bluetooth low energie integration für das Nuki SmartLock 2, basiert auf der esp32 Arduino BLE library.

Zur Installation, kann folgende Anweisung genutzt werden:
```
git submodule update --init --recursive
```

Und im Weiteren zur Aktualisierung:
```
git submodule update
```

Änderungen müssen im jeweils entsprechenden Verzeichnis eines Submodules durch ```git add```, ```git commit``` (und ```git push```) festgehalten werden. Anschließend werden diese Änderungen im Submodul ebenfalls im übergeordneten Repositorium durch ```add``` ```commit``` (```push```) festgehalten.

# Kommunikation

Das esp32 steht im Datenaustausch mit dem Raspberry Pi via Serialport und mit dem TTN Netzwerk via LoRaWan.

## Serielle Kommunikation

Diese dient der kommunikation zwischen *Esp32* und *Raspberry Pi*.
Zur Kommunikation gehört die Anfrage, Weitergabe und Aktualisierung von Daten, sowie der Aufruf von Funktionen auf dem jeweils anderen Device.

Jede Aneinanderkettung von Befehl-, Daten- oder Code-Bytes bei Anweisungen bleibt zwischen Request und Response unverändert.
Eine Übertragung wird mit 0x00 abgeschlossen.

### Übertragung

| Command                   | Command Byte  | n-Byte                                                                                                | n Data-Bytes                                                                                      | Implementierung
|---                        |---            |---                                                                                                    |---                                                                                                |---
| Abschließendes Byte       | ```0x00```
| Request Data              | ```0x01```    | *n* ist gleich der Zahl der anforderten Parameter                                                     | Byte-Codes der angeforderten Parameter                                                            | all
| Response Data             | ```0x10```    | *n* ist gleich der Summe der angeforderten Data-Bytes, plus jeweils vorangestellten Parameter-Code    | Paare Parameter-Code und Data-Bytes der angeforderten Parameter mit entsprechender Formatierung   | all
| Request State             | ```0x02```    | ```0x00```                                                                                            | none                                                                                              | esp32
| Response State            | ```0x20```    | ```0x01```                                                                                            | Code des Ausführungszustand                                                                       | Raspberry Pi
| Update Data               | ```0x03```    | Summe: 1 (Byte Parameter-Code) + Länge Data-Bytes                                                     | Parameter-Code (1 Byte) gefolgt von Data-Bytes                                                    | all
| Update State              | ```0x13```    | ```0x01```                                                                                            | 1 Byte neuer Ausführungszustand                                                                   | **esp32**
| Open Lock                 | ```0x04```    | ```0x00```                                                                                            | none                                                                                              | Raspberry Pi
| Close Lock                | ```0x40```    | ```0x00```                                                                                            | none                                                                                              | Raspberry Pi
| LoRa Nachricht            | ```0x11```    | *n* ist gleich der Zahl der Bytes der LoRa Nachricht                                                  | Byte-Array                                                                                        | Raspberry Pi
| Vorbereitung auf Sleep    | ```0x06```    | ```0x00```                                                                                            | none                                                                                              | esp32
| Esp32 Neustart            | ```0x07```    | ```0x01```                                                                                            | 0xFF; zusätzlicher Wert um zufälligen Neustart zu vermeiden                                       | Raspberry Pi
| VeDirectHanlder On/Off    | ```0x08```    | ```0x01```                                                                                            | 0x01 oder größer ON; 0x00 OFF                                                                     | Raspberry Pi
| Esp32 Nuki Daten löschen  | ```0x09```    | ```0x01```                                                                                            | 0xFF; zusätzlicher Wert um zufälligen Neustart zu vermeiden                                       | Raspberry Pi

### Parameter Code

| Parameter                     | Byte-Code   | Bytes   | Formatierung
|---                            |---          |---      |---
| Temperatur Außen              | ```0x01```  | 2 Bytes | 0.1 °C
| Temperatur Innen              | ```0x02```  | 2 Bytes | 0.1 °C
| Luftfeuchtigkeit Innen        | ```0x03```  | 1 Byte  | % in 0.5% relative Luftfeuchtigkeit
| Türe                          | ```0x04```  | 1 Byte  | offen/geschlossen
| Schloss                       | ```0x05```  | 1 Byte  | Nuki lock state Byte Code
| Bewegungsmelder               | ```0x06```  | 1 Byte  | ausgelöst
| Rauchmelder                   | ```0x07```  | 1 Byte  | aus/an
| Batterieladung                | ```0x08```  | 2 Byte  | 0.01 V
| Licht schalter                | ```0x09```  | 1 Byte  | ausgelößt
| Zustand Gebläse               | ```0x09```  | 1 Byte  | aus/an

### Beispiel

Datenanfrage Parameter1, Parameter4 und Parameter3:

Reqest Data | Zahl der Parameter | Parameter1 | Parameter4 | Parameter3 | Abschließendes Byte

```01 03 01 04 03 00```

Antwort:

Response Data | Zahl der Data-Bytes | 1 Byte Parameter1 | 2 Byte Parameter4 | 1 Byte Parameter3 | Abschließendes Byte

```10 04 12 3456 78 00```

Update Außentemperatur:

Update Data | Summe Parameter-Byte + Daten-Bytes | 1 Byte Parameter Code | 2 Bytes Daten | Abschließendes Byte

```03 03 01 00 81 00```

---

## LoRaWan Kommunikation

### Down-Link

| Anweisung                         | Anweisungs-Byte       | Daten-Bytes
|---                                |---                    |---
| Ausführungszustand ändern         | 0x01                  | Neuer Zustand
| Schloss öffnen                    | 0x04                  | none
| Schloss schließen                 | 0x40                  | none
| Sync Datetime ???                 | 0x04                  | Jahr x2, Monat, Tag, Stunde, Minute, Sekunde
| Sync Time     ???                 | 0x05                  | Stunde, Minute, Sekunde
| Raspberry Pi sleep                | 0x06                  | 0xFF
| Raspberry Pi wake                 | 0x60                  | 0xFF
| Esp32 Neustart                    | 0x07                  | 0xFF
| [...]

| Zustand                   | Code
|---                        |---
| CLOSED_TEMP               | 0x01
| CLOSED_CLEAN              | 0x02
| CLOSED_MAINTENANCE        | 0x03
| CLOSED_OTHER              | 0x04
| AVAILABLE                 | 0x05
| AV_DOOR_OPEN              | 0x06
| AV_DOOR_CLOSED            | 0x07
| OCC_LOCKED                | 0x08
| OCC_DOOR_OPEN             | 0x09
| OCC_DOOR_CLOSED           | 0x10
| OCC_MOTION                | 0x11

### Beispiel Anweisungen

| Anweisung                     | Byte Array
|---                            |---
| Closed, temperature           | ```[0x01, 0x01]```
| Closed, cleaning              | ```[0x01, 0x02]```
| Closed, maintenance           | ```[0x01, 0x03]```
| Closed, other                 | ```[0x01, 0x04]```
| Available                     | ```[0x01, 0x05]```
| Available, door open          | ```[0x01, 0x06]```
| Available, door closed        | ```[0x01, 0x07]```
| Occupied, locked              | ```[0x01, 0x08]```
| Occupied, door open           | ```[0x01, 0x09]```
| Occupied, door closed         | ```[0x01, 0x10]```
| Occupied, motion              | ```[0x01, 0x11]```
| Türe aufschließen             | ```[0x04]```
| Raspberry schlafen legen      | ```[0x06, 0xFF]```
| Raspberry aufwecken           | ```[0x60, 0xFF]```

---

### Up-Link

Mit [Cayenne LPP](https://developers.mydevices.com/cayenne/docs/lora/#lora-cayenne-low-power-payload)

| Info                            | Channel (Cayenne LPP)   | Datentyp                        | Datenformat
|---                              |---                      |---                              |---
| Fehlercode                      | ```0x00```              | Digital In                      | 1 Byte
| Ausführungszustand              | ```0x01```              | Digital In                      | 1 Byte
| Temeratur Außen                 | ```0x02```              | Temperatur                      | 2 Bytes; 0.1°C signed MSB
| Temperatur Innen                | ```0x03```              | Temperatur                      | 2 Bytes; 0.1°C signed MSB
| Luftfeuchtigkeit Innen          | ```0x04```              | relative Luftfeuchtigkeit       | 1 Byte; 0.5% Unsigned
| Türsensor                       | ```0x05```              | Digital In                      | 1 Byte; Zustands-Code
| Schlosssensor                   | ```0x06```              | Digital In                      | 1 Byte; Zustands-Code
| Rauchmelder                     | ```0x07```              | Digital In                      | 1 Byte; Zustands-Code
| Batterie Spannung               | ```0x08```              | Analog In                       | 2 Byte signed; 0.01 V
| Zähler: Türe                    | ```0x09```              | Digital In                      | MSB: letzter Zustand 0|1; 7-Bit: Zähler
| Zähler: Schloss                 | ```0x0A```              | Digital In                      | MSB: letzter Zustand 0|1; 7-Bit: Zähler
| Zähler: Bewegungsmelder         | ```0x0B```              | Digital In                      | 1 Byte; Zähler
| Zähler: Lichtschalter           | ```0x0C```              | Digital In                      | 1 Byte; Zähler
| stündlich: MPPT Batterie Volt   | ```0x0D```              | Analog In                       | 2 Byte; 1 mV / 100 signed
| stündlich: Verbraucher Energie  | ```0x0E```              | Analog In                       | 2 Byte; 0.01 mWh signed
| stünlich: PV yield today        | ```0x0F```              | Analog In                       | 2 Byte; 0.01 kWh signed

### Fehlercodes

Nach der initialen Verbindung von LoRaWan wird ein Fehlercode mit den ```RESET_REASON```s gesendet. Der Code enthält ein Wert für beide Cores des esp-Prozessors. Je Core kann der Wert einer von Sechzehn Möglichkeiten entsprechen. Somit können auch beide Werte mittels einem Byte übertragen werden.

Wichtig: Zum abgleich mit den entsprchenden [reset Gründen](https://github.com/espressif/arduino-esp32/blob/2452c1fb539246e47a715b74a3ad25b8a7ebbec7/tools/sdk/include/esp32/rom/rtc.h#L82) muss zu jedem der übertragenen Werte ```1``` addiert werden. Die Ursprungswerte reichen von Eins (0x01) bis Sechzehn (0x10). Zur Übermittlung wird somit ```1``` subtrahiert, um die Ergebnisse (0x0 bis 0xF) als eine Hexadezimalstelle eines Bytes zu repräsentieren.

### Ausführungszustand

| Zustand             | Code
|---                  |---
| CLOSED_TEMP         | 0x01
| CLOSED_CLEAN        | 0x02
| CLOSED_MAINTENANCE  | 0x03
| CLOSED_OTHER        | 0x04
| AVAILABLE           | 0x05
| AV_DOOR_OPEN        | 0x06
| AV_DOOR_CLOSED      | 0x07
| OCC_LOCKED          | 0x08
| OCC_DOOR_OPEN       | 0x09
| OCC_DOOR_CLOSED     | 0x10
| OCC_MOTION          | 0x11

### Türsensor und Schloss
| Zustand | Code
|---|---
| Geschlossen | 0x00
| Offen | 0x01

### Tür- und Schlosssensor MSB
| Zustand     | Code
|---          |---
| Geschlossen | 0b0XXXXXXX
| Offen       | 0b1XXXXXXX

### Rauchmelder
| Zustand   | Code
|---        |---
| Normal    | 0x00
| Alarm     | 0x01

### Licht
|Zustand    | Code
|---        |---
| Licht aus | 0x00
| Licht an  | 0x01

### Beispiel

Lora Payload:
```
 1 | {
 2 |   digital_in_1 =         16,
 3 |   temperature_2 =        -2.1,
 4 |   temperature_3 =         1.3,
 5 |   relative_humidity_4 =  74,
 6 |   analog_in_8 =          12.72,
 7 |   digital_in_9 =        139,
 8 |   digital_in_10 =       132,
 9 |   digital_in_11 =        23
10 | }
```

Die Werte sind mit Datentyp und Channel-Name betitelt. In Zeile drei findet man z.B. den Wert für »Temperatur, außen«, anhand des Datentyps ```temperature``` und dem Kanal ```2```, mit dem Wert ```–2.1``` in Grad Celsius.

Einige Werte sind als 1-Byte-lange Werte codiert. In Zeile Zwei (```digital_in_1```) findet man z.B. einen Code der den Ausführungszustand des Nests enthält. Der Wert ```16``` entspricht dem Hexadezimalwert ```0x10``` und kann der Tabelle »Ausführungszustand« als »OCC_DOOR_CLOSED« entnommen werden.

Des Weiteren gibt es Zähler, welche, wie etwa in Zeile Neun, einen 1-Byte-langen Wert beinhalten. Zähler zeichnen die Häufigkeit von Statusänderungen auf. Zusätzlich Enthalten die Zähler für den Tür- und Schlosssensor (Zeile Sieben und Acht) den letzten bekannten Status als Most Significant Bit (MSB). Das bedeutet, sobald der dezimale Wert gleich oder größer 128 ist, hat das MSB den Wert 1. Die übrigen sieben Bit enthalten den Wert des Zählers. Der Parameter des Schlosssensors in Zeile Acht ist größer als 128 und enthält demnach, an der Stelle des MSB, den Wert ```1``` (offen) als letzten bekannten Status, sowie den Aktionszähler mit dem Wert 4.