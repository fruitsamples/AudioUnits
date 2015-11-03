/*	Copyright: 	© Copyright 2004 Apple Computer, Inc. All rights reserved.

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
	AUPulseDetectorView.cpp
	
=============================================================================*/

#include "AUCarbonViewBase.h"
#include "AUControlGroup.h"
#include <map>
#include <vector>

#include "AUPulseShared.h"

class AUPulseDetectorView : public AUCarbonViewBase {

public:
	AUPulseDetectorView(AudioUnitCarbonView auv);
	virtual ~AUPulseDetectorView();
	
	virtual OSStatus		CreateUI (Float32	inXOffset, Float32 	inYOffset);
	virtual void			RebuildUI();
    
protected:
	virtual OSStatus		CreateUIForParameters (Float32 inXOffset, Float32 inYOffset, Rect *outSize);
	OSStatus				CreateSpecificUI (Float32 inXOffset, Float32 inYOffset, Rect *outSize);
	
	void 					DrawParameter (CAAUParameter &auvp, const int x, int &y, ControlFontStyleRec &fontStyle, Rect *outSize);

	void 					UpdatePulseDisplay();
    
private:
	AUEventListenerRef		mPropertyChangeListener;
	
	ControlRef				GetCarbonPane()	{	return mCarbonPane;	}    
	void					Cleanup();
	static void				AUPropertyChangedListener(   void *						inCallbackRefCon,
														void *						inObject,
														const AudioUnitEvent *		inEvent,
														UInt64						inEventHostTime,
														Float32						inParameterValue);

