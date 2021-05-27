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

////
- (void)runParser
{
	items = NSMutableDictionary.new;
	
	[NSThread detachNewThreadWithBlock:^{

		SInt16 status;
		SInt16 total;
		char *filename =
		//	"/Users/yan/Downloads/SAMPLE-AAA/_Hifi.ivc";
		//	"/Users/yan/Downloads/SAMPLE-AAA/_Travels.ivc";
		"/Users/yan/Downloads/SAMPLE-AAA/_Family Photos.ivc";
		
		IVCOpen(filename, &total, &status);
		NSLog(@"%s [open status = %d, file count = %d]\r", filename, status, total);
		
		void *clientInfo = (__bridge void * _Nullable)(self);
		
		IVCReport(clientInfo, _dataFeedProc, &status);
		NSLog(@"%s [read status = %d]\r", filename, status);
		
		IVCClose();
		NSLog(@"%s [close]\r", filename);

		[self performSelectorOnMainThread:@selector(logItems:) withObject:nil waitUntilDone:NO];
	}];
}

////
- (void)logItems:(id)param
{
	NSLog(@"Items %@", items);
	//	NSString *fieldName = [NSString stringWithUTF8String:kIPTC_Country];
	//	[items enumerateKeysAndObjectsUsingBlock:^(NSNumber * _Nonnull key, NSMutableDictionary * _Nonnull obj, BOOL * _Nonnull stop) {
	//		NSLog(@"Item %@ IPTC_Country = %@", key, obj[fieldName]);
	//	}];
}

////
static void _dataFeedProc(void *clientInfo,
						  const UInt32 uid,
						  const char *fieldName,
						  const UInt8 fieldType,
						  const void *fieldData
						  )
{
	ViewController *controller = (__bridge ViewController *)clientInfo;
	[controller processFeedWithUID:uid fieldName:fieldName fieldType:fieldType fieldData:fieldData];
}

////
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

@end
