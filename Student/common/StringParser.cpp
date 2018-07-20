
#include "StringParser.h"

UInt8 StringParser::sNonWordMask[] =
{
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //0-9 
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //10-19 
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //20-29
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //30-39 
	1, 1, 1, 1, 1, 0, 1, 1, 1, 1, //40-49 - is a word
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //50-59
	1, 1, 1, 1, 1, 0, 0, 0, 0, 0, //60-69 //stop on every character except a letter
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //70-79
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //80-89
	0, 1, 1, 1, 1, 0, 1, 0, 0, 0, //90-99 _ is a word
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //100-109
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //110-119
	0, 0, 0, 1, 1, 1, 1, 1, 1, 1, //120-129
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //130-139
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //140-149
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //150-159
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //160-169
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //170-179
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //180-189
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //190-199
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //200-209
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //210-219
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //220-229
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //230-239
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //240-249
	1, 1, 1, 1, 1, 1 			 //250-255
};

UInt8 StringParser::sWordMask[] =
{
	// Inverse of the above
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //0-9 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //10-19 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //20-29
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //30-39 
	0, 0, 0, 0, 0, 1, 0, 0, 0, 0, //40-49 - is a word
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //50-59
	0, 0, 0, 0, 0, 1, 1, 1, 1, 1, //60-69 //stop on every character except a letter
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //70-79
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //80-89
	1, 0, 0, 0, 0, 1, 0, 1, 1, 1, //90-99 _ is a word
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //100-109
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //110-119
	1, 1, 1, 0, 0, 0, 0, 0, 0, 0, //120-129
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //130-139
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //140-149
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //150-159
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //160-169
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //170-179
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //180-189
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //190-199
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //200-209
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //210-219
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //220-229
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //230-239
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //240-249
	0, 0, 0, 0, 0, 0 			 //250-255
};

UInt8 StringParser::sDigitMask[] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //0-9
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //10-19 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //20-29
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //30-39
	0, 0, 0, 0, 0, 0, 0, 0, 1, 1, //40-49 //stop on every character except a number
	1, 1, 1, 1, 1, 1, 1, 1, 0, 0, //50-59
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //60-69 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //70-79
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //80-89
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //90-99
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //100-109
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //110-119
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //120-129
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //130-139
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //140-149
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //150-159
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //160-169
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //170-179
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //180-189
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //190-199
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //200-209
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //210-219
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //220-229
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //230-239
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //240-249
	0, 0, 0, 0, 0, 0			 //250-255
};

UInt8 StringParser::sEOLMask[] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //0-9   
	1, 0, 0, 1, 0, 0, 0, 0, 0, 0, //10-19    //'\r' & '\n' are stop conditions
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //20-29
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //30-39 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //40-49
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //50-59
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //60-69  
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //70-79
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //80-89
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //90-99
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //100-109
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //110-119
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //120-129
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //130-139
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //140-149
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //150-159
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //160-169
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //170-179
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //180-189
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //190-199
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //200-209
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //210-219
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //220-229
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //230-239
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //240-249
	0, 0, 0, 0, 0, 0 			 //250-255
};

UInt8 StringParser::sWhitespaceMask[] =
{
	1, 1, 1, 1, 1, 1, 1, 1, 1, 0, //0-9      // stop on '\t'
	0, 0, 0, 0, 1, 1, 1, 1, 1, 1, //10-19  	 // '\r', \v', '\f' & '\n'
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //20-29
	1, 1, 0, 1, 1, 1, 1, 1, 1, 1, //30-39   //  ' '
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //40-49
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //50-59
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //60-69
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //70-79
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //80-89
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //90-99
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //100-109
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //110-119
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //120-129
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //130-139
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //140-149
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //150-159
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //160-169
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //170-179
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //180-189
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //190-199
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //200-209
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //210-219
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //220-229
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //230-239
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //240-249
	1, 1, 1, 1, 1, 1 			 //250-255
};

UInt8 StringParser::sEOLWhitespaceMask[] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 1, //0-9   	// \t is a stop
		1, 1, 1, 1, 0, 0, 0, 0, 0, 0, //10-19    //'\r' & '\n' are stop conditions
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //20-29
		0, 0, 1, 0, 0, 0, 0, 0, 0, 0, //30-39 	' '  is a stop
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //40-49
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //50-59
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //60-69  
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //70-79
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //80-89
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //90-99
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //100-109
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //110-119
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //120-129
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //130-139
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //140-149
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //150-159
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //160-169
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //170-179
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //180-189
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //190-199
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //200-209
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //210-219
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //220-229
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //230-239
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //240-249
		0, 0, 0, 0, 0, 0 			 //250-255
};

