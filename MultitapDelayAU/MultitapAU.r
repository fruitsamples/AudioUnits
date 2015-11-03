#include <AudioUnit/AudioUnit.r>
#include <AudioUnit/AudioUnitCarbonView.r>

// ____________________________________________________________________________
// component resources for Audio Unit
#define RES_ID			1000
#define COMP_TYPE		kAudioUnitType_Effect
#define COMP_SUBTYPE	'asmd'
#define COMP_MANUF		'appl'
#define VERSION			0x00010000
#define NAME			"Apple: Sample Multitap Delay"
#define DESCRIPTION		"Apple's sample multitap delay unit"
#define ENTRY_POINT		"MultitapAUEntry"

#include "AUResources.r"

// ____________________________________________________________________________
// component resources for Audio Unit Carbon View
#define RES_ID			2000
#define COMP_TYPE		kAudioUnitCarbonViewComponentType
#define COMP_SUBTYPE	'asmd'
#define COMP_MANUF		'appl'
#define VERSION			0x00010000
#define NAME			"Sample Multitap Delay AUView"
#define DESCRIPTION		"Sample Multitap Delay AUView"
#define ENTRY_POINT		"MultitapAUViewEntryShim"

#include "AUResources.r"
