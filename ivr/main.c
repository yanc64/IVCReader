//
//  main.c
//  ivr
//
//  Created by Yannis Calotychos on 19/5/21.
//

#include <stdio.h>
#include "iview_read.h"

void logstr(const char *t, const char *s) { printf("\t%-30s = %s\r", t, s?s:"-"); }
void lognum(const char *t, const long *l) { if( l ) printf("\t%-30s = %ld\r", t, *l); else printf("\t%-30s = -\r", t); }

/////
static void _dataFeedProc(const UInt32 uid, const char *fieldName, const UInt8 fieldType, const void *data, const UInt32 size)
{
	printf("uid = %-10u %-30s ", uid, fieldName);
	
	long 		len;
	char 		cstr[4096];
	SInt16 		s;
	SInt32 		rn, rn2, rn3;
	SInt32 		rd, rd2, rd3;
	UInt32 		uu;
	SInt32 		ss;

	switch( fieldType )
	{
		case text_utf8:
		case text_ascii:
			len = size >= 4095 ? 4095: size;
			memcpy(cstr, data, len);
			cstr[len] = 0;
			printf("= %s\r", cstr);
			break;

		case number_sint32:
			memcpy(&ss, data, 4);
			printf("= %d\r", EndianS32_BtoN(ss));
			break;
			
		case number_uint32:
			memcpy(&uu, data, 4);
			printf("= %d\r", EndianU32_BtoN(uu));
			break;

		case number_sint16:
			memcpy(&s, data, 2);
			printf("= %d\r", EndianS16_BtoN(s));
			break;

		case number_rational:
			memcpy(&rn, data, 4);
			memcpy(&rd, data+4, 4);
			printf("= %d/%d\r", EndianS32_BtoN(rn), EndianS32_BtoN(rd));
			break;

		case number_rational3:
			memcpy(&rn,  data,    4);
			memcpy(&rd,  data+4,  4);
			memcpy(&rn2, data+8,  4);
			memcpy(&rd2, data+12, 4);
			memcpy(&rn3, data+16, 4);
			memcpy(&rd3, data+20, 4);
			printf("= %d/%d %d/%d %d/%d\r", EndianS32_BtoN(rn), EndianS32_BtoN(rd), EndianS32_BtoN(rn2), EndianS32_BtoN(rd2), EndianS32_BtoN(rn3), EndianS32_BtoN(rd3));
			break;

		default:
			printf("\r");
			break;
	}
	
}

////
int main(int argc, const char * argv[])
{
	SInt16 status;
	SInt16 total;
//	char *filename = "/Users/yan/Downloads/SAMPLE-AAA/_Hifi.ivc";
	char *filename = "/Users/yan/Downloads/_Travels.ivc";
	

	IVCOpen(filename, &total, &status);
	IVCReport(_dataFeedProc, &status);
	IVCClose();

	return 0;
}
