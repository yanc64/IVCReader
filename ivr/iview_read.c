//
//  iview_read.c
//  ivr
//
//  Created by Yannis Calotychos on 19/5/21.
//

//#include <CoreServices/CoreServices.h>
//#include <CarbonCore/CarbonCore.h>

#include "iview_read.h"
#include "iview_defs.h"

// Local cariables
static IVCFile *	gFileList;
static UInt32		gFileCount;
static char *		gUserFields[16] = {0};

static UInt32		currentChuckOffset;
static UInt32		folderLevel;
static UInt32		filesInFolders;

//////////////////////////////
// Forward Declarations		//
//////////////////////////////

static IVCFile *	findIVCFile(UInt32 uid);

// Chunks
static OSErr 		readCatalogChunks(FILE *fp);
static OSErr 		readCatalogExtraAsPtr(FILE *fp, Ptr *p, UInt32 databytes);
static void 		unflattenMorsels(const char * masterData, UInt32 masterSize);
static OSErr 		readFolders(FILE *fp);
static char *		unflattenUFListProc(const char *path, const void *data, const UInt32 size);
static char *		unflattenMorselProc(const char *path, const void *inData, UInt32 inSize);

// File Cells
static OSErr 		readCells(FILE *fp, CellInfo *ci, UInt32 total);
static OSErr 		readRecordCache(FILE *fp, CellInfo *ci);
static void 		itemInfoBtoN(ItemInfo *in);
static void 		ParseBlock(const UInt32 uid, const Ptr buf, const UInt32 len, OSType tag);
static void 		ParseBlockIPTC(const UInt32 uid, const Ptr buf, const long len);
static void 		ParseBlockEXIF(const UInt32 uid, const Ptr buf, const UInt32 len);
static void			metadataAtomBtoN(const char *fname, UInt32 tag, void *buf, UInt32 len);
static const char *	fieldName(UInt32 tag);

// Low Level
static OSErr 		myfread(FILE *fp, UInt32 *len, void *val);
static OSErr 		myfseek(FILE *fp, int ref, SInt32 len);
static void 		memoryToUTF8String(UInt8 *buf, UInt32 bufLen, char *cs, UInt32 maxOutLen);
static char *		memToUTF8String(const UInt8 *buf, const UInt32 bufLen);

static char *		memASCIIToString (const void *buf, const UInt32 bufLen);
static long *		memShortToNum    (const void *buf);

static void 		blockMoveData(void *src, void *dst, UInt32 len);
static void 		llUnflattenStart(const char *flatData, UInt32 flatSize, ListUnflattenProc unflattenProc);
static UInt32		llUnflattenData(const char* path, const char* flatData, UInt32 flatSize, ListUnflattenProc unflattenProc);


///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark -

////
void IVCOpen(char *filename, long *status)
{
	OSErr myErr = noErr;

	FILE *fp = fopen(filename, "r");
	if( !fp )
	{
		myErr = fileNotFoundErr;
		goto bail;
	}

	UInt32	bytes;
	UInt32	version;
	UInt32	total;

	/////////////////
	// READ THE FILE HEADER
	bytes = 4;
	if((myErr = myfread(fp, &bytes, &total)) ||
	   (myErr = myfread(fp, &bytes, &version)) )
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
	if((myErr = myfseek(fp, SEEK_END, -8)) ||
	   (myErr = myfread(fp, &bytes, &version)))
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
	gFileList = calloc(gFileCount, sizeof(IVCFile));
	myErr = readCatalogChunks(fp);

bail:
	if( fp )
		fclose(fp);
	*status = myErr;
}

////
void IVCClose(void)
{
#define _free(v) if(v)free(v)
	
	if( gFileList )
	{
		IVCFile *	f = gFileList;
		UInt32		i = 0;
		
		for(; i < gFileCount; i++, f++)
		{
			_free(f->MetaMakerField);
			_free(f->MetaModelField);
			_free(f->MetaSoftwareField);
			_free(f->MetaFormatField);
			
			_free(f->EXIFLensField);
			_free(f->EXIFMeteringModeField);
			_free(f->EXIFContrastField);
			_free(f->EXIFSaturationField);
			_free(f->EXIFSharpnessField);
			_free(f->EXIFFocusModeField);
			_free(f->EXIFProgramField);
			_free(f->EXIFSensingMethodField);
			_free(f->EXIFLightSourceField);
			_free(f->EXIFFlashField);
			_free(f->EXIFISOSpeedField);
			_free(f->EXIFNoiseReductionField);
		}
		_free(gFileList);
	}
	
	for(int i=0; i < 16; i++)
		_free(gUserFields[i]);
}

