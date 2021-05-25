#include "Schluesselbund.h"

/***************
 * Constructor
 ***************/

Schluesselbund::Schluesselbund () {}


/******************
 * Getter, Setter
 ******************/

const uint8_t* Schluesselbund::get_device_name ()
{
  return device_name;
}

const uint32_t Schluesselbund::get_auth_id ()
{
  return auth_id;
}
const uint8_t* Schluesselbund::get_auth_id_le ()
{
  const uint8_t auth_id_le[4] = { auth_id, (auth_id >> 8), (auth_id >> 16), (auth_id >> 24) };
  return auth_id_le;
}
void Schluesselbund::set_auth_id (uint32_t id)
{
  auth_id = id;
  esp_storage.putUInt("auth_id", id);
}

const uint8_t* Schluesselbund::get_device_bound_uuid ()
{
  return device_bound_uuid;
}
void Schluesselbund::set_device_bound_uuid (const uint8_t* uuid)
{
  std::copy(uuid, uuid + 16, device_bound_uuid);
  if (debug) Serial.print("Storing uuid: ");
  print_hex(device_bound_uuid, 16);
  esp_storage.putBytes("bound_uuid", uuid, 16);
}

const uint8_t* Schluesselbund::get_public_key ()
{
  return public_key;
}

void Schluesselbund::set_sl_public_key (uint8_t* key, size_t len)
{
  std::copy(key, key + len, sl_public_key);
}

const uint8_t* Schluesselbund::get_sl_public_key ()
{
  return sl_public_key;
}

size_t Schluesselbund::get_address (char* stored_address)
{
  return esp_storage.getString("addr", stored_address, 18);
}

void Schluesselbund::store_address (char* address_to_remember)
{
  esp_storage.putString("addr", address_to_remember);
}


/*******************
 * Private Methods
 *******************/

bool Schluesselbund::store_key (const char* name, uint8_t* key, size_t len)
{
  if (key != nullptr && len == KEY_LENGTH)
  {
    esp_storage.putBytes(name, key, len);
    return true;
  }
  else
  {
    // Something went wrong!
    if (debug) Serial.print("Could not store ");
    if (debug) Serial.print(name);
    if (debug) Serial.println("!");
    return false;
  }
}


/******************
 * Public Methods
 ******************/

/**
 * Initialize Schluesselbund
 */
void Schluesselbund::init (std::__cxx11::string name)
{
  esp_storage.begin("nest_esp32", false);

  esp_storage.getString("device_name", String("default_name")).toCharArray((char*)device_name, 32);
  bool matching_name = true;
  for (size_t i = 0; i < 32; i++)
  {
    if (name.c_str()[i] != (char)device_name[i])
    {
      matching_name = false;
      break;
    }
  }
  if (!matching_name)
  {
    std::copy(name.c_str(), name.c_str() + 32, device_name);
    esp_storage.putString("device_name", name.c_str());
  }

  auth_id = esp_storage.getUInt("auth_id");
  esp_storage.getBytes("bound_uuid", device_bound_uuid, 16);

  if (debug) Serial.print("name: ");
  if (debug) Serial.print((char*)device_name);
  if (debug) Serial.print(" ");
  print_hex(device_name, 32);
  if (debug) Serial.print("id: ");
  if (debug) Serial.println(auth_id);
  print_hex(device_bound_uuid, 16);
}

/**
 * Generate a new public key and secret key
 */
void Schluesselbund::generate_keypair ()
{
  crypto_box_keypair(public_key, secret_key);
}

/**
 * Calculation for Diffie–Hellman key exchange
 */
void Schluesselbund::calc_dh_key ()
{
  if (secret_key != nullptr && sl_public_key != nullptr)
  {
    int e = crypto_scalarmult_curve25519(dh_key, secret_key, sl_public_key);

    if (e < 0)
    {
      if (debug) Serial.println(" # gernate_DH_key: something went wrong!");
    }
  }
  else
  {
    if (debug) Serial.println(" # gernate_DH_key: something went wrong!");
  }
}

/**
 * Calculate shared secret
 */
void Schluesselbund::calc_shared_secret ()
{
  calc_dh_key();

  unsigned char inv[16] = {0};
  unsigned char c[16];
  std::string c_buffer = "expand 32-byte k";
  std::copy(c_buffer.begin(), c_buffer.end(), c);

  crypto_core_hsalsa20(shared_secret, inv, dh_key, c);

  wipe(dh_key, KEY_LENGTH);
}

/**
 * Calculate authorization authentication
 */
