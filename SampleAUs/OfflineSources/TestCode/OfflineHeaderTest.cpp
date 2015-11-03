/*	Copyright: 	© Copyright 2003 Apple Computer, Inc. All rights reserved.

	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
			("Apple") in consideration of your agreement to the following terms, and your
			use, installation, modification or redistribution of this Apple software
			constitutes acceptance of these terms.  If you do not agree with these terms,
			please do not use, install, modify or redistribute this Apple software.

			In consideration of your agreement to abide by the following terms, and subject
			to these terms, Apple grants you a personal, non-exclusive license, under Apple’s
			copyrights in this original Apple software (the "Apple Software"), to use,
			reproduce, modify and redistribute the Apple Software, with or without
			modifications, in source and/or binary forms; provided that if you redistribute
			the Apple Software in its entirety and without modifications, you must retain
			this notice and the following text and disclaimers in all such redistributions of
			the Apple Software.  Neither the name, trademarks, service marks or logos of
			Apple Computer, Inc. may be used to endorse or promote products derived from the
			Apple Software without specific prior written permission from Apple.  Except as
			expressly stated in this notice, no other rights or licenses, express or implied,
			are granted by Apple herein, including but not limited to any patent rights that
			may be infringed by your derivative works or by other works in which the Apple
			Software may be incorporated.

			The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
			WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
			WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
			PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
			COMBINATION WITH YOUR PRODUCTS.

			IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
			CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
			GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
			ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
			OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
			(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
			ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
//#include "OfflineHeaderAdditions.h"
#include <AudioToolbox/AudioToolbox.h>
#include "CAStreamBasicDescription.h"

#define OFFLINE_AU_CMD 		"[-u TYPE SUBTYPE MANU] The offline units component descriptor - must be specified before file!!!\n\t"
#define INPUT_FILE	 		"[-f /Path/To/File] The file that is to be processed. The output file will have the same name and location as the input file with the component's subType appended to the name of the file\n\t"
#define OVERWRITE_CMD		"[-o] Specify this option if you want to overwrite an existing output file\n"

static char* usageStr = "Usage: OfflineTest\n\t" 
				OFFLINE_AU_CMD 
				INPUT_FILE
				OVERWRITE_CMD;



OSType str2OSType (const char * inString);
OSStatus OutputFile (const char 							*inputFilename, 
						UInt32								inFileType,
						OSType 								subType,
						bool								overwrite, 
						const AudioStreamBasicDescription 	&desc, 
						AudioFileID 						&outFile);
OSStatus InputFile (const char *filename, AudioFileID &outFile);

extern OSStatus Process (AudioUnit						&unit,
					ComponentDescription 				&compDesc, 
					AudioFileID 						&inputFileID, 
					CAStreamBasicDescription 			&desc, 
					AudioFileID 						&outputFileID);


int main (int argc, const char * argv[]) 
{
	char* filePath = NULL;
	bool overwrite = false;
	ComponentDescription	compDesc = {0, 0, 0, 0, 0};
	AudioFileID inputFileID = 0;
	AudioFileID outputFileID = 0;
	CAStreamBasicDescription desc;
	AudioUnit theUnit = 0;
	
	setbuf (stdout, NULL);
	
	for (int i = 1; i < argc; ++i)
	{
		if (strcmp (argv[i], "-u") == 0) {
            if ( (i + 3) < argc ) {                
                compDesc.componentType = str2OSType (argv[i + 1]);
                compDesc.componentSubType = str2OSType (argv[i + 2]);
                compDesc.componentManufacturer = str2OSType (argv[i + 3]);
				Component comp = FindNextComponent (NULL, &compDesc);
				if (comp == NULL)
					break;
				OpenAComponent (comp, &theUnit);
				i += 3;
			} else {
				printf ("Which Component:\n%s", usageStr);
				return -1;
			}
		}
		else if (strcmp (argv[i], "-f") == 0) {
			filePath = const_cast<char*>(argv[++i]);
			printf ("Input File:%s\n", filePath);
		}
		else if (strcmp (argv[i], "-o") == 0) {
			overwrite = true;
		}
		else {
			printf ("%s\n", usageStr);
			return -1;
		}
	}
	
	if (compDesc.componentType == 0) {
		printf ("Must specify AU:\n%s\n", usageStr);
		return -1;
	}
	
	if (theUnit == 0) {
		printf ("Can't find specified unit\n");
		return -1;
	}
	
	if (filePath == NULL) {
		printf ("Must specify file to process:\n%s\n", usageStr);
		return -1;
	}
	
	OSStatus result = 0;
	if (result = InputFile (filePath, inputFileID)) {
		printf ("Result = %ld, parsing input file, exit...\n", result);
		return result;
	}
			
		
	UInt32 fileType;
	UInt32 size = sizeof (fileType);
	result = AudioFileGetProperty (inputFileID, kAudioFilePropertyFileFormat, &size, &fileType);
	if (result) {
		printf ("Error getting File Type of input file:%ld, exit...\n", result);
		return result;
	}
	size = sizeof (desc);
	result = AudioFileGetProperty (inputFileID, kAudioFilePropertyDataFormat, &size, &desc);
	if (result) {
		printf ("Error getting File Format of input file:%ld, exit...\n", result);
		return result;
	}
	if (desc.IsPCM() == false) {
		printf ("Only processing linear PCM file types and data:\n");
		desc.Print();
		return -1;
	}
	result = OutputFile (filePath, fileType, compDesc.componentSubType, overwrite, desc, outputFileID);
	if (result) {
		printf ("Error creating output file:%ld, exit...\n", result);
		return result;
	}	
	
// at this point we're ready to process	
	return Process (theUnit, compDesc, inputFileID, desc, outputFileID);
}

OSStatus InputFile (const char *filename, AudioFileID &outFile)
{
	FSRef fsRef;
	OSStatus result = noErr;
	require_noerr (result = FSPathMakeRef ((const UInt8*)filename, &fsRef, 0), home);

	require_noerr (result = AudioFileOpen (&fsRef, fsRdPerm, 0, &outFile), home); 

home:
	return result;
}

OSStatus OutputFile (const char 							*inputFilename, 
						UInt32								inFileType,
						OSType 								subType, 
						bool								overwrite,
						const AudioStreamBasicDescription 	&desc, 
						AudioFileID 						&outFile)
{
	int len = strlen (inputFilename);
	char* copyStr = (char*)malloc (len);
	strcpy (copyStr, inputFilename);
	int i = len;
	FSRef parentDir;
	char filename[512];
	memset (filename, 0, 512);
	OSStatus result = fnfErr;
	CFStringRef cfstr = NULL;
	
	while (--i >= 0) {
		if (copyStr[i] == '/') {// find the delimiter for the directory
			copyStr[i] = 0;
			require_noerr (result = FSPathMakeRef ((const UInt8*)copyStr, &parentDir, 0), home);
			int j = len;
			while (--j > i) { //find the ext
				if (inputFilename[j] == '.')
					break;
			}
				// copy the name itself first
			memcpy (filename, inputFilename + i + 1, (len - i - (len - j + 1)));
				// append the -subType suffix before the .ext
			char* temp = filename + strlen(filename);
			temp += sprintf (temp, "-%4.4s", (char*)&subType);
				// append the file type .ext
			memcpy (temp, inputFilename + j, (len - j));
			
			printf ("Output File: Dir:%s, FileName:%s\n", copyStr, filename);
			
			cfstr = CFStringCreateWithCString (NULL, filename, kCFStringEncodingASCII);
			
			FSRef outRef;
			result = AudioFileCreate (&parentDir, cfstr, inFileType, &desc, 0/*flags*/, &outRef, &outFile);
			
			if (result == dupFNErr) { // file already exits - initialie it?
				if (overwrite) {
					len += 5;
					char* name = (char*)malloc (len);
					strcpy (name, copyStr);
					strcat (name, "/");
					strcat (name, filename);
	
					require_noerr (result = FSPathMakeRef ((const UInt8*)name, &outRef, 0), home);
					
					require_noerr (result = AudioFileInitialize (&outRef, inFileType,
														&desc, 0, &outFile), home);
					free (name);
				} else {
					printf ("Output File exists - specify overwrite\n");
				}
			}
			break;
		}
	}
home:
	if (cfstr)
		CFRelease (cfstr);
	free (copyStr);
	return result;
}
	

OSType str2OSType (const char * inString)
{
    OSType retval;
    char workingString[5];
    
    workingString[4] = 0;
    workingString[0] = workingString[1] = workingString[2] = workingString[3] = ' ';
    memcpy (workingString, inString, strlen(inString));

    retval = 	*(workingString + 0) <<	24	|
                *(workingString + 1) <<	16	|
                *(workingString + 2) <<	8	|
                *(workingString + 3);

    return retval;
}