	ControlRef	mLastFrameDisplay, mLastPulseDisplay, mNumMeasDisplay, 	mMeanDisplay, mStdDevDisplay, mMinDisplay, mMaxDisplay;
	UInt32		mLastDisplayValuePulse;
	bool		mLastDisplaySDWasZero;
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

static const CFStringRef kParametersLabelString = CFSTR("Parameters");

COMPONENT_ENTRY(AUPulseDetectorView)

AUPulseDetectorView::AUPulseDetectorView(AudioUnitCarbonView auv) 
		: AUCarbonViewBase(auv), 
		  mPropertyChangeListener (0)
{
}

AUPulseDetectorView::~AUPulseDetectorView()
{
	Cleanup();
	if (mPropertyChangeListener)
		AUListenerDispose(mPropertyChangeListener);
}

static CFStringRef resetStr = CFSTR("-");

void 		AUPulseDetectorView::UpdatePulseDisplay()
{
	AUPulseMetrics metrics;
	UInt32 size = sizeof(metrics);
	OSStatus result = AudioUnitGetProperty (GetEditAudioUnit(), 
								kAUPulseMetricsPropertyID, 
								kAudioUnitScope_Global, 
								0, 
								&metrics, 
								&size);
	if (result)
		return;

	Float64 sampleRate;
	size = sizeof(sampleRate);
	result = AudioUnitGetProperty (GetEditAudioUnit(), 
								kAudioUnitProperty_SampleRate, 
								kAudioUnitScope_Global, 
								0, 
								&sampleRate, 
								&size);	
	if (result)
		return;

	if (metrics.isValid) 
	{
		char cstr[64];
		
		bool needToRedraw = false;
		if (metrics.lastPulseTime != mLastDisplayValuePulse) 
		{
			sprintf (cstr, "%ld Samples, %.2f msecs", metrics.lastPulseTime, (metrics.lastPulseTime / sampleRate * 1000.));	
			CFStringRef str = CFStringCreateWithCString (0, cstr, kCFStringEncodingASCII);
			SetControlData (mLastPulseDisplay, 0, kControlStaticTextCFStringTag, sizeof(CFStringRef), &str);
			CFRelease (str);
	
			sprintf (cstr, "%ld Samples, %.2f msecs", metrics.numFrames, (metrics.numFrames / sampleRate * 1000.));	
			str = CFStringCreateWithCString (0, cstr, kCFStringEncodingASCII);
			SetControlData (mLastFrameDisplay, 0, kControlStaticTextCFStringTag, sizeof(CFStringRef), &str);
			CFRelease (str);

			sprintf (cstr, "%ld", metrics.numMeasurements);	
			str = CFStringCreateWithCString (0, cstr, kCFStringEncodingASCII);
			SetControlData (mNumMeasDisplay, 0, kControlStaticTextCFStringTag, sizeof(CFStringRef), &str);
			CFRelease (str);
	
			sprintf (cstr, "%.2f", metrics.mean);	
			str = CFStringCreateWithCString (0, cstr, kCFStringEncodingASCII);
			SetControlData (mMeanDisplay, 0, kControlStaticTextCFStringTag, sizeof(CFStringRef), &str);
			CFRelease (str);

			mLastDisplayValuePulse = metrics.lastPulseTime;
			needToRedraw = true;
		}
		else
		{
			sprintf (cstr, "%ld", metrics.numMeasurements);	
			CFStringRef str = CFStringCreateWithCString (0, cstr, kCFStringEncodingASCII);
			SetControlData (mNumMeasDisplay, 0, kControlStaticTextCFStringTag, sizeof(CFStringRef), &str);
			CFRelease (str);
			DrawOneControl (mNumMeasDisplay);
		}
		
		if (metrics.stdDev)
		{
			sprintf (cstr, "%.2f", metrics.stdDev);	
			CFStringRef str = CFStringCreateWithCString (0, cstr, kCFStringEncodingASCII);
			SetControlData (mStdDevDisplay, 0, kControlStaticTextCFStringTag, sizeof(CFStringRef), &str);
			CFRelease (str);

			sprintf (cstr, "%ld", metrics.min);	
			str = CFStringCreateWithCString (0, cstr, kCFStringEncodingASCII);
			SetControlData (mMinDisplay, 0, kControlStaticTextCFStringTag, sizeof(CFStringRef), &str);
			CFRelease (str);

			sprintf (cstr, "%ld", metrics.max);	
			str = CFStringCreateWithCString (0, cstr, kCFStringEncodingASCII);
			SetControlData (mMaxDisplay, 0, kControlStaticTextCFStringTag, sizeof(CFStringRef), &str);
			CFRelease (str);

			sprintf (cstr, "%.2f", metrics.mean);	
			str = CFStringCreateWithCString (0, cstr, kCFStringEncodingASCII);
			SetControlData (mMeanDisplay, 0, kControlStaticTextCFStringTag, sizeof(CFStringRef), &str);
			CFRelease (str);

			mLastDisplaySDWasZero = false;
			needToRedraw = true;
		} else {
			if (!mLastDisplaySDWasZero) 
			{
				SetControlData (mStdDevDisplay, 0, kControlStaticTextCFStringTag, sizeof(CFStringRef), &resetStr);
				SetControlData (mMinDisplay, 0, kControlStaticTextCFStringTag, sizeof(CFStringRef), &resetStr);
				SetControlData (mMaxDisplay, 0, kControlStaticTextCFStringTag, sizeof(CFStringRef), &resetStr);
				mLastDisplaySDWasZero = true;
				needToRedraw = true;
			}
		}
		
		if (needToRedraw)
			DrawOneControl (mCarbonPane);		
	} else {
		CFStringRef str = CFSTR("<<Unable To Detect Pulse>>");
		SetControlData (mLastPulseDisplay, 0, kControlStaticTextCFStringTag, sizeof(CFStringRef), &str);
		DrawOneControl (mLastPulseDisplay);		
		mLastDisplayValuePulse = 0;
		mLastDisplaySDWasZero = false;
	}
}

OSStatus	AUPulseDetectorView::CreateSpecificUI (Float32 inXOffset, Float32 inYOffset, Rect *outSize)
{
	static const int kRowHeight = 24;
	static const int kHeadingBodyDist = 24;
    static const int kNameWidth = 200;
	
	int y = short(inYOffset) + kHeadingBodyDist;
	const int x = short(inXOffset);
	
	outSize->left= x;
	outSize->right= x;
	outSize->top = y;
	outSize->bottom = y;
	
	ControlFontStyleRec fontStyle;
	fontStyle.flags = kControlUseFontMask | kControlUseJustMask;
	fontStyle.font = kControlFontSmallSystemFont;

	Rect r = *outSize;
	r.bottom += kRowHeight + 8;
	
	ControlRef ref;
	OSStatus result;

// PULSE TIME
	r.right += kNameWidth;
	fontStyle.just = teFlushRight;
	require_noerr (result = CreateStaticTextControl (GetCarbonWindow(), &r, CFSTR("Last Pulse Time: "), &fontStyle, &ref), home);
	EmbedControl(ref);
	
	r.left += r.right;
	r.right += kNameWidth;
	fontStyle.just = teFlushLeft;
	require_noerr (result = CreateStaticTextControl (GetCarbonWindow(), &r, resetStr, &fontStyle, &mLastPulseDisplay), home);
	EmbedControl(mLastPulseDisplay);
	r.top += kRowHeight + 20;
	r.bottom = r.top + kRowHeight;
	r.left = x;
	r.right = x;

// LAST FRAME SIZE
	r.right += kNameWidth;
	fontStyle.just = teFlushRight;
	require_noerr (result = CreateStaticTextControl (GetCarbonWindow(), &r, CFSTR("Last Frame Size: "), &fontStyle, &ref), home);
	EmbedControl(ref);
	
	r.left += r.right;
	r.right += kNameWidth;
	fontStyle.just = teFlushLeft;
	require_noerr (result = CreateStaticTextControl (GetCarbonWindow(), &r, resetStr, &fontStyle, &mLastFrameDisplay), home);
	EmbedControl(mLastFrameDisplay);
	r.top += kRowHeight;
	r.bottom = r.top + kRowHeight;
	r.left = x;
	r.right = x;

// NUM MEASUREMENTS
	r.right += kNameWidth;
	fontStyle.just = teFlushRight;
	require_noerr (result = CreateStaticTextControl (GetCarbonWindow(), &r, CFSTR("Number of Measurements: "), &fontStyle, &ref), home);
	EmbedControl(ref);
	
	r.left += r.right;
	r.right += kNameWidth;
	fontStyle.just = teFlushLeft;
	require_noerr (result = CreateStaticTextControl (GetCarbonWindow(), &r, resetStr, &fontStyle, &mNumMeasDisplay), home);
	EmbedControl(mNumMeasDisplay);
	r.top += kRowHeight;
	r.bottom = r.top + kRowHeight;
	r.left = x;
	r.right = x;

// MEAN
	r.right += kNameWidth;
	fontStyle.just = teFlushRight;
	require_noerr (result = CreateStaticTextControl (GetCarbonWindow(), &r, CFSTR("Mean: "), &fontStyle, &ref), home);
	EmbedControl(ref);
	
	r.left += r.right;
	r.right += kNameWidth;
	fontStyle.just = teFlushLeft;
	require_noerr (result = CreateStaticTextControl (GetCarbonWindow(), &r, resetStr, &fontStyle, &mMeanDisplay), home);
	EmbedControl(mMeanDisplay);
	r.top += kRowHeight;
	r.bottom = r.top + kRowHeight;
	r.left = x;
	r.right = x;

// STANDARD DEVIATION
	r.right += kNameWidth;
	fontStyle.just = teFlushRight;
	require_noerr (result = CreateStaticTextControl (GetCarbonWindow(), &r, CFSTR("Standard Deviation: "), &fontStyle, &ref), home);
	EmbedControl(ref);
	
	r.left += r.right;
	r.right += kNameWidth;
	fontStyle.just = teFlushLeft;
	require_noerr (result = CreateStaticTextControl (GetCarbonWindow(), &r, resetStr, &fontStyle, &mStdDevDisplay), home);
	EmbedControl(mStdDevDisplay);
	r.top += kRowHeight;
	r.bottom = r.top + kRowHeight;
	r.left = x;
	r.right = x;

// MIN
	r.right += kNameWidth;
	fontStyle.just = teFlushRight;
	require_noerr (result = CreateStaticTextControl (GetCarbonWindow(), &r, CFSTR("Minimum: "), &fontStyle, &ref), home);
	EmbedControl(ref);
	
	r.left += r.right;
	r.right += kNameWidth;
	fontStyle.just = teFlushLeft;
	require_noerr (result = CreateStaticTextControl (GetCarbonWindow(), &r, resetStr, &fontStyle, &mMinDisplay), home);
	EmbedControl(mMinDisplay);
	r.top += kRowHeight;
	r.bottom = r.top + kRowHeight;
	r.left = x;
	r.right = x;

// MAX
	r.right += kNameWidth;
	fontStyle.just = teFlushRight;
	require_noerr (result = CreateStaticTextControl (GetCarbonWindow(), &r, CFSTR("Maximum: "), &fontStyle, &ref), home);
	EmbedControl(ref);
	
	r.left += r.right;
	r.right += kNameWidth;
	fontStyle.just = teFlushLeft;
	require_noerr (result = CreateStaticTextControl (GetCarbonWindow(), &r, resetStr, &fontStyle, &mMaxDisplay), home);
	EmbedControl(mMaxDisplay);

	outSize->bottom = r.bottom;
	outSize->right = y + kNameWidth + kNameWidth;
	
home:
	return result;
}

void 		AUPulseDetectorView::Cleanup()
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
	mLastFrameDisplay = 0;
	mLastPulseDisplay = 0;
	mNumMeasDisplay = 0;
	mMeanDisplay = 0;
	mStdDevDisplay = 0;
	mMinDisplay = 0;
	mMaxDisplay = 0;
}

