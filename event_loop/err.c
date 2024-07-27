#include "err.h"

char *err_code_2_str(enum ErrorReason code) {
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
    case ErrParsingIsIncomplete:
      return "ErrParsingIsIncomplete";
    default:
      return "ErrUnknownError";
  }
}