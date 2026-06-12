static PF_FpLong BendRampRawAt(PF_FpLong u, const LineAnimRenderInfo &info)
{
	return BendRampBezierRawAt(u, info);
}

static DRAWBOT_ColorRGBA DrawbotColor(PF_FpLong r, PF_FpLong g, PF_FpLong b, PF_FpLong a = 1.0)
{
	DRAWBOT_ColorRGBA c;
	c.red = static_cast<float>(ClampF(r, 0.0, 1.0));
	c.green = static_cast<float>(ClampF(g, 0.0, 1.0));
	c.blue = static_cast<float>(ClampF(b, 0.0, 1.0));
	c.alpha = static_cast<float>(ClampF(a, 0.0, 1.0));
	return c;
}

static PF_Err FillRect(DRAWBOT_Suites &suites, DRAWBOT_SupplierRef supplier, DRAWBOT_SurfaceRef surface, const DRAWBOT_RectF32 &rect, const DRAWBOT_ColorRGBA &color)
{
	PF_Err err = PF_Err_NONE, err2 = PF_Err_NONE;
	DRAWBOT_BrushRef brush = nullptr;
	DRAWBOT_PathRef path = nullptr;
	ERR(suites.supplier_suiteP->NewBrush(supplier, &color, &brush));
	ERR(suites.supplier_suiteP->NewPath(supplier, &path));
	ERR(suites.path_suiteP->AddRect(path, &rect));
	ERR(suites.surface_suiteP->FillPath(surface, brush, path, kDRAWBOT_FillType_Default));
	if (path) {
		ERR2(suites.supplier_suiteP->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(path)));
	}
	if (brush) {
		ERR2(suites.supplier_suiteP->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(brush)));
	}
	return err ? err : err2;
}

static PF_Err StrokePath(DRAWBOT_Suites &suites, DRAWBOT_SupplierRef supplier, DRAWBOT_SurfaceRef surface, DRAWBOT_PathRef path, const DRAWBOT_ColorRGBA &color, float width)
{
	PF_Err err = PF_Err_NONE, err2 = PF_Err_NONE;
	DRAWBOT_PenRef pen = nullptr;
	ERR(suites.supplier_suiteP->NewPen(supplier, &color, width, &pen));
	ERR(suites.surface_suiteP->StrokePath(surface, pen, path));
	if (pen) {
		ERR2(suites.supplier_suiteP->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(pen)));
	}
	return err ? err : err2;
}

