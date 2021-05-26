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
static void _dataFeedProc(const UInt32 uid, const char *fieldName, const UInt8 fieldType, const void *data)
{
	printf("uid = %-10u %-30s ", uid, fieldName);

	switch( fieldType )
	{
		case string_utf8		: printf("= %s\r", (char *) data); break;
		case number_sint32    	: printf("= %d\r", *(SInt32 *)&data[0]); break;
		case number_uint32    	: printf("= %d\r", *(UInt32 *)&data[0]); break;
		case number_sint16    	: printf("= %d\r", *(SInt16 *)&data[0]); break;
		case number_rational  	: printf("= %d/%d\r", *(SInt32 *)&data[0], *(SInt32 *)&data[4]); break;
		case number_rational3 	: printf("= %d/%d %d/%d %d/%d\r", *(SInt32 *)&data[0], *(SInt32 *)&data[4], *(SInt32 *)&data[8], *(SInt32 *)&data[12], *(SInt32 *)&data[16], *(SInt32 *)&data[20]); break;
		default               	: printf("\r"); break;
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
