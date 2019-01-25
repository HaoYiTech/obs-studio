////////////////////////////////////////////////////////////////////////////////////////////////////////
//	Copyright (C) 2002	Kuihua Tech Inc. All rights reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////////
//	Author	:	Jackey
//	Date	:	2003.05.03
//	File	:	HTTPProtocol.h
//	Contains:	Define HTTP Protocol Class.....
//	History	:
//		1.0	:	2003.05.03 - First version...
//	Mailto	:	omega@263.net (Bug Report or Comments)
////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "HTTPDefine.h"
#include "StrPtrLen.h"

class HTTPProtocol
{
public:
	//
	// Methods defined in HTTPDefine.h
	//
  	static UINT         GetMethod(const StrPtrLen & inMethodStr);
  	static StrPtrLen &	GetMethodString(KT_HTTPMethod inMethod) { return sMethods[inMethod]; }
  	//
	// Headers defined in HTTPDefine.h
	//
  	static UINT			GetHeader(const StrPtrLen & inHeaderStr);
  	static StrPtrLen &	GetHeaderString(KT_HTTPHeader inHeader) { return sHeaders[inHeader]; }
  	//
	// Status codes defined in HTTPDefine.h
	//
	static StrPtrLen & 	GetStatusCodeString(KT_HTTPStatusIndex inStat)	{ return sStatusCodeStrings[inStat]; }
	static SInt32       GetStatusCode(KT_HTTPStatusIndex inStat)		{ return sStatusCodes[inStat]; }
	static StrPtrLen &	GetStatusCodeAsString(KT_HTTPStatusIndex inStat) { return sStatusCodeAsStrings[inStat]; }
	static SInt32		GetStatusIndex(UInt32 inCode);
  	//
	// Versions defined in HTTPDefine.h
	//
  	static UINT			GetVersion(StrPtrLen & versionStr);
  	static StrPtrLen &	GetVersionString(KT_HTTPVersion version) { return sVersionStrings[version]; }
private:
  	static StrPtrLen	sMethods[];
  	static StrPtrLen	sHeaders[];
  	static StrPtrLen	sStatusCodeStrings[];
  	static StrPtrLen	sStatusCodeAsStrings[];
  	static SInt32		sStatusCodes[];
  	static StrPtrLen	sVersionStrings[];
	static StrPtrLen	sUserAgents[];
};
