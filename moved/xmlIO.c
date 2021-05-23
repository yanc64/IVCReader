//
// XML Import/Export Module
// Copyright 2001-2006 iView Multimedia Ltd. All rights reserved.
//

#include "stdafx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <myQD.h>
#include <myFiles.h>
#include <myText.h>
#include <myUnicode.h>
#include <myFormat.h>
#include <myCursor.h>
#include <mySound.h>
#include <myPictures.h>
#include <myStringSecure.h>
#include <IML.h>

#if TARGET_OS_MAC
#include <MoreFilesX.h>
#endif

#include "private.h"
#include "FolderStructureCAPI.h"

//// Internal Definitions
enum
{
	kDT_Str,
	kDT_UStr,
	kDT_ULong,
	kDT_Long,
	kDT_Duration,
	kDT_Label,
	kDT_Date,
	kDT_DateTime,
	kDT_ViewRotation,
	kDT_SampleColor,
	kDT_OSType,
	kDT_EndOf
};

static ConstStringPtr kOriginalsFolder		= (ConstStringPtr) "\xAOriginals_";
static ConstStringPtr kThumbnailsFolder		= (ConstStringPtr) "\xBThumbnails_";
static ConstStringPtr kRecordingsFolder		= (ConstStringPtr) "\xBRecordings_";

static const char* kCR = "\r";

typedef struct
{
	OSType 		tag;
	char		element[32];
} XMLPair;

typedef struct
{
	FSSpec		xmlFile;
	Str255		suffix;
	FSSpec		folderForOriginals;
	FSSpec		folderForThumbnails;
	FSSpec		folderForRecordings;

	FSSpec *	fileList;

	long 		folderForOriginalsDirID;
	long 		folderForThumbnailsDirID;
	long 		folderForRecordingsDirID;
} XMLData;


//// Internal prototypes
static void 	local_XMLWriteHeader				(FILE *fp);
static void 	local_XMLWriteDTD					(FILE *fp, const FSSpec *dtdFSSpec);
static OSErr 	local_XMLCreateFileList				(const CatalogPtr ch, XMLData *xmlData);
static OSErr 	local_XMLWriteCatalog				(FILE						*fp,
													 CatalogPtr					ch,
													 const XMLOptions			*xmlOptions,
													 XMLData					*xmlData,
		 											 const ProgressCallback		progressRoutine,		// can be NULL
											         const long					userRef					// refcon for progressRoutine
													 );
static void 	local_XMLWriteUserFieldDefinitions	(FILE *fp, CatalogPtr ch);
static OSErr 	local_XMLWriteCatalogMediaList		(FILE						*fp,
													 CatalogPtr					ch,
													 const XMLOptions			*xmlOptions,
													 XMLData					*xmlData,
		 											 const ProgressCallback		progressRoutine,		// can be NULL
											         const long					userRef					// refcon for progressRoutine
													);
static OSErr 	local_XMLWriteCatalogMedia			(FILE *fp, CatalogPtr ch, long id, const XMLOptions *xmlOptions, XMLData *xmlData);
static void		local_XMLWriteMediaThumbnail		(FILE *fp, long level, CatalogPtr ch, CellInfo *ci, long id, const XMLOptions *xmlOptions, XMLData *xmlData);
static void		local_XMLWriteMediaVoiceRecording	(FILE *fp, long level, CatalogPtr ch, CellInfo *ci, long id, const XMLOptions *xmlOptions, XMLData *xmlData);

static void		local_XMLWriteAsset					(Handle hData, const FSSpec *folderSpec, ConstStringPtr folderName, const char* tag, const char *extension, OSType filetype, OSType creator, FILE *fp, long level, long id, const XMLOptions *xmlOptions, XMLData *xmlData);
static OSErr	local_XMLWriteAssetProperties		(FILE *fp, CatalogPtr ch, long id, CellInfo *ci, ItemInfo *info, const XMLOptions *xmlOptions, XMLData *xmlData);
static void		local_XMLWriteMediaProperties		(FILE *fp, ItemInfo *info);
static void		local_XMLWriteAnnotations			(FILE *fp, CatalogPtr ch, CellInfo *ci);
static void		local_XMLWriteMediaUserFields		(FILE *fp, CatalogPtr ch, CellInfo *ci);
static void		local_XMLWriteMediaMetaData			(FILE *fp, CatalogPtr ch, CellInfo *ci);

static void 	local_XMLWriteCatalogSets			(FILE *fp, CatalogPtr ch);
static long		local_XMLWriteAllSets				(FILE *fp, CatalogPtr ch, long morselCount, long level, long* morselIndex);
//static long	local_XMLWriteSetContainer			(FILE *fp, CatalogPtr ch, long morselCount, long level, MorselRef mref);
//static void 	local_XMLWriteSet					(FILE *fp, CatalogPtr ch, MorselRef mref);

static OSErr	local_XMLWriteItemFileDetails		(FILE *fp, long level, long id, const XMLOptions *xmlOptions, XMLData *xmlData);
static OSErr	local_XMLWriteTagData				(FILE *fp, long level, const char *tag, long datatype, void *data, long dataLen);
static void 	local_XMLWriteTagLine				(FILE *fp, long level, const char *tag);
static OSErr	local_XMLCreateExportFoldersRelativeToFile(const XMLOptions *xmlOptions, XMLData *xmlData);

static void		local_GetFileNameDetails			(FSSpec *srcFile, FSRef *fsRef, Boolean posix, char *ustrPath, long *ulenPath, char *ustrName, long *ulenName);
#if TARGET_OS_MAC
static void		local_GetRelativePathMac			(HFSUniStr255 *destName, ConstStringPtr infolder, Boolean posix, char *ustrPath, long *ulenPath);
#else
static void		local_GetRelativePathWin			(ConstStringPtr destName, ConstStringPtr infolder, Boolean posix, char *ustrPath, long *ulenPath);
#endif

static Boolean local_XMLWriteMovieData(FILE *fp, CatalogPtr ch, CellInfo *ci);
static Boolean local_XMLWriteMovieChapterData(FILE *fp, CatalogPtr ch, CellInfo *ci);

static Boolean gCatalogExp = false;

//////////////////////////////////////////////////////////////////////////////////////////////////
//																								//
// Interface																					//
//																								//
//////////////////////////////////////////////////////////////////////////////////////////////////

OSErr ICL_XMLExport(const CatalogPtr			ch,
					FSSpec						*outFSSpec,
					const FSSpec				*dtdFSSpec,
					const XMLOptions			*xmlOptions,
					Boolean						catalogExp,
				    const ProgressCallback		progressRoutine,		// can be NULL
				    const long					userRef					// refcon for progressRoutine
				   )
{
	if ( NULL == ch )
		return paramErr;
		
	if ( NULL == outFSSpec )
		return paramErr;

	OSErr			myErr		= noErr;
	char 			cpath[1024]	= {0};
	FILE*			fp			= NULL;
	XMLData			xmlData		= {0};
	Str255			tmpName		= "\017tmp_xml.noindex"; // no need to localise
	
	gCatalogExp = catalogExp;
	
	verify_noerr( FSp_GetParentFolder(&xmlData.xmlFile, outFSSpec) );
	FSp_GetUnusedPNameInFolder(&xmlData.xmlFile, tmpName);
	(void)FSp_GetFileInFolderWithPName(&xmlData.xmlFile, tmpName, &xmlData.xmlFile);
	FSp_GetPathAsCString(&xmlData.xmlFile, cpath, 1023);
	//MySpinCursor(kSpinLoad);

	// ensure destination exists (we'll create a temp and swap into it)
	if( !FSp_GetFileExists(outFSSpec) )
	{
		myErr = FSpCreate(outFSSpec, 'R*ch', 'TEXT', smSystemScript);
		require_noerr(myErr, bail);
	}

	// make sure all files are accessible
	myErr = local_XMLCreateFileList(ch, &xmlData);
	if( myErr )
	{
		if( myErr != memFullErr )
			myErr = fnfErr;
		goto bail;
	}
	//MySpinCursor(kSpinRun);

	ICL_GetCatalogTitleAsPString( ch, xmlData.suffix, false );

	// create the originals folder
	if( local_XMLCreateExportFoldersRelativeToFile(xmlOptions, &xmlData) != noErr )
	{
		myErr = fLckdErr;
		goto bail;
	}

	fp = s_fopen(cpath, "w");
	if( fp )
	{
		local_XMLWriteHeader(fp);
		if( xmlOptions->addDTD )
			local_XMLWriteDTD(fp, dtdFSSpec);
		myErr = local_XMLWriteCatalog(fp, ch, xmlOptions, &xmlData, progressRoutine, userRef);
		fclose(fp);
	}	
	//require_noerr( myErr, bail);
	if(!myErr)
		myErr = FSp_SwapFilesCompat(&xmlData.xmlFile, outFSSpec);

	bail:
	// clean up
	(void)FSpDelete(&xmlData.xmlFile);
	if( myErr )
	{
		FSpDelete(outFSSpec);
	}

	// dispose internal memory
	FreeAndClear( xmlData.fileList );

	//MySpinCursor(kSpinKill);
	return myErr;
}


