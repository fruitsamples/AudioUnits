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
	GenericAUView.cpp
	
=============================================================================*/

#include "GenericAUView.h"
#include <map>
#include <vector>

#define DISABLE_BYPASS

static bool sLocalized = false;
static CFStringRef kParametersLabelString = CFSTR("Parameters");
static CFStringRef kPropertiesLabelString = CFSTR("Properties");

class MeterView {
public:
		MeterView (AUCarbonViewBase 			* auView,
					AUVParameter 				& auvp, 
					Rect 						& area, 
					Point 						& labelSize,
					int							paramTagWidth,
					ControlFontStyleRec 		& fontStyle);
	
	void Draw ();

private:
	Float32 				mMinValue, mValueRange;
	UInt32					mLastValueDrawn;
	AudioUnit 				mUnit;
	AudioUnitParameterID 	mParam;
	Rect 					mRect;
};

static const int kHeadingBodyDist = 24;
static const int kParamNameWidth = 140;
static const int kSliderWidth = 240;
static const int kEditTextWidth = 40;
static const int kParamTagWidth = 30;
static const int kMinMaxWidth = 40;
static const int kPopupAdjustment = -13;
static const int kReadOnlyMeterWidth = 286;

static const int kClumpSeparator = 8;
static const int kRowHeight = 20;
static const int kTextHeight = 14;


/*** FUNCTION PROTOTYPES ***/
void GenericViewPropertyListener(void * inRefCon, AudioUnit ci, AudioUnitPropertyID  inID, AudioUnitScope inScope, AudioUnitElement inElement);


COMPONENT_ENTRY(GenericAUView)

GenericAUView::GenericAUView(AudioUnitCarbonView auv) 
		: AUCarbonViewBase(auv), mFactoryPresets(0), mBypassEffect(0), mLoadCPU(0), mQualityPopup(0), mStreamingCheckbox(0)  
{
	if (!sLocalized) {		
		// Because we are in a component, we need to load our bundle by identifier so we can access our localized strings
		// It is important that the string passed here exactly matches that in the Info.plist Identifier string
		CFBundleRef bundle = CFBundleGetBundleWithIdentifier( CFSTR("com.apple.audio.units.Components") );
		if (bundle != NULL) {
			kParametersLabelString = CFCopyLocalizedStringFromTableInBundle(kParametersLabelString, CFSTR("CustomUI"), bundle, CFSTR(""));	
			kPropertiesLabelString = CFCopyLocalizedStringFromTableInBundle(kPropertiesLabelString, CFSTR("CustomUI"), bundle, CFSTR(""));	
	
			sLocalized = true;
		}
	}

}

GenericAUView::~GenericAUView()
{
	Cleanup();	
}

void 		GenericAUView::Cleanup()
{
	AudioUnitRemovePropertyListener(mEditAudioUnit, kAudioUnitProperty_ParameterList, GenericViewPropertyListener);
	AudioUnitRemovePropertyListener(mEditAudioUnit, kAudioUnitProperty_ParameterInfo, GenericViewPropertyListener);

	if (mFactoryPresets) {
		delete mFactoryPresets;
		mFactoryPresets = NULL;
	}
	if (mBypassEffect) {
		delete mBypassEffect;
		mBypassEffect = NULL;
	}
	if (mLoadCPU) {
		delete mLoadCPU;
		mLoadCPU = NULL;
	}
	if (mQualityPopup) {
		delete mQualityPopup;
		mQualityPopup = NULL;
	}
	if (mStreamingCheckbox) {
		delete mStreamingCheckbox;
		mStreamingCheckbox = NULL;
	}
	for (MeterList::iterator iter = mMeters.begin(); iter < mMeters.end(); ++iter)
		delete (*iter);
	mMeters.clear();
}

