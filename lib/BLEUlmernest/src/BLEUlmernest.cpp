#include "BLEUlmernest.h"

// Nuki SL BLE UUIDs
BLEUUID BLEUlmernest::uuid_pairing_service("a92ee100-5501-11e4-916c-0800200c9a66");
BLEUUID BLEUlmernest::uuid_pairing_characteristic("a92ee101-5501-11e4-916c-0800200c9a66");
BLEUUID BLEUlmernest::uuid_service("a92ee200-5501-11e4-916c-0800200c9a66");
BLEUUID BLEUlmernest::uuid_user_specific_dio_characteristic("a92ee202-5501-11e4-916c-0800200c9a66");

Bote* BLEUlmernest::pBote;
Schluesselbund* BLEUlmernest::pBund;
std::vector<BLEAdvertisedDevice> BLEUlmernest::matched_devices;
BLEAdvertisedDevice BLEUlmernest::device;
std::vector<BLEAdvertisedDevice> BLEUlmernest::scan_results;
BLEAdvertisedDevice* BLEUlmernest::pNuki;
BLEScan* BLEUlmernest::pBLEScan;
BLEClient* BLEUlmernest::pClient;
BLERemoteService* BLEUlmernest::pRemoteService;
BLERemoteCharacteristic* BLEUlmernest::pRemoteCharacteristic;
BLERemoteService* BLEUlmernest::pUserService;
BLERemoteCharacteristic* BLEUlmernest::pUSDIO;
char BLEUlmernest::stored_address[18];
int BLEUlmernest::current_state;

// Nuki SL Keyturner States https://developer.nuki.io/page/nuki-smart-lock-api-latest/2/ -> Commands -> Keyturner States
KeyturnerStates BLEUlmernest::keyturner_states;


/***************
 * Constructor
 ***************/
BLEUlmernest::BLEUlmernest ()
{
}


/*******************
 * Private Methods
 *******************/

/**
 * Scan for BLE devices
 */
std::vector<BLEAdvertisedDevice> BLEUlmernest::scan()
{
  if (debug) Serial.println(" - Scanning for Nuki SL...");

  // create new scan
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99); // less or equal setInterval value

  uint8_t scan_count = 0;
  // While no matches and either not reached max number of tries or
  while (matched_devices.size() == 0 && (SCAN_MAX_TRYS > scan_count || SCAN_MAX_TRYS == 0))
  {
    pBLEScan->setActiveScan(true); // active scan uses more power, but get results faster
    BLEScanResults foundDevices = pBLEScan->start(SCAN_TIME_SEC, false);

    for (int i = 0; i < foundDevices.getCount(); i++) {

      device = foundDevices.getDevice(i);
      if (device.getName().find("Nuki") != std::string::npos) {
        if (debug) Serial.println();
        if (debug) Serial.print(" - Device found ");
        if (debug) Serial.println(device.toString().c_str());
        matched_devices.push_back(device);
      }
    }
    pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
    if (matched_devices.size() == 0)
    {
      if (debug) Serial.println(" - No Nuki found");
      // delay(2000);
      // esp_restart();
    }
    scan_count++;
  }
  return matched_devices;
}

/**
 * Initial connection to BLE
 */
