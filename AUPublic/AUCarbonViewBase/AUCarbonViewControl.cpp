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
	AUCarbonViewControl.cpp
	
=============================================================================*/

#include "AUCarbonViewControl.h"
#include "AUCarbonViewBase.h"

AUCarbonViewControl::AUCarbonViewControl(AUCarbonViewBase *ownerView, AUParameterListenerRef listener, ControlType type, const AUVParameter &param, ControlRef control) :
	mOwnerView(ownerView),
	mListener(listener),
	mType(type),
	mParam(param),
	mControl(control)
{
	SetControlReference(control, SInt32(this));
}

AUCarbonViewControl::~AUCarbonViewControl()
{
	AUListenerRemoveParameter(mListener, this, &mParam);
}

AUCarbonViewControl* AUCarbonViewControl::mLastControl = NULL;

void	AUCarbonViewControl::Bind()
{
	mInControlInitialization = true;
	AUListenerAddParameter(mListener, this, &mParam);
		// will cause an almost-immediate callback
	
	EventTypeSpec events[] = {
		{ kEventClassControl, kEventControlValueFieldChanged }	// N.B. OS X only
	};
	
	WantEventTypes(GetControlEventTarget(mControl), GetEventTypeCount(events), events);

	if (mType == kTypeContinuous || mType == kTypeText || mType == kTypeDiscrete) {
		EventTypeSpec events[] = {
			{ kEventClassControl, kEventControlHit },
			{ kEventClassControl, kEventControlClick }
		};
		WantEventTypes(GetControlEventTarget(mControl), GetEventTypeCount(events), events);
	} 

	if (mType == kTypeText) {
		EventTypeSpec events[] = {
			{ kEventClassControl, kEventControlSetFocusPart }
		};
		WantEventTypes(GetControlEventTarget(mControl), GetEventTypeCount(events), events); 
		ControlKeyFilterUPP proc = NumericKeyFilterCallback;
		verify_noerr(SetControlData(mControl, 0, kControlEditTextKeyFilterTag, sizeof(proc), &proc));
	}
	
	Update(true);
}

void	AUCarbonViewControl::ParameterToControl(Float32 paramValue)
{
	switch (mType) {
	case kTypeContinuous:
		SetValueFract(AUParameterValueToLinear(paramValue, &mParam));
		break;
	case kTypeDiscrete:
		{
			long value = long(paramValue);

			if (mParam.HasNamedParams()) {
				// if we're dealing with menus they behave differently!
				// becaue setting min and max doesn't work correctly for the control value
				// first menu item always reports a control value of 1
				ControlKind ctrlKind;
				if (GetControlKind(mControl, &ctrlKind) == noErr) {
					if ((ctrlKind.kind == kControlKindPopupArrow) 
						|| (ctrlKind.kind == kControlKindPopupButton))				
					{
						value = value - long(mParam.ParamInfo().minValue) + 1;
					}
				}
			}

			SetValue (value);
		}
		break;
	case kTypeText:
		{
			char valstr[32];
			AUParameterFormatValue(	paramValue, &mParam, valstr, 3);
			CFStringRef cfstr = CFStringCreateWithCString(NULL, valstr, kCFStringEncodingASCII);
			SetTextValue(cfstr);
			CFRelease (cfstr);
		}
		break;
	}
}

void	AUCarbonViewControl::ControlToParameter()
{
	const AudioUnitParameterInfo &paramInfo = ParamInfo();
	if (mInControlInitialization == true) {
		mInControlInitialization = false;
		return;
	}

	switch (mType) {
	case kTypeContinuous:
		{
			double controlValue = GetValueFract();
			Float32 paramValue = AUParameterValueFromLinear(controlValue, &mParam);
			mParam.SetValue(mListener, this, paramValue);
		}
		break;
	case kTypeDiscrete:
		{
			long value = GetValue();

			if (mParam.HasNamedParams()) {
				// if we're dealing with menus they behave differently!
				// becaue setting min and max doesn't work correctly for the control value
				// first menu item always reports a control value of 1
				ControlKind ctrlKind;
				if (GetControlKind(mControl, &ctrlKind) == noErr) {
					if ((ctrlKind.kind == kControlKindPopupArrow) 
						|| (ctrlKind.kind == kControlKindPopupButton))				
					{
						value = value + long(mParam.ParamInfo().minValue) - 1;
					}
				}
			}
			
			mParam.SetValue (mListener, this, value);
		}
		break;
	case kTypeText:
		{
			Float32 paramValue = paramInfo.defaultValue;
			CFStringRef cfstr = GetTextValue();
			char valstr[32];
			CFStringGetCString(cfstr, valstr, sizeof(valstr), kCFStringEncodingASCII);
			sscanf(valstr, "%f", &paramValue);
			mParam.SetValue(mListener, this, (mParam.IsIndexedParam() ? (int)paramValue : paramValue));
		}
		break;
	}
}

