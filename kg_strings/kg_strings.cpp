#include "src/kg_strings_core.cpp"
#include "src/kg_strings_params.cpp"
#include "src/kg_strings_paths.cpp"
#include "src/kg_strings_web.cpp"
#include "src/kg_strings_ui.cpp"
#include "src/kg_strings_render.cpp"

} // namespace

extern "C" DllExport PF_Err PluginDataEntryFunction(
	PF_PluginDataPtr inPtr,
	PF_PluginDataCB inPluginDataCallBackPtr,
	SPBasicSuite *,
	const char *,
	const char *)
{
	PF_Err result = PF_Err_INVALID_CALLBACK;

	result = PF_REGISTER_EFFECT(
		inPtr,
		inPluginDataCallBackPtr,
		KG_NAME,
		KG_MATCHNAME,
		KG_CATEGORY,
		AE_RESERVED_INFO);

	return result;
}

PF_Err EffectMain(
	PF_Cmd cmd,
	PF_InData *in_data,
	PF_OutData *out_data,
	PF_ParamDef *params[],
	PF_LayerDef *output,
	void *extra)
{
	PF_Err err = PF_Err_NONE;

	try {
		switch (cmd) {
			case PF_Cmd_ABOUT:
				err = About(in_data, out_data, params, output);
				break;
			case PF_Cmd_GLOBAL_SETUP:
				err = GlobalSetup(in_data, out_data);
				break;
			case PF_Cmd_PARAMS_SETUP:
				err = ParamsSetup(in_data, out_data);
				break;
			case PF_Cmd_RENDER:
				err = Render(in_data, out_data, params, output);
				break;
			case PF_Cmd_EVENT:
				err = HandleEvent(in_data, out_data, params, output, reinterpret_cast<PF_EventExtra *>(extra));
				break;
			case PF_Cmd_SMART_PRE_RENDER:
				err = PreRender(in_data, out_data, reinterpret_cast<PF_PreRenderExtra *>(extra));
				break;
			case PF_Cmd_SMART_RENDER:
				err = SmartRender(in_data, out_data, reinterpret_cast<PF_SmartRenderExtra *>(extra));
				break;
			default:
				break;
		}
	} catch (PF_Err &thrown_err) {
		err = thrown_err;
	}

	return err;
}
