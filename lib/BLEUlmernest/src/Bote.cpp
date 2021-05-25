#include <iostream>
#include "Bote.h"

/***************
 * Constructor
 ***************/

Bote::Bote ()
{
  std::vector<uint8_t> antwort;
  pBote = this;
}

Bote::Bote (Schluesselbund* _pBund)
{
  std::vector<uint8_t> antwort;
  pBote = this;
  pBund = _pBund;
}

Bote* Bote::pBote;

Bote::Bote (Bote& bote) {
    std::cout<<"Bote::Bote(Bote& bote)\n";
}


/*******************
 * Private Methods
 *******************/

void Bote::receive (Bote* pBote, uint8_t* pData, size_t len, bool keepData = false)
{
  if (keepData)
  {
    pBote->antwort.insert(pBote->antwort.end(), pData, pData + len);
  }
  else
  {
    pBote->antwort.clear();
    pBote->antwort.assign(pData, pData + len);
  }
}

void Bote::receive_crypto (Bote* pBote, uint8_t* pData, size_t len, bool keepData = false)
{
  if (pBote->pBund == nullptr)
  {
    if (debug) Serial.println("receive_crypto: pBund is nullptr");
    return;
  }

  uint8_t nonce[crypto_secretbox_NONCEBYTES];
  for (size_t i = 0; i < crypto_secretbox_NONCEBYTES; i++)
  {
    nonce[i] = pData[i];
  }
  // if (debug) Serial.print("nonce: ");
  // print_hex(nonce, crypto_secretbox_NONCEBYTES);
  uint32_t sl_auth_id = pData[crypto_secretbox_NONCEBYTES] | (pData[crypto_secretbox_NONCEBYTES + 1] << 8) |
    (pData[crypto_secretbox_NONCEBYTES + 2] << 16) | (pData[crypto_secretbox_NONCEBYTES + 3] << 24);
  // if (debug) Serial.print("sl id: ");
  // if (debug) Serial.println(sl_auth_id, HEX);
  size_t length = pData[crypto_secretbox_NONCEBYTES + 4] | pData[crypto_secretbox_NONCEBYTES + 5] << 8;
  size_t b_length = length + crypto_secretbox_BOXZEROBYTES - crypto_secretbox_ZEROBYTES;
  // if (debug) Serial.print("length of ");
  // if (debug) Serial.println(length);
  unsigned char b[b_length] = {0};
  unsigned char cipher[length];
  for (size_t i = 0; i < length; i++)
  {
    cipher[i] = pData[i + crypto_secretbox_NONCEBYTES + 6];
  }
  // if (debug) Serial.println("cipher: ");
  // print_hex(cipher, length);

  pBote->pBund->open(b, cipher, length, nonce);

  if (debug) Serial.println("crypto botschaft:");
  print_hex(b, b_length);

  if (!keepData) pBote->antwort.clear();
  pBote->antwort.insert(pBote->antwort.end(), b, b + b_length);
}


/******************
 * Getter, Setter
 ******************/

void Bote::set_pBund (Schluesselbund bund)
{
  pBund = &bund;
}
void Bote::set_pBund (Schluesselbund* pBund)
{
  pBund = pBund;
}

const std::vector<uint8_t> Bote::get_botschaft ()
{
  return botschaft;
}

const std::vector<uint8_t> Bote::get_antwort ()
{
  return antwort;
}

void Bote::get_antwort (uint8_t* out)
{
  std::copy(antwort.begin(), antwort.end(), out);
}


/******************
 * Public Methods
 ******************/

Bote& Bote::command (uint16_t command)
{
  botschaft.push_back(command);
  botschaft.push_back(command >> 8);

  return *this;
}

Bote& Bote::data (uint8_t* data, size_t len)
{
  if (data != nullptr && sizeof data > 0)
  {
    botschaft.insert(botschaft.end(), data, data + len);
  }
  return *this;
}

Bote& Bote::data (const uint8_t* data, size_t len)
{
  if (data != nullptr && sizeof data > 0)
  {
    botschaft.insert(botschaft.end(), data, data + len);
  }
  return *this;
}

