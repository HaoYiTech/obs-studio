
#include "HYDefine.h"
#include "HTTPProtocol.h"

StrPtrLen HTTPProtocol::sMethods[] =
{
	StrPtrLen("GET"),
  	StrPtrLen("HEAD"),
  	StrPtrLen("POST"),
  	StrPtrLen("OPTIONS"),
  	StrPtrLen("PUT"),
  	StrPtrLen("DELETE"),
  	StrPtrLen("TRACE"),
  	StrPtrLen("CONNECT"),

	StrPtrLen("LOGIN"),
	StrPtrLen("UPGRADE"),

	StrPtrLen("SEEK"),
	StrPtrLen("PAUSE"),
	StrPtrLen("CONTINUE"),
	StrPtrLen("ONLINE"),

	StrPtrLen("EXTEND"),
	StrPtrLen("REQUEST"),
};

UINT HTTPProtocol::GetMethod(const StrPtrLen & inMethodStr)
{
	//
	// 1.0 Check the input value is validate...
	KT_HTTPMethod theMethod = httpIllegalMethod;
	if( inMethodStr.Ptr == NULL || inMethodStr.Len <= 0 ) {
		MsgLogGM(-1);
		return theMethod;
	}
	ASSERT( inMethodStr.Ptr != NULL && inMethodStr.Len > 0 );
	//
	// 2.0 Find the other header immediatly...
	for (SInt32 x = 0; x < httpNumMethods; x++)
	{
		if( inMethodStr.EqualIgnoreCase(sMethods[x].Ptr, sMethods[x].Len) )
			return x;
	}
	MsgLogGM(-1);
	return httpIllegalMethod;
}

StrPtrLen HTTPProtocol::sHeaders[] =
{
  	StrPtrLen("Connection"),
  	StrPtrLen("Date"),
  	StrPtrLen("Authorization"),
  	StrPtrLen("If-Modified-Since"),
  	StrPtrLen("Server"),
  	StrPtrLen("WWW-Authenticate"),
  	StrPtrLen("Expires"),
  	StrPtrLen("Last-Modified"),//8

  	StrPtrLen("Cache-Control"),
  	StrPtrLen("Pragma"),
  	StrPtrLen("Trailer"),
  	StrPtrLen("Transfer-Encoding"),
  	StrPtrLen("Upgrade"),
  	StrPtrLen("Via"),
  	StrPtrLen("Warning"),//15

  	StrPtrLen("Accept"),
  	StrPtrLen("Accept-Charset"),
  	StrPtrLen("Accept-Encoding"),
  	StrPtrLen("Accept-Language"),
  	StrPtrLen("Expect"),
  	StrPtrLen("From"),
  	StrPtrLen("Host"),
  	StrPtrLen("If-Match"),
  	StrPtrLen("If-None-Match"),
  	StrPtrLen("If-Range"),
  	StrPtrLen("If-Unmodified-Since"),
  	StrPtrLen("Max-Forwards"),
  	StrPtrLen("Proxy-Authorization"),
  	StrPtrLen("Range"),
  	StrPtrLen("Referer"),
  	StrPtrLen("TE"),
  	StrPtrLen("User-Agent"),//32

  	StrPtrLen("Accept-Ranges"),
  	StrPtrLen("Age"),
  	StrPtrLen("ETag"),
  	StrPtrLen("Location"),
  	StrPtrLen("Proxy-Authenticate"),
  	StrPtrLen("Retry-After"),
  	StrPtrLen("Vary"),
	StrPtrLen("Public"),
	StrPtrLen("Allow"),//41

  	StrPtrLen("Content-Encoding"),
  	StrPtrLen("Content-Language"),
  	StrPtrLen("Content-Length"),
  	StrPtrLen("Content-Location"),
  	StrPtrLen("Content-MD5"),
  	StrPtrLen("Content-Range"),
  	StrPtrLen("Content-Type"),

	StrPtrLen("Keep-Alive"),
	StrPtrLen("Set-Cookie"),
	StrPtrLen("Cookie"),//51

	StrPtrLen("X-UpgradeLength"),
	StrPtrLen("X-ContentValue"),
	StrPtrLen("X-AbsURI"),
	StrPtrLen("X-URI"),
	StrPtrLen("X-QueryString"),
	StrPtrLen("X-ErrCode"),//57
	StrPtrLen("X-Method"),//58
	StrPtrLen("X-Encryp"),
	StrPtrLen("X-RecordGroup"),
	StrPtrLen("X-GetSub"),
	StrPtrLen("X-ParentGuid"),
	StrPtrLen("X-SelfGuid"),
	StrPtrLen("X-KMSType"),
	StrPtrLen("X-KMSName"),
	StrPtrLen("X-KMSArea"),
	StrPtrLen("X-WebType"),
	StrPtrLen("X-CenterIP"),
	StrPtrLen("X-ApachPort"),
	StrPtrLen("X-ServerTime"),
	StrPtrLen("X-PIndex"),
	StrPtrLen("X-XDCount"),
	StrPtrLen("X-DTag"),
	StrPtrLen("X-GUID"),
	StrPtrLen("X-RTMPPort")
};