BLEClient* BLEUlmernest::initial_connect ()
{
  // Call scan() and store results
  scan_results = scan();

  // Return nullptr if there are no scan results
  if (scan_results.size() == 0)
  {
    if (debug) Serial.printf(" ! Max number (%d) trys to connect reached, returning nullptr\n", SCAN_MAX_TRYS);
    return nullptr;
  }

  // Setup BLE client
  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new ClientCallbacks_Connect());

  // try to load stored BLE address
  //   1) no address loaded: continue to try to pair
  //   2) address loaded: try to match with scan results
  bool matching = true;
  std::string connected_address;
  if (pBund->get_address(stored_address) == 0)
  {
    if (debug) Serial.println(" ! no address stored: trying to pair");
  }
  else // try to match stored mac address with scan_resutls
  {
    if (debug)
    {
      Serial.print(" - stored address: ");
      Serial.println(stored_address);
    }

    size_t i, j;
    for (i = 0; i < scan_results.size(); i++)
    {
      device = scan_results.data()[i];

      // Connect to the remove BLE Server.
      pClient->connect(&device);
      if (pClient->isConnected()) {
        if (debug) Serial.print(" - Is Connected to ");
        if (debug) Serial.println(device.getName().c_str());
      }
      delay(50);

      connected_address = device.getAddress().toString();
      if (debug)
      {
        Serial.print(" - connected address: ");
        Serial.println(connected_address.c_str());
      }

      matching = true;
      j = 0;
      while (j < 18 && matching)
      {
        if (stored_address[j] != connected_address.c_str()[j++])
        {
          matching = false;
          if (debug)
          {
            Serial.print(" ! not a match: j = ");
            Serial.println(j);
          }
        }
      }

      // Match found: continue initial connection
      if (matching)
      {
        pNuki = &device;
        i = scan_results.size();
        scan_results.clear();

        // load stored credentials
        pBund->grab_keys();

        // try to connect with user specific credentials
        // this will most likely fail, but should result in successful calls later on
        connect_user_specific();

        return pClient;
      }
      // No match found: contiue to try scan results
      else
      {
        pClient->disconnect();
        delay(50);
      }
    }
    // None of the scan resuslts machted stored address
    if (debug) Serial.println(" ! No matching device found");
  }

  if (debug)
  {
#ifdef overwrite_stored_device_pairing
    if (matching) // * matching is true and function has not returned -> no address is stored
    {
      if (overwrite_stored_device_pairing || !overwrite_stored_device_pairing)
      {
        Serial.println(" - trying to pair with no stored address; nothing will be overwritten");
      }
    }
    else // * matching is false and will continue accoding to overwrite_stored_device_pairing, if defined
    {
      if (overwrite_stored_device_pairing)
      {
        pBund->clear_credentials();
        Serial.println(" - trying to pair and overwirte");
      }
      else if (!overwrite_stored_device_pairing)
      {
        Serial.println(" - not trying to pair to keep stored device");
        return nullptr;
      }
    }
#else
    if (matching) // * matching is true and function has not returned -> no address is stored
    {
      Serial.println(" - trying to pair");
    }
    else // * matching is false, an address stored and overwrite_stored_device_pairing not defined -> reutrn nullptr
    {
      Serial.println(" - not trying to pair to keep stored device");
      return nullptr;
    }
#endif
  }

  // try pairing with scan_results
  for (size_t i = 0; i < scan_results.size(); i++)
  {
    device = scan_results.data()[i];

    // Connect to the remote BLE Server.
    pClient->connect(&device);
    if (pClient->isConnected()) {
      if (debug) Serial.print(" - Is Connected to ");
      if (debug) Serial.println(device.getName().c_str());
    }
    delay(50);

    // Obtain a reference to a service from the remote BLE server.
    while (pClient->isConnected() && pRemoteService == nullptr)
    {
      pRemoteService = pClient->getService(uuid_pairing_service);
      delay(10);
    }
    if (pRemoteService == nullptr) {
      if (debug) Serial.print(" ! Failed to find our service UUID: ");
      if (debug) Serial.println(uuid_pairing_service.toString().c_str());
      pClient->disconnect();
      continue;
    }

    // Obtain a reference to the characteristic from a service.
    while (pClient->isConnected() && pRemoteCharacteristic == nullptr)
    {
      pRemoteCharacteristic = pRemoteService->getCharacteristic(uuid_pairing_characteristic);
      delay(50);
    }

    if (pRemoteCharacteristic == nullptr) {
      if (debug) Serial.print("Failed to find characteristic UUID: ");
      if (debug) Serial.println(uuid_pairing_characteristic.toString().c_str());
      pClient->disconnect();
      continue;
    }

    if (pair())
    {
      pNuki = &device;
      scan_results.clear();
      connect_user_specific();
      return pClient;
    }
    else
    {
      pClient->disconnect();
    }
    delay(50);
  }

  scan_results.clear();
  return nullptr;
}

/**
 * BLE pairing
 */
