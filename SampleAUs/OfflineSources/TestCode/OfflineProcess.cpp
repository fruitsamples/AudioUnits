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
#include "AUBuffer.h"


#define PRINT_MARKS 				\
	printf ("| ");					\
	for (int i = 0; i < 38; ++i)	\
		printf ("  ");				\
	printf ("|\n");


UInt64 renderTime_Start_Host = 0;
UInt64 renderTime_Accumulate_Host = 0;
UInt64 totalSamplesProcessed = 0; //includes tail time...
	
AURenderCallbackStruct			theRenderCallback;

struct InputCallbackInfo {
	AudioConverterRef 	theInputConverter;
	AudioFileID			theInputFile;
	UInt64				theStartSample;
	UInt64				theFilePacketCount;
};
InputCallbackInfo theInputCallbackInfo;

static void* theFileInputData = NULL;
static 	const UInt32 processPackets = 512;


OSStatus 	ToFloatInputProc (AudioConverterRef					inAudioConverter,
								UInt32							*ioNumberDataPackets,
								AudioBufferList					*ioData,
								AudioStreamPacketDescription	**outDataPacketDescription,
								void							*inUserData);

OSStatus 	FromFloatInputProc (AudioConverterRef				inAudioConverter,
								UInt32							*ioNumberDataPackets,
								AudioBufferList					*ioData,
								AudioStreamPacketDescription	**outDataPacketDescription,
								void							*inUserData);


OSStatus 	TheRenderCallback (void 					*inRefCon, 
						AudioUnitRenderActionFlags		*inActionFlags,
						const AudioTimeStamp 			*inTimeStamp, 
						UInt32 							inBusNumber,
						UInt32							inNumFrames, 
						AudioBufferList 				*ioData);



