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
#import "SampleEffectCocoaUI.h"

enum {
	kParam_One,
	kParam_Two,
    kParam_Three_Indexed,
	kNumberOfParameters
};

AudioUnitParameter parameter1 = { 0, kParam_One, kAudioUnitScope_Global, 0 };
AudioUnitParameter parameter2 = { 0, kParam_Two, kAudioUnitScope_Global, 0 };
AudioUnitParameter parameter3 = { 0, kParam_Three_Indexed, kAudioUnitScope_Global, 0 };

@implementation SampleEffectCocoaUI

- (id)init {
	self = [super init];
	
	// Load the nib file
	if (! [NSBundle loadNibNamed: @"SampleEffectUI" owner: self]) {
		NSLog(@"Loading SampleEffectUI.nib failed");
	}
	
	return self;
}

// always return 0 for now
- (unsigned) interfaceVersion {
	return 0;
}

// a string description of the Cocoa UI
- (NSString *) description {
	return @"Sample Effect Cocoa UI";
}

// return the NSView that fits into the size specified by the host
// in this case, we have 2 different sizes to choose from in horizontal and vertical layouts
// we choose the appropriate view for the preferred size and return it
- (NSView *) uiViewForAudioUnit: (AudioUnit) au withSize : (NSSize) inPreferredSize {
	
	// remove previous listeners
	if (itsAU != nil)
		[self removeListeners];
	itsAU = au;

	// add new listeners
	[self addListeners];
	
	// set the initial control values based on the preset parameter values of the AudioUnit
	[self updateControlValues];
	return itsView;
}

- (void) addListeners {
	AUListenerCreate(ParameterListener, self, 
		CFRunLoopGetCurrent(), kCFRunLoopDefaultMode, 0.100, // 100 ms
		&mParameterListener);
	
	parameter1.mAudioUnit = itsAU;
	parameter2.mAudioUnit = itsAU;
	parameter3.mAudioUnit = itsAU;

	AUListenerAddParameter (mParameterListener, NULL, &parameter1);
	AUListenerAddParameter (mParameterListener, NULL, &parameter2);
	AUListenerAddParameter (mParameterListener, NULL, &parameter3);
}

- (void) removeListeners {
	AUListenerRemoveParameter (mParameterListener, NULL, &parameter1);
	AUListenerRemoveParameter (mParameterListener, NULL, &parameter2);
	AUListenerRemoveParameter (mParameterListener, NULL, &parameter3);

	AUListenerDispose(mParameterListener);
}

- (void) updateControlValues {
	// get values for the parameters
	Float32 value;
	ComponentResult result = AudioUnitGetParameter( itsAU, kParam_One, kAudioUnitScope_Global, 0, &value);
	if (result != noErr) return;
	[self setParam1Value: value];
	
	result = AudioUnitGetParameter( itsAU, kParam_Two, kAudioUnitScope_Global, 0, &value);
	if (result != noErr) return;
	[self setParam2Value: value];
	
	result = AudioUnitGetParameter( itsAU, kParam_Three_Indexed, kAudioUnitScope_Global, 0, &value);	
	if (result != noErr) return;

	[self setParam3Value: value];
}

- (void) setParam1Value: (Float32) value {
	[param1Slider setFloatValue: value];
	[param1TextField setStringValue: [[NSNumber numberWithFloat: value] stringValue]];
}

- (void) setParam2Value: (Float32) value {
	[param2Slider setFloatValue: value];
	[param2TextField setStringValue: [[NSNumber numberWithFloat: value] stringValue]];
}

- (void) setParam3Value: (Float32) value {
	[indexedParamMatrix setState: 1 atRow: value - 4 column: 0];
}

- (IBAction)indexedParamChanged:(id)sender {
	[indexedParamMatrix selectedRow];
	OSStatus result = AUParameterSet( mParameterListener, sender, &parameter1, [indexedParamMatrix selectedRow] + 4, 0);
	if (result != noErr) return;
}

- (IBAction)param1Changed:(id)sender {
	OSStatus result = AUParameterSet( mParameterListener, sender, &parameter1, [sender floatValue], 0);
	if (result != noErr) return;
}

- (IBAction)param2Changed:(id)sender {
	OSStatus result = AUParameterSet( mParameterListener, sender, &parameter2, [sender floatValue], 0);
	if (result != noErr) return;

}

- (IBAction)presetChanged:(id)sender {
	int row = [indexedParamMatrix selectedRow];
	OSStatus result = AUParameterSet( mParameterListener, sender, &parameter3, row, 0);
	if (result != noErr) return;
}

@end

void ParameterListener(	void * inRefCon, void * inObject, const AudioUnitParameter *inParameter, Float32 inValue) {
	SampleEffectCocoaUI * theUI = (SampleEffectCocoaUI *) inRefCon;
		
	switch (inParameter->mParameterID) {
		case kParam_One:
			[theUI setParam1Value: inValue];
			break;
		case kParam_Two:
			[theUI setParam2Value: inValue];
			break;
		case kParam_Three_Indexed:
			[theUI setParam3Value: inValue];
			break;
	} 
}
