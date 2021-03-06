/*	Copyright � 2007 Apple Inc. All Rights Reserved.
	
	Disclaimer: IMPORTANT:  This Apple software is supplied to you by 
			Apple Inc. ("Apple") in consideration of your agreement to the
			following terms, and your use, installation, modification or
			redistribution of this Apple software constitutes acceptance of these
			terms.  If you do not agree with these terms, please do not use,
			install, modify or redistribute this Apple software.
			
			In consideration of your agreement to abide by the following terms, and
			subject to these terms, Apple grants you a personal, non-exclusive
			license, under Apple's copyrights in this original Apple software (the
			"Apple Software"), to use, reproduce, modify and redistribute the Apple
			Software, with or without modifications, in source and/or binary forms;
			provided that if you redistribute the Apple Software in its entirety and
			without modifications, you must retain this notice and the following
			text and disclaimers in all such redistributions of the Apple Software. 
			Neither the name, trademarks, service marks or logos of Apple Inc. 
			may be used to endorse or promote products derived from the Apple
			Software without specific prior written permission from Apple.  Except
			as expressly stated in this notice, no other rights or licenses, express
			or implied, are granted by Apple herein, including but not limited to
			any patent rights that may be infringed by your derivative works or by
			other works in which the Apple Software may be incorporated.
			
			The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
			MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
			THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS
			FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND
			OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
			
			IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL
			OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
			SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
			INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
			MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
			AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE),
			STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
			POSSIBILITY OF SUCH DAMAGE.
*/
/*=============================================================================
	GenericAUView.cpp
	
=============================================================================*/

#include "GenericAUView.h"
#include "AUViewLocalizedStringKeys.h"
#include "AUParamInfo.h"

// consistent spacing defines
#define GV_GUTTER_MARGIN			10
#define GV_TEXT_SPACING				5
#define GV_HORIZONTAL_SPACING		10
#define GV_VERTICAL_SPACING			5
#define GV_PROPERTY_INDENTATION		64

static bool sLocalized = false;
static CFStringRef kPropertiesLabelString = kAUViewLocalizedStringKey_Properties;
static CFStringRef kParametersLabelString = kAUViewLocalizedStringKey_Parameters;

	// comnpatibility with 10.2 SDK builds
enum {
	kTEMP_AudioUnitParameterFlag_MeterReadOnly		= (1L << 15)
};

class MeterView {
public:
		MeterView (GenericAUView	 			* auView,
					const CAAUParameter 		& auvp, 
					Rect 						& area, 
					Point 						& labelSize,
					int							paramTagWidth,
					ControlFontStyleRec 		& fontStyle);
	
	void Draw ();

private:
    GenericAUView *			mAUView;
	Float32 				mMinValue, mValueRange;
	UInt32					mLastValueDrawn;
    Boolean					mLastActiveState;
	CAAUParameter		 	mParam;
	Rect 					mRect;
    
    RGBColor				mForegroundActiveColor,
                            mBackgroundActiveColor,
                            mForegroundInactiveColor,
                            mBackgroundInactiveColor;
};

// properties
static const int kPropertyWidth = 140;
static const int kControlWidgetWidth = 280;
static const int kPropertyPopupAdjustment = -13;
static const int numProperties = 4;

// parameters
static const int kHeadingBodyDist = 16;
static const int kParamNameWidth = 140;
static const int kSliderWidth = 280;
static const int kEditTextWidth = 50;
static const int kParamTagWidth = 36;
static const int kMinMaxWidth = 40;
static const int kPopupAdjustment = -46;
static const int kReadOnlyMeterWidth = 326;

static const int kClumpSeparator = 8;
static const int kRowHeight = 20;
static const int kTextHeight = 14;

COMPONENT_ENTRY(GenericAUView)

GenericAUView::GenericAUView(AudioUnitCarbonView auv) 
		: AUCarbonViewBase(auv), 
		  mEventListener (0),
		  mFactoryPresets(0), 
		  mLoadCPU(0), 
		  mQualityPopup(0), 
		  mStreamingCheckbox(0)
{
	if (!sLocalized) {
		// Because we are in a component, we need to load our bundle by identifier so we can access our localized strings
		// It is important that the string passed here exactly matches that in the Info.plist Identifier string
		CFBundleRef bundle = CFBundleGetBundleWithIdentifier( kLocalizedStringBundle_AUView );
		if (bundle != NULL) {
			kParametersLabelString = CFCopyLocalizedStringFromTableInBundle(kParametersLabelString, kLocalizedStringTable_AUView, bundle, CFSTR(""));
			kPropertiesLabelString = CFCopyLocalizedStringFromTableInBundle(kPropertiesLabelString, kLocalizedStringTable_AUView, bundle, CFSTR(""));
            
			sLocalized = true;
		}
	}
}

GenericAUView::~GenericAUView()
{
	Cleanup();
	if (mEventListener)
		AUListenerDispose(mEventListener);
}