Bote& Bote::send (BLERemoteCharacteristic* pRemoteCharacteristic, bool and_reset = false)
{
  uint16_t crc = crc_ccitt(botschaft.data(), botschaft.size());
  botschaft.push_back(crc);
  botschaft.push_back(crc >> 8);

  print_hex(botschaft.data(), botschaft.size());

  if(pRemoteCharacteristic->canWrite()) {
    pRemoteCharacteristic->writeValue(botschaft.data(), botschaft.size(), true);
  }
  else
  {
    if (debug) Serial.println("Cannot write to characteristic!");
  }

  if (and_reset)
  {
    botschaft.clear();
  }

  return *this;
}

Bote& Bote::send_cipher (BLERemoteCharacteristic* pRemoteCharacteristic, Schluesselbund* pBund)
{
  std::vector<unsigned char> b;
  uint32_t auth_id = pBund->get_auth_id();
  uint8_t auth_id_le[4] = { auth_id, auth_id >> 8, auth_id >> 16, auth_id >>24 };

  b.insert(b.end(), auth_id_le, auth_id_le + 4);
  b.insert(b.end(), botschaft.begin(), botschaft.end());
  botschaft.clear();

  uint16_t crc = crc_ccitt(b.data(), b.size());
  b.push_back(crc);
  b.push_back(crc >> 8);

  size_t length = b.size();
  size_t c_length = length + crypto_secretbox_BOXZEROBYTES;
  uint8_t c_length_le[2] = { c_length, (c_length >> 8) };

  unsigned char cipher[c_length] = {0};
  unsigned char nonce[crypto_secretbox_xsalsa20poly1305_NONCEBYTES] = {0};

  pBund->seal(cipher, b.data(), length, nonce);
  pBote->data(nonce, 24).data(auth_id_le, 4).data(c_length_le, 2).data(cipher, length + crypto_secretbox_BOXZEROBYTES);

  if (debug) Serial.print("sending crypto message: ");
  print_hex(botschaft.data(), botschaft.size());

  if(pRemoteCharacteristic->canWrite()) {
    pRemoteCharacteristic->writeValue(botschaft.data(), botschaft.size(), true);
  }
  else
  {
    if (debug) Serial.println("Cannot write to characteristic!");
  }

  botschaft.clear();
  b.clear();
  memset(cipher, 0, KEY_LENGTH);

  return *this;
}

Bote& Bote::reset ()
{
  botschaft.clear();

  return *this;
}

void Bote::register_indicate () {};

/**
 * Indication callback to recieve data
 */
void Bote::notifyCallback (BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify)
{
  if (debug)
  {
    Serial.print("** Notify callback for characteristic ");
    Serial.println(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" * of data length ");
    Serial.println(length);
    Serial.print(" * data: ");
    print_hex(pData, length);
  }
}

/**
 * Indication callback to recieve Nuki SL public key
 */
