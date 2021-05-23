//
//  iview_read.c
//  ivr
//
//  Created by Yannis Calotychos on 19/5/21.
//

#include "iview_read.h"
#include "iview_defs.h"

// Local cariables
static IVCFile *	gFileList;
static UInt32		gFileCount;

static MorselTree 	sets;
static LinkedList *	gUserFieldsList;
static UInt32		folderBlockOffset;
static UInt32		folderLevel;
static UInt32		filesInFolders;

// Forward Declarations
static OSErr 		readCatalogChunks(FILE *fp);
static OSErr 		readCatalogExtraAsPtr(FILE *fp, Ptr *p, UInt32 databytes);
	
static void 		cellInfoListBtoN(CellInfo *ci, UInt32 total);
static void 		readCells(FILE *fp, CellInfo *ci, UInt32 total);
static OSErr 		fileRead(FILE *fp, UInt32 *len, void *val);
static OSErr 		setFilePos(FILE *fp, int ref, SInt32 len);
static void 		unflattenMorsels(const char * masterData, UInt32 masterSize);
static OSErr 		readFolders(FILE *fp);
static void 		blockMoveData(void *src, void *dst, UInt32 len);
static void 		morselAlloc(MorselData *m, const long entriesWanted);
static void *		unflattenUFListProc(const void *data, const UInt32 size);
static void *		unflattenMorselProc(const void *inData, UInt32 inSize);
static OSErr 		readRecordCache(FILE *fp, CellInfo *ci);
static void 		makeFieldCacheIptcDictionary(const Ptr buf, const long len, CacheAtom **atomArray, UInt32 *atomCount);
static void 		makeFieldCacheMetaDictionary(const Ptr buf, const UInt32 len, CacheAtom **atomArray, UInt32 *atomCount);
static void 		makeFieldCacheBlobDictionary(const Ptr buf, const UInt32 len, CacheAtom **atomArray, UInt32 *atomCount, OSType tag);

static void 		itemInfoBtoN(ItemInfo *in);
static void 		metadataAtomBtoN(UInt32 tag, void *buf, UInt32 len);
static void 		memoryToUTF8String(UInt8 *buf, UInt32 bufLen, char *cs, UInt32 maxOutLen);

static LinkedList *	llUnflatten(const char *flatData, UInt32 flatSize, LLUnflattenProc unflattenProc);
static long			llUnflattenOneList(LinkedList *list, const char* flatData, UInt32 flatSize, LLUnflattenProc unflattenProc);



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
	if((myErr = fileRead(fp, &bytes, &total)) ||
	   (myErr = fileRead(fp, &bytes, &version)) )
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
	if((myErr = setFilePos(fp, SEEK_END, -8)) ||
	   (myErr = fileRead(fp, &bytes, &version)))
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
			_free(f->str);
		}
		_free(gFileList);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark -

////
static OSErr readCatalogChunks(FILE *fp)
{
	OSErr		myErr = noErr;
	OSType		chunkTag;
	UInt32		chunkBytes;
	UInt32		offset;
	char *		pTemp;
	UInt32		bytes;
	CellInfo *	cells;

	const UInt32 kCatalogCellListTag 	= 'CELL';
	const UInt32 kCatalogMorselsTag 	= 'CMRS';
	const UInt32 kCatalogUserFieldsTag 	= 'USF3';
	const UInt32 kCatalogFSMTag 		= 'FSM!';

	// go to the contents offset
	bytes = 4;
	if((myErr = setFilePos(fp, SEEK_END, -4)) ||
	   (myErr = fileRead(fp, &bytes, &offset)) ||
	   (myErr = setFilePos(fp, SEEK_SET, EndianU32_BtoN(offset))))
		return myErr;

	// read all sets until we hit chunkTag = kCatalogFileFormat or an error
	while( !myErr )
	{
		bytes = 4;
		if( (myErr = fileRead(fp, &bytes, &chunkTag)) )
			break;
		chunkTag = EndianU32_BtoN(chunkTag);
		
		// Detect end of extras
		if( chunkTag == kCatalogFileFormat )
			break;
		
		// Get size of this xtra block
		if( (myErr = fileRead(fp, &bytes, &chunkBytes)) )
			break;
		chunkBytes = EndianU32_BtoN(chunkBytes);	// MacOS to Native order
		
		printf("\r--- %s\r", FourCC2Str(chunkTag));
		
		switch( chunkTag )
		{
			case kCatalogUserFieldsTag:
				myErr = readCatalogExtraAsPtr(fp, &pTemp, chunkBytes);
				if( !myErr )
				{
					gUserFieldsList = llUnflatten((char *) pTemp, chunkBytes, unflattenUFListProc);
					free(pTemp);
				}
				break;
				
			case kCatalogMorselsTag:
				myErr = readCatalogExtraAsPtr(fp, &pTemp, chunkBytes);
				if( !myErr )
				{
					unflattenMorsels(pTemp, chunkBytes);
					free(pTemp);
				}
				break;
				
			case kCatalogFSMTag:
				folderBlockOffset = (UInt32) ftell(fp);
				folderLevel = 0;
				filesInFolders = 0;
				myErr = readFolders(fp);
				if( !myErr )
				{
					myErr = setFilePos(fp, SEEK_SET, (SInt32) folderBlockOffset + chunkBytes);
					long o1 = ftell(fp);
					if( o1 == folderBlockOffset + chunkBytes )
						printf("");
				}
				break;
				
			case kCatalogCellListTag:
				cells = malloc(chunkBytes);
				myErr = fileRead(fp, &chunkBytes, cells);
				if( !myErr )
				{
					//						cellInfoListBtoN(cells, xtraBytes/sizeof(CellInfo));
					//						readCells(fp, cells, xtraBytes/sizeof(CellInfo));
				}
				break;
				
			default:
				printf("\tignored\r");
				myErr = setFilePos(fp, SEEK_CUR, chunkBytes);
				break;
		}
	}
	
	return myErr;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark -


////
static void cellInfoListBtoN(CellInfo *ci, UInt32 total)
{
	for(; total; total--, ci++)
	{
		ci->catoffset	= EndianU32_BtoN(ci->catoffset);
		ci->uniqueID 	= EndianU32_BtoN(ci->uniqueID);
	}
}

////
static void readCells(FILE *fp, CellInfo *ci, UInt32 total)
{
	for(; total; total--, ci++)
		readRecordCache(fp, ci);
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
		myErr = fileRead(fp, &databytes, *p);
	}
	
	return myErr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark - File Low Level IO

////
static OSErr fileRead(FILE *fp, UInt32 *len, void *val)
{
	size_t count = fread(val, *len, 1, fp);
	OSErr err = ( count == 1 ) ?  noErr: wrongBytesErr;
	return err;
}

////
static OSErr setFilePos(FILE *fp, int ref, SInt32 len)
{
	int status = fseek(fp, len, ref);
	OSErr err = ( status == 0 ) ?  noErr: wrongOffsetErr;
	return err;
}



///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark -

////
static void unflattenMorsels(const char * masterData, UInt32 masterSize)
{
//	long		i;
	OSType		fieldTag;
	long		bytes;
	UInt32		listSize;
	Boolean		visible;
	Boolean		expanded;
	long		bytesRead;
	
	
	if( !masterData	|| !masterSize )
		return;
	bytesRead = 1; // skip version
	
	while( bytesRead < masterSize )
	{
		bytes = 4;
		memcpy(&fieldTag, &masterData[bytesRead], bytes);
		fieldTag = EndianU32_BtoN(fieldTag);
		bytesRead += bytes;
		
		bytes = 1;
		memcpy(&visible, &masterData[bytesRead], bytes);
		bytesRead += bytes;
		
		bytes = 1;
		memcpy(&expanded, &masterData[bytesRead], bytes);
		bytesRead += bytes;
		
		bytes = 4;
		memcpy(&listSize, &masterData[bytesRead], bytes);
		listSize = EndianU32_BtoN(listSize);
		bytesRead += bytes;
		
		
		if( fieldTag == kCatalogSetField )
		{
			printf("morsel list kCatalogSetField of size %u", (unsigned int)listSize);
			if( listSize )
			{
				sets.list = llUnflatten(&masterData[bytesRead], listSize, unflattenMorselProc);
			}
		}
		
		bytesRead += listSize;
	}
}

////
static void *unflattenMorselProc(const void *inData, UInt32 inSize)
{
	UInt32			bytes;
	UInt32			offset;
	UInt32			i;
	UInt32			l;
	UInt32			s;
	UInt32 			ustrBytes;
	Ptr				p = (Ptr)inData;
	MorselData *	m = (MorselData *)calloc(1, sizeof(MorselData));

	// add fixed entries
	offset = 32;
	
	blockMoveData(&p[offset], &l, bytes = 4); offset += bytes; m->mid     = EndianU32_BtoN(l);
	blockMoveData(&p[offset], &l, bytes = 4); offset += bytes; m->entries = EndianU32_BtoN(l);
	
	if( m->entries )
	{
		morselAlloc(m, m->entries);
		for(i=0; i<(UInt32)m->entries; i++)
		{
			blockMoveData(&p[offset], &l, bytes = 4);
			offset += bytes;
			if( m->uniqueID )
				m->uniqueID[i] = EndianU32_BtoN(l);
		}
	}
	else
		m->uniqueID = NULL;
	
	// add converted name
	blockMoveData(&p[offset], &s, bytes = 4); offset += bytes; ustrBytes = EndianU32_BtoN(s);
//	m->ustr = ( ustrBytes ) ? MyStringCreateFromFileBuffer(&p[offset], ustrBytes): NULL;
	
	return m;
}

////
static void blockMoveData(void *src, void *dst, UInt32 len)
{
	memcpy(dst, src, len);
}

/////
static void morselAlloc(MorselData *m, const long entriesWanted)
{
	const int kAllocThreshold = 100;

	if (m)
	{
		long alignedEntriesNeeded = ( !(entriesWanted % kAllocThreshold) ) ? entriesWanted: (1 + (entriesWanted/kAllocThreshold) ) * kAllocThreshold;
		m->uniqueID = (unsigned long *)(( !m->uniqueID ) ? malloc(alignedEntriesNeeded * 4): realloc(m->uniqueID, alignedEntriesNeeded * 4));
	}
}

////
static void *unflattenUFListProc(const void *data, const UInt32 size)
{
	UserField *u = (UserField *)malloc(sizeof(UserField));
	
//	u->ufname = CFStringCreateMutable(nil, 100);
//	strcpy(u->name, "something");
///	if( u )
//		u->ufname = MyStringCreateFromFileBuffer(data, size);
	
	char cs[4000];
	memoryToUTF8String((UInt8*)data, size, cs, 4000);
	printf("\t%s\r", cs);

	
	return u;
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
	RecordCache cache;
	
	UInt32 b = sizeof(ItemInfo);
	myErr = setFilePos(fp, SEEK_SET, offset);
	if( !myErr )
	{
		myErr = fileRead(fp, &b, &cache.info);
		if( !myErr )
			itemInfoBtoN(&cache.info);
	}
	if( myErr )
		return myErr;
		
	///////
	// Iptc
	if( (bytes = cache.info.iptcSize) > 0 &&
	   (setFilePos(fp, SEEK_SET, offset + 1024 + cache.info.pictSize)) == noErr )
	{
		bufIptc = malloc(bytes);
		if( bufIptc && !fileRead(fp, &bytes, bufIptc) )
			makeFieldCacheIptcDictionary((Ptr)bufIptc, bytes, &cache.atomArray, &cache.atomCount);
	}
	
	///////
	// Meta
	if( (bytes = cache.info.metaSize) > 0 &&
	   (setFilePos(fp, SEEK_SET, offset + 1024 + cache.info.pictSize + cache.info.iptcSize + cache.info.urlfSize)) == noErr )
	{
		bufMeta = malloc(bytes);
		if( bufMeta && !fileRead(fp, &bytes, bufMeta) )
			makeFieldCacheMetaDictionary((Ptr)bufMeta, bytes, &cache.atomArray, &cache.atomCount);
	}

	///////
	// Surl
	if( (bytes = cache.info.urlfSize) > 0 &&
	   (setFilePos(fp, SEEK_SET, offset + 1024 + cache.info.pictSize + cache.info.iptcSize)) == noErr )
	{
		bufUrlf = malloc(bytes);
		if( bufUrlf && !fileRead(fp, &bytes, bufUrlf) )
			makeFieldCacheBlobDictionary((Ptr)bufUrlf, bytes, &cache.atomArray, &cache.atomCount, kSourceURLField);
	}

	////////////
	// Thumbnail
//	if( (bytes = cache.info.pictSize) > 0 &&
//	   (setFilePos(fp, SEEK_SET, offset + 1024)) == noErr )
//	{
//		bufPict = malloc(bytes);
//		if( bufPict && !fileRead(fp, &bytes, bufPict) )
//			makeFieldCacheBlobDictionary((Ptr)bufPict, bytes, &cache.atomArray, &cache.atomCount, 'PICT');
//	}
	
	////////////
	// Recording
//	if( (bytes = cache.info.talkSize) > 0 &&
//	   (setFilePos(fp, SEEK_SET, offset + 1024 + cache.info.pictSize + cache.info.iptcSize + cache.info.urlfSize + cache.info.metaSize)) == noErr )
//	{
//		bufTalk = malloc(bytes);
//		if( bufTalk && !fileRead(fp, &bytes, bufTalk) )
//			makeFieldCacheBlobDictionary((Ptr)bufTalk, bytes, &cache.atomArray, &cache.atomCount, 'TALK');
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
static void makeFieldCacheIptcDictionary(const Ptr buf, const long len, CacheAtom **atomArray, UInt32 *atomCount)
{
	CacheAtom *	d;
	fip	*		m;
	Ptr			p			= buf;
	UInt32		bytesRead	= 0;
	UInt32		c			= *atomCount;
	
	if( buf )
		while( bytesRead < len )
		{
			if( *p == kIPTCEncoding_TEXT || *p == kIPTCEncoding_UTF8 )
			{
				if( c == 0 )
					*atomArray = (CacheAtom *)malloc(100 * sizeof(CacheAtom));
				else if( (c%100) == 0 )
					*atomArray = (CacheAtom *)realloc(*atomArray, (c+100) * sizeof(CacheAtom));
				
				if (!*atomArray)
				{
					c = 0;
					break;
				}
				
				d = &(*atomArray)[c];
				c++;
				
				m = (fip *) &p[1];
				
				d->tag = EndianU16_BtoN(m->tag);
				d->len = EndianU16_BtoN(m->len);
				d->buf = buf + bytesRead + 5;
				d->enc = *p;
				
				// bug in v3.
				// make sure we don't go over the limit
				if( d->len > kMaxICLTextBuffer )
					d->len = kMaxICLTextBuffer;
				
				// we only convert plain text - utf8 remains unchanged
				// if( *p == kIPTCEncoding_TEXT )
				//	EndianText_BtoN((unsigned char *) d->buf, d->len);
			}
			// list has trailing garbage - ignore it
			else
				break;
			
			// advance to next tag
			p += (5 + d->len);
			bytesRead += (5 + d->len);
		}
	
	*atomCount = c;
}

////
static void makeFieldCacheMetaDictionary(const Ptr buf, const UInt32 len, CacheAtom **atomArray, UInt32 *atomCount)
{
	fud *		m;
	UInt32		l;
	CacheAtom *	d;
	Ptr			p			= buf;
	UInt32		bytesRead	= 0;
	UInt32		c			= *atomCount;
	
	// We no longer support structures prior to iView MediaPro v1.0
	
	if( buf && *p != 'C' &&  *p != 'I' && *p != 'M' )
		do
		{
			m = (fud *) p;
			l = EndianS32_BtoN(m->len);
			bytesRead += l;
			if( !l || bytesRead > len || bytesRead < 0)
				break;
			p += l;
			
			if( c == 0 )
				*atomArray = (CacheAtom *)malloc(100 * sizeof(CacheAtom));
			else if( (c%100) == 0 )
				*atomArray = (CacheAtom *)realloc(*atomArray, (c+100) * sizeof(CacheAtom));
			
			if (!atomArray)
			{
				c = 0;
				break;
			}
			
			d = &(*atomArray)[c];
			c++;
			
			d->tag =  EndianU32_BtoN(m->tag);
			
			
			///////////////////////////////////////////
			// store pointers to data and length of tag
			
			switch( d->tag )
			{
				case kMetaMakerField	: // These 4 fields are UserAtomText and have 4 bytes
				case kMetaModelField	: // at the beggining of the data buffer that are not used.
				case kMetaSoftwareField	: // We also check for NULL returns as we are at it.
				case kMetaFormatField	: d->buf = &m->buf[4]; d->len = ( m->buf[l-9] == '\0' ) ? l-13: l-12; d->enc = kIPTCEncoding_TEXT; break;
				default					: d->buf = &m->buf[0]; d->len = l-8 ; d->enc = 0; metadataAtomBtoN(d->tag, d->buf, d->len); break;
			}
			
			
		} while ( bytesRead < len );
	
	*atomCount = c;
}

////
static void metadataAtomBtoN(UInt32 tag, void *buf, UInt32 len)
{
	rational *		r;
	short *			s;
	UInt32 *		l;
	long			i;
	TrackInfo *		t;
	CuePointInfo *	c;
	long			n;
	
	switch(tag)
	{
			// 1 SHORT
		case kEXIFContrastField:
		case kEXIFSaturationField:
		case kEXIFSharpnessField:
		case kEXIFFocusModeField:
		case kEXIFProgramField:
		case kEXIFSensingMethodField:
		case kEXIFLightSourceField:
		case kEXIFMeteringModeField:
		case kEXIFFlashField:
		case kEXIFISOSpeedField:
		case kEXIFNoiseReductionField:
			s = (SInt16 *) buf;
			*s = EndianS16_BtoN(*s);
			break;
			
			// UNISGNED LONG
		case kEXIFVersionField:
			l = (UInt32 *) buf;
			*l = EndianU32_BtoN(*l);
			break;
			
			// 1 RATIONAL
		case kEXIFDigitalZoomField:
		case kEXIFShutterSpeedField:
		case kEXIFApertureField:
		case kEXIFFocusDistanceField:
		case kEXIFFocalLengthField:
		case kEXIFExposureBiasField:
		case kGPSAltitudeField:
			r = (rational *) buf;
			r->n = EndianS32_BtoN(r->n);
			r->d = EndianS32_BtoN(r->d);
			break;
			
			// 3 RATIONALS
		case kGPSLatitudeField:
		case kGPSLongitudeField:
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
			
			// OTHER (ASCII or Binary)
		case kEXIFCaptureDateField:
		case kEXIFLensField:
		case kGPSLatitudeRefField:
		case kGPSLongitudeRefField:
		case kJPEGPreviewField:
		case kMetaMakerField:
		case kMetaModelField:
		case kMetaSoftwareField:
		case kMetaFormatField:
			break;
	}
}


////
static void makeFieldCacheBlobDictionary(const Ptr buf, const UInt32 len, CacheAtom **atomArray, UInt32 *atomCount, OSType tag)
{
	CacheAtom *	d;
	UInt32 		c = *atomCount;
	
	if( c == 0 )
		*atomArray = (CacheAtom *)malloc(100 * sizeof(CacheAtom));
	else if( (c%100) == 0 )
		*atomArray = (CacheAtom *)realloc(*atomArray, (c+100) * sizeof(CacheAtom));
	
	if (!*atomArray)
	{
		c = 0;
	} else {
		d = &(*atomArray)[c];
		c++;
		
		d->tag = tag;
		d->len = len;
		d->buf = buf;
		d->enc = 0;
	}
	
	*atomCount = c;
}


//////////////////////////////////////////////////////////////////////////////////////////////////
#pragma - Folders

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
	myErr = fileRead(fp, &bytes, &dch);
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
	myErr = fileRead(fp, &bytes, &fh);
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
		myErr = fileRead(fp, &bytes, modernName);
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
			myErr = setFilePos(fp, SEEK_CUR, bytes);
			if( myErr )
				goto bail;
		}
		else
		{
			legacyName = malloc(bytes);
			myErr = fileRead(fp, &bytes, legacyName);
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
		myErr = setFilePos(fp, SEEK_CUR, bytes);
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
		myErr = fileRead(fp, &bytes, fileUids);
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
		myErr = fileRead(fp, &bytes, nameLengths);
		for(UInt32 i=0; i < fh.num_items; i++)
		{
			bytes = EndianU32_BtoN(nameLengths[i]);
			char *fileName = malloc(bytes);
			myErr = fileRead(fp, &bytes, fileName);
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
		myErr = fileRead(fp, &bytes, subFolderOffsets);
		for(UInt32 i=0; i < fh.num_subfolders; i++)
		{
			UInt32 subFolderOffset = EndianU32_BtoN(subFolderOffsets[i]);
			myErr = setFilePos(fp, SEEK_SET, folderBlockOffset + subFolderOffset);
			if( !myErr )
				myErr = readFolders(fp);
		}
	}


bail:
	folderLevel--;

	return noErr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark - Strings

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

////
//CFMutableStringRef MyStringCreateFromFileBuffer(const void *data, const long size)
//{
//	CFMutableStringRef myStr = NULL;
//	CFDataRef cfDataRef = NULL;
//	CFStringRef tmpStr = NULL;
//	long encoding = kCFStringEncodingUTF8;
//	unsigned char *c = (unsigned char *) data;
//
//	if( size > 0 )
//	{
//		if( size>=3 && c[0]==0xEF && c[1]==0xBB && c[2]==0xBF )		encoding = kCFStringEncodingUTF8;
//		else if( size>=2 && c[0]==0xFF && c[1]==0xFE )				encoding = kCFStringEncodingUnicode;
//		else if( size>=2 && c[0]==0xFE && c[1]==0xFF )				encoding = kCFStringEncodingUnicode;
//	}
//
//
//	cfDataRef = CFDataCreate(NULL, c, size);
//	tmpStr = CFStringCreateFromExternalRepresentation(NULL, cfDataRef, kCFStringEncodingUTF8); // encoding);
//	if( tmpStr )
//		myStr = CFStringCreateMutableCopy(NULL, 0, tmpStr);
//	CFRelease(tmpStr);
//	CFRelease(cfDataRef);
//
//	return myStr;
//}


///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark - Generic Linked List

////
static LinkedList *_LL_New(void)
{
	LinkedList* newList = (LinkedList*)( calloc(sizeof(LinkedList), 1) );
	
	if (newList)
	{
		newList->tailNode	= (LinkedListNode*) newList;
		newList->headNode 	= (LinkedListNode*) newList;
		newList->isList 	= true;
	}
	
	return newList;
}

////
static LinkedListNode *_LL_AddNode(LinkedList *list, void *data, LinkedListNode* afterNode)
{
	LinkedListNode *beforeNode;
	LinkedListNode *oldMoveNodePrev;
	LinkedListNode *oldMoveNodeNext;
	LinkedListNode *newNode	= NULL;
	
	if( list == NULL )
		return NULL;
	
	newNode = (LinkedListNode*)( calloc(1, sizeof(LinkedListNode)) );
	if( !newNode )
		return NULL;
	
	newNode->data = data;
	
	if ( afterNode == kPrependInList)
	{
		afterNode = (LinkedListNode*)list;
	}
	else if ( afterNode == kAppendInList)
	{
		afterNode = list->tailNode;
	}
	
	// arrange nodes
	oldMoveNodePrev			= newNode->prevNode;
	oldMoveNodeNext			= newNode->nextNode;
	beforeNode 				= afterNode->nextNode;
	beforeNode->prevNode 	= newNode;
	afterNode->nextNode		= newNode;
	newNode->nextNode 		= beforeNode;
	newNode->prevNode 		= afterNode;
	// if newNode was embedded before then rearrange old position
	if( oldMoveNodePrev && oldMoveNodeNext )
	{
		oldMoveNodePrev->nextNode = oldMoveNodeNext;
		oldMoveNodeNext->prevNode = oldMoveNodePrev;
	}
	
	list->nodeCount++;
	
	return newNode;
}

////
static long llUnflattenOneList(LinkedList *list, const char* flatData, UInt32 flatSize, LLUnflattenProc unflattenProc)
{
	Boolean				done = false;
	LinkedListNode *	node = NULL;
	long				bytes = 0;
	long				oldFlatSize = flatSize;
	char				tag = 0;
	LinkedList *		pNewList;
	UInt32				flags;
	UInt32				clientDataSize;
	void *				clientDataBuff;
	
	while( !done && flatSize > 0 )
	{
		tag = *flatData;
		flatData += sizeof(tag);
		flatSize -= sizeof(tag);
		
		switch(tag)
		{
			case kListStart:
				pNewList = _LL_New();
				bytes = llUnflattenOneList(pNewList, flatData, flatSize, unflattenProc);
				flatData += bytes;
				flatSize -= bytes;
				
				node->childList = list;
				if( list )
					list->parentNode = node;
				
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
						clientDataBuff = unflattenProc(flatData, clientDataSize);
					}
					else
					{
						clientDataBuff = malloc(clientDataSize);
						if( clientDataBuff )
							memcpy(clientDataBuff, flatData, clientDataSize);
					}
				}
				else
					clientDataBuff = NULL;
				
				node = _LL_AddNode(list, clientDataBuff, kAppendInList);
				flatData += clientDataSize;
				flatSize -= clientDataSize;
				
				node->flags = flags;
				break;
				
			case kNodeEnd:
				node = NULL;
				break;
				
			case kListEnd:
				done = true;
				break;
		}
	}
	return oldFlatSize - flatSize;
}

////
static LinkedList *llUnflatten(const char *flatData, UInt32 flatSize, LLUnflattenProc unflattenProc)
{
	LinkedList *	list = _LL_New();
	// long			bytes = 0;
	
	if( flatSize && *flatData == kListStart )
	{
		++flatData;
		--flatSize;
		
		// bytes =
		llUnflattenOneList(list, flatData, flatSize, unflattenProc);
	}
	
	return list;
}
