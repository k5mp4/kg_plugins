static PF_Err DrawSinglePath(PF_InData *in_data, PF_OutData *out_data, PF_EffectWorld *output, const LineAnimRenderInfo &baseInfo, PF_PathID pathID, PF_PixelFormat format)
{
	PF_Err err = PF_Err_NONE;
	PF_Err err2 = PF_Err_NONE;
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	LineAnimRenderInfo info = baseInfo;
	PF_PathOutlinePtr pathP = nullptr;
	PF_Boolean openB = TRUE;
	std::vector<SegInfo> pathSegments;
	PF_FpLong pathTotalLength = 0.0;

	if (!pathID || info.line_count <= 0 || info.width <= 0.0 || info.opacity <= 0.0) {
		return err;
	}

	ERR(suites.PathQuerySuite1()->PF_CheckoutPath(
		in_data->effect_ref,
		pathID,
		in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&pathP));

	if (!err && pathP) {
		ERR(suites.PathDataSuite1()->PF_PathIsOpen(in_data->effect_ref, pathP, &openB));
	}

	if (!err && pathP) {
		Vec2 twistCenter = { 0.0, 0.0 };
		ERR(ComputePathCenter(in_data, suites, pathP, twistCenter));
		info.twist_axis_x = twistCenter.x;
		info.twist_axis_y = twistCenter.y;
		if (info.enable_3d) {
			info.comp_center_x = static_cast<PF_FpLong>(output->width) * 0.5;
			info.comp_center_y = static_cast<PF_FpLong>(output->height) * 0.5;
			info.twist_axis_x += info.comp_center_x + info.position_x;
			info.twist_axis_y += info.comp_center_y + info.position_y;
		}
		info.twist_radius = std::max<PF_FpLong>(
			1.0,
			std::max<PF_FpLong>(
				std::fabs(info.spacing) * (static_cast<PF_FpLong>(info.line_count) - 1.0) * 0.5,
				info.width));
		info.sphere_z_span = std::max<PF_FpLong>(
			1.0,
			std::fabs(info.line_z_spacing) * (static_cast<PF_FpLong>(info.line_count) - 1.0) * 0.5);
		if (UseSphere3D(info)) {
			info.twist_radius = std::max(info.twist_radius, info.sphere_z_span);
		}
		info.twist_depth = 1.8;
		info.twist_loop_ramp = (openB == FALSE);
		if (!err) {
			A_Time compTime = { 0, 1 };
			PF_Err timeErr = suites.PFInterfaceSuite1()->AEGP_ConvertEffectToCompTime(
				in_data->effect_ref,
				in_data->current_time,
				in_data->time_scale,
				&compTime);
			if (!timeErr) {
				AEGP_LayerH effectLayerH = nullptr;
				A_Boolean isEffectLayer3D = FALSE;
				PF_Err layerErr = suites.PFInterfaceSuite1()->AEGP_GetEffectLayer(in_data->effect_ref, &effectLayerH);
				if (!layerErr && effectLayerH) {
					layerErr = suites.LayerSuite5()->AEGP_IsLayer3D(effectLayerH, &isEffectLayer3D);
				}
				if (!layerErr) {
					info.effect_layer_3d = isEffectLayer3D ? TRUE : FALSE;
				}
			}

			if (info.enable_3d) {
				A_Matrix4 cameraMatrix;
				A_Matrix4 cameraViewMatrix;
				A_FpLong distToImagePlane = 0.0;
				A_short imagePlaneWidth = 0;
				A_short imagePlaneHeight = 0;
				AEFX_CLR_STRUCT(cameraMatrix);
				AEFX_CLR_STRUCT(cameraViewMatrix);
				PF_Err camErr = timeErr;
				AEGP_LayerH cameraLayerH = nullptr;
				if (!camErr) {
					camErr = suites.PFInterfaceSuite1()->AEGP_GetEffectCamera(
						in_data->effect_ref,
						&compTime,
						&cameraLayerH);
				}
				if (!camErr && cameraLayerH) {
					camErr = suites.PFInterfaceSuite1()->AEGP_GetEffectCameraMatrix(
						in_data->effect_ref,
						&compTime,
						&cameraMatrix,
						&distToImagePlane,
						&imagePlaneWidth,
						&imagePlaneHeight);
				}
				if (!camErr && cameraLayerH && distToImagePlane > 1.0 && InvertMatrix4(cameraMatrix, cameraViewMatrix)) {
					info.camera_active = TRUE;
					info.camera_matrix = cameraViewMatrix;
					info.camera_dist = distToImagePlane;
					info.camera_image_width = imagePlaneWidth;
					info.camera_image_height = imagePlaneHeight;
					if (imagePlaneWidth > 0 && imagePlaneHeight > 0) {
						info.comp_center_x = static_cast<PF_FpLong>(imagePlaneWidth) * 0.5;
						info.comp_center_y = static_cast<PF_FpLong>(imagePlaneHeight) * 0.5;
					}
				}
			}
		}
	}

	if (!err && pathP) {
		A_long numSegments = 0;
		ERR(suites.PathDataSuite1()->PF_PathNumSegments(in_data->effect_ref, pathP, &numSegments));
		if (!err && numSegments > 0) {
			pathSegments.resize(static_cast<size_t>(numSegments));
			for (A_long i = 0; !err && i < numSegments; ++i) {
				SegInfo &seg = pathSegments[static_cast<size_t>(i)];
				seg.prep = nullptr;
				seg.length = 0.0;
				ERR(suites.PathDataSuite1()->PF_PathPrepareSegLength(in_data->effect_ref, pathP, i, info.curve_segments, &seg.prep));
				PF_PathSegPrepPtr prep = seg.prep;
				ERR(suites.PathDataSuite1()->PF_PathGetSegLength(in_data->effect_ref, pathP, i, &prep, &seg.length));
				pathTotalLength += std::max<PF_FpLong>(0.0, seg.length);
			}
		}
	}

	if (!err && pathP) {
		const PF_FpLong noise = info.noise_ramp;
		const PF_FpLong time = CurrentSeconds(in_data) * info.noise_speed * 0.01 + info.noise_phase * 6.283185307179586;
		const PF_FpLong center = (static_cast<PF_FpLong>(info.line_count) - 1.0) * 0.5;
		const PF_FpLong r = static_cast<PF_FpLong>(info.color.red) / static_cast<PF_FpLong>(PF_MAX_CHAN8);
		const PF_FpLong g = static_cast<PF_FpLong>(info.color.green) / static_cast<PF_FpLong>(PF_MAX_CHAN8);
		const PF_FpLong b = static_cast<PF_FpLong>(info.color.blue) / static_cast<PF_FpLong>(PF_MAX_CHAN8);
		const PF_FpLong er = static_cast<PF_FpLong>(info.end_color.red) / static_cast<PF_FpLong>(PF_MAX_CHAN8);
		const PF_FpLong eg = static_cast<PF_FpLong>(info.end_color.green) / static_cast<PF_FpLong>(PF_MAX_CHAN8);
		const PF_FpLong eb = static_cast<PF_FpLong>(info.end_color.blue) / static_cast<PF_FpLong>(PF_MAX_CHAN8);
		std::vector<std::vector<PathPolyline>> cachedPolylines(static_cast<size_t>(info.line_count));
		std::vector<char> cachedPolylineBuilt(static_cast<size_t>(info.line_count), 0);
		const bool useSharedTrimmedPolyline = TrimmedPolylineIsLineIndependent(info, openB != FALSE);
		std::vector<PathPolyline> sharedTrimmedPolylines;
		if (useSharedTrimmedPolyline) {
			ERR(BuildTrimmedPolyline(in_data, pathP, pathSegments, pathTotalLength, info, 0, time, openB != FALSE, sharedTrimmedPolylines));
		}

		if (info.web_mode != 3) {
			for (A_long line = 0; !err && line < info.line_count; ++line) {
				if (!useSharedTrimmedPolyline) {
					std::vector<PathPolyline> &cached = cachedPolylines[static_cast<size_t>(line)];
					ERR(BuildTrimmedPolyline(in_data, pathP, pathSegments, pathTotalLength, info, line, time, openB != FALSE, cached));
					if (!err) {
						cachedPolylineBuilt[static_cast<size_t>(line)] = 1;
					}
				}
				const std::vector<PathPolyline> &polylines = useSharedTrimmedPolyline ? sharedTrimmedPolylines : cachedPolylines[static_cast<size_t>(line)];
				if (err || polylines.empty()) {
					continue;
				}

				const PF_FpLong lineOffset = (static_cast<PF_FpLong>(line) - center) * info.spacing;
				const PF_FpLong lineXOffset = (static_cast<PF_FpLong>(line) - center) * info.line_x_spacing;
				const PF_FpLong lineYOffset = (static_cast<PF_FpLong>(line) - center) * info.line_y_spacing;
				const PF_FpLong lineZOffset = info.enable_3d ? (static_cast<PF_FpLong>(line) - center) * info.line_z_spacing : 0.0;
				const A_u_long seedBase = static_cast<A_u_long>(info.noise_seed * 65537 + line * 131);
				const PF_FpLong linePhase = static_cast<PF_FpLong>(line) * info.noise_line_phase * 6.283185307179586;
				const PF_FpLong offsetJitter = (noise * info.noise_position > 1.0e-6 && std::fabs(info.spacing) > 1.0e-6) ?
					SmoothNoise(seedBase + 17, time + linePhase) * info.spacing * 0.9 * noise * info.noise_position : 0.0;
				const PF_FpLong opacityMul = (noise * info.noise_opacity > 1.0e-6) ?
					ClampF(1.0 + SmoothNoise(seedBase + 23, time * 0.71 + linePhase) * 0.9 * noise * info.noise_opacity, 0.0, 4.0) : 1.0;
				const PF_FpLong waveAmp = (noise * info.noise_wave > 1.0e-6) ? info.spacing * 0.5 * noise * info.noise_wave : 0.0;
				const PF_FpLong wavePhase = (std::fabs(waveAmp) > 1.0e-6) ? Hash01(seedBase + 29) * 6.283185307179586 + time + linePhase : 0.0;
				const PF_FpLong waveFreq = 6.283185307179586 / std::max<PF_FpLong>(1.0, info.noise_scale);
				const PF_FpLong widthNoiseTime = time * 0.83 + linePhase;
				PF_FpLong lr = r, lg = g, lb = b;
				PF_FpLong ler = er, leg = eg, leb = eb;
				LineColorAt(info, line, info.line_count, r, g, b, lr, lg, lb);
				LineColorAt(info, line, info.line_count, er, eg, eb, ler, leg, leb);

				for (const PathPolyline &sourcePolyline : polylines) {
					PathPolyline mutablePolyline;
					const PathPolyline *polylineP = &sourcePolyline;
					const bool smoothPolyline = sourcePolyline.points.size() >= 3 && info.bend_smooth > 0.0 && info.line_count < 150;
					if (smoothPolyline) {
						mutablePolyline = sourcePolyline;
						ApplyBendSmoothing(mutablePolyline, info, lineOffset + offsetJitter);
						if (mutablePolyline.closed) {
							MakeClosedSeamContinuous(mutablePolyline, true);
						}
						polylineP = &mutablePolyline;
					}
					const PathPolyline &polyline = *polylineP;
					const std::vector<PathPoint> &points = polyline.points;
					const PF_FpLong dashTotal = points.size() >= 2 ? std::fabs(points.back().dist - points.front().dist) : 0.0;
					const DashPattern dash = MakeDashPattern(info, line, polyline.closed, dashTotal);
					for (size_t i = 1; i < points.size(); ++i) {
						const PathPoint &p0 = points[i - 1];
						const PathPoint &p1 = points[i];
						const PF_FpLong w0 = WaveOffsetAt(p0, polyline.closed, dashTotal, waveAmp, waveFreq, wavePhase, info);
						const PF_FpLong w1 = WaveOffsetAt(p1, polyline.closed, dashTotal, waveAmp, waveFreq, wavePhase, info);
						const PF_FpLong a0 = AudioOffsetAt(p0, info, line);
						const PF_FpLong a1 = AudioOffsetAt(p1, info, line);
						DrawDashedSegment(
							output,
							format,
							info,
							dash,
							p0,
							p1,
							lineOffset + offsetJitter + w0 + a0,
							lineOffset + offsetJitter + w1 + a1,
							lineXOffset,
							lineXOffset,
							lineYOffset,
							lineYOffset,
							lineZOffset,
							lineZOffset,
							info.width,
							seedBase + 19,
							widthNoiseTime,
							info.opacity * opacityMul,
							lr,
							lg,
							lb,
							ler,
							leg,
							leb);
					}
				}
			}
		}

		if (!err && info.web_mode != 1 && info.web_amount > 0.0) {
			if (info.web_pattern != 1) {
				std::vector<PathPolyline> patternSource;
				if (useSharedTrimmedPolyline) {
					patternSource = sharedTrimmedPolylines;
				} else {
					ERR(BuildTrimmedPolyline(in_data, pathP, pathSegments, pathTotalLength, info, 0, time, openB != FALSE, patternSource));
				}
				if (!err && !patternSource.empty()) {
					DrawWebPatternLines(output, format, info, patternSource, time, r, g, b, er, eg, eb);
				}
			} else {
			std::vector<WebPolylineRecord> webPolylines;
			const A_long webLineCount = ClampL(info.web_pattern_count, 0, 256);
			const PF_FpLong webCenter = (static_cast<PF_FpLong>(webLineCount) - 1.0) * 0.5;
			webPolylines.reserve(static_cast<size_t>(webLineCount));
			const PF_FpLong sharedTrimmedLength = useSharedTrimmedPolyline ? MeasurePolylines(sharedTrimmedPolylines) : 0.0;
			for (A_long line = 0; !err && line < webLineCount; ++line) {
				const A_long sourceLine = 0;
				if (!useSharedTrimmedPolyline && !cachedPolylineBuilt[static_cast<size_t>(sourceLine)]) {
					ERR(BuildTrimmedPolyline(in_data, pathP, pathSegments, pathTotalLength, info, sourceLine, time, openB != FALSE, cachedPolylines[static_cast<size_t>(sourceLine)]));
					if (!err) {
						cachedPolylineBuilt[static_cast<size_t>(sourceLine)] = 1;
					}
				}
				const std::vector<PathPolyline> &webSource = useSharedTrimmedPolyline ? sharedTrimmedPolylines : cachedPolylines[static_cast<size_t>(sourceLine)];
				if (err || webSource.empty()) {
					continue;
				}
				const PF_FpLong webSourceLength = useSharedTrimmedPolyline ? sharedTrimmedLength : MeasurePolylines(webSource);

				PathPoint webStart;
				PathPoint webEnd;
				const A_u_long seedBase = static_cast<A_u_long>(info.noise_seed * 65537 + line * 131);
				const PF_FpLong linePhase = static_cast<PF_FpLong>(line) * info.noise_line_phase * 6.283185307179586;
				const bool closedOppositeWeb = (info.web_target_mode == 2 && openB == FALSE);
				const PF_FpLong baseU = closedOppositeWeb ? Hash01(seedBase + 211) :
					((webLineCount <= 1) ? 0.5 : static_cast<PF_FpLong>(line) / static_cast<PF_FpLong>(webLineCount - 1));
				const PF_FpLong uJitter = (noise * info.noise_position > 1.0e-6) ?
					(closedOppositeWeb ?
						HashSigned(seedBase + 31) * 0.08 * noise * info.noise_position :
						SmoothNoise(seedBase + 31, time * 0.79 + linePhase) * 0.08 * noise * info.noise_position) : 0.0;
				const PF_FpLong u = closedOppositeWeb ? Wrap01(baseU + uJitter) : ClampF(baseU + uJitter, 0.0, 1.0);
				if (!SamplePolylinesAt(webSource, u, webSourceLength, webStart)) {
					continue;
				}

				webStart.dist = 0.0;
				webStart.u = 0.0;
				Vec2 webTarget = { info.web_target_x, info.web_target_y };
				if (closedOppositeWeb) {
					const PF_FpLong oppositeU = Wrap01(u + 0.25 + Hash01(seedBase + 223) * 0.5);
					if (!SamplePolylinesAt(webSource, oppositeU, webSourceLength, webEnd)) {
						continue;
					}
					webTarget = webEnd.pos;
				}
				const PathPolyline webPolyline = BuildNoisyWebPolyline(info, webStart, webTarget, seedBase, time + linePhase);
				if (webPolyline.points.size() < 2) {
					continue;
				}
				PF_FpLong webTrimStart = 0.0;
				PF_FpLong webTrimEnd = 1.0;
				if (!WebTrimRange(info, line, time, webTrimStart, webTrimEnd, closedOppositeWeb)) {
					continue;
				}
				const PF_FpLong opacityMul = (noise * info.noise_opacity > 1.0e-6) ?
					ClampF(1.0 + SmoothNoise(seedBase + 23, time * 0.71 + linePhase) * 0.9 * noise * info.noise_opacity, 0.0, 4.0) : 1.0;
				const PF_FpLong lineXOffset = (static_cast<PF_FpLong>(line) - webCenter) * info.line_x_spacing;
				const PF_FpLong lineYOffset = (static_cast<PF_FpLong>(line) - webCenter) * info.line_y_spacing;
				const PF_FpLong lineZOffset = info.enable_3d ? (static_cast<PF_FpLong>(line) - webCenter) * info.line_z_spacing : 0.0;
				const PF_FpLong webStartZ = lineZOffset;
				const PF_FpLong webEndZ = (info.enable_3d && closedOppositeWeb) ? -lineZOffset : lineZOffset;
				webPolylines.push_back({ webPolyline, line, webTrimStart, webTrimEnd, webStartZ, webEndZ });
				const PF_FpLong widthNoiseTime = time * 0.83 + linePhase;
				PF_FpLong lr = r, lg = g, lb = b;
				PF_FpLong ler = er, leg = eg, leb = eb;
				LineColorAt(info, line, webLineCount, r, g, b, lr, lg, lb);
				LineColorAt(info, line, webLineCount, er, eg, eb, ler, leg, leb);
				const DashPattern dash = { 0.0, 0.0, 0.0 };
				for (size_t i = 1; i < webPolyline.points.size(); ++i) {
					PathPoint p0;
					PathPoint p1;
					if (!TrimWebSegment(info, line, time, webPolyline.points[i - 1], webPolyline.points[i], p0, p1, closedOppositeWeb)) {
						continue;
					}
					std::vector<std::pair<PathPoint, PathPoint>> drawSegments;
					if (closedOppositeWeb) {
						ClipSegmentToClosedPolylines(webSource, p0, p1, drawSegments);
					} else {
						drawSegments.push_back({ p0, p1 });
					}
					for (const auto &segment : drawSegments) {
						const PF_FpLong a0 = AudioOffsetAt(segment.first, info, line);
						const PF_FpLong a1 = AudioOffsetAt(segment.second, info, line);
						DrawDashedSegment(
							output,
							format,
							info,
							dash,
							segment.first,
							segment.second,
							a0,
							a1,
							lineXOffset,
							lineXOffset,
							lineYOffset,
							lineYOffset,
							Lerp(webStartZ, webEndZ, ClampF(segment.first.u, 0.0, 1.0)),
							Lerp(webStartZ, webEndZ, ClampF(segment.second.u, 0.0, 1.0)),
							info.width,
							seedBase + 19,
							widthNoiseTime,
							info.opacity * info.web_amount * opacityMul,
							lr,
							lg,
							lb,
							ler,
							leg,
							leb);
					}
				}
			}

			if (!err && info.web_weave > 0.0 && info.web_weave_count > 0 && webPolylines.size() >= 2) {
				const A_long rings = ClampL(info.web_weave_count, 0, 64);
				for (A_long ring = 1; !err && ring <= rings; ++ring) {
					const PF_FpLong baseU = static_cast<PF_FpLong>(ring) / static_cast<PF_FpLong>(rings + 1);
					const A_u_long ringSeed = static_cast<A_u_long>(info.noise_seed * 131071 + ring * 911);
					const PF_FpLong ringJitter = (info.web_weave_random > 1.0e-6) ?
						SmoothNoise(ringSeed + 3, time * 0.57) * 0.04 * info.web_weave_random : 0.0;

					for (size_t line = 1; line < webPolylines.size(); ++line) {
						const WebPolylineRecord &prevWeb = webPolylines[line - 1];
						const WebPolylineRecord &curWeb = webPolylines[line];
						const PF_FpLong prevSpan = std::max<PF_FpLong>(0.0, prevWeb.trim_end - prevWeb.trim_start);
						const PF_FpLong curSpan = std::max<PF_FpLong>(0.0, curWeb.trim_end - curWeb.trim_start);
						const PF_FpLong prevU = ClampF(Lerp(prevWeb.trim_start, prevWeb.trim_end, baseU) + ringJitter * prevSpan, prevWeb.trim_start, prevWeb.trim_end);
						const PF_FpLong curU = ClampF(Lerp(curWeb.trim_start, curWeb.trim_end, baseU) + ringJitter * curSpan, curWeb.trim_start, curWeb.trim_end);

						PathPoint p0;
						PathPoint p1;
						if (!SamplePolylinePointAtU(prevWeb.polyline, prevU, p0) ||
							!SamplePolylinePointAtU(curWeb.polyline, curU, p1)) {
							continue;
						}

						const A_u_long seedBase = static_cast<A_u_long>(info.noise_seed * 65537 + curWeb.line_index * 131 + ring * 17);
						const PF_FpLong prevXOffset = (static_cast<PF_FpLong>(prevWeb.line_index) - webCenter) * info.line_x_spacing;
						const PF_FpLong curXOffset = (static_cast<PF_FpLong>(curWeb.line_index) - webCenter) * info.line_x_spacing;
						const PF_FpLong prevYOffset = (static_cast<PF_FpLong>(prevWeb.line_index) - webCenter) * info.line_y_spacing;
						const PF_FpLong curYOffset = (static_cast<PF_FpLong>(curWeb.line_index) - webCenter) * info.line_y_spacing;
						const PF_FpLong prevZOffset = info.enable_3d ? WebPointZAt(prevWeb, p0) : 0.0;
						const PF_FpLong curZOffset = info.enable_3d ? WebPointZAt(curWeb, p1) : 0.0;
						const PF_FpLong localWidth = info.width * ClampF(0.35 + info.web_weave * 0.65, 0.1, 1.0);
						const PF_FpLong localOpacity = info.opacity * info.web_amount * info.web_weave *
							((info.web_weave_random > 1.0e-6) ?
								ClampF(0.45 + SmoothNoise(seedBase + 67, time * 0.81) * 0.25 * info.web_weave_random, 0.05, 1.0) : 0.45);
						PF_FpLong lr = r, lg = g, lb = b;
						PF_FpLong ler = er, leg = eg, leb = eb;
						LineColorAt(info, curWeb.line_index, webLineCount, r, g, b, lr, lg, lb);
						LineColorAt(info, curWeb.line_index, webLineCount, er, eg, eb, ler, leg, leb);
						const DashPattern dash = { 0.0, 0.0, 0.0 };
						const PF_FpLong a0 = AudioOffsetAt(p0, info, prevWeb.line_index);
						const PF_FpLong a1 = AudioOffsetAt(p1, info, curWeb.line_index);
						DrawDashedSegment(
							output,
							format,
							info,
							dash,
							p0,
							p1,
							a0,
							a1,
							prevXOffset,
							curXOffset,
							prevYOffset,
							curYOffset,
							prevZOffset,
							curZOffset,
							localWidth,
							seedBase + 19,
							time * 0.83,
							localOpacity,
							lr,
							lg,
							lb,
							ler,
							leg,
							leb);
					}
				}
			}
			}
		}
	}

	for (A_long i = 0; i < static_cast<A_long>(pathSegments.size()); ++i) {
		if (pathSegments[static_cast<size_t>(i)].prep) {
			PF_PathSegPrepPtr prep = pathSegments[static_cast<size_t>(i)].prep;
			ERR2(suites.PathDataSuite1()->PF_PathCleanupSegLength(in_data->effect_ref, pathP, i, &prep));
		}
	}

	if (pathP) {
		ERR2(suites.PathQuerySuite1()->PF_CheckinPath(in_data->effect_ref, pathID, FALSE, pathP));
	}

	return err ? err : err2;
}