void 		GenericAUView::Cleanup()
{
	ClearControls();
    
	UInt16 		numSubControls;
	ControlRef 	subControl;
	if (CountSubControls (mCarbonPane, &numSubControls) == noErr){
		for (UInt16 i = numSubControls; i > 0; --i) {
			GetIndexedSubControl(mCarbonPane, i, &subControl);
			DisposeControl(subControl);
		}
	}
 
	if (mFactoryPresets) {
		mFactoryPresets->RemoveInterest (mEventListener, this);
		delete mFactoryPresets;
		mFactoryPresets = NULL;
	}
	if (mLoadCPU) {
		mLoadCPU->RemoveInterest (mEventListener, this);
		delete mLoadCPU;
		mLoadCPU = NULL;
	}
	if (mQualityPopup) {
		mQualityPopup->RemoveInterest (mEventListener, this);
		delete mQualityPopup;
		mQualityPopup = NULL;
	}
	if (mStreamingCheckbox) {
		mStreamingCheckbox->RemoveInterest (mEventListener, this);
		delete mStreamingCheckbox;
		mStreamingCheckbox = NULL;
	}

	for (MeterList::iterator iter = mMeters.begin(); iter < mMeters.end(); ++iter)
		delete (*iter);
	mMeters.clear();
}

OSStatus	GenericAUView::CreateUIForParameters (Float32 inXOffset, Float32 inYOffset, Rect *outSize) 
{
	// for each parameter in the global scope, create:
	//		label	horiozontal slider	text entry	label
	// descending vertically
	// inside mCarbonWindow, embedded in mCarbonPane

	outSize->top	= short(inYOffset);
	outSize->left	= short(inXOffset);
	outSize->bottom	= outSize->top;
	outSize->right	= outSize->left;
	
	try {
		// don't include expert params, inlcude read only
		AUParamInfo paramInfo (mEditAudioUnit, false, true);
		
		int y = short(inYOffset);
		const int x = short(inXOffset);
		
		Point location;
		location.h = x;
		location.v = y;
		
		location.v = (y += kHeadingBodyDist);
		outSize->bottom += kHeadingBodyDist;
		
		ControlFontStyleRec fontStyle;
		fontStyle.flags = kControlUseFontMask | kControlUseJustMask;
		fontStyle.font = kControlFontSmallSystemFont;
		
		// Create 'Parameters' header
		Rect r = {location.v, location.h, location.v + kTextHeight, location.h + 66};
		ControlRef ref = GetSizedStaticTextControl (kParametersLabelString, &r, true);
		if (ref) {
			EmbedControl(ref);
			Rect bestRect = {0, 0, 0, 0};
			GetControlBounds(ref, &bestRect);
			r.left = bestRect.right + 2;
			r.right = location.h + 465;
			r.bottom = bestRect.bottom;
			if (noErr == CreateSeparatorControl (GetCarbonWindow(), &r, &ref))
				EmbedControl(ref);
			y += kRowHeight;
			location.v += kRowHeight;
			outSize->bottom += kRowHeight;
		}
		
		AUParamInfo::ParameterMap::const_iterator i = paramInfo.Map().begin();
		while (1) {
			if (i != paramInfo.Map().end()) {
				for (AUParamInfo::ParameterList::const_iterator piter = (*i).second.begin(); piter != (*i).second.end(); ++piter)
				{
					DrawParameter (*piter, x, y, fontStyle, outSize);
					y += kRowHeight;
					outSize->bottom += kRowHeight;
				}
			}
			if (i != paramInfo.Map().end() && ++i != paramInfo.Map().end()) {
				y += kClumpSeparator;
				outSize->bottom += kClumpSeparator;
			} else {
				// now we want to add 16 pixels to the bottom and the right
				mBottomRight.h += 16;
				mBottomRight.v += 16;
				break;
			}
		}
	} catch (OSStatus err) {
		return err;
	} catch (...) {
		return -1;
	}
	return noErr;
}

void	GenericAUView::DrawParameterTitleInRect(const CFStringRef inString, ControlFontStyleRec &fontStyle, Rect &inRect)
{
	Rect r = inRect;
	Rect separatorRect = inRect;
	
	separatorRect.left = separatorRect.right - 4;
	r.right = r.right - 4;
	
	// create separator (":")
	ControlRef newControl = NULL;
	if (noErr == CreateStaticTextControl(mCarbonWindow, &separatorRect, kAUViewUnlocalizedString_TitleSeparator, &fontStyle, &newControl))
		verify_noerr(EmbedControl(newControl));
	
	// create parameter name
	if (noErr == CreateStaticTextControl(mCarbonWindow, &r, inString, &fontStyle, &newControl)) {
		// have control truncate at end-of-text
		Boolean BooleanFalse = FALSE;
		verify_noerr(SetControlData(newControl, kControlEntireControl, 'stim' /* kControlStaticTextIsMultilineTag */, sizeof(Boolean), &BooleanFalse));
		TruncCode tc = truncEnd;
		verify_noerr(SetControlData(newControl, kControlEntireControl, kControlStaticTextTruncTag, sizeof(TruncCode), &tc));
		verify_noerr(EmbedControl(newControl));
	}
}

