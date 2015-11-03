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
 *  AURenderQualityPopup.cpp
 *  CAServices
 *
 *  Created by Michael Hopkins on Fri Oct 25 2002.
 *  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 */

#include "AURenderQualityPopup.h"


#define kChangeRenderQualityCmd	'cqty'

#define kRenderTitleWidth 80
#define kQualityMenuID 21022

// Function prototypes
void RenderQualityListener(void * inRefCon, AudioUnit ci, AudioUnitPropertyID  inID, AudioUnitScope inScope, AudioUnitElement inElement);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AURenderQualityPopup::AURenderQualityPopup
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AURenderQualityPopup::AURenderQualityPopup (AUCarbonViewBase *inBase, 
				Point 					inLocation, 
				int 					inRightEdge, 
				ControlFontStyleRec & 	inFontStyle)
	: AUPropertyControl (inBase)
{
	Rect r;
	r.top = inLocation.v;		r.bottom = r.top + 17;
	r.left = inLocation.h;		r.right = r.left + inRightEdge;
	
	ControlFontStyleRec fontStyle = inFontStyle;
	inFontStyle.just = teFlushRight;

	ControlRef			ref;
		
	CFBundleRef mainBundle = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.audio.units.Components"));
	if (mainBundle != NULL) {
		CFMutableStringRef renderTitle = CFStringCreateMutable(NULL, 0);
		CFStringAppend(renderTitle, CFCopyLocalizedStringFromTableInBundle(
			CFSTR("Render Quality"), CFSTR("CustomUI"), mainBundle,
			CFSTR("The Render Quality Popup menu title")));
		CFStringAppend(renderTitle, CFSTR(":"));
	
		OSErr theErr =  CreateStaticTextControl (GetCarbonWindow(), &r, renderTitle, &inFontStyle, &ref);
		if (theErr == noErr)
			EmbedControl(ref);
		
		r.left = r.right + 8;
		r.right = r.left + 100;

		short bundleRef = CFBundleOpenBundleResourceMap (mainBundle);
		
		theErr = CreatePopupButtonControl (mView->GetCarbonWindow(), &r, NULL,
			/*kQualityMenuID*/ -12345, false, -1, 0, 0, &mControl);	
			
		MenuRef menuRef;
		verify_noerr(CreateNewMenu(kQualityMenuID, 0, &menuRef));
		
		verify_noerr(AppendMenuItemTextWithCFString (menuRef, CFCopyLocalizedStringFromTableInBundle(CFSTR("Maximum"), CFSTR("CustomUI"), mainBundle, CFSTR("")), 0, kChangeRenderQualityCmd, 0));
		verify_noerr(AppendMenuItemTextWithCFString (menuRef, CFCopyLocalizedStringFromTableInBundle(CFSTR("High"), CFSTR("CustomUI"), mainBundle, CFSTR("")), 0, kChangeRenderQualityCmd, 0));
		verify_noerr(AppendMenuItemTextWithCFString (menuRef, CFCopyLocalizedStringFromTableInBundle(CFSTR("Medium"), CFSTR("CustomUI"), mainBundle, CFSTR("")), 0, kChangeRenderQualityCmd, 0));
		verify_noerr(AppendMenuItemTextWithCFString (menuRef, CFCopyLocalizedStringFromTableInBundle(CFSTR("Low"), CFSTR("CustomUI"), mainBundle, CFSTR("")), 0, kChangeRenderQualityCmd, 0));
		verify_noerr(AppendMenuItemTextWithCFString (menuRef, CFCopyLocalizedStringFromTableInBundle(CFSTR("Minimum"), CFSTR("CustomUI"), mainBundle, CFSTR("")), 0, kChangeRenderQualityCmd, 0));
																
		verify_noerr(SetControlData(mControl, 0, kControlPopupButtonMenuRefTag, sizeof(menuRef), &menuRef));
					
		verify_noerr(SetControlFontStyle (mControl, &inFontStyle));
			
		SetControl32BitMaximum(mControl, 5);
		UInt32 renderQuality;
		UInt32 size = sizeof(UInt32);
		AudioUnitGetProperty (mView->GetEditAudioUnit(), 
							kAudioUnitProperty_RenderQuality,
							kAudioUnitScope_Global, 
							0, 
							&renderQuality, 
							&size);
							
		HandlePropertyChange(renderQuality);

		EmbedControl(mControl);
		theErr = AudioUnitAddPropertyListener(mView->GetEditAudioUnit(), kAudioUnitProperty_RenderQuality, RenderQualityListener, this);
		CFBundleCloseBundleResourceMap (mainBundle, bundleRef);
		CFRelease(renderTitle);
	}
	RegisterEvents();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AURenderQualityPopup::~AURenderQualityPopup
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AURenderQualityPopup::~AURenderQualityPopup() {
	AudioUnitRemovePropertyListener (mView->GetEditAudioUnit(), kAudioUnitProperty_RenderQuality, RenderQualityListener);

}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AURenderQualityPopup::HandleEvent
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool	AURenderQualityPopup::HandleEvent(EventRef event)
{	
	UInt32 		eventClass = GetEventClass(event);		// the class of the event
	HICommand	command;
	bool 		result = false;
	
	GetEventParameter (event, kEventParamDirectObject, typeHICommand, NULL, sizeof(HICommand), NULL, &command);
		
	switch(eventClass) {
		case kEventClassCommand:
			if (command.commandID == kChangeRenderQualityCmd)
				SetControl32BitValue(mControl, command.menu.menuItemIndex);
				HandleControlChange();
			break;
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AURenderQualityPopup::HandlePropertyChange
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	AURenderQualityPopup::HandlePropertyChange(UInt32 quality) 
{
	int item;
	switch (quality) {
		case kRenderQuality_Max:
			item = 1;
			break;
		case kRenderQuality_High:
			item = 2;
			break;
		case kRenderQuality_Medium:
			item = 3;
			break;
		case kRenderQuality_Low:
			item = 4;
			break;
		case kRenderQuality_Min:
			item = 5;
			break;
	}
	// only set the control value if it is different from the current value
	UInt32 value = GetControl32BitValue(mControl);
	if (item != (int)value)
		SetControl32BitValue(mControl, item);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AURenderQualityPopup::HandleControlChange
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	AURenderQualityPopup::HandleControlChange ()
{
	UInt32	renderQuality;
	int 	val = GetControl32BitValue(mControl);
	
	switch (val) {
		case 1:
			renderQuality = kRenderQuality_Max;
			break;
		case 2:
			renderQuality = kRenderQuality_High;
			break;
		case 3:
			renderQuality = kRenderQuality_Medium;
			break;
		case 4:
			renderQuality = kRenderQuality_Low;
			break;
		case 5:
			renderQuality = kRenderQuality_Min;
			break;
	}
	
	AudioUnitSetProperty (mView->GetEditAudioUnit(), 
							kAudioUnitProperty_RenderQuality,
							kAudioUnitScope_Global, 
							0, 
							&renderQuality, 
							sizeof(renderQuality));
	mView->Update (true);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AURenderQualityPopup::RegisterEvents
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	AURenderQualityPopup::RegisterEvents ()
{
	EventTypeSpec events[] = {
		{ kEventClassCommand, kEventCommandProcess}
	};
	
	WantEventTypes(GetWindowEventTarget(mView->GetCarbonWindow()), GetEventTypeCount(events), events);
}

void RenderQualityListener(void * inRefCon, AudioUnit ci, AudioUnitPropertyID  inID, AudioUnitScope inScope, AudioUnitElement inElement) {
	AURenderQualityPopup *This = (AURenderQualityPopup*)inRefCon;
	
	UInt32 theQuality;
	UInt32 theSize = sizeof (UInt32);
	
	ComponentResult result = AudioUnitGetProperty(ci, inID, inScope, inElement, &theQuality, &theSize);
	
	if (result == noErr)
		This->HandlePropertyChange(theQuality);
}




