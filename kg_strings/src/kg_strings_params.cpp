static PF_Err About(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef **, PF_LayerDef *)
{
	PF_SPRINTF(out_data->return_msg, "%s v%d.%d\r%s", KG_NAME, MAJOR_VERSION, MINOR_VERSION, KG_DESCRIPTION);
	return PF_Err_NONE;
}

static PF_Err GlobalSetup(PF_InData *in_data, PF_OutData *out_data)
{
	PF_Err err = PF_Err_NONE;

	out_data->my_version = KG_VERSION;
	out_data->out_flags = KG_OUT_FLAGS;
	out_data->out_flags2 = KG_OUT_FLAGS2;

	(void)in_data;
	return err;
}

static PF_Err ParamsSetup(PF_InData *in_data, PF_OutData *out_data)
{
	PF_Err err = PF_Err_NONE;
	PF_ParamDef def;

	AEFX_CLR_STRUCT(def);
	PF_ADD_TOPIC("Path", PATH_GROUP_START_DISK_ID);

	AEFX_CLR_STRUCT(def);
	def.param_type = PF_Param_PATH;
	def.uu.id = PATH_DISK_ID;
	PF_STRCPY(def.PF_DEF_NAME, "Path");
	def.u.path_d.dephault = 0;
	PF_ADD_PARAM(in_data, -1, &def);

	const A_long extraPathDiskIDs[LNA_MAX_PATHS - 1] = {
		PATH_2_DISK_ID,
		PATH_3_DISK_ID,
		PATH_4_DISK_ID,
		PATH_5_DISK_ID,
		PATH_6_DISK_ID,
		PATH_7_DISK_ID,
		PATH_8_DISK_ID
	};
	for (A_long i = 0; i < LNA_MAX_PATHS - 1; ++i) {
		AEFX_CLR_STRUCT(def);
		def.param_type = PF_Param_PATH;
		def.uu.id = extraPathDiskIDs[i];
		PF_SPRINTF(def.PF_DEF_NAME, "Path %d", static_cast<int>(i + 2));
		def.u.path_d.dephault = 0;
		PF_ADD_PARAM(in_data, -1, &def);
	}

	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(PATH_GROUP_END_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_TOPIC("Line Setup", LINE_GROUP_START_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_SLIDER("Line Count", 1, 1024, 1, 1024, 5, LINE_COUNT_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Spacing", 0, 500, 0, 100, 6, 1, 0, 0, SPACING_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Width", 0, 200, 0, 50, 2, 1, 0, 0, WIDTH_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_SLIDER("Curve Segments", 4, 160, 8, 128, 64, CURVE_SEGMENTS_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Path Offset", -100, 100, -100, 100, 0, 1, PF_ValueDisplayFlag_PERCENT, 0, PATH_OFFSET_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_CHECKBOX("Enable 3D", "", FALSE, 0, ENABLE_3D_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_POPUP("3D Shape", 2, 1, "Cylinder|Sphere", SHAPE_3D_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Position X", -100000, 100000, -2000, 2000, 0, 1, 0, 0, POSITION_X_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Position Y", -100000, 100000, -2000, 2000, 0, 1, 0, 0, POSITION_Y_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Position Z", -100000, 100000, -2000, 2000, 0, 1, 0, 0, POSITION_Z_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Line X Spacing", -10000, 10000, -1000, 1000, 0, 1, 0, 0, LINE_X_SPACING_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Line Y Spacing", -10000, 10000, -1000, 1000, 0, 1, 0, 0, LINE_Y_SPACING_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Line Z Spacing", -10000, 10000, -1000, 1000, 80, 1, 0, 0, LINE_Z_SPACING_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Twist", -1080, 1080, -360, 360, 0, 1, 0, 0, TWIST_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Phase", -36000, 36000, -360, 360, 0, 1, 0, 0, PHASE_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Tape Flatness", 0, 100, 0, 100, 0, 1, PF_ValueDisplayFlag_PERCENT, 0, TAPE_FLATNESS_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Tape Twist", -1440, 1440, -720, 720, 0, 1, 0, 0, TAPE_TWIST_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Bend Smooth", 0, 500, 0, 150, 24, 1, 0, 0, BEND_SMOOTH_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Bend Overshoot", 0, 300, 0, 150, 60, 1, PF_ValueDisplayFlag_PERCENT, 0, BEND_OVERSHOOT_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Bend Ramp Amount", -100, 100, -100, 100, 0, 1, PF_ValueDisplayFlag_PERCENT, 0, BEND_RAMP_AMOUNT_DISK_ID);

	AEFX_CLR_STRUCT(def);
	def.flags = PF_ParamFlag_SUPERVISE;
	def.ui_flags = PF_PUI_CONTROL | PF_PUI_DONT_ERASE_CONTROL;
	def.ui_width = BEND_RAMP_UI_WIDTH;
	def.ui_height = BEND_RAMP_UI_HEIGHT;
	PF_ADD_FLOAT_SLIDER("Bend Ramp", 0, 100, 0, 100, 0, 50, 1, 0, 0, BEND_RAMP_1_DISK_ID);

	AEFX_CLR_STRUCT(def);
	def.ui_flags = PF_PUI_INVISIBLE;
	PF_ADD_FLOAT_SLIDER("Bend Ramp 2", 0, 100, 0, 100, 0, 50, 1, 0, 0, BEND_RAMP_2_DISK_ID);

	AEFX_CLR_STRUCT(def);
	def.ui_flags = PF_PUI_INVISIBLE;
	PF_ADD_FLOAT_SLIDER("Bend Ramp 3", 0, 100, 0, 100, 0, 50, 1, 0, 0, BEND_RAMP_3_DISK_ID);

	AEFX_CLR_STRUCT(def);
	def.ui_flags = PF_PUI_INVISIBLE;
	PF_ADD_FLOAT_SLIDER("Bend Ramp 4", 0, 100, 0, 100, 0, 50, 1, 0, 0, BEND_RAMP_4_DISK_ID);

	AEFX_CLR_STRUCT(def);
	def.ui_flags = PF_PUI_INVISIBLE;
	PF_ADD_FLOAT_SLIDER("Bend Ramp 5", 0, 100, 0, 100, 0, 50, 1, 0, 0, BEND_RAMP_5_DISK_ID);

	AEFX_CLR_STRUCT(def);
	def.ui_flags = PF_PUI_INVISIBLE;
	PF_ADD_FLOAT_SLIDER("Bend Handle Out 1", 0, 100, 0, 100, 0, 50, 1, 0, 0, BEND_HANDLE_OUT_1_DISK_ID);

	AEFX_CLR_STRUCT(def);
	def.ui_flags = PF_PUI_INVISIBLE;
	PF_ADD_FLOAT_SLIDER("Bend Handle In 2", 0, 100, 0, 100, 0, 50, 1, 0, 0, BEND_HANDLE_IN_2_DISK_ID);

	AEFX_CLR_STRUCT(def);
	def.ui_flags = PF_PUI_INVISIBLE;
	PF_ADD_FLOAT_SLIDER("Bend Handle Out 2", 0, 100, 0, 100, 0, 50, 1, 0, 0, BEND_HANDLE_OUT_2_DISK_ID);

	AEFX_CLR_STRUCT(def);
	def.ui_flags = PF_PUI_INVISIBLE;
	PF_ADD_FLOAT_SLIDER("Bend Handle In 3", 0, 100, 0, 100, 0, 50, 1, 0, 0, BEND_HANDLE_IN_3_DISK_ID);

	AEFX_CLR_STRUCT(def);
	def.ui_flags = PF_PUI_INVISIBLE;
	PF_ADD_FLOAT_SLIDER("Bend Handle Out 3", 0, 100, 0, 100, 0, 50, 1, 0, 0, BEND_HANDLE_OUT_3_DISK_ID);

	AEFX_CLR_STRUCT(def);
	def.ui_flags = PF_PUI_INVISIBLE;
	PF_ADD_FLOAT_SLIDER("Bend Handle In 4", 0, 100, 0, 100, 0, 50, 1, 0, 0, BEND_HANDLE_IN_4_DISK_ID);

	AEFX_CLR_STRUCT(def);
	def.ui_flags = PF_PUI_INVISIBLE;
	PF_ADD_FLOAT_SLIDER("Bend Handle Out 4", 0, 100, 0, 100, 0, 50, 1, 0, 0, BEND_HANDLE_OUT_4_DISK_ID);

	AEFX_CLR_STRUCT(def);
	def.ui_flags = PF_PUI_INVISIBLE;
	PF_ADD_FLOAT_SLIDER("Bend Handle In 5", 0, 100, 0, 100, 0, 50, 1, 0, 0, BEND_HANDLE_IN_5_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(LINE_GROUP_END_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_TOPIC("Trim", TRIM_GROUP_START_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Start", 0, 100, 0, 100, 0, 1, PF_ValueDisplayFlag_PERCENT, 0, START_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("End", 0, 100, 0, 100, 100, 1, PF_ValueDisplayFlag_PERCENT, 0, END_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Offset", -100, 100, -100, 100, 0, 1, PF_ValueDisplayFlag_PERCENT, 0, OFFSET_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Head Random", 0, 100, 0, 100, 0, 1, PF_ValueDisplayFlag_PERCENT, 0, TRIM_HEAD_RANDOM_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Tail Random", 0, 100, 0, 100, 0, 1, PF_ValueDisplayFlag_PERCENT, 0, TRIM_TAIL_RANDOM_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(TRIM_GROUP_END_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_TOPIC("Dash", DASH_GROUP_START_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Dash Length", 0, 1000, 0, 300, 0, 1, 0, 0, DASH_LENGTH_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Dash Gap", 0, 1000, 0, 300, 12, 1, 0, 0, DASH_GAP_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Dash Offset", -10000, 10000, -300, 300, 0, 1, 0, 0, DASH_OFFSET_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Dash Line Phase", -100, 100, -100, 100, 0, 1, PF_ValueDisplayFlag_PERCENT, 0, DASH_LINE_PHASE_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(DASH_GROUP_END_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_TOPIC("Taper", TAPER_GROUP_START_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Start Taper", 0, 100, 0, 100, 0, 1, PF_ValueDisplayFlag_PERCENT, 0, TAPER_START_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("End Taper", 0, 100, 0, 100, 0, 1, PF_ValueDisplayFlag_PERCENT, 0, TAPER_END_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Min Width", 0, 100, 0, 100, 0, 1, PF_ValueDisplayFlag_PERCENT, 0, TAPER_MIN_WIDTH_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Taper Offset", -10000, 10000, -300, 300, 0, 1, 0, 0, TAPER_OFFSET_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(TAPER_GROUP_END_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_TOPIC("Style", STYLE_GROUP_START_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_COLOR("Color", PF_MAX_CHAN8, PF_MAX_CHAN8, PF_MAX_CHAN8, COLOR_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_COLOR("End Color", PF_MAX_CHAN8, PF_MAX_CHAN8, PF_MAX_CHAN8, END_COLOR_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Line Color Amount", 0, 100, 0, 100, 0, 1, PF_ValueDisplayFlag_PERCENT, 0, LINE_COLOR_AMOUNT_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Line Color Offset", -100, 100, -100, 100, 0, 1, PF_ValueDisplayFlag_PERCENT, 0, LINE_COLOR_OFFSET_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Line Color Random", 0, 100, 0, 100, 0, 1, PF_ValueDisplayFlag_PERCENT, 0, LINE_COLOR_RANDOM_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_COLOR("Line Color 1", 255, 0, 255, LINE_COLOR_1_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_COLOR("Line Color 2", 0, 128, 255, LINE_COLOR_2_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_COLOR("Line Color 3", 0, 255, 64, LINE_COLOR_3_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_COLOR("Line Color 4", 255, 192, 0, LINE_COLOR_4_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_COLOR("Line Color 5", 0, 192, 255, LINE_COLOR_5_DISK_ID);

	AEFX_CLR_STRUCT(def);
	def.flags = PF_ParamFlag_SUPERVISE;
	def.ui_flags = PF_PUI_CONTROL | PF_PUI_DONT_ERASE_CONTROL;
	def.ui_width = LINE_COLOR_UI_WIDTH;
	def.ui_height = LINE_COLOR_UI_HEIGHT;
	PF_ADD_FLOAT_SLIDER("Line Gradient", 0, 100, 0, 100, 0, 0, 1, 0, 0, LINE_COLOR_STOP_1_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Opacity", 0, 100, 0, 100, 100, 1, PF_ValueDisplayFlag_PERCENT, 0, OPACITY_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_POPUP("Blend Mode", 5, 1, "Normal|Add|Screen|Multiply|Max", BLEND_MODE_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(STYLE_GROUP_END_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_TOPIC("Web", WEB_GROUP_START_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_POPUP("Mode", 3, 1, "Off|Add|Only", WEB_MODE_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_POINT("Target", 50, 50, FALSE, WEB_TARGET_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_POPUP("Target Mode", 2, 1, "Point|Closed Opposite", WEB_TARGET_MODE_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_POPUP("Pattern", 3, 1, "Connect|Axis Circle|Mask Random", WEB_PATTERN_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_SLIDER("Pattern Count", 0, 256, 0, 128, 24, WEB_PATTERN_COUNT_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Pattern Random", 0, 100, 0, 100, 100, 1, PF_ValueDisplayFlag_PERCENT, 0, WEB_PATTERN_RANDOM_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Amount", 0, 100, 0, 100, 100, 1, PF_ValueDisplayFlag_PERCENT, 0, WEB_AMOUNT_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Web Start", 0, 100, 0, 100, 0, 1, PF_ValueDisplayFlag_PERCENT, 0, WEB_START_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Web End", 0, 100, 0, 100, 100, 1, PF_ValueDisplayFlag_PERCENT, 0, WEB_END_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Web Offset", -100, 100, -100, 100, 0, 1, PF_ValueDisplayFlag_PERCENT, 0, WEB_OFFSET_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_POPUP("Web Direction", 2, 1, "Outer to Center|Center to Outer", WEB_DIRECTION_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Weave", 0, 100, 0, 100, 0, 1, PF_ValueDisplayFlag_PERCENT, 0, WEB_WEAVE_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_SLIDER("Weave Count", 0, 64, 0, 32, 12, WEB_WEAVE_COUNT_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Weave Random", 0, 100, 0, 100, 35, 1, PF_ValueDisplayFlag_PERCENT, 0, WEB_WEAVE_RANDOM_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(WEB_GROUP_END_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_TOPIC("Noise", NOISE_GROUP_START_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Noise Ramp", 0, 500, 0, 500, 25, 1, PF_ValueDisplayFlag_PERCENT, 0, NOISE_RAMP_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_SLIDER("Noise Seed", 0, 99999, 0, 9999, 1, NOISE_SEED_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Noise Position", 0, 2000, 0, 400, 100, 1, PF_ValueDisplayFlag_PERCENT, 0, NOISE_POSITION_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Noise Width", 0, 2000, 0, 500, 100, 1, PF_ValueDisplayFlag_PERCENT, 0, NOISE_WIDTH_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Noise Opacity", 0, 2000, 0, 500, 100, 1, PF_ValueDisplayFlag_PERCENT, 0, NOISE_OPACITY_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Noise Trim", 0, 2000, 0, 500, 100, 1, PF_ValueDisplayFlag_PERCENT, 0, NOISE_TRIM_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Noise Wave", 0, 2000, 0, 500, 100, 1, PF_ValueDisplayFlag_PERCENT, 0, NOISE_WAVE_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Line Phase", 0, 100, 0, 100, 0, 1, PF_ValueDisplayFlag_PERCENT, 0, NOISE_LINE_PHASE_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Noise Scale", 1, 1000, 1, 1000, 500, 1, 0, 0, NOISE_SCALE_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Noise Speed", -1000, 1000, -300, 300, 0, 1, 0, 0, NOISE_SPEED_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Noise Phase", -3000, 3000, -3000, 3000, 0, 1, PF_ValueDisplayFlag_PERCENT, 0, NOISE_PHASE_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(NOISE_GROUP_END_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_TOPIC("Audio Reactive", AUDIO_GROUP_START_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_LAYER("Audio Layer", PF_LayerDefault_NONE, AUDIO_LAYER_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_POPUP("Source", 2, 1, "Waveform|Spectrum", AUDIO_SOURCE_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Amount", 0, 1000, 0, 300, 0, 1, 0, 0, AUDIO_AMOUNT_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Window ms", 10, 2000, 10, 1000, 120, 1, 0, 0, AUDIO_WINDOW_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Waveform Detail", 10, 800, 10, 400, 100, 1, PF_ValueDisplayFlag_PERCENT, 0, AUDIO_DETAIL_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Line Phase", -100, 100, -100, 100, 0, 1, PF_ValueDisplayFlag_PERCENT, 0, AUDIO_LINE_PHASE_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_COLOR("Audio Color", 255, 64, 0, AUDIO_COLOR_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Audio Color Amount", 0, 100, 0, 100, 0, 1, PF_ValueDisplayFlag_PERCENT, 0, AUDIO_COLOR_AMOUNT_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(AUDIO_GROUP_END_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_TOPIC("Grunge", GRUNGE_GROUP_START_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Grunge Amount", 0, 100, 0, 100, 0, 1, PF_ValueDisplayFlag_PERCENT, 0, GRUNGE_AMOUNT_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Roughness", 0, 100, 0, 100, 50, 1, PF_ValueDisplayFlag_PERCENT, 0, GRUNGE_ROUGHNESS_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Frequency", 1, 500, 1, 200, 80, 1, 0, 0, GRUNGE_FREQUENCY_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX("Jitter", 0, 100, 0, 100, 40, 1, PF_ValueDisplayFlag_PERCENT, 0, GRUNGE_JITTER_DISK_ID);

	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(GRUNGE_GROUP_END_DISK_ID);

	if (!err) {
		PF_CustomUIInfo ci;
		AEFX_CLR_STRUCT(ci);
		ci.events = PF_CustomEFlag_EFFECT;
		ERR((*(in_data->inter.register_ui))(in_data->effect_ref, &ci));
	}

	out_data->num_params = LNA_NUM_PARAMS;
	return err;
}

static LineAnimRenderInfo InfoFromParams(PF_ParamDef *params[])
{
	LineAnimRenderInfo info;
	AEFX_CLR_STRUCT(info);
	for (A_long i = 0; i < LNA_MAX_PATHS; ++i) {
		const PF_PathID pathID = params[PATH_PARAMS[i]]->u.path_d.path_id;
		if (pathID) {
			info.path_ids[info.path_count++] = pathID;
		}
	}
	info.line_count = ClampL(params[LNA_LINE_COUNT]->u.sd.value, 1, 1024);
	info.spacing = params[LNA_SPACING]->u.fs_d.value;
	info.width = params[LNA_WIDTH]->u.fs_d.value;
	info.curve_segments = ClampL(params[LNA_CURVE_SEGMENTS]->u.sd.value, 4, 160);
	info.path_offset = params[LNA_PATH_OFFSET]->u.fs_d.value / 100.0;
	info.enable_3d = params[LNA_ENABLE_3D]->u.bd.value;
	info.shape_3d = ClampL(params[LNA_3D_SHAPE]->u.pd.value, 1, 2);
	info.position_x = params[LNA_POSITION_X]->u.fs_d.value;
	info.position_y = params[LNA_POSITION_Y]->u.fs_d.value;
	info.position_z = params[LNA_POSITION_Z]->u.fs_d.value;
	info.line_x_spacing = params[LNA_LINE_X_SPACING]->u.fs_d.value;
	info.line_y_spacing = params[LNA_LINE_Y_SPACING]->u.fs_d.value;
	info.line_z_spacing = params[LNA_LINE_Z_SPACING]->u.fs_d.value;
	info.twist = params[LNA_TWIST]->u.fs_d.value * 3.141592653589793 / 180.0;
	info.phase = params[LNA_PHASE]->u.fs_d.value * 3.141592653589793 / 180.0;
	info.twist_axis_x = 0.0;
	info.twist_axis_y = 0.0;
	info.twist_radius = 1.0;
	info.twist_depth = 1.0;
	info.sphere_z_span = 1.0;
	info.twist_loop_ramp = FALSE;
	info.tape_flatness = ClampF(params[LNA_TAPE_FLATNESS]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.tape_twist = params[LNA_TAPE_TWIST]->u.fs_d.value * 3.141592653589793 / 180.0;
	info.bend_smooth = std::max<PF_FpLong>(0.0, params[LNA_BEND_SMOOTH]->u.fs_d.value);
	info.bend_overshoot = ClampF(params[LNA_BEND_OVERSHOOT]->u.fs_d.value / 100.0, 0.0, 3.0);
	info.bend_ramp_amount = ClampF(params[LNA_BEND_RAMP_AMOUNT]->u.fs_d.value / 100.0, -1.0, 1.0);
	info.bend_ramp[0] = ClampF(params[LNA_BEND_RAMP_1]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.bend_ramp[1] = ClampF(params[LNA_BEND_RAMP_2]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.bend_ramp[2] = ClampF(params[LNA_BEND_RAMP_3]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.bend_ramp[3] = ClampF(params[LNA_BEND_RAMP_4]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.bend_ramp[4] = ClampF(params[LNA_BEND_RAMP_5]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.bend_handle_out[0] = ClampF(params[LNA_BEND_HANDLE_OUT_1]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.bend_handle_out[1] = ClampF(params[LNA_BEND_HANDLE_OUT_2]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.bend_handle_out[2] = ClampF(params[LNA_BEND_HANDLE_OUT_3]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.bend_handle_out[3] = ClampF(params[LNA_BEND_HANDLE_OUT_4]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.bend_handle_in[0]  = ClampF(params[LNA_BEND_HANDLE_IN_2]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.bend_handle_in[1]  = ClampF(params[LNA_BEND_HANDLE_IN_3]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.bend_handle_in[2]  = ClampF(params[LNA_BEND_HANDLE_IN_4]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.bend_handle_in[3]  = ClampF(params[LNA_BEND_HANDLE_IN_5]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.color = params[LNA_COLOR]->u.cd.value;
	info.end_color = params[LNA_END_COLOR]->u.cd.value;
	info.line_color_amount = ClampF(params[LNA_LINE_COLOR_AMOUNT]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.line_color_offset = params[LNA_LINE_COLOR_OFFSET]->u.fs_d.value / 100.0;
	info.line_color_random = ClampF(params[LNA_LINE_COLOR_RANDOM]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.line_colors[0] = params[LNA_LINE_COLOR_1]->u.cd.value;
	info.line_colors[1] = params[LNA_LINE_COLOR_2]->u.cd.value;
	info.line_colors[2] = params[LNA_LINE_COLOR_3]->u.cd.value;
	info.line_colors[3] = params[LNA_LINE_COLOR_4]->u.cd.value;
	info.line_colors[4] = params[LNA_LINE_COLOR_5]->u.cd.value;
	info.line_color_stops[0] = ClampF(params[LNA_LINE_COLOR_STOP_1]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.line_color_stops[1] = 0.25;
	info.line_color_stops[2] = 0.5;
	info.line_color_stops[3] = 0.75;
	info.line_color_stops[4] = 1.0;
	info.opacity = ClampF(params[LNA_OPACITY]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.blend_mode = ClampL(params[LNA_BLEND_MODE]->u.pd.value, 1, 5);
	info.start = params[LNA_START]->u.fs_d.value / 100.0;
	info.end = params[LNA_END]->u.fs_d.value / 100.0;
	info.offset = params[LNA_OFFSET]->u.fs_d.value / 100.0;
	info.trim_head_random = ClampF(params[LNA_TRIM_HEAD_RANDOM]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.trim_tail_random = ClampF(params[LNA_TRIM_TAIL_RANDOM]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.dash_length = std::max<PF_FpLong>(0.0, params[LNA_DASH_LENGTH]->u.fs_d.value);
	info.dash_gap = std::max<PF_FpLong>(0.0, params[LNA_DASH_GAP]->u.fs_d.value);
	info.dash_offset = params[LNA_DASH_OFFSET]->u.fs_d.value;
	info.dash_line_phase = params[LNA_DASH_LINE_PHASE]->u.fs_d.value / 100.0;
	info.taper_start = ClampF(params[LNA_TAPER_START]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.taper_end = ClampF(params[LNA_TAPER_END]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.taper_min_width = ClampF(params[LNA_TAPER_MIN_WIDTH]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.taper_offset = params[LNA_TAPER_OFFSET]->u.fs_d.value;
	info.web_mode = ClampL(params[LNA_WEB_MODE]->u.pd.value, 1, 3);
	info.web_target_x = static_cast<PF_FpLong>(params[LNA_WEB_TARGET]->u.td.x_value) / 65536.0;
	info.web_target_y = static_cast<PF_FpLong>(params[LNA_WEB_TARGET]->u.td.y_value) / 65536.0;
	info.web_target_mode = ClampL(params[LNA_WEB_TARGET_MODE]->u.pd.value, 1, 2);
	info.web_pattern = ClampL(params[LNA_WEB_PATTERN]->u.pd.value, 1, 3);
	info.web_pattern_count = ClampL(params[LNA_WEB_PATTERN_COUNT]->u.sd.value, 0, 256);
	info.web_pattern_random = ClampF(params[LNA_WEB_PATTERN_RANDOM]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.web_amount = ClampF(params[LNA_WEB_AMOUNT]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.web_start = ClampF(params[LNA_WEB_START]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.web_end = ClampF(params[LNA_WEB_END]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.web_offset = params[LNA_WEB_OFFSET]->u.fs_d.value / 100.0;
	info.web_direction = ClampL(params[LNA_WEB_DIRECTION]->u.pd.value, 1, 2);
	info.web_weave = ClampF(params[LNA_WEB_WEAVE]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.web_weave_count = ClampL(params[LNA_WEB_WEAVE_COUNT]->u.sd.value, 0, 64);
	info.web_weave_random = ClampF(params[LNA_WEB_WEAVE_RANDOM]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.noise_ramp = ClampF(params[LNA_NOISE_RAMP]->u.fs_d.value / 100.0, 0.0, 5.0);
	info.noise_seed = params[LNA_NOISE_SEED]->u.sd.value;
	info.noise_position = ClampF(params[LNA_NOISE_POSITION]->u.fs_d.value / 100.0, 0.0, 20.0);
	info.noise_width = ClampF(params[LNA_NOISE_WIDTH]->u.fs_d.value / 100.0, 0.0, 20.0);
	info.noise_opacity = ClampF(params[LNA_NOISE_OPACITY]->u.fs_d.value / 100.0, 0.0, 20.0);
	info.noise_trim = ClampF(params[LNA_NOISE_TRIM]->u.fs_d.value / 100.0, 0.0, 20.0);
	info.noise_wave = ClampF(params[LNA_NOISE_WAVE]->u.fs_d.value / 100.0, 0.0, 20.0);
	info.noise_line_phase = ClampF(params[LNA_NOISE_LINE_PHASE]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.noise_scale = std::max<PF_FpLong>(1.0, params[LNA_NOISE_SCALE]->u.fs_d.value);
	info.noise_speed = params[LNA_NOISE_SPEED]->u.fs_d.value;
	info.noise_phase = params[LNA_NOISE_PHASE]->u.fs_d.value / 100.0;
	info.audio_source = ClampL(params[LNA_AUDIO_SOURCE]->u.pd.value, 1, 2);
	info.audio_amount = std::max<PF_FpLong>(0.0, params[LNA_AUDIO_AMOUNT]->u.fs_d.value);
	info.audio_window_ms = ClampF(params[LNA_AUDIO_WINDOW]->u.fs_d.value, 10.0, 2000.0);
	info.audio_detail = ClampF(params[LNA_AUDIO_DETAIL]->u.fs_d.value / 100.0, 0.1, 8.0);
	info.audio_line_phase = params[LNA_AUDIO_LINE_PHASE]->u.fs_d.value / 100.0;
	info.audio_color = params[LNA_AUDIO_COLOR]->u.cd.value;
	info.audio_color_amount = ClampF(params[LNA_AUDIO_COLOR_AMOUNT]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.audio_level = 0.0;
	info.audio_sample_count = 0;
	info.effect_layer_3d = FALSE;
	info.camera_active = FALSE;
	info.camera_dist = 0.0;
	info.camera_image_width = 0;
	info.camera_image_height = 0;
	info.comp_center_x = 0.0;
	info.comp_center_y = 0.0;
	info.grunge_amount    = ClampF(params[LNA_GRUNGE_AMOUNT]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.grunge_roughness = ClampF(params[LNA_GRUNGE_ROUGHNESS]->u.fs_d.value / 100.0, 0.0, 1.0);
	info.grunge_frequency = std::max<PF_FpLong>(1.0, params[LNA_GRUNGE_FREQUENCY]->u.fs_d.value);
	info.grunge_jitter    = ClampF(params[LNA_GRUNGE_JITTER]->u.fs_d.value / 100.0, 0.0, 1.0);
	return info;
}

static PF_Err CheckoutInfoParam(PF_InData *in_data, A_long index, PF_ParamDef *param)
{
	AEFX_CLR_STRUCT(*param);
	return PF_CHECKOUT_PARAM(in_data, index, in_data->current_time, in_data->time_step, in_data->time_scale, param);
}

static PF_Err BuildInfoAtTime(PF_InData *in_data, LineAnimRenderInfo *info)
{
	PF_Err err = PF_Err_NONE;
	PF_Err err2 = PF_Err_NONE;
	PF_ParamDef p[LNA_NUM_PARAMS];
	bool checkedOut[LNA_NUM_PARAMS] = {};
	const A_long actualParams[] = {
		LNA_PATH,
		LNA_PATH_2,
		LNA_PATH_3,
		LNA_PATH_4,
		LNA_PATH_5,
		LNA_PATH_6,
		LNA_PATH_7,
		LNA_PATH_8,
		LNA_LINE_COUNT,
		LNA_SPACING,
		LNA_WIDTH,
		LNA_CURVE_SEGMENTS,
		LNA_PATH_OFFSET,
		LNA_ENABLE_3D,
		LNA_3D_SHAPE,
		LNA_POSITION_X,
		LNA_POSITION_Y,
		LNA_POSITION_Z,
		LNA_LINE_X_SPACING,
		LNA_LINE_Y_SPACING,
		LNA_LINE_Z_SPACING,
		LNA_TWIST,
		LNA_PHASE,
		LNA_TAPE_FLATNESS,
		LNA_TAPE_TWIST,
		LNA_BEND_SMOOTH,
		LNA_BEND_OVERSHOOT,
		LNA_BEND_RAMP_AMOUNT,
		LNA_BEND_RAMP_1,
		LNA_BEND_RAMP_2,
		LNA_BEND_RAMP_3,
		LNA_BEND_RAMP_4,
		LNA_BEND_RAMP_5,
		LNA_BEND_HANDLE_OUT_1,
		LNA_BEND_HANDLE_IN_2,
		LNA_BEND_HANDLE_OUT_2,
		LNA_BEND_HANDLE_IN_3,
		LNA_BEND_HANDLE_OUT_3,
		LNA_BEND_HANDLE_IN_4,
		LNA_BEND_HANDLE_OUT_4,
		LNA_BEND_HANDLE_IN_5,
		LNA_COLOR,
		LNA_END_COLOR,
		LNA_LINE_COLOR_AMOUNT,
		LNA_LINE_COLOR_OFFSET,
		LNA_LINE_COLOR_RANDOM,
		LNA_LINE_COLOR_1,
		LNA_LINE_COLOR_2,
		LNA_LINE_COLOR_3,
		LNA_LINE_COLOR_4,
		LNA_LINE_COLOR_5,
		LNA_LINE_COLOR_STOP_1,
		LNA_OPACITY,
		LNA_BLEND_MODE,
		LNA_START,
		LNA_END,
		LNA_OFFSET,
		LNA_TRIM_HEAD_RANDOM,
		LNA_TRIM_TAIL_RANDOM,
		LNA_DASH_LENGTH,
		LNA_DASH_GAP,
		LNA_DASH_OFFSET,
		LNA_DASH_LINE_PHASE,
		LNA_TAPER_START,
		LNA_TAPER_END,
		LNA_TAPER_MIN_WIDTH,
		LNA_TAPER_OFFSET,
		LNA_WEB_MODE,
		LNA_WEB_TARGET,
		LNA_WEB_TARGET_MODE,
		LNA_WEB_PATTERN,
		LNA_WEB_PATTERN_COUNT,
		LNA_WEB_PATTERN_RANDOM,
		LNA_WEB_AMOUNT,
		LNA_WEB_START,
		LNA_WEB_END,
		LNA_WEB_OFFSET,
		LNA_WEB_DIRECTION,
		LNA_WEB_WEAVE,
		LNA_WEB_WEAVE_COUNT,
		LNA_WEB_WEAVE_RANDOM,
		LNA_NOISE_RAMP,
		LNA_NOISE_SEED,
		LNA_NOISE_POSITION,
		LNA_NOISE_WIDTH,
		LNA_NOISE_OPACITY,
		LNA_NOISE_TRIM,
		LNA_NOISE_WAVE,
		LNA_NOISE_LINE_PHASE,
		LNA_NOISE_SCALE,
		LNA_NOISE_SPEED,
		LNA_NOISE_PHASE,
		LNA_AUDIO_SOURCE,
		LNA_AUDIO_AMOUNT,
		LNA_AUDIO_WINDOW,
		LNA_AUDIO_DETAIL,
		LNA_AUDIO_LINE_PHASE,
		LNA_AUDIO_COLOR,
		LNA_AUDIO_COLOR_AMOUNT,
		LNA_GRUNGE_AMOUNT,
		LNA_GRUNGE_ROUGHNESS,
		LNA_GRUNGE_FREQUENCY,
		LNA_GRUNGE_JITTER
	};

	for (A_long index : actualParams) {
		if (!err) {
			ERR(CheckoutInfoParam(in_data, index, &p[index]));
			if (!err) {
				checkedOut[index] = true;
			}
		}
	}

	if (!err) {
		PF_ParamDef *pp[LNA_NUM_PARAMS] = {};
		for (A_long index : actualParams) {
			pp[index] = &p[index];
		}
		*info = InfoFromParams(pp);
		ERR(BuildAudioReactiveData(in_data, info));
	}

	for (A_long index : actualParams) {
		if (checkedOut[index]) {
			ERR2(PF_CHECKIN_PARAM(in_data, &p[index]));
		}
	}
	return err ? err : err2;
}

static void AnalyzeAudioSamples(const PF_FpShort *samples, A_long sampleCount, LineAnimRenderInfo *info)
{
	info->audio_sample_count = 0;
	info->audio_level = 0.0;
	for (A_long i = 0; i < LNA_AUDIO_WAVE_SAMPLES; ++i) {
		info->audio_wave[i] = 0.0;
	}

	if (!samples || sampleCount <= 0) {
		return;
	}

	PF_FpLong sumSq = 0.0;
	for (A_long i = 0; i < sampleCount; ++i) {
		const PF_FpLong v = ClampF(static_cast<PF_FpLong>(samples[i]), -1.0, 1.0);
		sumSq += v * v;
	}
	info->audio_level = std::sqrt(sumSq / static_cast<PF_FpLong>(sampleCount));
	info->audio_sample_count = LNA_AUDIO_WAVE_SAMPLES;

	if (info->audio_source == 2) {
		const A_long dftCount = std::max<A_long>(1, std::min<A_long>(sampleCount, 4096));
		const A_long offset = std::max<A_long>(0, (sampleCount - dftCount) / 2);
		PF_FpLong windowSum = 0.0;
		const PF_FpLong minFreq = 20.0;
		const PF_FpLong maxFreq = 16000.0;
		const PF_FpLong sampleRate = 44100.0;
		const PF_FpLong freqRatio = maxFreq / minFreq;

		for (A_long i = 0; i < dftCount; ++i) {
			const PF_FpLong w = 0.5 - 0.5 * std::cos((6.283185307179586 * static_cast<PF_FpLong>(i)) / static_cast<PF_FpLong>(std::max<A_long>(1, dftCount - 1)));
			windowSum += w;
		}
		windowSum = std::max<PF_FpLong>(1.0e-6, windowSum);

		for (A_long bin = 0; bin < LNA_AUDIO_WAVE_SAMPLES; ++bin) {
			const PF_FpLong u = static_cast<PF_FpLong>(bin) / static_cast<PF_FpLong>(LNA_AUDIO_WAVE_SAMPLES - 1);
			const PF_FpLong freq = minFreq * std::pow(freqRatio, u);
			const PF_FpLong phaseStep = 6.283185307179586 * freq / sampleRate;
			PF_FpLong re = 0.0;
			PF_FpLong im = 0.0;

			for (A_long i = 0; i < dftCount; ++i) {
				const PF_FpLong w = 0.5 - 0.5 * std::cos((6.283185307179586 * static_cast<PF_FpLong>(i)) / static_cast<PF_FpLong>(std::max<A_long>(1, dftCount - 1)));
				const PF_FpLong v = ClampF(static_cast<PF_FpLong>(samples[offset + i]), -1.0, 1.0) * w;
				const PF_FpLong phase = phaseStep * static_cast<PF_FpLong>(i);
				re += v * std::cos(phase);
				im -= v * std::sin(phase);
			}

			const PF_FpLong magnitude = std::sqrt(re * re + im * im) * 2.0 / windowSum;
			info->audio_wave[bin] = ClampF(std::log1p(magnitude * 24.0) / std::log1p(24.0), 0.0, 1.0);
		}
		return;
	}

	for (A_long bin = 0; bin < LNA_AUDIO_WAVE_SAMPLES; ++bin) {
		const A_long start = static_cast<A_long>(
			(static_cast<PF_FpLong>(bin) * static_cast<PF_FpLong>(sampleCount)) /
			static_cast<PF_FpLong>(LNA_AUDIO_WAVE_SAMPLES));
		const A_long end = std::max<A_long>(
			start + 1,
			static_cast<A_long>(
				(static_cast<PF_FpLong>(bin + 1) * static_cast<PF_FpLong>(sampleCount)) /
				static_cast<PF_FpLong>(LNA_AUDIO_WAVE_SAMPLES)));
		PF_FpLong signedPeak = 0.0;
		for (A_long i = start; i < std::min(end, sampleCount); ++i) {
			const PF_FpLong v = ClampF(static_cast<PF_FpLong>(samples[i]), -1.0, 1.0);
			if (std::fabs(v) > std::fabs(signedPeak)) {
				signedPeak = v;
			}
		}
		info->audio_wave[bin] = signedPeak;
	}
}

static PF_Err BuildAudioReactiveData(PF_InData *in_data, LineAnimRenderInfo *info)
{
	if (!info || info->audio_amount <= 1.0e-6 || in_data->time_scale <= 0) {
		return PF_Err_NONE;
	}

	PF_Err err = PF_Err_NONE;
	PF_Err err2 = PF_Err_NONE;
	PF_LayerAudio audio = NULL;
	PF_SndSamplePtr data = NULL;
	A_long sampleCount = 0;
	const A_long duration = std::max<A_long>(
		1,
		static_cast<A_long>(
			std::ceil(static_cast<PF_FpLong>(in_data->time_scale) * info->audio_window_ms / 1000.0)));
	const A_long start = in_data->current_time - duration / 2;

	err = PF_CHECKOUT_LAYER_AUDIO(
		in_data,
		LNA_AUDIO_LAYER,
		start,
		duration,
		in_data->time_scale,
		AUDIO_SAMPLE_RATE,
		PF_SSS_4,
		PF_Channels_MONO,
		PF_SIGNED_FLOAT,
		&audio);

	if (err || !audio) {
		return PF_Err_NONE;
	}

	ERR(PF_GET_AUDIO_DATA(in_data, audio, &data, &sampleCount, NULL, NULL, NULL, NULL));
	if (!err && sampleCount > 0) {
		--sampleCount;
		AnalyzeAudioSamples(reinterpret_cast<const PF_FpShort *>(data), sampleCount, info);
	}

	ERR2(PF_CHECKIN_LAYER_AUDIO(in_data, audio));
	return err ? PF_Err_NONE : err2;
}
