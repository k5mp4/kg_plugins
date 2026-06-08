#include "EffectStub.h"

#include "GridMath.h"
#include "JsonStorage.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <vector>

namespace {

enum class EffectAction {
	ApplyGuides,
	LockGuides,
	UnlockGuides,
	ClearAll
};

enum DisplayMode {
	DISPLAY_MODE_BANDS = 1,
	DISPLAY_MODE_GUIDE_LINES = 2
};

enum GuideMode {
	GUIDE_MODE_GRID = 1,
	GUIDE_MODE_GOLDEN_RATIO = 2,
	GUIDE_MODE_SILVER_RATIO = 3,
	GUIDE_MODE_BRONZE_RATIO = 4,
	GUIDE_MODE_RATIO_RECTS = 5,
	GUIDE_MODE_GOLDEN_SPIRAL = 6,
	GUIDE_MODE_RATIO_RECTS_AND_SPIRAL = 7
};

static const char* actionName(EffectAction action)
{
	switch (action) {
	case EffectAction::ApplyGuides: return "applyGuides";
	case EffectAction::LockGuides: return "lockGuides";
	case EffectAction::UnlockGuides: return "unlockGuides";
	case EffectAction::ClearAll: return "clearAll";
	default: return "applyGuides";
	}
}

static int paramInt(PF_ParamDef* params[], int index, int fallback)
{
	if (!params || !params[index]) {
		return fallback;
	}
	return std::max(1, static_cast<int>(params[index]->u.fs_d.value + 0.5));
}

static double paramDouble(PF_ParamDef* params[], int index, double fallback)
{
	if (!params || !params[index]) {
		return fallback;
	}
	return std::max(0.0, static_cast<double>(params[index]->u.fs_d.value));
}

static int popupValue(PF_ParamDef* params[], int index, int fallback)
{
	if (!params || !params[index]) {
		return fallback;
	}
	return std::max(1, static_cast<int>(params[index]->u.pd.value));
}

static aeflg::GridSettings settingsFromParams(PF_ParamDef* params[])
{
	aeflg::GridSettings settings;
	settings.columns.count = paramInt(params, KG_LAYOUTGRID_COL_COUNT, settings.columns.count);
	settings.columns.marginLeft = paramDouble(params, KG_LAYOUTGRID_COL_MARGIN, settings.columns.marginLeft);
	settings.columns.marginRight = settings.columns.marginLeft;
	settings.columns.gutter = paramDouble(params, KG_LAYOUTGRID_COL_GUTTER, settings.columns.gutter);
	settings.rows.count = paramInt(params, KG_LAYOUTGRID_ROW_COUNT, settings.rows.count);
	settings.rows.marginTop = paramDouble(params, KG_LAYOUTGRID_ROW_MARGIN, settings.rows.marginTop);
	settings.rows.marginBottom = settings.rows.marginTop;
	settings.rows.gutter = paramDouble(params, KG_LAYOUTGRID_ROW_GUTTER, settings.rows.gutter);
	return settings;
}

static int lineWidthFromParams(PF_ParamDef* params[])
{
	return std::max(1, std::min(64, static_cast<int>(paramDouble(params, KG_LAYOUTGRID_LINE_WIDTH, 2.0) + 0.5)));
}

static int bandOpacityFromParams(PF_ParamDef* params[])
{
	return std::max(1, std::min(255, static_cast<int>(paramDouble(params, KG_LAYOUTGRID_BAND_OPACITY, 48.0) + 0.5)));
}

static int displayModeFromParams(PF_ParamDef* params[])
{
	return popupValue(params, KG_LAYOUTGRID_DISPLAY_MODE, DISPLAY_MODE_BANDS);
}

static int guideModeFromParams(PF_ParamDef* params[])
{
	return popupValue(params, KG_LAYOUTGRID_GUIDE_MODE, GUIDE_MODE_GRID);
}

static double ratioForGuideMode(int guideMode)
{
	switch (guideMode) {
	case GUIDE_MODE_GOLDEN_RATIO:
		return 1.6180339887498948482;
	case GUIDE_MODE_SILVER_RATIO:
		return 2.4142135623730950488;
	case GUIDE_MODE_BRONZE_RATIO:
		return (3.0 + std::sqrt(13.0)) / 2.0;
	default:
		return 0.0;
	}
}

static double visualRatioForMode(int guideMode)
{
	const double ratio = ratioForGuideMode(guideMode);
	return ratio > 0.0 ? ratio : ratioForGuideMode(GUIDE_MODE_GOLDEN_RATIO);
}

static std::vector<aeflg::GridGuide> ratioGuides(double compWidth, double compHeight, int guideMode)
{
	std::vector<aeflg::GridGuide> guides;
	const double ratio = ratioForGuideMode(guideMode);
	if (ratio <= 0.0) {
		return guides;
	}

	const double x = compWidth / (ratio + 1.0);
	const double y = compHeight / (ratio + 1.0);
	guides.push_back({ 1, static_cast<int>(std::llround(x)) });
	guides.push_back({ 1, static_cast<int>(std::llround(compWidth - x)) });
	guides.push_back({ 0, static_cast<int>(std::llround(y)) });
	guides.push_back({ 0, static_cast<int>(std::llround(compHeight - y)) });
	return guides;
}

static std::vector<aeflg::GridGuide> guidesForMode(double compWidth, double compHeight, const aeflg::GridSettings& settings, int guideMode)
{
	if (guideMode == GUIDE_MODE_GRID) {
		return aeflg::calculateAllGuides(compWidth, compHeight, settings);
	}
	return ratioGuides(compWidth, compHeight, guideMode);
}

static void appendDebugLog(const std::string& text)
{
	std::ofstream log("C:\\tmp\\kg_LayoutGrid_debug.log", std::ios::app);
	if (!log) {
		return;
	}
	std::time_t now = std::time(nullptr);
	log << "\n[" << static_cast<long long>(now) << "] " << text << "\n";
}

static void writeLastScript(const std::string& script)
{
	std::ofstream file("C:\\tmp\\kg_LayoutGrid_last_script.jsx", std::ios::trunc);
	if (file) {
		file << script;
	}
	std::ofstream numbered("C:\\tmp\\kg_LayoutGrid_last_script_numbered.txt", std::ios::trunc);
	if (numbered) {
		std::istringstream lines(script);
		std::string line;
		int lineNo = 1;
		while (std::getline(lines, line)) {
			numbered << lineNo++ << ": " << line << "\n";
		}
	}
}

static std::string memHandleToString(AEGP_SuiteHandler& suites, AEGP_MemHandle handle)
{
	if (!handle) {
		return std::string();
	}

	AEGP_MemSize size = 0;
	if (suites.MemorySuite1()->AEGP_GetMemHandleSize(handle, &size) || size <= 0) {
		return std::string();
	}

	void* ptr = nullptr;
	if (suites.MemorySuite1()->AEGP_LockMemHandle(handle, &ptr) || !ptr) {
		return std::string();
	}

	std::string value(reinterpret_cast<const char*>(ptr), reinterpret_cast<const char*>(ptr) + size);
	while (!value.empty() && value.back() == '\0') {
		value.pop_back();
	}
	suites.MemorySuite1()->AEGP_UnlockMemHandle(handle);
	return value;
}

static void freeMemHandle(AEGP_SuiteHandler& suites, AEGP_MemHandle handle)
{
	if (handle) {
		suites.MemorySuite1()->AEGP_FreeMemHandle(handle);
	}
}

static std::string buildEffectScript(EffectAction action, const aeflg::GridSettings& settings, int guideMode)
{
	const std::string json = aeflg::settingsToJson(settings);
	std::ostringstream js;
	js << "(function(){\n";
	js << "var ACTION=\"" << actionName(action) << "\";\n";
	js << "var SETTINGS=" << json << ";\n";
	js << "var GUIDE_MODE=" << guideMode << ";\n";
	js << "function dbg(m){try{var f=new File('C:/tmp/kg_LayoutGrid_jsx_debug.log');if(f.open('a')){f.writeln((new Date()).toString()+' '+m);f.close();}}catch(e){}}\n";
	js << "dbg('start action='+ACTION+' guideMode='+GUIDE_MODE);\n";
	js << "function fail(m){alert('kg_LayoutGrid: '+m);throw m;}\n";
	js << "function comp(){dbg('comp');var c=app.project.activeItem;if(!c || c.numLayers===undefined)fail('Active item is not a composition.');return c;}\n";
	js << "function hasKgEffect(layer){try{var fx=layer.property('ADBE Effect Parade');if(!fx)return false;for(var i=1;i<=fx.numProperties;i++){var e=fx.property(i);if(e&&(e.matchName==='kg_LayoutGrid'||e.name==='kg_LayoutGrid'))return true;}}catch(e){}return false;}\n";
	js << "function controller(c){dbg('controller');var layers=null;try{layers=c.selectedLayers;}catch(e){layers=null;}if(layers&&layers.length){for(var i=0;i<layers.length;i++){if(hasKgEffect(layers[i]))return layers[i];}return layers[0];}for(var j=1;j<=c.numLayers;j++){var layer=c.layer(j);if(hasKgEffect(layer))return layer;}fail('Select the layer with kg_LayoutGrid applied.');}\n";
	js << "function propValue(layer,n,fb){try{var p=layer.property('ADBE Transform Group').property(n);if(p)return p.value;}catch(e){}return fb;}\n";
	js << "function layerBounds(c,layer){dbg('layerBounds');var w=0;var h=0;try{if(layer.source){w=layer.source.width;h=layer.source.height;}}catch(e){}if(!w||!h){try{var sr=layer.sourceRectAtTime(c.time,false);w=sr.width;h=sr.height;}catch(e2){}}if(!w||!h){w=c.width;h=c.height;}var pos=propValue(layer,'ADBE Position',[c.width/2,c.height/2,0]);var anc=propValue(layer,'ADBE Anchor Point',[w/2,h/2,0]);var scl=propValue(layer,'ADBE Scale',[100,100,100]);var rot=propValue(layer,'ADBE Rotate Z',0);try{var rz=layer.property('ADBE Transform Group').property('ADBE Rotation');if(rz)rot=rz.value;}catch(er){}var sx=scl[0]/100.0;var sy=scl[1]/100.0;var rad=rot*3.141592653589793/180.0;var cs=Math.cos(rad);var sn=Math.sin(rad);function xform(x,y){var dx=(x-anc[0])*sx;var dy=(y-anc[1])*sy;return [pos[0]+dx*cs-dy*sn,pos[1]+dx*sn+dy*cs];}var pts=[xform(0,0),xform(w,0),xform(w,h),xform(0,h)];var l=pts[0][0],r=pts[0][0],t=pts[0][1],b=pts[0][1];for(var i=1;i<pts.length;i++){if(pts[i][0]<l)l=pts[i][0];if(pts[i][0]>r)r=pts[i][0];if(pts[i][1]<t)t=pts[i][1];if(pts[i][1]>b)b=pts[i][1];}l=Math.max(0,Math.min(c.width,l));r=Math.max(0,Math.min(c.width,r));t=Math.max(0,Math.min(c.height,t));b=Math.max(0,Math.min(c.height,b));return {left:l,top:t,width:r-l,height:b-t};}\n";
	js << "function uniqueGuides(gs){var out=[];var seen={};for(var i=0;i<gs.length;i++){var p=Math.round(gs[i].position);var k=gs[i].orientation+':'+p;if(!seen[k]){seen[k]=true;out.push({orientation:gs[i].orientation,position:p});}}return out;}\n";
	js << "function ratioGuides(bounds,m){dbg('ratioGuides mode='+m);var r=(m===3)?2.414213562373095:(m===4)?3.302775637731995:1.618033988749895;var x=bounds.width/(r+1);var y=bounds.height/(r+1);return uniqueGuides([{orientation:1,position:bounds.left+x},{orientation:1,position:bounds.left+bounds.width-x},{orientation:0,position:bounds.top+y},{orientation:0,position:bounds.top+bounds.height-y}]);}\n";
	js << "function rectGuides(r){return [{orientation:1,position:r.left},{orientation:1,position:r.right},{orientation:0,position:r.top},{orientation:0,position:r.bottom}];}\n";
	js << "function goldenSquareGuides(bounds){dbg('goldenSquareGuides');var ratio=1.618033988749895;var compRatio=bounds.width/Math.max(1,bounds.height);var fit;if(compRatio>=ratio){var rw=bounds.height*ratio;fit={left:bounds.left+(bounds.width-rw)*0.5,top:bounds.top,right:bounds.left+(bounds.width-rw)*0.5+rw,bottom:bounds.top+bounds.height};}else{var rh=bounds.width/ratio;fit={left:bounds.left,top:bounds.top+(bounds.height-rh)*0.5,right:bounds.left+bounds.width,bottom:bounds.top+(bounds.height-rh)*0.5+rh};}var g=rectGuides(fit);var rect=fit;for(var i=0;i<10;i++){var w=rect.right-rect.left;var h=rect.bottom-rect.top;if(w<2||h<2)break;var side=Math.min(w,h);var sq;if(i%4===0){sq={left:rect.left,top:rect.top,right:rect.left+side,bottom:rect.top+side};rect.left+=side;}else if(i%4===1){sq={left:rect.left,top:rect.top,right:rect.left+side,bottom:rect.top+side};rect.top+=side;}else if(i%4===2){sq={left:rect.right-side,top:rect.top,right:rect.right,bottom:rect.top+side};rect.right-=side;}else{sq={left:rect.left,top:rect.bottom-side,right:rect.left+side,bottom:rect.bottom};rect.bottom-=side;}g=g.concat(rectGuides(sq));}return uniqueGuides(g);}\n";
	js << "function calc(c,s,bounds){\n";
	js << "dbg('calc');\n";
	js << "if(GUIDE_MODE===5||GUIDE_MODE===6||GUIDE_MODE===7)return goldenSquareGuides(bounds);\n";
	js << "if(GUIDE_MODE!==1)return ratioGuides(bounds,GUIDE_MODE);\n";
	js << "var g=[];\n";
	js << "var cw=(bounds.width-s.columns.marginLeft-s.columns.marginRight-s.columns.gutter*(s.columns.count-1))/s.columns.count;\n";
	js << "var rh=(bounds.height-s.rows.marginTop-s.rows.marginBottom-s.rows.gutter*(s.rows.count-1))/s.rows.count;\n";
	js << "if(cw<=0)fail('Column width is 0 or negative.');\n";
	js << "if(rh<=0)fail('Row height is 0 or negative.');\n";
	js << "for(var i=0;i<s.columns.count;i++){var l=bounds.left+s.columns.marginLeft+i*(cw+s.columns.gutter);g.push({orientation:1,position:l});g.push({orientation:1,position:l+cw});}\n";
	js << "for(var r=0;r<s.rows.count;r++){var t=bounds.top+s.rows.marginTop+r*(rh+s.rows.gutter);g.push({orientation:0,position:t});g.push({orientation:0,position:t+rh});}\n";
	js << "return uniqueGuides(g);}\n";
	js << "function validate(c,s,bounds){dbg('validate');if(s.columns.count<1||s.rows.count<1)fail('Count must be 1 or greater.');if(bounds.width<1||bounds.height<1)fail('Layer bounds are empty.');var vals=[s.columns.marginLeft,s.columns.marginRight,s.columns.gutter,s.rows.marginTop,s.rows.marginBottom,s.rows.gutter];for(var i=0;i<vals.length;i++){if(vals[i]<0||isNaN(vals[i]))fail('Margins and gutters must be 0 or greater.');}calc(c,s,bounds);}\n";
	js << "function readData(layer){dbg('readData');if(!layer.comment)return SETTINGS;try{var d=eval('('+layer.comment+')');return d&&d.plugin==='AE_Figma_Layout_Grid_AEX'?d:SETTINGS;}catch(e){dbg('readData fallback '+(e.message||e.toString()));return SETTINGS;}}\n";
	js << "function writeData(layer,s){dbg('writeData guides='+s.generatedGuides.length);var a=[];for(var i=0;i<s.generatedGuides.length;i++){a.push('{\"orientation\":'+s.generatedGuides[i].orientation+',\"position\":'+s.generatedGuides[i].position+'}');}layer.comment='{\"plugin\":\"AE_Figma_Layout_Grid_AEX\",\"version\":\"0.1.0\",\"columns\":{\"count\":'+s.columns.count+',\"marginLeft\":'+s.columns.marginLeft+',\"marginRight\":'+s.columns.marginRight+',\"gutter\":'+s.columns.gutter+'},\"rows\":{\"count\":'+s.rows.count+',\"marginTop\":'+s.rows.marginTop+',\"marginBottom\":'+s.rows.marginBottom+',\"gutter\":'+s.rows.gutter+'},\"generatedGuides\":['+a.join(',')+']}';}\n";
	js << "function guideCount(c){try{return c.guides?c.guides.length:0;}catch(e){return 0;}}\n";
	js << "function removeAllGuides(c){dbg('removeAllGuides');for(var guard=0;guard<10000&&guideCount(c)>0;guard++){c.removeGuide(guideCount(c)-1);}}\n";
	js << "function removeGuides(c,gs){dbg('removeGuides count='+gs.length);for(var pass=0;pass<2;pass++){for(var i=guideCount(c)-1;i>=0;i--){var gd=c.guides[i];for(var j=0;j<gs.length;j++){if(gd.orientationType===gs[j].orientation&&Math.round(gd.position)===Math.round(gs[j].position)){c.removeGuide(i);break;}}}}}\n";
	js << "function apply(c,layer,s){dbg('apply');var old=readData(layer);removeGuides(c,old.generatedGuides||[]);var b=layerBounds(c,layer);var ng=calc(c,s,b);dbg('addGuides count='+ng.length);for(var i=0;i<ng.length;i++){if((ng[i].orientation===1&&ng[i].position>=0&&ng[i].position<=c.width)||(ng[i].orientation===0&&ng[i].position>=0&&ng[i].position<=c.height)){c.addGuide(ng[i].orientation,ng[i].position);}}s.generatedGuides=ng;writeData(layer,s);}\n";
	js << "function setGuidesLocked(c,locked){dbg('setGuidesLocked '+locked);var v=null;try{v=app.activeViewer;}catch(e){}try{if(!v||!v.views||v.views.length<1)v=c.openInViewer();}catch(e2){}if(!v||!v.views||v.views.length<1)fail('Composition viewer is not available.');v.views[0].options.guidesLocked=locked;}\n";
	js << "app.beginUndoGroup('kg_LayoutGrid');try{var c=comp();var l=null;if(ACTION==='applyGuides'){l=controller(c);var b=layerBounds(c,l);validate(c,SETTINGS,b);apply(c,l,SETTINGS);}else if(ACTION==='lockGuides'){setGuidesLocked(c,true);}else if(ACTION==='unlockGuides'){setGuidesLocked(c,false);}else if(ACTION==='clearAll'){removeAllGuides(c);try{l=controller(c);l.comment='';}catch(ignore){}}dbg('done');}catch(e){dbg('error '+(e.message||e.toString()));alert('kg_LayoutGrid: '+(e.message||e.toString()));throw e;}finally{app.endUndoGroup();}\n";
	js << "})();\n";
	return js.str();
}

static KgLayoutGridGlobal* lockGlobal(PF_InData* in_data, AEGP_SuiteHandler& suites)
{
	if (!in_data || !in_data->global_data) {
		return nullptr;
	}
	return reinterpret_cast<KgLayoutGridGlobal*>(suites.HandleSuite1()->host_lock_handle(in_data->global_data));
}

static void unlockGlobal(PF_InData* in_data, AEGP_SuiteHandler& suites)
{
	if (in_data && in_data->global_data) {
		suites.HandleSuite1()->host_unlock_handle(in_data->global_data);
	}
}

static PF_Err executeAction(PF_InData* in_data, PF_ParamDef* params[], EffectAction action)
{
	PF_Err err = PF_Err_NONE;
	if (!in_data || !in_data->pica_basicP) {
		return PF_Err_BAD_CALLBACK_PARAM;
	}

	AEGP_SuiteHandler suites(in_data->pica_basicP);
	KgLayoutGridGlobal* global = lockGlobal(in_data, suites);
	if (!global || !global->plugin_id) {
		if (global) {
			unlockGlobal(in_data, suites);
		}
		return PF_Err_BAD_CALLBACK_PARAM;
	}

	const auto settings = settingsFromParams(params);
	const int guideMode = guideModeFromParams(params);
	const std::string script = buildEffectScript(action, settings, guideMode);
	writeLastScript(script);
	appendDebugLog(std::string("ExecuteScript action=") + actionName(action) + " guideMode=" + std::to_string(guideMode) + " settings=" + aeflg::settingsToJson(settings));

	AEGP_MemHandle resultH = nullptr;
	AEGP_MemHandle errorH = nullptr;
	ERR(suites.UtilitySuite5()->AEGP_ExecuteScript(global->plugin_id, script.c_str(), FALSE, &resultH, &errorH));
	const std::string result = memHandleToString(suites, resultH);
	const std::string errorText = memHandleToString(suites, errorH);
	appendDebugLog("ExecuteScript err=" + std::to_string(err) + " result=" + result + " error=" + errorText);
	freeMemHandle(suites, resultH);
	freeMemHandle(suites, errorH);
	unlockGlobal(in_data, suites);
	return err;
}

static PF_Err About(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef**, PF_LayerDef*)
{
	PF_SPRINTF(
		out_data->return_msg,
		"%s v%d.%d\rUse Window > kg_LayoutGrid... to create layout grids.",
		KG_LAYOUTGRID_EFFECT_NAME,
		KG_LAYOUTGRID_MAJOR_VERSION,
		KG_LAYOUTGRID_MINOR_VERSION);
	(void)in_data;
	return PF_Err_NONE;
}

static PF_Err GlobalSetup(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef**, PF_LayerDef*)
{
	PF_Err err = PF_Err_NONE;
	out_data->my_version = KG_LAYOUTGRID_EFFECT_VERSION;
	out_data->out_flags = PF_OutFlag_PIX_INDEPENDENT | PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_NON_PARAM_VARY;
	out_data->out_flags2 = PF_OutFlag2_SUPPORTS_THREADED_RENDERING;

	AEGP_SuiteHandler suites(in_data->pica_basicP);
	PF_Handle globalH = suites.HandleSuite1()->host_new_handle(sizeof(KgLayoutGridGlobal));
	if (!globalH) {
		return PF_Err_OUT_OF_MEMORY;
	}

	KgLayoutGridGlobal* global = reinterpret_cast<KgLayoutGridGlobal*>(suites.HandleSuite1()->host_lock_handle(globalH));
	if (!global) {
		return PF_Err_OUT_OF_MEMORY;
	}

	global->plugin_id = 0;
	ERR(suites.UtilitySuite3()->AEGP_RegisterWithAEGP(nullptr, KG_LAYOUTGRID_EFFECT_NAME, &global->plugin_id));
	suites.HandleSuite1()->host_unlock_handle(globalH);
	if (!err) {
		out_data->global_data = globalH;
	}
	return err;
}

static PF_Err GlobalSetdown(PF_InData* in_data, PF_OutData*, PF_ParamDef**, PF_LayerDef*)
{
	if (in_data && in_data->global_data && in_data->pica_basicP) {
		AEGP_SuiteHandler suites(in_data->pica_basicP);
		suites.HandleSuite1()->host_dispose_handle(in_data->global_data);
	}
	return PF_Err_NONE;
}

static PF_Err ParamsSetup(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef**, PF_LayerDef*)
{
	PF_ParamDef def;
	AEFX_CLR_STRUCT(def);

	PF_ADD_TOPIC("Columns", COLUMNS_TOPIC_START_DISK_ID);
	PF_ADD_FLOAT_SLIDERX("Columns Count", 1, 64, 1, 24, 12, 0, 0, PF_ParamFlag_SUPERVISE, COL_COUNT_DISK_ID);
	PF_ADD_FLOAT_SLIDERX("Margin Left / Right", 0, 10000, 0, 500, 80, 1, 0, PF_ParamFlag_SUPERVISE, COL_MARGIN_DISK_ID);
	PF_ADD_FLOAT_SLIDERX("Column Gutter", 0, 10000, 0, 500, 24, 1, 0, PF_ParamFlag_SUPERVISE, COL_GUTTER_DISK_ID);
	PF_END_TOPIC(COLUMNS_TOPIC_END_DISK_ID);

	PF_ADD_TOPIC("Rows", ROWS_TOPIC_START_DISK_ID);
	PF_ADD_FLOAT_SLIDERX("Rows Count", 1, 64, 1, 24, 12, 0, 0, PF_ParamFlag_SUPERVISE, ROW_COUNT_DISK_ID);
	PF_ADD_FLOAT_SLIDERX("Margin Top / Bottom", 0, 10000, 0, 500, 80, 1, 0, PF_ParamFlag_SUPERVISE, ROW_MARGIN_DISK_ID);
	PF_ADD_FLOAT_SLIDERX("Row Gutter", 0, 10000, 0, 500, 24, 1, 0, PF_ParamFlag_SUPERVISE, ROW_GUTTER_DISK_ID);
	PF_END_TOPIC(ROWS_TOPIC_END_DISK_ID);

	PF_ADD_TOPIC("Style", STYLE_TOPIC_START_DISK_ID);
	PF_ADD_POPUP("Display Mode", 2, DISPLAY_MODE_BANDS, "Bands|Guide Lines", DISPLAY_MODE_DISK_ID);
	PF_ADD_FLOAT_SLIDERX("Line Width", 1, 64, 1, 16, 2, 1, 0, 0, LINE_WIDTH_DISK_ID);
	PF_ADD_FLOAT_SLIDERX("Band Opacity", 1, 255, 1, 160, 48, 0, 0, 0, BAND_OPACITY_DISK_ID);
	PF_ADD_COLOR("Column Color", 255, 140, 140, COLUMN_COLOR_DISK_ID);
	PF_ADD_COLOR("Row Color", 255, 170, 170, ROW_COLOR_DISK_ID);
	PF_ADD_COLOR("Spiral Color", 255, 210, 64, SPIRAL_COLOR_DISK_ID);
	PF_END_TOPIC(STYLE_TOPIC_END_DISK_ID);

	PF_ADD_TOPIC("Guides", GUIDES_TOPIC_START_DISK_ID);
	PF_ADD_POPUP("Guide Mode", 7, GUIDE_MODE_GRID, "Grid|Golden Ratio|Silver Ratio|Bronze Ratio|Ratio Rectangles|Golden Spiral|Ratio Rect + Spiral", GUIDE_MODE_DISK_ID);
	PF_ADD_BUTTON("Apply Guides", "Apply Guides", 0, PF_ParamFlag_SUPERVISE, APPLY_GUIDES_DISK_ID);
	PF_ADD_BUTTON("Lock Guides", "Lock Guides", 0, PF_ParamFlag_SUPERVISE, LOCK_GUIDES_DISK_ID);
	PF_ADD_BUTTON("Unlock Guides", "Unlock Guides", 0, PF_ParamFlag_SUPERVISE, UNLOCK_GUIDES_DISK_ID);
	PF_ADD_BUTTON("Clear All", "Clear All", 0, PF_ParamFlag_SUPERVISE, CLEAR_ALL_DISK_ID);
	PF_END_TOPIC(GUIDES_TOPIC_END_DISK_ID);

	out_data->num_params = KG_LAYOUTGRID_NUM_PARAMS;
	return PF_Err_NONE;
}

static PF_Pixel colorParam(PF_ParamDef* params[], int index, PF_Pixel fallback)
{
	if (!params || !params[index]) {
		return fallback;
	}
	PF_Pixel p = params[index]->u.cd.value;
	p.alpha = PF_MAX_CHAN8;
	return p;
}

static void blendPixel8(PF_Pixel8& dst, const PF_Pixel& src, A_long opacity)
{
	const A_long addA = std::max<A_long>(0, std::min<A_long>(PF_MAX_CHAN8, opacity));
	dst.alpha = static_cast<A_u_char>(std::min<A_long>(PF_MAX_CHAN8, dst.alpha + addA));
	dst.red = static_cast<A_u_char>(std::min<A_long>(PF_MAX_CHAN8, dst.red + (src.red * addA) / PF_MAX_CHAN8));
	dst.green = static_cast<A_u_char>(std::min<A_long>(PF_MAX_CHAN8, dst.green + (src.green * addA) / PF_MAX_CHAN8));
	dst.blue = static_cast<A_u_char>(std::min<A_long>(PF_MAX_CHAN8, dst.blue + (src.blue * addA) / PF_MAX_CHAN8));
}

static void blendPixel16(PF_Pixel16& dst, const PF_Pixel& src, A_long opacity)
{
	const A_long addA8 = std::max<A_long>(0, std::min<A_long>(PF_MAX_CHAN8, opacity));
	const A_long addA16 = addA8 * 257;
	dst.alpha = static_cast<A_u_short>(std::min<A_long>(PF_MAX_CHAN16, dst.alpha + addA16));
	dst.red = static_cast<A_u_short>(std::min<A_long>(PF_MAX_CHAN16, dst.red + (src.red * 257 * addA8) / PF_MAX_CHAN8));
	dst.green = static_cast<A_u_short>(std::min<A_long>(PF_MAX_CHAN16, dst.green + (src.green * 257 * addA8) / PF_MAX_CHAN8));
	dst.blue = static_cast<A_u_short>(std::min<A_long>(PF_MAX_CHAN16, dst.blue + (src.blue * 257 * addA8) / PF_MAX_CHAN8));
}

static void blendPixelFloat(PF_PixelFloat& dst, const PF_Pixel& src, A_long opacity)
{
	const PF_FpShort addA = std::max<PF_FpShort>(0.0f, std::min<PF_FpShort>(1.0f, static_cast<PF_FpShort>(opacity / 255.0f)));
	constexpr PF_FpShort inv = 1.0f / 255.0f;
	dst.alpha = std::min<PF_FpShort>(1.0f, dst.alpha + addA);
	dst.red = std::min<PF_FpShort>(1.0f, dst.red + src.red * inv * addA);
	dst.green = std::min<PF_FpShort>(1.0f, dst.green + src.green * inv * addA);
	dst.blue = std::min<PF_FpShort>(1.0f, dst.blue + src.blue * inv * addA);
}

template <typename RectT>
static void clampRect(const RectT& rect, A_long width, A_long height, A_long& x0, A_long& y0, A_long& x1, A_long& y1)
{
	x0 = std::max<A_long>(0, static_cast<A_long>(std::floor(rect.left + 0.5)));
	y0 = std::max<A_long>(0, static_cast<A_long>(std::floor(rect.top + 0.5)));
	x1 = std::min<A_long>(width, static_cast<A_long>(std::floor(rect.right + 0.5)));
	y1 = std::min<A_long>(height, static_cast<A_long>(std::floor(rect.bottom + 0.5)));
}

struct DrawRectD {
	double left = 0.0;
	double top = 0.0;
	double right = 0.0;
	double bottom = 0.0;
};

static void drawRect8(PF_EffectWorld* output, A_long x0, A_long y0, A_long x1, A_long y1, const PF_Pixel& color, A_long opacity)
{
	for (A_long y = y0; y < y1; ++y) {
		auto* row = reinterpret_cast<PF_Pixel8*>(reinterpret_cast<char*>(output->data) + y * output->rowbytes);
		for (A_long x = x0; x < x1; ++x) {
			blendPixel8(row[x], color, opacity);
		}
	}
}

static void drawRect16(PF_EffectWorld* output, A_long x0, A_long y0, A_long x1, A_long y1, const PF_Pixel& color, A_long opacity)
{
	for (A_long y = y0; y < y1; ++y) {
		auto* row = reinterpret_cast<PF_Pixel16*>(reinterpret_cast<char*>(output->data) + y * output->rowbytes);
		for (A_long x = x0; x < x1; ++x) {
			blendPixel16(row[x], color, opacity);
		}
	}
}

static void drawRectFloat(PF_EffectWorld* output, A_long x0, A_long y0, A_long x1, A_long y1, const PF_Pixel& color, A_long opacity)
{
	for (A_long y = y0; y < y1; ++y) {
		auto* row = reinterpret_cast<PF_PixelFloat*>(reinterpret_cast<char*>(output->data) + y * output->rowbytes);
		for (A_long x = x0; x < x1; ++x) {
			blendPixelFloat(row[x], color, opacity);
		}
	}
}

static void drawRectByFormat(PF_EffectWorld* output, PF_PixelFormat format, A_long x0, A_long y0, A_long x1, A_long y1, const PF_Pixel& color, A_long opacity)
{
	x0 = std::max<A_long>(0, x0);
	y0 = std::max<A_long>(0, y0);
	x1 = std::min<A_long>(output->width, x1);
	y1 = std::min<A_long>(output->height, y1);
	if (x0 >= x1 || y0 >= y1) {
		return;
	}
	if (format == PF_PixelFormat_ARGB64) {
		drawRect16(output, x0, y0, x1, y1, color, opacity);
	} else if (format == PF_PixelFormat_ARGB128) {
		drawRectFloat(output, x0, y0, x1, y1, color, opacity);
	} else {
		drawRect8(output, x0, y0, x1, y1, color, opacity);
	}
}

static void drawGuideLine(PF_EffectWorld* output, PF_PixelFormat format, const aeflg::GridGuide& guide, A_long lineWidth, const PF_Pixel& color, A_long opacity)
{
	const A_long half = std::max<A_long>(0, lineWidth / 2);
	if (guide.orientation == 1) {
		const A_long x0 = std::max<A_long>(0, guide.position - half);
		const A_long x1 = std::min<A_long>(output->width, x0 + lineWidth);
		drawRectByFormat(output, format, x0, 0, x1, output->height, color, opacity);
	} else {
		const A_long y0 = std::max<A_long>(0, guide.position - half);
		const A_long y1 = std::min<A_long>(output->height, y0 + lineWidth);
		drawRectByFormat(output, format, 0, y0, output->width, y1, color, opacity);
	}
}

static void drawPoint(PF_EffectWorld* output, PF_PixelFormat format, double x, double y, A_long lineWidth, const PF_Pixel& color, A_long opacity)
{
	const A_long size = std::max<A_long>(1, lineWidth);
	const A_long half = size / 2;
	const A_long px = static_cast<A_long>(std::floor(x + 0.5));
	const A_long py = static_cast<A_long>(std::floor(y + 0.5));
	drawRectByFormat(output, format, px - half, py - half, px - half + size, py - half + size, color, opacity);
}

static void drawLine(PF_EffectWorld* output, PF_PixelFormat format, double x0, double y0, double x1, double y1, A_long lineWidth, const PF_Pixel& color, A_long opacity)
{
	const double dx = x1 - x0;
	const double dy = y1 - y0;
	const int steps = std::max(1, static_cast<int>(std::ceil(std::max(std::abs(dx), std::abs(dy)))));
	for (int i = 0; i <= steps; ++i) {
		const double t = static_cast<double>(i) / static_cast<double>(steps);
		drawPoint(output, format, x0 + dx * t, y0 + dy * t, lineWidth, color, opacity);
	}
}

static void drawOutline(PF_EffectWorld* output, PF_PixelFormat format, const DrawRectD& r, A_long lineWidth, const PF_Pixel& color, A_long opacity)
{
	drawLine(output, format, r.left, r.top, r.right, r.top, lineWidth, color, opacity);
	drawLine(output, format, r.right, r.top, r.right, r.bottom, lineWidth, color, opacity);
	drawLine(output, format, r.right, r.bottom, r.left, r.bottom, lineWidth, color, opacity);
	drawLine(output, format, r.left, r.bottom, r.left, r.top, lineWidth, color, opacity);
}

static DrawRectD fittedRatioRect(double width, double height, double ratio)
{
	DrawRectD rect;
	const double compRatio = width / std::max(1.0, height);
	if (compRatio >= ratio) {
		const double rectWidth = height * ratio;
		rect.left = (width - rectWidth) * 0.5;
		rect.right = rect.left + rectWidth;
		rect.top = 0.0;
		rect.bottom = height;
	} else {
		const double rectHeight = width / ratio;
		rect.left = 0.0;
		rect.right = width;
		rect.top = (height - rectHeight) * 0.5;
		rect.bottom = rect.top + rectHeight;
	}
	return rect;
}

static std::vector<DrawRectD> spiralSquares(const DrawRectD& fitted, int maxCount)
{
	std::vector<DrawRectD> squares;
	DrawRectD rect = fitted;
	for (int i = 0; i < maxCount; ++i) {
		const double w = rect.right - rect.left;
		const double h = rect.bottom - rect.top;
		if (w < 2.0 || h < 2.0) {
			break;
		}

		DrawRectD sq;
		switch (i % 4) {
		case 0: {
			const double side = std::min(w, h);
			sq = { rect.left, rect.top, rect.left + side, rect.top + side };
			rect.left += side;
			break;
		}
		case 1: {
			const double side = std::min(w, h);
			sq = { rect.left, rect.top, rect.left + side, rect.top + side };
			rect.top += side;
			break;
		}
		case 2: {
			const double side = std::min(w, h);
			sq = { rect.right - side, rect.top, rect.right, rect.top + side };
			rect.right -= side;
			break;
		}
		default: {
			const double side = std::min(w, h);
			sq = { rect.left, rect.bottom - side, rect.left + side, rect.bottom };
			rect.bottom -= side;
			break;
		}
		}
		if (sq.right > sq.left && sq.bottom > sq.top) {
			squares.push_back(sq);
		}
	}
	return squares;
}

static void drawRatioRectangles(PF_EffectWorld* output, PF_PixelFormat format, double ratio, A_long lineWidth, const PF_Pixel& color, A_long opacity)
{
	const DrawRectD fitted = fittedRatioRect(static_cast<double>(output->width), static_cast<double>(output->height), ratio);
	drawOutline(output, format, fitted, lineWidth, color, opacity);
	const auto squares = spiralSquares(fitted, 10);
	for (const auto& square : squares) {
		drawOutline(output, format, square, lineWidth, color, opacity);
	}
}

static void drawArc(PF_EffectWorld* output, PF_PixelFormat format, double cx, double cy, double radius, double startAngle, double endAngle, A_long lineWidth, const PF_Pixel& color, A_long opacity)
{
	const int steps = std::max(12, static_cast<int>(std::ceil(radius * 0.75)));
	double prevX = cx + std::cos(startAngle) * radius;
	double prevY = cy + std::sin(startAngle) * radius;
	for (int i = 1; i <= steps; ++i) {
		const double t = static_cast<double>(i) / static_cast<double>(steps);
		const double a = startAngle + (endAngle - startAngle) * t;
		const double x = cx + std::cos(a) * radius;
		const double y = cy + std::sin(a) * radius;
		drawLine(output, format, prevX, prevY, x, y, lineWidth, color, opacity);
		prevX = x;
		prevY = y;
	}
}

static void drawGoldenSpiral(PF_EffectWorld* output, PF_PixelFormat format, A_long lineWidth, const PF_Pixel& color, A_long opacity)
{
	const DrawRectD fitted = fittedRatioRect(static_cast<double>(output->width), static_cast<double>(output->height), ratioForGuideMode(GUIDE_MODE_GOLDEN_RATIO));
	const auto squares = spiralSquares(fitted, 10);
	for (size_t i = 0; i < squares.size(); ++i) {
		const DrawRectD& sq = squares[i];
		const double side = sq.right - sq.left;
		switch (i % 4) {
		case 0:
			drawArc(output, format, sq.right, sq.bottom, side, 3.1415926535897932, 4.7123889803846899, lineWidth, color, opacity);
			break;
		case 1:
			drawArc(output, format, sq.left, sq.bottom, side, 4.7123889803846899, 6.2831853071795865, lineWidth, color, opacity);
			break;
		case 2:
			drawArc(output, format, sq.left, sq.top, side, 0.0, 1.5707963267948966, lineWidth, color, opacity);
			break;
		default:
			drawArc(output, format, sq.right, sq.top, side, 1.5707963267948966, 3.1415926535897932, lineWidth, color, opacity);
			break;
		}
	}
}

static PF_Err Render(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output)
{
	if (!params || !params[KG_LAYOUTGRID_INPUT] || !output) {
		return PF_Err_BAD_CALLBACK_PARAM;
	}

	PF_EffectWorld* input = &params[KG_LAYOUTGRID_INPUT]->u.ld;
	if (!input->data || !output->data) {
		return PF_Err_BAD_CALLBACK_PARAM;
	}

	for (A_long y = 0; y < output->height; ++y) {
		char* dst = reinterpret_cast<char*>(output->data) + y * output->rowbytes;
		std::memset(dst, 0, static_cast<size_t>(output->rowbytes));
	}

	const auto settings = settingsFromParams(params);
	const int displayMode = displayModeFromParams(params);
	const int guideMode = guideModeFromParams(params);
	const A_long opacity = static_cast<A_long>(bandOpacityFromParams(params));
	const A_long lineWidth = static_cast<A_long>(lineWidthFromParams(params));
	const PF_Pixel colColor = colorParam(params, KG_LAYOUTGRID_COLUMN_COLOR, { PF_MAX_CHAN8, 80, 170, 255 });
	const PF_Pixel rowColor = colorParam(params, KG_LAYOUTGRID_ROW_COLOR, { PF_MAX_CHAN8, 255, 140, 90 });
	const PF_Pixel spiralColor = colorParam(params, KG_LAYOUTGRID_SPIRAL_COLOR, { PF_MAX_CHAN8, 255, 210, 64 });
	PF_PixelFormat format = PF_PixelFormat_ARGB32;
	AEFX_SuiteScoper<PF_WorldSuite2> worldSuite(in_data, kPFWorldSuite, kPFWorldSuiteVersion2, out_data);
	if (!worldSuite->PF_GetPixelFormat(output, &format)) {
		if (guideMode == GUIDE_MODE_RATIO_RECTS || guideMode == GUIDE_MODE_RATIO_RECTS_AND_SPIRAL) {
			drawRatioRectangles(output, format, ratioForGuideMode(GUIDE_MODE_GOLDEN_RATIO), lineWidth, colColor, opacity);
			if (guideMode == GUIDE_MODE_RATIO_RECTS_AND_SPIRAL) {
				drawGoldenSpiral(output, format, lineWidth, spiralColor, opacity);
			}
		} else if (guideMode == GUIDE_MODE_GOLDEN_SPIRAL) {
			drawGoldenSpiral(output, format, lineWidth, spiralColor, opacity);
		} else if (displayMode == DISPLAY_MODE_BANDS && guideMode == GUIDE_MODE_GRID) {
			const auto columns = aeflg::calculateColumns(static_cast<double>(output->width), static_cast<double>(output->height), settings);
			const auto rows = aeflg::calculateRows(static_cast<double>(output->width), static_cast<double>(output->height), settings);
			for (const auto& rect : columns) {
				A_long x0 = 0;
				A_long y0 = 0;
				A_long x1 = 0;
				A_long y1 = 0;
				clampRect(rect, output->width, output->height, x0, y0, x1, y1);
				drawRectByFormat(output, format, x0, y0, x1, y1, colColor, opacity);
			}
			for (const auto& rect : rows) {
				A_long x0 = 0;
				A_long y0 = 0;
				A_long x1 = 0;
				A_long y1 = 0;
				clampRect(rect, output->width, output->height, x0, y0, x1, y1);
				drawRectByFormat(output, format, x0, y0, x1, y1, rowColor, opacity);
			}
		} else {
			const auto guides = guidesForMode(static_cast<double>(output->width), static_cast<double>(output->height), settings, guideMode);
			for (const auto& guide : guides) {
				const PF_Pixel color = guide.orientation == 1 ? colColor : rowColor;
				drawGuideLine(output, format, guide, lineWidth, color, opacity);
			}
		}
	}
	return PF_Err_NONE;
}

static PF_Err UserChangedParam(PF_InData* in_data, PF_OutData*, PF_ParamDef* params[], PF_LayerDef*, void* extra)
{
	if (!extra) {
		return PF_Err_NONE;
	}

	const auto* changed = reinterpret_cast<const PF_UserChangedParamExtra*>(extra);
	switch (changed->param_index) {
	case KG_LAYOUTGRID_APPLY_GUIDES:
		return executeAction(in_data, params, EffectAction::ApplyGuides);
	case KG_LAYOUTGRID_LOCK_GUIDES:
		return executeAction(in_data, params, EffectAction::LockGuides);
	case KG_LAYOUTGRID_UNLOCK_GUIDES:
		return executeAction(in_data, params, EffectAction::UnlockGuides);
	case KG_LAYOUTGRID_CLEAR_ALL:
		return executeAction(in_data, params, EffectAction::ClearAll);
	default:
		break;
	}
	return PF_Err_NONE;
}

} // namespace