OSStatus	GenericAUView::CreateUIForParameters (Float32 inXOffset, Float32 inYOffset, Rect *outSize) {
	// for each parameter in the global scope, create:
	//		label	horiozontal slider	text entry	label
	// descending vertically
	// inside mCarbonWindow, embedded in mCarbonPane
	
	OSStatus err;
	UInt32 propertySize;
	
	outSize->top = (short) inYOffset;
	outSize->left= (short) inXOffset;
	outSize->bottom = outSize->top;
	outSize->right  = outSize->left;
	
	err = AudioUnitGetPropertyInfo(mEditAudioUnit, kAudioUnitProperty_ParameterList,
		kAudioUnitScope_Global, 0, &propertySize, NULL);
	if (err) return err;
	
	int nparams = propertySize / sizeof(AudioUnitPropertyID);
	AudioUnitParameterID *paramIDs = new AudioUnitParameterID[nparams];

	err = AudioUnitGetProperty(mEditAudioUnit, kAudioUnitProperty_ParameterList,
		kAudioUnitScope_Global, 0, paramIDs, &propertySize);
	
	typedef std::vector <AUVParameter> SortedParamList;
	typedef std::map <UInt32, SortedParamList, std::less<UInt32> > ParameterMap;
	
	ParameterMap params;
	if (!err) {
		for (int i = 0; i < nparams; ++i) {
			AUVParameter auvp(mEditAudioUnit, paramIDs[i], kAudioUnitScope_Global, 0);
			const AudioUnitParameterInfo &paramInfo = auvp.ParamInfo();
			
			if (paramInfo.flags & kAudioUnitParameterFlag_ExpertMode)
				continue;
			
			if (!(paramInfo.flags & kAudioUnitParameterFlag_IsWritable)
					&& !(paramInfo.flags & kAudioUnitParameterFlag_IsReadable))
				continue;
			
			// ok - if we're here, then we have a parameter we are going to display.
			UInt32 clump = 0;
			auvp.GetClumpID (clump);
			params[clump].push_back (auvp);
		}
	}
	
	delete[] paramIDs;

	if (!err) {
		
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
						
		if (nparams >= 2) {
			// Create the header for the parameters
			Rect r = {location.v, location.h, location.v + kRowHeight, location.h + 66};
			
			ControlRef ref;
			fontStyle.font = kControlFontSmallBoldSystemFont;
	
			OSErr theErr =  CreateStaticTextControl (GetCarbonWindow(), &r, kParametersLabelString, &fontStyle, &ref);
			if (theErr == noErr) {
				EmbedControl(ref);
				r.left = r.right + 2;
				r.right = location.h + 440;
				r.bottom -= 5;

				if (noErr == CreateSeparatorControl (GetCarbonWindow(), &r, &ref))
				EmbedControl(ref);
				
				y += kRowHeight;
				location.v += kRowHeight;
				outSize->bottom += kRowHeight;
				
				// we can use the ref to get the mCarbonPane
				ControlRef carbonPane;
				theErr = GetSuperControl (ref, &carbonPane);
				if (theErr == noErr)
					mParentViewContainer = carbonPane;
			}
			
			fontStyle.font = kControlFontSmallSystemFont;
		}
		
		ParameterMap::iterator i = params.begin();
		while (1) {
			for (SortedParamList::iterator piter = (*i).second.begin(); piter != (*i).second.end(); ++piter)
			{
				DrawParameter (*piter, x, y, fontStyle, outSize);
				y += kRowHeight;
				outSize->bottom += kRowHeight;
			}
			if (++i != params.end()) {
				y += kClumpSeparator;
				outSize->bottom += kClumpSeparator;
			} else {
				// now we want to add 16 pixels to the bottom and the right
				mBottomRight.h += 16;
				mBottomRight.v += 16;
				break;
			}
		}
	}
	return noErr;
}

