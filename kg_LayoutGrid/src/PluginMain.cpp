#include "PluginMain.h"

#include "GridMath.h"
#include "GridValidation.h"
#include "JsonStorage.h"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <string>

namespace {

using namespace aeflg;

constexpr const char* kPluginName = "AE Figma Layout Grid";
constexpr const char* kMenuName = "AE Figma Layout Grid...";
constexpr const char* kGenericError = "AE Figma Layout Grid failed.";

static AEGP_Command S_command = 0;
static AEGP_PluginID S_my_id = 0;
static SPBasicSuite* sP = nullptr;

enum class DialogAction {
	None,
	ApplyGuides,
	ClearGridGuides,
	ClearAll
};

struct DialogResult {
	DialogAction action = DialogAction::None;
	GridSettings settings;
};

#ifdef AE_OS_WIN

enum ControlId {
	ID_COL_COUNT = 1001,
	ID_COL_LEFT,
	ID_COL_GUTTER,
	ID_ROW_COUNT,
	ID_ROW_TOP,
	ID_ROW_GUTTER,
	ID_APPLY,
	ID_CLEAR_GUIDES,
	ID_CLEAR_ALL
};

static void addLabel(HWND parent, const char* text, int x, int y, int w, int h)
{
	CreateWindowExA(0, "STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, h, parent, nullptr, nullptr, nullptr);
}

static HWND addEdit(HWND parent, int id, const char* value, int x, int y)
{
	return CreateWindowExA(
		WS_EX_CLIENTEDGE,
		"EDIT",
		value,
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
		x,
		y,
		78,
		22,
		parent,
		reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
		nullptr,
		nullptr);
}

static void addButton(HWND parent, int id, const char* text, int x, int y, int w)
{
	CreateWindowExA(
		0,
		"BUTTON",
		text,
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		x,
		y,
		w,
		26,
		parent,
		reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
		nullptr,
		nullptr);
}

static std::string getText(HWND parent, int id)
{
	char buf[128] = {};
	GetWindowTextA(GetDlgItem(parent, id), buf, static_cast<int>(sizeof(buf)));
	return buf;
}

static double readDouble(HWND parent, int id, double fallback)
{
	const std::string text = getText(parent, id);
	char* end = nullptr;
	const double value = std::strtod(text.c_str(), &end);
	return end != text.c_str() ? value : fallback;
}

static int readInt(HWND parent, int id, int fallback)
{
	const std::string text = getText(parent, id);
	char* end = nullptr;
	const long value = std::strtol(text.c_str(), &end, 10);
	return end != text.c_str() ? static_cast<int>(std::max<long>(1, value)) : fallback;
}

static void readSettings(HWND hwnd, DialogResult* result)
{
	result->settings.columns.count = readInt(hwnd, ID_COL_COUNT, result->settings.columns.count);
	result->settings.columns.marginLeft = std::max(0.0, readDouble(hwnd, ID_COL_LEFT, result->settings.columns.marginLeft));
	result->settings.columns.marginRight = result->settings.columns.marginLeft;
	result->settings.columns.gutter = std::max(0.0, readDouble(hwnd, ID_COL_GUTTER, result->settings.columns.gutter));
	result->settings.rows.count = readInt(hwnd, ID_ROW_COUNT, result->settings.rows.count);
	result->settings.rows.marginTop = std::max(0.0, readDouble(hwnd, ID_ROW_TOP, result->settings.rows.marginTop));
	result->settings.rows.marginBottom = result->settings.rows.marginTop;
	result->settings.rows.gutter = std::max(0.0, readDouble(hwnd, ID_ROW_GUTTER, result->settings.rows.gutter));
}

static LRESULT CALLBACK dialogWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_CREATE) {
		auto* result = reinterpret_cast<DialogResult*>(reinterpret_cast<CREATESTRUCTA*>(lParam)->lpCreateParams);
		SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(result));

		addLabel(hwnd, "Columns", 16, 14, 120, 18);
		addLabel(hwnd, "Count", 18, 42, 72, 18);
		addLabel(hwnd, "Margin L/R", 112, 42, 92, 18);
		addLabel(hwnd, "Gutter", 224, 42, 72, 18);
		addEdit(hwnd, ID_COL_COUNT, "12", 18, 62);
		addEdit(hwnd, ID_COL_LEFT, "80", 112, 62);
		addEdit(hwnd, ID_COL_GUTTER, "24", 224, 62);

		addLabel(hwnd, "Rows", 16, 98, 120, 18);
		addLabel(hwnd, "Count", 18, 126, 72, 18);
		addLabel(hwnd, "Margin T/B", 112, 126, 92, 18);
		addLabel(hwnd, "Gutter", 224, 126, 72, 18);
		addEdit(hwnd, ID_ROW_COUNT, "12", 18, 146);
		addEdit(hwnd, ID_ROW_TOP, "80", 112, 146);
		addEdit(hwnd, ID_ROW_GUTTER, "24", 224, 146);