void Bote::notifyCallback_receive_pk (BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify)
{
    if (debug)
  {
    Serial.print("** Notify callback for characteristic ");
    Serial.println(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.println("** Key exchange");
    Serial.print(" * of data length ");
    Serial.println(length);
    Serial.print(" * data: ");
    print_hex(pData, length);
  }

  /**
   * Receiving Nuki SL public key in one indication requires a firmware update of the SL.
   * You can do so with the mobile app.
   */
  if (debug) Serial.println(" ** Public key exchange");

  if (pData[0] == 0x03 && pData[1] == 0x00)
  {
    if (crc_validate(pData, length))
    {
      BLEUlmernest::get_Bund()->set_sl_public_key(pData + 2, KEY_LENGTH);
      BLEUlmernest::set_current_state((int)pairing_state::send_pk);
    }
    else
    {
      BLEUlmernest::set_current_state((int)pairing_state::failed);
    }
  }
  else
  {
    if (debug) Serial.println(" ! public key not indicated: Is target Nuki SL in pairing mode?");
    BLEUlmernest::set_current_state((int)pairing_state::failed);
  }
}

/**
 * Indication callback for challenge
 */
void Bote::notifyCallback_challenge (BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify)
{
  if (debug)
  {
    Serial.print("** Notify callback for characteristic ");
    Serial.println(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.println("** Challenge");
    Serial.print(" * of data length ");
    Serial.println(length);
    Serial.print(" * data: ");
    print_hex(pData, length);
  }

  if (crc_validate(pData, length))
  {
    receive(pBote, pData + 2, length - 2);
    BLEUlmernest::set_current_state((int)pairing_state::challenge);
  }
  else
  {
    BLEUlmernest::set_current_state((int)pairing_state::failed);
  }
}

/**
 * Indication callback for authorization challenge
 */
void Bote::notifyCallback_challenge_auth (BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify)
{
  if (debug)
  {
    Serial.print("** Notify callback for characteristic ");
    Serial.println(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.println("** Challenge Auth");
    Serial.print(" * of data length ");
    Serial.println(length);
    Serial.print(" * data: ");
    print_hex(pData, length);
  }

  if (crc_validate(pData, length))
  {
    // TODO: crypto verification
    receive(pBote, pData + 2, length - 2);
    BLEUlmernest::set_current_state((int)pairing_state::challenge_auth);
  }
  else
  {
    BLEUlmernest::set_current_state((int)pairing_state::failed);
  }
}

/**
 * Indication callback to recieve authorization id
 */
void Bote::notifyCallback_get_auth_id (BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify)
{
  if (debug)
  {
    Serial.print("** Notify callback for characteristic ");
    Serial.println(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.println("** Get Auth ID");
    Serial.print(" * of data length ");
    Serial.println(length);
    Serial.print(" * data: ");
    print_hex(pData, length);
  }

  if (crc_validate(pData, length))
  {
    receive(pBote, pData + 2, length - 2);
    BLEUlmernest::set_current_state((int)pairing_state::conf_auth_id);
  }
  else
  {
    BLEUlmernest::set_current_state((int)pairing_state::failed);
  }
}

/**
 * Indication callback for authorization id confirmation
 */
void Bote::notifyCallback_confirm_auth_id (BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify)
{
  if (debug)
  {
    Serial.print("** Notify callback for characteristic ");
    Serial.println(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.println("** Confirm Auth ID");
    Serial.print(" * of data length ");
    Serial.println(length);
    Serial.print(" * data: ");
    print_hex(pData, length);
  }

  if (crc_validate(pData, length))
  {
    if (pData[0] == 0x0E && pData[1] == 0x00 && pData[2] == 0x00)
    {
    receive(pBote, pData + 2, length - 2);
    BLEUlmernest::set_current_state((int)pairing_state::done);
    }
    else
    {
      if (debug)
      {
        Serial.print(" ! error confirm auth id: ");
        Serial.println(pData[2]);
      }
      BLEUlmernest::set_current_state((int)pairing_state::failed);
    }
  }
  else
  {
    BLEUlmernest::set_current_state((int)pairing_state::failed);
  }
}

/**
 * Indication callback for encrypted data
 */
void Bote::notifyCallback_crypto (BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify)
{
  if (debug) Serial.print("** Notify callback for characteristic ");
  if (debug) Serial.println(pBLERemoteCharacteristic->getUUID().toString().c_str());
  if (debug) Serial.print(" * of data length ");
  if (debug) Serial.println(length);
  if (debug) Serial.print(" * data: ");
  print_hex(pData, length);

  receive_crypto(pBote, pData, length);

  BLEUlmernest::set_current_state((int)transmission::t_rx_success);
}

/**
 * Indication callback for challenge
 */
void Bote::notifyCallback_req_challenge (BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify)
{
  if (debug) Serial.print("** Notify callback for characteristic ");
  if (debug) Serial.println(pBLERemoteCharacteristic->getUUID().toString().c_str());
  if (debug) Serial.println("** Challenge");
  if (debug) Serial.print(" * of data length ");
  if (debug) Serial.println(length);
  if (debug) Serial.print(" * data: ");
  print_hex(pData, length);

  receive_crypto(pBote, pData, length);

  BLEUlmernest::set_current_state((int)transmission::t_challenge);
}
