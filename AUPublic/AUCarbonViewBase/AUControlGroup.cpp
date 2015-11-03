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
	AUControlGroup.cpp
	
=============================================================================*/

#include <Carbon/Carbon.h>
#include "AUCarbonViewBase.h"
#include "AUCarbonViewControl.h"
#include "AUControlGroup.h"

#define kSliderThinDimension 10
#define kLabelAndSliderSpacing	4

void	AUControlGroup::CreateLabelledSlider(
										AUCarbonViewBase *			auView, 
										AUVParameter &				auvp, 
										const Rect &				area, 
										Point 						labelSize, 
										const ControlFontStyleRec &	inFontStyle)
{
	ControlFontStyleRec fontStyle = inFontStyle;
	Rect minValRect, maxValRect, sliderRect;
	ControlRef newControl;
	int width = area.right - area.left, height = area.bottom - area.top;
	char text[64];
	CFStringRef cfstr;
	int sliderValueMax, sliderValueMin, sliderValueDefault;
	AUCarbonViewControl::ControlType sliderType;
	
	bool horizontal = (width > height);

	if (horizontal) {
		maxValRect.top = minValRect.top = area.top + (height - labelSize.v) / 2;
		minValRect.left = area.left;
		maxValRect.left = area.right - labelSize.h;
		
		minValRect.bottom = minValRect.top + labelSize.v;
		minValRect.right = minValRect.left + labelSize.h;
		maxValRect.bottom = maxValRect.top + labelSize.v;
		maxValRect.right = maxValRect.left + labelSize.h;

		sliderRect.left = minValRect.right + kLabelAndSliderSpacing;
		sliderRect.right = maxValRect.left - kLabelAndSliderSpacing;
		sliderRect.top = area.top + (height - kSliderThinDimension) / 2;
		sliderRect.bottom = sliderRect.top + kSliderThinDimension + 4;

		if (auvp.IsIndexedParam ()) {
			sliderValueMin = sliderValueDefault = int(auvp.ParamInfo().minValue);
			sliderValueMax = int(auvp.ParamInfo().maxValue);
			sliderType = AUCarbonViewControl::kTypeDiscrete;
		} else {
			sliderValueMin = sliderValueDefault = 0;
			sliderValueMax = sliderRect.right - sliderRect.left;
			sliderType = AUCarbonViewControl::kTypeContinuous;
		}
	} else {
		maxValRect.left = minValRect.left = area.left + (width - labelSize.h) / 2;
		maxValRect.top = area.top;
		minValRect.top = area.bottom - labelSize.v;
		
		minValRect.bottom = minValRect.top + labelSize.v;
		minValRect.right = minValRect.left + labelSize.h;
		maxValRect.bottom = maxValRect.top + labelSize.v;
		maxValRect.right = maxValRect.left + labelSize.h;
	
		sliderRect.left = area.left + (width - kSliderThinDimension) / 2;
		sliderRect.right = sliderRect.left + kSliderThinDimension + 4;
		sliderRect.top = maxValRect.bottom + kLabelAndSliderSpacing;
		sliderRect.bottom = minValRect.top - kLabelAndSliderSpacing;

		if (auvp.IsIndexedParam ()) {
			sliderValueMin = sliderValueDefault = int(auvp.ParamInfo().minValue);
			sliderValueMax = int(auvp.ParamInfo().maxValue);			
			sliderType = AUCarbonViewControl::kTypeDiscrete;
		} else {
			sliderValueMin = sliderValueDefault = 0;
			sliderValueMax = sliderRect.bottom - sliderRect.top;
			sliderType = AUCarbonViewControl::kTypeContinuous;
		}
	}

	// minimum value label
	if (labelSize.v > 0 && labelSize.h > 0) {
		// check to see if the minimum value has a label
		cfstr = auvp.GetValueNameCopy(auvp.ParamInfo().minValue);
		if (cfstr == NULL) {
			AUParameterFormatValue(auvp.ParamInfo().minValue, &auvp, text, 3);
			cfstr = CFStringCreateWithCString(NULL, text, kCFStringEncodingMacRoman);
		}
		fontStyle.just = horizontal ? teFlushRight : teCenter;
		verify_noerr(CreateStaticTextControl(auView->GetCarbonWindow(), &minValRect, cfstr, &fontStyle, &newControl));
		CFRelease(cfstr);
		verify_noerr(auView->EmbedControl(newControl));
	
		// maximum value label
		cfstr = auvp.GetValueNameCopy(auvp.ParamInfo().maxValue);
		if (cfstr == NULL) {
			AUParameterFormatValue(auvp.ParamInfo().maxValue, &auvp, text, 3);
			cfstr = CFStringCreateWithCString(NULL, text, kCFStringEncodingMacRoman);
		}
		fontStyle.just = horizontal ? teFlushLeft : teCenter;
		verify_noerr(CreateStaticTextControl(auView->GetCarbonWindow(), &maxValRect, cfstr, &fontStyle, &newControl));
		CFRelease(cfstr);
		verify_noerr(auView->EmbedControl(newControl));
	}
	
	// slider
	verify_noerr(CreateSliderControl(auView->GetCarbonWindow(), &sliderRect, sliderValueDefault, sliderValueMin, sliderValueMax, kControlSliderDoesNotPoint, 0, true, AUCarbonViewControl::SliderTrackProc, &newControl));


	ControlSize small = kControlSizeSmall;
	SetControlData(newControl, kControlEntireControl, kControlSizeTag, sizeof(ControlSize), &small);
	auView->AddCarbonControl(sliderType, auvp, newControl);
}