UInt8 StringParser::sURLStopConditions[] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 1, //0-9      //'\t' is a stop condition
		1, 0, 0, 1, 0, 0, 0, 0, 0, 0, //10-19    //'\r' & '\n' are stop conditions
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //20-29
		0, 0, 1, 0, 0, 0, 0, 0, 0, 0, //30-39    //' '
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //40-49
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //50-59
		0, 0, 0, 1, 0, 0, 0, 0, 0, 0, //60-69	 //'?' 
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //70-79
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //80-89
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //90-99
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //100-109
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //110-119
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //120-129
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //130-139
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //140-149
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //150-159
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //160-169
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //170-179
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //180-189
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //190-199
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //200-209
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //210-219
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //220-229
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //230-239
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //240-249
		0, 0, 0, 0, 0, 0 			  //250-255
};

StringParser::StringParser(StrPtrLen * inStream)
:	fStartGet(inStream == NULL ? NULL : inStream->Ptr),
fEndGet(inStream == NULL ? NULL : inStream->Ptr + inStream->Len),
fCurLineNumber(1), fStream(inStream)
{
}

void StringParser::ConsumeUntil(StrPtrLen* outString, char inStop)
{
	ASSERT(fStartGet != NULL);
	ASSERT(fEndGet != NULL);
	ASSERT(fStartGet <= fEndGet);

	char *originalStartGet = fStartGet;

	while ((fStartGet < fEndGet) && (*fStartGet != inStop))
		AdvanceMark();

	if (outString != NULL)
	{
		outString->Ptr = originalStartGet;
		outString->Len = fStartGet - originalStartGet;
	}
}

void StringParser::AdvanceMark()
{
	if ((*fStartGet == '\n') || ((*fStartGet == '\r') && (fStartGet[1] != '\n')))
	{
		// we are progressing beyond a line boundary (don't count \r\n twice)
		fCurLineNumber++;
	}
	fStartGet++;
}

void StringParser::ConsumeUntil(StrPtrLen* outString, UInt8 *inMask)
{
	ASSERT(fStartGet != NULL);
	ASSERT(fEndGet != NULL);
	ASSERT(fStartGet <= fEndGet);

	char *originalStartGet = fStartGet;

	while ((fStartGet < fEndGet) && (!inMask[*fStartGet]))
		AdvanceMark();

	if (outString != NULL)
	{
		outString->Ptr = originalStartGet;
		outString->Len = fStartGet - originalStartGet;
	}
}

void StringParser::ConsumeLength(StrPtrLen* spl, SInt32 inLength)
{
	ASSERT(fStartGet != NULL);
	ASSERT(fEndGet != NULL);
	ASSERT(fStartGet <= fEndGet);

	//sanity check to make sure we aren't being told to run off the end of the
	//buffer
	if ((fEndGet - fStartGet) < inLength)
		inLength = fEndGet - fStartGet;

	if (spl != NULL)
	{
		spl->Ptr = fStartGet;
		spl->Len = inLength;
	}
	if (inLength > 0)
	{
		for (short i=0; i<inLength; i++)
			AdvanceMark();
	}
	else
		fStartGet += inLength;	// ***may mess up line number if we back up too much
}


UInt64 StringParser::ConsumeLong64(StrPtrLen* outString/* = NULL*/)
{
	ASSERT(fStartGet != NULL);
	ASSERT(fEndGet != NULL);
	ASSERT(fStartGet <= fEndGet);

	UInt64 theValue = 0;
	char *originalStartGet = fStartGet;

	while ((fStartGet < fEndGet) && (*fStartGet >= '0') && (*fStartGet <= '9'))
	{
		theValue = (theValue * 10) + (*fStartGet - '0');
		AdvanceMark();
	}

	if (outString != NULL)
	{
		outString->Ptr = originalStartGet;
		outString->Len = fStartGet - originalStartGet;
	}
	return theValue;
}

UInt32 StringParser::ConsumeInteger(StrPtrLen* outString)
{
	ASSERT(fStartGet != NULL);
	ASSERT(fEndGet != NULL);
	ASSERT(fStartGet <= fEndGet);

	UInt32 theValue = 0;
	char *originalStartGet = fStartGet;

	while ((fStartGet < fEndGet) && (*fStartGet >= '0') && (*fStartGet <= '9'))
	{
		theValue = (theValue * 10) + (*fStartGet - '0');
		AdvanceMark();
	}

	if (outString != NULL)
	{
		outString->Ptr = originalStartGet;
		outString->Len = fStartGet - originalStartGet;
	}
	return theValue;
}