////
void logstr(const char *t, const char *s) { printf("\t%-30s = %s\r", t, s?s:"-"); }
void lognum(const char *t, const long *l) { if( l ) printf("\t%-30s = %ld\r", t, *l); else printf("\t%-30s = -\r", t); }

////
void IVCReport(void)
{
	IVCFile *	f = gFileList;
	UInt32		i = 0;
	
	printf("/////////////////////////\r");
	printf("//   C O N T E N T S   //\r");
	printf("/////////////////////////\r");
	printf("\r");

	for(; i < gFileCount; i++, f++)
	{
		printf("FILE %d (uid = %u)\r", i, (unsigned int)f->uid);
		printf("-------------------------\r");
		printf("\r");
		logstr("MetaMakerField", f->MetaMakerField);
		logstr("MetaModelField", f->MetaModelField);
		logstr("MetaSoftwareField", f->MetaSoftwareField);
		logstr("MetaFormatField", f->MetaFormatField);
		printf("\r");
		logstr("EXIFLensField", f->EXIFLensField);
		lognum("EXIFMeteringModeField", f->EXIFMeteringModeField);
		lognum("EXIFContrastField", f->EXIFContrastField);
		lognum("EXIFSaturationField", f->EXIFSaturationField);
		lognum("EXIFSharpnessField", f->EXIFSharpnessField);
		lognum("EXIFFocusModeField", f->EXIFFocusModeField);
		lognum("EXIFProgramField", f->EXIFProgramField);
		lognum("EXIFSensingMethodField", f->EXIFSensingMethodField);
		lognum("EXIFLightSourceField", f->EXIFLightSourceField);
		lognum("EXIFFlashField", f->EXIFFlashField);
		lognum("EXIFISOSpeedField", f->EXIFISOSpeedField);
		lognum("EXIFNoiseReductionField", f->EXIFNoiseReductionField);

		
		printf("\r");
	}
}

