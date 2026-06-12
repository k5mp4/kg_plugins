#include "AEConfig.h"
#include "AE_EffectVers.h"

#ifndef AE_OS_WIN
	#include "AE_General.r"
#endif

#define KG_NAME "kg_strings"
#define KG_MATCHNAME "kg_strings"
#define KG_CATEGORY "kg_plugins"
#define KG_VERSION 524308
#define KG_OUT_FLAGS 34636804
#define KG_OUT_FLAGS2 134217738

resource 'PiPL' (16001) {
	{
		Kind {
			AEEffect
		},
		Name {
			KG_NAME
		},
		Category {
			KG_CATEGORY
		},
#ifdef AE_OS_WIN
	#ifdef AE_PROC_INTELx64
		CodeWin64X86 {"EffectMain"},
	#endif
#else
	#ifdef AE_OS_MAC
		CodeMacIntel64 {"EffectMain"},
		CodeMacARM64 {"EffectMain"},
	#endif
#endif
		AE_PiPL_Version {
			2,
			0
		},
		AE_Effect_Spec_Version {
			PF_PLUG_IN_VERSION,
			PF_PLUG_IN_SUBVERS
		},
		AE_Effect_Version {
			KG_VERSION
		},
		AE_Effect_Info_Flags {
			0
		},
		AE_Effect_Global_OutFlags {
			KG_OUT_FLAGS
		},
		AE_Effect_Global_OutFlags_2 {
			KG_OUT_FLAGS2
		},
		AE_Effect_Match_Name {
			KG_MATCHNAME
		},
		AE_Reserved_Info {
			0
		}
	}
};
