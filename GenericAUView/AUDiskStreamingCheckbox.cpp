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
/*
 *  AUDiskStreamingCheckbox.cpp
 *  CAServices
 *
 *  Created by Michael Hopkins on Tue Aug 26 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#include "AUDiskStreamingCheckbox.h"

#define kChangeDiskStreaming	'cdst'

// Function prototypes
void DiskStreamingListener(void * inRefCon, AudioUnit ci, AudioUnitPropertyID  inID, AudioUnitScope inScope, AudioUnitElement inElement);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AUDiskStreamingCheckbox::AUDiskStreamingCheckbox
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AUDiskStreamingCheckbox::AUDiskStreamingCheckbox (AUCarbonViewBase *inBase, 
				Point 					inLocation, 
				int 					inRightEdge, 
				ControlFontStyleRec & 	inFontStyle)
	: AUPropertyControl (inBase)
{
	Rect r;
	r.top = inLocation.v;		r.bottom = r.top + 16;
	r.left = inLocation.h;		r.right = r.left + inRightEdge;
	
	verify_noerr(CreateCheckBoxControl(mView->GetCarbonWindow(), 
										&r, 
										CFSTR("Disk Streaming"), 
										0, 
										true, 
										&mControl));
	verify_noerr (SetControlFontStyle (mControl, &inFontStyle));
	ControlSize smallSize = kControlSizeSmall;
	verify_noerr (SetControlData (mControl, kControlEntireControl, kControlSizeTag, sizeof (ControlSize), &smallSize));

	SInt16 baseLineOffset;
	verify_noerr (GetBestControlRect (mControl, &r, &baseLineOffset));
	r.bottom += baseLineOffset;
	SInt16 difference = (r.left + inRightEdge) - r.right;
	r.right += difference;
	r.left += difference;
	SetControlBounds (mControl, &r);
	EmbedControl(mControl);
	verify_noerr (AudioUnitAddPropertyListener(mView->GetEditAudioUnit(), kMusicDeviceProperty_StreamFromDisk, DiskStreamingListener, this));
	UInt32 value;
	UInt32 size = sizeof(value);
	verify_noerr (AudioUnitGetProperty (mView->GetEditAudioUnit(), kMusicDeviceProperty_StreamFromDisk, kAudioUnitScope_Global, 0, &value, &size));
	
	HandlePropertyChange (value);
	
	RegisterEvents();

}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AUDiskStreamingCheckbox::~AUDiskStreamingCheckbox
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AUDiskStreamingCheckbox::~AUDiskStreamingCheckbox() {
	AudioUnitRemovePropertyListener (mView->GetEditAudioUnit(), kMusicDeviceProperty_StreamFromDisk, DiskStreamingListener);

}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	kChangeDiskStreaming::HandlePropertyChange
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	AUDiskStreamingCheckbox::HandlePropertyChange(UInt32 streaming) 
{
	// only set the control value if it is different from the current value
	UInt32 value = GetControl32BitValue(mControl);
	if (streaming != value)
		SetControl32BitValue(mControl, streaming);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AURenderQualityPopup::HandleControlChange
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	AUDiskStreamingCheckbox::HandleControlChange ()
{

	UInt32 	streaming = GetControl32BitValue(mControl);

	AudioUnitSetProperty (mView->GetEditAudioUnit(), 
							kMusicDeviceProperty_StreamFromDisk,
							kAudioUnitScope_Global, 
							0, 
							&streaming, 
							sizeof(streaming));
	mView->Update (true);
}


void DiskStreamingListener(void * inRefCon, AudioUnit ci, AudioUnitPropertyID  inID, AudioUnitScope inScope, AudioUnitElement inElement) {
	AUDiskStreamingCheckbox *This = (AUDiskStreamingCheckbox*)inRefCon;
	
	UInt32 streaming;
	UInt32 theSize = sizeof (UInt32);
	
	ComponentResult result = AudioUnitGetProperty(ci, inID, inScope, inElement, &streaming, &theSize);
	
	if (result == noErr)
		This->HandlePropertyChange(streaming);
}