void 	GenericAUView::DrawParameter (AUVParameter &auvp, const int x, int &y, ControlFontStyleRec &fontStyle, Rect *outSize)
{
	ControlRef newControl;					
	Rect r;

	const AudioUnitParameterInfo &paramInfo = auvp.ParamInfo();
			
	if (!(paramInfo.flags & kAudioUnitParameterFlag_IsWritable)			// METERS
		&& (paramInfo.flags & kAudioUnitParameterFlag_IsReadable))
	{
		r.top = y;		r.bottom = y + kTextHeight;
		r.left = x;		r.right = r.left + kParamNameWidth;
		fontStyle.font = kControlFontSmallSystemFont;
		fontStyle.just = teFlushRight;

		CFMutableStringRef str = CFStringCreateMutableCopy(NULL, 
										CFStringGetLength(auvp.GetName())+1, 
										auvp.GetName());
		CFStringCapitalize (str, NULL);
		CFStringAppend (str, CFSTR(":"));
		
		r.top += 1;
		r.bottom += 1;
		if (noErr == CreateStaticTextControl(mCarbonWindow, 
											&r,
											str, 
											&fontStyle, 
											&newControl)) 
			verify_noerr(EmbedControl(newControl));
		
		CFRelease (str);
		
		if (mMeters.empty())
			CreateEventLoopTimer (0.005, 0.035);
		
		r.left = r.right;	r.right = r.left + kReadOnlyMeterWidth;
		Point labelSize;
		labelSize.v = kTextHeight - 3;
		labelSize.h = kMinMaxWidth + 8;

		r.bottom -= 3; //take the tail adjustement out
		mMeters.push_back (
			new MeterView (this, auvp, r, labelSize, kParamTagWidth, fontStyle));
		
		y -= 3;
	}
	else if (auvp.HasNamedParams()) 									// NAMED PARAMETER (POPUP)
	{
		fontStyle.font = kControlFontSmallSystemFont;
		
		r.top = y + 2;		r.bottom = r.top + kTextHeight;
		r.left = x;		r.right = r.left + kParamNameWidth;
		fontStyle.just = teFlushRight;
		
		CFMutableStringRef str = CFStringCreateMutableCopy(NULL, CFStringGetLength(auvp.GetName())+1, auvp.GetName());
		CFStringCapitalize (str, NULL);
		CFStringAppend (str, CFSTR(":"));
		
		if (noErr == CreateStaticTextControl(mCarbonWindow, 
											&r,
											str, 
											&fontStyle, 
											&newControl)) 
			verify_noerr(EmbedControl(newControl));
		
		CFRelease (str);
		
		static int topOffset = -100;
		static int rowOffset = -100;
		if (topOffset == -100) {
			SInt32 sysVers;
			Gestalt(gestaltSystemVersion, &sysVers);
			// 1030 for Panther
			SInt32 minorVers = sysVers & 0xFF;
			SInt32 majorVers = (sysVers & 0xFF00) >> 8;
			topOffset = 2;
			rowOffset = 5;
			if (majorVers == 0x10 && minorVers < 0x30) {
				topOffset = 4;
				rowOffset = 7;
			}
		}
		
		r.top -= topOffset; // Jaguar 4 Panther 2
		r.left = r.right + 8;	r.right = r.left + kSliderWidth + kPopupAdjustment;
		r.bottom += 5;
		fontStyle.font = kControlFontSmallSystemFont;
		
		AUControlGroup::CreatePopupMenu (this, auvp, r, fontStyle);
		if (outSize->right - outSize->left < r.right - r.left)
			outSize->right = outSize->left + (r.right - r.left);
		
		y += rowOffset; // Jaguar 7 - Panther 5
		
	} 
	else if (auvp.ParamInfo().unit == kAudioUnitParameterUnit_Boolean) 	// BOOLEAN (Checkbox)
	{
		fontStyle.font = kControlFontSmallSystemFont;
					
		Rect r;
		r.top = y;								r.bottom = r.top + kTextHeight; // tail allowance
		r.left = x + kParamNameWidth + kMinMaxWidth + 10;		r.right = r.left + 100;
		
		r.top -= 1;
		r.bottom -= 2;
		
		CFMutableStringRef str = CFStringCreateMutableCopy(NULL, CFStringGetLength(auvp.GetName()), auvp.GetName());
		CFStringCapitalize (str, NULL);

		if (noErr == CreateCheckBoxControl(GetCarbonWindow(), 
											&r, 
											str, 
											0, 
											true, 
											&newControl)) 
		{
			verify_noerr (SetControlFontStyle (newControl, &fontStyle));
			SInt16 baseLineOffset;
			verify_noerr (GetBestControlRect (newControl, &r, &baseLineOffset));
			r.bottom += baseLineOffset;
			SetControlBounds (newControl, &r);
			EmbedControl (newControl);
		
			AddCarbonControl(AUCarbonViewControl::kTypeDiscrete, auvp, newControl);
		}
		CFRelease (str);
		
		if (outSize->right - outSize->left < r.right - r.left)
			outSize->right = outSize->left + (r.right - r.left);
		
		y -= 4;
	}
	else 																// ALL OTHERS (Slider control)
	{
		fontStyle.font = kControlFontSmallSystemFont;
		
		r.top = y;		r.bottom = y + kTextHeight; // for tails of letters
		r.left = x;		r.right = r.left + kParamNameWidth;
		fontStyle.just = teFlushRight;
		
		int theWidth = r.right - r.left;
		
		r.top += 1;
		r.bottom += 1;
		CFMutableStringRef str = CFStringCreateMutableCopy(NULL, CFStringGetLength(auvp.GetName())+1, auvp.GetName());
		CFStringCapitalize (str, NULL);
		CFStringAppend (str, CFSTR(":"));
		
		if (noErr == CreateStaticTextControl(mCarbonWindow, &r, str, &fontStyle, &newControl))
			verify_noerr(EmbedControl(newControl));
			
		CFRelease (str);
		
		r.left = r.right + 8;	r.right = r.left + kSliderWidth;
		theWidth += 8 + kSliderWidth;
		r.bottom -= 2; //take the tail adjustement out
		
		Point labelSize, textSize;
		labelSize.v = textSize.v = kTextHeight - 3;
		labelSize.h = kMinMaxWidth;
		textSize.h = kEditTextWidth;
						
		AUControlGroup::CreateLabelledSliderAndEditText(this, auvp, r, labelSize, textSize, fontStyle);					
		if (auvp.GetParamTag()) {
			r.left = r.right + 8; 	r.right = r.left + kParamTagWidth;
			fontStyle.just = 0;
			
			if (noErr == CreateStaticTextControl(mCarbonWindow, &r, auvp.GetParamTag(), &fontStyle, &newControl))
				verify_noerr(EmbedControl(newControl));
			theWidth += 8 + kParamTagWidth;
		}
		if (outSize->right - outSize->left < theWidth)
			outSize->right = outSize->left + theWidth;
	}
}