extern "C" DllExport PF_Err EffectMain(
	PF_Cmd cmd,
	PF_InData* in_data,
	PF_OutData* out_data,
	PF_ParamDef* params[],
	PF_LayerDef* output,
	void* extra)
{
	PF_Err err = PF_Err_NONE;
	try {
		switch (cmd) {
		case PF_Cmd_ABOUT:
			err = About(in_data, out_data, params, output);
			break;
		case PF_Cmd_GLOBAL_SETUP:
			err = GlobalSetup(in_data, out_data, params, output);
			break;
		case PF_Cmd_GLOBAL_SETDOWN:
			err = GlobalSetdown(in_data, out_data, params, output);
			break;
		case PF_Cmd_PARAMS_SETUP:
			err = ParamsSetup(in_data, out_data, params, output);
			break;
		case PF_Cmd_USER_CHANGED_PARAM:
			err = UserChangedParam(in_data, out_data, params, output, extra);
			break;
		case PF_Cmd_RENDER:
			err = Render(in_data, out_data, params, output);
			break;
		default:
			break;
		}
	} catch (PF_Err& thrown_err) {
		err = thrown_err;
	} catch (...) {
		err = PF_Err_INTERNAL_STRUCT_DAMAGED;
	}

	(void)extra;
	return err;
}
