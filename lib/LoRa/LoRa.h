#ifndef LORA_H
#define LORA_H

#define DISABLE_PING
#define DISABLE_BEACONS

#include <Arduino.h>
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

bool lmic_is_joined = false;

/**
 * Function decleration
 */
uint8_t* lora_queue ();
size_t lora_queue_len ();
void parse_downlink (unsigned char* data, size_t len);

void do_send (osjob_t* j);

void onEvent (ev_t ev);

void printHex2 (unsigned v);

// LoRa Pins
#define LORA_SCK (5)
#define LORA_CS (18)
#define LORA_MISO (19)
#define LORA_MOSI (27)
#define LORA_RST (23)
#define LORA_DIO0 (26)
#define LORA_DIO1 (33)
#define LORA_DIO2 (32)

static osjob_t sendjob;

#ifndef ENV_NEST
# ifndef COMPILE_REGRESSION_TEST
# error "You must create a working env/env_nest**.h file!"
# endif
static const unsigned char ENV_APPEUI[8] =  {0};
static const unsigned char ENV_DEVEUI[8] =  {0};
static const unsigned char ENV_APPKEY[16] = {0};
#endif

// This EUI must be in little-endian format, so least-significant-byte
// first. When copying an EUI from ttnctl output, this means to reverse
// the bytes. For TTN issued EUIs the last bytes should be 0xD5, 0xB3,
// 0x70.
void os_getArtEui (u1_t* buf) { memcpy_P(buf, ENV_APPEUI, 8);}

// This should also be in little endian format, see above.
void os_getDevEui (u1_t* buf) { memcpy_P(buf, ENV_DEVEUI, 8);}

// Pin mapping
const lmic_pinmap lmic_pins = {
  .nss = LORA_CS,
  .rxtx = LMIC_UNUSED_PIN,
  .rst = LORA_RST,
  .dio = {LORA_DIO0, LORA_DIO1, LORA_DIO2},
};

bool COMM_BLOCK = true;

// Schedule TX every this many seconds (might become longer due to duty
// cycle limitations).
const unsigned TX_INTERVAL = 300;

// This key should be in big endian format (or, since it is not really a
// number but a block of memory, endianness does not really apply). In
// practice, a key taken from ttnctl can be copied as-is.
void os_getDevKey (u1_t* buf) { memcpy_P(buf, ENV_APPKEY, 16);}

void do_send(osjob_t* j){
  // Check if there is not a current TX/RX job running
  if (LMIC.opmode & OP_TXRXPEND) {
    if (debug) Serial.println(F("OP_TXRXPEND, not sending"));
  } else {
    // Prepare upstream data transmission at the next possible time.
    uint8_t* data = lora_queue();
    size_t len = lora_queue_len();
    LMIC_setTxData2(1, data, len, 0);
    if (debug)
    {
      Serial.print(F("Packet queued "));
      print_hex(data, len);
    }
  }
  // Next TX is scheduled after TX_COMPLETE event.
}