bool BLEUlmernest::pair ()
{
  // Initial pairing state: idle
  current_state = (int)pairing_state::idle;

  // get public key
  const uint8_t* public_key = pBund->get_public_key();

  // set BLE indicated callback to recieve the public key of the Nuki SL
  pRemoteCharacteristic->registerForNotify(pBote->notifyCallback_receive_pk, false);
  delay(50);

  // update pairing state to wait for the public key callback
  current_state = (int)pairing_state::get_pk;
  // send request for public key of Nuki SL
  pBote->command((uint8_t)cmd::request_data).command((uint8_t)cmd::public_key).send(pRemoteCharacteristic, true);

  // loop until pairing is either done or the connection expired
  // possible improvements:
  //   1) Run BLE loops like this as one or multiple tasks
  //   2) Refactor to use loop() in main.cpp and continously check BLE states, so other routines are not blocked
  while (pClient->isConnected() && current_state != (int)pairing_state::done)
  {
    switch (current_state)
    {
    case (int)pairing_state::get_pk:
      // Waiting for SmartLock public key
      break;

    case (int)pairing_state::send_pk:
      {
        // generate keypair
        if (debug) Serial.println("  * generate key pair");
        pBund->generate_keypair();

        // set BLE indicated callback to recieve challenge bytes
        pRemoteCharacteristic->registerForNotify(pBote->notifyCallback_challenge, false);
        delay(50);

        if (debug) Serial.print("  * sending client public key ");
        // print_hex(public_key, KEY_LENGTH);

        // update pairing state to calculate shared secret
        current_state = (int)pairing_state::calc_shared_secret;
        // send public key to Nuki SL
        pBote->command((uint8_t)cmd::public_key).data(public_key, KEY_LENGTH).send(pRemoteCharacteristic, true);
        break;
      }

    case (int)pairing_state::calc_shared_secret:
      break;

    case (int)pairing_state::challenge:
      {
        // calculate authentication with Nuki SL
        uint8_t pPayload[KEY_LENGTH];
        uint8_t pHash[KEY_LENGTH];
        const uint8_t* sl_public_key = pBund->get_sl_public_key();
        pBote->get_antwort(pPayload);
        // print_hex(pPayload, 32);
        std::vector<uint8_t> r;
        r.insert(r.end(), public_key, public_key + KEY_LENGTH);
        r.insert(r.end(), sl_public_key, sl_public_key + KEY_LENGTH);
        r.insert(r.end(), pPayload, pPayload + KEY_LENGTH);
        print_hex(r.data(), r.size());
        pBund->calc_auth(r.data(), r.size(), pHash);

        // store address and credentials
        char* addr_to_store = (char*)pClient->getPeerAddress().toString().c_str();
        pBund->store_address(addr_to_store);
        pBund->store_keys();

        // set BLE indicated callback to challenge authentication
        pRemoteCharacteristic->registerForNotify(pBote->notifyCallback_challenge_auth, false);
        delay(50);

        if (debug) Serial.println("  * Sending client Authorization Authenticator");
        current_state = (int)pairing_state::idle;
        pBote->command((uint8_t)cmd::authorization_authenticator).data(pHash, sizeof pHash).send(pRemoteCharacteristic, true);
        break;
      }

    case (int)pairing_state::challenge_auth:
      {
        /**
         * type - app = 0             // uint8
         * id                         // uint32
         * name                       // uint8[32]
         * Nonce n client             // uint8[32]
         * Nonce n_k - challenge msg  // uint8[32]
         * */
        uint8_t pPayload[KEY_LENGTH];
        uint8_t pHash[KEY_LENGTH];
        pBote->get_antwort(pPayload);
        uint8_t is_app = 0;
        uint32_t app_id = 3000000;
        uint8_t app_id_uint8_le[4] = { app_id, app_id >> 8, app_id >> 16, app_id >> 24 };
        uint8_t app_name[KEY_LENGTH] = "ulmernest-esp32";
        uint8_t nonce[KEY_LENGTH];
        esp_fill_random(nonce, KEY_LENGTH);
        std::vector<uint8_t> r;
        r.push_back(is_app);
        r.insert(r.end(), app_id_uint8_le, app_id_uint8_le + 4);
        r.insert(r.end(), app_name, app_name + KEY_LENGTH);
        r.insert(r.end(), nonce, nonce + KEY_LENGTH);
        r.insert(r.end(), pPayload, pPayload + KEY_LENGTH);
        print_hex(r.data(), r.size());
        pBund->calc_auth(r.data(), r.size(), pHash);

        // set BLE indicated callback to recieve auth id
        pRemoteCharacteristic->registerForNotify(pBote->notifyCallback_get_auth_id, false);
        delay(50);

        if (debug) Serial.println("  * Sending client Authorization Data");
        current_state = (int)pairing_state::idle;
        pBote->command((uint8_t)cmd::authorization_data);
        pBote->data(pHash, KEY_LENGTH).data(&is_app, 1).data(app_id_uint8_le, 4).data(app_name, KEY_LENGTH);
        pBote->data(nonce, KEY_LENGTH);
        pBote->send(pRemoteCharacteristic, true);
        break;
      }

    case (int)pairing_state::conf_auth_id:
      {
        // 84 = 32 + 4 + 16 + 32
        uint8_t msg[84];
        pBote->get_antwort(msg);

        uint8_t auth[32];
        std::copy(msg, msg + 32, auth);
        print_hex(auth, 32);
        uint8_t authorization_id[4];
        std::copy(msg + 32, msg + 36, authorization_id);
        uint32_t authorization_id_32 = msg[32] | (msg[33] << 8) | (msg[34] << 16) | (msg[35] << 24);
        if (debug) Serial.println(authorization_id_32);
        uint8_t uuid[16];
        std::copy(msg + 36, msg + 52, uuid);
        if (debug) Serial.print("user specific uuid: ");
        print_hex(uuid, 16);
        uint8_t nonce[32];
        std::copy(msg + 52, msg + 84, nonce);
        print_hex(nonce, 32);

        pBund->set_auth_id(authorization_id_32);
        pBund->set_device_bound_uuid(uuid);

        std::vector<uint8_t> r;
        r.assign(authorization_id, authorization_id + 4);
        r.insert(r.end(), nonce, nonce + 32);
        uint8_t hash[KEY_LENGTH];
        pBund->calc_auth(r.data(), r.size(), hash);

        pRemoteCharacteristic->registerForNotify(pBote->notifyCallback_confirm_auth_id, false);
        delay(50);

        if (debug) Serial.println("  * Sending client Authorization confirmation");
        BLEUlmernest::current_state = (int)pairing_state::idle;
        pBote->reset();
        pBote->command((uint8_t)cmd::authorization_id_confirmation).data(hash, KEY_LENGTH).data(authorization_id, 4).send(pRemoteCharacteristic, true);

        break;
      }

    case (int)pairing_state::failed:
      return false;
      break;

    default:
      break;
    }
    delay(10);
  }
  esp_task_wdt_reset();

  // pairing complete
  if (debug)
  {
    Serial.println(" - pairing success");
  }

  return true;
}