OSStatus Process (AudioUnit								&unit,
					ComponentDescription 				&compDesc, 
					AudioFileID 						&inputFileID, 
					CAStreamBasicDescription 			&inOutFormatDesc, 
					AudioFileID 						&outputFileID)
{
	OSStatus result;
	
		// we're only processing linear PCM so make format desc out of the files one, but Float32 for the AU
	CAStreamBasicDescription floatDesc (inOutFormatDesc);
	floatDesc.SetCanonical (inOutFormatDesc.NumberChannels(), false);

		// these are our converter from and back to the file format
	AudioConverterRef toFloatConverter = 0;
	AudioConverterRef fromFloatConverter = 0;

		// these are our buffer lists we use for the conversion
	AUBufferList outputABL, renderABL;
	
		// we need to know how many frames of sample data are in the file that we want to process
			// set up our processing buffers
		// how many samples do we have to process
	UInt64 totalNumSamples, totalOutputSamples;
	UInt32 propSize = sizeof (totalNumSamples);
	require_noerr (result = AudioFileGetProperty (inputFileID, kAudioFilePropertyAudioDataPacketCount, 
														&propSize, &totalNumSamples), home);

		// set up our converters
	require_noerr (result = AudioConverterNew (&inOutFormatDesc, &floatDesc, &toFloatConverter), home);
	printf ("\nConverting from file to processing format:\n");
	CAShow (toFloatConverter);

	require_noerr (result = AudioConverterNew (&floatDesc, &inOutFormatDesc, &fromFloatConverter), home);
	printf ("\nConverting from processing back to the file format:\n");
	CAShow (fromFloatConverter);
	printf ("\n");
	
		// set up our AU
	require_noerr (result = AudioUnitSetProperty (unit, kAudioUnitProperty_StreamFormat,
								kAudioUnitScope_Input, 0, &floatDesc, sizeof(floatDesc)), home);
								 
	require_noerr (result = AudioUnitSetProperty (unit, kAudioUnitProperty_StreamFormat,
								kAudioUnitScope_Output, 0, &floatDesc, sizeof(floatDesc)), home); 
								
	theInputCallbackInfo.theInputConverter = toFloatConverter;
	theInputCallbackInfo.theInputFile = inputFileID;
	theInputCallbackInfo.theFilePacketCount = totalNumSamples;
	
	theRenderCallback.inputProc = TheRenderCallback;
	theRenderCallback.inputProcRefCon = &theInputCallbackInfo;
        
    require_noerr (result = AudioUnitSetProperty (unit, kAudioUnitProperty_SetRenderCallback, 
											kAudioUnitScope_Input, 0,
											&theRenderCallback, sizeof(theRenderCallback)), home);
	
	require_noerr (result = AudioUnitInitialize (unit), home);

			// tell the offline unit how many input samples we wish to process...
	require_noerr (result = AudioUnitSetProperty (unit, kAudioOfflineUnitProperty_InputSize,
												kAudioUnitScope_Global, 0,
												&totalNumSamples, sizeof(totalNumSamples)), home);
	
			// get the unit to tell us how many output samples it is likely to produce
			// this is only an estimate and the render flags MUST be used to actually do the work
	propSize = sizeof (totalOutputSamples);
	require_noerr (result = AudioUnitGetProperty (unit, kAudioOfflineUnitProperty_OutputSize,
												kAudioUnitScope_Global, 0,
												&totalOutputSamples, &propSize), home);
			
		// prepare our buffers for processing...
	renderABL.Allocate (floatDesc, processPackets);
	renderABL.PrepareBuffer (floatDesc, processPackets);		

	outputABL.Allocate (inOutFormatDesc, processPackets);
	outputABL.PrepareBuffer (inOutFormatDesc, processPackets);
	
	UInt32 maxFrames;
	propSize = sizeof (maxFrames);
	require_noerr (result = AudioUnitGetProperty (unit, kAudioUnitProperty_MaximumFramesPerSlice,
											kAudioUnitScope_Global, 0, &maxFrames, &propSize), home);
	
	UInt32 sizeofOfFileInputData;
		// this is what we need to read from the file..
		// we can be asked to read up to max frames
		// so then we need to multiple by numChannels (interleaved file data) and size of the samples.
	sizeofOfFileInputData = maxFrames * inOutFormatDesc.SampleWordSize() * inOutFormatDesc.NumberChannels();
	theFileInputData = static_cast<void*>(malloc (sizeofOfFileInputData));

		// the time stamp we use with the AU Render - only sample count is valid
	AudioUnitRenderActionFlags renderFlags;
	AudioTimeStamp time;
	memset (&time, 0, sizeof(time));
	time.mFlags = kAudioTimeStampSampleTimeValid;
	
	UInt64 framesDoneWritePos;
	framesDoneWritePos = 0; 
	int lastDone;
	
#if 0
// need to implement this property with the reverse unit
// and redo the render call so that it handles this properly
// but this is essentially what the host would need to do...
	UInt32 shouldPreflight;
	propSize = sizeof (shouldPreflight);
	require_noerr (result = AudioUnitGetProperty (unit, kAudioUnitOfflineProperty_PreflightRequirements,
											kAudioUnitScope_Global, 0, &shouldPreflight, &propSize), home);

		// here we need to do the preflight loop - we don't expect any data back, but have to 
		// give the offline unit all of its input data to allow it to prepare its processing
		// we do this if preflight is required (or optional)
		// in a normal hosting environent the view/UI host would present a preflight button..
//	if (shouldPreflight) 
#endif
	if (true) 
	{
		printf ("Doing Preflight\n");
		PRINT_MARKS
		
		lastDone = 0;
		while (!(renderFlags & kAudioOfflineUnitRenderAction_Complete)) {
			renderFlags = kAudioOfflineUnitRenderAction_Preflight;
			require_noerr (result = AudioUnitRender (unit, &renderFlags, &time, 0, processPackets, 
												&renderABL.GetBufferList()), home);
			time.mSampleTime += processPackets;

			int done = int(time.mSampleTime / totalOutputSamples * 40);
			if (done != lastDone) {
				printf ("* ");
				lastDone = done;
			}
		}
		
		printf ("\n");
		
		// reset our time stamp to zero...
		time.mSampleTime = 0;
	}
	
		// the loop breaks based on different conditions for the different AU's we're rendering
	bool isOfflineFinished;
	isOfflineFinished = false;

	printf ("\nDoing Process\n");
	PRINT_MARKS

	lastDone = 0;
	while (!isOfflineFinished) 
	{
		UInt32 numFramesToProcess = processPackets;
		
		renderFlags = kAudioOfflineUnitRenderAction_Render;
		

			// process the samples (the unit's input callback will read the samples
			// from the file and convert them to float for processing
		require_noerr (result = AudioUnitRender (unit, &renderFlags, &time, 0, numFramesToProcess, 
												&renderABL.GetBufferList()), home);

		renderTime_Accumulate_Host += (AudioGetCurrentHostTime() - renderTime_Start_Host);
		UInt32 numPacketsToWrite = numFramesToProcess;
		
		if (renderFlags & kAudioOfflineUnitRenderAction_Complete) 
		{
			// we're finished with the offline rendering
			// now we have to figure out how many samples we got this last time...
			numPacketsToWrite = renderABL.GetBufferList().mBuffers[0].mDataByteSize / sizeof(Float32);
			isOfflineFinished = true;
		}
		totalSamplesProcessed += numPacketsToWrite;
				
		outputABL.Allocate (inOutFormatDesc, numPacketsToWrite);
		outputABL.PrepareBuffer (inOutFormatDesc, numPacketsToWrite);

			// convert our float processed samples back to the file format
		require_noerr (result = AudioConverterFillComplexBuffer (fromFloatConverter, 
															FromFloatInputProc, 
															&renderABL.GetBufferList(),
															&numPacketsToWrite, 
															&outputABL.GetBufferList(), 
															0), home);
															
			// write out the reconverted and processed samples to the file
		require_noerr (result = AudioFileWritePackets (outputFileID, false, 
												outputABL.GetBufferList().mBuffers[0].mDataByteSize, 
												NULL, framesDoneWritePos, 
												&numPacketsToWrite, 
												outputABL.GetBufferList().mBuffers[0].mData), home);
			
			// ok - we increment the time stamp by the amount we processed.
		time.mSampleTime += numFramesToProcess;
			// increment our counter by the amount we wrote
		framesDoneWritePos += numPacketsToWrite;

		int done = int(time.mSampleTime / totalOutputSamples * 40);
		if (done != lastDone) {
			printf ("* ");
			lastDone = done;
		}
	}

	printf ("\n");
	
home:

// cleanup
	AudioFileClose (inputFileID);
	AudioFileClose (outputFileID);

	AudioConverterDispose (toFloatConverter);
	AudioConverterDispose (fromFloatConverter);
	
	free (theFileInputData);
	
	CloseComponent (unit);
	
	return result;
}

