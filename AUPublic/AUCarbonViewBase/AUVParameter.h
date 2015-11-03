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
/*=============================================================================
	AUVParameter.h
	
=============================================================================*/

#ifndef __AUVParameter_h__
#define __AUVParameter_h__

#include <AudioToolbox/AudioUnitUtilities.h>

// ____________________________________________________________________________
//	AUVParameter
//	complete parameter specification
	/*! @class AUVParameter */
class AUVParameter : public AudioUnitParameter {
public:
	/*! @ctor AUVParameter.0 */
	AUVParameter();
	/*! @ctor AUVParameter.1 */
	AUVParameter(AudioUnit au, AudioUnitParameterID param, AudioUnitScope scope, AudioUnitElement element);
	/*! @ctor AUVParameter.2 */
	AUVParameter(const AUVParameter &a);
	/*! @dtor ~AUVParameter */
	~AUVParameter();
		
	/*! @method operator <@ */
	bool operator < (const AUVParameter &a) const { return memcmp(this, &a, sizeof(AudioUnitParameter)) < 0; }
	
	/*! @method operator =@ */
	AUVParameter &	operator = (const AUVParameter &a);
	
	/*! @method GetValue */
	Float32							GetValue();
	/*! @method SetValue */
	void							SetValue(	AUParameterListenerRef			inListener, 
												void *							inObject,
												Float32							inValue);
	
	/*! @method GetName */
	CFStringRef						GetName()		{ return mParamName; }	// borrowed reference!
	
	CFStringRef 					GetValueNameCopy();						// returns a copy of the name of the current parameter value or null if there is no name associated
																			// caller must release
																		

	CFStringRef 					GetValueNameCopy(Float32 value);		// returns a copy of the name of the specified parameter value or null if there is no name associated
																			// caller must release

	/*! @method ParamInfo */
	const AudioUnitParameterInfo &	ParamInfo()		{ return mParamInfo; }
		// this may return null! - in which case there is no descriptive tag for the parameter
	/*! @method GetParamTag */
	CFStringRef						GetParamTag()	{ return mParamTag; }
		// this can return null if there is no name for the parameter
	/*! @method GetParamName */
	CFStringRef						GetParamName (int inIndex) 
	{ 
		return (mNamedParams && inIndex < mNumIndexedParams) 
					? (CFStringRef) CFArrayGetValueAtIndex(mNamedParams, inIndex)
					: 0; 
	}
	
	/*! @method GetNumIndexedParams */
	int								GetNumIndexedParams () const { return mNumIndexedParams; }
	
	/*! @method IsIndexedParam */
	bool							IsIndexedParam () const { return mNumIndexedParams != 0; }
	
	/*! @method HasNamedParams */
	bool							HasNamedParams () const { return IsIndexedParam() && mNamedParams; }
	
	bool							GetClumpID (UInt32 &outClumpID) const 
									{ 
										if (mParamInfo.flags & kAudioUnitParameterFlag_HasClump) {
											outClumpID = mParamInfo.clumpID;
											return true;
										}
										return false;
									}
 
protected:	
	// cached parameter info
	/*! @var mParamInfo */
	AudioUnitParameterInfo		mParamInfo;
	/*! @var mParamName */
	CFStringRef					mParamName;
	/*! @var mParamTag */
	CFStringRef					mParamTag;
	/*! @var mNumIndexedParams */
	short						mNumIndexedParams;
	/*! @var mNamedParams */
	CFArrayRef					mNamedParams;
};



#endif // __AUVParameter_h__