UINT HTTPProtocol::GetHeader(const StrPtrLen & inHeaderStr)
{
	//
	// 1.0 Check the input value is validate...
	KT_HTTPHeader theHeader = httpIllegalHeader;
	if( inHeaderStr.Ptr == NULL || inHeaderStr.Len <= 0 ) {
		MsgLogGM(-1);
		return theHeader;
	}
	ASSERT( inHeaderStr.Ptr != NULL && inHeaderStr.Len > 0 );
	//
	// 4.0 Find the other header immediatly...
	for (SInt32 x = 0; x < httpNumHeaders; x++)
	{
		if (inHeaderStr.EqualIgnoreCase(sHeaders[x].Ptr, sHeaders[x].Len))
			return x;
	}
	MsgLogGM(-1);
	return httpIllegalHeader;
}

StrPtrLen HTTPProtocol::sStatusCodeStrings[] =
{
	StrPtrLen("Continue"),						//kContinue
	StrPtrLen("Switching Protocols"),			//kSwitchingProtocols
	StrPtrLen("OK"),							//kOK
	StrPtrLen("Created"),						//kCreated
	StrPtrLen("Accepted"),						//kAccepted
	StrPtrLen("Non Authoritative Information"),	//kNonAuthoritativeInformation
	StrPtrLen("No Content"),					//kNoContent
	StrPtrLen("Reset Content"),					//kResetContent
	StrPtrLen("Partial Content"),				//kPartialContent
	StrPtrLen("Multiple Choices"),				//kMultipleChoices
	StrPtrLen("Moved Permanently"),				//kMovedPermanently
	StrPtrLen("Found"),							//kFound
	StrPtrLen("See Other"),						//kSeeOther
	StrPtrLen("Not Modified"),					//kNotModified
	StrPtrLen("Use Proxy"),						//kUseProxy
	StrPtrLen("Temporary Redirect"),			//kTemporaryRedirect
	StrPtrLen("Bad Request"),					//kBadRequest
	StrPtrLen("Unauthorized"),					//kUnAuthorized
	StrPtrLen("Payment Required"),				//kPaymentRequired
	StrPtrLen("Forbidden"),						//kForbidden
	StrPtrLen("Not Found"),						//kNotFound
	StrPtrLen("Method Not Allowed"),			//kMethodNotAllowed
	StrPtrLen("Not Acceptable"),				//kNotAcceptable
	StrPtrLen("Proxy Authentication Required"),	//kProxyAuthenticationRequired
	StrPtrLen("Request Time-out"),				//kRequestTimeout
	StrPtrLen("Conflict"),						//kConflict
	StrPtrLen("Gone"),							//kGone
	StrPtrLen("Length Required"),				//kLengthRequired
	StrPtrLen("Precondition Failed"),			//kPreconditionFailed
	StrPtrLen("Request Entity Too Large"),		//kRequestEntityTooLarge
	StrPtrLen("Request-URI Too Large"),			//kRequestURITooLarge
	StrPtrLen("Unsupported Media Type"),		//kUnsupportedMediaType
	StrPtrLen("Request Range Not Satisfiable"),	//kRequestRangeNotSatisfiable
	StrPtrLen("Expectation Failed"),			//kExpectationFailed
	StrPtrLen("Internal Server Error"),			//kInternalServerError
	StrPtrLen("Not Implemented"),				//kNotImplemented
	StrPtrLen("Bad Gateway"),					//kBadGateway
	StrPtrLen("Service Unavailable"),			//kServiceUnavailable
	StrPtrLen("Gateway Timeout"),				//kGatewayTimeout
	StrPtrLen("HTTP Version not supported")		//kHTTPVersionNotSupported
};