/******************
 * Getter, Setter
 ******************/
Bote* BLEUlmernest::get_Bote ()
{
  return pBote;
}

Schluesselbund* BLEUlmernest::get_Bund ()
{
  return pBund;
}

void BLEUlmernest::set_current_state (int pairing_state)
{
  current_state = (int)pairing_state;
}

int BLEUlmernest::get_current_state ()
{
  return current_state;
}

BLERemoteCharacteristic* BLEUlmernest::get_RemoteCharacteristic ()
{
  return pRemoteCharacteristic;
}

KeyturnerStates BLEUlmernest::get_keytuerner_states ()
{
  return keyturner_states;
}


/******************
 * Public Methods
 ******************/

// Method to initialize all reuired elements to operate the BLE functionality.
bool BLEUlmernest::init (std::__cxx11::string device_name)
{
  // Call from BLEDevice inherited init() function
  BLEDevice::init(device_name);

  // Create Schluesselbund Object and initialize
  pBund = new Schluesselbund();
  pBund->init(device_name);
  // Create Bote Object
  pBote = new Bote(pBund);
  // Try to initially connect to Nuki SL
  pClient = initial_connect();

  // If initial_connect() was successful return true, else false
  if (pClient == nullptr) return false;
  else return true;
}

/**
 * (re)connect to BLE user specific
 */
