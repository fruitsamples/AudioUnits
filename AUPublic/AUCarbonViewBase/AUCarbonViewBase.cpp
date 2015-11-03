/*	Copyright: 	� Copyright 2003 Apple Computer, Inc. All rights reserved.

	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
			("Apple") in consideration of your agreement to the following terms, and your
			use, installation, modification or redistribution of this Apple software
			constitutes acceptance of these terms.  If you do not agree with these terms,
			please do not use, install, modify or redistribute this Apple software.

			In consideration of your agreement to abide by the following terms, and subject
			to these terms, Apple grants you a personal, non-exclusive license, under Apple�s
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
	AUCarbonViewBase.cpp
	
=============================================================================*/

#include "AUCarbonViewBase.h"
#include "AUCarbonViewControl.h"
#include <algorithm>

/* notification interval defaults to 100ms */
AUCarbonViewBase::AUCarbonViewBase(AudioUnitCarbonView inInstance) :
	ComponentBase(inInstance),
	mEditAudioUnit(0),
	mParameterListener(NULL),
	mEventListener(NULL),
	mTimerRef (NULL),
	mTimerUPP (NULL),
	mCarbonWindow(NULL),
	mCarbonPane(NULL),
	mXOffset(0), 
	mYOffset (0)
{
	AUListenerCreate(	ParameterListener, this,
		CFRunLoopGetCurrent(), kCFRunLoopDefaultMode, 0.100 /* 100ms */,
		&mParameterListener);
}

AUCarbonViewBase::AUCarbonViewBase(AudioUnitCarbonView inInstance, Float32 inNotificationInterval /* in seconds */) :
	ComponentBase(inInstance),
	mEditAudioUnit(0),
	mParameterListener(NULL),
	mEventListener(NULL),
	mTimerRef (NULL),
	mTimerUPP (NULL),
	mCarbonWindow(NULL),
	mCarbonPane(NULL),
	mXOffset(0), 
	mYOffset (0)
{
	AUListenerCreate(	ParameterListener, this,
		CFRunLoopGetCurrent(), kCFRunLoopDefaultMode, inNotificationInterval,
		&mParameterListener);
}

AUCarbonViewBase::~AUCarbonViewBase()
{
	if (mCarbonPane)
		DisposeControl(mCarbonPane);

	for (ControlList::iterator it = mControlList.begin(); it != mControlList.end(); ++it) {
		AUCarbonViewControl *ctl = *it;
		delete ctl;
	}
	AUListenerDispose(mParameterListener);

	if (mTimerRef)
		::RemoveEventLoopTimer (mTimerRef);
		
	if (mTimerUPP)
		::DisposeEventLoopTimerUPP (mTimerUPP);
}
	
void	AUCarbonViewBase::AddControl(AUCarbonViewControl *control)
{
	ControlList::iterator it = find(mControlList.begin(), mControlList.end(), control);
	if (it == mControlList.end())
		mControlList.push_back(control);
}

void	AUCarbonViewBase::RemoveControl(AUCarbonViewControl *control)
{
	ControlList::iterator it = find(mControlList.begin(), mControlList.end(), control);
	if (it != mControlList.end()) {
		AUCarbonViewControl *ctl = *it;
		mControlList.erase(it);
		delete ctl;
	}
}

void	AUCarbonViewBase::ParameterListener(	void *						inRefCon,
												void *						inObject,
												const AudioUnitParameter *	inParameter,
												Float32						inValue)
{
	AUCarbonViewControl *ctl = (AUCarbonViewControl *)inObject;
	ctl->ParameterToControl(inValue);
}

