/*  
    EQ2Emulator:  Everquest II Server Emulator
    Copyright (C) 2007  EQ2EMulator Development Team (http://www.eq2emulator.net)

    This file is part of EQ2Emulator.

    EQ2Emulator is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    EQ2Emulator is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with EQ2Emulator.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef MISCFUNCTIONS_H
#define MISCFUNCTIONS_H

#include "types.h"
#include "seperator.h"
#include <stdio.h>
#include <ctype.h>
#include <vector>
#include <map>

#ifndef ERRBUF_SIZE
#define ERRBUF_SIZE		1024
#endif

int		MakeAnyLenString(char** ret, const char* format, ...);
int32	hextoi(char* num);
int64	hextoi64(char* num);
sint32	filesize(FILE* fp);
int32	ResolveIP(const char* hostname, char* errbuf = 0);
void	CoutTimestamp(bool ms = true);
char*	strn0cpy(char* dest, const char* source, int32 size);
		// return value =true if entire string(source) fit, false if it was truncated
bool	strn0cpyt(char* dest, const char* source, int32 size);
string	loadInt32String(uchar* buffer, int16 buffer_size, int16* pos, EQ2_32BitString* eq_string = NULL);
string	loadInt16String(uchar* buffer, int16 buffer_size, int16* pos, EQ2_16BitString* eq_string = NULL);
string	 loadInt8String(uchar* buffer, int16 buffer_size, int16* pos, EQ2_8BitString* eq_string = NULL);
sint16  storeInt32String(uchar* buffer, int16 buffer_size, string in_str);
sint16  storeInt16String(uchar* buffer, int16 buffer_size, string in_str);
sint16   storeInt8String(uchar* buffer, int16 buffer_size, string in_str);
int		MakeRandomInt(int low, int high);
float	MakeRandomFloat(float low, float high);
int32	GenerateEQ2Color(float r, float g, float b);
int32	GenerateEQ2Color(float* rgb[3]);
void	SetColor(EQ2_Color* color, long data);
//void	CreateEQ2Color(EQ2_Color* color, uchar* data, int16* size);
int8	MakeInt8(uchar* data, int16* size);
int8	MakeInt8(float* input);
bool	Unpack(int32 srcLen, uchar* data, uchar* dst, int16 dstLen, int16 version = 0, bool reverse = true);
bool	Unpack(uchar* data, uchar* dst, int16 dstLen, int16 version = 0, bool reverse = true);
int32	Pack(uchar* data, uchar* src, int16 srcLen, int16 dstLen, int16 version = 0);
void	Reverse(uchar* input, int32 srcLen);
void	Encode(uchar* dst, uchar* src, int16 len);
void	Decode(uchar* dst, uchar* src, int16 len);
string	ToUpper(string input);
string	ToLower(string input);
int32 ParseIntValue(string input);
map<string, string> TranslateBrokerRequest(string request);
void	MovementDecode(uchar* dst, uchar* newval, uchar* orig, int16 len);
vector<string>* SplitString(string str, char delim);
int8 DoOverLoad(int32 val, uchar* data);
int8 CheckOverLoadSize(int32 val);
int32	CountWordsInString(const char* text);
bool IsNumber(const char *num);
void PrintSep(Seperator *sep, const char *name = 0);
///<summary>Gets the packet type for the given version</summary>
///<param name='version'>The client version</param>
int16 GetItemPacketType(int32 version);
///<summary>Gets the opcode version_range1 from the clients version</summary>
///<param name='version'>The client version</param>
int16 GetOpcodeVersion(int16 version);
void SleepMS(int32 milliseconds);
size_t strlcpy(char *dst, const char *src, size_t size);

bool INIReadBool(FILE *f, const char *section, const char *property, bool *out);
bool INIReadInt(FILE *f, const char *section, const char *property, int *out);

template<class Type> void AddData(Type input, string* datastring){
	if(datastring)
		datastring->append((char*)&input, sizeof(input));
}

template<class Type> void AddData(Type input, int32 array_size, string* datastring){
	if(array_size>0){
		for(int32 i=0;i<array_size;i++)
			AddData(input[i], datastring);
	}
	else
		AddData(input, datastring);
}

#ifndef WIN32
#define _ITOA_BUFLEN	25

const char * itoa(int value);
char * itoa(int value, char *result, int base);

#endif

class InitWinsock {
public:
	InitWinsock();
	~InitWinsock();
};

template<class T> class AutoDelete {
public:
	AutoDelete(T** iVar, T* iSetTo = 0) {
		init(iVar, iSetTo);
	}
	AutoDelete() {}
	void init(T** iVar, T* iSetTo = 0)
	{
		pVar = iVar;
		if (iSetTo)
			*pVar = iSetTo;
	}
	~AutoDelete() {
		safe_delete(*pVar);
	}
private:
	T** pVar;
};

#endif