void 	GenericAUView::DrawParameter (const CAAUParameter &auvp, const int x, int &y, ControlFontStyleRec &fontStyle, Rect *outSize)
{
	ControlRef newControl;			
	Rect r;
    	
	const AudioUnitParameterInfo &paramInfo = auvp.ParamInfo();
	
	// we treat Boolean params differently, so lets just deal with them here.
	if (auvp.ParamInfo().unit == kAudioUnitParameterUnit_Boolean) 	// BOOLEAN - any flavour
	{
		// Read/Write and Read-only boolean parameters
		if (paramInfo.flags & kAudioUnitParameterFlag_IsReadable) {
			fontStyle.font = kControlFontSmallSystemFont;
						
			Rect r;
			r.bottom = r.top = y; // y -= 1 seems to look better on Tiger -- try on Panther
			r.right = r.left = x + kParamNameWidth + kMinMaxWidth + 10;
			
			if (noErr == CreateCheckBoxControl(GetCarbonWindow(), 
												&r, 
												auvp.GetName(), 
												0, 
												true, 
												&newControl)) 
			{
				// if it's read-only, set the check-box as disabled
				if (!(paramInfo.flags & kAudioUnitParameterFlag_IsWritable)) {
					DisableControl(newControl);
				}
				
				ControlSize smallSize = kControlSizeSmall;
				verify_noerr (SetControlData (newControl, kControlEntireControl, kControlSizeTag, sizeof(ControlSize), &smallSize));
				
				verify_noerr (SetControlFontStyle (newControl, &fontStyle));
				AUCarbonViewControl::SizeControlToFit(newControl);
				
				AddCarbonControl(AUCarbonViewControl::kTypeDiscrete, auvp, newControl);
			}
			
			if (outSize->right - outSize->left < r.right - r.left)
				outSize->right = outSize->left + (r.right - r.left);
			
			y -= 4;	// y += 1 seems to look better on Tiger -- try on Panther
		} else if (paramInfo.flags & kAudioUnitParameterFlag_IsWritable) {
			// Write-only boolean parameters
			
			// [1a] create "Set" button
			Rect buttonRect;
			buttonRect.bottom = buttonRect.top = y - 2;
			buttonRect.right = buttonRect.left = x + kParamNameWidth + kMinMaxWidth + 10; // aligns with read/write check box
			
			CFBundleRef localizationBundle = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.audio.units.Components"));
			CFStringRef buttonTitle =  CFCopyLocalizedStringFromTableInBundle( CFSTR("Set"), CFSTR("CustomUI"), localizationBundle,
										CFSTR("Button title for 'Setting' write-only Boolean Parameters"));
			
			OSStatus err = CreateBevelButtonControl (	GetCarbonWindow(),
														&buttonRect,
														buttonTitle,
														kControlBevelButtonSmallBevel,
														kControlBehaviorPushbutton, NULL, 0, 0, 0,
														&newControl	);
			if (buttonTitle)
				CFRelease (buttonTitle);
			
			if (err == noErr) {
				ControlFontStyleRec fontStyle;
				fontStyle.flags = kControlUseFontMask | kControlUseJustMask;
				fontStyle.font = kControlFontSmallSystemFont;
				fontStyle.just = teFlushLeft;
				verify_noerr (SetControlFontStyle (newControl, &fontStyle));
				
				SInt16 outWidth;
				AUCarbonViewControl::SizeControlToFit(newControl, &outWidth);

				AddCarbonControl(AUCarbonViewControl::kTypeDiscrete, auvp, newControl);
				
				// [2] create parameter title
				Rect parameterNameRect = buttonRect;
				
				parameterNameRect.right = parameterNameRect.left = buttonRect.right + 40;
				parameterNameRect.top += 4;
				parameterNameRect.bottom += 4;
				ControlRef parameterNameText;
				
				if (noErr == CreateStaticTextControl(mCarbonWindow, &parameterNameRect, auvp.GetName(), &fontStyle, &parameterNameText)) {
					AUCarbonViewControl::SizeControlToFit(parameterNameText);
					verify_noerr(EmbedControl(parameterNameText));
				}
			}
		}
		
			// lets get out of here...
		return;
	}
	
	if (	(paramInfo.flags & kTEMP_AudioUnitParameterFlag_MeterReadOnly)			// METER PARAMS
			&& !(paramInfo.flags & kAudioUnitParameterFlag_IsWritable)	
			&& (paramInfo.flags & kAudioUnitParameterFlag_IsReadable) )
	{
		r.top = y;		r.bottom = y + kTextHeight;
		r.left = x;		r.right = r.left + kParamNameWidth;
		fontStyle.font = kControlFontSmallSystemFont;
		fontStyle.just = teFlushRight;
		r.top += 1;
		r.bottom += 1;
		
		DrawParameterTitleInRect(auvp.GetName(), fontStyle, r);
				
		if (mMeters.empty())
			CreateEventLoopTimer (0.005, 0.035);
		
		r.left = r.right + 8;
		r.right = r.left + kReadOnlyMeterWidth;
		Point labelSize;
		labelSize.v = kTextHeight - 3;
		labelSize.h = kMinMaxWidth; // + 8;

		r.bottom -= 3; // take the tail adjustement out
		mMeters.push_back (
			new MeterView (this, auvp, r, labelSize, kParamTagWidth, fontStyle));
		
		y -= 3;
	}
	else if (	!(paramInfo.flags & kAudioUnitParameterFlag_IsWritable)			//READ ONLY PARAMS
				&& (paramInfo.flags & kAudioUnitParameterFlag_IsReadable))
	{
		fontStyle.font = kControlFontSmallSystemFont;
				
		r.top = y;		r.bottom = y + kTextHeight; // for tails of letters
		r.left = x;		r.right = r.left + kParamNameWidth;
		fontStyle.just = teFlushRight;
				
		r.top += 1;
		r.bottom += 1;
		DrawParameterTitleInRect(auvp.GetName(), fontStyle, r);
		
		CFStringRef value = auvp.GetStringFromValueCopy();

		CFMutableStringRef str = CFStringCreateMutableCopy(NULL, 256, value);
		
		if (auvp.GetParamTag()) {
			CFStringAppend (str, CFSTR(" "));
			CFStringAppend (str, auvp.GetParamTag());
		}
		
		r.left = x + kParamNameWidth + kMinMaxWidth + 10; // aligns with read/write check box
		r.right = r.left + kSliderWidth;

		fontStyle.just = teFlushLeft;
		fontStyle.font = kControlFontSmallBoldSystemFont;

		if (noErr == CreateStaticTextControl(mCarbonWindow, &r, str, &fontStyle, &newControl)) {
			verify_noerr(EmbedControl(newControl));
			AddCarbonControl(AUCarbonViewControl::kTypeText, auvp, newControl);
		}
				
		CFRelease (str);
		CFRelease (value);
	}
	else if (auvp.HasNamedParams() && (paramInfo.unit == kAudioUnitParameterUnit_Indexed)) // NAMED PARAMETER (POPUP)
	{
		fontStyle.font = kControlFontSmallSystemFont;
		fontStyle.just = teFlushRight;
		
		r.top = y + 2;		r.bottom = r.top + kTextHeight;
		r.left = x;			r.right = r.left + kParamNameWidth;
		
		DrawParameterTitleInRect(auvp.GetName(), fontStyle, r);
		
		static int topOffset = -100;
		if (topOffset == -100) {
			SInt32 sysVers;
			Gestalt(gestaltSystemVersion, &sysVers);
			// 1030 for Panther & later
			SInt32 minorVers = sysVers & 0xFF;
			SInt32 majorVers = (sysVers & 0xFF00) >> 8;
			topOffset = 2;
			if (majorVers == 0x10 && minorVers < 0x30) {
				topOffset = 4;
			}
		}
		
		r.top -= topOffset; // Jaguar 4 Panther 2
		r.left = r.right + 8;	r.right = r.left + kSliderWidth + kPopupAdjustment;
		r.bottom += 5;
		fontStyle.font = kControlFontSmallSystemFont;
			
		AUControlGroup::CreatePopupMenu (this, auvp, r, fontStyle, false);
		if (outSize->right - outSize->left < r.right - r.left)
			outSize->right = outSize->left + (r.right - r.left);
		
		y += topOffset + 1;
	} 
	else 																// ALL OTHERS (Slider control)
	{
		fontStyle.font = kControlFontSmallSystemFont;
		
		r.top = y;		r.bottom = y + kTextHeight; // for tails of letters
		r.left = x;		r.right = r.left + kParamNameWidth;
		fontStyle.just = teFlushRight;
		
		int theWidth = kParamNameWidth;
		
		r.top += 1;
		r.bottom += 1;
		DrawParameterTitleInRect(auvp.GetName(), fontStyle, r);		
		
		r.left = r.right + 8;	r.right = r.left + kSliderWidth;
		theWidth += 8 + kSliderWidth;
		
		Point labelSize, textSize;
		labelSize.v = textSize.v = kTextHeight - 1;
		labelSize.h = kMinMaxWidth;
		textSize.h = kEditTextWidth;
				
		AUControlGroup::CreateLabelledSliderAndEditText(this, auvp, r, labelSize, textSize, fontStyle);					
		if (auvp.GetParamTag())
		{
			r.left = r.right + 8;
			r.right = r.left + kParamTagWidth;
			fontStyle.just = 0;
			
            // try to localize param tag
            CFStringRef tagString = auvp.GetParamTag();
            
            CFBundleRef bundle = CFBundleGetBundleWithIdentifier( kLocalizedStringBundle_AUView );
            if (bundle != NULL) {
				CFRetain(bundle);
                // the tag (CFString) returned from CAAUParameter is the key for our localized strings table
                tagString = CFCopyLocalizedStringFromTableInBundle(tagString, kLocalizedStringTable_AUView, bundle, tagString);
                CFRelease(bundle);
            }
            
			if (noErr == CreateStaticTextControl(mCarbonWindow, &r, tagString, &fontStyle, &newControl)) {
				Boolean multiline = false;
				if (SetControlData(newControl, kControlEntireControl, kControlStaticTextIsMultilineTag, sizeof(Boolean), &multiline) == noErr) {
					TruncCode truncType = truncEnd;
					SetControlData(newControl, kControlEntireControl, kControlStaticTextTruncTag, sizeof(TruncCode), &truncType);
				}
				verify_noerr(EmbedControl(newControl));
			}
			theWidth += 8 + kParamTagWidth;
		}
		if (outSize->right - outSize->left < theWidth) {
			outSize->right = outSize->left + theWidth;
        }
	}
}