//////////////////////////////////////////////////////////////////////////////////////////////////
//																								//
// PREPARATION ROUTINES																			//
//																								//
//////////////////////////////////////////////////////////////////////////////////////////////////

#pragma mark -

static OSErr local_XMLCreateFileList(const CatalogPtr ch, XMLData *xmlData)
{
	long i;
	CellInfo *ci;
	//long count = ch->total.numTotalItems;
	long count = (gCatalogExp)? ch->total.numTotalItems : ICL_GetCatalogNumVisibleFiles(ch);
	OSErr myErr = noErr;
	FSSpec *pf;

	xmlData->fileList = (FSSpec *)malloc( count * sizeof(FSSpec) );
	if( !xmlData->fileList )
		return memFullErr;

	pf = xmlData->fileList;
	for(i=1/*, ci=GetCellInfoFromItemID(ch, 1)*/; i<=count; i++, /*ci++,*/ pf++)
	{
		if(gCatalogExp)
			ci = GetCellInfoFromItemID(ch, i);
		else
			ci = GetCellInfoFromVisibleID(ch,i);
		myErr = GetCellFile(ch, ci, pf);
		if( myErr )
			break;
	}

	return myErr;
}


////
static OSErr local_XMLCreateExportFoldersRelativeToFile(const XMLOptions* xmlOptions, XMLData* xmlData)
{
	OSErr myErr = noErr;

	// create the originals folder
	if ( ( noErr == myErr ) && xmlOptions->copyOriginals )
	{
		Str255 originalsFolderName = {0};
		CopyPascalStrings(  originalsFolderName, kOriginalsFolder );
		ConcatTextToString( originalsFolderName, "(", 1 );
		ConcatPascalString( originalsFolderName, xmlData->suffix );
		ConcatTextToString( originalsFolderName, ")", 1 );

		FSSpec folder1 = {0,0,{0}};
		FSp_GetFileInSameFolderWithPName( &xmlData->xmlFile, originalsFolderName, &folder1 );
		
		myErr = FSp_MakeFolder( folder1.vRefNum, folder1.parID, folder1.name, &xmlData->folderForOriginals );

		if ( noErr == myErr )
			myErr = FSp_DeleteFolderContents( &xmlData->folderForOriginals );
	}

	// create the thumbnails folder
	if ( ( noErr == myErr ) && xmlOptions->copyThumbnails )
	{
		Str255 thumbnailsFolderName = {0};
		CopyPascalStrings(  thumbnailsFolderName, kThumbnailsFolder );
		ConcatTextToString( thumbnailsFolderName, "(", 1 );
		ConcatPascalString( thumbnailsFolderName, xmlData->suffix );
		ConcatTextToString( thumbnailsFolderName, ")", 1 );

		FSSpec folder2 = {0,0,{0}};
		FSp_GetFileInSameFolderWithPName( &xmlData->xmlFile, thumbnailsFolderName, &folder2 );
		
		myErr = FSp_MakeFolder( folder2.vRefNum, folder2.parID, folder2.name, &xmlData->folderForThumbnails );
		
		if ( noErr == myErr )
			myErr = FSp_DeleteFolderContents( &xmlData->folderForThumbnails );
	}

	// create the recordings folder
	if ( ( noErr == myErr ) && xmlOptions->copyRecordings )
	{
		Str255 recordingsFolderName = {0};
		CopyPascalStrings(  recordingsFolderName, kRecordingsFolder );
		ConcatTextToString( recordingsFolderName, "(", 1 );
		ConcatPascalString( recordingsFolderName, xmlData->suffix );
		ConcatTextToString( recordingsFolderName, ")", 1 );

		FSSpec folder3 = {0,0,{0}};
		FSp_GetFileInSameFolderWithPName( &xmlData->xmlFile, recordingsFolderName, &folder3 );

		myErr = FSp_MakeFolder( folder3.vRefNum, folder3.parID, folder3.name, &xmlData->folderForRecordings );

		if ( noErr == myErr )
			myErr = FSp_DeleteFolderContents( &xmlData->folderForRecordings );
	}

	return myErr;
}


////
static void local_GetFileNameDetails(FSSpec *srcFile, FSRef *infsRef, Boolean posix, char *ustrPath, long *ulenPath, char *ustrName, long *ulenName)
{
#if TARGET_OS_MAC
	CFURLRef 			cfURLRef;
	CFStringRef			cfStringRef;
	CFMutableStringRef	cfmStringRef;
	CFRange				range;
	HFSUniStr255		fname;
	FSRef				thefsRef;
	FSRef *				fsRef = &thefsRef;

	if( infsRef )
		fsRef = infsRef;
	else
		FSpMakeFSRef(srcFile, fsRef);

	if( ustrPath )
	{

		cfURLRef = CFURLCreateFromFSRef(kCFAllocatorDefault, fsRef);

		cfmStringRef = CFStringCreateMutable(NULL, 0);

		if( posix )
		{
			cfStringRef = CFURLCopyScheme(cfURLRef);
			CFStringAppend(cfmStringRef, cfStringRef);
			CFStringAppendCString(cfmStringRef, "://", kCFStringEncodingASCII);
			CFRelease(cfStringRef);
		}

		cfStringRef = CFURLCopyFileSystemPath(cfURLRef, posix ? kCFURLPOSIXPathStyle: kCFURLHFSPathStyle);
		CFStringAppend(cfmStringRef, cfStringRef);
		CFRelease(cfStringRef);

		if( cfmStringRef )
		{
			range.location = 0;
			range.length = CFStringGetLength(cfmStringRef);
			CFStringGetBytes(cfmStringRef, range, kCFStringEncodingUTF8, 0, true, (unsigned char *) ustrPath, *ulenPath, ulenPath);
		}
		if( cfURLRef )
			CFRelease(cfURLRef);
		if( cfmStringRef )
			CFRelease(cfmStringRef);   
	}

	if( ustrName )
	{
		FSGetCatalogInfo(fsRef, kFSCatInfoNone, NULL, &fname, NULL, NULL);

		cfStringRef = CFStringCreateWithCharacters(NULL, fname.unicode, fname.length);
		if( cfStringRef )
		{
			range.location = 0;
			range.length = CFStringGetLength(cfStringRef);
			CFStringGetBytes(cfStringRef, range, kCFStringEncodingUTF8, 0, true, (unsigned char *) ustrName, *ulenName, ulenName);
		}
	}
#else
	OSErr err = noErr;
	HRESULT hr = 0;

	require(srcFile, bail);
	(void)&infsRef; // ignored

	if( ustrPath )
	{
		err = FSSpecToNativePathName(srcFile, ustrPath, *ulenPath, kFullNativePath);
		if( err == noErr && posix )
		{
			hr = UrlCreateFromPathA(ustrPath, ustrPath, (LPDWORD)ulenPath, 0);
			check(SUCCEEDED(hr));
		}
		*ulenPath = (long) strlen(ustrPath);
	}

	if( ustrName )
	{
		err = FSSpecToNativePathName(srcFile, ustrName, *ulenName, kFileNameOnly);
		*ulenName = (long) strlen(ustrName);
	}

bail:
	return;
#endif
}