////
static IVCFile *findIVCFile(UInt32 uid)
{
	IVCFile *	f = gFileList;
	UInt32		i = 0;

	for(; i < gFileCount; i++, f++)
	{
		// return if found
		if( f->uid == uid )
			return f;
		
		// return next available slot
		if( f->uid == 0 )
		{
			f->uid = uid;
			return f;
		}
	}

	return nil;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark -

////
static OSErr readCatalogChunks(FILE *fp)
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
		
		printf("\r--- %s\r", FourCC2Str(chunkTag));
		currentChuckOffset = (UInt32) ftell(fp);

		switch( chunkTag )
		{
			case kCatalogUserFieldsTag:
				myErr = readCatalogExtraAsPtr(fp, &chunkData, chunkBytes);
				if( !myErr )
				{
					llUnflattenStart(chunkData, chunkBytes, unflattenUFListProc);
					free(chunkData);
				}
				break;
				
			case kCatalogMorselsTag:
				// This contains all visible morsels + keywords tree + sets tree
				myErr = readCatalogExtraAsPtr(fp, &chunkData, chunkBytes);
				if( !myErr )
				{
					unflattenMorsels(chunkData, chunkBytes);
					free(chunkData);
				}
				break;
				
			case kCatalogFSMTag:
				folderLevel = 0;
				filesInFolders = 0;
				myErr = readFolders(fp);
				if( !myErr )
					myErr = myfseek(fp, SEEK_SET, (SInt32) currentChuckOffset + chunkBytes);
				break;
				
			case kCatalogCellListTag:
				cells = malloc(chunkBytes);
				myErr = myfread(fp, &chunkBytes, cells);
				if( !myErr )
				{
					if( gFileCount != chunkBytes/sizeof(CellInfo) )
						myErr = wrongFileCountErr;
					else
						myErr = readCells(fp, cells, gFileCount);

					if( !myErr )
						myErr = myfseek(fp, SEEK_SET, (SInt32) currentChuckOffset + chunkBytes);
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
static OSErr readCatalogExtraAsPtr(FILE *fp, Ptr *p, UInt32 databytes)
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
static void unflattenMorsels(const char * masterData, UInt32 masterSize)
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
			llUnflattenStart(&masterData[bytesRead], listSize, unflattenMorselProc);

		bytesRead += listSize;
	}
}

////
static char *unflattenMorselProc(const char *path, const void *inData, UInt32 inSize)
{
	UInt32				bytes;
	UInt32				offset;
	UInt32 				ustrBytes;
	UInt32				mid;				// unique ID, field type for root nodes
	long				entries;			// number of item unique ids
	UInt32 *			fileIDs = nil;		// list of unique ids
	Ptr					p = (Ptr)inData;


	// free space for future compatibility (32 bytes)
	offset = 32;

	// morsel id (4 bytes)
	blockMoveData(&p[offset], &mid, bytes = 4);
	offset += bytes;
	mid = EndianU32_BtoN(mid);

	// number of entries (4 bytes)
	blockMoveData(&p[offset], &entries, bytes = 4);
	offset += bytes;
	entries = EndianU32_BtoN(entries);
	if( entries )
	{
		fileIDs = (UInt32 *) &p[offset];
		offset += (4 * entries);
	}

	// name size (4 bytes)
	blockMoveData(&p[offset], &ustrBytes, bytes = 4);
	offset += bytes;
	ustrBytes = EndianU32_BtoN(ustrBytes);
	
	// convert name
	char *n = memToUTF8String((UInt8 *)&p[offset], ustrBytes);

	
	printf("%s/%s\r", path, n);

	/*
	printf("morsel : %s\r", n);
	if( !entries )
		printf(" no items");
	else for(UInt32 i=0; i<entries; i++)
			printf(" [%u]", EndianU32_BtoN(fileIDs[i]));
	
	printf("\r");
	 */

	return n;
}

////
static char *unflattenUFListProc(const char *path, const void *data, const UInt32 size)
{
	int i = 0;
	for(; i < 16; i++)
		if( gUserFields[i] == nil )
		{
			gUserFields[i] = memToUTF8String((UInt8 *)data, size);
			printf("\t%s\r", gUserFields[i]);
			return gUserFields[i];
		}
	
	return nil;
}

////
static OSErr readFolders(FILE *fp)
{
	folderLevel++;

	data_chunk_header dch;
	folder_structure_header	fh;
	UInt32 bytes;
//	UInt32 bytes_read = 0;
	
	char *modernName = nil;
	char *legacyName = nil;
	UInt32 *fileUids = nil;
	UInt32 *nameLengths = nil;
	UInt32 *subFolderOffsets = nil;
	

	OSErr myErr;
	

	////////////////////
	// read chunk header

	bytes = sizeof(dch);
	myErr = myfread(fp, &bytes, &dch);
	if( myErr )
		goto bail;
//	bytes_read += bytes;
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
	myErr = myfread(fp, &bytes, &fh);
	if( myErr )
		goto bail;
//	bytes_read += bytes;
	fh.name_length			= EndianU32_BtoN(fh.name_length);
	fh.legacy_name_length	= EndianU32_BtoN(fh.legacy_name_length);
	fh.alias_length			= EndianU32_BtoN(fh.alias_length);
	fh.num_items			= EndianU32_BtoN(fh.num_items);
	fh.num_subfolders		= EndianU32_BtoN(fh.num_subfolders);
	fh.flags 				= EndianU32_BtoN(fh.flags);
	fh.parent_folder_offset = EndianU32_BtoN(fh.parent_folder_offset);
	fh.script_ref_offset	= EndianU32_BtoN(fh.script_ref_offset);


	////////////////////
	// read in unicode name

	if( fh.name_length )
	{
		bytes = fh.name_length;
		modernName = malloc(bytes);
		myErr = myfread(fp, &bytes, modernName);
		if( myErr )
			goto bail;

		char cs[4000];
		memoryToUTF8String((UInt8*)modernName, bytes, cs, 4000);
		
		printf("\r");
		for(int l=0;l<folderLevel;l++)printf("\t");
		printf("Reading Folder (modernName) '%s'\r", cs);
	}


	////////////////////
	// read in legacy name

	if( fh.legacy_name_length )
	{
		bytes = fh.legacy_name_length;
		if( fh.name_length )
		{
			myErr = myfseek(fp, SEEK_CUR, bytes);
			if( myErr )
				goto bail;
		}
		else
		{
			legacyName = malloc(bytes);
			myErr = myfread(fp, &bytes, legacyName);
			if( myErr )
				goto bail;

			char cs[4000];
			memoryToUTF8String((UInt8*)legacyName, bytes, cs, 4000);

			printf("\r");
			for(int l=0;l<folderLevel;l++)printf("\t");
			printf("Reading Folder (legacyName) '%s'\r", cs);
		}
	}


	////////////////////
	// read in alias (we don't need this)

	if( fh.alias_length )
	{
		bytes = fh.alias_length;
		myErr = myfseek(fp, SEEK_CUR, bytes);
		if( myErr )
			goto bail;
	}


	////////////////////
	// read in file uids

	if( fh.num_items )
	{
		for(int l=0;l<=folderLevel;l++)printf("\t");
		printf("item ids = ");
		bytes = sizeof(UInt32) * fh.num_items;
		fileUids = (UInt32 *) malloc(bytes);
		myErr = myfread(fp, &bytes, fileUids);
		if( myErr )
			goto bail;
		for(UInt32 i=0; i < fh.num_items; i++)
		{
			UInt32 uid = EndianU32_BtoN(fileUids[i]);
			printf("[%u] ", uid);
		}
		printf("\r");
	}

	//////////////////////
	// read in unicode file names
	
	if( fh.num_items )
	{
		for(int l=0;l<=folderLevel;l++)printf("\t");
		printf("item nms = ");
		bytes = sizeof(UInt32) * fh.num_items;
		nameLengths = (UInt32 *) malloc(bytes);
		myErr = myfread(fp, &bytes, nameLengths);
		for(UInt32 i=0; i < fh.num_items; i++)
		{
			bytes = EndianU32_BtoN(nameLengths[i]);
			char *fileName = malloc(bytes);
			myErr = myfread(fp, &bytes, fileName);
			if( myErr )
				goto bail;
			
			char cs[4000];
			memoryToUTF8String((UInt8*)fileName, bytes, cs, 4000);
			printf("[%s]", cs);
		}
	}

	filesInFolders += fh.num_items;
	
	///////////////////////////////
	// read in subfolders (recurse)
	
	if( fh.num_subfolders )
	{
		bytes = sizeof(UInt32) * fh.num_subfolders;
		subFolderOffsets = (UInt32 *) malloc(bytes);
		myErr = myfread(fp, &bytes, subFolderOffsets);
		for(UInt32 i=0; i < fh.num_subfolders; i++)
		{
			UInt32 subFolderOffset = EndianU32_BtoN(subFolderOffsets[i]);
			myErr = myfseek(fp, SEEK_SET, currentChuckOffset + subFolderOffset);
			if( !myErr )
				myErr = readFolders(fp);
		}
	}


bail:
	folderLevel--;

	return noErr;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark - File Items

////
static OSErr readCells(FILE *fp, CellInfo *ci, UInt32 total)
{
	OSErr myErr = noErr;
	
	for(; total && !myErr; total--, ci++)
	{
		ci->catoffset	= EndianU32_BtoN(ci->catoffset);
		ci->uniqueID 	= EndianU32_BtoN(ci->uniqueID);

		myErr = readRecordCache(fp, ci);
	}
	
	return myErr;
}

////
static OSErr readRecordCache(FILE *fp, CellInfo *ci)
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
	itemInfoBtoN(&cache.info);

	
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
			ParseBlock(ci->uniqueID, (Ptr)bufUrlf, bytes, kSourceURLField);
	}
	
	////////////
	// Thumbnail
	//	if( (bytes = cache.info.pictSize) > 0 &&
	//	   (myfseek(fp, SEEK_SET, offset + 1024)) == noErr )
	//	{
	//		bufPict = malloc(bytes);
	//		if( bufPict && !myfread(fp, &bytes, bufPict) )
	//			ParseBlock((Ptr)bufPict, bytes, &cache.atomArray, &cache.atomCount, 'PICT');
	//	}
	
	////////////
	// Recording
	//	if( (bytes = cache.info.talkSize) > 0 &&
	//	   (myfseek(fp, SEEK_SET, offset + 1024 + cache.info.pictSize + cache.info.iptcSize + cache.info.urlfSize + cache.info.metaSize)) == noErr )
	//	{
	//		bufTalk = malloc(bytes);
	//		if( bufTalk && !myfread(fp, &bytes, bufTalk) )
	//			ParseBlock((Ptr)bufTalk, bytes, &cache.atomArray, &cache.atomCount, 'TALK');
	//	}
	
	return noErr;
}