Float32 StringParser::ConsumeFloat()
{
	Float32 theFloat = 0;
	while ((fStartGet < fEndGet) && (*fStartGet >= '0') && (*fStartGet <= '9'))
	{
		theFloat = (theFloat * 10) + (*fStartGet - '0');
		AdvanceMark();
	}
	if ((fStartGet < fEndGet) && (*fStartGet == '.'))
		AdvanceMark();
	Float32 multiplier = 0.1f;
	while ((fStartGet < fEndGet) && (*fStartGet >= '0') && (*fStartGet <= '9'))
	{
		theFloat += (multiplier * (*fStartGet - '0'));
		multiplier *= 0.1f;
		AdvanceMark();
	}
	return theFloat;
}

//
// 解析微软特有的 \r\n 数据包格式...
Bool16 StringParser::GetThruSPLIT(StrPtrLen * outString)
{
	ASSERT(fStartGet != NULL);
	ASSERT(fEndGet != NULL);
	ASSERT(fStartGet <= fEndGet);

	Bool16 retVal = false;
	char * originalStartGet = fStartGet;

	while( (fStartGet < fEndGet) && ((fStartGet+1) <= fEndGet) )
	{
		if( (*fStartGet == '{') && 
			(fStartGet[1] == '\r') && (fStartGet[2] == '\r') &&
			(fStartGet[3] == '*') &&
			(fStartGet[4] == '\r') && (fStartGet[5] == '\r') && 
			(fStartGet[6] == '}')) {
				retVal = true;
				break;
			}
		else
			AdvanceMark();
	}
	if( outString != NULL )
	{
		outString->Ptr = originalStartGet;
		outString->Len = fStartGet - originalStartGet;
	}
	retVal ? this->ConsumeLength(NULL, 7) : NULL;
	return retVal;
}
//
// 解析微软特有的 \r\n 数据包格式...
Bool16 StringParser::GetThruMSEOL(StrPtrLen * outString)
{
	ASSERT(fStartGet != NULL);
	ASSERT(fEndGet != NULL);
	ASSERT(fStartGet <= fEndGet);

	Bool16 retVal = false;
	char * originalStartGet = fStartGet;

	while( (fStartGet < fEndGet) && ((fStartGet+1) <= fEndGet) )
	{
		if( (*fStartGet == '\r') && (fStartGet[1] == '\n') ) {
			retVal = true;
			break;
		}
		else
			AdvanceMark();
	}
	if( outString != NULL )
	{
		outString->Ptr = originalStartGet;
		outString->Len = fStartGet - originalStartGet;
	}
	retVal ? this->ConsumeLength(NULL, 2) : NULL;
	return retVal;
}

Bool16	StringParser::Expect(char stopChar)
{
	if (fStartGet >= fEndGet)
		return false;
	if(*fStartGet != stopChar)
		return false;
	else
	{
		AdvanceMark();
		return true;
	}
}
Bool16 StringParser::ExpectEOL()
{
	//This function processes all legal forms of HTTP / RTSP eols.
	//They are: \r (alone), \n (alone), \r\n
	Bool16 retVal = false;
	if ((fStartGet < fEndGet) && ((*fStartGet == '\r') || (*fStartGet == '\n')))
	{
		retVal = true;
		AdvanceMark();
		//check for a \r\n, which is the most common EOL sequence.
		if ((fStartGet < fEndGet) && ((*(fStartGet - 1) == '\r') && (*fStartGet == '\n')))
			AdvanceMark();
	}
	return retVal;
}

void StringParser::UnQuote(StrPtrLen* outString)
{
	// If a string is contained within double or single quotes 
	// then UnQuote() will remove them. - [sfu]

	// sanity check
	if (outString->Ptr == NULL || outString->Len < 2)
		return;

	// remove begining quote if it's there.
	if (outString->Ptr[0] == '"' || outString->Ptr[0] == '\'')
	{
		outString->Ptr++; outString->Len--;
	}
	// remove ending quote if it's there.
	if ( outString->Ptr[outString->Len-1] == '"' || 
		outString->Ptr[outString->Len-1] == '\'' )
	{
		outString->Len--;
	}
}