static PF_Err DrawLineGradientUI(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], PF_EventExtra *event_extra)
{
	PF_Err err = PF_Err_NONE, err2 = PF_Err_NONE;
	DRAWBOT_DrawRef drawRef = nullptr;
	DRAWBOT_SurfaceRef surface = nullptr;
	DRAWBOT_SupplierRef supplier = nullptr;
	DRAWBOT_Suites suites;
	AEFX_CLR_STRUCT(suites);
	ERR(AEFX_AcquireDrawbotSuites(in_data, out_data, &suites));

	PF_EffectCustomUISuite1 *customSuite = nullptr;
	ERR(AEFX_AcquireSuite(in_data, out_data, kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion1, nullptr, reinterpret_cast<void **>(&customSuite)));
	if (!err && customSuite) {
		ERR(customSuite->PF_GetDrawingReference(event_extra->contextH, &drawRef));
		ERR2(AEFX_ReleaseSuite(in_data, out_data, kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion1, nullptr));
	}
	ERR(suites.drawbot_suiteP->GetSupplier(drawRef, &supplier));
	ERR(suites.drawbot_suiteP->GetSurface(drawRef, &surface));

	if (!err) {
		const PF_UnionableRect &f = event_extra->effect_win.current_frame;
		DRAWBOT_RectF32 bg = { static_cast<float>(f.left), static_cast<float>(f.top), static_cast<float>(f.right - f.left), static_cast<float>(f.bottom - f.top) };
		ERR(FillRect(suites, supplier, surface, bg, DrawbotColor(0.16, 0.16, 0.16, 1.0)));

		const float left = static_cast<float>(f.left + 10);
		const float top = static_cast<float>(f.top + 7);
		const float width = static_cast<float>(std::max<A_long>(1, f.right - f.left - 20));
		const float height = static_cast<float>(std::max<A_long>(1, f.bottom - f.top - 16));
		LineAnimRenderInfo info = InfoFromParams(params);
		for (A_long i = 0; i < static_cast<A_long>(width); ++i) {
			PF_FpLong r = 0.0, g = 0.0, b = 0.0;
			GradientColorAt(info, static_cast<PF_FpLong>(i) / std::max<PF_FpLong>(1.0, width - 1.0f), r, g, b);
			DRAWBOT_RectF32 strip = { left + static_cast<float>(i), top, 1.0f, height };
			ERR(FillRect(suites, supplier, surface, strip, DrawbotColor(r, g, b, 1.0)));
		}

		DRAWBOT_PathRef border = nullptr;
		ERR(suites.supplier_suiteP->NewPath(supplier, &border));
		DRAWBOT_RectF32 bar = { left, top, width, height };
		ERR(suites.path_suiteP->AddRect(border, &bar));
		ERR(StrokePath(suites, supplier, surface, border, DrawbotColor(0.85, 0.85, 0.85, 1.0), 1.0f));
		for (A_long i = 0; i < 5; ++i) {
			const float x = left + width * static_cast<float>(ClampF(info.line_color_stops[i], 0.0, 1.0));
			DRAWBOT_RectF32 stop = { x - 4.0f, top + height - 4.0f, 8.0f, 8.0f };
			DRAWBOT_PathRef stopPath = nullptr;
			ERR(FillRect(suites, supplier, surface, stop, DrawbotColor(0.05, 0.05, 0.05, 1.0)));
			ERR(suites.supplier_suiteP->NewPath(supplier, &stopPath));
			ERR(suites.path_suiteP->AddRect(stopPath, &stop));
			ERR(StrokePath(suites, supplier, surface, stopPath, DrawbotColor(0.85, 0.85, 0.85, 1.0), 1.0f));
			if (stopPath) {
				ERR2(suites.supplier_suiteP->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(stopPath)));
			}
		}
		if (border) {
			ERR2(suites.supplier_suiteP->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(border)));
		}
	}
	ERR2(AEFX_ReleaseDrawbotSuites(in_data, out_data));
	if (!err) {
		event_extra->evt_out_flags = PF_EO_HANDLED_EVENT;
	}
	return err ? err : err2;
}