////
static void itemInfoBtoN(ItemInfo *in)
{
	in->importTicks				= EndianS32_BtoN(in->importTicks);
	in->type					= EndianU32_BtoN(in->type);
	in->show.tm_mode			= EndianS32_BtoN(in->show.tm_mode);
	in->show.tm_secs			= EndianS32_BtoN(in->show.tm_secs);
	in->fileSize				= EndianS32_BtoN(in->fileSize);
	in->width					= EndianS32_BtoN(in->width);
	in->height					= EndianS32_BtoN(in->height);
	in->resolution				= EndianS32_BtoN(in->resolution);
	in->sampleColor				= EndianU32_BtoN(in->sampleColor);
	in->depth					= EndianS32_BtoN(in->depth);
	in->legacy_alias.type		= EndianS32_BtoN(in->legacy_alias.type);
	in->legacy_alias.orig_size	= EndianS16_BtoN(in->legacy_alias.orig_size);
	in->legacy_alias.size		= EndianS16_BtoN(in->legacy_alias.size);
	in->rotate					= EndianU16_BtoN(in->rotate);
	in->archived				= EndianU32_BtoN(in->archived);
	in->created					= EndianU32_BtoN(in->created);
	in->modified				= EndianU32_BtoN(in->modified);
	in->pages					= EndianS32_BtoN(in->pages);
	in->poster					= EndianS32_BtoN(in->poster);
	in->tracks					= EndianS32_BtoN(in->tracks);
	in->duration				= EndianS32_BtoN(in->duration);
	in->channels				= EndianS32_BtoN(in->channels);
	in->sampleRate				= EndianS32_BtoN(in->sampleRate);
	in->sampleSize				= EndianS32_BtoN(in->sampleSize);
	in->annotated				= EndianU32_BtoN(in->annotated);
	in->vdFrameRate				= EndianU32_BtoN(in->vdFrameRate);
	in->avDataRate				= EndianU32_BtoN(in->avDataRate);
	in->vdQuality				= EndianU32_BtoN(in->vdQuality);
	in->colorSpace				= EndianU32_BtoN(in->colorSpace);
	in->compression				= EndianS32_BtoN(in->compression);
	in->textCharacters			= EndianS32_BtoN(in->textCharacters);
	in->textParagraphs			= EndianS32_BtoN(in->textParagraphs);
	in->scratch					= EndianS32_BtoN(in->scratch);
	in->talkSize				= EndianS32_BtoN(in->talkSize);
	in->metaSize				= EndianS32_BtoN(in->metaSize);
	in->pictSize				= EndianS32_BtoN(in->pictSize);
	in->iptcSize				= EndianS32_BtoN(in->iptcSize);
	in->urlfSize				= EndianS32_BtoN(in->urlfSize);
	
	//	EndianText_BtoN(&in->legacy_name[1], in->legacy_name[0]);
	//	EndianText_BtoN(&in->legacy_path[1], in->legacy_path[0]);
}