SInt32 StringParser::DecodeURI(const char* inSrc, SInt32 inSrcLen, char* ioDest, SInt32 inDestLen)
{
	// return the number of chars written to ioDest
	// or KH_BadURLFormat in the case of any error.

	if( inSrcLen <= 0 || inSrc == NULL || ioDest == NULL || inDestLen <= 0 ) {
		MsgLogGM(-1);
		return -1;
	}

	//ASSERT(*inSrc == '/'); //For the purposes of '..' stripping, we assume first char is a /

	SInt32 theLengthWritten = 0;
	int tempChar = 0;
	int numDotChars = 0;

	while (inSrcLen > 0)
	{
		if (theLengthWritten == inDestLen) {
			MsgLogGM(-1);
			return -1;
		}

		if (*inSrc == '%')
		{
			if (inSrcLen < 3) {
				MsgLogGM(-1);
				return -1;
			}

			//if there is a special character in this URL, extract it
			char tempbuff[3];
			inSrc++;
			if (!isxdigit(*inSrc)) {
				MsgLogGM(-1);
				return -1;
			}
			tempbuff[0] = *inSrc;
			inSrc++;
			if (!isxdigit(*inSrc)) {
				MsgLogGM(-1);
				return -1;
			}
			tempbuff[1] = *inSrc;
			inSrc++;
			tempbuff[2] = '\0';
			sscanf(tempbuff, "%x", &tempChar);
			if( tempChar >= 256 ) {
				MsgLogGM(-1);
				return -1;
			}
			ASSERT(tempChar < 256);
			inSrcLen -= 3;		
		}
		else if (*inSrc == '\0') {
			MsgLogGM(-1);
			return -1;
		}
		else
		{
			// Any normal character just gets copied into the destination buffer
			tempChar = *inSrc;
			inSrcLen--;
			inSrc++;
		}

		//
		// If we are in a file system that uses a character besides '/' as a
		// path delimiter, we should not allow this character to appear in the URL.
		// In URLs, only '/' has control meaning.
		//if ((tempChar == kPathDelimiterChar) && (kPathDelimiterChar != '/'))
		if (tempChar == '\\') {
			MsgLogGM(-1);
			return -1;
		}

		// Check to see if this character is a path delimiter ('/')
		// If so, we need to further check whether backup is required due to
		// dot chars that need to be stripped
		if ((tempChar == '/') && (numDotChars <= 2) && (numDotChars > 0))
		{
			if( theLengthWritten <= numDotChars ) {
				MsgLogGM(-1);
				return -1;
			}
			ASSERT(theLengthWritten > numDotChars);
			ioDest -= (numDotChars + 1);
			theLengthWritten -= (numDotChars + 1);
		}

		*ioDest = tempChar;

		// Note that because we are doing this dotchar check here, we catch dotchars
		// even if they were encoded to begin with.

		// If this is a . , check to see if it's one of those cases where we need to track
		// how many '.'s in a row we've gotten, for stripping out later on
		if (*ioDest == '.')
		{
			if( theLengthWritten <= 0 ) {
				MsgLogGM(-1);
				return -1;
			}
			ASSERT(theLengthWritten > 0);//first char is always '/', right?
			if ((numDotChars == 0) && (*(ioDest - 1) == '/'))
				numDotChars++;
			else if ((numDotChars > 0) && (*(ioDest - 1) == '.'))
				numDotChars++;
		}
		// If this isn't a dot char, we don't care at all, reset this value to 0.
		else
			numDotChars = 0;

		theLengthWritten++;
		ioDest++;
	}

	// Before returning, "strip" any trailing "." or ".." by adjusting "theLengthWritten
	// accordingly
	if (numDotChars <= 2)
		theLengthWritten -= numDotChars;
	return theLengthWritten;
}