void onEvent (ev_t ev) {
  if (debug) Serial.print(os_getTime());
  if (debug) Serial.print(": ");
  switch(ev) {
    case EV_SCAN_TIMEOUT:
      if (debug) Serial.println(F("EV_SCAN_TIMEOUT"));
      break;
    case EV_BEACON_FOUND:
      if (debug) Serial.println(F("EV_BEACON_FOUND"));
      break;
    case EV_BEACON_MISSED:
      if (debug) Serial.println(F("EV_BEACON_MISSED"));
      break;
    case EV_BEACON_TRACKED:
      if (debug) Serial.println(F("EV_BEACON_TRACKED"));
      break;
    case EV_JOINING:
      if (debug) Serial.println(F("EV_JOINING"));
      break;
    case EV_JOINED:
      if (debug) Serial.println(F("EV_JOINED"));
      {
        u4_t netid = 0;
        devaddr_t devaddr = 0;
        u1_t nwkKey[16];
        u1_t artKey[16];
        LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
        if (debug) Serial.print("netid: ");
        if (debug) Serial.println(netid, DEC);
        if (debug) Serial.print("devaddr: ");
        if (debug) Serial.println(devaddr, HEX);
        if (debug) Serial.print("AppSKey: ");
        for (size_t i=0; i<sizeof(artKey); ++i) {
          if (i != 0)
            if (debug) Serial.print("-");
          printHex2(artKey[i]);
        }
        if (debug) Serial.println("");
        if (debug) Serial.print("NwkSKey: ");
        for (size_t i=0; i<sizeof(nwkKey); ++i) {
          if (i != 0)
            if (debug) Serial.print("-");
          printHex2(nwkKey[i]);
        }
        if (debug) Serial.println();
      }
      // Disable link check validation (automatically enabled
      // during join, but because slow data rates change max TX
      // size, we don't use it in this example.
      LMIC_setLinkCheckMode(0);

      // change joined bool state to true
      lmic_is_joined = true;
      break;
    /*
    || This event is defined but not used in the code. No
    || point in wasting codespace on it.
    ||
    || case EV_RFU1:
    ||     if (debug) Serial.println(F("EV_RFU1"));
    ||     break;
    */
    case EV_JOIN_FAILED:
      if (debug) Serial.println(F("EV_JOIN_FAILED"));
      break;
    case EV_REJOIN_FAILED:
      if (debug) Serial.println(F("EV_REJOIN_FAILED"));
      break;
    case EV_TXCOMPLETE:
      if (debug) Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
      if (LMIC.txrxFlags & TXRX_ACK)
        if (debug) Serial.println(F("Received ack"));
      if (LMIC.dataLen) {
        if (debug) Serial.print(F("Received "));
        if (debug) Serial.print(LMIC.dataLen);
        if (debug) Serial.println(F(" bytes of payload"));

        unsigned char data[LMIC.dataLen];
        for (size_t i = 0; i < LMIC.dataLen; i++)
        {
          data[i] = LMIC.frame[LMIC.dataBeg + i];
        }

        parse_downlink(data, LMIC.dataLen);

      }
      // Schedule next transmission
      os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
      break;
    case EV_LOST_TSYNC:
      if (debug) Serial.println(F("EV_LOST_TSYNC"));
      break;
    case EV_RESET:
      if (debug) Serial.println(F("EV_RESET"));
      break;
    case EV_RXCOMPLETE:
      // data received in ping slot
      if (debug) Serial.println(F("EV_RXCOMPLETE"));
      break;
    case EV_LINK_DEAD:
      if (debug) Serial.println(F("EV_LINK_DEAD"));
      break;
    case EV_LINK_ALIVE:
      if (debug) Serial.println(F("EV_LINK_ALIVE"));
      break;
    /*
    || This event is defined but not used in the code. No
    || point in wasting codespace on it.
    ||
    || case EV_SCAN_FOUND:
    ||    if (debug) Serial.println(F("EV_SCAN_FOUND"));
    ||    break;
    */
    case EV_TXSTART:
      if (debug) Serial.println(F("EV_TXSTART"));
      break;
    case EV_TXCANCELED:
      if (debug) Serial.println(F("EV_TXCANCELED"));
      break;
    case EV_RXSTART:
      /* do not print anything -- it wrecks timing */
      break;
    case EV_JOIN_TXCOMPLETE:
      if (debug) Serial.println(F("EV_JOIN_TXCOMPLETE: no JoinAccept"));
      break;

    default:
      if (debug) Serial.print(F("Unknown event: "));
      if (debug) Serial.println((unsigned) ev);
      break;
  }
}

void printHex2(unsigned v) {
  v &= 0xff;
  if (v < 16)
    if (debug) Serial.print('0');
  if (debug) Serial.print(v, HEX);
}

#endif