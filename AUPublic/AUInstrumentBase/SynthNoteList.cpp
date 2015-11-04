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
/*
 *  SynthNoteList.cpp
 *  TestSynth
 *
 *  Created by James McCartney on Mon Mar 29 2004.
 *  Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
=============================================================================*/

#include "SynthNoteList.h"
#include <stdexcept>

void SynthNoteList::SanityCheck() const
{
	if (mState >= kNumberOfNoteStates) {
		throw std::runtime_error("mState is bad");
	}
	
	if (mHead == NULL) {
		if (mTail != NULL) 
			throw std::runtime_error("mHead is NULL but not mTail");
		return;
	}
	if (mTail == NULL) {
		throw std::runtime_error("mTail is NULL but not mHead");
	}
	
	if (mHead->mPrev) {
		throw std::runtime_error("mHead has a mPrev");
	}
	if (mTail->mNext) {
		throw std::runtime_error("mTail has a mNext");
	}
	
	SynthNote *note = mHead;
	while (note)
	{
		if (note->mState != mState)
			throw std::runtime_error("note in wrong state");
		if (note->mNext) {
			if (note->mNext->mPrev != note)
				throw std::runtime_error("bad link 1");
		} else {
			if (mTail != note)
				throw std::runtime_error("note->mNext is nil, but mTail != note");
		}
		if (note->mPrev) {
			if (note->mPrev->mNext != note)
				throw std::runtime_error("bad link 2");
		} else {
			if (mHead != note)
				throw std::runtime_error("note->mPrev is nil, but mHead != note");
		}
		note = note->mNext;
	}
}