		addButton(hwnd, ID_APPLY, "Apply Guides", 18, 196, 128);
		addButton(hwnd, ID_CLEAR_GUIDES, "Clear Grid Guides", 158, 196, 144);
		addButton(hwnd, ID_CLEAR_ALL, "Clear All", 314, 196, 112);
		return 0;
	}

	if (msg == WM_COMMAND) {
		auto* result = reinterpret_cast<DialogResult*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
		if (!result) {
			return 0;
		}
		readSettings(hwnd, result);
		switch (LOWORD(wParam)) {
		case ID_APPLY: result->action = DialogAction::ApplyGuides; break;
		case ID_CLEAR_GUIDES: result->action = DialogAction::ClearGridGuides; break;
		case ID_CLEAR_ALL: result->action = DialogAction::ClearAll; break;
		default: return 0;
		}
		DestroyWindow(hwnd);
		return 0;
	}

	if (msg == WM_CLOSE) {
		DestroyWindow(hwnd);
		return 0;
	}
	return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static DialogResult showDialog()
{
	DialogResult result;
	const HINSTANCE instance = GetModuleHandleA(nullptr);
	const char* className = "kgLayoutGridDialog";
	WNDCLASSA wc = {};
	wc.lpfnWndProc = dialogWndProc;
	wc.hInstance = instance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	wc.lpszClassName = className;
	RegisterClassA(&wc);

	HWND hwnd = CreateWindowExA(
		WS_EX_DLGMODALFRAME,
		className,
		kPluginName,
		WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		500,
		320,
		nullptr,
		nullptr,
		instance,
		&result);
	if (!hwnd) {
		return result;
	}

	MSG msg;
	while (IsWindow(hwnd) && GetMessageA(&msg, nullptr, 0, 0) > 0) {
		if (!IsDialogMessageA(hwnd, &msg)) {
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
	}
	return result;
}

#else
static DialogResult showDialog()
{
	return {};
}
#endif

static const char* actionName(DialogAction action)
{
	switch (action) {
	case DialogAction::ApplyGuides: return "applyGuides";
	case DialogAction::ClearGridGuides: return "clearGridGuides";
	case DialogAction::ClearAll: return "clearAll";
	default: return "none";
	}
}

static std::string buildScript(DialogAction action, const GridSettings& settings)
{
	const std::string json = settingsToJson(settings);
	std::ostringstream js;
	js << "(function(){\n";
	js << "var ACTION=\"" << actionName(action) << "\";\n";
	js << "var SETTINGS=JSON.parse(\"" << escapeJsString(json) << "\");\n";
	js << "function fail(m){alert('AE Figma Layout Grid: '+m);throw new Error(m);}\n";
	js << "function comp(){var c=app.project&&app.project.activeItem;if(!c||!(c instanceof CompItem))fail('Active item is not a composition.');return c;}\n";
	js << "function controller(c){if(!c.selectedLayers||c.selectedLayers.length<1)fail('Select the layer with kg_LayoutGrid applied.');return c.selectedLayers[0];}\n";
	js << "function uniqueGuides(gs){var out=[];var seen={};for(var i=0;i<gs.length;i++){var p=Math.round(gs[i].position);var k=gs[i].orientation+':'+p;if(!seen[k]){seen[k]=true;out.push({orientation:gs[i].orientation,position:p});}}return out;}\n";
	js << "function roundGuides(gs){var out=[];for(var i=0;i<gs.length;i++)out.push({orientation:gs[i].orientation,position:Math.round(gs[i].position)});return out;}\n";
	js << "function trProp(l,n,m){var t=l.property('ADBE Transform Group')||l.property('Transform');if(!t)return null;return t.property(m)||t.property(n);}\n";
	js << "function propVal(l,n,m,d){var p=trProp(l,n,m);try{return p?p.value:d;}catch(e){return d;}}\n";
	js << "function pointToComp2D(l,p){if(typeof l.sourcePointToComp==='function')return l.sourcePointToComp(p);var a=propVal(l,'Anchor Point','ADBE Anchor Point',[0,0,0]);var pos=propVal(l,'Position','ADBE Position',[0,0,0]);var sc=propVal(l,'Scale','ADBE Scale',[100,100,100]);var rot=propVal(l,'Rotation','ADBE Rotate Z',0);var rad=rot*Math.PI/180;var x=(p[0]-a[0])*(sc[0]/100);var y=(p[1]-a[1])*(sc[1]/100);var cr=Math.cos(rad),sr=Math.sin(rad);return [pos[0]+x*cr-y*sr,pos[1]+x*sr+y*cr];}\n";
	js << "function layerRect(c,l){var r=null;try{r=l.sourceRectAtTime(c.time,false);}catch(e){r=null;}if(!r||r.width===undefined||r.height===undefined){r={left:0,top:0,width:l.width,height:l.height};}var pts=[pointToComp2D(l,[r.left,r.top]),pointToComp2D(l,[r.left+r.width,r.top]),pointToComp2D(l,[r.left+r.width,r.top+r.height]),pointToComp2D(l,[r.left,r.top+r.height])];var left=Math.min(pts[0][0],pts[1][0],pts[2][0],pts[3][0]);var right=Math.max(pts[0][0],pts[1][0],pts[2][0],pts[3][0]);var top=Math.min(pts[0][1],pts[1][1],pts[2][1],pts[3][1]);var bottom=Math.max(pts[0][1],pts[1][1],pts[2][1],pts[3][1]);return {left:left,top:top,width:right-left,height:bottom-top};}\n";
	js << "function calc(c,l,s){var b=layerRect(c,l),g=[],cw=(b.width-s.columns.marginLeft-s.columns.marginRight-s.columns.gutter*(s.columns.count-1))/s.columns.count,rh=(b.height-s.rows.marginTop-s.rows.marginBottom-s.rows.gutter*(s.rows.count-1))/s.rows.count;if(cw<=0)fail('Column width is 0 or negative.');if(rh<=0)fail('Row height is 0 or negative.');for(var i=0;i<s.columns.count;i++){var x=b.left+s.columns.marginLeft+i*(cw+s.columns.gutter);g.push({orientation:1,position:x});g.push({orientation:1,position:x+cw});}for(var r=0;r<s.rows.count;r++){var y=b.top+s.rows.marginTop+r*(rh+s.rows.gutter);g.push({orientation:0,position:y});g.push({orientation:0,position:y+rh});}return roundGuides(g);}\n";
	js << "function aeOrientation(o){return o===1?1:0;}\n";
	js << "function legacyAeOrientation(o){return o===1?0:1;}\n";
	js << "function guideCounts(gs){var h=0,v=0;for(var i=0;i<gs.length;i++){if(gs[i].orientation===1)v++;else h++;}return {horizontal:h,vertical:v};}\n";
	js << "function validateGridGuideCounts(gs,s){var got=guideCounts(gs);var eh=s.rows.count*2;var ev=s.columns.count*2;if(got.horizontal!==eh||got.vertical!==ev)fail('Guide count mismatch. Expected horizontal '+eh+' and vertical '+ev+', got horizontal '+got.horizontal+' and vertical '+got.vertical+'.');}\n";
	js << "function validate(c,s){if(s.columns.count<1||s.rows.count<1)fail('Count must be 1 or greater.');var vals=[s.columns.marginLeft,s.columns.marginRight,s.columns.gutter,s.rows.marginTop,s.rows.marginBottom,s.rows.gutter];for(var i=0;i<vals.length;i++){if(vals[i]<0||isNaN(vals[i]))fail('Margins and gutters must be 0 or greater.');}}\n";
	js << "function readData(layer){if(!layer.comment)return SETTINGS;try{var d=eval('('+layer.comment+')');return d&&d.plugin==='AE_Figma_Layout_Grid_AEX'?d:SETTINGS;}catch(e){return SETTINGS;}}\n";
	js << "function writeData(layer,s){var a=[];for(var i=0;i<s.generatedGuides.length;i++){a.push('{\"orientation\":'+s.generatedGuides[i].orientation+',\"position\":'+s.generatedGuides[i].position+'}');}layer.comment='{\"plugin\":\"AE_Figma_Layout_Grid_AEX\",\"version\":\"0.1.1\",\"guideOrientation\":\"ae\",\"columns\":{\"count\":'+s.columns.count+',\"marginLeft\":'+s.columns.marginLeft+',\"marginRight\":'+s.columns.marginRight+',\"gutter\":'+s.columns.gutter+'},\"rows\":{\"count\":'+s.rows.count+',\"marginTop\":'+s.rows.marginTop+',\"marginBottom\":'+s.rows.marginBottom+',\"gutter\":'+s.rows.gutter+'},\"generatedGuides\":['+a.join(',')+']}';}\n";
	js << "function guideMatches(gd,g,allowLegacy){var p=Math.round(g.position);if(Math.round(gd.position)!==p)return false;var o=aeOrientation(g.orientation);return gd.orientationType===o||(allowLegacy&&gd.orientationType===legacyAeOrientation(g.orientation));}\n";
	js << "function removeGuides(c,gs,allowLegacy){if(!c.guides)return;for(var i=c.guides.length-1;i>=0;i--){var gd=c.guides[i];for(var j=0;j<gs.length;j++){if(guideMatches(gd,gs[j],allowLegacy)){c.removeGuide(i);break;}}}}\n";
	js << "function apply(c,layer,s){var old=readData(layer);removeGuides(c,old.generatedGuides||[],old.guideOrientation!=='ae');var ng=calc(c,layer,s);validateGridGuideCounts(ng,s);for(var i=0;i<ng.length;i++){c.addGuide(aeOrientation(ng[i].orientation),ng[i].position);}s.generatedGuides=ng;s.guideOrientation='ae';writeData(layer,s);}\n";
	js << "app.beginUndoGroup('kg_LayoutGrid');try{var c=comp();var l=controller(c);if(ACTION==='applyGuides'){validate(c,SETTINGS);apply(c,l,SETTINGS);}else if(ACTION==='clearGridGuides'){var s=readData(l);removeGuides(c,s.generatedGuides||[],s.guideOrientation!=='ae');s.generatedGuides=[];s.guideOrientation='ae';writeData(l,s);}else if(ACTION==='clearAll'){var s2=readData(l);removeGuides(c,s2.generatedGuides||[],s2.guideOrientation!=='ae');l.comment='';}}catch(e){if(e.message)alert('kg_LayoutGrid: '+e.message);}finally{app.endUndoGroup();}\n";
	js << "})();\n";
	return js.str();
}

static A_Err executeScript(DialogAction action, const GridSettings& settings)
{
	A_Err err = A_Err_NONE;
	AEGP_SuiteHandler suites(sP);
	const std::string script = buildScript(action, settings);
	ERR(suites.UtilitySuite5()->AEGP_ExecuteScript(S_my_id, script.c_str(), TRUE, nullptr, nullptr));
	return err;
}

static A_Err UpdateMenuHook(AEGP_GlobalRefcon, AEGP_UpdateMenuRefcon, AEGP_WindowType)
{
	A_Err err = A_Err_NONE;
	AEGP_SuiteHandler suites(sP);
	AEGP_ItemH activeItem = nullptr;
	AEGP_ItemType itemType = AEGP_ItemType_NONE;
	ERR(suites.ItemSuite6()->AEGP_GetActiveItem(&activeItem));
	if (activeItem) {
		ERR(suites.ItemSuite6()->AEGP_GetItemType(activeItem, &itemType));
	}
	if (!err && itemType == AEGP_ItemType_COMP) {
		ERR(suites.CommandSuite1()->AEGP_EnableCommand(S_command));
	} else {
		ERR(suites.CommandSuite1()->AEGP_DisableCommand(S_command));
	}
	return err;
}

static A_Err CommandHook(AEGP_GlobalRefcon, AEGP_CommandRefcon, AEGP_Command command, AEGP_HookPriority, A_Boolean, A_Boolean* handledPB)
{
	A_Err err = A_Err_NONE;
	if (command != S_command) {
		return err;
	}

	try {
		const DialogResult result = showDialog();
		if (result.action != DialogAction::None) {
			ERR(executeScript(result.action, result.settings));
		}
		*handledPB = TRUE;
	} catch (A_Err& thrownErr) {
		err = thrownErr;
	} catch (...) {
		err = A_Err_GENERIC;
	}
	return err;
}

} // namespace

