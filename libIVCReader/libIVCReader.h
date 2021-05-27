//
//  libIVCReader.h
//  libIVCReader
//
//  Created by Yannis Calotychos on 21/05/2021.
//  Copyright © 2021 Smart Toolbox Ideas. All rights reserved.
//

#ifndef __libIVCReader__
#define __libIVCReader__

#pragma once
#include "libIVCFields.h"
#include <MacTypes.h>

// Error Definitions
enum {
	fileNotFoundErr = 1, unsupportedVersionErr, wrongBytesErr, wrongOffsetErr, ZeroFolderBuffer,
	FoldersChunkIdentifierError, memoryErr, wrongFileCountErr, parsingCantStartErr
};

// Callback Field Types
enum {
	string_utf8, 			// null terminated UTF8 string
	number_sint32,			// signed integer 32 bit
	number_uint32,			// unsigned integer 32 bit
	number_rational,		// an array of 2 unsigned 32 bit integers. first is nominator, second is denominator
	number_rational3,		// an array of 3 rationals (i.e. 6 x 32 bit integers)
};

typedef void (*DataFeed)(const UInt32 uid,
						 const char *fieldName,
						 const UInt8 fieldType,
						 const void *fieldData
						 );


#ifdef  __cplusplus
extern  "C" {
#endif

void IVCOpen(char *filename, SInt16 *total, SInt16 *err);
void IVCReport(DataFeed dataFeed, SInt16 *err);
void IVCClose(void);


#ifdef __cplusplus
}
#endif
#endif