OSStatus	GenericAUView::CreateUIForStandardProperties (Float32 inXOffset, Float32 inYOffset, Rect *outSize)
{
	OSStatus err;
	UInt32 propertySize;
    
	const int x = short(inXOffset);
	const int y = short(inYOffset);
	
	outSize->top = y;
	outSize->left= x;
	outSize->right= x;
	outSize->bottom = y;
	
	Point location;
	location.h = x;
	location.v = y + kHeadingBodyDist;
	
	Boolean hasProperty [numProperties];
	int 	propertyCount = 0;
	
	// [A] **** CHECK FOR PROPERTIES ****
	
	//    ** Bypass Effect Property ** [REMOVED IN MAC OS X v10.4]
	
	//    [1] ** Factory Presets Property **
	CFArrayRef presets = NULL;
	propertySize = sizeof(presets);
	err = AudioUnitGetProperty (GetEditAudioUnit(), 
								kAudioUnitProperty_FactoryPresets,
								kAudioUnitScope_Global, 
								0, 
								&presets, 
								&propertySize);
	hasProperty[0] = (!err && presets) ? true : false;
	propertyCount += hasProperty[0] ? 1 : 0;
	
	//    [2] ** Disk Streaming Property **
	UInt32 isDiskStreaming;
	UInt32 size = sizeof(UInt32);
	err = AudioUnitGetProperty (GetEditAudioUnit(), 
								kMusicDeviceProperty_StreamFromDisk, 
								kAudioUnitScope_Global, 
								0, 
								&isDiskStreaming, 
								&size);
	
	hasProperty[1] = (!err) ? true : false;
	propertyCount += hasProperty[1] ? 1 : 0;
	
	//    [3] ** CPU Load Property **
	Float32 theCPULoad;
	size = sizeof(Float32);
	err = AudioUnitGetProperty (GetEditAudioUnit(), 
								kAudioUnitProperty_CPULoad, 
								kAudioUnitScope_Global, 
								0, 
								&theCPULoad, 
								&size);
	hasProperty[2] = (!err) ? true : false;
	propertyCount += hasProperty[2] ? 1 : 0;
	
	//    [4] ** Render Quality Property **
	UInt32 theRenderQuality;
	size = sizeof(UInt32);
	err = AudioUnitGetProperty (GetEditAudioUnit(), 
								kAudioUnitProperty_RenderQuality, 
								kAudioUnitScope_Global, 
								0, 
								&theRenderQuality, 
								&size);
								
	hasProperty[3] = (!err) ? true : false;
	propertyCount += hasProperty[3] ? 1 : 0;
	
	// [B] **** LAYOUT PROPERTIES ****
	
	//    [1] ** Properties title/header **
	if (propertyCount > 0) {
		Rect r = {location.v, location.h, location.v + kTextHeight, location.h + 62};
		ControlRef ref = GetSizedStaticTextControl (kPropertiesLabelString, &r, true);
		if (ref) {
			EmbedControl(ref);
			Rect bestRect;
			GetControlBounds(ref, &bestRect);
			r.left = bestRect.right + 2;
			r.right = location.h + 465;
			r.bottom = bestRect.bottom;
			int textHeight = (bestRect.bottom - bestRect.top) + 1;
			location.v += textHeight + GV_VERTICAL_SPACING;
			outSize->bottom += textHeight + GV_VERTICAL_SPACING;
			if (noErr == CreateSeparatorControl (GetCarbonWindow(), &r, &ref))
				EmbedControl(ref);
		}
	}
	
	// indent for properties
	location.h += GV_PROPERTY_INDENTATION;
	
	ControlFontStyleRec fontStyle;
	fontStyle.flags = kControlUseFontMask | kControlUseJustMask;
	fontStyle.font = kControlFontSmallSystemFont;
	fontStyle.just = teFlushLeft;
	
	int additionalHeight = 0;
	
	//    ** Bypass Effect Checkbox **
	//    ** REMOVED IN Mac OS X v10.4 **/
	
	//    [1] ** Factory Presets PopUp menu **
	if (hasProperty[0]) {
		location.h += 4;
		mFactoryPresets = new AUVPresets (this, presets, location, kPropertyWidth, kControlWidgetWidth + kPropertyPopupAdjustment, fontStyle);
		additionalHeight = mFactoryPresets->GetHeight() + GV_VERTICAL_SPACING;
		location.v += additionalHeight;
		outSize->bottom += additionalHeight;
		mFactoryPresets->AddInterest (mEventListener, this);
		location.h -= 4;
	}
	
	//    [3] ** Disk Streaming Checkbox **
	if (hasProperty[1]) {
		mStreamingCheckbox = new AUDiskStreamingCheckbox (this, location, fontStyle);
		additionalHeight = mStreamingCheckbox->GetHeight() + GV_VERTICAL_SPACING;
		location.v += additionalHeight;
		outSize->bottom += additionalHeight;
		mStreamingCheckbox->AddInterest (mEventListener, this);
	}
	
	//    [4] ** CPU Load UI **
	if (hasProperty[2]) {
		mLoadCPU = new AULoadCPU (this, location, fontStyle);
		additionalHeight = mLoadCPU->GetHeight() + GV_VERTICAL_SPACING;
        location.v += additionalHeight;
		outSize->bottom += additionalHeight;
		mLoadCPU->AddInterest (mEventListener, this);
	}
	
	//    [5] ** Render Quality PopUp menu **
	if (hasProperty[3]) {
		mQualityPopup = new AURenderQualityPopup (this, location, fontStyle);
		additionalHeight = mQualityPopup->GetHeight() + GV_VERTICAL_SPACING;
		location.v += additionalHeight;
		outSize->bottom += additionalHeight;
		mQualityPopup->AddInterest (mEventListener, this);
	}
	
	return noErr;
}

