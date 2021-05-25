# Aktualisiert im respository readme: ```/readme.md```

# Serielle Kommunikation

Jede Aneinanderkettung von Befehl-, Daten- oder Code-Bytes bei Anweisungen bleibt zwischen Request und Response unverändert.
Eine Übertragung wird mit 0x00 abgeschlossen.

## Übertragung

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
| Vorbereitung auf Sleep    | ```0x06```    | ```0x00```

## Parameter Code

| Parameter                     | Byte-Code   | Bytes   | Formatierung
|---                            |---          |---      |---
| Temperatur Außen              | ```0x01```  | 2 Bytes |
| Temperatur Innen              | ```0x02```  | 2 Bytes |
| Luftfeuchtigkeit Innen        | ```0x03```  | 1 Byte  |
| Türe                          | ```0x04```  | 1 Byte  |
| Schloss                       | ```0x05```  | 1 Byte  |
| Bewegungsmelder               | ```0x06```  | 1 Byte  |
| Rauchmelder                   | ```0x07```  | 1 Byte  |
| Batterieladung                | ```0x08```  | 1 Byte  |
| Zähler Türsensor              | ```0x09```  | 1 Byte  |
| Zähler Schlossaktionen        | ```0x0A```  | 1 Byte  |
| Zähler Bewegungsmelder        | ```0x0B```  | 1 Byte  |
| Zähler Lichtschalter          | ```0x0C```  | 1 Byte  |
| MPPT Batterie Volt            | ```0x0D```  | 1 Byte  |
| MPPT Energie Verbraucher      | ```0x0E```  | 1 Byte  |
| MPPT Yield today              | ```0x0E```  | 1 Byte  |
