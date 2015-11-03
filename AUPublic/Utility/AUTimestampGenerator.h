/*	Copyright: 	© Copyright 2003 Apple Computer, Inc. All rights reserved.

	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
			("Apple") in consideration of your agreement to the following terms, and your
			use, installation, modification or redistribution of this Apple software
			constitutes acceptance of these terms.  If you do not agree with these terms,
			please do not use, install, modify or redistribute this Apple software.

			In consideration of your agreement to abide by the following terms, and subject
			to these terms, Apple grants you a personal, non-exclusive license, under AppleÕs
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
/*=============================================================================
	AUTimestampGenerator.h
	
=============================================================================*/

#ifndef __AUTimestampGenerator_h__
#define __AUTimestampGenerator_h__

// This class generates a continuously increasing series of timestamps based
// on a series of potentially discontinuous timestamps (as can be delivered from
// CoreAudio in the event of an overload or major engine change).
class AUTimestampGenerator {
public:
	AUTimestampGenerator()
	{
		Reset();
	}
	
	// Call this to reset the timeline.
	void	Reset()
	{
		mCurrentInputTime.mSampleTime = 0.;
		mNextInputSampleTime = 0.;
		mCurrentOutputTime.mSampleTime = 0.;
		mFirstTime = true;
	}
	
	// Call this once per render cycle with the downstream timestamp.
	// expectedDeltaFrames is the expected difference between the current and previous 
	//	downstream timestamps.
	void	AddOutputTime(const AudioTimeStamp &inTimeStamp, UInt32 expectedDeltaFrames)
	{
		mLastOutputTime = mCurrentOutputTime;
		mCurrentOutputTime = inTimeStamp;
		if (mFirstTime) {
			mFirstTime = false;
			mDiscontinuous = false;
		} else
			mDiscontinuous = ((mCurrentOutputTime.mSampleTime - mLastOutputTime.mSampleTime) != 
										double(expectedDeltaFrames));
	}
	
	// Call this once per render cycle to obtain the upstream timestamp.
	// framesToAdvance is the number of frames the input timeline is to be
	//	advanced during this render cycle.
	const AudioTimeStamp &	GenerateInputTime(UInt32 framesToAdvance, double sampleRate)
	{
		double inputSampleTime;
		
		mCurrentInputTime.mFlags = kAudioTimeStampSampleTimeValid;
		if (mCurrentOutputTime.mFlags & kAudioTimeStampRateScalarValid) {
			mCurrentInputTime.mFlags |= kAudioTimeStampRateScalarValid;
			mCurrentInputTime.mRateScalar = mCurrentOutputTime.mRateScalar;
		}
		if (mCurrentOutputTime.mFlags & kAudioTimeStampHostTimeValid) {
			mCurrentInputTime.mFlags |= kAudioTimeStampHostTimeValid;
			mCurrentInputTime.mHostTime = mCurrentOutputTime.mHostTime;
			if (mDiscontinuous) {
				// we had a discontinuous output time, need to resync
				UInt64 deltaHostTime = mCurrentOutputTime.mHostTime - mLastOutputTime.mHostTime;
				UInt64 deltaNanos = AudioConvertHostTimeToNanos(deltaHostTime);
				double rateScalar = (mCurrentOutputTime.mFlags & kAudioTimeStampRateScalarValid) ? 
										mCurrentOutputTime.mRateScalar : 1.0;
				// samples/second * seconds = samples
				double deltaSamples = floor(sampleRate * rateScalar * double(deltaNanos) / 1.0e9 + 0.5);
				double lastInputSampleTime = mCurrentInputTime.mSampleTime;
				inputSampleTime = lastInputSampleTime + deltaSamples;
				//printf("adjusted input time: %.0f -> %.0f\n", lastInputSampleTime, inputSampleTime);
				mDiscontinuous = false;
			} else
				inputSampleTime = mNextInputSampleTime;
		} else
			inputSampleTime = mNextInputSampleTime;

		if (mCurrentOutputTime.mFlags & kAudioTimeStampWordClockTimeValid) {
			mCurrentInputTime.mFlags |= kAudioTimeStampWordClockTimeValid;
			mCurrentInputTime.mWordClockTime = mCurrentOutputTime.mWordClockTime;
		}
		if (mCurrentOutputTime.mFlags & kAudioTimeStampSMPTETimeValid) {
			mCurrentInputTime.mFlags |= kAudioTimeStampSMPTETimeValid;
			mCurrentInputTime.mSMPTETime = mCurrentOutputTime.mSMPTETime;
		}
		
		mCurrentInputTime.mSampleTime = inputSampleTime;
		mNextInputSampleTime = inputSampleTime + double(framesToAdvance);
		return mCurrentInputTime;
	}
	
	void					Advance(double framesToAdvance)
	{
		mNextInputSampleTime = mCurrentInputTime.mSampleTime + framesToAdvance;
	}
	
	
private:
	AudioTimeStamp		mCurrentInputTime;
	Float64				mNextInputSampleTime;

	AudioTimeStamp		mLastOutputTime;
	AudioTimeStamp		mCurrentOutputTime;

	bool				mFirstTime;
	bool				mDiscontinuous;
};


#endif // __AUTimestampGenerator_h__