void		GenericAUView::AddListenerEvents()
{
	AudioUnitEvent e;
	e.mEventType = kAudioUnitEvent_PropertyChange;
	e.mArgument.mProperty.mAudioUnit = mEditAudioUnit;
	e.mArgument.mProperty.mPropertyID = kAudioUnitProperty_ParameterList;
	e.mArgument.mProperty.mScope = kAudioUnitScope_Global;
	e.mArgument.mProperty.mElement = 0;
	AUEventListenerAddEventType (mEventListener, this, &e);
	
	e.mArgument.mProperty.mPropertyID = kAudioUnitProperty_ParameterInfo;
	AUEventListenerAddEventType (mEventListener, this, &e);
}

// The responsibility of the subclass is to make sure outSize
// is set to the size and location of the custom UI added.
OSStatus	GenericAUView::CreateSpecificUI (Float32 inXOffset, Float32 inYOffset, Rect *outSize) 
{
	outSize->left	= short(inXOffset);
	outSize->right	= short(inXOffset);
	outSize->top	= short(inYOffset);
	outSize->bottom	= short(inYOffset);
	return noErr;
}

OSStatus	GenericAUView::CreateUI(Float32	inXOffset, Float32 inYOffset)
{
	if (mEventListener == NULL) {
		AUEventListenerCreate(AUEventChange, this, CFRunLoopGetCurrent(), kCFRunLoopCommonModes, 0.05, 0.05, &mEventListener);
		AddListenerEvents();
	}

	Point location;
	location.h = short(inXOffset) + GV_GUTTER_MARGIN;
	location.v = short(inYOffset) + GV_GUTTER_MARGIN;
	
	static const SInt16 kRightStartOffset = kParamNameWidth + kEditTextWidth + kParamTagWidth;
	static const SInt16 kTotalWidth = kRightStartOffset + kSliderWidth;
		
	// Display the Name of the unit
	AUControlGroup::AddAUInfo (this, location, kRightStartOffset, kTotalWidth);
	
	location.v += 12;

	Rect theRect;
	ComponentResult result = CreateSpecificUI ((Float32)location.h, (Float32) location.v, &theRect);
		if (result != noErr) return result;
	
	result = CreateUIForStandardProperties ((Float32)theRect.left, (Float32)theRect.bottom, &theRect);
		if (result != noErr) return result;
	
	return CreateUIForParameters ((Float32)location.h, (Float32)theRect.bottom, &theRect);
}