void	AUControlGroup::CreateLabelledSliderAndEditText(
										AUCarbonViewBase *			auView, 
										AUVParameter &				auvp, 
										const Rect &				area, 
										Point 						labelSize, 
										Point						editTextSize,
										const ControlFontStyleRec &	inFontStyle)
{
	ControlFontStyleRec fontStyle = inFontStyle;
	Rect sliderArea, textArea;
	ControlRef newControl;
	int width = area.right - area.left, height = area.bottom - area.top;
	
	bool horizontal = (width > height);

	sliderArea = area;
	textArea = area;
	if (horizontal) {
		textArea.left = area.right - editTextSize.h;
		sliderArea.right = textArea.left - kLabelAndSliderSpacing;
		textArea.top = area.top + (height - editTextSize.v) / 2;
		textArea.bottom = textArea.top + editTextSize.v;
	} else {
		textArea.top = area.bottom - editTextSize.v;
		sliderArea.bottom = textArea.top - kLabelAndSliderSpacing;
		textArea.left = area.left + (width - editTextSize.h) / 2;
		textArea.right = textArea.left + editTextSize.h;
	}
	CreateLabelledSlider(auView, auvp, sliderArea, labelSize, fontStyle);
	
	verify_noerr(CreateEditUnicodeTextControl(auView->GetCarbonWindow(), &textArea, CFSTR(""), false, 
			&fontStyle, &newControl));
	auView->AddCarbonControl(AUCarbonViewControl::kTypeText, auvp, newControl);
}

void	AUControlGroup::CreatePopupMenu (AUCarbonViewBase *			auView, 
										AUVParameter &				auvp, 
										const Rect &				area, 
										const ControlFontStyleRec &	inFontStyle)
{
	ControlRef thePopUp;
			
	verify_noerr(CreatePopupButtonControl (auView->GetCarbonWindow(), &area, NULL, 
												-12345,// DON'T GET MENU FROM RESOURCE mMenuID,!!!
												FALSE, //variableWidth, 
												0, // titleWidth, 
												0,//titleJustification, 
												0,//titleStyle, 
												&thePopUp));
	
	MenuRef menuRef;
	verify_noerr(CreateNewMenu( 1, 0, &menuRef));

	for (int i = 0; i < auvp.GetNumIndexedParams(); ++i)
	{
		verify_noerr(AppendMenuItemTextWithCFString (menuRef, auvp.GetParamName(i), kMenuItemAttrIgnoreMeta, 0, 0));
	}
					
	verify_noerr(SetControlData(thePopUp, 0, kControlPopupButtonMenuRefTag, sizeof(menuRef), &menuRef));

	verify_noerr (SetControlFontStyle (thePopUp, &inFontStyle));

	SetControl32BitMaximum(thePopUp, auvp.GetNumIndexedParams());
	
	auView->AddCarbonControl(AUCarbonViewControl::kTypeDiscrete, auvp, thePopUp);
}