void Schluesselbund::calc_auth (const unsigned char* challenge, size_t len, unsigned char* hash)
{
  calc_shared_secret();

  crypto_auth_hmacsha256_state state;
  crypto_auth_hmacsha256_init(&state, shared_secret, KEY_LENGTH);
  crypto_auth_hmacsha256_update(&state, challenge, len);
  crypto_auth_hmacsha256_final(&state, hash);

  wipe(shared_secret, KEY_LENGTH);
}

/**
 * Load keys from non-volatile memory
 */
void Schluesselbund::grab_keys ()
{
  if (debug) Serial.println(" # grab...");
  esp_storage.getBytes("public_key", public_key, sizeof public_key);
  esp_storage.getBytes("secret_key", secret_key, sizeof secret_key);
  esp_storage.getBytes("sl_public_key", sl_public_key, sizeof sl_public_key);

  if (debug)
  {
    Serial.print("  public key: ");
    print_hex(public_key, 32);
    Serial.print("  secret key: ");
    print_hex(secret_key, 32);
    Serial.print("  sl public key: ");
    print_hex(sl_public_key, 32);
  }
}

/**
 * Store this devices public key, secret key, and Nuki SL public key from RAM in non-volatile memory
 */
void Schluesselbund::store_keys ()
{
  if (debug) Serial.println(" # Store!");
  store_key("public_key", public_key, sizeof public_key);
  store_key("secret_key", secret_key, sizeof secret_key);
  store_key("sl_public_key", sl_public_key, sizeof sl_public_key);
}

/**
 * Remove credentials from non-volatile memory
 */
void Schluesselbund::clear_credentials ()
{
  esp_storage.remove("addr");
  esp_storage.remove("public_key");
  esp_storage.remove("secret_key");
  esp_storage.remove("sl_public_key");
}

/**
 * Create cipher for a message
 */
void Schluesselbund::seal (unsigned char* cipher, const unsigned char* botschaft, size_t length, unsigned char* nonce_out)
{
  esp_fill_random(nonce_out, crypto_secretbox_NONCEBYTES);
  calc_shared_secret();

  size_t length_padded = length + crypto_secretbox_ZEROBYTES;
  unsigned char c_padded[length_padded] = {0};
  unsigned char b_padded[length_padded] = {0};
  for (size_t i = crypto_secretbox_ZEROBYTES;i < length_padded; ++i) b_padded[i] = botschaft[i - crypto_secretbox_ZEROBYTES];

  if (crypto_secretbox_xsalsa20poly1305(c_padded, b_padded, length_padded, nonce_out, shared_secret) != 0)
  {
    wipe(shared_secret, KEY_LENGTH);
    if (debug) Serial.print(" # seal: something went wrong! ");
    return;
  }

  std::copy(c_padded + crypto_secretbox_BOXZEROBYTES, c_padded + length_padded, cipher);

  wipe(shared_secret, KEY_LENGTH);
}

/**
 * Get message for a cipher
 */
void Schluesselbund::open (unsigned char* botschaft, const unsigned char* cipher, size_t length, unsigned char* nonce)
{
  calc_shared_secret();

  size_t length_padded = length + crypto_secretbox_BOXZEROBYTES;
  unsigned char b_padded[length_padded] = {0};
  unsigned char c_padded[length_padded] = {0};
  for (size_t i = crypto_secretbox_BOXZEROBYTES; i < length_padded; ++i) c_padded[i] = cipher[i - crypto_secretbox_BOXZEROBYTES];

  if (crypto_secretbox_xsalsa20poly1305_open(b_padded, c_padded, length_padded, nonce, shared_secret) != 0)
  {
    wipe(shared_secret, KEY_LENGTH);
    if (debug) Serial.println(" # open: something went wrong!");
    return;
  }

  if (length - crypto_secretbox_BOXZEROBYTES < 0)
  {
    wipe(shared_secret, KEY_LENGTH);
    if (debug) Serial.print(" # open: 'length' is too short! ");
    if (debug) Serial.println(length - crypto_secretbox_BOXZEROBYTES);
    return;
  }

  std::copy(b_padded + crypto_secretbox_ZEROBYTES, b_padded + (length_padded), botschaft);

  wipe(shared_secret, KEY_LENGTH);
}

/**
 * Wipe memory
 */
void Schluesselbund::wipe (void* buffer, size_t len)
{
  sodium_memzero(buffer, len);
}

/**
 * Wipe non-volatile memory
 */
void Schluesselbund::wipe_storage ()
{
  if (esp_storage.clear())
  {
    if (debug)
    {
      Serial.println(" # Successfully cleared non-volatile memory");
    }
  }
  else
  {
    if (debug)
    {
      Serial.println(" ! Failed to clear non-volatile memory!");
    }
  }
}