void GenericAUView::RebuildUI() 
{
	Cleanup();
	
	mBottomRight.h = mBottomRight.v = 0;
	SizeControl(mCarbonPane, 0, 0);
		
		// now that all controls have been removed, we need to re-add controls
	CreateUI(mXOffset, mYOffset);
	
		// we should only resize the control if a subclass has embedded
		// controls in this AND this is done with the EmbedControl call below
		// if mBottomRight is STILL equal to zero, then that wasn't done
		// so don't size the control
	Rect paneBounds;
	GetControlBounds(mCarbonPane, &paneBounds);
	if (mBottomRight.h != 0 && mBottomRight.v != 0)
		SizeControl(mCarbonPane, (short) (mBottomRight.h - mXOffset), (short) (mBottomRight.v - mYOffset));
}

void	GenericAUView::HandlePropertyChange (const AudioUnitProperty  & inProp)
{
	if (inProp.mPropertyID == kAudioUnitProperty_ParameterList 
		|| inProp.mPropertyID == kAudioUnitProperty_ParameterInfo)
	{
		RebuildUI();
	}
	else
	{
		if (mFactoryPresets) 
			if (mFactoryPresets->HandlePropertyChange (inProp)) return;

		if (mLoadCPU) 
			if (mLoadCPU->HandlePropertyChange (inProp)) return;

		if (mQualityPopup) 
			if (mQualityPopup->HandlePropertyChange (inProp)) return;

		if (mStreamingCheckbox) 
			if (mStreamingCheckbox->HandlePropertyChange (inProp)) return;
	}
}