void	AUCarbonViewControl::SetValueFract(double value)
{
	SInt32 minimum = GetControl32BitMinimum(mControl);
	SInt32 maximum = GetControl32BitMaximum(mControl);
	SInt32 cval = SInt32(value * (maximum - minimum) + minimum + 0.5);
	SetControl32BitValue(mControl, cval);
}

#define WORKAROUND_10_1_2_SLIDER_BUG 0

double	AUCarbonViewControl::GetValueFract()
{
	SInt32 minimum = GetControl32BitMinimum(mControl);
	SInt32 maximum = GetControl32BitMaximum(mControl);
	SInt32 cval = GetControl32BitValue(mControl);
	double result = double(cval - minimum) / double(maximum - minimum);
#if WORKAROUND_10_1_2_SLIDER_BUG
	Rect bounds;
	GetControlBounds(mControl, &bounds);
	int width = bounds.right - bounds.left, height = bounds.top - bounds.bottom;
	int bigdim = width > height ? width : height;
	double fudge = double(bigdim - 2) / double(bigdim);
	if (result >= fudge) result = 1.0;
#endif
//	printf("min=%ld, max=%ld, value=%ld, result=%f\n", minimum, maximum, cval, result);
	return result;
}

void	AUCarbonViewControl::SetTextValue(CFStringRef cfstr)
{
	verify_noerr(SetControlData(mControl, 0, kControlEditTextCFStringTag, sizeof(CFStringRef), &cfstr));
	DrawOneControl(mControl);	// !!msh-- This needs to be changed to HIViewSetNeedsDisplay()
}

CFStringRef	AUCarbonViewControl::GetTextValue()
{
	CFStringRef cfstr;
	verify_noerr(GetControlData(mControl, 0, kControlEditTextCFStringTag, sizeof(CFStringRef), &cfstr, NULL));
	return cfstr;
}

void	AUCarbonViewControl::SetValue(long value)
{
	SetControl32BitValue(mControl, value);
}

long	AUCarbonViewControl::GetValue()
{
	return GetControl32BitValue(mControl);
}


bool	AUCarbonViewControl::HandleEvent(EventRef event)
{	
	UInt32 eclass = GetEventClass(event);
	UInt32 ekind = GetEventKind(event);
	ControlRef control;
	bool		handled = true;
	
	switch (eclass) {
	case kEventClassControl:
		switch (ekind) {
			case kEventControlSetFocusPart:	// tab
				handled = !handled;		// fall through to next case
				mLastControl = this;
			case kEventControlValueFieldChanged:
				GetEventParameter(event, kEventParamDirectObject, typeControlRef, NULL, sizeof(ControlRef), NULL, &control);
				verify(control == mControl);
				ControlToParameter();
				return handled;			
			case kEventControlClick:
				if (mLastControl != this) {
					if (mLastControl != NULL)
						mLastControl->Update(false);
					mLastControl = this;	
				} mOwnerView->TellListener(mParam, kAudioUnitCarbonViewEvent_MouseDownInControl, NULL);
				break;	// don't return true, continue normal processing
			case kEventControlHit:
				if (mLastControl != this) {
					if (mLastControl != NULL)
						mLastControl->Update(false);
					mLastControl = this;	
				} 
				mOwnerView->TellListener(mParam, kAudioUnitCarbonViewEvent_MouseUpInControl, NULL);

				break;	// don't return true, continue normal processing
		}
	}
	return !handled;
}

pascal void	AUCarbonViewControl::SliderTrackProc(ControlRef theControl, ControlPartCode partCode)
{
	// this doesn't need to actually do anything
//	AUCarbonViewControl *This = (AUCarbonViewControl *)GetControlReference(theControl);
}

