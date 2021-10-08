//
//  libIVCReader.c
//  libIVCReader
//
//  Created by Yannis Calotychos on 21/05/2021.
//  See LICENSE.txt folder for information.
//

#include <CoreServices/CoreServices.h>	// Needed Types & for Endian Conversions
#include <iconv.h>						// Needed for unicode conversion
#include "private.h"
#include "libIVCReader.h"

#define kVersionNumber 	"1.1"
#define kVersionDate	"8.Oct.2021"

// Local cariables
static OSType			gCatFormat;
static UInt32			gFileCount;
static UInt32			gCurrentChuckOffset;
static bool 			gMorselIsHK = false;

static bool 			gWantsInfo = true;
static bool 			gWantsIptc = true;
static bool 			gWantsExif = true;
static bool 			gWantsPict = true;

static FILE *			gfp;
static DataFeed 		outFeed;
static void *			outClientInfo;

enum {
	text_outString, // null terminated
	number_sint16N = 100, number_sint16B, text_utf16, text_utf8, text_ascii, pict_data
};


///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark -

////
void IVCOpen(const char *filename,
			 const bool wantsInfo, const bool wantsIptc, const bool wantsExif, const bool wantsPict,
			 SInt16 *fileCount, SInt16 *status)
{
	printf("\rlibIVCReader %s [%s]\r\r", kVersionNumber, kVersionDate);

	OSErr myErr = noErr;
	gFileCount = 0;
	gCatFormat = 0;

	gWantsInfo = wantsInfo;
	gWantsIptc = wantsIptc;
	gWantsExif = wantsExif;
	gWantsPict = wantsPict;

	
	gfp = fopen(filename, "r");
	if( !gfp )
	{
		myErr = fileNotFoundErr;
		goto bail;
	}
	
	UInt32 bytes;

	/////////////////
	// READ THE FILE HEADER
	bytes = 4;
	if((myErr = myfread(gfp, &bytes, &gFileCount)) ||
	   (myErr = myfread(gfp, &bytes, &gCatFormat)) )
		goto bail;

	/*
	 DETERMINE IF THIS IS :
	 (a) Early iView Verions (ignore)
	 (b) A "PRO" FILE, BY READING THE FILE FOOTER (2 LONGS) : IF THE 1ST OF THE 2 LONGS = kCatalog025iFormat WE HAVE A PRO FILE
	 (b) A "PHASE ONE" FILE, FILE FOOTER
	 */
	
	gCatFormat = EndianU32_BtoN(gCatFormat);
	myErr = ConfirmFormat(gfp);
	gFileCount = ( myErr == noErr ) ? EndianU32_BtoN(gFileCount): 0;

bail:
	if( status )
		*status = myErr;
	if( fileCount )
		*fileCount = gFileCount;
	
	if( myErr && gfp )
	{
		fclose(gfp);
		gfp = nil;
	}
}

////
void IVCReport(void *inClientInfo, DataFeed inDataFeed, SInt16 *outStatus)
{
	outFeed = inDataFeed;
	outClientInfo = inClientInfo;
	
	OSErr myErr = noErr;
	
	if( gfp && gCatFormat == kCatalog025iFormat )
		myErr = ReadCatalog32BitChunks(gfp);
	else if( gfp && gCatFormat == kCatalog030iFormat )
		myErr = ReadCatalog64BitChunks(gfp);
	else
		myErr = parsingCantStartErr;

	if( outStatus )
		*outStatus = myErr;
}

////
void IVCClose(void)
{
	if( gfp )
		fclose(gfp);
	gfp = nil;
}

////
UInt8 IVCIsRepeatingField(const char *fieldName)
{
	return (!strcmp(fieldName, kIPTC_Keyword) ||
			!strcmp(fieldName, kIPTC_Category) ||
			!strcmp(fieldName, kIPTC_Scene) ||
			!strcmp(fieldName, kIPTC_People) ||
			!strcmp(fieldName, kIPTC_SubjectReference) ||

			!strcmp(fieldName, kPATH_KeywordTree) ||
			!strcmp(fieldName, kPATH_SetTree) ||

			!strcmp(fieldName, kUF_Definition)
			);
}