ComponentResult		AUCarbonViewBase::CreateCarbonView(AudioUnit inAudioUnit, WindowRef inWindow, ControlRef inParentControl, const Float32Point &inLocation, const Float32Point &inSize, ControlRef &outParentControl)
{
	mEditAudioUnit = inAudioUnit;
	mCarbonWindow = inWindow;
	
	Rect area;
	area.left = short(inLocation.x); area.top = short(inLocation.y);
	area.right = short(area.left + inSize.x); area.bottom = short(area.top + inSize.y);
	OSStatus err = ::CreateUserPaneControl(inWindow, &area, 
						kControlSupportsEmbedding,
						&mCarbonPane);	// subclass can resize mCarbonPane to taste
	verify_noerr(err);
	if (err) return err;
	outParentControl = mCarbonPane;
	
	// register for mouse-down in our pane -- we want to clear focus
	EventTypeSpec paneEvents[] = {
		{ kEventClassControl, kEventControlClick }
	};
	WantEventTypes(GetControlEventTarget(mCarbonPane), GetEventTypeCount(paneEvents), paneEvents);
	
	// register for window-closed hook
	EventTypeSpec windowEvents[] = {
		{ kEventClassWindow, kEventWindowClosed }
	};
	WantEventTypes(GetWindowEventTarget(mCarbonWindow), GetEventTypeCount(windowEvents), windowEvents);
	
	WindowAttributes attributes;
	verify_noerr(GetWindowAttributes(mCarbonWindow, &attributes));
	if (attributes & kWindowCompositingAttribute) {
		verify_noerr(::HIViewAddSubview(inParentControl, mCarbonPane));
		mXOffset = 0;
		mYOffset = 0;
	}
	else {
		verify_noerr(::EmbedControl(mCarbonPane, inParentControl));
		mXOffset = inLocation.x;
		mYOffset = inLocation.y;
	}
	mBottomRight.h = mBottomRight.v = 0;
	
/*	err = CreateUI (mXOffset, mYOffset);
			// we should only resize the control if a subclass has embedded
		// controls in this AND this is done with the EmbedControl call below
		// if mBottomRight is STILL equal to zero, then that wasn't done
		// so don't size the control
	if (mBottomRight.h != 0 && mBottomRight.v != 0)
		SizeControl(mCarbonPane, short(mBottomRight.h - mXOffset), short(mBottomRight.v - mYOffset));
*/

	SizeControl(mCarbonPane, 0, 0);
	err = CreateUI(mXOffset, mYOffset);
	// we should only resize the control if a subclass has embedded
	// controls in this AND this is done with the EmbedControl call below
	// if mBottomRight is STILL equal to zero, then that wasn't done
	// so don't size the control
	Rect paneBounds;
	GetControlBounds(mCarbonPane, &paneBounds);
	// only resize mCarbonPane if it has not already been resized during CreateUI
	if ((paneBounds.top == paneBounds.bottom) && (paneBounds.left == paneBounds.right)) {
		if (mBottomRight.h != 0 && mBottomRight.v != 0)
			SizeControl(mCarbonPane, (short) (mBottomRight.h - mXOffset), (short) (mBottomRight.v - mYOffset));
	}
	return err;
}

OSStatus	AUCarbonViewBase::CreateUI(Float32	inXOffset, Float32 	inYOffset)
{
	return noErr;
}

OSStatus	AUCarbonViewBase::EmbedControl(ControlRef ctl)
{
	Rect r;
	::GetControlBounds(ctl, &r);
	if (r.right > mBottomRight.h) mBottomRight.h = r.right;
	if (r.bottom > mBottomRight.v) mBottomRight.v = r.bottom;

	WindowAttributes attributes;
	verify_noerr(GetWindowAttributes(mCarbonWindow, &attributes));
	if (attributes & kWindowCompositingAttribute) 
		return ::HIViewAddSubview(mCarbonPane, ctl);
	else 
		return ::EmbedControl(ctl, mCarbonPane);	
}

void	AUCarbonViewBase::AddCarbonControl(AUCarbonViewControl::ControlType type, const AUVParameter &param, ControlRef control)
{
	verify_noerr(EmbedControl(control));
    
	AUCarbonViewControl *auvc = new AUCarbonViewControl(this, mParameterListener, type, param, control);
	auvc->Bind();
	AddControl(auvc);
}

bool	AUCarbonViewBase::HandleEvent(EventRef event)
{	
	UInt32 eclass = GetEventClass(event);
	UInt32 ekind = GetEventKind(event);
	ControlRef control;
	
	switch (eclass) {
	case kEventClassControl:
		switch (ekind) {
		case kEventControlClick:
			GetEventParameter(event, kEventParamDirectObject, typeControlRef, NULL, sizeof(ControlRef), NULL, &control);
			if (control == mCarbonPane) {
				ClearKeyboardFocus(mCarbonWindow);
				return true;
			}
		}
		break;
	case kEventClassWindow:
		switch (ekind) {
		case kEventWindowClosed:
			printf("caught window-closed\n");
			break;
		}
		break;
	}

	return false;
}

void	AUCarbonViewBase::Update (bool inUIThread)
{
	for (ControlList::iterator iter = mControlList.begin(); iter != mControlList.end(); ++iter)
	{
		(*iter)->Update(inUIThread);
	}
}

void 	AUCarbonViewBase::TheTimerProc (EventLoopTimerRef inTimer, void *inUserData)
{
	AUCarbonViewBase* This = reinterpret_cast<AUCarbonViewBase*>(inUserData);
	This->RespondToEventTimer (inTimer);
}

void 	AUCarbonViewBase::RespondToEventTimer (EventLoopTimerRef inTimer) 
{}

/* 
	THESE are reasonable values for these two times
	0.005 // delay 
	0.050 // interval
*/

OSStatus	AUCarbonViewBase::CreateEventLoopTimer (Float32 inDelay, Float32 inInterval) 
{
	if (mTimerUPP)
		return noErr;
	
	mTimerUPP = ::NewEventLoopTimerUPP(TheTimerProc);
	
	EventLoopRef mainEventLoop = GetMainEventLoop();
	
		//doesn't seem to like too small a value
	if (inDelay < 0.005)
		inDelay = 0.005;
			
	OSStatus timerResult =  ::InstallEventLoopTimer(
									mainEventLoop,
									inDelay,
									inInterval,
									mTimerUPP,
									this,
									&mTimerRef);                       
	return timerResult;
}