pascal ControlKeyFilterResult	AUCarbonViewControl::NumericKeyFilterCallback(ControlRef theControl, 
												SInt16 *keyCode, SInt16 *charCode, 
												EventModifiers *modifiers)
{
	SInt16 c = *charCode;
	if (isdigit(c) || c == '+' || c == '-' || c == '.' || c == '\b' || c == 0x7F || (c >= 0x1c && c <= 0x1f)
	|| c == '\t')
		return kControlKeyFilterPassKey;
	if (c == '\r' || c == 3) {	// return or Enter
		AUCarbonViewControl *This = (AUCarbonViewControl *)GetControlReference(theControl);
		ControlEditTextSelectionRec sel = { 0, 32767 };
		SetControlData(This->mControl, 0, kControlEditTextSelectionTag, sizeof(sel), &sel);
		This->ControlToParameter();
	}
	return kControlKeyFilterBlockKey;
}

#pragma mark ___AUPropertyControl
bool	AUPropertyControl::HandleEvent(EventRef event)
{	
	UInt32 eclass = GetEventClass(event);
	UInt32 ekind = GetEventKind(event);
	
	switch (eclass) {
	case kEventClassControl:
		switch (ekind) {
		case kEventControlValueFieldChanged:
			HandleControlChange();
			return true;	// handled
		}
	}

	return false;
}

void	AUPropertyControl::RegisterEvents ()
{
	EventTypeSpec events[] = {
		{ kEventClassControl, kEventControlValueFieldChanged }	// N.B. OS X only
	};
	
	WantEventTypes(GetControlEventTarget(mControl), GetEventTypeCount(events), events);
}

void	AUPropertyControl::EmbedControl (ControlRef theControl) 
{ 
	mView->EmbedControl (theControl); 
}

#pragma mark ___AUVPreset
void PresetListener(void * inRefCon, AudioUnit ci, AudioUnitPropertyID  inID, AudioUnitScope inScope, AudioUnitElement inElement);

AUVPresets::AUVPresets (AUCarbonViewBase* 		inParentView, 
						CFArrayRef& 			inPresets,
						Point 					inLocation, 
						int 					nameWidth, 
						int 					controlWidth, 
						ControlFontStyleRec & 	inFontStyle)
	: AUPropertyControl (inParentView),
	  mPresets (inPresets),
	  mView (inParentView)
{
	Rect r;
	
	// ok we now have an array of factory presets
	// get their strings and display them

	r.top = inLocation.v;		r.bottom = r.top + 16;
	r.left = inLocation.h;		r.right = r.left + nameWidth;
	inFontStyle.just = teFlushRight;
	
	ControlRef theControl;
	verify_noerr(CreateStaticTextControl(mView->GetCarbonWindow(), &r, CFSTR("Factory Presets:"), &inFontStyle, &theControl));
	EmbedControl(theControl);
	
	r.top -= 2;
	r.left = r.right + 8;	r.right = r.left + controlWidth;

	verify_noerr(CreatePopupButtonControl (mView->GetCarbonWindow(), &r, NULL, 
												-12345,// DON'T GET MENU FROM RESOURCE mMenuID,!!!
												FALSE, //variableWidth, 
												0, // titleWidth, 
												0,//titleJustification, 
												0,//titleStyle, 
												&mControl));
	
	MenuRef menuRef;
	verify_noerr(CreateNewMenu( 1, 0, &menuRef));
	
	int numPresets = CFArrayGetCount(mPresets);
	
	for (int i = 0; i < numPresets; ++i)
	{
		AUPreset* preset = (AUPreset*) CFArrayGetValueAtIndex (mPresets, i);
		verify_noerr(AppendMenuItemTextWithCFString (menuRef, preset->presetName, 0, 0, 0));
	}
					
	verify_noerr(SetControlData(mControl, 0, kControlPopupButtonMenuRefTag, sizeof(menuRef), &menuRef));

	inFontStyle.just = 0;
	verify_noerr (SetControlFontStyle (mControl, &inFontStyle));

	SetControl32BitMaximum (mControl, numPresets);

	// find which menu item is the Default preset
	UInt32 propertySize = sizeof(AUPreset);
	AUPreset defaultPreset;
	ComponentResult result = AudioUnitGetProperty (mView->GetEditAudioUnit(), 
									kAudioUnitProperty_PresentPreset,
									kAudioUnitScope_Global, 
									0, 
									&defaultPreset, 
									&propertySize);
	
	mPropertyID = kAudioUnitProperty_PresentPreset;
	
	if (result != noErr) {	// if the PresentPreset property is not implemented, fall back to the CurrentPreset property
		ComponentResult result = AudioUnitGetProperty (mView->GetEditAudioUnit(), 
									kAudioUnitProperty_CurrentPreset,
									kAudioUnitScope_Global, 
									0, 
									&defaultPreset, 
									&propertySize);
		mPropertyID = kAudioUnitProperty_CurrentPreset;
		if (result == noErr)
			CFRetain (defaultPreset.presetName);
	} 
	
	HandlePropertyChange(defaultPreset);

	EmbedControl (mControl);
	
	AudioUnitAddPropertyListener(mView->GetEditAudioUnit(), mPropertyID, PresetListener, this);
	
	RegisterEvents();
}