OSStatus	AUPulseDetectorView::CreateUI(Float32	inXOffset, Float32 	inYOffset)
{
	if (mPropertyChangeListener == NULL) {
		AUEventListenerCreate(AUPropertyChangedListener, this, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode, 0., 0., &mPropertyChangeListener);
		AudioUnitEvent e;
		e.mEventType = kAudioUnitEvent_PropertyChange;
		e.mArgument.mProperty.mAudioUnit = mEditAudioUnit;
		e.mArgument.mProperty.mPropertyID = kAudioUnitProperty_ParameterList;
		e.mArgument.mProperty.mScope = kAudioUnitScope_Global;
		e.mArgument.mProperty.mElement = 0;
		AUEventListenerAddEventType(mPropertyChangeListener, this, &e);
		
		e.mArgument.mProperty.mPropertyID = kAudioUnitProperty_ParameterInfo;
		AUEventListenerAddEventType(mPropertyChangeListener, this, &e);

		e.mArgument.mProperty.mPropertyID = kAUPulseMetricsPropertyID;
		AUEventListenerAddEventType(mPropertyChangeListener, this, &e);
	}
	mLastDisplayValuePulse = 0;
	mLastDisplaySDWasZero = false;
	
	Point location;
	location.h = 6 + short(inXOffset);
	location.v = 10 + short(inYOffset);
	
	static const int kParamNameWidth = 140;
	static const int kSliderWidth = 240;
	static const int kEditTextWidth = 40;
	static const int kParamTagWidth = 30;
	static const SInt16 kRightStartOffset = kParamNameWidth + kEditTextWidth + kParamTagWidth;
	static const SInt16 kTotalWidth = kRightStartOffset + kSliderWidth;
		
	// Display the Name of the unit
	AUControlGroup::AddAUInfo (this, location, kRightStartOffset, kTotalWidth);
	
	Rect theRect;
	ComponentResult result = CreateSpecificUI ((Float32)location.h, (Float32) location.v, &theRect);
	if (result != noErr)
		return result;
		
	return CreateUIForParameters ((Float32)location.h, (Float32)theRect.bottom, &theRect);
}