#if TARGET_OS_MAC
////
static void local_GetRelativePathMac(HFSUniStr255 *destName, ConstStringPtr infolder, Boolean posix, char *ustrPath, long *ulenPath)
{
	if	(	( NULL != destName )
		&&	( NULL != ustrPath )
		&&	( NULL != ulenPath )
		)
	{
		CFStringRef s1 = CFStringCreateWithPascalString( kCFAllocatorDefault, infolder, kCFStringEncodingASCII );
		CFStringRef s2 = CFStringCreateWithPascalString( kCFAllocatorDefault, posix ? "\p/": "\p:", kCFStringEncodingASCII );
		CFStringRef s3 = CFStringCreateWithCharacters( kCFAllocatorDefault, destName->unicode, destName->length );

		CFMutableStringRef ms = CFStringCreateMutable( kCFAllocatorDefault, 0 );
		if ( NULL != ms )
		{
			if ( NULL != s1 )		CFStringAppend( ms, s1 );
			if ( NULL != s2 )		CFStringAppend( ms, s2 );
			if ( NULL != s3 )		CFStringAppend( ms, s3 );
		}

		CFReleaseClear( s1 );
		CFReleaseClear( s2 );
		CFReleaseClear( s3 );

		if ( NULL != ms )
		{
			CFRange range = { 0, CFStringGetLength( ms ) };
			CFStringGetBytes( ms, range, kCFStringEncodingUTF8, 0, true, (unsigned char*) ustrPath, *ulenPath, ulenPath );
		}

		CFReleaseClear( ms );
	}
}
#else
static void	 local_GetRelativePathWin(ConstStringPtr destName, ConstStringPtr infolder, Boolean posix, char *ustrPath, long *ulenPath)
{
	long len = StrLength(infolder);
	if( len + StrLength(destName) + 1 <= *ulenPath)
	{
		BlockMoveData(infolder+1, ustrPath, len);
		ustrPath[len++] = posix ? '/': '\\';
		BlockMoveData(destName+1, ustrPath + len, StrLength(destName));
		len += StrLength(destName);
		ustrPath[len] = 0;
		*ulenPath = len;
	}
}
#endif


//////////////////////////////////////////////////////////////////////////////////////////////////
//																								//
// WRITE																						//
//																								//
//////////////////////////////////////////////////////////////////////////////////////////////////

#pragma mark -

static void local_XMLWriteHeader(FILE *fp)
{
	fprintf(fp, "%s%s", "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\" ?>", kCR);
	fprintf(fp, "%s%s", "<?iview mediapro exportversion=\"3.0\" appversion=\"3.0\" ?>", kCR);
	fprintf(fp, kCR);
}


////
static void local_XMLWriteDTD(FILE *fp, const FSSpec *dtdFSSpec)
{
	long	bytes	= 0;
	Handle	hData	= NULL;

	if( FSp_ReadDataFork(dtdFSSpec, &hData, &bytes) == noErr )
	{
		HLock(hData);
		fwrite(*hData, bytes, 1, fp);
		DisposeHandle(hData);
	}
}


////
static OSErr local_XMLWriteCatalog(FILE							*fp,
								   CatalogPtr					ch,
								   const XMLOptions				*xmlOptions,
								   XMLData						*xmlData,
								   const ProgressCallback		progressRoutine,		// can be NULL
								   const long					userRef					// refcon for progressRoutine
								  )
{
	OSErr myErr;
	char tmpStr[256];
	MyString ustr = NULL;
	unsigned char *p = NULL;
	long strSize;

	local_XMLWriteTagLine(fp, 0, "CatalogType");


#if TARGET_OS_MAC
	sprintf(tmpStr,"Catalog pathType = \"%s\"",((xmlOptions->posixPaths)? "Posix" : "MAC"));
#else
	s_sprintf(tmpStr, _countof(tmpStr), "Catalog pathType = \"%s\"",((xmlOptions->posixPaths)? "Posix" : "DOS"));
#endif

	ustr = ICL_GetCatalogTitleAsMyString(ch, false);
	if( ustr )
	{
		strSize = 0;
		p = MyStringGetAsUTF8(ustr, &strSize, false);
		if( p && strSize )
		{
			local_XMLWriteTagData(fp, 1, tmpStr, kDT_UStr,  p, strSize);
			free(p);
		}
		MyStringDispose(&ustr);
	}

	// catalog comment is no served in utf16
	ustr = ICL_GetCatalogComment(ch);
	if( ustr && MyStringGetLength(ustr)) //comment could be "" 
	{
		strSize = 0;
		p = MyStringGetAsUTF8(ustr, &strSize, false);
		if( p && strSize )
		{	
			local_XMLWriteTagData(fp, 1, "CatalogComment", kDT_UStr, p, strSize);
			free(p);
		}
	}
	MyStringDispose(&ustr);

	if( xmlOptions->annotations )
		local_XMLWriteUserFieldDefinitions(fp, ch);

	myErr = local_XMLWriteCatalogMediaList(fp, ch, xmlOptions, xmlData, progressRoutine, userRef );
	local_XMLWriteCatalogSets(fp, ch);

	local_XMLWriteTagLine(fp, 0, "/CatalogType");

	return myErr;
}


////
static void local_XMLWriteUserFieldDefinitions(FILE *fp, CatalogPtr ch)
{
	long 			i;
	MyString		ustr;
	long 			count;
	long			strSize;
	unsigned char *	p;

	count = ICL_CountUserFields(ch);
	if( count )
	{
		local_XMLWriteTagLine(fp, 1, "UserFieldList");
		for(i=0; i<count; i++)
		{
			ustr = ICL_GetUserFieldDefinition(ch, kUser1Field+i);
			p = MyStringGetAsUTF8(ustr, &strSize, false);
			local_XMLWriteTagData(fp, 2, "UserFieldDefinition", kDT_UStr, p, strSize);
			free(p);
			MyStringDispose(&ustr);
		}
		local_XMLWriteTagLine(fp, 1, "/UserFieldList");
	}
}


////
static OSErr local_XMLWriteCatalogMediaList(FILE						*fp, 
											CatalogPtr					ch,
											const XMLOptions			*xmlOptions,
											XMLData						*xmlData,
											const ProgressCallback		progressRoutine,		// can be NULL
											const long					userRef					// refcon for progressRoutine
											)
{
	long i;
	//long count = ch->total.numTotalItems;
	OSErr myErr = noErr;

	//ICL_GetCatalogNumTotalFiles(ch);
	long count = (gCatalogExp)? ch->total.numTotalItems : ICL_GetCatalogNumVisibleFiles(ch); 
	if( count )
	{
		ProgressInit( progressRoutine, 0, count, userRef );
		
		if( !myErr )
			local_XMLWriteTagLine(fp, 1, "MediaItemList");

		for(i=1; i<=count && !myErr; i++)
		{
			myErr = ProgressRuns( progressRoutine, i, count, userRef );
			if(!myErr)
				myErr = local_XMLWriteCatalogMedia(fp, ch, i, xmlOptions, xmlData);
		}

		if( !myErr )
			local_XMLWriteTagLine(fp, 1, "/MediaItemList");

		ProgressStop( progressRoutine, count, count, userRef );
	}

	return myErr;
}


////
static OSErr local_XMLWriteCatalogMedia(FILE *fp, CatalogPtr ch, long id, const XMLOptions *xmlOptions, XMLData *xmlData)
{
	OSErr 		myErr;
	ItemInfo	info;
	CellInfo *	ci = (gCatalogExp)? GetCellInfoFromItemID(ch, id) : GetCellInfoFromVisibleID(ch,id); 

	GetCellItemInfo(ch, ci, &info);
	local_XMLWriteTagLine(fp, 2, "MediaItem");

	// Asset Properties
	myErr = local_XMLWriteAssetProperties(fp, ch, id, ci, &info, xmlOptions, xmlData);
	if( myErr )
		return myErr;

	// Media Properties
	if( xmlOptions->mediaProperties )
		local_XMLWriteMediaProperties(fp, &info);

	// AnnotationFields
	if( xmlOptions->annotations )
	{
		local_XMLWriteAnnotations(fp, ch, ci);
		local_XMLWriteMediaUserFields(fp, ch, ci);
	}

	// Meta Data
	if( xmlOptions->metaData )
	{
		local_XMLWriteMediaMetaData(fp, ch, ci);
		local_XMLWriteMovieData(fp, ch, ci);
		local_XMLWriteMovieChapterData(fp, ch, ci);
	}

	local_XMLWriteTagLine(fp, 2, "/MediaItem");

	return myErr;
}