int BLEUlmernest::connect_user_specific ()
{
  if (pClient == nullptr)
  {
    if (debug) Serial.println(" ! connect_user_specific: pClient is nullptr");
    return -1;
  }

  if (!pClient->isConnected())
  {
    if (debug) Serial.println(" - Trying to connect...");
    if (debug) Serial.println(pNuki->toString().c_str());
    pClient->connect(pNuki);
    if (pClient->isConnected())
    {
      if (debug) Serial.print(" - Is Connected to ");
      if (debug) Serial.println(pNuki->getName().c_str());
    }
    else
    {
      if (debug) Serial.println(" - Could not connect!");
      return -1;
    }
    delay(50);
  }

  // while (pClient->isConnected() && pUserService == nullptr)
  {
    pUserService = pClient->getService(uuid_service);
    delay(50);
  }
  if (pUserService == nullptr)
  {
    if (debug)
    {
      Serial.print("could not get service: ");
      Serial.println(uuid_user_specific_dio_characteristic.toString().c_str());
    }
    return -1;
  }

  // while (pClient->isConnected() && pUSDIO == nullptr)
  {
    pUSDIO = pUserService->getCharacteristic(uuid_user_specific_dio_characteristic);
    delay(50);
  }
  if (pUSDIO == nullptr)
  {
    if (debug)
    {
      Serial.print("could not get characteristic: ");
      Serial.println(uuid_user_specific_dio_characteristic.toString().c_str());
    }
    return -1;
  }
  return 0;
}

void BLEUlmernest::write (uint8_t* data, size_t len, bool response = false)
{
  if (pRemoteCharacteristic->canWrite())
  {
    if (debug) Serial.print("  * sending client public key: ");
    print_hex(data, len);
    pRemoteCharacteristic->writeValue(data, len, response);
  }
}

int BLEUlmernest::read_keyturner_state ()
{
  if (connect_user_specific() != 0) return -1;

  if (pUSDIO->canIndicate())
  {
    pUSDIO->registerForNotify(pBote->notifyCallback_crypto, false);
    delay(50);
  }
  else
  {
    return -1;
  }

  if (debug) Serial.println("Lock state: ");
  current_state = (int)transmission::t_await_rx;
  if (pUSDIO->canWrite()) pBote->command((uint8_t)cmd::request_data).command((uint8_t)cmd::keyturn_states).send_cipher(pUSDIO, pBund);
  else
  {
    // handle error
    return -1;
  }

  while (pClient->isConnected() && current_state != (int)transmission::t_done)
  {
    switch (current_state)
    {
    case (int)transmission::t_rx_success:
    {
      std::vector<uint8_t> a = pBote->get_antwort();
      if (debug) Serial.print("decrypted data: ");
      print_hex(a.data(), a.size());

      // 0-3 authorization id, 4-5 command
      size_t i = 6, j = 0;
      keyturner_states.nuki_state                         = a.data()[i++];
      keyturner_states.lock_state                         = a.data()[i++];
      keyturner_states.trigger                            = a.data()[i++];
      while (j < 7) keyturner_states.current_time[j++]    = a.data()[i++];
      keyturner_states.timezone_offset                    = a.data()[i++] | a.data()[i++] << 8;
      keyturner_states.critical_battery_state             = a.data()[i++];
      keyturner_states.config_update_count                = a.data()[i++];
      keyturner_states.lock_n_go_timer                    = a.data()[i++];
      keyturner_states.last_lock_action                   = a.data()[i++];
      keyturner_states.last_lock_action_trigger           = a.data()[i++];
      keyturner_states.last_lock_action_completion_status = a.data()[i++];
      keyturner_states.door_sensor_state                  = a.data()[i++];
      keyturner_states.nightmode_active                   = a.data()[i++] | a.data()[i++] << 8;
      keyturner_states.accessory_battery_state            = a.data()[i++];

      current_state = (int)transmission::t_done;
      break;
    }

    case (int)transmission::t_failed:
      return -1;
      break;

    default:
      break;
    }
  }
  esp_task_wdt_reset();
  return 0;
}

