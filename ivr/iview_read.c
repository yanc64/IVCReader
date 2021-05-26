//
//  iview_read.c
//  ivr
//
//  Created by Yannis Calotychos on 19/5/21.
//

#include "iview_read.h"
#include "iview_defs.h"

// TODO: flags to label / rating
// TODO: separate hierarchycal keywords from sets
// TODO: remove CFString to make it cross platform

// Local cariables
static UInt32		gFileCount;
static UInt32		gCurrentChuckOffset;
static UInt32 		gUserFieldCount = 0;

#define kMaxOutStringLength	10480
static char			gOutString[kMaxOutStringLength];

static FILE *		gfp;
static DataFeed 	outFeed;

enum {
	text_outString, // null terminated
	text_utf8 = 100, text_ascii = 101
};



//////////////////////////////
// Forward Declarations		//
//////////////////////////////

// Chunks
static OSErr 		ReadCatalogChunks(FILE *fp);
static OSErr 		ReadCatalogExtraAsPtr(FILE *fp, Ptr *p, UInt32 databytes);
static void 		ParseMorsels(const char * masterData, UInt32 masterSize);
static char *		unflattenMorselProc(const char *path, const void *inData, UInt32 inSize);
static char *		unflattenUFListProc(const char *path, const void *data, const UInt32 size);

// File Cells
static OSErr 		ReadFolders(FILE *fp, char *path);

// File Cells
static OSErr 		ReadFileCells(FILE *fp, CellInfo *ci, UInt32 total);
static OSErr 		ReadFileBlocks(FILE *fp, CellInfo *ci);
static void 		ParseBlockInfo(const UInt32 uid, ItemInfo *in);
static void 		ParseBlockText(const UInt32 uid, const Ptr buf, const UInt32 len, OSType tag);
static void 		ParseBlockIPTC(const UInt32 uid, const Ptr buf, const long len);
static void 		ParseBlockEXIF(const UInt32 uid, const Ptr buf, const UInt32 len);

// Low Level
static OSErr 		myfread(FILE *fp, UInt32 *len, void *val);
static OSErr 		myfseek(FILE *fp, int ref, SInt32 len);

static char *		UTF16_TO_UTF8(UInt8 *buf, UInt32 bufLen);

static void 		UnflattenStart(const char *flatData, UInt32 flatSize, ListUnflattenProc unflattenProc);
static UInt32		UnflattenData(const char* path, const char* flatData, UInt32 flatSize, ListUnflattenProc unflattenProc);

static const char *	fieldName(UInt32 tag);


///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark -

////
void IVCOpen(char *filename, SInt16 *outTotal, SInt16 *outStatus)
{
	UInt32 total = 0;
	OSErr myErr = noErr;

	gfp = fopen(filename, "r");
	if( !gfp )
	{
		myErr = fileNotFoundErr;
		goto bail;
	}

	UInt32	bytes;
	UInt32	version;

	/////////////////
	// READ THE FILE HEADER
	bytes = 4;
	if((myErr = myfread(gfp, &bytes, &total)) ||
	   (myErr = myfread(gfp, &bytes, &version)) )
		goto bail;
	version = EndianU32_BtoN(version);
	total = EndianU32_BtoN(total);
	if( version != kCatalogFileFormat )
	{
		myErr = unsupportedVersionErr;
		goto bail;
	}
	
	/////////////////
	// DETERMINE IF THIS IS A "PRO" FILE, BY READING THE FILE FOOTER (2 LONGS)
	// IF THE 1ST OF THE 2 LONGS = kCatalogFileFormat WE HAVE A PRO FILE
	bytes = 4;
	version = 0;
	if((myErr = myfseek(gfp, SEEK_END, -8)) ||
	   (myErr = myfread(gfp, &bytes, &version)))
		goto bail;
	version = EndianU32_BtoN(version);
	if( version != kCatalogFileFormat )
	{
		myErr = unsupportedVersionErr;
		goto bail;
	}

	/////////////////
	// READ CATALOG EXTRAS
	gFileCount = total;

bail:
	if( outStatus )
		*outStatus = myErr;
	if( outTotal )
		*outTotal = total;
	
	if( myErr && gfp )
	{
		fclose(gfp);
		gfp = nil;
	}
}

