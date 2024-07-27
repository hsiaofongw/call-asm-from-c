#include "err.h"

char *err_get_error_code_str(enum ErrorReason code) {
  switch (code) {
    case ErrAllocaFailed:
      return "ErrAllocaFailed";
    case ErrNonSupportedField:
      return "ErrNonSupportedField";
    case ErrNonSupportedMsgType:
      return "ErrNonSupportedMsgType";
    case ErrSizeTooLarge:
      return "ErrSizeTooLarge";
    case ErrTooSmallBuffer:
      return "ErrTooSmallBuffer";
    case ErrBodyTooLarge:
      return "ErrBodyTooLarge";
    case ErrSerializeCtxBusy:
      return "ErrBodyTooLarge";
    case ErrNoEnoughCapacity:
      return "ErrNoEnoughCapacity";
    case ErrPacketTooBig:
      return "ErrPacketTooBig";
    case ErrMagicWordsMisMatch:
      return "ErrMagicWordsMisMatch";
    case ErrNoDataToParse:
      return "ErrNoDataToParse";
    case ErrNeedMore:
      return "ErrNeedMore";
    case ErrInvalidHeaderValue:
      return "ErrInvalidHeaderValue";
    case ErrExtractParsedPacketFirst:
      return "ErrExtractParsedPacketFirst";
    default:
      return "ErrUnknownError";
  }
}