/**
 * Requests Nuki SL to do a specific lock action.
 * Currently can block code execution in main.cpp loop() for up to serveral seconds,
 * because of the time it can take for the Nuki SL to execute a lock action.
 */
int BLEUlmernest::lock_action (uint8_t action)
{
  int locking_state = 0;
  // return -1 if connection failed
  if (connect_user_specific() != 0) return -1;

  // read keyturner states and get current nuki state (mode)
  read_keyturner_state();
  unsigned char nuki_state = get_keytuerner_states().nuki_state;

  // check if Nuki SL is in door mode
  // return -1 if not
  if (nuki_state != (unsigned char)nuki_states::door_mode)
  {
    if (debug)
    {
      Serial.print(" ! lock_action: nuki state is not door mode - ");
      Serial.println(nuki_state, HEX);
    }
    return -1;
  }

  pUSDIO->registerForNotify(pBote->notifyCallback_req_challenge, false);
  delay(50);

  if (debug) Serial.println("Request Challenge: ");
  current_state = (int)transmission::t_idle;
  pBote->command((uint8_t)cmd::request_data).command((uint8_t)cmd::req_challenge).send_cipher(pUSDIO, pBund);

  while (pClient->isConnected() && current_state != (int)transmission::t_done)
  {
    switch (current_state)
    {
    case (int)transmission::t_challenge:
    {
      std::vector<uint8_t> a = pBote->get_antwort();
      if (debug) Serial.print("decrypted challenge: ");
      print_hex(a.data() + 6, 32);
      uint8_t nonce[KEY_LENGTH];
      std::copy(a.data() + 6, a.data() + 6 + KEY_LENGTH, nonce);

      std::vector<uint8_t> d;
      d.push_back(action);
      d.push_back(0x00);
      d.insert(d.end(), 4, 0);

      if (debug) Serial.println("  send lock command");
      pUSDIO->registerForNotify(pBote->notifyCallback_crypto, false);
      delay(50);

      current_state = (int)transmission::t_idle;
      pBote->command((uint8_t)cmd::lock_action);
      pBote->data(d.data(), d.size()).data(nonce, KEY_LENGTH).send_cipher(pUSDIO, pBund);
      break;
    }

    case (int)transmission::t_rx_success:
    {
      std::vector<uint8_t> a = pBote->get_antwort();

      // check for status command and code 'COMPLETE'
      if (a.data()[4] == 0x0E && a.data()[5] == 0x00 && a.data()[6] == 0x00)
      {
        if (debug) Serial.println(" + locking done!");
        current_state = (int)transmission::t_done;
      }
      // check for Nuki Error command
      else if (a.data()[4] == 0x12 && a.data()[5] == 0x00)
      {
        locking_state = a.data()[6];
        current_state = (int)transmission::t_failed;
      }
      // check for locking state
      else
      {
        locking_state = a.data()[7];
        current_state = (int)transmission::t_idle;
      }
      break;
    }

    case (int)transmission::t_failed:
      if (debug) Serial.println("lock action: failed, something went wrong!");
      current_state = (int)transmission::t_done;
      break;

    default:
      break;
    }
    delay(10);
  }
  esp_task_wdt_reset();

  return locking_state;
}

/**
 * Request log entries from Nuki SL
 */