////
void IVCReport(DataFeed inDataFeed, SInt16 *outStatus)
{
	outFeed = inDataFeed;
	
	OSErr myErr = gfp ? ReadCatalogChunks(gfp): parsingCantStartErr;
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
void dataFeed(const UInt32 uid, const char *fieldName, const UInt8 fieldType,
			   void *data, const UInt32 size)
{
	UInt32 *	uint32	= (UInt32 *)data;
	SInt32 *	sint32	= (SInt32 *)data;
	SInt16 *	sint16	= (SInt16 *)data;
	long 		l;

	if( outFeed )
		switch( fieldType )
		{
			case text_outString:
				outFeed(uid, fieldName, string_utf8, data);
				break;
				
			case text_utf8:
			case text_ascii:
				l = size >= kMaxOutStringLength - 1 ? kMaxOutStringLength - 1: size;
				memcpy(gOutString, data, l);
				gOutString[l] = 0;
				outFeed(uid, fieldName, string_utf8, gOutString);
				break;

			case number_sint16:
				*sint16 = EndianS16_BtoN(*sint16);
				outFeed(uid, fieldName, fieldType, data);
				break;
				
			case number_uint32:
				*uint32 = EndianU32_BtoN(*uint32);
				outFeed(uid, fieldName, fieldType, data);
				break;

			case number_sint32:
				*sint32 = EndianS32_BtoN(*sint32);
				outFeed(uid, fieldName, fieldType, data);
				break;
				
			case number_rational:
				for(int i=0; i<2; i++, sint32++)
					*sint32 = EndianS32_BtoN(*sint32);
				outFeed(uid, fieldName, fieldType, data);
				break;

			case number_rational3:
				for(int i=0; i<6; i++, sint32++)
					*sint32 = EndianS32_BtoN(*sint32);
				outFeed(uid, fieldName, fieldType, data);
				break;
		}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark -

////
static OSErr ReadCatalogChunks(FILE *fp)
{
	OSErr		myErr = noErr;
	OSType		chunkTag;
	char *		chunkData;
	UInt32		chunkBytes;
	UInt32		offset;
	UInt32		bytes;
	CellInfo *	cells;

	const UInt32 kCatalogCellListTag 	= 'CELL';
	const UInt32 kCatalogMorselsTag 	= 'CMRS';
	const UInt32 kCatalogUserFieldsTag 	= 'USF3';
	const UInt32 kCatalogFSMTag 		= 'FSM!';

	// go to the contents offset
	bytes = 4;
	if((myErr = myfseek(fp, SEEK_END, -4)) ||
	   (myErr = myfread(fp, &bytes, &offset)) ||
	   (myErr = myfseek(fp, SEEK_SET, EndianU32_BtoN(offset))))
		return myErr;

	// read all sets until we hit chunkTag = kCatalogFileFormat or an error
	while( !myErr )
	{
		bytes = 4;
		if( (myErr = myfread(fp, &bytes, &chunkTag)) )
			break;
		chunkTag = EndianU32_BtoN(chunkTag);
		
		// Detect end of extras
		if( chunkTag == kCatalogFileFormat )
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
				myErr = ReadCatalogExtraAsPtr(fp, &chunkData, chunkBytes);
				if( !myErr )
				{
					UnflattenStart(chunkData, chunkBytes, unflattenUFListProc);
					free(chunkData);
				}
				break;
				
			case kCatalogMorselsTag:
				// This contains all visible morsels + keywords tree + sets tree
				myErr = ReadCatalogExtraAsPtr(fp, &chunkData, chunkBytes);
				if( !myErr )
				{
					ParseMorsels(chunkData, chunkBytes);
					free(chunkData);
				}
				break;
				
			case kCatalogFSMTag:
				myErr = ReadFolders(fp, "");
				if( !myErr )
					myErr = myfseek(fp, SEEK_SET, (SInt32) gCurrentChuckOffset + chunkBytes);
				break;
				
			case kCatalogCellListTag:
				cells = malloc(chunkBytes);
				myErr = myfread(fp, &chunkBytes, cells);
				if( !myErr )
				{
					if( gFileCount != chunkBytes/sizeof(CellInfo) )
						myErr = wrongFileCountErr;
					else
						myErr = ReadFileCells(fp, cells, gFileCount);

					if( !myErr )
						myErr = myfseek(fp, SEEK_SET, (SInt32) gCurrentChuckOffset + chunkBytes);
				}
				break;
				
			default:
				// printf("\tignored\r");
				myErr = myfseek(fp, SEEK_CUR, chunkBytes);
				break;
		}
	}
	
	return myErr;
}

////
static OSErr ReadCatalogExtraAsPtr(FILE *fp, Ptr *p, UInt32 databytes)
{
	OSErr myErr;
	
	*p = (Ptr)malloc(databytes);
	if( !*p )
		myErr = memoryErr;
	else
	{
		myErr = myfread(fp, &databytes, *p);
	}
	
	return myErr;
}

////
static void ParseMorsels(const char *masterData, UInt32 masterSize)
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
static char *unflattenMorselProc(const char *path, const void *inData, UInt32 inSize)
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
	char *name = UTF16_TO_UTF8((UInt8 *)p, nameBytes);
	
	// dataFeed pathname for uids
	if( uidCount )
	{
		char *fieldName;
		sprintf(gOutString, "%s/%s", path, name);
		fieldName = "PATH_Sets";
			
		for(UInt32 i=0; i<uidCount; i++)
			dataFeed(EndianU32_BtoN(uids[i]), fieldName, text_outString, gOutString, 1);
	}
	
	return name;
}