OSStatus	GenericAUView::CreateUIForStandardProperties (Float32 inXOffset, Float32 inYOffset, Rect *outSize)
{
	OSStatus err;
	UInt32 propertySize;
	
	static const int kRowHeight = 24;
	static const int kHeadingBodyDist = 24;
	static const int kParamNameWidth = 140;
	static const int kSliderWidth = 240;
	static const int kPopupAdjustment = -13;
	static const int numProperties = 5;
	
	int y = short(inYOffset);
	const int x = short(inXOffset);
	
	outSize->top = y;
	outSize->left= x;
	outSize->right= x;
	outSize->bottom = y;
	
	Point location;
	location.h = x;
	location.v = y;
			
	location.v = (y += kHeadingBodyDist);

	ControlFontStyleRec fontStyle;
	fontStyle.flags = kControlUseFontMask | kControlUseJustMask;
	fontStyle.font = kControlFontSmallSystemFont;
	
	Boolean hasProperty [numProperties];
	int 	propertyCount = 0;
	
	/********* BYPASS PROPERTY *********/
	err = AudioUnitGetPropertyInfo (GetEditAudioUnit(), 
								kAudioUnitProperty_BypassEffect, 
								kAudioUnitScope_Global, 
								0, 
								NULL, 
								&hasProperty[0]);
#ifdef DISABLE_BYPASS
	hasProperty[0] = false;
#else
	hasProperty[0] = (!err && hasProperty[0]) ? true : false;
#endif  /* DISABLE_BYPASS */
	propertyCount += hasProperty[0]? 1 : 0;
	
	/********* FACTORY PRESETS PROPERTY *********/
	CFArrayRef presets = NULL;
	propertySize = sizeof(presets);
	err = AudioUnitGetProperty (GetEditAudioUnit(), 
								kAudioUnitProperty_FactoryPresets,
								kAudioUnitScope_Global, 
								0, 
								&presets, 
								&propertySize);
	hasProperty[1] = (!err && presets) ? true : false;
	propertyCount += hasProperty[1]? 1 : 0;
	
	/********* CPU LOAD PROPERTY *********/
	Float32 theCPULoad;
	UInt32 size = sizeof(Float32);
	err = AudioUnitGetProperty (GetEditAudioUnit(), 
								kAudioUnitProperty_CPULoad, 
								kAudioUnitScope_Global, 
								0, 
								&theCPULoad, 
								&size);
	hasProperty[2] = (!err) ? true : false;
	propertyCount += hasProperty[2]? 1 : 0;
	
	/********* RENDER QUALITY PROPERTY *********/
	UInt32 theRenderQuality;
	size = sizeof(UInt32);
	err = AudioUnitGetProperty (GetEditAudioUnit(), 
								kAudioUnitProperty_RenderQuality, 
								kAudioUnitScope_Global, 
								0, 
								&theRenderQuality, 
								&size);
								
	hasProperty[3] = (!err) ? true : false;
	propertyCount += hasProperty[3]? 1 : 0;
	
	/********** DISK STREAMING PROPERTY ********/
	UInt32 isDiskStreaming;
	size = sizeof(UInt32);
	err = AudioUnitGetProperty (GetEditAudioUnit(), 
								kMusicDeviceProperty_StreamFromDisk, 
								kAudioUnitScope_Global, 
								0, 
								&isDiskStreaming, 
								&size);
	
	hasProperty[4] = (!err) ? true : false;
	propertyCount += hasProperty[4]? 1 : 0;
	
	if (propertyCount >= 2) {
		// Create the header for the Properties
		Rect r = {location.v, location.h, location.v + kRowHeight, location.h + 62};
	
		ControlRef ref;
		fontStyle.font = kControlFontSmallBoldSystemFont;

		OSErr theErr =  CreateStaticTextControl (GetCarbonWindow(), &r, kPropertiesLabelString, &fontStyle, &ref);
		if (theErr == noErr) {
			Rect bestRect;
			SInt16 baseLineOffset;
			
			GetBestControlRect(ref, &bestRect, &baseLineOffset);
			EmbedControl(ref);
			r.left = r.right + 2;
			r.right = location.h + 440;
			r.bottom -= 5;
			if (noErr == CreateSeparatorControl (GetCarbonWindow(), &r, &ref))
				EmbedControl(ref);
			y += kRowHeight;
			location.v += kRowHeight;
			outSize->bottom += kRowHeight;
		}
		fontStyle.font = kControlFontSmallSystemFont;
	}
	
	if (hasProperty[0]) {	// Bypass
		mBypassEffect = new AUBypassEffect (this, location, kParamNameWidth, fontStyle);
		y += kRowHeight;
		location.v += kRowHeight;
		outSize->right = kParamNameWidth + kRowHeight;
		outSize->bottom += kRowHeight;
	}
	
	if (hasProperty[1]) {	// Presets
		location.h = x;
		mFactoryPresets = new AUVPresets (this, presets, location, kParamNameWidth, kSliderWidth + kPopupAdjustment, fontStyle); 
		y += kRowHeight;
		outSize->bottom += kRowHeight;
	}
	
	if (hasProperty[2]) {	// CPU Load
		location.h = x;
		mLoadCPU = new AULoadCPU (this, location, kParamNameWidth, fontStyle);
		y += kRowHeight;
		outSize->bottom += kRowHeight;
	}	
	
	if (hasProperty[4]) {	// Disk Streaming
		location.h = x;
		if (hasProperty[2] || hasProperty[3]) 
			location.h += 230;
		else 
			location.v += kRowHeight;
			
		mStreamingCheckbox = new AUDiskStreamingCheckbox (this, location, kParamNameWidth, fontStyle);
		if (!hasProperty[2]) {
			y += kRowHeight;
			outSize->bottom += kRowHeight;
		}
	}
	
	if (hasProperty[3]) {	// Render Quality
		location.h = x;
		location.v += kRowHeight + 4;
		mQualityPopup = new AURenderQualityPopup (this, location, kParamNameWidth, fontStyle);
		y += (kRowHeight - 12);
		outSize->bottom += kRowHeight;
	}	

	return noErr;
}