SInt32 StringParser::EncodeURI(const char* inSrc, SInt32 inSrcLen, char* ioDest, SInt32 inDestLen)
{
	// return the number of chars written to ioDest

	SInt32 theLengthWritten = 0;

	while (inSrcLen > 0)
	{
		if (theLengthWritten == inDestLen) {
			MsgLogGM(-1);
			return -1;
		}

		//
		// Always encode 8-bit characters
		if ((unsigned char)*inSrc > 127)
		{
			if (inDestLen - theLengthWritten < 3) {
				MsgLogGM(-1);
				return -1;
			}
			sprintf(ioDest,"%%%X",(BYTE)*inSrc);
			ioDest += 3;
			theLengthWritten += 3;

			inSrc++;
			inSrcLen--;
			continue;
		}

		//
		// Only encode certain 7-bit characters
		switch (*inSrc)
		{
			// This is the URL RFC list of illegal characters.
		case (' '):
		case ('\r'):
		case ('\n'):
		case ('\t'):
		case ('<'):
		case ('>'):
		case ('#'):
		case ('%'):
		case ('{'):
		case ('}'):
		case ('|'):
		case ('\\'):
		case ('^'):
		case ('~'):
		case ('['):
		case (']'):
		case ('`'):
		case (';'):
			//case ('/'):
		case ('?'):
		case ('@'):
		case ('='):
		case ('&'):
		case ('$'):
		case ('"'):
			{
				if ((inDestLen - theLengthWritten) < 3) {
					MsgLogGM(-1);
					return -1;
				}

				sprintf(ioDest,"%%%X",(BYTE)*inSrc);
				ioDest += 3;
				theLengthWritten += 3;
				break;
			}
		default:
			{
				*ioDest = *inSrc;
				ioDest++;
				theLengthWritten++;
			}
		}

		inSrc++;
		inSrcLen--;
	}

	return theLengthWritten;
}

void StringParser::EncodePath(char* inSrc, UInt32 inSrcLen)
{
	for (UInt32 x = 0; x < inSrcLen; x++)
	{
		if (inSrc[x] == '\\')		// '\\' => '/'
			inSrc[x] = '/';
	}
}

void StringParser::DecodePath(char* inSrc, UInt32 inSrcLen, BOOL bExtend/*=false*/)
{
	for (UInt32 x = 0; x < inSrcLen; x++)
	{
		if (inSrc[x] == '/')		// '/' => '\\'
			inSrc[x] = '\\';
		if( !bExtend )				// 是否进行扩展解码
			continue;
		if (inSrc[x] == '<')		// '<' => ' '
			inSrc[x] = ' ';
		if (inSrc[x] == '>')		// '>' => '&'
			inSrc[x] = '&';
		if (inSrc[x] == '?')		// '?' => '+'
			inSrc[x] = '+';
		if (inSrc[x] == '|')		// '|' => '%'
			inSrc[x] = '%';
	}
}
//
// &amp;&nbsp; 这些特殊字符编码，主要处理 & 
void StringParser::EncodeHTML(char* inSrc, UInt32 inSrcLen)
{
	for (UInt32 x = 0; x < inSrcLen; x++)
	{
		if (inSrc[x] == '&')		// '&' => '>'
			inSrc[x] = '>';
	}
}
//
// &amp;&nbsp; 这些特殊字符解码，主要处理 & 
void StringParser::DecodeHTML(char* inSrc, UInt32 inSrcLen, BOOL bExtend/*=false*/)
{
	for (UInt32 x = 0; x < inSrcLen; x++)
	{
		if (inSrc[x] == '>')		// '>' => '&'
			inSrc[x] = '&';
		if( !bExtend )				// 是否进行扩展解码
			continue;
		if (inSrc[x] == '/')		// '/' => '\\'
			inSrc[x] = '\\';
	}
}
//
// (2003.05.22)Recover the old string position...
void StringParser::Wrap(char * inSrc, int inLineNum)
{
	ASSERT( inSrc != NULL );
	ASSERT( inSrc < fEndGet );
	fStartGet	   = inSrc;
	fCurLineNumber = inLineNum;
}

BOOL StringParser::MoveFrontPtr(int nLen)
{
	ASSERT(fStartGet + nLen < fEndGet);
	if(fStartGet + nLen > fEndGet)
		return FALSE;	
	fStartGet = fStartGet + nLen;
	return TRUE;
}

#if STRINGPARSERTESTING
Bool16 StringParser::Test()
{
	static char* string1 = "RTSP 200 OK\r\nContent-Type: MeowMix\r\n\t   \n3450";
	
	StrPtrLen theString(string1, strlen(string1));
	
	StringParser victim(&theString);
	
	StrPtrLen rtsp;
	SInt32 theInt = victim.ConsumeInteger();
	if (theInt != 0)
		return false;
	victim.ConsumeWord(&rtsp);
	if ((rtsp.Len != 4) && (strncmp(rtsp.Ptr, "RTSP", 4) != 0))
		return false;
		
	victim.ConsumeWhitespace();
	theInt = victim.ConsumeInteger();
	if (theInt != 200)
		return false;
		
	return true;
}
#endif
