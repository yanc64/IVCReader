//
//  ViewController.m
//  Sample App
//
//  Created by Yannis Calotychos on 27/5/21.
//

#import "libIVCReader.h"
#import "ViewController.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
@interface ViewController ()
{
	__weak IBOutlet NSTextField *text1;
	__weak IBOutlet NSTextField *text2;
	__weak IBOutlet NSTextField *text3;

	NSMutableDictionary <NSNumber*, NSMutableDictionary*> *items;
}
@end

@implementation ViewController

////
- (void)viewDidLoad
{
	[super viewDidLoad];
	[self runParser];
}


///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark -

#define _logAsDictionaries

////
- (void)runParser
{
	char *filename =
	//"/Users/yan/Downloads/Sample_Catalogs/Hifi.ivc";
	"/Users/yan/Downloads/Sample_Catalogs/Travels.ivc";
	//"/Users/yan/Downloads/Sample_Catalogs/Family Photos.ivc";
	//"/Users/yan/Downloads/Sample_Catalogs/Catalog-1.mpcatalog";
	//"/Users/yan/Downloads/Sample_Catalogs/Catalog-unsplash (no read).ivc";
	//"/Users/yan/Downloads/Sample_Catalogs/Crash on save.ivc";

	items = NSMutableDictionary.new;
	
	text1.stringValue = [NSString stringWithCString:filename encoding:NSUTF8StringEncoding];

	[NSThread detachNewThreadWithBlock:^{
		
		SInt16 status;
		SInt16 total;
		
		bool wantsInfo = true;
		bool wantsIptc = true;
		bool wantsExif = false;
		bool wantsPict = false;

		// [1] Open file and watch for status return value
		IVCOpen(filename, wantsInfo, wantsIptc, wantsExif, wantsPict, &total, &status);
		[self->text2 performSelectorOnMainThread:@selector(setStringValue:) withObject:( status == 0 ) ? [NSString stringWithFormat:@"File opened successfully. File count = %hd", total]: [NSString stringWithFormat:@"Error opening file [%hd]", status] waitUntilDone:YES];
		
		if( status != 0 )
			return;
		
		/////////////////////
		// [2] Report and data found in file using _dataFeedProc callback
		void *client = (__bridge void * _Nullable)(self);
		IVCReport(client, _dataFeedProc, &status);
		
		if( status )
			[self->text2 performSelectorOnMainThread:@selector(setStringValue:) withObject:[NSString stringWithFormat:@"Error reading file [%hd]", status] waitUntilDone:YES];
		else
			[self->text3 performSelectorOnMainThread:@selector(setStringValue:) withObject:@"File parsing done!" waitUntilDone:YES];
		
		// [3] Close file
		IVCClose();
		
#ifdef _logAsDictionaries
		[self performSelectorOnMainThread:@selector(logItems:) withObject:nil waitUntilDone:YES];
#endif
		
	}];
}

////
- (void)logItems:(id)param
{
	NSArray <NSNumber *> *array = [items.allKeys sortedArrayUsingSelector:@selector(compare:)];
	for(id key in array)
		NSLog(@"%@", items[key]);
}

////
static void _dataFeedProc(void *client,
						  const UInt32 uid,
						  const char *fieldName,
						  const UInt8 fieldType,
						  const void *fieldData
						  )
{
	// can't use self in 'C' so we need to extract controller passed as an argument
	ViewController *controller = (__bridge ViewController *)client;
	[controller processFeedWithUID:uid fieldName:fieldName fieldType:fieldType fieldData:fieldData];
}

////
#ifdef _logAsDictionaries
// This methhod will collate all data into a dictionary
- (void)processFeedWithUID:(const UInt32 )uid fieldName:(const char *)fieldName fieldType:(const UInt8)fieldType fieldData:(const void *)fieldData
{
	NSNumber *nuid = @(uid);
	NSMutableDictionary *properties = items[nuid];
	
	if( properties == nil )
	{
		properties = NSMutableDictionary.new;
		items[nuid] = properties;
	}
	
	NSString *name = [NSString stringWithUTF8String:fieldName];
	
	switch( fieldType )
	{
		case string_utf8		:
			if( IVCIsRepeatingField(fieldName) )
			{
				if( properties[name] )
					[(NSMutableArray *)properties[name] addObject:[NSString stringWithUTF8String:fieldData]];
				else
					properties[name] = [NSMutableArray arrayWithObject:[NSString stringWithUTF8String:fieldData]];
			}
			else
			{
				properties[name] = [NSString stringWithUTF8String:fieldData];
			}
			break;
			
		case number_sint32    	: properties[name] = [NSNumber numberWithInt:*(SInt32 *)fieldData]; break;
		case number_uint32    	: properties[name] = [NSNumber numberWithUnsignedInt:*(UInt32 *)fieldData]; break;
		case number_rational   	: properties[name] = @[[NSNumber numberWithInt:*(SInt32 *)&fieldData[0]],
													   [NSNumber numberWithInt:*(SInt32 *)&fieldData[4]]]; break;
		case number_rational3  	: properties[name] = @[[NSNumber numberWithInt:*(SInt32 *)&fieldData[0]],
													   [NSNumber numberWithInt:*(SInt32 *)&fieldData[4]],
													   [NSNumber numberWithInt:*(SInt32 *)&fieldData[8]],
													   [NSNumber numberWithInt:*(SInt32 *)&fieldData[12]],
													   [NSNumber numberWithInt:*(SInt32 *)&fieldData[16]],
													   [NSNumber numberWithInt:*(SInt32 *)&fieldData[20]]]; break;
		case data_feed :
		{
			// binary_data (32-bit size + followed by data)
			const UInt32 *len = (const UInt32 *)fieldData;
			NSData *data = [NSData dataWithBytes:&fieldData[4] length:*len];
			if( data )
			{
				properties[name] = data;
				// Test data
				// NSImage *image = [[NSImage alloc]initWithData:data];
				// if( image )
				//	NSLog(@"image size = %@", NSStringFromSize(image.size));
			}
		} break;
	}
}
////
#else
// This methhod will just log data as they come
- (void)processFeedWithUID:(const UInt32 )uid fieldName:(const char *)fieldName fieldType:(const UInt8)fieldType fieldData:(const void *)fieldData
{
	NSString *name = [NSString stringWithUTF8String:fieldName];
	NSString *value;
	
	switch( fieldType )
	{
		case string_utf8		: value = [NSString stringWithUTF8String:fieldData]; break;
		case number_sint32    	: value = [NSString stringWithFormat:@"%d", *(SInt32 *)fieldData]; break;
		case number_uint32    	: value = [NSString stringWithFormat:@"%d", *(UInt32 *)fieldData]; break;
		case number_rational    : value = [NSString stringWithFormat:@"%d/%d",
										   *(SInt32 *)&fieldData[0],
										   *(SInt32 *)&fieldData[4]
										   ]; break;
		case number_rational3   : value = [NSString stringWithFormat:@"%d/%d %d/%d %d/%d",
										   *(SInt32 *)&fieldData[0],
										   *(SInt32 *)&fieldData[4],
										   *(SInt32 *)&fieldData[8],
										   *(SInt32 *)&fieldData[12],
										   *(SInt32 *)&fieldData[16],
										   *(SInt32 *)&fieldData[20]
										   ]; break;
		default			    	: value = @"Other... ?!!?!?!?!?!";
	}
	
	NSLog(@"[%10u] %@ = %@", (unsigned int)uid, name, value);
}
#endif

@end
