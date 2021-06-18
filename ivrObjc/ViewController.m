//
//  ViewController.m
//  ivrObjc
//
//  Created by Yannis Calotychos on 27/5/21.
//

#import "libIVCReader.h"
#import "ViewController.h"

@implementation ViewController
{
	NSMutableDictionary <NSNumber*, NSMutableDictionary*> *items;
}

- (void)viewDidLoad {
	[super viewDidLoad];

	// Do any additional setup after loading the view.
	[self runParser];
}


- (void)setRepresentedObject:(id)representedObject {
	[super setRepresentedObject:representedObject];

	// Update the view, if already loaded.
}


///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark -

#define _logAsDictionaries

////
- (void)runParser
{
	items = NSMutableDictionary.new;
	
	[NSThread detachNewThreadWithBlock:^{

		SInt16 status;
		SInt16 total;
		char *filename =
		//"/Users/yan/Downloads/Sample_Catalogs/Hifi.ivc";
		"/Users/yan/Downloads/Sample_Catalogs/Travels.ivc";
		//"/Users/yan/Downloads/Sample_Catalogs/Family Photos.ivc";
		//"/Users/yan/Downloads/Sample_Catalogs/Catalog-1.mpcatalog";
		// "/Users/yan/Downloads/Sample_Catalogs/Catalog-unsplash (no read).ivc";
		// "/Users/yan/Downloads/Sample_Catalogs/Crash on save.ivc";


		IVCOpen(filename, &total, &status);
		NSLog(@"%s [open status = %d, file count = %d]\r", filename, status, total);
		
		void *clientInfo = (__bridge void * _Nullable)(self);
		
		IVCReport(clientInfo, _dataFeedProc, &status);
		NSLog(@"%s [read status = %d]\r", filename, status);
		
		IVCClose();
		NSLog(@"%s [close]\r", filename);
		
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
static void _dataFeedProc(void *clientInfo,
						  const UInt32 uid,
						  const char *fieldName,
						  const UInt8 fieldType,
						  const void *fieldData
						  )
{
	// can't use self in 'C' so we need to extract controller passed as an argument
	ViewController *controller = (__bridge ViewController *)clientInfo;
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
