/*	Copyright: 	� Copyright 2003 Apple Computer, Inc. All rights reserved.

	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
			("Apple") in consideration of your agreement to the following terms, and your
			use, installation, modification or redistribution of this Apple software
			constitutes acceptance of these terms.  If you do not agree with these terms,
			please do not use, install, modify or redistribute this Apple software.

			In consideration of your agreement to abide by the following terms, and subject
			to these terms, Apple grants you a personal, non-exclusive license, under Apple�s
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
/*
	An effect unit will work on the canonical format - ie. PCM Float32
	Its input and output formats are equivalent - it does NO transformation of
	the format of its input to its output.
	
	It assumes that there will only ever be one input bus and one output bus.
*/

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	ReverseOfflineUnit.cpp
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#include "AUBase.h"
#include "ReverseOfflineUnitVersion.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ____ReverseOfflineUnit

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// THIS UNIT HAS NO PARAMETERS...
// 

class ReverseOfflineUnit : public AUBase
{
public:
								ReverseOfflineUnit(AudioUnit component);
	
	virtual ComponentResult		GetPropertyInfo(	AudioUnitPropertyID		inID,
													AudioUnitScope			inScope,
													AudioUnitElement		inElement,
													UInt32 &				outDataSize,
													Boolean	&				outWritable );

	virtual ComponentResult		GetProperty(		AudioUnitPropertyID 	inID,
													AudioUnitScope 			inScope,
													AudioUnitElement 		inElement,
													void 					* outData );

	virtual ComponentResult		SetProperty(		AudioUnitPropertyID 	inID,
													AudioUnitScope 			inScope,
													AudioUnitElement 		inElement,
													const void *			inData,
													UInt32 					inDataSize);

		// same logic as AUEffectBase
	virtual ComponentResult 	Initialize();
														
	virtual bool				StreamFormatWritable(	AudioUnitScope		scope,
														AudioUnitElement	element);

	virtual ComponentResult 	Render(	AudioUnitRenderActionFlags 			& ioActionFlags,
											const AudioTimeStamp 			& inTimeStamp,
											UInt32							nFrames);

		// in this case we don't have a preflight string
		// outStr can be NULL
	virtual bool				GetPreflightString(CFStringRef *outStr) const { return false; }

	virtual void				InputChanged () { mNeedToPreflight = true; }

private:
	UInt64			mNumInputSamples;
	bool			mNeedToPreflight;
		
