////////////////////////////////////////////////////////////////////////////////////////////////////////
//	Copyright (C) 2003	Kuihua Tech Inc. All rights reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////////
//	Author	:	Jackey
//	Date	:	2003.01.07
//	File	:	StringParser.h
//	Contains:	Define String Parser Class...
//	History	:
//		1.0	:	2003.01.07 - First Version...
//				2003.05.22 - Add Wrap() Function...
//	Mailto	:	Omega@Kuihua.net (Bug Report or Comments)
////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _STRINGPARSER_H_
#define _STRINGPARSER_H_

#include "assert.h"
#include "StrPtrLen.h"

#ifndef ASSERT
#define ASSERT assert
#endif // ASSERT

#define STRINGPARSERTESTING 0

class StringParser
{
	public:
		
		StringParser(StrPtrLen *inStream);
		~StringParser() {}
		
		// Built-in masks for common stop conditions
		static UInt8 sDigitMask[]; 			// stop when you hit a digit
		static UInt8 sWordMask[]; 			// stop when you hit a word
		static UInt8 sEOLMask[];			// stop when you hit an eol
		static UInt8 sEOLWhitespaceMask[];	// stop when you hit an EOL or whitespace
		static UInt8 sWhitespaceMask[];		// skip over whitespace
		static UInt8 sURLStopConditions[];	// stop for url

		//GetBuffer:
		//Returns a pointer to the string object
		StrPtrLen*		GetStream()		{ return fStream; }
		char	 *		GetCurStart()	{ return fStartGet ;}
		//Expect:
		//These functions consume the given token/word if it is in the stream.
		//If not, they return false.
		//In all other situations, true is returned.
		//NOTE: if these functions return an error, the object goes into a state where
		//it cannot be guarenteed to function correctly.
		Bool16			Expect(char stopChar);
		Bool16			ExpectEOL();
		
		//Returns the next word
		void			ConsumeWord(StrPtrLen* outString = NULL)
							{ ConsumeUntil(outString, sNonWordMask); }

		//Returns all the data before inStopChar
		void			ConsumeUntil(StrPtrLen* outString, char inStopChar);

		//Returns whatever integer is currently in the stream
		UInt64			ConsumeLong64(StrPtrLen* outString = NULL);
		UInt32			ConsumeInteger(StrPtrLen* outString = NULL);
		Float32 		ConsumeFloat();

		//Keeps on going until non-whitespace
		void			ConsumeWhitespace()
							{ ConsumeUntil(NULL, sWhitespaceMask); }
		
		//Assumes 'stop' is a 255-char array of booleans. Set this array
		//to a mask of what the stop characters are. true means stop character.
		//You may also pass in one of the many prepackaged masks defined above.
		void			ConsumeUntil(StrPtrLen* spl, UInt8 *stop);


		//+ rt 8.19.99
		//returns whatever is avaliable until non-whitespace
		void			ConsumeUntilWhitespace(StrPtrLen* spl = NULL)
							{ ConsumeUntil( spl, sEOLWhitespaceMask); }
		void			ConsumeLength(StrPtrLen* spl, SInt32 numBytes);

		//GetThru:
		//Works very similar to ConsumeUntil except that it moves past the stop token,
		//and if it can't find the stop token it returns false
		inline Bool16	GetThru(StrPtrLen* spl, char stop);
		inline Bool16	GetThruEOL(StrPtrLen* spl);
		       Bool16	GetThruMSEOL(StrPtrLen * outString);
			   Bool16	GetThruSPLIT(StrPtrLen * outString);
		
		//Returns the current character, doesn't move past it.
		inline char		PeekFast() { return *fStartGet; }
		char operator[](int i) { ASSERT((fStartGet+i) < fEndGet); return fStartGet[i]; }
		
		//Returns some info about the stream
		UInt32			GetDataParsedLen() 
			{ ASSERT(fStartGet >= fStream->Ptr); return (UInt32)(fStartGet - fStream->Ptr); }
		UInt32			GetDataReceivedLen()
			{ ASSERT(fEndGet >= fStream->Ptr); return (UInt32)(fEndGet - fStream->Ptr); }
		UInt32			GetDataRemaining()
			{ ASSERT(fEndGet >= fStartGet); return (UInt32)(fEndGet - fStartGet); }
		char*			GetCurrentPosition() { return fStartGet; }
		int				GetCurrentLineNumber() { return fCurLineNumber; }
		
		void			Wrap(char * inSrc, int inLineNum);

		// A utility for extracting quotes from the start and end of a parsed
		// string. (Warning: Do not call this method if you allocated your own  
		// pointer for the Ptr field of the StrPtrLen class.) - [sfu]
		static void UnQuote(StrPtrLen* outString);

		//DecodeURI:
		//
		// This function does 2 things: Decodes % encoded characters in URLs, and strips out
		// any ".." or "." complete filenames from the URL. Writes the result into ioDest.
		//
		//If successful, returns the length of the destination string.
		//If failure, returns an OS errorcode: KH_BadURLFormat, KH_NotEnoughSpace

		static SInt32	DecodeURI(const char* inSrc, SInt32 inSrcLen, char* ioDest, SInt32 inDestLen);

		//EncodeURI:
		//
		// This function takes a character string and % encodes any special URL characters.
		// In general, the output buffer will be longer than the input buffer, so caller should
		// be aware of that.
		//
		//If successful, returns the length of the destination string.
		//If failure, returns an QTSS errorcode: KH_NotEnoughSpace
		//
		// If function returns E2BIG, ioDest will be valid, but will contain
		// only the portion of the URL that fit.
		static SInt32	EncodeURI(const char* inSrc, SInt32 inSrcLen, char* ioDest, SInt32 inDestLen);
		
		// DecodePath:
		//
		// This function converts "network" or "URL" path delimiters (the '/' char) to
		// the path delimiter of the local file system. It does this conversion in place,
		// so the old data will be overwritten
		static void		DecodePath(char* inSrc, UInt32 inSrcLen, BOOL bExtend=false);
		static void		EncodePath(char* inSrc, UInt32 inSrcLen);
		static void		DecodeHTML(char* inSrc, UInt32 inSrcLen, BOOL bExtend=false);
		static void		EncodeHTML(char* inSrc, UInt32 inSrcLen);

		BOOL MoveFrontPtr(int nLen);
#if STRINGPARSERTESTING
		static Bool16		Test();
#endif

	private:
		void		AdvanceMark();

		//built in masks for some common stop conditions
		static UInt8 sNonWordMask[];

		char*		fStartGet;
		char*		fEndGet;
		int			fCurLineNumber;
		StrPtrLen*	fStream;
		
};

Bool16 StringParser::GetThru(StrPtrLen* outString, char inStopChar)
{
	ConsumeUntil(outString, inStopChar);
	return Expect(inStopChar);
}

Bool16 StringParser::GetThruEOL(StrPtrLen* outString)
{
	ConsumeUntil(outString, sEOLMask);
	return ExpectEOL();
}
#endif	// _STRINGPARSER_H_