////
static void local_XMLWriteAsset(	Handle hData, const FSSpec *folderSpec, ConstStringPtr folderName,
									const char* tag, const char *extension, OSType filetype, OSType creator,
									FILE *fp, long level, long id, const XMLOptions *xmlOptions, XMLData *xmlData)
{
	#if TARGET_OS_MAC
	{
		OSErr myErr = noErr;

		if ( NULL == xmlData )
			return;  // paramErr;

		FSSpec* srcFile = xmlData->fileList + ( id - 1 );

		FileCopyData fcd = {0};
		FSPrepareCopy( &fcd, srcFile, folderSpec, true, extension );
		
		FSSpec dstFile = {0,0,{0}};
		if ( folderName == kOriginalsFolder )
			myErr = FSp_GetFileInFolderWithPName( &xmlData->folderForOriginals, "\ptemp.noindex", &dstFile);
		else if( folderName == kThumbnailsFolder )
			myErr = FSp_GetFileInFolderWithPName( &xmlData->folderForThumbnails, "\ptemp.noindex", &dstFile);
		else if( folderName == kRecordingsFolder )
			myErr = FSp_GetFileInFolderWithPName( &xmlData->folderForRecordings, "\ptemp.noindex", &dstFile);
		else
			myErr = paramErr;
			
		if ( (noErr == myErr) || (fnfErr == myErr) )
		{
			HLock( hData );
			myErr = FSp_WriteDataFork( &dstFile, *hData, GetHandleSize(hData), creator, filetype, 0 );
			
			if ( noErr == myErr )
			{
				FSRef fsRef = {0};
				myErr = FSpMakeFSRef( &dstFile, &fsRef );
				
				if ( noErr == myErr )
				{
					myErr = FSRenameUnicode( &fsRef, fcd.dstName.length, fcd.dstName.unicode, kTextEncodingUnknown, &fcd.dstRef );

					if ( noErr == myErr )
					{
						char ustrPath[4096]	= {0};
						long ulenPath = sizeof( ustrPath );

						if ( folderName == kOriginalsFolder )
							local_GetRelativePathMac( &fcd.dstName, xmlData->folderForOriginals.name, xmlOptions->posixPaths, ustrPath, &ulenPath );
						else if( folderName == kThumbnailsFolder )
							local_GetRelativePathMac( &fcd.dstName, xmlData->folderForThumbnails.name, xmlOptions->posixPaths, ustrPath, &ulenPath );
						else if( folderName == kRecordingsFolder )
							local_GetRelativePathMac( &fcd.dstName, xmlData->folderForRecordings.name, xmlOptions->posixPaths, ustrPath, &ulenPath );

						local_XMLWriteTagData( fp, level, tag, kDT_UStr, ustrPath, ulenPath );
					}
				}
			}
		}
	}
	#else
	{
		FSSpec*			srcFile;
		FSSpec			dstFile;
		char			ustrPath[MAX_PATH];
		long			ulenPath = MAX_PATH;
		Str255			newName;

		srcFile = xmlData->fileList + ( id - 1 );
		FSp_GetFilenameAsPString(srcFile, newName, false);
		SetPNameExtensionFromCString(newName, extension);

		FSp_GetUnusedPNameInFolder(folderSpec, newName);
		FSp_GetFileInFolderWithPName(folderSpec, newName, &dstFile);

		HLock(hData);
		FSp_WriteDataFork(&dstFile, *hData, GetHandleSize(hData), creator, filetype, kOverwrite);
		FSp_GetFilenameAsPString(&dstFile, newName, false);

		Str255 dirName = {0};
		CopyPascalStrings(  dirName, folderName );
		ConcatTextToString( dirName, "(", 1 );
		ConcatPascalString( dirName, xmlData->suffix );
		ConcatTextToString( dirName, ")", 1 );

		local_GetRelativePathWin(newName, dirName, xmlOptions->posixPaths, ustrPath, &ulenPath);
		local_XMLWriteTagData(fp, level, tag, kDT_Str, ustrPath, ulenPath);
	}
	#endif
}


////
static void local_XMLWriteMediaThumbnail(FILE *fp, long level, CatalogPtr ch, CellInfo *ci, long id, const XMLOptions *xmlOptions, XMLData *xmlData)
{
	Handle hPICData = (Handle) ReadRecordDataBlock(ch, ci, kPictBlock);

	if( hPICData )
	{
		// Prior to v301 - buggy anyway as it doesn't generate valid pic files
		// local_XMLWriteAsset(hPICData, &xmlData->folderForThumbnails, kThumbnailsFolder, "ThumbnailSource", "pic", 'PICT', 'prvw', fp, level, id, xmlOptions, xmlData);
		// DisposeHandle(hPICData);

		Handle hJPGData = NULL;

		Picture_GetAsJPEG((PicHandle) hPICData, &hJPGData);
		if( hJPGData )
		{
			local_XMLWriteAsset(hJPGData, &xmlData->folderForThumbnails, kThumbnailsFolder, "ThumbnailSource", "jpg", 'JPEG', 'prvw', fp, level, id, xmlOptions, xmlData);
			DisposeHandle(hJPGData);
		}
		DisposeHandle(hPICData);
	}
}


////
static void local_XMLWriteMediaVoiceRecording(FILE *fp, long level, CatalogPtr ch, CellInfo *ci, long id, const XMLOptions *xmlOptions, XMLData *xmlData)
{
	OSType 	dataOSType;
	char	fileNameCstr[8];
	Handle	hData = (Handle) ReadRecordDataBlock(ch, ci, kTalkBlock);

	if( !hData )
		return;

	dataOSType = Sound_GetAudioFormat(hData);
	if (dataOSType==kQTFileTypeWave)
		s_strcpy(fileNameCstr, _countof(fileNameCstr), "wav");
	else if (dataOSType==kQTFileTypeAIFF)
		s_strcpy(fileNameCstr, _countof(fileNameCstr), "aif");
	else if (dataOSType==kQTFileTypeAIFC)
		s_strcpy(fileNameCstr, _countof(fileNameCstr), "aif");
	else if (dataOSType==kQTFileTypeSystemSevenSound)
		s_strcpy(fileNameCstr, _countof(fileNameCstr), "snd");

	local_XMLWriteAsset(hData, &xmlData->folderForRecordings, kRecordingsFolder, "VoiceRecordingSource", fileNameCstr, dataOSType, 'TVOD', fp, level, id, xmlOptions, xmlData);

	DisposeHandleClear(hData);
}