static PF_Err DrawLines(PF_InData *in_data, PF_OutData *out_data, PF_EffectWorld *output, const LineAnimRenderInfo &info, PF_PixelFormat format)
{
	PF_Err err = PF_Err_NONE;

	for (A_long i = 0; !err && i < info.path_count; ++i) {
		ERR(DrawSinglePath(in_data, out_data, output, info, info.path_ids[i], format));
	}

	return err;
}

static PF_Err RenderCommon(PF_InData *in_data, PF_OutData *out_data, PF_EffectWorld *output, const LineAnimRenderInfo &info)
{
	PF_Err err = PF_Err_NONE;
	PF_PixelFormat format = PF_PixelFormat_ARGB32;

	AEFX_SuiteScoper<PF_WorldSuite2> worldSuite(in_data, kPFWorldSuite, kPFWorldSuiteVersion2, out_data);
	ERR(worldSuite->PF_GetPixelFormat(output, &format));

	ERR(ClearWorld(in_data, out_data, output, format));
	ERR(DrawLines(in_data, out_data, output, info, format));

	return err;
}

static PF_Err Render(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], PF_LayerDef *output)
{
	LineAnimRenderInfo info = InfoFromParams(params);
	BuildAudioReactiveData(in_data, &info);
	return RenderCommon(in_data, out_data, output, info);
}