		//use by some units to display the start offset as information to the user...
	UInt64			mStartOffset;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

COMPONENT_ENTRY(ReverseOfflineUnit)

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	ReverseOfflineUnit::ReverseOfflineUnit
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ReverseOfflineUnit::ReverseOfflineUnit(AudioUnit component)
	: AUBase(component, 1, 1), 
	  mNumInputSamples(0),
	  mNeedToPreflight(true),
	  mStartOffset (0)
{
}


#pragma mark ____ReverseOfflineProperties
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	ReverseOfflineUnit::GetPropertyInfo
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ComponentResult		ReverseOfflineUnit::GetPropertyInfo (AudioUnitPropertyID	inID,
												AudioUnitScope					inScope,
												AudioUnitElement				inElement,
												UInt32 &						outDataSize,
												Boolean &						outWritable)
{
	if (inScope == kAudioUnitScope_Global) {
		switch (inID) {
			case kAudioOfflineUnitProperty_InputSize:
				outDataSize = sizeof(mNumInputSamples);
				outWritable = true;
				return noErr;
			case kAudioOfflineUnitProperty_OutputSize:
				outDataSize = sizeof(mNumInputSamples);
				outWritable = false;
				return noErr;
			case kAudioUnitOfflineProperty_StartOffset:
				outDataSize = sizeof(mStartOffset);
				outWritable = false;
				return noErr;
			case kAudioUnitOfflineProperty_PreflightRequirements:
				outDataSize = sizeof (UInt32);
				outWritable = false;
				return noErr;
			case kAudioUnitOfflineProperty_PreflightName:
				if (GetPreflightString (NULL)) {
					outDataSize = sizeof (CFStringRef);
					outWritable = false;
					return noErr;
				}
				return kAudioUnitErr_InvalidProperty;
		}
	}
	return AUBase::GetPropertyInfo (inID, inScope, inElement, outDataSize, outWritable);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	ReverseOfflineUnit::GetProperty
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ComponentResult		ReverseOfflineUnit::GetProperty (AudioUnitPropertyID 		inID,
												AudioUnitScope 					inScope,
												AudioUnitElement			 	inElement,
												void *							outData)
{
	if (inScope == kAudioUnitScope_Global) {
		switch (inID) {
			// for this AU output is the same size as input...
			case kAudioOfflineUnitProperty_OutputSize:
			case kAudioOfflineUnitProperty_InputSize:
				*(UInt64*)outData = mNumInputSamples;
				return noErr;
			case kAudioUnitOfflineProperty_StartOffset:
				*(UInt64*)outData = mStartOffset;
				return noErr;
			// for this AU no preflighting is required...
			case kAudioUnitOfflineProperty_PreflightRequirements:
				*(UInt32*)outData = kOfflinePreflight_NotRequired;
				return noErr;

			case kAudioUnitOfflineProperty_PreflightName:
				if (GetPreflightString (NULL)) {
					GetPreflightString ((CFStringRef*)outData);
					return noErr;
				}
				return kAudioUnitErr_InvalidProperty;
		}
	}
	return AUBase::GetProperty (inID, inScope, inElement, outData);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	ReverseOfflineUnit::GetProperty
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ComponentResult		ReverseOfflineUnit::SetProperty(AudioUnitPropertyID 	inID,
													AudioUnitScope 			inScope,
													AudioUnitElement 		inElement,
													const void *			inData,
													UInt32 					inDataSize)
{
	if (inScope == kAudioUnitScope_Global) {
		switch (inID) {
				//whenever these properties are set we *MAY* take this to mean the input has changed
				// at this point we require preflighting again...
			case kAudioOfflineUnitProperty_InputSize:
				if (inDataSize < sizeof(UInt64)) return kAudioUnitErr_InvalidPropertyValue;
				
				mNumInputSamples = *(UInt64*)inData;
				InputChanged();
				return noErr;

			case kAudioUnitOfflineProperty_StartOffset:
				if (inDataSize < sizeof(UInt32)) return kAudioUnitErr_InvalidPropertyValue;
					
				mStartOffset = *(UInt32*)inData;
				InputChanged();
				return noErr;
		}
	}
	return AUBase::SetProperty (inID, inScope, inElement, inData, inDataSize);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ____Initialization_Formats

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	ReverseOfflineUnit::Initialize
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ComponentResult 	ReverseOfflineUnit::Initialize()
{
    const AUChannelInfo *auChannelConfigs = NULL;
    UInt32 numIOconfigs = SupportedNumChannels(&auChannelConfigs);
		// does the unit publish specific information about channel configurations?
    if ((numIOconfigs > 0) && (auChannelConfigs != NULL))
    {
        SInt16 auNumInputs = (SInt16) GetStreamFormat(kAudioUnitScope_Input, 0).mChannelsPerFrame;
        SInt16 auNumOutputs = (SInt16) GetStreamFormat(kAudioUnitScope_Output, 0).mChannelsPerFrame;
        bool foundMatch = false;
        for (UInt32 i = 0; (i < numIOconfigs) && !foundMatch; ++i)
        {
            SInt16 configNumInputs = auChannelConfigs[i].inChannels;
            SInt16 configNumOutputs = auChannelConfigs[i].outChannels;
            if ((configNumInputs < 0) && (configNumOutputs < 0))
            {
					// unit accepts any number of channels on input and output
                if (((configNumInputs == -1) && (configNumOutputs == -2)) || ((configNumInputs == -2) && (configNumOutputs == -1)))
                    foundMatch = true;
					// unit accepts any number of channels on input and output IFF they are the same number on both scopes
                else if (((configNumInputs == -1) && (configNumOutputs == -1)) && (auNumInputs == auNumOutputs))
                    foundMatch = true;
					// unit has specified a particular number of channels on both scopes
                else
                    continue;
            }
            else
            {
					// the -1 case on either scope is saying that the unit doesn't care about the 
					// number of channels on that scope
                bool inputMatch = (auNumInputs == configNumInputs) || (configNumInputs == -1);
                bool outputMatch = (auNumOutputs == configNumOutputs) || (configNumOutputs == -1);
                if (inputMatch && outputMatch)
                    foundMatch = true;
            }
        }
        if (!foundMatch)
            return kAudioUnitErr_FormatNotSupported;
    }
    else
    {
			// there is no specifically published channel info
			// so for those kinds of effects, the assumption is that the channels (whatever their number)
			// should match on both scopes
		UInt32 outputChannels = GetOutput(0)->GetStreamFormat().mChannelsPerFrame;
		UInt32 inputChannels = GetInput(0)->GetStreamFormat().mChannelsPerFrame;
        if ((outputChannels != inputChannels) || (outputChannels == 0))
            return kAudioUnitErr_FormatNotSupported;
    }

    return noErr;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	ReverseOfflineUnit::StreamFormatWritable
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool		ReverseOfflineUnit::StreamFormatWritable(	AudioUnitScope					scope,
														AudioUnitElement				element)
{
	return IsInitialized() ? false : true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ____Render

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	ReverseOfflineUnit::StreamFormatWritable
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ComponentResult 	ReverseOfflineUnit::Render(	AudioUnitRenderActionFlags &ioActionFlags,
											const AudioTimeStamp &			inTimeStamp,
											UInt32							nFrames)
{
	if (!HasInput(0))
		return kAudioUnitErr_NoConnection;

	// first we have to make sure that the rendering flag matches our internal state...
	bool preflight = (ioActionFlags & kAudioOfflineUnitRenderAction_Preflight);
	bool doRender = (ioActionFlags & kAudioOfflineUnitRenderAction_Render);
		
		// one of these two flags have to be provided
	if (!preflight && !doRender)
		return kAudioUnitErr_InvalidOfflineRender;

		// is the right state being given to us...
	if (mNeedToPreflight) {
		if (!preflight || doRender)
			return kAudioUnitErr_InvalidOfflineRender;
	} else {
		if (preflight || !doRender)
			return kAudioUnitErr_InvalidOfflineRender;
	}
	
	// so in this PARTICULAR UNIT we don't need to do any preFlighting of the AU
	// because all it simply does is reverse the input samples...
	// but we'll let all this go through just to show the basic methodology of handling
	// the two states...
	
	// OK - now we have to figure out which input we want based on the output time
	// we're asked for
	
	// in this case, as we're a reverse unit this is a simple calculation...
	// we need a new time stamp based on the one we were given.
	
	AudioTimeStamp ts (inTimeStamp);
	ts.mSampleTime = mNumInputSamples - nFrames - inTimeStamp.mSampleTime;

	UInt32 numFramesToPull = nFrames;
	bool renderPhaseComplete = false;
	
	if (ts.mSampleTime < 0) {

		// one word of caution.. if we're preflighting we need to change that state to be ready to render
		// as to get here we're basically done with our sample processing
		
			// do we have a partial buffer to fill
		ts.mSampleTime += nFrames;
		if (ts.mSampleTime > 0) {
			numFramesToPull = (UInt32)ts.mSampleTime;
			ts.mSampleTime = 0;
			renderPhaseComplete = true;
		} else {
				// this is just a protection if someone pulls us for data way past what we have...
			mNeedToPreflight = false;
			ioActionFlags |= kAudioOfflineUnitRenderAction_Complete;
		}
	}

	// OK - now we are good to go...
		
	AUOutputElement *theOutput = GetOutput(0);	// throws if error
	AUInputElement *theInput = GetInput(0);
	
	
	ComponentResult result = theInput->PullInput (ioActionFlags, ts, 0 /* element */, numFramesToPull);
	
	if (result)
		return result;

		// for reverse we don't have any preflight analysis of the input data
		// so at this point we just return..
		
		// however - need to check if the preflighting is complete
		// if so we turn preflighting off...
	if (mNeedToPreflight) {
		if (renderPhaseComplete) {
			mNeedToPreflight = false;
			ioActionFlags |= kAudioOfflineUnitRenderAction_Complete;
		}
		return noErr;
	}

	// ok - now we reverse our input data
	// if we have a remainder we need to zero out the output buffer
	
	AudioBufferList &inputBuffer = theInput->GetBufferList();
	AudioBufferList &outputBuffer = theOutput->GetBufferList();
	
	// we'll do the reverse one channel at a time...
	for (UInt32 i = 0; i < inputBuffer.mNumberBuffers; ++i) 
	{
		Float32* inSampleData = (Float32*)inputBuffer.mBuffers[i].mData;
		Float32* outSampleData = (Float32*)outputBuffer.mBuffers[i].mData;
		
		
		for (SInt32 in = numFramesToPull, out = 0; --in >= 0 ;++out)
			outSampleData[out] = inSampleData[in];
	}

	if (renderPhaseComplete) {
		UInt32 numValidBytes = numFramesToPull * sizeof (Float32);
			// we just need to reset the numbytes field as that indicates the valid portion of the buffer
		for (UInt32 i = 0; i < outputBuffer.mNumberBuffers; ++i) {
			outputBuffer.mBuffers[i].mDataByteSize = numValidBytes;
		}

		ioActionFlags |= kAudioOfflineUnitRenderAction_Complete;
	}

	return noErr;
}