OSStatus 	ToFloatInputProc (AudioConverterRef					inAudioConverter,
								UInt32							*ioNumberDataPackets,
								AudioBufferList					*ioData,
								AudioStreamPacketDescription	**outDataPacketDescription,
								void							*inUserData)
{
	InputCallbackInfo &info = *(InputCallbackInfo*)inUserData;
	OSStatus result;
	UInt32 outNumBytes;
	
	result = AudioFileReadPackets (info.theInputFile, false, &outNumBytes,
													NULL, SInt64(info.theStartSample), 
													ioNumberDataPackets, theFileInputData);
	
	ioData->mBuffers[0].mData = theFileInputData;
	ioData->mBuffers[0].mDataByteSize = outNumBytes;
	
	info.theStartSample += *ioNumberDataPackets; // we do this just in case we get called again by the converter
	
	if (result == eofErr)
		result = noErr;
	
	return result;
}

OSStatus 	FromFloatInputProc (AudioConverterRef				inAudioConverter,
								UInt32							*ioNumberDataPackets,
								AudioBufferList					*ioData,
								AudioStreamPacketDescription	**outDataPacketDescription,
								void							*inUserData)
{
	AudioBufferList *list = static_cast<AudioBufferList*>(inUserData);

	memcpy (ioData, list, offsetof(AudioBufferList, mBuffers[list->mNumberBuffers]));
	
	// we have to reset the data count or we tell the converter that we have more than we actually do
	for (UInt32 i = 0; i < ioData->mNumberBuffers; ++i)
		ioData->mBuffers[i].mDataByteSize = *ioNumberDataPackets * sizeof(Float32);
		
	return noErr;
}

OSStatus 	TheRenderCallback (void 					*inRefCon, 
						AudioUnitRenderActionFlags		*inActionFlags,
						const AudioTimeStamp 			*inTimeStamp, 
						UInt32 							inBusNumber,
						UInt32							inNumFrames, 
						AudioBufferList 				*ioData)
{
	OSStatus result = noErr;
	
	InputCallbackInfo &info = *(InputCallbackInfo*)inRefCon;

	info.theStartSample = (UInt64)inTimeStamp->mSampleTime;
		//first check to see that we're not being asked for data past our file end.
	if (info.theStartSample >= info.theFilePacketCount) 
	{
		// we've nothing to do here but to return with silence
		// we really shouldn't get here!
		AUBufferList::ZeroBuffer (*ioData);
		*inActionFlags |= kAudioUnitRenderAction_OutputIsSilence;
	} 
	else
	{
		UInt32 numFrames = inNumFrames;
		UInt32 remainderFrames = 0;
		
			// second test is are we trying to read past the end of the file?
			// if we don't do this here the converter asks us for more data than we have, 
			// but we won't have had the chance to update from where that data should come from
			// so we read from the same place twice...
			// this is an artefact of the fact that we're passing in the start count - we could also manage
			// that from converter's input proc by resetting the start count, but we have to deal with the situation
			// here that we might get less back than we have, so might as well localise this all to here...
			
			// then the converter's input proc can be really dumb...
		if (info.theStartSample + numFrames >= info.theFilePacketCount) {
			remainderFrames = info.theStartSample + numFrames - info.theFilePacketCount;
			numFrames -= remainderFrames;
		}
		
		result = AudioConverterFillComplexBuffer (info.theInputConverter, 
															ToFloatInputProc, 
															inRefCon,
															&numFrames, 
															ioData, 
															0);
			
		// ok - we've now got the data from the converter we need.. if we have any remainer frames we need to 
		// make sure we silence out the rest of the buffer...	
		if (remainderFrames) {
				// first we "know" that we're dealing with deinterleaved Float32 samples, so a sample is sizeof(Float32)
			UInt32 numBytesToZero = remainderFrames * sizeof(Float32);
			UInt32 offsetIntoBuffer = inNumFrames - remainderFrames;
				//we're still passing back the same amount of data to the AU - just the end of it is silenced...
			for (UInt32 i = 0; i < ioData->mNumberBuffers; ++i) {
				Float32* sampleData = (Float32*)ioData->mBuffers[i].mData;
				sampleData += offsetIntoBuffer;
				memset (sampleData, 0, numBytesToZero);
			}
		}
	}
	
	renderTime_Start_Host = AudioGetCurrentHostTime();
	
	return result; // return this, so if something bad happened in the converter we'll see it - render will stop!
}
