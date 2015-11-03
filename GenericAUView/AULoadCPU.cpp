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
 *  AULoadCPU.cpp
 *  CAServices
 *
 *  Created by Michael Hopkins on Thu Oct 24 2002.
 *  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 */

#include "AULoadCPU.h"

#define kToggleRestrictCPULoadCmd 	'rcpu'
#define kChangeCPULoadCmd		 	'ccpu'
#define kIncrementCPULoadCmd		'+cpu'
#define kDecrementCPULoadCmd		'-cpu'

#define kLabelAndSliderSpacing		4
#define kSliderThinDimension 		16

/*** FUNCTION PROTOTYPES ***/
void SetControlStringAndValue(ControlRef control, int value);
void CPULoadListener(void * inRefCon, AudioUnit ci, AudioUnitPropertyID  inID, AudioUnitScope inScope, AudioUnitElement inElement);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AULoadCPU::AULoadCPU
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AULoadCPU::AULoadCPU (AUCarbonViewBase *inBase, 
				Point 					inLocation, 
				int 					inRightEdge, 
				ControlFontStyleRec & 	inFontStyle)
	: AUPropertyControl (inBase)
{
	Rect 				r;
	OSErr				err;
	ControlFontStyleRec fontStyle = inFontStyle;
	fontStyle.just = teFlushRight;

	r.top = inLocation.v;		r.bottom = r.top + 16;
	r.left = inLocation.h;		r.right = r.left + inRightEdge;
	
	CFBundleRef bundle = CFBundleGetBundleWithIdentifier( CFSTR("com.apple.audio.units.Components") );
	CFRetain (bundle);
	
	CFMutableStringRef cpuLoadTitle = CFStringCreateMutable(NULL, 0);
	CFStringAppend(cpuLoadTitle, CFCopyLocalizedStringFromTableInBundle(
		CFSTR("Restrict CPU Load"), CFSTR("CustomUI"), bundle,
		CFSTR("The cpu load restriction button title")));
	CFStringAppend(cpuLoadTitle, CFSTR(":"));

	
	err = CreateCheckBoxControl(mView->GetCarbonWindow(), 
									&r, 
									cpuLoadTitle, 
									0, 
									true, 
									&mNoCPURestrictionsBtn);
	CFRelease(cpuLoadTitle);
	if (err != noErr)
		return;
		
	ControlSize smallSize = kControlSizeSmall;
	verify_noerr (SetControlData (mNoCPURestrictionsBtn, kControlEntireControl, kControlSizeTag, sizeof (ControlSize), &smallSize));
	verify_noerr (SetControlFontStyle (mNoCPURestrictionsBtn, &fontStyle));
	SInt16 baseLineOffset = 0;
	verify_noerr (GetBestControlRect (mNoCPURestrictionsBtn, &r, &baseLineOffset));
	r.bottom += baseLineOffset;
	SInt16 difference = (r.left + inRightEdge) - r.right;
	r.right += difference;
	r.left += difference;
	SetControlBounds (mNoCPURestrictionsBtn, &r);
	SetControlCommandID (mNoCPURestrictionsBtn, kToggleRestrictCPULoadCmd);
	SetControl32BitValue (mNoCPURestrictionsBtn, 1);

	EmbedControl(mNoCPURestrictionsBtn);
	
	r.left 	= r.right + 8; 
	r.right = r.left + 8;
	r.top -= 2;
	r.bottom= r.top + 22;

	// get the current value of the property
	Float32 theCPULoad;
	UInt32 size = sizeof(Float32);
	ComponentResult result = AudioUnitGetProperty(mView->GetEditAudioUnit(), kAudioUnitProperty_CPULoad, kAudioUnitScope_Global, 0, &theCPULoad, &size);
	if (result == noErr) {
		char text[64];
		
		sprintf(text, "%.0f", theCPULoad);
		
		if (CreateLittleArrowsControl (mView->GetCarbonWindow(), &r, (long)theCPULoad * 100, 0, 100, 5, &mLittleArrowsBtn) == noErr) {
			r.left 	= r.right + 12;
			r.right = r.left + 36;
			r.top  += 2;
			r.bottom = r.top + 17;
			
			CFStringRef	cfstr = CFStringCreateWithCString(NULL, text, kCFStringEncodingMacRoman);
			if (CreateEditUnicodeTextControl (mView->GetCarbonWindow(), &r, cfstr, false, &fontStyle, &mControl) == noErr) {
				SetControl32BitMaximum(mControl, 100);
				SetControl32BitMinimum(mControl, 0);
				mUpdating = false;
				mLoadValue = 60;
				HandlePropertyChange(theCPULoad*100);
				ControlKeyFilterUPP proc = AULoadCPU::NumericKeyFilterCallback;
				verify_noerr(SetControlData(mControl, 0, kControlEditTextKeyFilterTag, sizeof(proc), &proc));		
				EmbedControl(mControl);
			}
			CFRelease (cfstr);
			EmbedControl(mLittleArrowsBtn);
			
			CFStringRef percentStr = CFCopyLocalizedStringFromTableInBundle(
				CFSTR("%"), CFSTR("CustomUI"), bundle,
				CFSTR("Percent symbol"));
		
			ControlRef ref;
			r.left = r.right + 8;
			r.right = r.left + 14;
			r.top = inLocation.v;
			r.bottom = r.top + 16;
			
			OSErr theErr =  CreateStaticTextControl (GetCarbonWindow(), &r, percentStr, &inFontStyle, &ref);
			if (theErr == noErr)
				EmbedControl(ref);
			CFRelease(percentStr);
			CFRelease(bundle);
		}
		err = AudioUnitAddPropertyListener(mView->GetEditAudioUnit(), kAudioUnitProperty_CPULoad, CPULoadListener, this);
	}
	RegisterEvents();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AULoadCPU::~AULoadCPU
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AULoadCPU::~AULoadCPU() {
	AudioUnitRemovePropertyListener (mView->GetEditAudioUnit(), kAudioUnitProperty_CPULoad, CPULoadListener);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AULoadCPU::NumericKeyFilterCallback
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ControlKeyFilterResult	AULoadCPU::NumericKeyFilterCallback(ControlRef theControl, 
												SInt16 *keyCode, SInt16 *charCode, 
												EventModifiers *modifiers)
{
	SInt16 c = *charCode;
	OSErr  err;
	if (isdigit(c) || c == '+' || c == '-' || c == '.' || c == '\b' || c == 0x7F || (c >= 0x1c && c <= 0x1f)
	|| c == '\t')
		return kControlKeyFilterPassKey;
	if (c == '\r' || c == 3) {	// return or Enter
		CFStringRef cfstr;
		err = GetControlData(theControl, 0, kControlEditTextCFStringTag, sizeof(CFStringRef), &cfstr, NULL);
		if (err != noErr) {
			CFRelease (cfstr);
			return kControlKeyFilterBlockKey;
		}
		int paramValue = 0;
		char valstr[32];
		CFStringGetCString(cfstr, valstr, sizeof(valstr), kCFStringEncodingASCII);
		sscanf(valstr, "%d", &paramValue);	
		SetControl32BitValue(theControl, paramValue);
		CFRelease (cfstr);
	}
	return kControlKeyFilterBlockKey;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AULoadCPU::HandleEvent
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool	AULoadCPU::HandleEvent(EventRef event)
{	
	UInt32 		eventClass = GetEventClass(event);		// the class of the event
	bool 		result = false;
		
	switch(eventClass) {
		case kEventClassMouse:
			result = HandleMouseEvent(event);
			break;
		case kEventClassCommand:
			result = HandleCommandEvent(event);
			break;
		case kEventClassControl:
			result = HandleControlEvent(event);
			break;
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AULoadCPU::RegisterEvents
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	AULoadCPU::RegisterEvents ()
{
	EventTypeSpec events[] = {
		{ kEventClassMouse, kEventMouseDown},	
		{ kEventClassMouse, kEventMouseUp},	
		{ kEventClassCommand, kEventCommandProcess}
	};
	
	WantEventTypes(GetWindowEventTarget(mView->GetCarbonWindow()), GetEventTypeCount(events), events);

	EventTypeSpec controlEvents[] = {
		{ kEventClassControl, kEventControlValueFieldChanged }	// N.B. OS X only
	};
	
	WantEventTypes(GetControlEventTarget(mControl), GetEventTypeCount(controlEvents), controlEvents);	
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AULoadCPU::HandlePropertyChange
//
//	Responds to a property changed message from the registered listener and updates the UI
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	AULoadCPU::HandlePropertyChange(Float32 load) {
	if (load == 0 || load == 100) {
		if (!mUpdating)
			SetControlStringAndValue((int)load);				
		DeactivateControl (mControl);
		DeactivateControl (mLittleArrowsBtn);
		SetControl32BitValue (mNoCPURestrictionsBtn, 0);
		
	} else {
		if (!mUpdating)
			SetControlStringAndValue ((int)load);
		ActivateControl (mControl);
		ActivateControl (mLittleArrowsBtn);
		SetControl32BitValue (mNoCPURestrictionsBtn, 1);
	}

	mUpdating = false;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AULoadCPU::HandleControlChange
//
//	Sets the property with the appropriate value based on the UI
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	AULoadCPU::HandleControlChange ()
{
	Float32 val 	= GetControl32BitValue(mControl) / 100.0;
	
	AudioUnitSetProperty (mView->GetEditAudioUnit(), 
							kAudioUnitProperty_CPULoad,
							kAudioUnitScope_Global, 
							0, 
							&val, 
							sizeof(val));
//	mView->Update (true);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AULoadCPU::HandleMouseEvent
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AULoadCPU::HandleMouseEvent(EventRef inEvent) {
	UInt32 		eventKind  = GetEventKind(inEvent);			// the kind of the event
	CGrafPtr 	gp = GetWindowPort(mView->GetCarbonWindow()), save;
	Point 		pt;
	HIPoint		mousePos;
	bool 		status = false;
	
	GetPort(&save);
	SetPort (gp);	

	GetEventParameter (	inEvent, kEventParamMouseLocation, 
						typeHIPoint, NULL, sizeof (mousePos), NULL, &mousePos);
	
	pt.h = short(mousePos.x); pt.v = short(mousePos.y);
	GlobalToLocal(&pt);
	
	if (eventKind == kEventMouseDown) {
		ControlPartCode part;
		ControlRef theControl = FindControlUnderMouse (pt, GetCarbonWindow(), &part);
		
		if (theControl == mLittleArrowsBtn) {
			if (part == kControlUpButtonPart)
				SetControlCommandID (mLittleArrowsBtn, kIncrementCPULoadCmd);
			else
				SetControlCommandID (mLittleArrowsBtn, kDecrementCPULoadCmd);			
		}
	}
	SetPort (save); 
	return status;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AULoadCPU::HandleControlEvent
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AULoadCPU::HandleControlEvent(EventRef inEvent) {
	UInt32 		eventKind  = GetEventKind(inEvent);			// the kind of the event
	ControlRef 	ref;
	
	GetEventParameter (inEvent, kEventParamDirectObject, typeControlRef, NULL, sizeof(ControlRef), NULL, &ref);

	switch (eventKind) {
		case kEventControlValueFieldChanged:
			if (ref == mControl) {
				mUpdating = true;
				HandleControlChange();		// !!MSH- make sure this is called even when the arrows are pressed
				return true;	// handled
			} else
				return false;
	}
	return false;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AULoadCPU::HandleCommandEvent
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AULoadCPU::HandleCommandEvent(EventRef inEvent) {
	HICommand	command;
	GetEventParameter (inEvent, kEventParamDirectObject, typeHICommand, NULL, sizeof(HICommand), NULL, &command);
	bool 		status = false;
	SInt32		value;
	ControlRef	ref;
	
	GetEventParameter (inEvent, kEventParamDirectObject, typeControlRef, NULL, sizeof(ControlRef), NULL, &ref);
	value = GetControl32BitValue (mControl);
			
	switch (command.commandID) {
		case kIncrementCPULoadCmd:
			value += 5;
			if (value > 100)
				value = 100;
			
			SetControlStringAndValue(value);
			status =  true;
			break;
		case kDecrementCPULoadCmd:
			value -= 5;
			if (value < 0)
				value = 0;
			SetControlStringAndValue(value);			
			status =  true;
			break;
		case kToggleRestrictCPULoadCmd:
			value = GetControl32BitValue (mNoCPURestrictionsBtn);
			if (value == 1) {
				// return the cpu load to previous value
				SetControlStringAndValue(mLoadValue);				
				ActivateControl(mControl);
				ActivateControl(mLittleArrowsBtn);
			}
			else {
				// set the cpu load to 0
				mLoadValue = GetControl32BitValue(mControl);
				SetControlStringAndValue(0);				
				DeactivateControl (mControl);
				DeactivateControl (mLittleArrowsBtn);
			}
			status =  true;
			break;
	}
	return status;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AULoadCPU::SetControlStringAndValue
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AULoadCPU::SetControlStringAndValue(int value) {
	::SetControlStringAndValue(mControl, value);
}

void SetControlStringAndValue(ControlRef control, int value) {
	char	str [5];
	CFStringRef cfstr;

	sprintf(str, "%d", value);
	cfstr = CFStringCreateWithCString(NULL, str, kCFStringEncodingMacRoman);
	SetControl32BitValue (control, value);
	verify_noerr(SetControlData(control, 0, kControlEditTextCFStringTag, sizeof(CFStringRef), &cfstr));
	HIViewSetNeedsDisplay(control, true);
	CFRelease(cfstr);
}

void CPULoadListener(void * inRefCon, AudioUnit ci, AudioUnitPropertyID  inID, AudioUnitScope inScope, AudioUnitElement inElement) {
	AULoadCPU *This = (AULoadCPU*)inRefCon;
	
	Float32 theLoad;
	UInt32 theSize = sizeof (Float32);
	
	ComponentResult result = AudioUnitGetProperty(ci, inID, inScope, inElement, &theLoad, &theSize);
	
	if (result == noErr && theSize > 0)
		This->HandlePropertyChange(theLoad*100);
}