void	AUControlGroup::AddAUInfo (		AUCarbonViewBase *			auView, 
										const Point &				inLocation, 
										const SInt16 				inRightOffset,
										const SInt16				inTotalWidth)
{
	Rect r;
	ControlRef newControl;
	ControlFontStyleRec fontStyle;
	fontStyle.flags = kControlUseFontMask | kControlUseJustMask;
	
	Handle h1 = NewHandle(4);
	ComponentDescription desc;
	OSStatus err = GetComponentInfo ((Component)auView->GetEditAudioUnit(), &desc, h1, 0, 0);

	CFStringRef cfstr;
	UInt32 size = sizeof(cfstr);
	OSStatus noContextName = AudioUnitGetProperty (auView->GetEditAudioUnit(),
												kAudioUnitProperty_ContextName,
												kAudioUnitScope_Global,
												0,
												&cfstr,
												&size);
	if (err == noErr)
	{
		fontStyle.font = kControlFontSmallBoldSystemFont;
		char str[256];
		HLock(h1);
		char* ptr1 = *h1;
		// Get the manufacturer's name... look for the ':' character convention
		int len = *ptr1++;
		char* displayStr = 0;
		
		for (int i = 0; i < len; ++i) {
			if (ptr1[i] == ':') { // found the name
				ptr1[i++] = 0;
				displayStr = ptr1;
				break;
			}
		}
		
		r.top = SInt16(inLocation.v);		r.bottom = SInt16(inLocation.v) + 16;

		if (displayStr)
		{
			static const char* manuStr = "Manufacturer: ";
			static const int manuStrLen = strlen(manuStr);
			
			strcpy (str, manuStr);
			strcpy (str + manuStrLen, displayStr);
			
			r.left = inLocation.h + inRightOffset;	
			r.right = r.left + inTotalWidth - inRightOffset;
			fontStyle.just = teFlushRight;
			
			CFStringRef cfstr = CFStringCreateWithCString(NULL, str, kCFStringEncodingUTF8);
			verify_noerr(CreateStaticTextControl(auView->GetCarbonWindow(), &r, cfstr, &fontStyle, &newControl));
			verify_noerr(auView->EmbedControl(newControl));
			CFRelease (cfstr);
										
			//move displayStr ptr past the manu, to the name
			// we move the characters down a index, because the handle doesn't have any room
			// at the end for the \0
			if (noContextName) {
				int i = strlen(displayStr), j = 0;
				while (displayStr[++i] == ' ' && i < len)
					;
				while (i < len)
					displayStr[j++] = displayStr[i++];
				displayStr[j] = 0;
			}
		} else {
			if (noContextName) {
				displayStr = ptr1;
				int i = 1, j = 0;
				while (++i < len)
					displayStr[j++] = displayStr[i];
				displayStr[j] = 0;
			}
		}
		
		r.left = inLocation.h;	r.right = r.left + inRightOffset;
		fontStyle.just = 0;
	
		if (noContextName) {
			static const char* nameStr = "Audio Unit: ";
			static const int nameStrLen = strlen(nameStr);
			
			strcpy (str, nameStr);
			strcpy (str + nameStrLen, displayStr);
			
			cfstr = CFStringCreateWithCString(NULL, str, kCFStringEncodingUTF8);
		}
		
		verify_noerr(CreateStaticTextControl(auView->GetCarbonWindow(), &r, cfstr, &fontStyle, &newControl));
		verify_noerr(auView->EmbedControl(newControl));
		CFRelease (cfstr);
	}
	DisposeHandle (h1);
}