////
static OSErr local_XMLWriteItemFileDetails(FILE *fp, long level, long id, const XMLOptions *xmlOptions, XMLData *xmlData)
{
	OSErr myErr = noErr;
	
	if ( NULL == xmlData )
		myErr = paramErr;
	
	if ( NULL == xmlOptions )
		myErr = paramErr;
	
	if ( noErr == myErr )
	{
		Str255 originalsFolderName = {0};
		CopyPascalStrings(  originalsFolderName, kOriginalsFolder );
		ConcatTextToString( originalsFolderName, "(", 1 );
		ConcatPascalString( originalsFolderName, xmlData->suffix );
		ConcatTextToString( originalsFolderName, ")", 1 );

		#if TARGET_OS_MAC
		{
			FSSpec*			srcFile			= NULL;
			FSSpec			dstFile			= {0,0,{0}};
			FileCopyData	fcd				= {0};
			char			ustrPath[4096]	= {0};
			char			ustrName[4096]	= {0};
			long			ulenPath		= 4096;
			long			ulenName		= 4096;

			srcFile  = xmlData->fileList + ( id - 1 );

			if( xmlOptions->copyOriginals )
			{
				FSPrepareCopy(&fcd, srcFile, &xmlData->folderForOriginals, true, 0);
				myErr = FSExecuteCopy(&fcd);
				if( myErr )
					return myErr;
				FSRefMakeFSSpec(&fcd.dstRef, &dstFile);
			}
			else
			{
				dstFile = *srcFile;
			}

			local_GetFileNameDetails(&dstFile , NULL, xmlOptions->posixPaths, ustrPath, &ulenPath, ustrName, &ulenName);
			local_XMLWriteTagData(fp, level, "Filename", kDT_UStr, ustrName, ulenName);
			local_GetFileNameDetails(srcFile, NULL, xmlOptions->posixPaths, ustrPath, &ulenPath, ustrName, &ulenName);
			local_XMLWriteTagData(fp, level, "Filepath", kDT_UStr, ustrPath, ulenPath);


			local_GetRelativePathMac( &fcd.dstName, xmlData->folderForOriginals.name, xmlOptions->posixPaths, ustrPath, &ulenPath );
			local_XMLWriteTagData(fp, level, "OriginalSource", kDT_UStr, ustrPath, ulenPath);
			
		}
		#else
		{
			FSSpec *		srcFile;
			//FSSpec			dstFile;
			Str255			newName;
			char			ustrPath[MAX_PATH];
			char			ustrName[MAX_PATH];
			long			ulenPath = MAX_PATH;
			long			ulenName = MAX_PATH;

			srcFile  = xmlData->fileList + ( id - 1 );
			FSp_GetFilenameAsPString(srcFile, newName, false);

			if( xmlOptions->copyOriginals )
			{
				FSp_GetUnusedPNameInFolder(&xmlData->folderForOriginals, newName);
				//FSp_GetFileInFolderWithPName(&xmlData->folderForOriginals, newName, &dstFile);
				FSp_CopyFile(srcFile, &xmlData->folderForOriginals, newName, false);
			}
			else
			{
				//dstFile = *srcFile;
			}

			local_GetFileNameDetails(srcFile, NULL, xmlOptions->posixPaths, ustrPath, &ulenPath, ustrName, &ulenName);
			local_XMLWriteTagData(fp, level, "Filename", kDT_Str, &newName[1], newName[0]);
			local_XMLWriteTagData(fp, level, "Filepath", kDT_Str, ustrPath, ulenPath);

			local_GetRelativePathWin(newName, originalsFolderName, xmlOptions->posixPaths, ustrPath, &ulenPath);
			local_XMLWriteTagData(fp, level, "OriginalSource", kDT_Str, ustrPath, ulenPath);
		}
		#endif
	}
	
	return myErr;
}


////
static OSErr local_XMLWriteAssetProperties(FILE *fp, CatalogPtr ch, long id, CellInfo *ci, ItemInfo *info, const XMLOptions *xmlOptions, XMLData *xmlData)
{
	OSErr myErr;
	char label = ci->flags.label; // JRB: we need this because it is illegal to take the address of a bit field
	char rating = ci->flags.rating; 
	local_XMLWriteTagLine(fp, 3, "AssetProperties");

	if( (myErr = local_XMLWriteItemFileDetails(fp, 4, id, xmlOptions, xmlData)) )								goto bail;
	if( (myErr = local_XMLWriteTagData(fp, 4, "UniqueID", kDT_ULong, &ci->uniqueID, 1)) )						goto bail;
	if( (myErr = local_XMLWriteTagData(fp, 4, "Label", kDT_Label, &label, 1)) )									goto bail;
	if( (myErr = local_XMLWriteTagData(fp, 4, "Rating", kDT_Label, &rating, 1)) )								goto bail;
	if( (myErr = local_XMLWriteTagData(fp, 4, "MediaType", kDT_OSType, &info->type, 4)) )							goto bail;
	if( (myErr = local_XMLWriteTagData(fp, 4, "FileSize unit=\"Bytes\"", kDT_ULong, &info->fileSize, 1)) )		goto bail;
	if( info->created   && (local_XMLWriteTagData(fp, 4, "Created", kDT_DateTime, &info->created, 1)) )			goto bail;
	if( info->modified  && (local_XMLWriteTagData(fp, 4, "Modified", kDT_DateTime, &info->modified, 1)) )		goto bail;
	if( info->archived  && (local_XMLWriteTagData(fp, 4, "Added", kDT_DateTime, &info->archived, 1)) )			goto bail;
	if( info->annotated && (local_XMLWriteTagData(fp, 4, "Annotated", kDT_DateTime, &info->annotated, 1)) )		goto bail;

	if( xmlOptions->copyThumbnails )
		local_XMLWriteMediaThumbnail(fp, 3, ch, ci, id, xmlOptions, xmlData);
		
	if( xmlOptions->copyRecordings )
		local_XMLWriteMediaVoiceRecording(fp, 3, ch, ci, id, xmlOptions, xmlData);

	local_XMLWriteTagLine(fp, 3, "/AssetProperties");

	bail:
	return myErr;
}


////
static void local_XMLWriteMediaProperties(FILE *fp, ItemInfo *info)
{
	local_XMLWriteTagLine(fp, 3, "MediaProperties");

	if( info->width )			local_XMLWriteTagData(fp, 4, "Width unit=\"Pixels\"", kDT_Long, &info->width, 1);
	if( info->height )			local_XMLWriteTagData(fp, 4, "Height unit=\"Pixels\"", kDT_Long, &info->height, 1);
	if( info->resolution )		local_XMLWriteTagData(fp, 4, "Resolution unit=\"DPI\"", kDT_Long, &info->resolution, 1);
	if( info->depth )			local_XMLWriteTagData(fp, 4, "Depth unit=\"Bits\"", kDT_Long, &info->depth, 1);
	if( info->width )			local_XMLWriteTagData(fp, 4, "ViewRotation", kDT_ViewRotation, &info->rotate, 1);
	if( info->sampleColor )		local_XMLWriteTagData(fp, 4, "SampleColor", kDT_SampleColor, &info->sampleColor, 1);
	if( info->pages >= 1 )		local_XMLWriteTagData(fp, 4, "Pages", kDT_Long, &info->pages, 1);
	if( info->tracks )			local_XMLWriteTagData(fp, 4, "Tracks", kDT_Long, &info->tracks, 1);
	if( info->duration )		local_XMLWriteTagData(fp, 4, "Duration", kDT_Duration, &info->duration, 1);
	if( info->channels )		local_XMLWriteTagData(fp, 4, "Channels", kDT_Long, &info->channels, 1);
	if( info->sampleRate )		local_XMLWriteTagData(fp, 4, "SampleRate", kDT_Long, &info->sampleRate, 1);
	if( info->sampleSize )		local_XMLWriteTagData(fp, 4, "SampleSize", kDT_Long, &info->sampleSize, 1);
	if( info->avDataRate )		local_XMLWriteTagData(fp, 4, "AverageDataRate", kDT_Long, &info->avDataRate, 1);
	if( info->vdFrameRate )		local_XMLWriteTagData(fp, 4, "AverageFrameRate", kDT_Long, &info->vdFrameRate, 1);
	if( info->vdQuality )		local_XMLWriteTagData(fp, 4, "SpatialQuality", kDT_Long, &info->vdQuality, 1);
	if( info->colorSpace )		local_XMLWriteTagData(fp, 4, "ColorSpace", kDT_OSType, &info->colorSpace, 4);
	if( info->compression )		local_XMLWriteTagData(fp, 4, "Compression", kDT_Long, &info->compression, 1);
	if( info->encoding[0] )		local_XMLWriteTagData(fp, 4, "PrimaryEncoding", kDT_Str, &info->encoding[1], info->encoding[0]);
	if( info->colorProfile )	local_XMLWriteTagData(fp, 4, "ColorProfile", kDT_Str, &info->colorProfile[1], info->colorProfile[0]);
	if( info->textCharacters )	local_XMLWriteTagData(fp, 4, "CharacterCount", kDT_Long, &info->textCharacters, 1);
	if( info->textParagraphs )	local_XMLWriteTagData(fp, 4, "ParagraphCount", kDT_Long, &info->textParagraphs, 1);

	local_XMLWriteTagLine(fp, 3, "/MediaProperties");
}


