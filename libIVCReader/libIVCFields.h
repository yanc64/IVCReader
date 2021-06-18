//
//  libIVCFields.h
//  libIVCReader
//
//  Created by Yannis Calotychos on 21/05/2021.
//  Copyright © 2021 Smart Toolbox Ideas. All rights reserved.
//

#ifndef __libIVCFields__
#define __libIVCFields__

// Path strings contain components separated by slash '/'
#define kPATH_FileTree				"PATH_FileTree"				// string_utf8,
#define kPATH_SetTree				"PATH_SetTree"				// string_utf8 		: repeating field
#define kPATH_KeywordTree			"PATH_KeywordTree"			// string_utf8 		: repeating field

#define kINFO_Label					"INFO_Label"				// number_sint32 	: value range 0-5
#define kINFO_Rating				"INFO_Rating"				// number_sint32 	: value range 0-9
#define kINFO_FileSize				"INFO_FileSize"				// number_uint32 	: File size
#define kINFO_Width					"INFO_Width"				// number_sint32 	: pixels
#define kINFO_Height				"INFO_Height"				// number_sint32 	: pixels
#define kINFO_Resolution			"INFO_Resolution"			// number_sint32 	: dpi
#define kINFO_Depth					"INFO_Depth"				// number_sint32 	: Image Color, bits per channel
#define kINFO_Duration				"INFO_Duration"				// number_sint32	: Audio Video Duration in seconds/10
#define kINFO_ColorSpace			"INFO_ColorSpace"			// string_utf8		: e.g. 'RGB ', 'GRAY' etc.
#define kINFO_ColorProfile			"INFO_ColorProfile"			// string_utf8
#define kINFO_URLSource			 	"INFO_URLSource"			// string_utf8

// kIPTC_xxx are all string_utf8
#define kIPTC_Headline				"IPTC_Headline"
#define kIPTC_Title					"IPTC_Title"
#define kIPTC_PrimaryCategory		"IPTC_PrimaryCategory"
#define kIPTC_IntellectualGenre		"IPTC_IntellectualGenre"
#define kIPTC_Event					"IPTC_Event"
#define kIPTC_EventDate				"IPTC_EventDate"
#define kIPTC_Creator				"IPTC_Creator"
#define kIPTC_CreatorTitle			"IPTC_CreatorTitle"
#define kIPTC_CreatorAddress  		"IPTC_CreatorAddress"
#define kIPTC_CreatorCity			"IPTC_CreatorCity"
#define kIPTC_CreatorState			"IPTC_CreatorState"
#define kIPTC_CreatorPostcode		"IPTC_CreatorPostcode"
#define kIPTC_CreatorCountry		"IPTC_CreatorCountry"
#define kIPTC_CreatorPhone			"IPTC_CreatorPhone"
#define kIPTC_CreatorEmail			"IPTC_CreatorEmail"
#define kIPTC_CreatorURL			"IPTC_CreatorURL"
#define kIPTC_Credit				"IPTC_Credit"
#define kIPTC_Source				"IPTC_Source"
#define kIPTC_Copyright				"IPTC_Copyright"
#define kIPTC_Transmission			"IPTC_Transmission"
#define kIPTC_UsageTerms			"IPTC_UsageTerms"
#define kIPTC_URL					"IPTC_URL"
#define kIPTC_Location				"IPTC_Location"
#define kIPTC_City					"IPTC_City"
#define kIPTC_State					"IPTC_State"
#define kIPTC_Country				"IPTC_Country"
#define kIPTC_CountryCode			"IPTC_CountryCode"
#define kIPTC_Instructions			"IPTC_Instructions"
#define kIPTC_Status				"IPTC_Status"
#define kIPTC_CaptionWriter			"IPTC_CaptionWriter"
#define kIPTC_Caption				"IPTC_Caption"
#define kIPTC_People				"IPTC_People"				// string_utf8 		: repeating field
#define kIPTC_Keyword				"IPTC_Keyword"				// string_utf8 		: repeating field
#define kIPTC_Category				"IPTC_Category"				// string_utf8 		: repeating field
#define kIPTC_Scene					"IPTC_Scene"				// string_utf8 		: repeating field
#define kIPTC_SubjectReference		"IPTC_SubjectReference"		// string_utf8 		: repeating field

#define kEXIF_Software				"EXIF_Software"				// string_utf8
#define kEXIF_Maker					"EXIF_Maker"				// string_utf8
#define kEXIF_Model					"EXIF_Model"				// string_utf8
#define kEXIF_Lens					"EXIF_Lens"					// string_utf8
#define kEXIF_CaptureDate			"EXIF_CaptureDate"			// number_uint32	: in seconds
#define kEXIF_ShutterSpeed			"EXIF_ShutterSpeed"			// number_rational
#define kEXIF_Aperture				"EXIF_Aperture"				// "
#define kEXIF_FocusDistance			"EXIF_FocusDistance"		// "
#define kEXIF_FocalLength			"EXIF_FocalLength"			// "
#define kEXIF_ExposureBias			"EXIF_ExposureBias"			// "
#define kEXIF_MeteringMode			"EXIF_MeteringMode"			// number_sint32
#define kEXIF_Program				"EXIF_Program"				// "
#define kEXIF_LightSource			"EXIF_LightSource"			// "
#define kEXIF_Flash					"EXIF_Flash"				// " (this is some kind of bitword)
#define kEXIF_SensingMethod			"EXIF_SensingMethod"		// "
#define kEXIF_FocusMode				"EXIF_FocusMode"			// "
#define kEXIF_ISOSpeed				"EXIF_ISOSpeed"				// "

#define kGPS_Altitude		      	"GPS_Altitude"				// number_rational
#define kGPS_Latitude		      	"GPS_Latitude"				// number_rational3
#define kGPS_Longitude		      	"GPS_Longitude"				// number_rational3
#define kGPS_LatitudeRef			"GPS_LatitudeRef"			// string_utf8		: N/S
#define kGPS_LongitudeRef			"GPS_LongitudeRef"			// string_utf8		: W/E

// User Fields are all string_utf8
#define kUF_01						"UF_01"
#define kUF_02						"UF_02"
#define kUF_03						"UF_03"
#define kUF_04						"UF_04"
#define kUF_05						"UF_05"
#define kUF_06						"UF_06"
#define kUF_07						"UF_07"
#define kUF_08						"UF_08"
#define kUF_09						"UF_09"
#define kUF_10						"UF_10"
#define kUF_11						"UF_11"
#define kUF_12						"UF_12"
#define kUF_13						"UF_13"
#define kUF_14						"UF_14"
#define kUF_15						"UF_15"
#define kUF_16						"UF_16"

//////////////////

#define kUF_Definition				"UF_Definition"

#endif