static PF_Err DrawBendRampUI(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], PF_EventExtra *event_extra)
{
	PF_Err err = PF_Err_NONE, err2 = PF_Err_NONE;
	DRAWBOT_DrawRef drawRef = nullptr;
	DRAWBOT_SurfaceRef surface = nullptr;
	DRAWBOT_SupplierRef supplier = nullptr;
	DRAWBOT_Suites suites;
	AEFX_CLR_STRUCT(suites);
	ERR(AEFX_AcquireDrawbotSuites(in_data, out_data, &suites));
	PF_EffectCustomUISuite1 *customSuite = nullptr;
	ERR(AEFX_AcquireSuite(in_data, out_data, kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion1, nullptr, reinterpret_cast<void **>(&customSuite)));
	if (!err && customSuite) {
		ERR(customSuite->PF_GetDrawingReference(event_extra->contextH, &drawRef));
		ERR2(AEFX_ReleaseSuite(in_data, out_data, kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion1, nullptr));
	}
	ERR(suites.drawbot_suiteP->GetSupplier(drawRef, &supplier));
	ERR(suites.drawbot_suiteP->GetSurface(drawRef, &surface));

	if (!err) {
		const PF_UnionableRect &f = event_extra->effect_win.current_frame;
		DRAWBOT_RectF32 bg = { static_cast<float>(f.left), static_cast<float>(f.top), static_cast<float>(f.right - f.left), static_cast<float>(f.bottom - f.top) };
		ERR(FillRect(suites, supplier, surface, bg, DrawbotColor(0.22, 0.22, 0.22, 1.0)));
		const float left = static_cast<float>(f.left + 12);
		const float top = static_cast<float>(f.top + 10);
		const float width = static_cast<float>(std::max<A_long>(1, f.right - f.left - 24));
		const float height = static_cast<float>(std::max<A_long>(1, f.bottom - f.top - 22));
		DRAWBOT_RectF32 plot = { left, top, width, height };
		ERR(FillRect(suites, supplier, surface, plot, DrawbotColor(0.28, 0.28, 0.28, 1.0)));

		LineAnimRenderInfo info = InfoFromParams(params);
		DRAWBOT_PathRef handles = nullptr;
		ERR(suites.supplier_suiteP->NewPath(supplier, &handles));
		for (A_long i = 0; i < 4; ++i) {
			const float ax0 = left + width * static_cast<float>(i) / 4.0f;
			const float ay0 = top + height * static_cast<float>(1.0 - info.bend_ramp[i]);
			const float ox = left + width * (static_cast<float>(i) + 1.0f / 3.0f) / 4.0f;
			const float oy = top + height * static_cast<float>(1.0 - info.bend_handle_out[i]);
			const float ix = left + width * (static_cast<float>(i) + 2.0f / 3.0f) / 4.0f;
			const float iy = top + height * static_cast<float>(1.0 - info.bend_handle_in[i]);
			const float ax1 = left + width * static_cast<float>(i + 1) / 4.0f;
			const float ay1 = top + height * static_cast<float>(1.0 - info.bend_ramp[i + 1]);
			ERR(suites.path_suiteP->MoveTo(handles, ax0, ay0));
			ERR(suites.path_suiteP->LineTo(handles, ox, oy));
			ERR(suites.path_suiteP->MoveTo(handles, ix, iy));
			ERR(suites.path_suiteP->LineTo(handles, ax1, ay1));
		}
		ERR(StrokePath(suites, supplier, surface, handles, DrawbotColor(0.42, 0.52, 0.58, 1.0), 1.0f));

		DRAWBOT_PathRef curve = nullptr;
		ERR(suites.supplier_suiteP->NewPath(supplier, &curve));
		for (A_long i = 0; i <= 96; ++i) {
			const PF_FpLong u = static_cast<PF_FpLong>(i) / 96.0;
			const PF_FpLong x = left + width * u;
			const PF_FpLong y = top + height * (1.0 - BendRampRawAt(u, info));
			if (i == 0) {
				ERR(suites.path_suiteP->MoveTo(curve, static_cast<float>(x), static_cast<float>(y)));
			} else {
				ERR(suites.path_suiteP->LineTo(curve, static_cast<float>(x), static_cast<float>(y)));
			}
		}
		ERR(StrokePath(suites, supplier, surface, curve, DrawbotColor(0.78, 0.78, 0.78, 1.0), 1.5f));

		for (A_long i = 0; i < 4; ++i) {
			const float ox = left + width * (static_cast<float>(i) + 1.0f / 3.0f) / 4.0f;
			const float oy = top + height * static_cast<float>(1.0 - info.bend_handle_out[i]);
			const float ix = left + width * (static_cast<float>(i) + 2.0f / 3.0f) / 4.0f;
			const float iy = top + height * static_cast<float>(1.0 - info.bend_handle_in[i]);
			DRAWBOT_RectF32 outHandle = { ox - 3.0f, oy - 3.0f, 6.0f, 6.0f };
			DRAWBOT_RectF32 inHandle = { ix - 3.0f, iy - 3.0f, 6.0f, 6.0f };
			ERR(FillRect(suites, supplier, surface, outHandle, DrawbotColor(0.40, 0.55, 0.66, 1.0)));
			ERR(FillRect(suites, supplier, surface, inHandle, DrawbotColor(0.40, 0.55, 0.66, 1.0)));
		}
		for (A_long i = 0; i < 5; ++i) {
			const float x = left + width * static_cast<float>(i) / 4.0f;
			const float y = top + height * static_cast<float>(1.0 - info.bend_ramp[i]);
			DRAWBOT_RectF32 handle = { x - 4.0f, y - 4.0f, 8.0f, 8.0f };
			ERR(FillRect(suites, supplier, surface, handle, DrawbotColor(0.18, 0.34, 0.46, 1.0)));
		}
		if (curve) {
			ERR2(suites.supplier_suiteP->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(curve)));
		}
		if (handles) {
			ERR2(suites.supplier_suiteP->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(handles)));
		}
	}
	ERR2(AEFX_ReleaseDrawbotSuites(in_data, out_data));
	if (!err) {
		event_extra->evt_out_flags = PF_EO_HANDLED_EVENT;
	}
	return err ? err : err2;
}