void	GenericAUView::AUEventChange(		void *						inCallbackRefCon,
											void *						inObject,
											const AudioUnitEvent *		inEvent,
											UInt64						inEventHostTime,
											Float32						inParameterValue)
{
	GenericAUView *This = static_cast<GenericAUView *>(inCallbackRefCon);
	if (inEvent->mEventType == kAudioUnitEvent_PropertyChange) {
		This->HandlePropertyChange (inEvent->mArgument.mProperty);
	} else if (inEvent->mEventType == kAudioUnitEvent_ParameterValueChange) {
		This->HandleParameterChange (inEvent->mArgument.mParameter, inEventHostTime, inParameterValue);
	}
}

ControlRef  GenericAUView::GetSizedStaticTextControl (CFStringRef inString, Rect *inRect, Boolean inIsBold)
{
	ControlFontStyleRec fontStyle;
	fontStyle.flags = kControlUseFontMask | kControlUseJustMask;
	fontStyle.font = inIsBold ? kControlFontSmallBoldSystemFont : kControlFontSmallSystemFont;
	fontStyle.just = teFlushLeft;
	
	ControlRef ref = NULL;
	
	OSErr theErr =  CreateStaticTextControl (GetCarbonWindow(), inRect, inString, &fontStyle, &ref);
	if (theErr != noErr) return NULL;
	
	Boolean bValue = false;
	theErr = SetControlData(ref, kControlEntireControl, 'stim' /* kControlStaticTextIsMultilineTag */, sizeof(Boolean), &bValue);
	if (theErr != noErr) return NULL;
	
	Rect bestRect = {0, 0, 0, 0};
	SInt16 baseLineOffset = 0;
	GetBestControlRect(ref, &bestRect, &baseLineOffset);
	SizeControl(ref, bestRect.right - bestRect.left + 1, bestRect.bottom - bestRect.top + 1);
	
	return ref;
}

void 	GenericAUView::RespondToEventTimer (EventLoopTimerRef inTimer)
{
	CGrafPtr gp = GetWindowPort (GetCarbonWindow());

	CGrafPtr save;
	GetPort(&save);
	SetPort(gp );

	for (MeterList::iterator iter = mMeters.begin(); iter < mMeters.end(); ++iter)
	{
		(*iter)->Draw();
	}
	
	SetPort(save );
}