////
static char *unflattenUFListProc(const char *path, const void *data, const UInt32 dataBytes)
{
	char *userField = UTF16_TO_UTF8((UInt8 *)data, dataBytes);
	dataFeed(gUserFieldCount++, "DEF_UserField", text_outString, userField, 1);
	free(userField);

	return nil;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark - Folders

////
static OSErr ReadFolders(FILE *fp, char *inpath)
{
	char *folderName = nil;
	char path[10240];
	
	data_chunk_header dch;
	folder_structure_header	fh;
	UInt32 bytes;
	
	UInt32 *fileUids = nil;
	UInt32 *nameLengths = nil;
	UInt32 *subFolderOffsets = nil;
	
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
		folderName = UTF16_TO_UTF8(nameBuf, bytes);
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
			char *fileNameBuffer = malloc(bytes);
			if((myErr = myfread(fp, &bytes, fileNameBuffer)))
				goto bail;
			
			char *fileName = UTF16_TO_UTF8((UInt8*)fileNameBuffer, bytes);
			char filePath[10280];
			strcpy(filePath, path);
			strcat(filePath, "/");
			strcat(filePath, fileName);
			free(fileName);

			dataFeed(fileUids[i], "FILE_Path", text_utf8, filePath, (UInt32) strlen(filePath));
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
	if( folderName )
		free(folderName);
	return myErr;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark - File Items

////
static OSErr ReadFileCells(FILE *fp, CellInfo *ci, UInt32 total)
{
	OSErr myErr = noErr;
	
	for(; total && !myErr; total--, ci++)
	{
		ci->catoffset	= EndianU32_BtoN(ci->catoffset);
		ci->uniqueID 	= EndianU32_BtoN(ci->uniqueID);

		myErr = ReadFileBlocks(fp, ci);
	}
	
	return myErr;
}

////
static OSErr ReadFileBlocks(FILE *fp, CellInfo *ci)
{
	UInt32		bytes;
	OSErr		myErr = noErr;
	UInt32 		offset;
	void *		bufIptc; 			// iptc block read buffer
	void *		bufMeta; 			// meta block read buffer
	void *		bufUrlf;			// urlf read buffer
									//	void *		bufPict; 			// pict read buffer
									//	void *		bufTalk;			// talk read buffer
	
	offset = ci->catoffset;
	RecordCache cache = {0};
	
	bytes = sizeof(ItemInfo);
	if((myErr = myfseek(fp, SEEK_SET, offset)) ||
	   (myErr = myfread(fp, &bytes, &cache.info)) )
		return myErr;
	ParseBlockInfo(ci->uniqueID, &cache.info);
	
	cache.info.talkSize = EndianS32_BtoN(cache.info.talkSize);
	cache.info.metaSize = EndianS32_BtoN(cache.info.metaSize);
	cache.info.pictSize = EndianS32_BtoN(cache.info.pictSize);
	cache.info.iptcSize = EndianS32_BtoN(cache.info.iptcSize);
	cache.info.urlfSize = EndianS32_BtoN(cache.info.urlfSize);

	///////
	// Iptc
	if((bytes = cache.info.iptcSize) > 0 &&
	   (myfseek(fp, SEEK_SET, offset + 1024 + cache.info.pictSize)) == noErr )
	{
		bufIptc = malloc(bytes);
		if( bufIptc && !myfread(fp, &bytes, bufIptc) )
			ParseBlockIPTC(ci->uniqueID, (Ptr)bufIptc, bytes);
	}
	
	///////
	// Meta
	if((bytes = cache.info.metaSize) > 0 &&
	   (myfseek(fp, SEEK_SET, offset + 1024 + cache.info.pictSize + cache.info.iptcSize + cache.info.urlfSize)) == noErr )
	{
		bufMeta = malloc(bytes);
		if( bufMeta && !myfread(fp, &bytes, bufMeta) )
			ParseBlockEXIF(ci->uniqueID, (Ptr)bufMeta, bytes);
	}
	
	///////
	// Surl
	if((bytes = cache.info.urlfSize) > 0 &&
	   (myfseek(fp, SEEK_SET, offset + 1024 + cache.info.pictSize + cache.info.iptcSize)) == noErr )
	{
		bufUrlf = malloc(bytes);
		if( bufUrlf && !myfread(fp, &bytes, bufUrlf) )
			ParseBlockText(ci->uniqueID, (Ptr)bufUrlf, bytes, kSourceURLField);
	}
	
	return noErr;
}

////
static void ParseBlockInfo(const UInt32 uid, ItemInfo *in)
{
//	in->importTicks				= EndianS32_BtoN(in->importTicks);
//	in->type					= EndianU32_BtoN(in->type);
//	in->show.tm_mode			= EndianS32_BtoN(in->show.tm_mode);
//	in->show.tm_secs			= EndianS32_BtoN(in->show.tm_secs);
//	in->fileSize				= EndianS32_BtoN(in->fileSize);

	dataFeed(uid, "INFO_width"       , number_sint32, &in->width, 1);
	dataFeed(uid, "INFO_height"      , number_sint32, &in->height, 1);
	dataFeed(uid, "INFO_resolution"  , number_sint32, &in->resolution, 1);
	dataFeed(uid, "INFO_depth"       , number_sint32, &in->depth, 1);

	dataFeed(uid, "INFO_pages"		 , number_sint32, &in->pages, 1);
	dataFeed(uid, "INFO_tracks"		 , number_sint32, &in->tracks, 1);
	dataFeed(uid, "INFO_duration"	 , number_sint32, &in->duration, 1);
	dataFeed(uid, "INFO_channels"	 , number_sint32, &in->channels, 1);
	dataFeed(uid, "INFO_sampleRate"	 , number_sint32, &in->sampleRate, 1);
	dataFeed(uid, "INFO_sampleSize"	 , number_sint32, &in->sampleSize, 1);

	dataFeed(uid, "INFO_vdFrameRate" , number_uint32, &in->vdFrameRate, 1);
	dataFeed(uid, "INFO_avDataRate"	 , number_uint32, &in->avDataRate, 1);
	dataFeed(uid, "INFO_colorSpace"	 , number_uint32, &in->colorSpace, 1);

	
//	in->sampleColor				= EndianU32_BtoN(in->sampleColor);
//	in->legacy_alias.type		= EndianS32_BtoN(in->legacy_alias.type);
//	in->legacy_alias.orig_size	= EndianS16_BtoN(in->legacy_alias.orig_size);
//	in->legacy_alias.size		= EndianS16_BtoN(in->legacy_alias.size);
//	in->rotate					= EndianU16_BtoN(in->rotate);
//	in->archived				= EndianU32_BtoN(in->archived);
//	in->created					= EndianU32_BtoN(in->created);
//	in->modified				= EndianU32_BtoN(in->modified);
//	in->annotated				= EndianU32_BtoN(in->annotated);
//	in->poster					= EndianS32_BtoN(in->poster);
//	in->pages					= EndianS32_BtoN(in->pages);
//	in->tracks					= EndianS32_BtoN(in->tracks);
//	in->duration				= EndianS32_BtoN(in->duration);
//	in->channels				= EndianS32_BtoN(in->channels);
//	in->sampleRate				= EndianS32_BtoN(in->sampleRate);
//	in->sampleSize				= EndianS32_BtoN(in->sampleSize);
//	in->vdFrameRate				= EndianU32_BtoN(in->vdFrameRate);
//	in->avDataRate				= EndianU32_BtoN(in->avDataRate);
//	in->colorSpace				= EndianU32_BtoN(in->colorSpace);
//	in->compression				= EndianS32_BtoN(in->compression);
//	in->textCharacters			= EndianS32_BtoN(in->textCharacters);
//	in->textParagraphs			= EndianS32_BtoN(in->textParagraphs);
//	in->vdQuality				= EndianU32_BtoN(in->vdQuality);
//	in->scratch					= EndianS32_BtoN(in->scratch);
//	in->talkSize				= EndianS32_BtoN(in->talkSize);
//	in->metaSize				= EndianS32_BtoN(in->metaSize);
//	in->pictSize				= EndianS32_BtoN(in->pictSize);
//	in->iptcSize				= EndianS32_BtoN(in->iptcSize);
//	in->urlfSize				= EndianS32_BtoN(in->urlfSize);
	
	//	EndianText_BtoN(&in->legacy_name[1], in->legacy_name[0]);
	//	EndianText_BtoN(&in->legacy_path[1], in->legacy_path[0]);
}

////
static void ParseBlockText(const UInt32 uid, const Ptr buf, const UInt32 len, OSType tag)
{
	const char *fname = fieldName(tag);
	dataFeed(uid, fname, text_ascii, buf, len);
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
		if( *p == kIPTCEncoding_TEXT || *p == kIPTCEncoding_UTF8 )
		{
			m = (fip *) &p[1];
			
			d.tag = EndianU16_BtoN(m->tag);
			d.len = EndianU16_BtoN(m->len);
			d.buf = buf + bufRead + 5;
			d.enc = *p;
			
			////
			const char *fname = fieldName(d.tag);
			dataFeed(uid, fname, d.enc == kIPTCEncoding_UTF8 ? text_utf8: text_ascii, d.buf, d.len);
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
			case kMetaMakerField				:
			case kMetaModelField				:
			case kMetaSoftwareField				:
			case kMetaFormatField				: dataFeed(uid, fname, text_ascii,       &m->buf[4], l-12); break;
				
			case kEXIFLensField					: dataFeed(uid, fname, text_ascii,       m->buf, l-8); break;

			case kEXIFMeteringModeField			:
			case kEXIFContrastField      		:
			case kEXIFSaturationField      		:
			case kEXIFSharpnessField      		:
			case kEXIFFocusModeField      		:
			case kEXIFProgramField      		:
			case kEXIFSensingMethodField      	:
			case kEXIFLightSourceField      	:
			case kEXIFFlashField      			:
			case kEXIFISOSpeedField      		:
			case kEXIFNoiseReductionField      	: dataFeed(uid, fname, number_sint16,    m->buf, 1); break;
				
			case kEXIFShutterSpeedField      	:
			case kEXIFDigitalZoomField      	:
			case kEXIFApertureField		      	:
			case kEXIFFocusDistanceField      	:
			case kEXIFFocalLengthField      	:
			case kEXIFExposureBiasField      	: dataFeed(uid, fname, number_rational,  m->buf, 1); break;
				
			case kEXIFCaptureDateField			: dataFeed(uid, fname, number_uint32,    m->buf, 1); break;

			case kGPSLatitudeField		      	:
			case kGPSLongitudeField		      	: dataFeed(uid, fname, number_rational3, m->buf, 1); break;
			case kGPSLatitudeRefField			:
			case kGPSLongitudeRefField			: dataFeed(uid, fname, text_ascii,       m->buf, l-8); break;
			case kGPSAltitudeField		      	: dataFeed(uid, fname, number_rational,  m->buf, 1); break;
		}
	} while ( bufRead < bufLen );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark - Generic Linked List

////
static void UnflattenStart(const char *flatData, UInt32 flatSize, ListUnflattenProc unflattenProc)
{
	if( flatSize && *flatData == kListStart )
	{
		++flatData;
		--flatSize;
		
		// bytes =
		UnflattenData("", flatData, flatSize, unflattenProc);
	}
}


////
static UInt32 UnflattenData(const char* path, const char* flatData, UInt32 flatSize, ListUnflattenProc unflattenProc)
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
				bytes = UnflattenData(pp, flatData, flatSize, unflattenProc);
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
							ll = (UInt32) strlen(pp);
							strcat(pp, "/");
							strcat(pp, clientDataBuff);
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
#pragma mark - Low Level

////
static OSErr myfread(FILE *fp, UInt32 *len, void *val)
{
	size_t count = fread(val, *len, 1, fp);
	OSErr err = ( count == 1 ) ?  noErr: wrongBytesErr;
	return err;
}

////
static OSErr myfseek(FILE *fp, int ref, SInt32 len)
{
	int status = fseek(fp, len, ref);
	OSErr err = ( status == 0 ) ?  noErr: wrongOffsetErr;
	return err;
}

////
static char *UTF16_TO_UTF8(UInt8 *buf, UInt32 bufLen)
{
	char *name = calloc(bufLen+2, 1);
	CFStringRef str = CFStringCreateWithBytesNoCopy(NULL, buf, bufLen, kCFStringEncodingUTF16, false, NULL);
	CFStringGetCString(str, name, bufLen, kCFStringEncodingUTF8);
	return name;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark - Labels

////
static const char *fieldName(UInt32 tag)
{
	switch( tag )
	{
		case kIPTCHeadlineField				: return "IPTC_Headline" ;
		case kIPTCTitleField				: return "IPTC_Title" ;
		case kIPTCPrimaryCategoryField		: return "IPTC_PrimaryCategory" ;
		case kIPTCIntellectualGenreField	: return "IPTC_IntellectualGenre";
		case kIPTCEventField				: return "IPTC_Event" ;
		case kIPTCEventDateField			: return "IPTC_EventDate" ;
		case kIPTCCreatorField				: return "IPTC_Creator" ;
		case kIPTCCreatorTitleField			: return "IPTC_CreatorTitle" ;
		case kIPTCCreatorAddressField  		: return "IPTC_CreatorAddress";
		case kIPTCCreatorCityField			: return "IPTC_CreatorCity";
		case kIPTCCreatorStateField			: return "IPTC_CreatorState";
		case kIPTCCreatorPostcodeField		: return "IPTC_CreatorPostcode";
		case kIPTCCreatorCountryField		: return "IPTC_CreatorCountry";
		case kIPTCCreatorPhoneField			: return "IPTC_CreatorPhone";
		case kIPTCCreatorEmailField			: return "IPTC_CreatorEmail";
		case kIPTCCreatorURLField			: return "IPTC_CreatorURL";
		case kIPTCCreditField				: return "IPTC_Credit" ;
		case kIPTCSourceField				: return "IPTC_Source" ;
		case kIPTCCopyrightField			: return "IPTC_Copyright" ;
		case kIPTCTransmissionField			: return "IPTC_Transmission" ;
		case kIPTCUsageTermsField			: return "IPTC_UsageTerms";
		case kIPTCURLField					: return "IPTC_URL" ;
		case kIPTCLocationField				: return "IPTC_Location" ;
		case kIPTCCityField					: return "IPTC_City" ;
		case kIPTCStateField				: return "IPTC_State" ;
		case kIPTCCountryField				: return "IPTC_Country" ;
		case kIPTCCountryCodeField			: return "IPTC_CountryCode";
		case kIPTCInstructionsField			: return "IPTC_Instructions" ;
		case kIPTCStatusField				: return "IPTC_Status" ;
		case kIPTCCaptionWriterField		: return "IPTC_CaptionWriter" ;
		case kIPTCCaptionField				: return "IPTC_Caption" ;
		case kIPTCPeopleField				: return "IPTC_People" ;
		case kIPTCKeywordField				: return "IPTC_Keyword" ;
		case kIPTCCategoryField				: return "IPTC_Category" ;
		case kIPTCSceneField				: return "IPTC_Scene";
		case kIPTCSubjectReferenceField		: return "IPTC_SubjectReference";
			
		case kMetaMakerField				: return "Meta_Maker" ;
		case kMetaModelField				: return "Meta_Model" ;
		case kMetaSoftwareField				: return "Meta_Software" ;
		case kMetaFormatField				: return "Meta_Format" ;
			
		case kSourceURLField				: return "URL_Source" ;
			
		case kEXIFVersionField				: return "EXIF_Version" ;
		case kEXIFCaptureDateField			: return "EXIF_CaptureDate" ;
		case kEXIFProgramField				: return "EXIF_Program" ;
		case kEXIFISOSpeedField				: return "EXIF_ISOSpeed" ;
		case kEXIFExposureBiasField			: return "EXIF_ExposureBias" ;
		case kEXIFShutterSpeedField			: return "EXIF_ShutterSpeed" ;
		case kEXIFApertureField				: return "EXIF_Aperture" ;
		case kEXIFFocusDistanceField		: return "EXIF_FocusDistance" ;
		case kEXIFMeteringModeField			: return "EXIF_MeteringMode" ;
		case kEXIFLightSourceField			: return "EXIF_LightSource" ;
		case kEXIFFlashField				: return "EXIF_Flash" ;
		case kEXIFFocalLengthField			: return "EXIF_FocalLength" ;
		case kEXIFSensingMethodField		: return "EXIF_SensingMethod" ;
		case kEXIFNoiseReductionField		: return "EXIF_NoiseReduction" ;
		case kEXIFContrastField				: return "EXIF_Contrast" ;
		case kEXIFSharpnessField			: return "EXIF_Sharpness" ;
		case kEXIFSaturationField			: return "EXIF_Saturation" ;
		case kEXIFFocusModeField			: return "EXIF_FocusMode" ;
		case kEXIFDigitalZoomField			: return "EXIF_DigitalZoom" ;
		case kEXIFLensField					: return "EXIF_Lens" ;
			
		case kGPSLatitudeField		      	: return "GPS_LatitudeField" ;
		case kGPSLongitudeField		      	: return "GPS_LongitudeField" ;
		case kGPSLatitudeRefField			: return "GPS_LatitudeRefField" ;
		case kGPSLongitudeRefField			: return "GPS_LongitudeRefField" ;
		case kGPSAltitudeField		      	: return "GPS_AltitudeField" ;
			
		case kUser1Field					: return "USER01_Field" ;
		case kUser2Field					: return "USER02_Field" ;
		case kUser3Field					: return "USER03_Field" ;
		case kUser4Field					: return "USER04_Field" ;
		case kUser5Field					: return "USER05_Field" ;
		case kUser6Field					: return "USER06_Field" ;
		case kUser7Field					: return "USER07_Field" ;
		case kUser8Field					: return "USER08_Field" ;
		case kUser9Field					: return "USER09_Field" ;
		case kUser10Field					: return "USER10_Field" ;
		case kUser11Field					: return "USER11_Field" ;
		case kUser12Field					: return "USER12_Field" ;
		case kUser13Field					: return "USER13_Field" ;
		case kUser14Field					: return "USER14_Field" ;
		case kUser15Field					: return "USER15_Field" ;
		case kUser16Field					: return "USER15_Field" ;
	}
	
	printf("uknown tag %#010x\r", tag);
	return "<?>";
}