AUVPresets::~AUVPresets () { 
	AudioUnitRemovePropertyListener (mView->GetEditAudioUnit(), mPropertyID, PresetListener);
	CFRelease (mPresets); 
}

void	AUVPresets::HandleControlChange ()
{
	SInt32 i = GetControl32BitValue(mControl);
	printf ("value = %ld\n", i);
	if (i > 0)
	{
		AUPreset* preset = (AUPreset*) CFArrayGetValueAtIndex (mPresets, i-1);
	
		verify_noerr(AudioUnitSetProperty (mView->GetEditAudioUnit(), 
									mPropertyID,	// either currentPreset or PresentPreset depending on which is supported
									kAudioUnitScope_Global, 
									0, 
									preset, 
									sizeof(AUPreset)));
									
		// when we change a preset we can't expect the AU to update its state
		// as it isn't meant to know that its being viewed!
		// so we tell the view to update its parameter values
		// we're in the UI thread here so pass in true.
		mView->Update (true);
	}
}

void	AUVPresets::HandlePropertyChange(AUPreset &preset) {
	// check to see if the preset is in our menu
	int numPresets = CFArrayGetCount(mPresets);
	
	if (preset.presetNumber < 0)	
		SetControl32BitValue (mControl, 0); //controls are one-based
	else {
		for (SInt32 i = 0; i < numPresets; ++i) {
			AUPreset* currPreset = (AUPreset*) CFArrayGetValueAtIndex (mPresets, i);
			if (preset.presetNumber == currPreset->presetNumber) {
				SetControl32BitValue (mControl, ++i); //controls are one-based
				break;
			}
		}
	}
	
	if (preset.presetName)
		CFRelease (preset.presetName);
}


void PresetListener(void * inRefCon, AudioUnit ci, AudioUnitPropertyID  inID, AudioUnitScope inScope, AudioUnitElement inElement) {
	AUVPresets *This = (AUVPresets*)inRefCon;
	
	UInt32 propertySize = sizeof(AUPreset);
	AUPreset currentPreset;
	ComponentResult result = AudioUnitGetProperty (ci, 
									inID,
									kAudioUnitScope_Global, 
									0, 
									&currentPreset, 
									&propertySize);
	
	if (result == noErr) {
		if (inID == kAudioUnitProperty_CurrentPreset && currentPreset.presetName)
			CFRetain (currentPreset.presetName);
		This->HandlePropertyChange(currentPreset);
	}
}

#pragma mark ___AUBypassEffect

AUBypassEffect::AUBypassEffect (AUCarbonViewBase * 		inBase, 
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
										CFSTR("Bypass effect"), 
										0, 
										true, 
										&mControl));
	verify_noerr (SetControlFontStyle (mControl, &inFontStyle));
	SInt16 baseLineOffset;
	verify_noerr (GetBestControlRect (mControl, &r, &baseLineOffset));
	r.bottom += baseLineOffset;
	SInt16 difference = (r.left + inRightEdge) - r.right;
	r.right += difference;
	r.left += difference;
	SetControlBounds (mControl, &r);
	EmbedControl(mControl);

	RegisterEvents();
}

void	AUBypassEffect::HandleControlChange ()
{
	SInt32 val = GetControl32BitValue(mControl);
	
	AudioUnitSetProperty (mView->GetEditAudioUnit(), 
							kAudioUnitProperty_BypassEffect,
							kAudioUnitScope_Global, 
							0, 
							&val, 
							sizeof(val));
}