MeterView::MeterView (GenericAUView 			* auView,
					const CAAUParameter			& auvp, 
					Rect 						& area, 
					Point 						& labelSize,
					int							paramTagWidth,
					ControlFontStyleRec 		& fontStyle)
	: mParam (auvp)
{
    mAUView = auView;
    
	mMinValue = auvp.ParamInfo().minValue;
	Float32 maxValue = auvp.ParamInfo().maxValue;
	
	Rect minValRect, maxValRect, paramTagRect;
	minValRect = mRect = maxValRect = paramTagRect = area;
	
	minValRect.left = area.left;
	minValRect.right = area.left + kMinMaxWidth;
	
	mRect.top += 3;
	mRect.left = minValRect.right + 4;
	mRect.right = mRect.left + (kSliderWidth - kParamNameWidth) + 8 + kMinMaxWidth;
	
	maxValRect.left = mRect.right + 8;
	maxValRect.right = maxValRect.left + kEditTextWidth;
	
	paramTagRect.left = maxValRect.right + 8;
	paramTagRect.right = paramTagRect.left + kParamTagWidth;
	
		//min value label
	CFStringRef cfstr = auvp.GetStringFromValueCopy(&mMinValue);
	fontStyle.just = teFlushRight;
	ControlRef newControl;
	verify_noerr(CreateStaticTextControl (auView->GetCarbonWindow(), &minValRect, cfstr, &fontStyle, &newControl));
	CFRelease(cfstr);
	verify_noerr(auView->EmbedControl(newControl));
	
		//max value label
	cfstr = auvp.GetStringFromValueCopy(&maxValue);
	fontStyle.just = teFlushLeft;
	verify_noerr(CreateStaticTextControl(auView->GetCarbonWindow(), &maxValRect, cfstr, &fontStyle, &newControl));
	CFRelease(cfstr);
	verify_noerr(auView->EmbedControl(newControl));
	
		// unit label
	CFStringRef tagString = auvp.GetParamTag();
	
	if (tagString) {
		// try to localize param tag
		CFBundleRef bundle = CFBundleGetBundleWithIdentifier( kLocalizedStringBundle_AUView );
		if (bundle != NULL) {
			CFRetain(bundle);
			// the tag (CFString) returned from CAAUParameter is the key for our localized strings table
			tagString = CFCopyLocalizedStringFromTableInBundle(tagString, kLocalizedStringTable_AUView, bundle, tagString);
			CFRelease(bundle);
		}
		
		fontStyle.just = teFlushLeft;
		if (noErr == CreateStaticTextControl(auView->GetCarbonWindow(), &paramTagRect, tagString, &fontStyle, &newControl))
			verify_noerr(auView->EmbedControl(newControl));
	}
	
	mValueRange = (maxValue - mMinValue);
	mLastValueDrawn = 0xFFFFFFFF;
    mLastActiveState = false;
    
    WindowAttributes attr;
    verify_noerr(GetWindowAttributes(mAUView->GetCarbonWindow(), &attr));
    // set up colors differently for metal/non-metal.
    if (attr & kWindowMetalAttribute) {
        mForegroundActiveColor.red =		64 * 256;
        mForegroundActiveColor.green =		128 * 256;
        mForegroundActiveColor.blue =		192 * 256;
        
        mBackgroundActiveColor.red =		96 * 256;
		mBackgroundActiveColor.green =		96 * 256;
		mBackgroundActiveColor.blue =		96 * 256;
    } else {
        mForegroundActiveColor.red =		64 * 256;
        mForegroundActiveColor.green =		128 * 256;
        mForegroundActiveColor.blue =		192 * 256;
        
        mBackgroundActiveColor.red =		192 * 256;
		mBackgroundActiveColor.green =		192 * 256;
		mBackgroundActiveColor.blue =		192 * 256;
    }
    
    UInt32 windowBackgroundShade = 255 * 256;
    
    // create inactive colors from active colors mathematically:
    UInt32 backgroundInactiveBase = (mBackgroundActiveColor.red + mBackgroundActiveColor.green + mBackgroundActiveColor.blue) / 3;
    UInt32 backgroundInactive = backgroundInactiveBase + UInt32((windowBackgroundShade - backgroundInactiveBase) * 0.5f);
    mBackgroundInactiveColor.red = mBackgroundInactiveColor.green = mBackgroundInactiveColor.blue = backgroundInactive;
    
    UInt32 foregroundInactiveBase = (mForegroundActiveColor.red + mForegroundActiveColor.green + mForegroundActiveColor.blue) / 3;
    UInt32 foregroundInactive = foregroundInactiveBase + UInt32((backgroundInactive - foregroundInactiveBase) * 0.5f);
    mForegroundInactiveColor.red = mForegroundInactiveColor.green = mForegroundInactiveColor.blue = foregroundInactive;
}

void MeterView::Draw ()
{
	Float32 value = AUParameterValueToLinear(mParam.GetValue (), &mParam);
	if (value < 0)
		value = 0;
	if (value > 1)
		value = 1;
		
	UInt32 width = mRect.right - mRect.left;
	UInt32 xOffset = UInt32(width * value);
	
    Boolean isActive = IsWindowActive(mAUView->GetCarbonWindow());
    
    // don't redraw for some cases
	if ((xOffset == mLastValueDrawn) && (isActive == mLastActiveState))
		return;
    
	mLastValueDrawn = xOffset;
	mLastActiveState = isActive;
    
    UInt32 xPanelOffset = 0;
    UInt32 yPanelOffset = 0;
    
    // with compositing on, we're drawing relative to the window,
    // with it off, we're drawing relative to the control.
    if (mAUView->IsCompositWindow()) {
        Rect paneBounds;
        GetControlBounds(mAUView->GetCarbonPane(), &paneBounds);
        xPanelOffset = paneBounds.left;
        yPanelOffset = paneBounds.top;
    }
    
	Rect valueRect = {	mRect.top + yPanelOffset, mRect.left + xPanelOffset,
                        mRect.bottom + yPanelOffset, mRect.left + xOffset + xPanelOffset	};
    
	if (value > 0) {
        if (isActive) {
            RGBForeColor (&mForegroundActiveColor);
        } else {
            RGBForeColor (&mForegroundInactiveColor);
        }
		PaintRect (&valueRect);
	}
	
	if (value < 1) {
		valueRect.left = mRect.left + xOffset + xPanelOffset;
		valueRect.right = mRect.right + xPanelOffset;
		
        if (isActive) {
            RGBForeColor (&mBackgroundActiveColor);
        } else {
            RGBForeColor (&mBackgroundInactiveColor);
        }
		PaintRect (&valueRect);
	}
}
