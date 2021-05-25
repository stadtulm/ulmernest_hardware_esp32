#ifndef STATUS_CODE_H
#define STATUS_CODE_H

enum class status_codes : char
{
 	COMPLETE = 0x00, // Returned to signal the successful completion of a command
  ACCEPTED = 0x01 // Returned to signal that a command has been accepted but the completion status will be signaled later.
};

#endif // STATUS_CODE_H