DllExport A_Err EntryPointFunc(
	struct SPBasicSuite* pica_basicP,
	A_long,
	A_long,
	const A_char*,
	const A_char*,
	AEGP_PluginID aegp_plugin_id,
	void*)
{
	A_Err err = A_Err_NONE;
	A_Err err2 = A_Err_NONE;
	sP = pica_basicP;
	S_my_id = aegp_plugin_id;

	try {
		AEGP_SuiteHandler suites(sP);
		ERR(suites.CommandSuite1()->AEGP_GetUniqueCommand(&S_command));
		ERR(suites.CommandSuite1()->AEGP_InsertMenuCommand(S_command, kMenuName, AEGP_Menu_WINDOW, AEGP_MENU_INSERT_SORTED));
		ERR(suites.RegisterSuite5()->AEGP_RegisterCommandHook(S_my_id, AEGP_HP_BeforeAE, AEGP_Command_ALL, CommandHook, nullptr));
		ERR(suites.RegisterSuite5()->AEGP_RegisterUpdateMenuHook(S_my_id, UpdateMenuHook, nullptr));
		if (err) {
			ERR2(suites.UtilitySuite3()->AEGP_ReportInfo(S_my_id, kGenericError));
		}
	} catch (A_Err& thrownErr) {
		err = thrownErr;
	}

	return err;
}
