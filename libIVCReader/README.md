# About libIVCReader

libIVCReader is a C library for reading media catalog files (.ivc and maybe others), created with iView MediaPro 2.5 and later incarnations of the product.

## Using libIVCReader

To use libIVCReader, you will need to  
1. Add libIVCReader.a static library to your project
2. Include files : libIVCReader.h and libIVCFields.h

You can run the library in a thread.

## The Functions

libIVCReader provides 3 functions :

IVCOpen( )
Will open the file and return a status (0 if it's OK to continue), and the count of files referenced by the catalog file.

IVCReport( )
libIVCReader will call you callback to report items found in the catalog.
An item is reported with an ID (the ID of the file), a property and a property type and value.
Note that items with IO=0 refer to name definitions of user fields.

IVCClose( )
Closes the file.