static PF_Err PreRender(PF_InData *in_data, PF_OutData *out_data, PF_PreRenderExtra *extra)
{
	PF_Err err = PF_Err_NONE;

	LineAnimRenderInfo *infoP = new (std::nothrow) LineAnimRenderInfo;
	if (!infoP) {
		return PF_Err_OUT_OF_MEMORY;
	}

	ERR(BuildInfoAtTime(in_data, infoP));

	if (err) {
		delete infoP;
		return err;
	}

	extra->output->pre_render_data = infoP;
	extra->output->delete_pre_render_data_func = [](void *data) {
		delete reinterpret_cast<LineAnimRenderInfo *>(data);
	};
	SetFullRect(extra->output->result_rect, in_data->width, in_data->height);
	SetFullRect(extra->output->max_result_rect, in_data->width, in_data->height);
	extra->output->solid = FALSE;

	return err;
}

static PF_Err SmartRender(PF_InData *in_data, PF_OutData *out_data, PF_SmartRenderExtra *extra)
{
	PF_Err err = PF_Err_NONE;
	PF_EffectWorld *output = nullptr;

	LineAnimRenderInfo *infoP = reinterpret_cast<LineAnimRenderInfo *>(extra->input->pre_render_data);

	ERR(extra->cb->checkout_output(in_data->effect_ref, &output));
	if (!err && output && infoP) {
		ERR(RenderCommon(in_data, out_data, output, *infoP));
	}

	return err;
}