////
static void local_XMLWriteAnnotations(FILE *fp, CatalogPtr ch, CellInfo *ci)
{
	const XMLPair allPairs[] = {
		{ kIPTCHeadlineField		, "Headline" },
		{ kIPTCTitleField			, "Product" },
		{ kIPTCPrimaryCategoryField	, "Classification" },
		{ kIPTCIntellectualGenreField,"IntellectualGenre"}, //V3
		{ kIPTCEventField			, "Fixture" },
		{ kIPTCEventDateField		, "EventDate" },
		{ kIPTCCreatorField			, "Author" },
		{ kIPTCCreatorTitleField	, "AuthorTitle" },
		{ kIPTCCreatorAddressField  , "CreatorAddress"},	//->V3
		{ kIPTCCreatorCityField		, "CreatorCity"},
		{ kIPTCCreatorStateField	, "CreatorState"},
		{ kIPTCCreatorPostcodeField	, "CreatorPostcode"},
		{ kIPTCCreatorCountryField	, "CreatorCountry"},
		{ kIPTCCreatorPhoneField	, "CreatorPhone"},
		{ kIPTCCreatorEmailField	, "CreatorEmail"},
		{ kIPTCCreatorURLField		, "CreatorURL"},	    //<-V3
		{ kIPTCCreditField			, "Credit" },
		{ kIPTCSourceField			, "Source" },
		{ kIPTCCopyrightField		, "Copyright" },
		{ kIPTCTransmissionField	, "Transmission" },
		{ kIPTCUsageTermsField		, "Rights"},		    //V3
		{ kIPTCURLField				, "URL" },
		{ kIPTCLocationField		, "Location" },
		{ kIPTCCityField			, "City" },
		{ kIPTCStateField			, "State" },
		{ kIPTCCountryField			, "Country" },
		{ kIPTCCountryCodeField		, "CountryCode"},		//V3
		{ kIPTCInstructionsField	, "Instructions" },
		{ kIPTCStatusField			, "Status" },
		{ kIPTCCaptionWriterField	, "Writer" },
		{ kIPTCCaptionField			, "Caption" },

		{ kIPTCPeopleField			, "People" },
		{ kIPTCKeywordField			, "Keyword" },
		{ kIPTCCategoryField		, "Category" },
		{ kIPTCSceneField			, "Scene"},				//V3
		{ kIPTCSubjectReferenceField, "SubjectReference"},  //V3
		
		{ 0,0 } };

	XMLPair *	p			= (XMLPair *) allPairs;
	Boolean 	doneOne		= false;
	Field		field;
	Byte		dataType;
	long		idx;

	do
	{
		idx = 1;
		while( (dataType = GetCellField(ch, ci, idx++, p->tag, &field)) != kTpUnkn )
		{
			if( !doneOne )
				local_XMLWriteTagLine(fp, 3, "AnnotationFields");

			if( p->tag == kIPTCEventDateField ) 
				local_XMLWriteTagData(fp, 4, p->element, kDT_Date, &field.l, 1);
			else if(field.s.buf[0] != 0) //we don't want to print out buf of 256 filled with 0! Will mess up xml file. 
			{
				//some times we get strange data when real length of field.s.buf less then it's said in field.s.len!
				long		  nSize;
				unsigned char *pSrc = field.s.buf;
				for (nSize = 0; nSize < field.s.len && *pSrc; nSize++, pSrc++);

				local_XMLWriteTagData(fp, 4, p->element, FIELD_UTF8(field) ? kDT_UStr: kDT_Str, field.s.buf, nSize);
			}
			ICL_DisposeField(&field, dataType);
			doneOne = true;
			if( !ICL_GetFieldIsRepeating( p->tag ) )
				break;
		}
		p++;
	} while( p->tag );

	if( doneOne )
		local_XMLWriteTagLine(fp, 3, "/AnnotationFields");
}


////
static void local_XMLWriteMediaUserFields(FILE *fp, CatalogPtr ch, CellInfo *ci)
{
	const XMLPair allPairs[] = {
				{ kUser1Field	, "UserField_1" },
				{ kUser2Field	, "UserField_2" },
				{ kUser3Field	, "UserField_3" },
				{ kUser4Field	, "UserField_4" },
				{ kUser5Field	, "UserField_5" },
				{ kUser6Field	, "UserField_6" },
				{ kUser7Field	, "UserField_7" },
				{ kUser8Field	, "UserField_8" },
				{ kUser9Field	, "UserField_9" },
				{ kUser10Field	, "UserField_10" },
				{ kUser11Field	, "UserField_11" },
				{ kUser12Field	, "UserField_12" },
				{ kUser13Field	, "UserField_13" },
				{ kUser14Field	, "UserField_14" },
				{ kUser15Field	, "UserField_15" },
				{ kUser16Field	, "UserField_16" },
				{ 0,0 } };

	XMLPair *	p			= (XMLPair *) allPairs;
	Boolean 	doneOne		= false;
	Field		field;
	Byte		dataType;
	long		count;
	long		i;

	count = ICL_CountUserFields(ch);
	for(i=0; i<count; i++, p++)
		if( (dataType = GetCellField(ch, ci, 1, p->tag, &field)) != kTpUnkn )
		{
			if( !doneOne )
				local_XMLWriteTagLine(fp, 3, "UserFields");
			local_XMLWriteTagData(fp, 4, p->element, FIELD_UTF8(field) ? kDT_UStr: kDT_Str, field.s.buf, field.s.len);
			ICL_DisposeField(&field, dataType);
			doneOne = true;
		}

	if( doneOne )
		local_XMLWriteTagLine(fp, 3, "/UserFields");
}