OSStatus	GenericAUView::CreateUI(Float32	inXOffset, Float32 	inYOffset)
{
	initialLoc.h = (short)inXOffset;	// store initial location in case we have to rebuild our interface
	initialLoc.v = (short)inYOffset;
	
	Point location;
	location.h = 6 + initialLoc.h;
	location.v = 10 + initialLoc.v;
	
	static const int kParamNameWidth = 140;
	static const int kSliderWidth = 240;
	static const int kEditTextWidth = 40;
	static const int kParamTagWidth = 30;
	static const SInt16 kRightStartOffset = kParamNameWidth + kEditTextWidth + kParamTagWidth;
	static const SInt16 kTotalWidth = kRightStartOffset + kSliderWidth;
		
	// Display the Name of the unit
	AUControlGroup::AddAUInfo (this, location, kRightStartOffset, kTotalWidth);
	
	Rect theRect;
	ComponentResult result = CreateUIForStandardProperties ((Float32)location.h, (Float32) location.v, &theRect);
	if (result != noErr)
		return result;
		
	result = CreateUIForParameters ((Float32)location.h, (Float32)theRect.bottom, &theRect);
	
	if (result != noErr)
		return result;

		// don't add these until we know we can display the parameter list!
	result = AudioUnitAddPropertyListener(mEditAudioUnit, kAudioUnitProperty_ParameterList, GenericViewPropertyListener, this);
	result = AudioUnitAddPropertyListener(mEditAudioUnit, kAudioUnitProperty_ParameterInfo, GenericViewPropertyListener, this);
	
	return result;
}