SInt32 HTTPProtocol::sStatusCodes[] =
{
	100,            //kContinue
	101,            //kSwitchingProtocols
	200,            //kOK
	201,            //kCreated
	202,            //kAccepted
	203,            //kNonAuthoritativeInformation
	204,            //kNoContent
	205,            //kResetContent
	206,            //kPartialContent
	300,            //kMultipleChoices
	301,            //kMovedPermanently
	302,            //kFound
	303,            //kSeeOther
	304,            //kNotModified
	305,            //kUseProxy
	307,            //kTemporaryRedirect
	400,            //kBadRequest
	401,            //kUnAuthorized
	402,            //kPaymentRequired
	403,            //kForbidden
	404,            //kNotFound
	405,            //kMethodNotAllowed
	406,            //kNotAcceptable
	407,            //kProxyAuthenticationRequired
	408,            //kRequestTimeout
	409,            //kConflict
	410,            //kGone
	411,            //kLengthRequired
	412,            //kPreconditionFailed
	413,            //kRequestEntityTooLarge
	414,            //kRequestURITooLarge
	415,            //kUnsupportedMediaType
	416,            //kRequestRangeNotSatisfiable
	417,            //kExpectationFailed
	500,            //kInternalServerError
	501,            //kNotImplemented
	502,            //kBadGateway
	503,            //kServiceUnavailable
	504,            //kGatewayTimeout
	505				//kHTTPVersionNotSupported
};

StrPtrLen HTTPProtocol::sStatusCodeAsStrings[] =
{
	StrPtrLen("100"),               //kContinue
	StrPtrLen("101"),               //kSwitchingProtocols
	StrPtrLen("200"),               //kOK
	StrPtrLen("201"),               //kCreated
	StrPtrLen("202"),               //kAccepted
	StrPtrLen("203"),               //kNonAuthoritativeInformation
	StrPtrLen("204"),               //kNoContent
	StrPtrLen("205"),               //kResetContent
	StrPtrLen("206"),               //kPartialContent
	StrPtrLen("300"),               //kMultipleChoices
	StrPtrLen("301"),               //kMovedPermanently
	StrPtrLen("302"),               //kFound
	StrPtrLen("303"),               //kSeeOther
	StrPtrLen("304"),               //kNotModified
	StrPtrLen("305"),               //kUseProxy
	StrPtrLen("307"),               //kTemporaryRedirect
	StrPtrLen("400"),               //kBadRequest
	StrPtrLen("401"),               //kUnAuthorized
	StrPtrLen("402"),               //kPaymentRequired
	StrPtrLen("403"),               //kForbidden
	StrPtrLen("404"),               //kNotFound
	StrPtrLen("405"),               //kMethodNotAllowed
	StrPtrLen("406"),               //kNotAcceptable
	StrPtrLen("407"),               //kProxyAuthenticationRequired
	StrPtrLen("408"),               //kRequestTimeout
	StrPtrLen("409"),               //kConflict
	StrPtrLen("410"),               //kGone
	StrPtrLen("411"),               //kLengthRequired
	StrPtrLen("412"),               //kPreconditionFailed
	StrPtrLen("413"),               //kRequestEntityTooLarge
	StrPtrLen("414"),               //kRequestURITooLarge
	StrPtrLen("415"),               //kUnsupportedMediaType
	StrPtrLen("416"),               //kRequestRangeNotSatisfiable
	StrPtrLen("417"),               //kExpectationFailed
	StrPtrLen("500"),               //kInternalServerError
	StrPtrLen("501"),               //kNotImplemented
	StrPtrLen("502"),               //kBadGateway
	StrPtrLen("503"),               //kServiceUnavailable
	StrPtrLen("504"),               //kGatewayTimeout
	StrPtrLen("505")                //kHTTPVersionNotSupported
};

SInt32 HTTPProtocol::GetStatusIndex(UInt32 inCode)
{
	SInt32 nRet = -1;
	for(SInt32 i = 0; i < httpNumStatusCodes; i++)
	{
		if(sStatusCodes[i] == (SInt32)inCode)
		{
			nRet = i;
			break;
		}
	}
	return nRet;
}

StrPtrLen HTTPProtocol::sVersionStrings[] =
{
	StrPtrLen("HTTP/1.0"),
	StrPtrLen("HTTP/1.1")
};

UINT HTTPProtocol::GetVersion(StrPtrLen & versionStr)
{
	if( versionStr.Len != 8 ) 
	{
		MsgLogGM(-1);
		return httpIllegalVersion;
	}

    SInt32 limit = httpNumVersions;
	for (SInt32 x = 0; x < limit; x++)
	{
		if( versionStr.EqualIgnoreCase(sVersionStrings[x].Ptr, sVersionStrings[x].Len))
			return x;
    }
	
	MsgLogGM(-1);
	return httpIllegalVersion;
}