////
static void local_XMLWriteMediaMetaData(FILE *fp, CatalogPtr ch, CellInfo *ci)
{
	const XMLPair allPairs[] = {
				{ kMetaMakerField				, "Maker" },
				{ kMetaModelField				, "Model" },
				{ kMetaSoftwareField			, "Software" },
				{ kMetaFormatField				, "Format" },
				{ kSourceURLField				, "SourceURL" },
				{ kEXIFVersionField				, "ExifVersion" },
				{ kEXIFCaptureDateField			, "CaptureDate" },
				{ kEXIFProgramField				, "ExposureProgram" },
				{ kEXIFISOSpeedField			, "ISOSpeedRating" },
				{ kEXIFExposureBiasField		, "ExposureBias" },
				{ kEXIFShutterSpeedField		, "ExposureTime" },
				{ kEXIFApertureField			, "Aperture" },
				{ kEXIFFocusDistanceField		, "SubjectDistance" },
				{ kEXIFMeteringModeField		, "MeteringMode" },
				{ kEXIFLightSourceField			, "LightSource" },
				{ kEXIFFlashField				, "Flash" },
				{ kEXIFFocalLengthField			, "FocalLength" },
				{ kEXIFSensingMethodField		, "SensingMethod" },
				{ kEXIFNoiseReductionField		, "NoiseReduction" },
				//->V3
				{ kEXIFContrastField			, "Contrast" },
				{ kEXIFSharpnessField			, "Sharpness" },
				{ kEXIFSaturationField			, "Saturation" },
				{ kEXIFFocusModeField			, "FocusMode" },
				
				{ kEXIFDigitalZoomField			, "DigitalZoom" },
				//<-V3
				{ kEXIFLensField				, "Lens" },
				{ kMergedLatitudeField			, "Latitude" },
				{ kMergedLongitudeField			, "Longitude" },
				{ kMergedAltitudeField			, "Altitude" },
				{ 0,0 } };

	XMLPair *	p			= (XMLPair *) allPairs;
	Boolean 	doneOne		= false;
	Field		field;
	Byte		dataType;
	Str255		pstr;
	char		tmpStr[200]; 
	MyString    ustr = NULL;	
	long		strSize;
	unsigned char *utf8Buff = NULL;

	doneOne = false;
	do
	{
		if( (dataType = GetCellField(ch, ci, 1, p->tag, &field)) != kTpUnkn )
		{
			if( !doneOne )
				local_XMLWriteTagLine(fp, 3, "MetaDataFields");

			if(p->tag == kSourceURLField)
			{	
				Handle hData;
				hData = ReadRecordDataBlock(ch, ci, kSURLBlock);
				if( hData )
				{
					CopyTextToString(pstr, *hData, GetHandleSize(hData));
					DisposeHandle(hData);
				}
				else
				{
					pstr[0] = 0;
				}
				local_XMLWriteTagData(fp, 4, p->element, kDT_Str, &pstr[1], pstr[0]);
			}
			else
			{
				ustr = NULL;
				strSize = 0;
				ICL_FieldToUString(&field, dataType, p->tag, &ustr, false);
				if( (ustr && MyStringGetLength(ustr)) || (p->tag == kEXIFCaptureDateField)) //comment could be "" 
				{
					switch(p->tag)
					{
						case kEXIFProgramField:
						case kEXIFSensingMethodField:
						case kEXIFNoiseReductionField:
						case kEXIFMeteringModeField:
						case kEXIFLightSourceField:
						case kEXIFContrastField:
						case kEXIFSharpnessField:
						case kEXIFSaturationField:
						case kEXIFFocusModeField:
							s_sprintf(tmpStr, _countof(tmpStr), "%s strID = \"%d\"", p->element, field.w);
							utf8Buff = MyStringGetAsUTF8(ustr, &strSize, false);
							if( utf8Buff && strSize )
							{	
								local_XMLWriteTagData(fp, 4, tmpStr, kDT_UStr, utf8Buff, strSize);
								free(utf8Buff);
							}
							break;
						case kEXIFFlashField:
							s_sprintf(tmpStr, _countof(tmpStr), "%s flash = \"%d\"", p->element, field.w);
							utf8Buff = MyStringGetAsUTF8(ustr, &strSize, false);
							if( utf8Buff && strSize )
							{	
								local_XMLWriteTagData(fp, 4, tmpStr, kDT_UStr, utf8Buff, strSize);
								free(utf8Buff);
							}
							break;
						case kEXIFCaptureDateField:
							local_XMLWriteTagData(fp, 4, p->element, kDT_DateTime, &field.l, 1);
							break;
						default:
							{
								utf8Buff = MyStringGetAsUTF8(ustr, &strSize, false);
								if( utf8Buff && strSize )
								{	
									local_XMLWriteTagData(fp, 4, p->element, kDT_UStr, utf8Buff, strSize);
									free(utf8Buff);
								}
							}
					}
				}
				ICL_DisposeField(&field, dataType);
				MyStringDispose(&ustr);
			}
			doneOne = true;
		}
		p++;
	} while( p->tag );

	if( doneOne )
		local_XMLWriteTagLine(fp, 3, "/MetaDataFields");
}


static Boolean local_XMLWriteMovieData(FILE *fp, CatalogPtr ch, CellInfo *ci)
{
	Boolean 	doneOne		= false;
	Field		field;
	Byte		dataType;
	Str255		pstr;
	int			i = 1;
	
	MyBlockZero(&field, sizeof(Field) );

	while( (dataType = GetCellField(ch, ci, i, kTrackInfoListField, &field)) != kTpUnkn )
	{
		if( !doneOne )
			local_XMLWriteTagLine(fp, 3, "MovieTracks");
		
		local_XMLWriteTagLine(fp, 4, "MovieTrack");
		
		if( field.t.name[0] )		local_XMLWriteTagData(fp, 5, "TrackName", kDT_Str, &field.t.name[1], field.t.name[0]);
		if( field.t.encoding[0] )	local_XMLWriteTagData(fp, 5, "TrackEncoding", kDT_Str, &field.t.encoding[1], field.t.encoding[0]);
		if( field.t.offset_ts )		local_XMLWriteTagData(fp, 5, "StartTime", kDT_Duration, &field.t.offset_ts, 1);
		if( field.t.duration_ts )	local_XMLWriteTagData(fp, 5, "TrackDuration", kDT_Duration, &field.t.duration_ts, 1);
		if( field.t.dataSize )		local_XMLWriteTagData(fp, 5, "DataSize", kDT_Long, &field.t.dataSize, 1);
		if( field.t.dataRate )		local_XMLWriteTagData(fp, 5, "DataRate", kDT_Long, &field.t.dataRate, 1);
		if( field.t.fpms )			local_XMLWriteTagData(fp, 5, "FPMS", kDT_Long, &field.t.fpms, 1);
		if( field.t.mediaType )		local_XMLWriteTagData(fp, 5, "TrackType", kDT_OSType, &field.t.mediaType, 1);
		
		(field.t.isChapter)? CopyCStringToPascal("True",pstr) : CopyCStringToPascal("False",pstr);
		local_XMLWriteTagData(fp, 5, "Chapter", kDT_Str, &pstr[1], pstr[0]);

		i++;
		doneOne = true;
		ICL_DisposeField(&field, dataType);
		local_XMLWriteTagLine(fp, 4, "/MovieTrack");
		MyBlockZero(&field, sizeof(Field) );
	}

	if( doneOne )
		local_XMLWriteTagLine(fp, 3, "/MovieTracks");

	return doneOne;
}