////
static void ParseBlock(const UInt32 uid, const Ptr buf, const UInt32 len, OSType tag)
{
	CacheAtom d;
	
	d.tag = tag;
	d.len = len;
	d.buf = buf;
	d.enc = 0;
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
			if( fname )
			{
				if( strcmp("URL", fname) )
					printf("");

				// bug in v3.
				// make sure we don't go over the limit
				if( d.len > kMaxICLTextBuffer )
					d.len = kMaxICLTextBuffer;
				
				char cs[kMaxICLTextBuffer] = {0};
				if( d.enc == kIPTCEncoding_UTF8 )
					memcpy(cs, d.buf, d.len);
				else
					memcpy(cs, d.buf, d.len);

				printf("IPTC > %s = %s\r", fname, cs);
			}
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
	fud *		m;
	UInt32		l;
	CacheAtom 	d = {0};
	Ptr			p = buf;
	UInt32		bufRead = 0;
	const char *fname;

	// We no longer support structures prior to iView MediaPro v1.0
	if( !buf || *p == 'C' ||  *p == 'I' || *p == 'M' )
		return;

	IVCFile *f = findIVCFile(uid);
	if( !f )
		return;

	do
	{
		m = (fud *) p;
		l = EndianS32_BtoN(m->len);
		bufRead += l;
		if( !l || bufRead > bufLen || bufRead < 0)
			break;
		p += l;
		
		///////////////////////////////////////////
		// store pointers to data and length of tag
		
		d.tag =  EndianU32_BtoN(m->tag);
		switch( d.tag )
		{
				// These 4 fields are UserAtomText and have 4 bytes
				// at the beggining of the data buffer that are not used.
				// We also check for NULL returns as we are at it.
			case kMetaMakerField				: f->MetaMakerField				= memASCIIToString((const UInt8 *)&m->buf[4], ( m->buf[l-9] == '\0' ) ? l-13: l-12); break;
			case kMetaModelField				: f->MetaModelField				= memASCIIToString((const UInt8 *)&m->buf[4], ( m->buf[l-9] == '\0' ) ? l-13: l-12); break;
			case kMetaSoftwareField				: f->MetaSoftwareField			= memASCIIToString((const UInt8 *)&m->buf[4], ( m->buf[l-9] == '\0' ) ? l-13: l-12); break;
			case kMetaFormatField				: f->MetaFormatField			= memASCIIToString((const UInt8 *)&m->buf[4], ( m->buf[l-9] == '\0' ) ? l-13: l-12); break;
				
				// ASCII
			case kEXIFLensField					: f->EXIFLensField				= memASCIIToString((const UInt8 *)&m->buf[0], l-8); break;

				// 1 SHORT
			case kEXIFMeteringModeField			: f->EXIFMeteringModeField		= memShortToNum(&m->buf[0]); break;
			case kEXIFContrastField      		: f->EXIFContrastField      	= memShortToNum(&m->buf[0]); break;
			case kEXIFSaturationField      		: f->EXIFSaturationField      	= memShortToNum(&m->buf[0]); break;
			case kEXIFSharpnessField      		: f->EXIFSharpnessField      	= memShortToNum(&m->buf[0]); break;
			case kEXIFFocusModeField      		: f->EXIFFocusModeField      	= memShortToNum(&m->buf[0]); break;
			case kEXIFProgramField      		: f->EXIFProgramField      		= memShortToNum(&m->buf[0]); break;
			case kEXIFSensingMethodField      	: f->EXIFSensingMethodField     = memShortToNum(&m->buf[0]); break;
			case kEXIFLightSourceField      	: f->EXIFLightSourceField      	= memShortToNum(&m->buf[0]); break;
			case kEXIFFlashField      			: f->EXIFFlashField      		= memShortToNum(&m->buf[0]); break;
			case kEXIFISOSpeedField      		: f->EXIFISOSpeedField      	= memShortToNum(&m->buf[0]); break;
			case kEXIFNoiseReductionField      	: f->EXIFNoiseReductionField    = memShortToNum(&m->buf[0]); break;


			default					:
				d.buf = &m->buf[0];
				d.len = l-8;
//				d.enc = 0;
//				metadataAtomBtoN(&d);
				fname = fieldName(d.tag);
				if( fname )
					metadataAtomBtoN(fname, d.tag, d.buf, d.len);
				break;
		}

		
	} while ( bufRead < bufLen );
}

