////////////////////////////////////////////////////////////////////////////////////////////////////////
//	Copyright (C) 2002	Kuihua Tech Inc. All rights reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////////
//	Author	:	Jackey
//	Date	:	2003.05.03
//	File	:	HTTPDefine.h
//	Contains:	Define HTTP Protocol Macro.....
//	History	:
//		1.0	:	2003.05.03 - First version...
//	Mailto	:	omega@kuihua.net (Bug Report or Comments)
////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

//
// HTTP Versions Define...
//
const long HTTP_LINE_START		= __LINE__ + 3;
enum
{
	http10Version				= __LINE__ - HTTP_LINE_START,
	http11Version				= __LINE__ - HTTP_LINE_START,
	httpNumVersions				= __LINE__ - HTTP_LINE_START,
	httpIllegalVersion			= httpNumVersions,
};
typedef	UINT KT_HTTPVersion;

//
// HTTP Base Method Define...
//
enum
{
	httpGetMethod				= 0,
	httpHeadMethod				= 1,
	httpPostMethod				= 2,
	httpOptionsMethod			= 3,
	httpPutMethod				= 4,
	httpDeleteMethod			= 5,
	httpTraceMethod				= 6,
	httpConnectMethod			= 7,

	httpLoginMethod				= 8,
	httpUpgradeMethod			= 9,

	httpSeekMethod				= 10,
	httpPauseMethod				= 11,
	httpContinueMethod			= 12,
	httpOnLineMethod			= 13,

	httpExtendMethod			= 14,
	httpRequestMethod			= 15,

	httpNumMethods				= 16,
	httpIllegalMethod			= 16,
};
typedef	UINT KT_HTTPMethod;

//
// HTTP Headers Define...
//
enum
{
	//
	// HTTP VIP Headers...
	//
  	httpConnectionHeader		= 0,	// general header
  	httpDateHeader				= 1,	// general header
  	httpAuthorizationHeader     = 2,	// request header
  	httpIfModifiedSinceHeader	= 3,	// request header
  	httpServerHeader			= 4,	// response header
  	httpWWWAuthenticateHeader	= 5,	// response header
  	httpExpiresHeader			= 6,	// entity header
  	httpLastModifiedHeader		= 7,	// entity header
  	httpNumVIPHeaders			= 8,
	//
  	// Other general HTTP headers...
	//
  	httpCacheControlHeader		= 8,
  	httpPragmaHeader			= 9,
  	httpTrailerHeader			= 10,
  	httpTransferEncodingHeader	= 11,
  	httpUpgradeHeader			= 12,
  	httpViaHeader				= 13,
  	httpWarningHeader			= 14,
	//
  	// Other request headers...
	//
  	httpAcceptHeader			= 15,
  	httpAcceptCharsetHeader		= 16,
  	httpAcceptEncodingHeader	= 17,
  	httpAcceptLanguageHeader	= 18,
  	httpExpectHeader			= 19,
  	httpFromHeader				= 20,
	httpHostHeader				= 21,
	httpIfMatchHeader			= 22,
	httpIfNoneMatchHeader		= 23,
	httpIfRangeHeader			= 24,
	httpIfUnmodifiedSinceHeader	= 25,
	httpMaxForwardsHeader		= 26,
	httpProxyAuthorizationHeader= 27,
	httpRangeHeader				= 28,
	httpRefererHeader			= 29,
	httpTEHeader				= 30,
	httpUserAgentHeader			= 31,
	//
	// Other response headers
	//
	httpAcceptRangesHeader		= 32,
	httpAgeHeader				= 33,
	httpETagHeader				= 34,
	httpLocationHeader			= 35,
	httpProxyAuthenticateHeader	= 36,
	httpRetryAfterHeader		= 37,
	httpVaryHeader				= 38,
	httpPublicHeader			= 39,
	httpAllowHeader				= 40,
	//
	// Other entity headers
	//
	httpContentEncodingHeader	= 41,
	httpContentLanguageHeader	= 42,
	httpContentLengthHeader		= 43,
	httpContentLocationHeader	= 44,
	httpContentMD5Header		= 45,
	httpContentRangeHeader		= 46,
	httpContentTypeHeader		= 47,

	httpKeepAliveHeader			= 48,

	//
	// Additional headers
	//
	httpExtensionHeaders		= 49,
	httpSetCookieHeader			= 49,
	httpCookieHeader			= 50,

	httpXUpgradeLengthHeader	= 51,
	httpXContentValueHeader		= 52,
	httpXAbsURIHeader			= 53,
	httpXURIHeader				= 54,
	httpXQueryStringHeader		= 55,
	httpXErrCodeHeader			= 56,
	httpXMethodHeader			= 57,
	httpXEncryptHeader			= 58,
	httpRecordGroup				= 59,
	httpGetSub					= 60,
	httpParentGuid				= 61,
	httpSelfGuid				= 62,
	httpKMSType					= 63,
	httpKMSName					= 64,
	httpKMSArea					= 65,
	httpWebType					= 66,
	httpCenterIP				= 67,
	httpApachPort				= 68,
	httpServerTime				= 69,

	httpXPIndex					= 70,
	httpXDCount					= 71,
	httpXDTag					= 72,
	httpXGUID					= 73,

	httpXRTMPPort				= 74,

	httpNumHeaders				= 75,
	httpIllegalHeader			= 75,
};
typedef	UINT KT_HTTPHeader;

//
// HTTP Status codes...
//
enum
{
	httpContinue					= 0,		//100
	httpSwitchingProtocols			= 1,		//101

	httpOK							= 2,		//200
	httpCreated						= 3,		//201
	httpAccepted 					= 4,		//202
	httpNonAuthoritativeInformation = 5,		//203
	httpNoContent					= 6,		//204
	httpResetContent				= 7,		//205
	httpPartialContent				= 8,		//206

	httpMultipleChoices				= 9,		//300
	httpMovedPermanently			= 10,		//301
	httpFound						= 11,		//302
	httpSeeOther					= 12,		//303
	httpNotModified					= 13,		//304
	httpUseProxy					= 14,		//305
	httpTemporaryRedirect			= 15,		//307
	httpBadRequest					= 16,		//400
	httpUnAuthorized				= 17,		//401
	httpPaymentRequired				= 18,		//402
	httpForbidden					= 19,		//403
	httpNotFound					= 20,		//404
	httpMethodNotAllowed			= 21,		//405
	httpNotAcceptable				= 22,		//406
	httpProxyAuthenticationRequired	= 23,		//407
	httpRequestTimeout				= 24,		//408
	httpConflict					= 25,		//409
	httpGone						= 26,		//410
	httpLengthRequired				= 27,		//411
	httpPreconditionFailed			= 28,		//412
	httpRequestEntityTooLarge		= 29,		//413
	httpRequestURITooLarge			= 30,		//414
	httpUnsupportedMediaType		= 31,		//415
	httpRequestRangeNotSatisfiable	= 32,		//416
	httpExpectationFailed			= 33,		//417
	httpInternalServerError			= 34,		//500
	httpNotImplemented				= 35,		//501
	httpBadGateway					= 36,		//502
	httpServiceUnavailable			= 37,		//503
	httpGatewayTimeout				= 38,		//504
	httpHTTPVersionNotSupported		= 39,		//505

	httpNumStatusCodes				= 40
};
typedef UINT KT_HTTPStatusIndex;