static Boolean local_XMLWriteMovieChapterData(FILE *fp, CatalogPtr ch, CellInfo *ci)
{
	Boolean 	doneOne		= false;
	Field		field;
	Byte		dataType;
	Str255		pstr;
	int			i = 1;
	MyBlockZero(&field, sizeof(Field) );

		while( (dataType = GetCellField(ch, ci, i, kCuePointInfoListField, &field)) != kTpUnkn )
		{
			if( !doneOne )
				local_XMLWriteTagLine(fp, 3, "MovieChapterTracks");
			
			local_XMLWriteTagLine(fp, 4, "MovieChapterTrack");

			CopyPascalStrings(pstr,field.c.name);
			local_XMLWriteTagData(fp, 5, "ChapName", kDT_Str, &pstr[1], pstr[0]);
			SecToTimeString(field.c.offset_ts, pstr, false, false);
			local_XMLWriteTagData(fp, 5, "Offset", kDT_Str, &pstr[1], pstr[0]);
			i++;
			doneOne = true;
			ICL_DisposeField(&field, dataType);
			local_XMLWriteTagLine(fp, 4, "/MovieChapterTrack");
			MyBlockZero(&field, sizeof(Field) );
		}

		if( doneOne )
			local_XMLWriteTagLine(fp, 3, "/MovieChapterTracks");
return doneOne;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//																								//
// Write Sets																					//
//																								//
//////////////////////////////////////////////////////////////////////////////////////////////////

#pragma mark -

static void local_XMLWriteCatalogSets(FILE *fp, CatalogPtr ch)
{
	long morselCount = MorselList_CountMorsels(ch, kCatalogSetField, true);

	if( morselCount > 0 )
	{
		long morselIndex = 0;
		local_XMLWriteTagLine(fp, 1, "SetList");
		local_XMLWriteAllSets(fp, ch, morselCount, -1, &morselIndex);
		local_XMLWriteTagLine(fp, 1, "/SetList");
	}
}


////
static long local_XMLWriteAllSets(FILE *fp, CatalogPtr ch, long morselCount, long level, long* morselIndex)
{
	MorselRef		mref;
	long 			newLevel;
	long 	entryIndex;
	MyString morselString = NULL;
	long	strSize = 0;
	unsigned char* p; 

	while( *morselIndex < morselCount )
	{
		(*morselIndex)++;
		mref = MorselList_GetMorselAtLocation(ch, kCatalogSetField, *morselIndex);
		newLevel = Morsel_GetLevel(ch, mref);

		if( newLevel <= level )
			break;

		if( newLevel == level )
			local_XMLWriteTagLine(fp, Morsel_GetLevel(ch, mref)+2, "/Set");

		//if( newLevel > level )
			local_XMLWriteTagLine(fp, newLevel+2, "Set");
		morselString = Morsel_PeekNameAsMyString( ch, mref );  // do not dispose
		p = MyStringGetAsUTF8(morselString, &strSize, false);
		local_XMLWriteTagData(fp, newLevel+3, "SetName", kDT_UStr, p, strSize);
		free(p);
		if( Morsel_GetItemCount(ch, mref) )
		{
			for(entryIndex=0; entryIndex<Morsel_GetItemCount(ch, mref); entryIndex++)
			{
				unsigned long ul = Morsel_GetNthItemUniqueID(ch, mref, entryIndex);
				if(gCatalogExp || ICL_GetItemWithUniqueID(ch,ul))
					local_XMLWriteTagData(fp, Morsel_GetLevel(ch, mref)+3, "UniqueID", kDT_ULong, &ul, 1);
			}
		}


		if( Morsel_IsParent(ch, mref) )
			local_XMLWriteAllSets(fp, ch, morselCount, newLevel, morselIndex);

	//	if( newLevel > level )
			local_XMLWriteTagLine(fp, Morsel_GetLevel(ch, mref)+2, "/Set");

	}

	return (*morselIndex == morselCount)? *morselIndex: (*morselIndex)--; //- 1;
}



//////////////////////////////////////////////////////////////////////////////////////////////////
//																								//
// Output data																					//
//																								//
//////////////////////////////////////////////////////////////////////////////////////////////////

#pragma mark -

static OSErr local_XMLWriteTagData(FILE *fp, long level, const char *tag, long datatype, void *data, long dataLen)
{
	long			i;
	char 			ele[256];
	char *			c;
	RGBColor		rgb;
	char			ustr[4096];
	unsigned long	ulen = 4095;
	unsigned long	ul = 0;

	ustr[0] = 0;
	switch(datatype)
	{
		case kDT_Str:
			// make sure there are no NULL characters in the input string.
			for(i=0, c=(char*)data; i<dataLen; i++, c++)
				if( *c == 0 )
				{
					dataLen = i;
					break;
				}

			MyTextToUnicode(data, dataLen, ustr, 4095, &ulen, kUnicodeUTF8Format, MyGetTextEncoding());
			XMLLegaliseAscii(ustr, &ulen, false);
			break;

		case kDT_UStr:
			if((unsigned long)dataLen < ulen) ulen = dataLen;
			BlockMoveData(data, ustr, ulen);
			XMLLegaliseAscii(ustr, &ulen, false);
			break;

		case kDT_ULong:
			s_sprintf(ustr, _countof(ustr), "%lu", *((unsigned long *) data));
			ulen = (unsigned long) strlen(ustr);
			break;

		case kDT_Label:
			c = (char *) data;
			ustr[0] = '0' + *c;
			ulen = 1;
			break;
		
		case kDT_Duration:
			ul = *((unsigned long *) data);
			SecToTimeString(ul, (unsigned char *) ustr, false, false);
			ConvertPtoCString((unsigned char *) ustr);
			ulen = (unsigned long) strlen(ustr);
			break;

		case kDT_DateTime:
			#if TARGET_OS_MAC
			{
				LongDateRec dateRec = {0};
				LongDateTime ldt;

				ul = *((unsigned long *) data);
				ldt = ul;
				LongSecondsToDate(&ldt, &dateRec);
				snprintf(ustr, ulen, "%d:%s%d:%s%d %s%d:%s%d:%s%d",
					dateRec.ld.year,
					( dateRec.ld.month  < 10 ) ? "0":"", dateRec.ld.month,
					( dateRec.ld.day    < 10 ) ? "0":"", dateRec.ld.day,
					( dateRec.ld.hour   < 10 ) ? "0":"", dateRec.ld.hour,
					( dateRec.ld.minute < 10 ) ? "0":"", dateRec.ld.minute,
					( dateRec.ld.second < 10 ) ? "0":"", dateRec.ld.second);
				ulen = strlen(ustr);
			}
			#else
			{
				//DateTimeToString((unsigned char *) ustr, *((unsigned long *) data), true, true, shortDate, true);
				//ConvertPtoCString((unsigned char *) ustr);
				SYSTEMTIME fTime;
				MacSecondsToWinTime(*((unsigned long *) data), &fTime);
				s_sprintf(ustr, _countof(ustr), "%d:%s%d:%s%d %s%d:%s%d:%s%d",
					fTime.wYear,
					( fTime.wMonth  < 10 ) ? "0":"", fTime.wMonth,
					( fTime.wDay    < 10 ) ? "0":"", fTime.wDay,
					( fTime.wHour   < 10 ) ? "0":"", fTime.wHour,
					( fTime.wMinute < 10 ) ? "0":"", fTime.wMinute,
					( fTime.wSecond < 10 ) ? "0":"", fTime.wSecond);

				ulen = (unsigned long) strlen(ustr);
			}
			#endif
			break;

		case kDT_Date:
			#if TARGET_OS_MAC
			{
				LongDateRec dateRec = {0};
				LongDateTime ldt;

				ul = *((unsigned long *) data);
				ldt = ul;
				LongSecondsToDate(&ldt, &dateRec);
				sprintf(ustr, "%d:%s%d:%s%d",
					dateRec.ld.year,
					( dateRec.ld.month  < 10 ) ? "0":"", dateRec.ld.month,
					( dateRec.ld.day    < 10 ) ? "0":"", dateRec.ld.day);
				ulen = strlen(ustr);
			}
			#else
			{
				//DateTimeToString((unsigned char *) ustr, *((unsigned long *) data), true, false, shortDate, false);
				//ConvertPtoCString((unsigned char *) ustr);
				SYSTEMTIME fTime;
				MacSecondsToWinTime(*((unsigned long *) data), &fTime);
				s_sprintf(ustr, _countof(ustr), "%d:%s%d:%s%d",
					fTime.wYear,
					( fTime.wMonth  < 10 ) ? "0":"", fTime.wMonth,
					( fTime.wDay    < 10 ) ? "0":"", fTime.wDay);
				ulen = (unsigned long) strlen(ustr);
			}
			#endif
			break;


		case kDT_ViewRotation:
			switch( *((unsigned short *) data) )
			{
				case kRotateCW			: ustr[0] = '8'; break;
				case kRotateCWFlipped	: ustr[0] = '7'; break;
				case kRotateCCW			: ustr[0] = '6'; break;
				case kRotateCCWFlipped	: ustr[0] = '5'; break;
				case kTransformFlipV	: ustr[0] = '4'; break;
				case kTransformFlipHV	: ustr[0] = '3'; break;
				case kTransformFlipH	: ustr[0] = '2'; break;
				default					: ustr[0] = '1'; break; // kTransformIdentity
			}
			ulen = 1;
			break;

		case kDT_SampleColor:
			MediaLib_SampleColorToRGBColor(&rgb, (long *) data);
			s_sprintf(ustr, _countof(ustr), "R:%02X G:%02X B:%02X", rgb.red/256, rgb.green/256, rgb.blue/256);
			ulen = (unsigned long) strlen(ustr);
			break;

		case kDT_Long:
			s_sprintf(ustr, _countof(ustr), "%d", *((long *) data));
			ulen = (unsigned long) strlen(ustr);
			break;

		case kDT_OSType:
			{
			OSType swappedType = EndianU32_BtoN( *(OSType*)(data) );
			s_sprintf(ustr, _countof(ustr), "%4.4s", (char *)(&swappedType) );
			ulen = (unsigned long) strlen(ustr);
			}
			break;
	}

	for(; level>0; level--)
		fprintf(fp, "\t");
	fprintf(fp, "<%s>", tag);
	fwrite(ustr, 1, ulen, fp);

	#if TARGET_OS_WIN32
		sscanf_s(tag, "%s", ele, _countof(ele));
	#else
		sscanf(tag, "%s", ele);
	#endif
		fprintf(fp, "</%s>%s", ele, kCR);

	return noErr;
}


////
static void local_XMLWriteTagLine(FILE *fp, long level, const char *tag)
{
	for(; level>0; level--)
		fprintf(fp, "\t");
	fprintf(fp, "<%s>%s", tag, kCR);
}




////
void EndianXMLOptions_BtoN( XMLOptions* xml_i ) { ((void*)(&xml_i)); }	// nothing to do
void EndianXMLOptions_NtoB( XMLOptions* xml_i ) { ((void*)(&xml_i)); }	// nothing to do


