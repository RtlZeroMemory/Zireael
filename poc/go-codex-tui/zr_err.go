package main

import "fmt"

func zrErrString(rc int32) string {
	switch rc {
	case zrOK:
		return "ZR_OK"
	case zrErrInvalidArg:
		return "ZR_ERR_INVALID_ARGUMENT"
	case zrErrOOM:
		return "ZR_ERR_OOM"
	case zrErrLimit:
		return "ZR_ERR_LIMIT"
	case zrErrUnsupported:
		return "ZR_ERR_UNSUPPORTED"
	case zrErrFormat:
		return "ZR_ERR_FORMAT"
	case zrErrPlatform:
		return "ZR_ERR_PLATFORM"
	default:
		return fmt.Sprintf("ZR_ERR_%d", rc)
	}
}