////
static void metadataAtomBtoN(const char *fname, UInt32 tag, void *buf, UInt32 len)
{
	rational *		r;
	SInt16 *		s;
	UInt32 *		l;
//	long			i;
//	TrackInfo *		t;
//	CuePointInfo *	c;
//	long			n;
	
	switch(tag)
	{
			// 1 RATIONAL
		case kEXIFDigitalZoomField:
		case kEXIFShutterSpeedField:
		case kEXIFApertureField:
		case kEXIFFocusDistanceField:
		case kEXIFFocalLengthField:
		case kEXIFExposureBiasField:
		case kGPSAltitudeField:
			r = (rational *) buf;
			printf("META > %s = %d/%d\r", fname, EndianS32_BtoN(r->n), EndianS32_BtoN(r->d));
			break;
			
			// 3 RATIONALS
		case kGPSLatitudeField:
		case kGPSLongitudeField:
			// TODO: GPS (3 rationals, e.g 1/1 495984/10000 0/1))
			r = (rational *) buf;
			r->n = EndianS32_BtoN(r->n);
			r->d = EndianS32_BtoN(r->d);
			r++;
			r->n = EndianS32_BtoN(r->n);
			r->d = EndianS32_BtoN(r->d);
			r++;
			r->n = EndianS32_BtoN(r->n);
			r->d = EndianS32_BtoN(r->d);
			break;

			// ASCII or Binary ???
		case kGPSLatitudeRefField:
		case kGPSLongitudeRefField:
			// TODO: GPS (ASCII e.g. W)
			break;

			// ASCII or Binary ???
		case kEXIFCaptureDateField:
			l = (UInt32 *)buf;
			// LongDateRec dateRec = {0};
			// LongDateTime ldt = *l;
			// LongSecondsToDate(&ldt, &dateRec);
			printf("META > %s = %d (seconds)\r", fname, EndianU32_BtoN(*l));
			break;

			/* SKiP

			 // BINARY
		 case kJPEGPreviewField:
			 break;

			 // UNISGNED LONG
		case kEXIFVersionField:
			l = (UInt32 *)buf;
			printf("META > %s = %d\r", fname, EndianU32_BtoN(*l));
			break;

		case kTrackInfoListField: // n TrackInfo
			n = len / sizeof(TrackInfo);
			for(i=0, t=(TrackInfo *)buf; i<n; i++, t++)
			{
				t->preloadTime		= EndianS32_BtoN(t->preloadTime);
				t->preloadDuration	= EndianS32_BtoN(t->preloadDuration);
				t->preloadFlags		= EndianS32_BtoN(t->preloadFlags);
				t->defaultHints		= EndianS32_BtoN(t->defaultHints);
				t->mediaType		= EndianS32_BtoN(t->mediaType);
				t->offset_ts		= EndianS32_BtoN(t->offset_ts);
				t->duration_ts		= EndianS32_BtoN(t->duration_ts);
				t->dataSize			= EndianS32_BtoN(t->dataSize);
				t->dataRate			= EndianS32_BtoN(t->dataRate);
				t->fpms				= EndianS32_BtoN(t->fpms);
			}
			break;
			
		case kCuePointInfoListField: // n cue points
			n = len / sizeof(CuePointInfo);
			for(i=0, c=(CuePointInfo *)buf; i<n; i++, c++)
			{
				c->offset_ts = EndianU32_BtoN(c->offset_ts);
			}
			break;
			 */
	}
}

