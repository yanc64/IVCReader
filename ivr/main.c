//
//  main.c
//  ivr
//
//  Created by Yannis Calotychos on 19/5/21.
//

#include <stdio.h>
#include "libIVCReader.h"

/////
static void _dataFeedProc(void *clientInfo,
						  const UInt32 uid,
						  const char *fieldName,
						  const UInt8 fieldType,
						  const void *data
						  )
{
	printf("uid = %-10u %-30s ", uid, fieldName);

	switch( fieldType )
	{
		case string_utf8		: printf("= %s\r", (char *) data); break;
		case number_sint32    	: printf("= %d\r", *(SInt32 *)&data[0]); break;
		case number_uint32    	: printf("= %d\r", *(UInt32 *)&data[0]); break;
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
	char *filename = "/Users/yan/Downloads/SAMPLE-AAA/_Travels.ivc";

	IVCOpen(filename, &total, &status);
	printf("%s [open status = %d, file count = %d]\r", filename, status, total);

	IVCReport(nil, _dataFeedProc, &status);
	printf("%s [read status = %d]\r", filename, status);

	IVCClose();
	printf("%s [close]\r", filename);

	
	return 0;
}