std::vector<std::vector<uint8_t>> BLEUlmernest::req_log_entries (uint32_t start_index, uint16_t count, uint16_t &out_logs_available, uint8_t order)
{
  // vector variable to store logs for return
  std::vector<std::vector<uint8_t>> logs;
  uint16_t logs_available = 0;

  // return empty vector if there is no successful connection
  if (connect_user_specific() != 0) return logs;

  // register inication callback for requested challenge
  pUSDIO->registerForNotify(pBote->notifyCallback_req_challenge, false);
  delay(50);

  if (debug) Serial.println("Request Challenge: ");
  current_state = (int)transmission::t_idle;
  pBote->command((uint8_t)cmd::request_data).command((uint8_t)cmd::req_challenge).send_cipher(pUSDIO, pBund);

  while (pClient->isConnected() && current_state != (int)transmission::t_done)
  {
    switch (current_state)
    {
      // Nuki SL has responded with challenge
    case (int)transmission::t_challenge:
      {
        std::vector<uint8_t> a = pBote->get_antwort();
        if (debug) Serial.print("decrypted challenge: ");
        print_hex(a.data() + 6, 32);
        uint8_t nonce[KEY_LENGTH];
        std::copy(a.data() + 6, a.data() + 6 + KEY_LENGTH, nonce);

        const unsigned char req[8] = {
          start_index, start_index >> 8, start_index >> 16, start_index >> 24,
          count, count >> 8,
          order/** sort order 0x00 asc, 0x01 desc */,
          out_logs_available == 0 ? 0x01 : 0x00 };
        std::vector<uint8_t> d;
        d.insert(d.end(), req, req + 8);

        const unsigned char pin[2] = { 0x00, 0x00 }; // pin 0:0:0:0

        if (debug) Serial.println("  send request log entries");
        pUSDIO->registerForNotify(pBote->notifyCallback_crypto, false);
        delay(50);

        current_state = (int)transmission::t_idle;
        pBote->command((uint8_t)cmd::request_log_entries);
        pBote->data(d.data(), d.size()).data(nonce, KEY_LENGTH).data(pin, 2).send_cipher(pUSDIO, pBund);

        esp_task_wdt_reset();
        break;
      }

      // Handle different responses from Nuki SL
    case (int)transmission::t_rx_success:
      {
        // get recieved data
        std::vector<uint8_t> a = pBote->get_antwort();

        // check for status command and code 'COMPLETE'
        if (a.data()[4] == 0x0E && a.data()[5] == 0x00 && a.data()[6] == 0x00)
        {
          if (debug) Serial.println(" + req_log_entries: done!");
          // set state flag to t_done when status is COMPLETE
          current_state = (int)transmission::t_done;
        }
        // check for Nuki Error command
        else if (a.data()[4] == 0x12 && a.data()[5] == 0x00)
        {
          if (debug)
          {
            Serial.print(" ! req_log_entries - nuki error: ");
            print_hex(a.data(), a.size());
            // set state flag to t_failed if Error command was recieved
            current_state = (int)transmission::t_failed;
          }
        }
        // check for 'Log Entry Count' response
        else if (a.data()[4] == 0x33 && a.data()[5] == 0x00)
        {
          // get number of available logs
          logs_available = a.data()[7] | a.data()[8] << 8;
          // set out variable to the number of available logs
          out_logs_available = logs_available;
          if (debug)
          {
            Serial.print(" + logs available ");
            Serial.println(out_logs_available);
          }
          // wait for next indication
          current_state = (int)transmission::t_idle;
        }
        // check for 'Log Entry' response
        else if (a.data()[4] == 0x32 && a.data()[5] == 0x00)
        {
          if (debug > 1)
          {
            delay(50);
            Serial.print("id: ");
            Serial.println(a.data()[6] | a.data()[7] << 8 | a.data()[8] << 16 | a.data()[9] << 24);
            uint16_t year = a.data()[10] | a.data()[11] << 8;
            uint8_t month = a.data()[12], day = a.data()[13], hour = a.data()[14], min = a.data()[15], sec = a.data()[16];
            Serial.printf("date time: %d-%d-%d %d:%d:%d\n", year, month, day, hour, min, sec);
            Serial.print("type: ");
            Serial.println(a.data()[53]);
          }

          // add log to return variable
          logs.push_back(a);
          // wait for next indication
          current_state = (int)transmission::t_idle;
        }

        esp_task_wdt_reset();
        break;
      }

    case (int)transmission::t_failed:
      // error handling
      current_state = (int)transmission::t_done;
      break;

    default:
      break;
    }
    delay(5);
  }
  esp_task_wdt_reset();
  return logs;
}