////
static const char *fieldName(UInt32 tag)
{
	switch( tag )
	{
		case kIPTCHeadlineField				: return "Headline" ;
		case kIPTCTitleField				: return "Product" ;
		case kIPTCPrimaryCategoryField		: return "Classification" ;
		case kIPTCIntellectualGenreField	: return "IntellectualGenre";
		case kIPTCEventField				: return "Fixture" ;
		case kIPTCEventDateField			: return "EventDate" ;
		case kIPTCCreatorField				: return "Author" ;
		case kIPTCCreatorTitleField			: return "AuthorTitle" ;
		case kIPTCCreatorAddressField  		: return "CreatorAddress";
		case kIPTCCreatorCityField			: return "CreatorCity";
		case kIPTCCreatorStateField			: return "CreatorState";
		case kIPTCCreatorPostcodeField		: return "CreatorPostcode";
		case kIPTCCreatorCountryField		: return "CreatorCountry";
		case kIPTCCreatorPhoneField			: return "CreatorPhone";
		case kIPTCCreatorEmailField			: return "CreatorEmail";
		case kIPTCCreatorURLField			: return "CreatorURL";
		case kIPTCCreditField				: return "Credit" ;
		case kIPTCSourceField				: return "Source" ;
		case kIPTCCopyrightField			: return "Copyright" ;
		case kIPTCTransmissionField			: return "Transmission" ;
		case kIPTCUsageTermsField			: return "Rights";
		case kIPTCURLField					: return "URL" ;
		case kIPTCLocationField				: return "Location" ;
		case kIPTCCityField					: return "City" ;
		case kIPTCStateField				: return "State" ;
		case kIPTCCountryField				: return "Country" ;
		case kIPTCCountryCodeField			: return "CountryCode";
		case kIPTCInstructionsField			: return "Instructions" ;
		case kIPTCStatusField				: return "Status" ;
		case kIPTCCaptionWriterField		: return "Writer" ;
		case kIPTCCaptionField				: return "Caption" ;
		case kIPTCPeopleField				: return "People" ;
		case kIPTCKeywordField				: return "Keyword" ;
		case kIPTCCategoryField				: return "Category" ;
		case kIPTCSceneField				: return "Scene";
		case kIPTCSubjectReferenceField		: return "SubjectReference";
		case kMetaMakerField				: return "Maker" ;
		case kMetaModelField				: return "Model" ;
		case kMetaSoftwareField				: return "Software" ;
		case kMetaFormatField				: return "Format" ;
		case kSourceURLField				: return "SourceURL" ;
		case kEXIFVersionField				: return "ExifVersion" ;
		case kEXIFCaptureDateField			: return "CaptureDate" ;
		case kEXIFProgramField				: return "ExposureProgram" ;
		case kEXIFISOSpeedField				: return "ISOSpeedRating" ;
		case kEXIFExposureBiasField			: return "ExposureBias" ;
		case kEXIFShutterSpeedField			: return "ExposureTime" ;
		case kEXIFApertureField				: return "Aperture" ;
		case kEXIFFocusDistanceField		: return "SubjectDistance" ;
		case kEXIFMeteringModeField			: return "MeteringMode" ;
		case kEXIFLightSourceField			: return "LightSource" ;
		case kEXIFFlashField				: return "Flash" ;
		case kEXIFFocalLengthField			: return "FocalLength" ;
		case kEXIFSensingMethodField		: return "SensingMethod" ;
		case kEXIFNoiseReductionField		: return "NoiseReduction" ;
		case kEXIFContrastField				: return "Contrast" ;
		case kEXIFSharpnessField			: return "Sharpness" ;
		case kEXIFSaturationField			: return "Saturation" ;
		case kEXIFFocusModeField			: return "FocusMode" ;
		case kEXIFDigitalZoomField			: return "DigitalZoom" ;
		case kEXIFLensField					: return "Lens" ;
		case kMergedLatitudeField			: return "Latitude" ;
		case kMergedLongitudeField			: return "Longitude" ;
		case kMergedAltitudeField			: return "Altitude" ;
	}
	
	return nil;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark - Generic Linked List

////
static void llUnflattenStart(const char *flatData, UInt32 flatSize, ListUnflattenProc unflattenProc)
{
	if( flatSize && *flatData == kListStart )
	{
		++flatData;
		--flatSize;
		
		// bytes =
		llUnflattenData("", flatData, flatSize, unflattenProc);
	}
	
	//	return list;
}


////
static UInt32 llUnflattenData(const char* path, const char* flatData, UInt32 flatSize, ListUnflattenProc unflattenProc)
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
//	printf("%s\r", pp);

	while( !done && flatSize > 0 )
	{
		tag = *flatData;
		flatData += sizeof(tag);
		flatSize -= sizeof(tag);
		
		switch(tag)
		{
			case kListStart:
				bytes = llUnflattenData(pp, flatData, flatSize, unflattenProc);
				flatData += bytes;
				flatSize -= bytes;
//				printf("%s\r", path);
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
//							printf("%s\r", pp);
						}
					}
				}
				
				flatData += clientDataSize;
				flatSize -= clientDataSize;
				break;
				
			case kNodeEnd:
				pp[ll] = '\0';