void AUPulseDetectorView::RebuildUI() 
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

OSStatus	AUPulseDetectorView::CreateUIForParameters (Float32 inXOffset, Float32 inYOffset, Rect *outSize) 
{
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
	if (propertySize == 0)
		return noErr;
	
	int nparams = propertySize / sizeof(AudioUnitPropertyID);
	AudioUnitParameterID *paramIDs = new AudioUnitParameterID[nparams];

	err = AudioUnitGetProperty(mEditAudioUnit, kAudioUnitProperty_ParameterList,
		kAudioUnitScope_Global, 0, paramIDs, &propertySize);
	
	typedef std::vector <CAAUParameter> SortedParamList;
	typedef std::map <UInt32, SortedParamList, std::less<UInt32> > ParameterMap;
	
	ParameterMap params;
	if (!err) {
		for (int i = 0; i < nparams; ++i) {
			CAAUParameter auvp(mEditAudioUnit, paramIDs[i], kAudioUnitScope_Global, 0);
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
                
		ControlFontStyleRec fontStyle;
		fontStyle.flags = kControlUseFontMask | kControlUseJustMask;
		fontStyle.font = kControlFontSmallSystemFont;
						
		if (nparams > 0) {
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
			}
			
			fontStyle.font = kControlFontSmallSystemFont;
		}
		
		ParameterMap::iterator i = params.begin();
		while (1) {
			if (i != params.end()) {
				for (SortedParamList::iterator piter = (*i).second.begin(); piter != (*i).second.end(); ++piter)
				{
					DrawParameter (*piter, x, y, fontStyle, outSize);
					y += kRowHeight;
					outSize->bottom += kRowHeight;
				}
			}
			if (i != params.end() && ++i != params.end()) {
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

void 	AUPulseDetectorView::DrawParameter (CAAUParameter &auvp, const int x, int &y, ControlFontStyleRec &fontStyle, Rect *outSize)
{
	ControlRef newControl;			
	Rect r;
    			
	if (auvp.HasNamedParams()) 									// NAMED PARAMETER (POPUP)
	{
		fontStyle.font = kControlFontSmallSystemFont;
		
		r.top = y + 2;		r.bottom = r.top + kTextHeight;
		r.left = x;		r.right = r.left + kParamNameWidth;
		fontStyle.just = teFlushRight;
		
		CFMutableStringRef str = CFStringCreateMutableCopy(NULL, CFStringGetLength(auvp.GetName()) + 1, auvp.GetName());
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
		CFStringAppend (str, CFSTR(":"));
		
		if (noErr == CreateStaticTextControl(mCarbonWindow, &r, str, &fontStyle, &newControl))
			verify_noerr(EmbedControl(newControl));
			
		CFRelease (str);
		
		r.left = r.right + 8;	r.right = r.left + kSliderWidth;
		theWidth += 8 + kSliderWidth;
		r.bottom -= 2; // take the tail adjustement out
		
		Point labelSize, textSize;
		labelSize.v = textSize.v = kTextHeight - 3;
		labelSize.h = kMinMaxWidth;
		textSize.h = kEditTextWidth;
		
		AUControlGroup::CreateLabelledSliderAndEditText(this, auvp, r, labelSize, textSize, fontStyle);					
		if (auvp.GetParamTag()) {
			r.left = r.right + 8; 	r.right = r.left + kParamTagWidth;
			fontStyle.just = 0;
            
            // try to localize param tag
            CFStringRef tagString = auvp.GetParamTag();
            
			if (noErr == CreateStaticTextControl(mCarbonWindow, &r, tagString, &fontStyle, &newControl))
				verify_noerr(EmbedControl(newControl));
			theWidth += 8 + kParamTagWidth;
		}
		if (outSize->right - outSize->left < theWidth) {
			outSize->right = outSize->left + theWidth;
        }
	}
}

void	AUPulseDetectorView::AUPropertyChangedListener(	void *					inCallbackRefCon,
													void *						inObject,
													const AudioUnitEvent *		inEvent,
													UInt64						inEventHostTime,
													Float32						inParameterValue)
{
	if (inEvent->mEventType == kAudioUnitEvent_PropertyChange)
	{
		if (inEvent->mArgument.mProperty.mPropertyID == kAudioUnitProperty_ParameterList 
			|| inEvent->mArgument.mProperty.mPropertyID == kAudioUnitProperty_ParameterInfo) 
		{
			AUPulseDetectorView *This = static_cast<AUPulseDetectorView *>(inCallbackRefCon);
			This->RebuildUI();
		}
		else if (inEvent->mArgument.mProperty.mPropertyID == kAUPulseMetricsPropertyID) 
		{
			AUPulseDetectorView *This = static_cast<AUPulseDetectorView *>(inCallbackRefCon);
			This->UpdatePulseDisplay();
		}
	}
}