static PF_Err InvalidateCustomControl(PF_InData *in_data, PF_EventExtra *event_extra)
{
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	PF_Rect inval(event_extra->effect_win.current_frame);
	return suites.AppSuite4()->PF_InvalidateRect(event_extra->contextH, &inval);
}

static PF_Err HandleEvent(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], PF_LayerDef *, PF_EventExtra *event_extra)
{
	PF_Err err = PF_Err_NONE;
	const PF_ParamIndex index = event_extra->effect_win.index;
	if (event_extra->e_type == PF_Event_DRAW && event_extra->effect_win.area == PF_EA_CONTROL) {
		if (index == LNA_LINE_COLOR_STOP_1) {
			err = DrawLineGradientUI(in_data, out_data, params, event_extra);
		} else if (index == LNA_BEND_RAMP_1) {
			err = DrawBendRampUI(in_data, out_data, params, event_extra);
		}
	} else if ((event_extra->e_type == PF_Event_DO_CLICK || event_extra->e_type == PF_Event_DRAG) && event_extra->effect_win.area == PF_EA_CONTROL) {
		const PF_UnionableRect &f = event_extra->effect_win.current_frame;
		A_long mx = event_extra->u.do_click.screen_point.h - f.left;
		A_long my = event_extra->u.do_click.screen_point.v - f.top;
		if (mx < 0 || my < 0) {
			mx = event_extra->u.do_click.screen_point.h;
			my = event_extra->u.do_click.screen_point.v;
		}
		if (index == LNA_LINE_COLOR_STOP_1) {
			const A_long w = std::max<A_long>(1, f.right - f.left - 20);
			const PF_FpLong u = ClampF(static_cast<PF_FpLong>(mx - 10) / static_cast<PF_FpLong>(w), 0.0, 1.0);
			params[LNA_LINE_COLOR_STOP_1]->u.fs_d.value = u * 100.0;
			params[LNA_LINE_COLOR_STOP_1]->uu.change_flags = PF_ChangeFlag_CHANGED_VALUE;
			if (event_extra->e_type == PF_Event_DO_CLICK) {
				event_extra->u.do_click.send_drag = TRUE;
			}
			ERR(InvalidateCustomControl(in_data, event_extra));
			event_extra->evt_out_flags = PF_EO_HANDLED_EVENT | PF_EO_UPDATE_NOW | PF_EO_ALWAYS_UPDATE;
		} else if (index == LNA_BEND_RAMP_1) {
			// Compute coordinate system
			const PF_FpLong uiWidth  = static_cast<PF_FpLong>(std::max<A_long>(1, f.right - f.left - 24));
			const PF_FpLong uiHeight = static_cast<PF_FpLong>(std::max<A_long>(1, f.bottom - f.top - 22));
			const PF_FpLong left = static_cast<PF_FpLong>(f.left + 12);
			const PF_FpLong top  = static_cast<PF_FpLong>(f.top  + 10);

			// Mouse position relative to plot area
			const PF_FpLong pxRaw = static_cast<PF_FpLong>(event_extra->u.do_click.screen_point.h);
			const PF_FpLong pyRaw = static_cast<PF_FpLong>(event_extra->u.do_click.screen_point.v);
			const PF_FpLong px = pxRaw - left;
			const PF_FpLong py = pyRaw - top;
			const PF_FpLong vRaw = ClampF(1.0 - (pyRaw - top) / uiHeight, 0.0, 1.0);

			// Read current info from params
			LineAnimRenderInfo info = InfoFromParams(params);

			// On DO_CLICK: find nearest handle and record it in a static variable
			// Handle layout:
			//   indices 0..4  = bend_ramp[0..4]  (anchor points, x = i/4 of width)
			//   indices 5..8  = bend_handle_out[0..3] (x = (i + 1/3)/4 of width)
			//   indices 9..12 = bend_handle_in[0..3]  (x = (i + 2/3)/4 of width)
			static A_long s_dragHandle = 0; // persists across DO_CLICK / DRAG pairs

			if (event_extra->e_type == PF_Event_DO_CLICK) {
				// Find nearest handle
				PF_FpLong bestDist2 = 1.0e18;
				A_long bestHandle = 0;

				auto checkHandle = [&](A_long hi, PF_FpLong hx, PF_FpLong hy) {
					const PF_FpLong dx = px - hx;
					const PF_FpLong dy = py - hy;
					const PF_FpLong d2 = dx * dx + dy * dy;
					if (d2 < bestDist2) {
						bestDist2 = d2;
						bestHandle = hi;
					}
				};

				// Anchor (ramp) points at x = i/4 * width
				for (A_long i = 0; i < 5; ++i) {
					const PF_FpLong hx = uiWidth * static_cast<PF_FpLong>(i) / 4.0;
					const PF_FpLong hy = uiHeight * (1.0 - info.bend_ramp[i]);
					checkHandle(i, hx, hy);
				}
				// Out handles at x = (i + 1/3)/4 * width
				for (A_long i = 0; i < 4; ++i) {
					const PF_FpLong hx = uiWidth * (static_cast<PF_FpLong>(i) + 1.0 / 3.0) / 4.0;
					const PF_FpLong hy = uiHeight * (1.0 - info.bend_handle_out[i]);
					checkHandle(5 + i, hx, hy);
				}
				// In handles at x = (i + 2/3)/4 * width
				for (A_long i = 0; i < 4; ++i) {
					const PF_FpLong hx = uiWidth * (static_cast<PF_FpLong>(i) + 2.0 / 3.0) / 4.0;
					const PF_FpLong hy = uiHeight * (1.0 - info.bend_handle_in[i]);
					checkHandle(9 + i, hx, hy);
				}
				s_dragHandle = bestHandle;
				event_extra->u.do_click.send_drag = TRUE;
			}

			// Apply the drag value to the selected handle
			static const PF_ParamIndex BEND_RAMP_PARAMS[] = {
				LNA_BEND_RAMP_1,
				LNA_BEND_RAMP_2,
				LNA_BEND_RAMP_3,
				LNA_BEND_RAMP_4,
				LNA_BEND_RAMP_5
			};

			static const PF_ParamIndex BEND_HANDLE_OUT_PARAMS[] = {
				LNA_BEND_HANDLE_OUT_1,
				LNA_BEND_HANDLE_OUT_2,
				LNA_BEND_HANDLE_OUT_3,
				LNA_BEND_HANDLE_OUT_4
			};

			static const PF_ParamIndex BEND_HANDLE_IN_PARAMS[] = {
				LNA_BEND_HANDLE_IN_2,
				LNA_BEND_HANDLE_IN_3,
				LNA_BEND_HANDLE_IN_4,
				LNA_BEND_HANDLE_IN_5
			};

			PF_ParamIndex targetParamIndex = LNA_BEND_RAMP_1;
			if (s_dragHandle >= 0 && s_dragHandle < 5) {
				targetParamIndex = BEND_RAMP_PARAMS[s_dragHandle];
			} else if (s_dragHandle >= 5 && s_dragHandle < 9) {
				targetParamIndex = BEND_HANDLE_OUT_PARAMS[s_dragHandle - 5];
			} else if (s_dragHandle >= 9 && s_dragHandle < 13) {
				targetParamIndex = BEND_HANDLE_IN_PARAMS[s_dragHandle - 9];
			}

			params[targetParamIndex]->u.fs_d.value = vRaw * 100.0;
			params[targetParamIndex]->uu.change_flags = PF_ChangeFlag_CHANGED_VALUE;
			ERR(InvalidateCustomControl(in_data, event_extra));
			event_extra->evt_out_flags = PF_EO_HANDLED_EVENT | PF_EO_UPDATE_NOW | PF_EO_ALWAYS_UPDATE;
		}
	}
	return err;
}