void GenericAUView::RebuildUI() {
	UInt16 		numSubControls;
	ControlRef 	subControl;
	
	if (mParentViewContainer != NULL) {
		Cleanup();
		
		if (CountSubControls (mParentViewContainer, &numSubControls) == noErr) {
		
			for (UInt16 i = 1; i<= numSubControls; i++) {
				GetIndexedSubControl(mParentViewContainer, i, &subControl);
				DisposeControl(subControl);
			}
			
			// now that all controls have been removed, we need to clean up and re-add controls
			CreateUI(initialLoc.h, initialLoc.v);
		}
	}
}

void GenericViewPropertyListener(void * inRefCon, AudioUnit ci, AudioUnitPropertyID  inID, 
										AudioUnitScope inScope, AudioUnitElement inElement) 
{
	GenericAUView *This = (GenericAUView*)inRefCon;
	
	if (inID == kAudioUnitProperty_ParameterList || inID == kAudioUnitProperty_ParameterInfo) {
		// when we receive a kAudioUnitProperty_ParameterList or kAudioUnitProperty_ParameterInfo, we need to rebuild the UI
		// call disposeControl on all controls and recreate them
		This->RebuildUI();
	}
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


/*
				r.left = r.right + 52;	r.right = r.left + kSliderWidth - 70;
				r.bottom -= 1; //take the tail adjustement out
				r.top += 3; //take the tail adjustement out
*/

MeterView::MeterView (AUCarbonViewBase 			* auView,
					AUVParameter 				& auvp, 
					Rect 						& area, 
					Point 						& labelSize,
					int							paramTagWidth,
					ControlFontStyleRec 		& fontStyle)
	: mUnit (auView->GetEditAudioUnit()),
	  mParam (auvp.mParameterID),
	  mRect (area)
{
	mMinValue = auvp.ParamInfo().minValue;
	Float32 maxValue = auvp.ParamInfo().maxValue;

	int height = area.bottom - area.top;
	
	Rect minValRect, maxValRect;
	maxValRect.top = minValRect.top = area.top + (height - labelSize.v) / 2;
	minValRect.left = area.left;
	maxValRect.left = area.right - labelSize.h - paramTagWidth;
	
	maxValRect.bottom = minValRect.bottom = minValRect.top + labelSize.v;
	minValRect.right = minValRect.left + labelSize.h;
	maxValRect.right = maxValRect.left + labelSize.h;
	
	char text[64];
		
		//min value label
	CFStringRef cfstr = auvp.GetValueNameCopy(mMinValue);
	if (cfstr == NULL) {
		AUParameterFormatValue (mMinValue, &auvp, text, 3);
		cfstr = CFStringCreateWithCString(NULL, text, kCFStringEncodingASCII);
	}
	
	fontStyle.just = teFlushRight;
	ControlRef newControl;
	verify_noerr(CreateStaticTextControl (auView->GetCarbonWindow(), &minValRect, cfstr, &fontStyle, &newControl));
	CFRelease(cfstr);
	verify_noerr(auView->EmbedControl(newControl));

		//max value label
	cfstr = auvp.GetValueNameCopy(maxValue);
	if (cfstr == NULL) {
		AUParameterFormatValue(maxValue, &auvp, text, 3);
		cfstr = CFStringCreateWithCString(NULL, text, kCFStringEncodingMacRoman);
	}
	fontStyle.just = teFlushLeft;
	verify_noerr(CreateStaticTextControl(auView->GetCarbonWindow(), &maxValRect, cfstr, &fontStyle, &newControl));
	CFRelease(cfstr);
	verify_noerr(auView->EmbedControl(newControl));

	if (auvp.GetParamTag()) {
		Rect paramTagRect;
		paramTagRect.top = maxValRect.top;
		paramTagRect.left = area.right - paramTagWidth;
		paramTagRect.bottom = maxValRect.bottom;
		paramTagRect.right = area.right;
		
		fontStyle.just = teFlushLeft;
		
		if (noErr == CreateStaticTextControl(auView->GetCarbonWindow(), &paramTagRect, auvp.GetParamTag(), &fontStyle, &newControl))
			verify_noerr(auView->EmbedControl(newControl));
	}
	
	mValueRange = (maxValue - mMinValue);
	mLastValueDrawn = 0xFFFFFFFF;
	//set up drawing rect coordinates
	mRect.left += 52;
	mRect.right -= 82;
	mRect.top += 3;
}

void MeterView::Draw ()
{
	Float32 value = 0;
	AudioUnitGetParameter(mUnit, mParam, kAudioUnitScope_Global, 0, &value);
			
		// givea us a 0->1 scale
	value = (value - mMinValue) / mValueRange;
	if (value < 0)
		value = 0;
	if (value > 1)
		value = 1;
	
	UInt32 width = mRect.right - mRect.left;
	UInt32 xOffset = UInt32(width * value);
	
	if (xOffset == mLastValueDrawn)
		return;

	mLastValueDrawn = xOffset;
	
	Rect valueRect = { mRect.top, mRect.left, mRect.bottom, xOffset + mRect.left };
		
	RGBColor color;
	if (value > 0) {
		color.red = 32 * 256;
		color.blue = 185 * 256;
		color.green = 32768;
		RGBForeColor (&color);
		
		PaintRect (&valueRect);
	}
	
	if (value < 1) {
		valueRect.left = xOffset + mRect.left;
		valueRect.right = mRect.right;
		
		color.red = 48000;
		color.blue = 48000;
		color.green = 48000;
		RGBForeColor (&color);
		PaintRect (&valueRect);
	}
}