////
void dataFeed(const UInt32 uid, const char *fName, const UInt8 fieldType, void *data, const UInt32 strlen)
{
	UInt32 *	uint32	= (UInt32 *)data;
	SInt32 *	sint32	= (SInt32 *)data;
	SInt16 *	sint16	= (SInt16 *)data;

	char *		cstr;
	SInt32 		snum32;
	
	
	if( outFeed )
		switch( fieldType )
		{
			case text_outString:
				outFeed(outClientInfo, uid, fName, string_utf8, data);
				break;
				
			case text_utf16:
				if( strlen )
				{
					cstr = UTF8_FROM_UTF16(data, strlen);
					outFeed(outClientInfo, uid, fName, string_utf8, cstr);
					free(cstr);
				}
				break;
				
			case text_ascii:
				if( strlen )
				{
					cstr = UTF8_FROM_ASCII(data, strlen);
					outFeed(outClientInfo, uid, fName, string_utf8, cstr);
					free(cstr);
				}
				break;
				
			case text_utf8:
				if( strlen )
				{
					cstr = UTF8_FROM_UTF8(data, strlen);
					outFeed(outClientInfo, uid, fName, string_utf8, cstr);
					free(cstr);
				}
				break;
				
			case number_sint16N:
				snum32 = (SInt32)*sint16;
				outFeed(outClientInfo, uid, fName, number_sint32, &snum32);
				break;
				
			case number_sint16B:
				snum32 = EndianS16_BtoN(*sint16);
				outFeed(outClientInfo, uid, fName, number_sint32, &snum32);
				break;
				
			case number_uint32:
				*uint32 = EndianU32_BtoN(*uint32);
				outFeed(outClientInfo, uid, fName, fieldType, data);
				break;
				
			case number_sint32:
				*sint32 = EndianS32_BtoN(*sint32);
				outFeed(outClientInfo, uid, fName, fieldType, data);
				break;
				
			case number_rational:
				for(int i=0; i<2; i++, sint32++)
					*sint32 = EndianS32_BtoN(*sint32);
				outFeed(outClientInfo, uid, fName, fieldType, data);
				break;
				
			case number_rational3:
				for(int i=0; i<6; i++, sint32++)
					*sint32 = EndianS32_BtoN(*sint32);
				outFeed(outClientInfo, uid, fName, fieldType, data);
				break;
				
			case pict_data:
#define kOffsetToJPEGSize 172
#define kOffsetToJPEGData 214
				if( strlen > kOffsetToJPEGData )
				{
					// In late cases (since iView 3?), a JPEG is wrapped inside a PICT enclosure.
					// It has fixed offsets to the jpeg size and actual data.
					unsigned char *	pict = data;
					UInt32 *		len1 = (UInt32 *)&pict[kOffsetToJPEGSize];
					UInt32			len  = EndianS32_BtoN(*len1);
					
					if( strlen >= len + kOffsetToJPEGData )
					{
						unsigned char *	jpeg = data + kOffsetToJPEGData;
						
						// check this is really a jpeg
						if( jpeg[0] == 0xFF && jpeg[1] == 0xD8 )
						{
							// as there is no way in C to pass the length of data with the data itself,
							// we make a copy of data and prepend with a 32 bit (its size).
							unsigned char * feed = malloc(4 + len);
							memcpy(feed, &len, 4);
							memcpy(&feed[4], jpeg, len);
							outFeed(outClientInfo, uid, fName, data_feed, feed);
						}
					}
				}
				break;
		}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark -

////
static OSErr ConfirmFormat(FILE *fp)
{
	OSErr myErr = noErr;
	OSType confirmFormat = 0;
	UInt32 bytes = 4;
	size_t offset = (gCatFormat == kCatalog025iFormat) ? -8:-12;
	
	if((myErr = myfseek(fp, SEEK_END, offset)) ||
	   (myErr = myfread(fp, &bytes, &confirmFormat)))
		return myErr;

	confirmFormat = EndianU32_BtoN(confirmFormat);
	if( confirmFormat != gCatFormat )
		myErr = unsupportedVersionErr;
	
	return myErr;
}

////
static OSErr ReadCatalog32BitChunks(FILE *fp)
{
	OSErr		myErr = noErr;
	OSType		chunkTag;
	char *		chunkData;
	UInt32		chunkBytes;
	UInt32		offset;
	UInt32		bytes;
	
	// go to the contents offset
	bytes = 4;
	if((myErr = myfseek(fp, SEEK_END, -4)) ||
	   (myErr = myfread(fp, &bytes, &offset)) ||
	   (myErr = myfseek(fp, SEEK_SET, EndianU32_BtoN(offset))))
		return myErr;
	
	// read all sets until we hit chunkTag = kCatalog025iFormat or an error
	while( !myErr )
	{
		bytes = 4;
		if( (myErr = myfread(fp, &bytes, &chunkTag)) )
			break;
		chunkTag = EndianU32_BtoN(chunkTag);
		
		// Detect end of extras
		if( chunkTag == gCatFormat )
			break;
		
		// Get size of this xtra block
		if( (myErr = myfread(fp, &bytes, &chunkBytes)) )
			break;
		chunkBytes = EndianU32_BtoN(chunkBytes);	// MacOS to Native order
		
		// printf("\r--- %s\r", FourCC2Str(chunkTag));
		gCurrentChuckOffset = (UInt32) ftell(fp);
		
		switch( chunkTag )
		{
			case kCatalogUserFieldsTag:
				myErr = ReadAsPtr(fp, &chunkData, chunkBytes);
				if( !myErr )
				{
					UnflattenStart(chunkData, chunkBytes, unflattenUFieldProc);
					free(chunkData);
				}
				break;
				
			case kCatalogMorselsTag:
				// This contains all visible morsels + keywords tree + sets tree
				myErr = ReadAsPtr(fp, &chunkData, chunkBytes);
				if( !myErr )
				{
					ParseMorsels(chunkData, chunkBytes);
					free(chunkData);
				}
				break;
				
			case kCatalogFSMTag:
				myErr = ReadFolders(fp, "");
				break;
				
			case kCatalogCellListTag:
				if( !(myErr = ReadAsPtr(fp, &chunkData, chunkBytes)) )
				{
					long cellSize = sizeof(CellInfo32);
					myErr = ( gFileCount * cellSize == chunkBytes ) ?
						ReadFileCells(fp, (CellInfo32 *)chunkData, gFileCount):
						wrongFileCountErr;
					free(chunkData);
				}
				break;
				
			default:
				break;
		}

		if( !myErr )
			myErr = myfseek(fp, SEEK_SET, (SInt32) gCurrentChuckOffset + chunkBytes);
	}
	
	return myErr;
}

////
static OSErr ReadCatalog64BitChunks(FILE *fp)
{
	OSErr		myErr = noErr;
	OSType		chunkTag;
	char *		chunkData;
	UInt32		chunkBytes;
	UInt64		offset;
	UInt32		bytes;
	
	// go to the contents offset
	bytes = 8;
	if((myErr = myfseek(fp, SEEK_END, -8)) ||
	   (myErr = myfread(fp, &bytes, &offset)) ||
	   (myErr = myfseek(fp, SEEK_SET, EndianU64_BtoN(offset))))
		return myErr;
	
	// read all sets until we hit chunkTag = kCatalog025iFormat or an error
	while( !myErr )
	{
		bytes = 4;
		if( (myErr = myfread(fp, &bytes, &chunkTag)) )
			break;
		chunkTag = EndianU32_BtoN(chunkTag);
		
		// Detect end of extras
		if( chunkTag == gCatFormat )
			break;
		
		// Get size of this xtra block (tis is still 32 bit??)
		bytes = 4;
		if( (myErr = myfread(fp, &bytes, &chunkBytes)) )
			break;
		chunkBytes = EndianU32_BtoN(chunkBytes);	// MacOS to Native order
		
		// printf("\r--- %s\r", FourCC2Str(chunkTag));
		gCurrentChuckOffset = (UInt32) ftell(fp);
		
		switch( chunkTag )
		{
			case kCatalogUserFieldsTag:
				myErr = ReadAsPtr(fp, &chunkData, chunkBytes);
				if( !myErr )
				{
					UnflattenStart(chunkData, chunkBytes, unflattenUFieldProc);
					free(chunkData);
				}
				break;
				
			case kCatalogMorselsTag:
				// This contains all visible morsels + keywords tree + sets tree
				myErr = ReadAsPtr(fp, &chunkData, chunkBytes);
				if( !myErr )
				{
					ParseMorsels(chunkData, chunkBytes);
					free(chunkData);
				}
				break;
				
			case kCatalogFSMTag:
				myErr = ReadFolders(fp, "");
				break;
				
			case kCatalogCellListTag:
				if( !(myErr = ReadAsPtr(fp, &chunkData, chunkBytes)) )
				{
					long cellSize = sizeof(CellInfo64);
					myErr = ( gFileCount * cellSize == chunkBytes ) ?
						ReadFileCells(fp, (CellInfo32 *)chunkData, gFileCount):
						wrongFileCountErr;
					free(chunkData);
				}
				break;
				
			default:
				break;
		}

		if( !myErr )
			myErr = myfseek(fp, SEEK_SET, (SInt32) gCurrentChuckOffset + chunkBytes);
	}
	
	return myErr;
}

////
static void ParseMorsels(char *masterData, UInt32 masterSize)
{
	UInt32	fieldTag;
	UInt32	bytes;
	UInt32	listSize;
	UInt32	bytesRead;
	
	if( !masterData	|| !masterSize )
		return;
	bytesRead = 1; // skip version
	
	while( bytesRead < masterSize )
	{
		// Field tag : 4 bytes
		memcpy(&fieldTag, &masterData[bytesRead], bytes = 4);
		fieldTag = EndianU32_BtoN(fieldTag);
		bytesRead += bytes;
		
		// Ignore : 2 bytes
		bytes = 2;
		bytesRead += bytes;
		
		// morsel data size : 4 bytes
		memcpy(&listSize, &masterData[bytesRead], bytes = 4);
		listSize = EndianU32_BtoN(listSize);
		bytesRead += bytes;
		
		// Process morsels (we only care for Catalog Sets :
		// In newer versions Catalog Sets also includes hierarchycal keywords tree
		// The root for this tree set is "@KeywordsSet"
		// printf("processing morsels with fieldTag %#010x (size %u)\r", fieldTag, (unsigned int)listSize);
		if( fieldTag == kCatalogSetField && listSize )
			UnflattenStart(&masterData[bytesRead], listSize, unflattenMorselProc);
		
		bytesRead += listSize;
	}
}

////
static char *unflattenMorselProc(char *path, void *inData, UInt32 inSize)
{
	UInt32 		nameBytes;
	UInt32		uidCount;		// number of item unique ids
	UInt32 *	uids = nil;		// list of unique ids
	char *		p = (char *)inData;
	
	// free space
	p+=32;
	
	// morsel id, 4 bytes (ignore)
	p+=4;
	
	// number of uids, 4 bytes, followed by the uids
	memcpy(&uidCount, p, 4);
	uidCount = EndianU32_BtoN(uidCount);
	p+=4;
	if( uidCount )
	{
		uids = (UInt32 *)p;
		p += (4 * uidCount);
	}
	
	////////////////////////////////////
	// Morsel names are always stored on disc in UTF16 format.
	// We'll convert to UTF8 before
	
	// name size, 4 bytes
	memcpy(&nameBytes, p, 4);
	nameBytes = EndianU32_BtoN(nameBytes);
	p+=4;
	char *name = UTF8_FROM_UTF16((UInt8 *)p, nameBytes);
	
	// dataFeed pathname for uids
	if( uidCount )
	{
		char pathString[10480];
		sprintf(pathString, "%s/%s", path, name);
		for(UInt32 i=0; i<uidCount; i++)
			dataFeed(EndianU32_BtoN(uids[i]), gMorselIsHK ? kPATH_KeywordTree: kPATH_SetTree, text_outString, pathString, 0);
	}
	
	return name;
}

////
static char *unflattenUFieldProc(char *path, void *inData, UInt32 inSize)
{
	// User field definitiona return withh uid = 0
	dataFeed(0, kUF_Definition, text_utf16, inData, inSize);
	return nil;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark - Folders

////
static OSErr ReadFolders(FILE *fp, char *inpath)
{
	char *folderName = nil;
	UInt32 *fileUids = nil;
	UInt32 *nameLengths = nil;
	UInt32 *subFolderOffsets = nil;
	
	
	char path[10240];
	
	data_chunk_header dch;
	folder_structure_header	fh;
	UInt32 bytes;
	
	OSErr myErr;
	
	
	////////////////////
	// read chunk header
	
	bytes = sizeof(dch);
	if((myErr = myfread(fp, &bytes, &dch)))
		goto bail;
	dch.tag					= EndianU32_BtoN(dch.tag);
	dch.skip_bytes			= EndianU32_BtoN(dch.skip_bytes);
	if(dch.tag != 'fldr')
	{
		myErr = FoldersChunkIdentifierError;
		goto bail;
	}
	
	
	////////////////////
	// read header
	
	bytes = sizeof(fh);
	if((myErr = myfread(fp, &bytes, &fh)))
		goto bail;
	fh.modern_name_length	= EndianU32_BtoN(fh.modern_name_length);
	fh.legacy_name_length	= EndianU32_BtoN(fh.legacy_name_length);
	fh.alias_length			= EndianU32_BtoN(fh.alias_length);
	fh.num_items			= EndianU32_BtoN(fh.num_items);
	fh.num_subfolders		= EndianU32_BtoN(fh.num_subfolders);
	fh.flags 				= EndianU32_BtoN(fh.flags);
	fh.parent_folder_offset = EndianU32_BtoN(fh.parent_folder_offset);
	fh.script_ref_offset	= EndianU32_BtoN(fh.script_ref_offset);
	
	
	////////////////////
	// read in unicode name UTF16 & Convert to UTF8
	
	if( fh.modern_name_length )
	{
		bytes = fh.modern_name_length;
		UInt8 *nameBuf = malloc(bytes);
		myfread(fp, &bytes, nameBuf);
		folderName = UTF8_FROM_UTF16(nameBuf, bytes);
		free(nameBuf);
	}
	
	
	////////////////////
	// read in legacy name
	// will ignore if we have modern_name_length
	
	if( fh.legacy_name_length )
	{
		bytes = fh.legacy_name_length;
		if( fh.modern_name_length )
		{
			if((myErr = myfseek(fp, SEEK_CUR, bytes)))
				goto bail;
		}
		else
		{
			folderName = calloc(bytes + 1, 1);
			myfread(fp, &bytes, folderName);
		}
	}
	
	if( folderName && strcmp(folderName, "<root>") )
	{
		strcpy(path, inpath);
		strcat(path, "/");
		strcat(path, folderName);
	}
	else
		path[0] = 0;
	
	////////////////////
	// read in alias (we don't need this)
	
	if( fh.alias_length )
	{
		bytes = fh.alias_length;
		if((myErr = myfseek(fp, SEEK_CUR, bytes)))
			goto bail;
	}
	
	
	////////////////////
	// read in file uids
	
	if( fh.num_items )
	{
		bytes = sizeof(UInt32) * fh.num_items;
		fileUids = (UInt32 *) malloc(bytes);
		if((myErr = myfread(fp, &bytes, fileUids)))
			goto bail;
		for(UInt32 i=0; i < fh.num_items; i++)
			fileUids[i] = EndianU32_BtoN(fileUids[i]);
	}
	
	//////////////////////
	// read in unicode file names
	
	if( fh.num_items )
	{
		bytes = sizeof(UInt32) * fh.num_items;
		nameLengths = (UInt32 *) malloc(bytes);
		if((myErr = myfread(fp, &bytes, nameLengths)))
			goto bail;
		for(UInt32 i=0; i < fh.num_items; i++)
		{
			bytes = EndianU32_BtoN(nameLengths[i]);
			
			char *fileNameBuffer = nil;
			myErr = ReadAsPtr(fp, &fileNameBuffer, bytes);
			if( myErr )
				goto bail;
			
			char *fileName = UTF8_FROM_UTF16((UInt8*)fileNameBuffer, bytes);
			char filePath[10280];
			strcpy(filePath, path);
			strcat(filePath, "/");
			strcat(filePath, fileName);
			
			free(fileName);
			free(fileNameBuffer);
			
			dataFeed(fileUids[i], kPATH_FileTree, text_outString, filePath, 0);
		}
	}
	
	///////////////////////////////
	// read in subfolders (recurse)
	
	if( fh.num_subfolders )
	{
		bytes = sizeof(UInt32) * fh.num_subfolders;
		subFolderOffsets = (UInt32 *) malloc(bytes);
		if((myErr = myfread(fp, &bytes, subFolderOffsets)))
			goto bail;
		for(UInt32 i=0; i < fh.num_subfolders; i++)
		{
			UInt32 subFolderOffset = EndianU32_BtoN(subFolderOffsets[i]);
			myErr = myfseek(fp, SEEK_SET, gCurrentChuckOffset + subFolderOffset);
			if( !myErr )
				myErr = ReadFolders(fp, path);
		}
	}
	
	
bail:
	if( folderName )		free(folderName);
	if( fileUids )			free(fileUids);
	if( nameLengths )		free(nameLengths);
	if( subFolderOffsets)	free(subFolderOffsets);
	
	return myErr;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark - File Items

////
static OSErr ReadFileCells(FILE *fp, void *buffer, UInt32 total)
{
	OSErr myErr = noErr;
	
	if( gCatFormat == kCatalog025iFormat )
	{
		CellInfo32 *ci = buffer;
		for(; total && !myErr; total--, ci++)
		{
			ci->catoffset	= EndianU32_BtoN(ci->catoffset);
			ci->uniqueID 	= EndianU32_BtoN(ci->uniqueID);
			
			SInt16 v;
			
			v = ci->flags.label;
			if( v )
				dataFeed(ci->uniqueID, kINFO_Label, number_sint16N, &v, 0);
			v = ci->flags.rating;
			if( v )
				dataFeed(ci->uniqueID, kINFO_Rating, number_sint16N, &v, 0);
			
			myErr = ReadFileBlocks(fp, ci->uniqueID, ci->catoffset);
		}
	}
	else
	{
		CellInfo64 *ci = buffer;
		for(; total && !myErr; total--, ci++)
		{
			ci->catoffset	= EndianU64_BtoN(ci->catoffset);
			ci->uniqueID 	= EndianU32_BtoN(ci->uniqueID);
			
			SInt16 v;
			
			v = ci->flags.label;
			if( v )
				dataFeed(ci->uniqueID, kINFO_Label, number_sint16N, &v, 0);
			v = ci->flags.rating;
			if( v )
				dataFeed(ci->uniqueID, kINFO_Rating, number_sint16N, &v, 0);
			
			myErr = ReadFileBlocks(fp, ci->uniqueID, ci->catoffset);
		}
	}
	
	return myErr;
}

////
static OSErr ReadFileBlocks(FILE *fp, UInt32 uniqueID, size_t offset)
{
	OSErr		myErr = noErr;
	UInt32		bytes;
	
	RecordCache cache = {0};
	
	bytes = sizeof(ItemInfo);
	if((myErr = myfseek(fp, SEEK_SET, offset)) ||
	   (myErr = myfread(fp, &bytes, &cache.info)) )
		return myErr;
	
	if( gWantsInfo )
		ParseBlockINFO(uniqueID, &cache.info);
	
	cache.info.talkSize = EndianS32_BtoN(cache.info.talkSize);
	cache.info.metaSize = EndianS32_BtoN(cache.info.metaSize);
	cache.info.pictSize = EndianS32_BtoN(cache.info.pictSize);
	cache.info.iptcSize = EndianS32_BtoN(cache.info.iptcSize);
	cache.info.urlfSize = EndianS32_BtoN(cache.info.urlfSize);

	///////
	// Thumbnail PICT
	if( gWantsPict && cache.info.pictSize )
	{
		Ptr data = nil;
		if((myErr = myfseek(fp, SEEK_SET, offset + 1024)) ||
		   (myErr = ReadAsPtr(fp, &data, cache.info.pictSize)))
			return myErr;
		dataFeed(uniqueID, kINFO_Thumbnail, pict_data, data, cache.info.pictSize);
		free(data);
	}
	
	///////
	// Iptc
	if( gWantsIptc && cache.info.iptcSize )
	{
		Ptr data = nil;
		if((myErr = myfseek(fp, SEEK_SET, offset + 1024 + cache.info.pictSize)) ||
		   (myErr = ReadAsPtr(fp, &data, cache.info.iptcSize)))
			return myErr;
		ParseBlockIPTC(uniqueID, data, cache.info.iptcSize);
		free(data);
	}
	
	///////
	// Surl (return as Iptc group)
	if( gWantsIptc && cache.info.urlfSize )
	{
		Ptr data = nil;
		if((myErr = myfseek(fp, SEEK_SET, offset + 1024 + cache.info.pictSize + cache.info.iptcSize)) ||
		   (myErr = ReadAsPtr(fp, &data, cache.info.urlfSize)))
			return myErr;
		dataFeed(uniqueID, kINFO_URLSource, text_ascii, data, bytes);
		free(data);
	}
	
	///////
	// Exif
	if( gWantsExif && cache.info.metaSize )
	{
		Ptr data = nil;
		if((myErr = myfseek(fp, SEEK_SET, offset + 1024 + cache.info.pictSize + cache.info.iptcSize + cache.info.urlfSize)) ||
		   (myErr = ReadAsPtr(fp, &data, cache.info.metaSize)))
			return myErr;
		ParseBlockEXIF(uniqueID, data, cache.info.metaSize);
		free(data);
	}
	
	return myErr;
}

////
static void ParseBlockINFO(const UInt32 uid, ItemInfo *in)
{
	/* if( in->fileSize > 0    ) */ dataFeed(uid, kINFO_FileSize     , number_uint32, &in->fileSize, 1);
	/* if( in->width > 0       ) */ dataFeed(uid, kINFO_Width        , number_sint32, &in->width, 1);
	/* if( in->height > 0      ) */ dataFeed(uid, kINFO_Height       , number_sint32, &in->height, 1);
	/* if( in->resolution > 0  ) */ dataFeed(uid, kINFO_Resolution   , number_sint32, &in->resolution, 1);
	/* if( in->depth > 0       ) */ dataFeed(uid, kINFO_Depth        , number_sint32, &in->depth, 1);
	/* if( in->duration > 0    ) */ dataFeed(uid, kINFO_Duration     , number_uint32, &in->duration, 1);
	/* if( in->colorProfile[0] ) */ dataFeed(uid, kINFO_ColorProfile , text_ascii, &in->colorProfile[1], in->colorProfile[0]);

	char *css = UTF8_FROM_ASCII((UInt8*)&in->colorSpace, 4);
	if( css[0] )
		dataFeed(uid, kINFO_ColorSpace 	 , text_outString, css, 1);
	free(css);

	//	dataFeed(uid, kINFO_Tracks 		 , number_sint32, &in->tracks, 1);
	//	dataFeed(uid, kINFO_Channels 	 , number_sint32, &in->channels, 1);
	//	dataFeed(uid, kINFO_SampleRate 	 , number_sint32, &in->sampleRate, 1);
	//	dataFeed(uid, kINFO_SampleSize 	 , number_sint32, &in->sampleSize, 1);

	//	EndianText_BtoN(&in->legacy_name[1], in->legacy_name[0]);
	//	EndianText_BtoN(&in->legacy_path[1], in->legacy_path[0]);
}

////
static void ParseBlockIPTC(const UInt32 uid, const Ptr buf, const long bufLen)
{
	CacheAtom 	d = {0};
	fip	*		m;
	Ptr			p = buf;
	UInt32		bufRead = 0;
	
	while( bufRead < bufLen )
	{
		if( *p == kIPTC_Encoding_TEXT || *p == kIPTC_Encoding_UTF8 )
		{
			m = (fip *) &p[1];
			
			d.tag = EndianU16_BtoN(m->tag);
			d.len = EndianU16_BtoN(m->len);
			d.buf = buf + bufRead + 5;
			d.enc = *p;
			
			////
			const char *fname = fieldName(d.tag);
			dataFeed(uid, fname, d.enc == kIPTC_Encoding_UTF8 ? text_utf8: text_ascii, d.buf, d.len);
		}
		// list has trailing garbage - ignore it
		else
			break;
		
		// advance to next tag
		p += (5 + d.len);
		bufRead += (5 + d.len);
	}
}

////
static void ParseBlockEXIF(const UInt32 uid, const Ptr buf, const UInt32 bufLen)
{
	Ptr			p = buf;
	UInt32		bufRead = 0;
	
	// We no longer support structures prior to iView MediaPro v1.0
	if( !buf || *p == 'C' ||  *p == 'I' || *p == 'M' )
		return;
	
	do
	{
		fud *m = (fud *) p;
		UInt32 l = EndianS32_BtoN(m->len);
		bufRead += l;
		if( !l || bufRead > bufLen || bufRead < 0)
			break;
		p += l;
		
		UInt32 tag = EndianU32_BtoN(m->tag);
		const char *fname = fieldName(tag);
		switch( tag )
		{
				// ASCII
				// These 4 fields are have 4 bytes at the beggining of the data buffer that are not used.
			case kEXIF_MakerField				:
			case kEXIF_ModelField				:
			case kEXIF_SoftwareField			:
			case kEXIF_FormatField				: dataFeed(uid, fname, text_ascii,       &m->buf[4], l-12); break;
				
			case kEXIF_LensField				: dataFeed(uid, fname, text_ascii,       m->buf, l-8); break;
				
			case kEXIF_MeteringModeField		:
			case kEXIF_ProgramField      		:
			case kEXIF_FocusModeField      		:
			case kEXIF_SensingMethodField      	:
			case kEXIF_LightSourceField      	:
			case kEXIF_FlashField      			:
			case kEXIF_ISOSpeedField      		: dataFeed(uid, fname, number_sint16B,    m->buf, 1); break;
				
			case kEXIF_ShutterSpeedField      	:
			case kEXIF_ApertureField		    :
			case kEXIF_FocusDistanceField      	:
			case kEXIF_FocalLengthField      	:
			case kEXIF_ExposureBiasField      	: dataFeed(uid, fname, number_rational,  m->buf, 1); break;
			case kEXIF_CaptureDateField			: dataFeed(uid, fname, number_uint32,    m->buf, 1); break;
				
			case kGPS_LatitudeField		      	:
			case kGPS_LongitudeField		    : dataFeed(uid, fname, number_rational3, m->buf, 1); break;
			case kGPS_LatitudeRefField			:
			case kGPS_LongitudeRefField			: dataFeed(uid, fname, text_ascii,       m->buf, l-8); break;
			case kGPS_AltitudeField		      	: dataFeed(uid, fname, number_rational,  m->buf, 1); break;
		}
	} while ( bufRead < bufLen );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark - Generic Linked List

////
static void UnflattenStart(char *flatData, UInt32 flatSize, ListUnflattenProc unflattenProc)
{
	if( flatSize && *flatData == kListStart )
	{
		++flatData;
		--flatSize;
		
		gMorselIsHK = false;
		UnflattenData("", flatData, flatSize, true, unflattenProc);
	}
}


////
static UInt32 UnflattenData(char* path, char* flatData, UInt32 flatSize, bool root, ListUnflattenProc unflattenProc)
{
	Boolean	done = false;
	UInt32	bytes = 0;
	UInt32	oldFlatSize = flatSize;
	UInt32	flags;
	UInt32	clientDataSize;
	char	tag = 0;
	
	UInt32	ll = 0;
	
	char pp[10280];
	strcpy(pp, path);
	
	while( !done && flatSize > 0 )
	{
		tag = *flatData;
		flatData += sizeof(tag);
		flatSize -= sizeof(tag);
		
		switch(tag)
		{
			case kListStart:
				bytes = UnflattenData(pp, flatData, flatSize, false, unflattenProc);
				flatData += bytes;
				flatSize -= bytes;
				break;
				
			case kNodeStart:
				// read flags
				memcpy(&flags, flatData, sizeof(UInt32));
				flags = EndianU32_BtoN(flags);
				flatData += sizeof(UInt32);
				flatSize -= sizeof(UInt32);
				
				// read data
				memcpy(&clientDataSize, flatData, sizeof(clientDataSize));
				clientDataSize = EndianU32_BtoN(clientDataSize);
				flatData += sizeof(clientDataSize);
				flatSize -= sizeof(clientDataSize);
				
				if( clientDataSize )
				{
					if( unflattenProc )
					{
						char *clientDataBuff = unflattenProc(pp, flatData, clientDataSize);
						if( clientDataBuff )
						{
							if( root && !strcmp(clientDataBuff, "@KeywordsSet") )
							{
								gMorselIsHK = true;
							}
							else
							{
								ll = (UInt32) strlen(pp);
								strcat(pp, "/");
								strcat(pp, clientDataBuff);
							}
							free(clientDataBuff);
						}
					}
				}
				
				flatData += clientDataSize;
				flatSize -= clientDataSize;
				break;
				
			case kNodeEnd:
				pp[ll] = '\0';
				break;
				
			case kListEnd:
				done = true;
				break;
		}
	}
	return oldFlatSize - flatSize;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark - File IO

////
static OSErr myfread(FILE *fp, UInt32 *len, void *val)
{
	size_t count = fread(val, *len, 1, fp);
	OSErr err = ( count == 1 ) ?  noErr: wrongBytesErr;
	return err;
}

////
static OSErr myfseek(FILE *fp, int ref, size_t len)
{
	int status = fseek(fp, len, ref);
	OSErr err = ( status == 0 ) ?  noErr: wrongOffsetErr;
	return err;
}

////
static OSErr ReadAsPtr(FILE *fp, Ptr *p, UInt32 databytes)
{
	OSErr myErr;
	
	*p = (Ptr)malloc(databytes);
	if( !*p )
	{
		myErr = memoryErr;
		goto bail;
	}
	
	myErr = myfread(fp, &databytes, *p);
	if( myErr )
	{
		free(*p);
		*p = nil;
	}
	
bail:
	return myErr;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark - Strings

////
static char *UTF8_FROM_UTF8(UInt8 *srcBuf, size_t srcLen)
{
	char * dstBuf = calloc(srcLen + 1, 1);
	memcpy(dstBuf, srcBuf, srcLen);
	return dstBuf;
}

////
static char *UTF8_FROM_UTF16(UInt8 *srcBuf, size_t srcLen)
{
	size_t dstLen = srcLen;
	char * dstBuf = calloc(dstLen + 1, 1);
	
	char *srcPtr = (char*) srcBuf;
	char *dstPtr = (char*) dstBuf;
	
	iconv_t cvt = iconv_open("UTF-8", "UTF-16");
	iconv(cvt, &srcPtr, &srcLen, &dstPtr, &dstLen);
	iconv_close(cvt);
	
	return dstBuf;
}

////
static char *UTF8_FROM_ASCII(UInt8 *srcBuf, size_t srcLen)
{
	// size_t tstLen = srcLen;
	
	size_t dstLen = 2 * srcLen;
	char * dstBuf = calloc(dstLen + 1, 1);
	
	char *srcPtr = (char*) srcBuf;
	char *dstPtr = (char*) dstBuf;
	
	// Use Mac Roman instead of ascii
	// Test string "Adobe Photoshop® 5.2", the '®' character is lost when using ASCII
	iconv_t cvt = iconv_open("UTF-8", "MACROMAN"); // ASCII");
	iconv(cvt, &srcPtr, &srcLen, &dstPtr, &dstLen);
	iconv_close(cvt);
	
	return dstBuf;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark - Field Names

////
static const char *fieldName(UInt32 tag)
{
	switch( tag )
	{
		case kIPTC_HeadlineField			: return kIPTC_Headline;
		case kIPTC_TitleField				: return kIPTC_Title;
		case kIPTC_PrimaryCategoryField		: return kIPTC_PrimaryCategory;
		case kIPTC_IntellectualGenreField	: return kIPTC_IntellectualGenre;
		case kIPTC_EventField				: return kIPTC_Event;
		case kIPTC_EventDateField			: return kIPTC_EventDate;
		case kIPTC_CreatorField				: return kIPTC_Creator;
		case kIPTC_CreatorTitleField		: return kIPTC_CreatorTitle;
		case kIPTC_CreatorAddressField  	: return kIPTC_CreatorAddress;
		case kIPTC_CreatorCityField			: return kIPTC_CreatorCity;
		case kIPTC_CreatorStateField		: return kIPTC_CreatorState;
		case kIPTC_CreatorPostcodeField		: return kIPTC_CreatorPostcode;
		case kIPTC_CreatorCountryField		: return kIPTC_CreatorCountry;
		case kIPTC_CreatorPhoneField		: return kIPTC_CreatorPhone;
		case kIPTC_CreatorEmailField		: return kIPTC_CreatorEmail;
		case kIPTC_CreatorURLField			: return kIPTC_CreatorURL;
		case kIPTC_CreditField				: return kIPTC_Credit;
		case kIPTC_SourceField				: return kIPTC_Source;
		case kIPTC_CopyrightField			: return kIPTC_Copyright;
		case kIPTC_TransmissionField		: return kIPTC_Transmission;
		case kIPTC_UsageTermsField			: return kIPTC_UsageTerms;
		case kIPTC_URLField					: return kIPTC_URL;
		case kIPTC_LocationField			: return kIPTC_Location;
		case kIPTC_CityField				: return kIPTC_City;
		case kIPTC_StateField				: return kIPTC_State;
		case kIPTC_CountryField				: return kIPTC_Country;
		case kIPTC_CountryCodeField			: return kIPTC_CountryCode;
		case kIPTC_InstructionsField		: return kIPTC_Instructions;
		case kIPTC_StatusField				: return kIPTC_Status;
		case kIPTC_CaptionWriterField		: return kIPTC_CaptionWriter;
		case kIPTC_CaptionField				: return kIPTC_Caption;
		case kIPTC_PeopleField				: return kIPTC_People;
		case kIPTC_KeywordField				: return kIPTC_Keyword;
		case kIPTC_CategoryField			: return kIPTC_Category;
		case kIPTC_SceneField				: return kIPTC_Scene;
		case kIPTC_SubjectReferenceField	: return kIPTC_SubjectReference;
			
		case kEXIF_MakerField				: return kEXIF_Maker;
		case kEXIF_ModelField				: return kEXIF_Model;
		case kEXIF_SoftwareField			: return kEXIF_Software;
		case kEXIF_CaptureDateField			: return kEXIF_CaptureDate;
		case kEXIF_ProgramField				: return kEXIF_Program;
		case kEXIF_ISOSpeedField			: return kEXIF_ISOSpeed;
		case kEXIF_ExposureBiasField		: return kEXIF_ExposureBias;
		case kEXIF_ShutterSpeedField		: return kEXIF_ShutterSpeed;
		case kEXIF_ApertureField			: return kEXIF_Aperture;
		case kEXIF_FocusDistanceField		: return kEXIF_FocusDistance;
		case kEXIF_MeteringModeField		: return kEXIF_MeteringMode;
		case kEXIF_LightSourceField			: return kEXIF_LightSource;
		case kEXIF_FlashField				: return kEXIF_Flash;
		case kEXIF_FocalLengthField			: return kEXIF_FocalLength;
		case kEXIF_SensingMethodField		: return kEXIF_SensingMethod;
		case kEXIF_FocusModeField			: return kEXIF_FocusMode;
		case kEXIF_LensField				: return kEXIF_Lens;
			
		case kGPS_LatitudeField		      	: return kGPS_Latitude;
		case kGPS_LongitudeField		    : return kGPS_Longitude;
		case kGPS_LatitudeRefField			: return kGPS_LatitudeRef;
		case kGPS_LongitudeRefField			: return kGPS_LongitudeRef;
		case kGPS_AltitudeField		      	: return kGPS_Altitude;

		case kUser01Field					: return kUF_01;
		case kUser02Field					: return kUF_02;
		case kUser03Field					: return kUF_03;
		case kUser04Field					: return kUF_04;
		case kUser05Field					: return kUF_05;
		case kUser06Field					: return kUF_06;
		case kUser07Field					: return kUF_07;
		case kUser08Field					: return kUF_08;
		case kUser09Field					: return kUF_09;
		case kUser10Field					: return kUF_10;
		case kUser11Field					: return kUF_11;
		case kUser12Field					: return kUF_12;
		case kUser13Field					: return kUF_13;
		case kUser14Field					: return kUF_14;
		case kUser15Field					: return kUF_15;
		case kUser16Field					: return kUF_16;
	}
	
	return "<?>";
}
