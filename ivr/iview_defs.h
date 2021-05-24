//// iView Libraries - Catalog Data Types// Copyright 2001-2006 iView Multimedia Ltd. All rights reserved.//#ifndef __ICLTYPES__#define __ICLTYPES__#pragma once#include <CoreServices/CoreServices.h>////////////////////////////////////////////////////////////////////////////////////////////////////																								//// CONSTANTS																					////																								////////////////////////////////////////////////////////////////////////////////////////////////////#pragma mark -  CONSTANTS#define kCatalogFileFormat 		'025i'#define kICLTextEncoding_TEXT 	kIPTCEncoding_TEXT#define FIELD_TEXT(f)			(kICLTextEncoding_TEXT == f.s.enc)#define kICLTextEncoding_UTF8 	kIPTCEncoding_UTF8#define FIELD_UTF8(f)			(kICLTextEncoding_UTF8 == f.s.enc)// Data Blocks/Blobsenum { kPictBlock = 1, kIptcBlock, kSURLBlock, kMetaBlock, kTalkBlock };enum { kThumbnailBlob = 1, kHiRezPreviewBlob, kVoiceRecordingBlob, kSourceTextBlob };// General#define kMaxFolderSize		2048#define kScratchBuffer		10240L// MediaLib_GetMediaInfo constants#define kTryThread			true#define kDontThread			false#define kTryRender			true#define kDontRender			false// Limits#define kMaxCatalogBytes	(SInt32) 0x70000000 // 1.8 MB#define kMaxCatalogItemsPro	128000L#define kMaxCatalogItems	8000L//// required string resourcesenum{	kIPTCFieldNamesIPTC_ResID	= 501,	kIPTCFieldNamesIVIEW_ResID	= 502,	kIPTCFieldNamesADOBE_ResID	= 503,		kFieldNames_ResID			= 510,		kFieldUnits_ResID			= 511,	kRESERVED_ResID				= 512,	kFileSizeUnits_ResID		= 513,	kMonths_ResID				= 514,	kDay_ResID					= 515,	kFieldGroup_ResID			= 516,	kEnumBase_ResID				= 520,	kMediaTypes_ResID			= 521,	kLightSources_ResID			= 522,	kMeteringModes_ResID		= 523,	kExposurePrograms_ResID		= 524,	kAudioChannels_ResID		= 525,	kMovieQualityModes_ResID	= 526,	kSensingModes_ResID			= 527,	kFlash_ResID				= 528,	kFlashModes_ResID			= 529,	kGeneralStrings_ResID		= 599};#define kMaxICLTextBuffer 	4096#define kMaxUserFields		16#define kIPTCEncoding_TEXT	0x1C#define kIPTCEncoding_UTF8	0x1Dtypedef unsigned char FindType;enum { // use for comparison tests between fields	kFindAsNone				= 0,	kFindAsText				= 1,	kFindAsNumb				= 2,	kFindAsDate				= 3,	kFindAsType				= 4};typedef unsigned char FieldMask;enum { // use to retrieve fields that meet criteria	kNoFieldMask			= 0,	kCanEditFieldMask		= 1,	kCanDisplayFieldMask	= 2,	kCanFindFieldMask		= 3};typedef unsigned char FieldGroup;enum { // grouping of related fields	kResreved_1 = 1,	kGrInfo,	kReserved_2,	kGrIptc,	kReserved_3,	kReserved_4,	kReserved_5,	kReserved_6,	kGrFoto,	kGrSets,	kGrFont,	kGrDoci,	kGrTrck,	kGrChpt,	kGrKeys,	kGrCats,	kGrPple,	kGrScen,	kGrSref,	kAnyGroup = 0};typedef unsigned char FieldType;enum { 				// use to indicate which field is populated	kTpUnkn = 0, 	// non of the fields is populated	kTpByte,		// 'b' field is on	kTpWord,		// 'w'	kTpLong,		// 'l'	kTpRtnl,		// 'r'	kTpText,		// 's'	kTpChap,		// 'c'	kTpTrak,		// 't'	kTpEND};#define FourCC2Str(code) (char[5]){(code >> 24) & 0xFF, (code >> 16) & 0xFF, (code >> 8) & 0xFF, code & 0xFF, 0}////////////////////////////////////////////////////////////////////////////////////////////////////																								//// TYPE_DEFINITIONS																				////																								////////////////////////////////////////////////////////////////////////////////////////////////////#pragma mark -  TYPE_DEFINITIONStypedef struct { SInt32 n, d; } rational;typedef struct { unsigned char a,r,g,b; } Pixel32M;typedef struct { unsigned char b,g,r,a; } Pixel32I;typedef struct { SInt32 gmt; short Y,M,D, h,m,s; } MyDate;typedef struct { SInt32 v, h; } LongPt;typedef struct { SInt32 top, left, bottom, right; } LongRect;typedef struct{	Str255 name;	UInt32 offset_ts; // in tenths of a second} CuePointInfo;typedef struct{	// this structure was changed for version 2 (read compatibility notes)	// the 256 char name field has now been split into a series of strings and SInt32s	unsigned char	name[96];	unsigned char	encoding[96];	SInt32			reserved_0[12];	SInt32			preloadTime;	SInt32			preloadDuration;	SInt32			preloadFlags;	SInt32			defaultHints;		// fields common to both versions	UInt32	offset_ts;		// in tenths of a second	UInt32	duration_ts;	// duration in tenths of a second	UInt32	dataSize;	UInt32	dataRate;		// in v1 these 2 fields were joined as a double for fps	UInt32	fpms;	UInt32	mediaType;		// fields common to both versions	unsigned char	isChapter;	unsigned char	isEnabled;	unsigned char	reserved_1;	unsigned char	revision;} TrackInfo;typedef struct ICLText{	short			len;	Byte			enc;	Byte			pad;	Byte			buf[kMaxICLTextBuffer];} ICLText;typedef union{	char			b;	short			w;	SInt32			l;	rational		r;	ICLText			s;	CuePointInfo	c;	TrackInfo		t;} Field;typedef struct{	OSType 		tag;	char		element[32];} XMLPair;////////////////////////////////////////////////////////////////////////////////////////////////////																								//// FIELD DEFINITIONS																			////																								////////////////////////////////////////////////////////////////////////////////////////////////////#pragma mark - FIELD DEFINITIONS// Media Fields ..............................#define kPathnameField				0x00000001#define kFilenameField				0x00000002#define kDisknameField				'VNam'#define kFileTypeNameField			0x00000003#define kFileTypeField				0x00000004#define kFileSizeField				0x00000005#define kWidthField					0x00000006#define kHeightField				0x00000007#define kResolutionField			0x00000008#define kDepthField					0x00000009#define kAddedField					0x0000000A#define kCreatedField				0x0000000B#define kModifiedField				0x0000000C#define kPagesField					0x0000000D#define kPosterField				0x0000000E#define kTracksField				0x0000000F#define kDurationField				0x00000010#define kChannelsField				0x00000011#define kSampleRateField			0x00000012#define kSampleSizeField			0x00000013#define kMovieQualityField			0x00000014#define kColorSpaceField			0x00000015#define kCompressionField			0x00000016#define kEncodingField				0x00000017#define kColorProfileField			0x00000018#define kCharactersField			0x00000019#define kParagraphsField			0x0000001A#define kSampleColorField			0x0000001B#define kTalkHSizeField				0x0000001C#define kMetaHSizeField				0x0000001D#define kPictHSizeField				0x0000001E#define kIptcHSizeField				0x0000001F#define kUrlfHSizeField				0x00000020#define kAnnotatedField				0x00000021#define kMovieDataRateField			0x00000022#define kMovieFrameRateField		0x00000023#define kSourceURLField				0x000000B4// Merged Fields .............................#define kMergedDimensionsField		0x00000041#define kMergedMediaTypeField		0x00000042#define kMergedMakerModelField		0x00000043#define kUniqueIDField				0x00000044#define kOrientationField			0x00000045#define kMergedExifField			0x00000046#define kASCIICaptureDateField		0x00000047#define kMergedGeographyField		0x00000048#define kMergedDateField			0x000000A1#define kCatalogSetField			0x000000A2#define kMergedLatitudeField		'LATD'#define kMergedLongitudeField		'LOTD'#define kMergedAltitudeField		'ALTD'// User Fields ...............................#define kUser1Field					0x00000301#define kUser2Field					0x00000302#define kUser3Field					0x00000303#define kUser4Field					0x00000304#define kUser5Field					0x00000305#define kUser6Field					0x00000306#define kUser7Field					0x00000307#define kUser8Field					0x00000308#define kUser9Field					0x00000309#define kUser10Field				0x0000030A#define kUser11Field				0x0000030B#define kUser12Field				0x0000030C#define kUser13Field				0x0000030D#define kUser14Field				0x0000030E#define kUser15Field				0x0000030F#define kUser16Field				0x00000310// Dummy fields for Sorting ..................#define kRandomPseudoField			8000#define kDefaultPseudoField			8001#define	kInvertPseudoField			8002////////////////////////////////////////////////////////////////////////////////////////////////////																								//// Slide Show	 																				////																								////////////////////////////////////////////////////////////////////////////////////////////////////#pragma mark - SlideShowenum{	// ShowOptions textBits	tb_filename = 0, // title or filename	tb_mediainfo,	tb_fotoinfo,	tb_gpsinfo,	tb_location,	tb_people,	tb_credits,	tb_caption,	tb_end};enum{	// transition modes	kSS_FX_None 				= 1,	kSS_FX_Random 				= 2,	kSS_FX_Barn_horizontal 		= 4,	kSS_FX_Barn_vertical		= 5,	kSS_FX_Blinds_horizontal	= 6,	kSS_FX_Blinds_vertical		= 7,	kSS_FX_Iris_circle			= 8,	kSS_FX_Iris_square			= 9,	kSS_FX_Slide_left_to_right	= 10,	kSS_FX_Slide_right_to_left	= 11,	kSS_FX_CrossFade			= 12,	kSS_FX_Radial				= 13};enum{	// run modes	kSS_AutoPaused				= 0,	kSS_AutoRunning				= 1,	kSS_InteractivePaused		= 2,	kSS_InteractiveRunning		= 3};enum{	// tab panes	kSS_TabFiles				= 1,	kSS_TabDisplay				= 2,	kSS_TabInfo					= 3};//// Slide options structure [40 UInt8s]typedef struct{	unsigned char	tabsPane;			// visible tab within panel			[tab panes]	unsigned char	tabsVisible;		// tabs are visible 				[true|false]	unsigned char	panelVisible;		// panel is visible 				[true|false]	signed char		runMode;			// one of run modes	SInt16			panelV;				// saved panel window top coordinate	SInt16			panelH;				// saved panel window left coordinate	unsigned char	infoPage;			// active page in info panel		[1=>x=>4]	signed char		global_tix;			// slide duration in seconds		[kSSL_MinDuration|kSSL_MaxDuration]	unsigned char 	txAlign;			// text alignment					[0:center, 1:left, 2:right]	unsigned char 	R0;	unsigned char	bgClrR;				// back color red value	unsigned char	bgClrG;				// back color green value	unsigned char	bgClrB;				// back color blue value	unsigned char 	captionLines;		// number of lines to draw in caption	signed char		zm_mode;			// media scalling					[scaling modes]	unsigned char	fadeInOut;			// gamma fade screen 				[true|false]	unsigned char	pickRandom;			// enable randomizer				[true|false]	unsigned char	loopShow;			// continuous						[true|false]	signed char		soundVolume;		// sound volume						[0:off|1 to 7]	unsigned char	gr_mode;			// grid ID							[>1]	unsigned char	fx_mode;			// transition mode					[>1]	unsigned char	cellMargins;		// margin between cells				[true|false]	unsigned char	multiSound;			// play sound for all movies		[true|false]	unsigned char	voice;				// play sound for each file			[true|false]	unsigned char	textBits;			// draw info in cells				[8 bit]	unsigned char 	R1;	unsigned char 	R2;	unsigned char 	R3;	unsigned char 	R4;	unsigned char 	R5;	unsigned char 	R6;	unsigned char 	R7;	unsigned char 	R8;	unsigned char 	R9;	unsigned char 	R10;	unsigned char	txClrR;				// text color red value	unsigned char	txClrG;				// text color green value	unsigned char	txClrB;				// text color blue value} ShowOptions;#define kShowOptionsDefs {						\	kSS_TabFiles,1,1,kSS_AutoRunning,			\	40,15,										\	1,4,0,0,									\	0,0,0,3,									\	2/* kZoomMenu_ScaleToFit */,1,0,1,			\	7,1,kSS_FX_None,0,							\	0,1,0,0,0,									\	0,0,0,0,									\	0,0,0,0,									\	(unsigned char) 0xFF,						\	(unsigned char) 0xFF,						\	(unsigned char) 0xFF }enum{	// MediaShow tm_mode	kDefault = 1,	kUserSet,	kWaitForAudio};typedef struct{	SInt32	tm_mode;				// timer mode	SInt32	tm_secs;				// timer duration		char	reserved_1;	char	gr_mode;				// custom grid mode (or 0 if using defaults)	char	zm_mode;				// custom zoom mode (or 0 if using defaults)	char	fx_mode;				// custom effect mode (or 0 if using defaults)	char	reserved_2;	char	reserved_3;	char	reserved_4;	char	fx_type;				// V1.X only} MediaShow;////typedef struct{	// 4 UInt8s	char 		l;	char 		t;	char 		r;	char 		b;} CellFormat;#define kMaxGridCount	16#define kGVersion		0x31typedef struct{	// 68 UInt8s	UInt8 		cells;	UInt8 		inclusive;	UInt8 		divisions;	UInt8 		version;	CellFormat 	track[kMaxGridCount];} GridFormat;////////////////////////////////////////////////////////////////////////////////////////////////////																								//// Utility folder enums 																		////																								////////////////////////////////////////////////////////////////////////////////////////////////////#pragma mark -#pragma mark UtilityFolderTypetypedef unsigned char UtilityFolderType;enum {	// use to get/set catalog utility folders	kVersionsFolder			= 0,	kPrimaryDropFolder		= 1,	kSecondaryDropFolder	= 2,	kUtilFolders};////////////////////////////////////////////////////////////////////////////////////////////////////																								//// Catalog Layout Options																		////																								////////////////////////////////////////////////////////////////////////////////////////////////////#pragma mark CatalogLayouts#define kMaxVisFields 10typedef struct FontsOptions{	SInt16		size;	SInt16		face;	char		name[512];} FontsOptions;typedef struct ColorOptions{	SInt32 	paper;		// itms	SInt32 	textF;		// itms	SInt32 	textB;		// -tms	SInt32 	hline;		// i---} ColorOptions;typedef struct FieldOptions{	UInt32		tag;			// field tag	SInt16		dim;			// field dimension (width | height)	UInt8		res;	UInt8		sty;			// field-text style} FieldOptions;typedef enum {	eSquare			= 0,	eLandscape		= 1,	ePortrait		= 2} eOrientation;typedef struct StyleOptions{	UInt8		version;	UInt8		unused_1;	UInt8		unused_2;	UInt8		unused_3;	SInt16		zoom;			// --M- (view zooming 5% to 1600% )	UInt8		orient;			// -T-- (0:square 1:landscape 2:portrait )	UInt8		size;			// LTM- (enums)	UInt8		center;			// -TM-	UInt8		sclBox;			// -T-- (used to be column)	UInt8		labels;			// -TM-	UInt8		margin;			// -T-- (0=no margin, 1=plain margin, 2=sunken, 3=raised )	UInt8		gadget;			// -TM- ratings/label gadget	UInt8		icons;			// L--- (1:draw icons only, 0:draw thumbs)	UInt8		frame;			// LT--	UInt8		histogram;		// --M-} StyleOptions;typedef struct{	// Thumbnail & Full Screen Preview Options (12 UInt8s)	UInt8		version;	UInt8 		thmSize;		// 0=variable | 1=160 pixels | 2=320 pixels | 3=480 pixels | 4=640 pixels	UInt8 		thmQuality;		// 0=low | 1=medium | 2=high	UInt8		thmIgnore;		// true/false	UInt8		fspPreview;		// true/false	UInt8		fspCreate;		// true/false	UInt8		fspSize;		// 0=small 800 pixels | 1=normal 1024 pixels | 2=large 1280 pixels	UInt8		fspQuality;		// 0=low | 1=medium | 2=high	UInt8		unused[4];} ICL_BuildOptions;#define kICL_BuildOptionsVersion 0x31#define kICL_BuildOptions_ForNewFiles		{ kICL_BuildOptionsVersion, 0,2,0, 0,0,1,1, {0} }#define kICL_BuildOptions_ForExistingFiles	{ kICL_BuildOptionsVersion, 0,2,1, 0,0,1,1, {0} }////////////////////////////////////////////////////////////////////////////////////////////////////																								//// Media Item																					////																								////////////////////////////////////////////////////////////////////////////////////////////////////#pragma mark MediaItem#define kSavedAliasSize		444enum { kMacAlias = 0, kMacPackedAlias = 1};typedef struct{	SInt32  		type;	SInt16			orig_size;	SInt16			size;	UInt8			data[kSavedAliasSize];} MediaAlias;////typedef struct{	unsigned char	legacy_path[256];	// Pascal string: file path (NB MacRoman encoding on disk, native encoding in RAM)	unsigned char	legacy_name[64];	// Pascal string: file name (NB MacRoman encoding on disk, native encoding in RAM)	SInt32 			importTicks;		// number of ticks taken to import the file	UInt8			unused_1[8];	UInt32			type;				// file type	MediaShow		show;				// slide show options	SInt32			fileSize;			// file size (data+resource)	SInt32			width;				// spatial width	SInt32			height;				// spatial height	SInt32			resolution;			// spatial resolution	SInt32			sampleColor;		// item's prominant color	SInt32			depth;				// spatial depth	MediaAlias		legacy_alias;		// file alias (used in earlier versions of iview)	UInt8			unused_2[2];	UInt16			rotate;				// one of 'H ','V ','HV','W ','CW'	UInt32			archived;			// catalog date	UInt32			created;			// file creation date	UInt32			modified;			// file modification date	SInt32			pages;				// number of image layers/pages	SInt32			poster;				// time value for thumbnail	SInt32			tracks;				// number of tracks	SInt32			duration;			// total duration of movie/sound	SInt32			channels;			// maximum number of sound channels in a single track	SInt32			sampleRate;			// maximum sound sample rate in a single track	SInt32			sampleSize;			// maximum sound bit size in a single track		UInt32			annotated;			// file annotation date - archived updates when iptc is edited	SInt32			avDataRate;			// sum of all track data rates in UInt8s per second	SInt32			vdFrameRate;		// lower track rate in frames per millisecond	SInt32			vdQuality;			// lower track video rate (video only)	UInt8			unused_3[32];	UInt32			colorSpace;	SInt32			compression;	unsigned char	encoding[32];		// Pascal string	unsigned char	colorProfile[32];	// Pascal string	SInt32			textCharacters;	SInt32			textParagraphs;	SInt32			scratch;		// temporary scratch buffer - 0 when saved	// metadata blocks	SInt32			talkSize;		// size of voice 'snd' recording	SInt32			metaSize;		// size of read only atom formatted metadata	SInt32			pictSize;		// size of pict preview	SInt32			iptcSize;		// size of editable 'iptc' formatted data	SInt32			urlfSize;		// size of attached URL} ItemInfo;////////////////////////////////////////////////////////////////////////////////////////////////////																								//// Catalog Cache																				////																								////////////////////////////////////////////////////////////////////////////////////////////////////#pragma mark Catalog/Record Cache// values are native endian// therefore we could assign the datatype when we read the valuetypedef struct {	UInt32 		tag;	UInt32 		len;	void *		buf;	UInt8 		enc, dec, un1, un2;} CacheAtom;typedef struct {	UInt32 		uniqueID;	ItemInfo 	info;	CacheAtom *	atomArray; 	UInt32 		atomCount;} RecordCache;typedef struct{	unsigned short	tag;	unsigned short len;} fip; // flattenned iptc data formattypedef struct{	SInt32 len;	SInt32 tag;	char buf[1];} fud; // flattenned user data format////////////////////////////////////////////////////////////////////////////////////////////////////																								//// Catalog Preferences																			////																								////////////////////////////////////////////////////////////////////////////////////////////////////#pragma mark CatalogPrefs// Window Saved Preferences// 1024 UInt8s fixed size structure// Fields marked with X are not used in version 2typedef struct CatalogPrefs{	UInt32				format;						// = kFileFormat	UInt32				sortField;	UInt8				infoPane;					// see Info panel modes	UInt8				viewPane;					// see View panel modes	UInt8				foldersWatchRate;			// frequency of chacking (see enums)	UInt8				unused_0;	UInt16				unused_1;	UInt16				panelWidth;					// width of side panel	UInt32				unused_2;	UInt32				mediaInfoClosed;			// visibility of info sub sections (32 bits)	UInt8				sortDirection;				// direction can be 0=ascending or descending	UInt8				filmstripVisible;			// filmstrip is visible in media view	UInt8				toolbarHidden;				// is toolbar hidden	UInt8				unused_3;					// media box is visible	SInt16				organizerSplit;				// propotion of list/list in organizer panel	SInt16				infoPanelSplit;				// propotion of list/edit in info panel	UInt8				unused_4;	UInt8				unused_5;					// N/A smallestTBox	UInt8	 			organizerVisBits;			// visiblility of info and organizer lists	UInt8	 			unused_6;	Rect				winRect;					// window saved rect	ShowOptions			showOptions;				// slide show settings	FieldOptions 		visField[4][kMaxVisFields];	// displayed fields - 4 views	StyleOptions 		visStyle[4];				// field styles     - "	ColorOptions 		visColor[4];				// colors           - "	UInt32				unused_7;	GridFormat			customGrid;	UInt32				mediaMask;	unsigned char 		themeName[64];				// title of the theme selected for this catalog	unsigned char		altTitle[64];				// alternative catalog title (this is used for html exporting only)	unsigned char		reserved_1[256];			// 'legacy_comment' do not access this field - use accessors} CatalogPrefs;//// catalog defaults#define kIFieldDefs { {kFilenameField,200,0,0}, {kFileSizeField,60,0,0}, {kFileTypeNameField,60,0,0}, {kWidthField,60,0,0}, {kHeightField,60,0,0}, {kDurationField,60,0,0}, {kPathnameField,200,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0} }#define kTFieldDefs { {kFilenameField, 1,0}, {0} }#define kMFieldDefs { {kFilenameField, 1,0}, {0} }#define kSFieldDefs { 0 }#define kDefFontSize 11#define kIStyleDefs { 3,0,0,0,   0, 0, 1,0,0,0,0,0,0,1,0 }#define kTStyleDefs { 3,0,0,0,   0, 1,16,1,0,0,2,1,0,0,0 }#define kMStyleDefs { 3,0,0,0, 200, 0, 2,1,0,0,0,0,0,0,0 }#define kSStyleDefs { 3,0,0,0, 200, 0, 2,1,0,0,0,0,0,0,0 }#define kColorsDefs			{ {0xFFFF,0xFFFF,0xFFFF},{0x0000,0x0000,0x0000},{0xDDDD,0xDDDD,0xDDDD},{0xDDDD,0xDDDD,0xDDDD} }#define kColorsDefs_Alt 	{ {0x4040,0x4040,0x4040},{0xFFFF,0xFFFF,0xFFFF},{0x2626,0x2626,0x2626},{0xDDDD,0xDDDD,0xDDDD} }////////////////////////////////////////////////////////////////////////////////////////////////////																								//// Catalog Definitions Structures																////																								////////////////////////////////////////////////////////////////////////////////////////////////////#pragma mark Catalog// Opaque structure definitionsstruct CellInfo;					// opaquetypedef struct CellInfo* 			CellInfoPtr;////////////////////////////////////////////////////////////////////////////////////////////////////																								//// MetaTags																						////																								////////////////////////////////////////////////////////////////////////////////////////////////////enum MetaTags{	/////////////////////	// SAVED FIELDS		// EXIF ISO fields ..........................	kEXIFVersionField					= 0x65789000, // 1 OSTYPE	kEXIFCaptureDateField				= 0x65789003, // 20 ASCIIs	kEXIFProgramField					= 0x65788822, // 1 SHORT	kEXIFISOSpeedField					= 0x65788827, // 1 SHORT - The EXIF standard specifies several entries for this field. We keep the first non-zero entry.	kEXIFExposureBiasField				= 0x65789204, // 1 RATIONAL	kEXIFShutterSpeedField				= 0x6578829A, // 1 RATIONAL	kEXIFApertureField					= 0x6578829D, // 1 RATIONAL	kEXIFFocusDistanceField				= 0x65789206, // 1 RATIONAL	kEXIFMeteringModeField				= 0x65789207, // 1 SHORT	kEXIFLightSourceField				= 0x65789208, // 1 SHORT	kEXIFFlashField						= 0x65789209, // 1 SHORT	kEXIFFocalLengthField				= 0x6578920A, // 1 RATIONAL	kEXIFSensingMethodField				= 0x6578A217, // 1 SHORT	kEXIFDigitalZoomField				= 0x6578A404, // 1 RATIONAL (V3)	kEXIFContrastField					= 0x6578A408, // 1 SHORT (V3)	kEXIFSaturationField				= 0x6578A409, // 1 SHORT (V3)	kEXIFSharpnessField					= 0x6578A40A, // 1 SHORT (V3)		// EXIF-IVIEW fields ........................	kEXIFNoiseReductionField			= 0x6578B001, // 1 SHORT	kEXIFLensField						= 0x6578B002, // n ASCIIs	kEXIFFocusModeField					= 0x6578B003, // 1 SHORT (V3)		// GPS fields ...............................	kGPSLatitudeRefField				= 0x06770001, // 2 ASCIIs	kGPSLatitudeField					= 0x06770002, // 3 RATIONALs	kGPSLongitudeRefField				= 0x06770003, // 2 ASCIIs	kGPSLongitudeField					= 0x06770004, // 3 RATIONALs	kGPSAltitudeField					= 0x06770006, // 1 RATIONAL		// iView Structure fields ...................	kTrackInfoListField					= 0x5452434B, // n TrackInfo	kCuePointInfoListField				= 0x43484150, // n CuePointInfo	kJPEGPreviewField					= 0x4A504547, // 1 JPEG binary		// Saved property fields ...................	kMetaMakerField						= 0xA96D616B, // n ASCIIs	kMetaModelField						= 0xA96D6F64, // n ASCIIs	kMetaSoftwareField					= 0xA9737772, // n ASCIIs	kMetaFormatField					= 0xA9666D74, // n ASCIIs			//////////////////	// CONVERT TO IPTC		kMetaLabelField						= 0xA9000001, // 1 ASCII  -> kIPTCLabelField	kMetaCommentField					= 0xA9000002, // n ASCIIs -> kIPTCCaptionField	kMetaAuthorField					= 0xA9000003, // n ASCIIs -> kIPTCCreatorField	kMetaHeadlineField					= 0xA9000004, // n ASCIIs -> kIPTCHeadlineField	kMetaProductField					= 0xA9000005, // n ASCIIs -> kIPTCTitleField	kMetaOriginalSourceField			= 0xA9000006, // n ASCIIs -> kIPTCSourceField	kMetaCopyrightField					= 0xA9000007, // n ASCIIs -> kIPTCCopyrightField	kMetaURLLinkField					= 0xA9000008, // n ASCIIs -> kIPTCURLField	kMetaKeywordsField					= 0xA9000009, // n ASCIIs -> kIPTCKeywordField (multiple)			/////////////////////	// UTILITY FIELDS (Used during metadata harvest, not saved)		// Annotation block fields .................	kMetaTalkBlockField					= 0x74616C6B, // n bytes	kMetaIPTCBlockField					= 0x69707463, // n bytes	kMetaXMPBlockField					= 0x786D7020, // n bytes		// EXIF ISO fields (not saved) ..............	kEXIFUserCommentField				= 0x65789286, // n ASCIIs	kAPEXShutterSpeedField				= 0x65789201, // 1 RATIONAL	kAPEXApertureField					= 0x65789202, // 1 RATIONAL	kSpecialISOField					= 0x65789215  // 1 LONG};// ----------------------------------------------------------------------------------------------------------------------------------------------------// IVIEW-V3			                               T V		XMP MAPPING		           						QT MAPPING// ----------------------------------------------------------------------------------------------------------------------------------------------------// T - Type (R = Repeating, L = Linefeed)// V - Version Number#define kIPTCTitleField				0x00000205	//	 1		photoshop:Title									kUserDataTextProduct | kUserDataTextAlbum (mp3)#define kIPTCStatusField			0x00000207	//   2		mediapro:Status									kUserDataTextDisclaimer#define kIPTCSubjectReferenceField	0x0000020C	// R 3		Iptc4xmpCore:SubjectCode						-#define kIPTCPrimaryCategoryField	0x0000020F	//	 1		photoshop:Category								kUserDataTextGenre#define kIPTCCategoryField			0x00000214	// R 1		photoshop:SupplementalCategories				-#define kIPTCKeywordField			0x00000219	// R 1		photoshop:Keywords								kUserDataTextKeywords#define kIPTCCountryCodeField		0x00000226	//	 3		Iptc4xmpCore:CountryCode						-#define kIPTCInstructionsField		0x00000228	// L 1		photoshop:Instructions							kUserDataTextSpecialPlaybackRequirements#define kIPTCEventDateField			0x00000237	//	 1		photoshop:DateCreated							kUserDataTextCreationDate#define kIPTCCreatorField			0x00000250	//	 1		photoshop:Author								kUserDataTextAuthor | kUserDataTextOriginalArtist | kUserDataTextArtist (mp3)#define kIPTCCreatorTitleField		0x00000255	//	 1		photoshop:AuthorsPosition						-#define kIPTCCityField				0x0000025A	//	 1		photoshop:City									-#define kIPTCStateField				0x0000025F	//	 1		photoshop:State									-#define kIPTCCountryField			0x00000265	//	 1		photoshop:Country								-#define kIPTCTransmissionField		0x00000267	// L 1		photoshop:TransmissionReference					-#define kIPTCHeadlineField			0x00000269	// L 1		photoshop:Headline								kUserDataTextInformation | kUserDataTextFullName#define kIPTCCreditField			0x0000026E	//	 1		photoshop:Credit								kUserDataTextProducer#define kIPTCSourceField			0x00000273	//	 1		photoshop:Source								kUserDataTextOriginalSource#define kIPTCCopyrightField			0x00000274	// L 1		photoshop:Copyright								kUserDataTextCopyright#define kIPTCCaptionField			0x00000278	// L 1		photoshop:Caption								kUserDataTextComment#define kIPTCCaptionWriterField		0x0000027A	//	 1		photoshop:CaptionWriter							kUserDataTextWriter#define kIPTCEventField				0x00000216	//	 2		mediapro:Event									kUserDataTextDescription#define kIPTCPeopleField			0x00000276	// R 2		mediapro:People									kUserDataTextPerformers#define kIPTCLocationField			0x0000025C	//	 2		Iptc4xmpCore:Location							-												//			mediapro:Location								-#define kIPTCCreatorAddressField	0x000002D0	// L 3		Iptc4xmpCore:CreatorContactInfo/CiAdrExtadr		-#define kIPTCCreatorCityField		0x000002D1	//	 3		Iptc4xmpCore:CreatorContactInfo/CiAdrCity		-#define kIPTCCreatorStateField		0x000002D2	//	 3		Iptc4xmpCore:CreatorContactInfo/CiAdrRegion		-#define kIPTCCreatorPostcodeField	0x000002D3	//	 3		Iptc4xmpCore:CreatorContactInfo/CiAdrPcode		-#define kIPTCCreatorCountryField	0x000002D4	//	 3		Iptc4xmpCore:CreatorContactInfo/CiAdrCtry		-#define kIPTCCreatorPhoneField		0x000002D5	// L 3		Iptc4xmpCore:CreatorContactInfo/CiAdrTelWork	-#define kIPTCCreatorEmailField		0x000002D6	// L 3		Iptc4xmpCore:CreatorContactInfo/CiEmailWork		-#define kIPTCCreatorURLField		0x000002D7	// L 3		Iptc4xmpCore:CreatorContactInfo/CiUrlWork		-#define kIPTCSceneField				0x000002E0	// R 3		Iptc4xmpCore:Scene								-#define kIPTCIntellectualGenreField	0x000002E1	//   3		Iptc4xmpCore:IntellectualGenre					-#define kIPTCUsageTermsField		0x000002E2	//   3		xapRights:UsageTerms/*[@xml:lang=\'x-default\']	-#define kIPTCURLField				0x000002E3	// L 3		photoshop:WebStatement							-// Fields used for import/export, not saved in IPTC block ----------------------------------#define kIPTCLabelField				0x0000020A	//	 1		photoshop:Urgency								- (0|1|2|3|4|5|6|7|8|9) - photohop goes up to 7#define kIPTCRatingField			0x000002F2	//   3		xap:Rating										- (0|1|2|3|4|5|6|7|8|9) - photohop goes up to 5#define kIPTCUFListField			0x000002FE	// R 3		mediapro:UserFields								-#define kIPTCSetListField			0x000002FF	// R 3		mediapro:CatalogSets							-// ---------------------------------------------------------------// IMPORTANT NOTE:// Fields UFList and SetList are our own. The correct syntax is:// ---------------------------------------------------------------// kIPTCSetListField:  	"Branch 1"|"Branch 2"|"Leaf"// kIPTCUFListField:	"User field name=User field value"//						quates are optional in both cases// ---------------------------------------------------------------////////////////////////////////////////////////////////////////////////////////////////////////////																								//// Linked Lists																					////																								////////////////////////////////////////////////////////////////////////////////////////////////////#pragma mark <Linked Lists>enum { kListStart = 1, kNodeStart = 2, kListEnd	= 3, kNodeEnd = 4 };typedef char *	(*ListUnflattenProc)	(const char *path, const void *data, UInt32 size);////////////////////////////////////////////////////////////////////////////////////////////////////																								//// Data Structures																				////																								////////////////////////////////////////////////////////////////////////////////////////////////////#pragma mark <Data Structures>typedef struct CellInfoFlags{	unsigned label	  	:4;				// label (0-15 max 9)	unsigned rating	  	:4;				// rating (0-15 max 5)	unsigned unused	    :16;	unsigned lastimport	:1;				// item has just been imported - saved	unsigned intemp		:1;				// item needs to be rebuild - saved	unsigned hidden		:1;				// set/used only when creating hidden records : 0 at open	unsigned inscratch	:1;				// entry updated: 0 at open	unsigned bit_5		:1;				// unused	unsigned bit_6		:1;				// unused	unsigned bit_7		:1;				// unused	unsigned bit_8		:1;				// unused} CellInfoFlags;typedef struct CellInfo{	UInt32			catoffset;			// offset in scratch catalog	UInt32			uniqueID;			// unique item ID	CellInfoFlags   flags;				// cell flags} CellInfo;typedef struct UnitSystem{	unsigned char		dimUnits;		// dimension units		1:pix 2:in 3:cm 4:points 5:picas	unsigned char		resUnits;		// resolution units		1:dpi 2:dpcm	unsigned char		dateUnits;		// date units 			1:short 2:abrev 3:long	unsigned char		reserved;} UnitSystem;typedef struct CatalogFolder{	Byte 				frequency;		// how often do we scan	Boolean 			confirm;		// confirm before importing	Boolean				subfolders;		// scan sub folders	Boolean 			dropzone;		// use as drop zone	SInt32 				reserved_1;	SInt32 				reserved_2;	char		 		folder[kMaxFolderSize];	// unicode - url} CatalogFolder;typedef struct{	CFMutableStringRef ufname;} UserField;////////////////////////////////////////////////////////////////////////////////////////////////////																								//// Folder Stucture On Disk																		////																								/////////////////////////////////////////////////////////////////////////////////////////////////////* * On disk, using big-endian format, we have: * *	--	0) 'fldr' tag										[SInt32] -- *		1) skip bytes to skip this record					[SInt32] *	    2) unicode name length								[SInt32] *		3) legacy name length								[SInt32] *		4) alias length										[SInt32] *		5) number of items in the catalog from this folder	[SInt32]	   > fixed size header *		6) number of subfolders on disk						[SInt32] *		7) flags											[SInt32] *		8) parent folder block offset						[SInt32] *	--	9) script reference block offset					[SInt32] -- * 10) unicode name										[variable length, bytes] * 11) legacy name										[variable length, bytes] * 11) alias											[variable length, bytes] * 12) list of item uids								[variable length, SInt32s] * 13) filename lengths									[variable length, SInt32s] * 14) unicode filenames								[variable length, bytes] * 15) subfolder block offsets							[variable length, SInt32s] * * Similarly, script definitions will be prefixed thus * *	--	0) 'scpt' tag										[SInt32] -- * 		1) skip bytes										[SInt32] * */typedef struct folder_structure_header{	SInt32 modern_name_length;	SInt32 legacy_name_length;	SInt32 alias_length;	SInt32 num_items;	SInt32 num_subfolders;	SInt32 flags;	SInt32 parent_folder_offset;	SInt32 script_ref_offset;} folder_structure_header;typedef struct data_chunk_header{	SInt32 tag;	SInt32 skip_bytes;} data_chunk_header;#endif