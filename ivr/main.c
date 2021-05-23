//
//  main.c
//  ivr
//
//  Created by Yannis Calotychos on 19/5/21.
//

#include <stdio.h>
#include "iview_read.h"

////
int main(int argc, const char * argv[]) {

	long status;
	char *filename = "/Users/yan/Downloads/SAMPLE-AAA/_Hifi.ivc";
	printf("reading catalog %s\r", filename);
	IVCOpen(filename, &status);
	IVCClose();
	
	if( status == 0 )
		printf("completed with no errors\r");
	else
		printf("couldn't read catalog. (error %ld)\r", status);
	return 0;
}