//				ll = strlen(pp);
				break;
				
			case kListEnd:
				done = true;
				break;
		}
	}
	return oldFlatSize - flatSize;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark - Strings

////
static char *memASCIIToString(const void *buf, const UInt32 bufLen)
{
	char *str = calloc(bufLen + 1, 1);
	memcpy(str, buf, bufLen);
	return str;
}

////
static long *memShortToNum(const void *buf)
{
	SInt16 *s = (SInt16 *)buf;
	long *n = calloc(1, sizeof(long));
	*n = EndianU16_BtoN(*s);
	return n;
}

////
static char *memToUTF8String(const UInt8 *buf, const UInt32 bufLen)
{
	CFStringEncoding encoding = kCFStringEncodingUTF8;
	if( bufLen > 0 )
	{
		if( bufLen>=2 && buf[0]==0xFF && buf[1]==0xFE )
			encoding = kCFStringEncodingUnicode;
		else if( bufLen>=2 && buf[0]==0xFE && buf[1]==0xFF )
			encoding = kCFStringEncodingUnicode;
	}
	
	CFStringRef str = CFStringCreateWithBytesNoCopy(NULL, buf, bufLen, encoding, false, NULL);
	CFIndex len = 2 * (CFStringGetLength(str) + 1); // storage is UTF 16 + 1 char for nil termination
	char *cs = calloc(len, 1);
	CFStringGetCString(str, cs, len, kCFStringEncodingUTF8);
	return cs;
}


////
static void memoryToUTF8String(UInt8 *buf, UInt32 bufLen, char *cs, UInt32 maxOutLen)
{
	CFStringEncoding encoding = kCFStringEncodingUTF8;
	if( bufLen > 0 )
	{
		if( bufLen>=2 && buf[0]==0xFF && buf[1]==0xFE )
			encoding = kCFStringEncodingUnicode;
		else if( bufLen>=2 && buf[0]==0xFE && buf[1]==0xFF )
			encoding = kCFStringEncodingUnicode;
	}
	
	CFStringRef str = CFStringCreateWithBytesNoCopy(NULL, buf, bufLen, encoding, false, NULL);
	CFStringGetCString(str, cs, maxOutLen, kCFStringEncodingUTF8);
	// CFRelease(str);
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
static void blockMoveData(void *src, void *dst, UInt32 len)
{
	memcpy(dst, src, len);
}
