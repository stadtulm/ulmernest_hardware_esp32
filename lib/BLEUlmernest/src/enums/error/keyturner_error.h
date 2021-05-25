#ifndef KEYTURNER_ERROR_H
#define KEYTURNER_ERROR_H

enum class keyturn_error : unsigned char
{
  K_ERROR_NOT_AUTHORIZED = 0x20, // Returned if the provided authorization id is invalid or the payload could not be decrypted using the shared key for this authorization id
  K_ERROR_BAD_PIN = 0x21, // Returned if the provided pin does not match the stored one.
  K_ERROR_BAD_NONCE = 0x22, // Returned if the provided nonce does not match the last stored one of this authorization id or has already been used before.
  K_ERROR_BAD_PARAMETER = 0x23, // Returned if a provided parameter is outside of its valid range.
  K_ERROR_INVALID_AUTH_ID = 0x24, // Returned if the desired authorization id could not be deleted because it does not exist.
  K_ERROR_DISABLED = 0x25, // Returned if the provided authorization id is currently disabled.
  K_ERROR_REMOTE_NOT_ALLOWED = 0x26, // Returned if the request has been forwarded by the Nuki Bridge and the provided authorization id has not been granted remote access.
  K_ERROR_TIME_NOT_ALLOWED = 0x27, // Returned if the provided authorization id has not been granted access at the current time.
  K_ERROR_TOO_MANY_PIN_ATTEMPTS = 0x28, // Returned if an invalid pin has been provided too often
  K_ERROR_TOO_MANY_ENTRIES = 0x29, // Returned if no more entries can be stored
  K_ERROR_CODE_ALREADY_EXISTS = 0x2A, // Returned if a Keypad Code should be added but the given code already exists.
  K_ERROR_CODE_INVALID = 0x2B, // Returned if a Keypad Code that has been entered is invalid.
  K_ERROR_CODE_INVALID_TIMEOUT_1 = 0x2C, // Returned if an invalid pin has been provided multiple times.
  K_ERROR_CODE_INVALID_TIMEOUT_2 = 0x2D, // Returned if an invalid pin has been provided multiple times.
  K_ERROR_CODE_INVALID_TIMEOUT_3 = 0x2E, // Returned if an invalid pin has been provided multiple times.
  K_ERROR_AUTO_UNLOCK_TOO_RECENT = 0x40, // Returned on an incoming auto unlock request and if a lock action has already been executed within short time.
  K_ERROR_POSITION_UNKNOWN = 0x41, // Returned on an incoming unlock request if the request has been forwarded by the Nuki Bridge and the Smart Lock is unsure about its actual lock position.
  K_ERROR_MOTOR_BLOCKED = 0x42, // Returned if the motor blocks.
  K_ERROR_CLUTCH_FAILURE = 0x43, // Returned if there is a problem with the clutch during motor movement.
  K_ERROR_MOTOR_TIMEOUT = 0x44, // Returned if the motor moves for a given period of time but did not block.
  K_ERROR_BUSY = 0x45, // Returned on any lock action via bluetooth if there is already a lock action processing.
  K_ERROR_CANCELED = 0x46, // Returned on any lock action or during calibration if the user canceled the motor movement by pressing the button
  K_ERROR_NOT_CALIBRATED = 0x47, // Returned on any lock action if the Smart Lock has not yet been calibrated
  K_ERROR_MOTOR_POSITION_LIMIT = 0x48, // Returned during calibration if the internal position database is not able to store any more values
  K_ERROR_MOTOR_LOW_VOLTAGE = 0x49, // Returned if the motor blocks because of low voltage.
  K_ERROR_MOTOR_POWER_FAILURE = 0x4A, // Returned if the power drain during motor movement is zero
  K_ERROR_CLUTCH_POWER_FAILURE = 0x4B, // Returned if the power drain during clutch movement is zero
  K_ERROR_VOLTAGE_TOO_LOW = 0x4C, // Returned on a calibration request if the battery voltage is too low and a calibration will therefore not be started
  K_ERROR_FIRMWARE_UPDATE_NEEDED = 0x4D // Returned during any motor action if a firmware update is mandatory
};

#endif