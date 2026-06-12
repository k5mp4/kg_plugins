static PF_Err ClearWorld(PF_InData *in_data, PF_OutData *out_data, PF_EffectWorld *world, PF_PixelFormat format)
{
	AEFX_SuiteScoper<PF_FillMatteSuite2> fillSuite(in_data, kPFFillMatteSuite, kPFFillMatteSuiteVersion2, out_data);

	if (format == PF_PixelFormat_ARGB128) {
		PF_PixelFloat clear = { 0, 0, 0, 0 };
		return fillSuite->fill_float(in_data->effect_ref, &clear, nullptr, world);
	}
	if (format == PF_PixelFormat_ARGB64) {
		PF_Pixel16 clear = { 0, 0, 0, 0 };
		return fillSuite->fill16(in_data->effect_ref, &clear, nullptr, world);
	}

	PF_Pixel clear = { 0, 0, 0, 0 };
	return fillSuite->fill(in_data->effect_ref, &clear, nullptr, world);
}

static PF_Err EvalPathAt(
	PF_InData *in_data,
	AEGP_SuiteHandler &suites,
	PF_PathOutlinePtr pathP,
	const std::vector<SegInfo> &segments,
	PF_FpLong pathDistance,
	PF_FpLong totalLength,
	PathPoint &point)
{
	PF_Err err = PF_Err_NONE;
	PF_FpLong d = ClampF(pathDistance, 0.0, totalLength);

	for (A_long i = 0; i < static_cast<A_long>(segments.size()); ++i) {
		const PF_FpLong segLen = segments[static_cast<size_t>(i)].length;
		if (d <= segLen || i == static_cast<A_long>(segments.size()) - 1) {
			PF_FpLong x = 0.0, y = 0.0, dx = 1.0, dy = 0.0;
			PF_PathSegPrepPtr prep = segments[static_cast<size_t>(i)].prep;
			ERR(suites.PathDataSuite1()->PF_PathEvalSegLengthDeriv1(
				in_data->effect_ref,
				pathP,
				&prep,
				i,
				ClampF(d, 0.0, segLen),
				&x,
				&y,
				&dx,
				&dy));
			point.pos = { x, y };
			Vec2 tangent = { dx, dy };
			Normalize(tangent);
			point.normal = { -tangent.y, tangent.x };
			point.dist = pathDistance;
			point.u = (totalLength > 0.0) ? ClampF(pathDistance / totalLength, 0.0, 1.0) : 0.0;
			point.turn_angle = 0.0;
			return err;
		}
		d -= segLen;
	}

	return err;
}

static PF_Err ComputePathCenter(
	PF_InData *in_data,
	AEGP_SuiteHandler &suites,
	PF_PathOutlinePtr pathP,
	Vec2 &center)
{
	PF_Err err = PF_Err_NONE;
	PF_Err err2 = PF_Err_NONE;
	A_long numSegments = 0;
	center = { 0.0, 0.0 };

	ERR(suites.PathDataSuite1()->PF_PathNumSegments(in_data->effect_ref, pathP, &numSegments));
	if (err || numSegments <= 0) {
		return err;
	}

	std::vector<SegInfo> segments(static_cast<size_t>(numSegments));
	PF_FpLong minX = 0.0;
	PF_FpLong minY = 0.0;
	PF_FpLong maxX = 0.0;
	PF_FpLong maxY = 0.0;
	bool havePoint = false;

	for (A_long i = 0; !err && i < numSegments; ++i) {
		segments[static_cast<size_t>(i)].prep = nullptr;
		segments[static_cast<size_t>(i)].length = 0.0;
		ERR(suites.PathDataSuite1()->PF_PathPrepareSegLength(in_data->effect_ref, pathP, i, 32, &segments[static_cast<size_t>(i)].prep));
		PF_PathSegPrepPtr prep = segments[static_cast<size_t>(i)].prep;
		ERR(suites.PathDataSuite1()->PF_PathGetSegLength(in_data->effect_ref, pathP, i, &prep, &segments[static_cast<size_t>(i)].length));

		const A_long samples = std::max<A_long>(2, static_cast<A_long>(std::ceil(segments[static_cast<size_t>(i)].length / 12.0)) + 1);
		for (A_long j = 0; !err && j < samples; ++j) {
			PF_FpLong x = 0.0;
			PF_FpLong y = 0.0;
			const PF_FpLong t = (samples <= 1) ? 0.0 : static_cast<PF_FpLong>(j) / static_cast<PF_FpLong>(samples - 1);
			PF_PathSegPrepPtr evalPrep = segments[static_cast<size_t>(i)].prep;
			ERR(suites.PathDataSuite1()->PF_PathEvalSegLength(
				in_data->effect_ref,
				pathP,
				&evalPrep,
				i,
				segments[static_cast<size_t>(i)].length * t,
				&x,
				&y));
			if (!err) {
				if (!havePoint) {
					minX = maxX = x;
					minY = maxY = y;
					havePoint = true;
				} else {
					minX = std::min(minX, x);
					minY = std::min(minY, y);
					maxX = std::max(maxX, x);
					maxY = std::max(maxY, y);
				}
			}
		}
	}

	for (A_long i = 0; i < numSegments; ++i) {
		if (segments[static_cast<size_t>(i)].prep) {
			PF_PathSegPrepPtr prep = segments[static_cast<size_t>(i)].prep;
			ERR2(suites.PathDataSuite1()->PF_PathCleanupSegLength(in_data->effect_ref, pathP, i, &prep));
		}
	}

	if (!err && havePoint) {
		center = { (minX + maxX) * 0.5, (minY + maxY) * 0.5 };
	}
	return err ? err : err2;
}

static PF_Err BuildTrimmedPolyline(
	PF_InData *in_data,
	PF_PathOutlinePtr pathP,
	const std::vector<SegInfo> &segments,
	PF_FpLong totalLength,
	const LineAnimRenderInfo &info,
	A_long lineIndex,
	PF_FpLong time,
	bool openPath,
	std::vector<PathPolyline> &polylines)
{
	PF_Err err = PF_Err_NONE;
	AEGP_SuiteHandler suites(in_data->pica_basicP);

	polylines.clear();
	if (segments.empty() || totalLength <= 0.0) {
		return err;
	}

	if (!err) {
		const PF_FpLong n = info.noise_ramp;
		const PF_FpLong trimNoise = n * info.noise_trim;
		const bool zeroTrim = std::fabs(info.end - info.start) < 1.0e-6;
		const bool fullClosedTrim = !openPath && info.start <= 1.0e-6 && info.end >= 1.0 - 1.0e-6;
		const bool keepTrimStable = zeroTrim || fullClosedTrim;
		const PF_FpLong headRand = keepTrimStable ? 0.0 : HashSigned(static_cast<A_u_long>(info.noise_seed + lineIndex * 47 + 11)) * 0.18 * info.trim_head_random;
		const PF_FpLong tailRand = keepTrimStable ? 0.0 : HashSigned(static_cast<A_u_long>(info.noise_seed + lineIndex * 53 + 13)) * 0.18 * info.trim_tail_random;
		const PF_FpLong noiseStart = keepTrimStable ? 0.0 : SmoothNoise(static_cast<A_u_long>(info.noise_seed + lineIndex * 41 + 5), time * 0.61) * 0.025 * trimNoise;
		const PF_FpLong noiseEnd = keepTrimStable ? 0.0 : SmoothNoise(static_cast<A_u_long>(info.noise_seed + lineIndex * 43 + 7), time * 0.67) * 0.025 * trimNoise;
		const PF_FpLong localStart = ClampF(info.start + headRand + noiseStart, 0.0, 1.0);
		const PF_FpLong localEnd = ClampF(info.end + tailRand + noiseEnd, 0.0, 1.0);
		PF_FpLong span = localEnd - localStart;
		if (std::fabs(span) < 1.0e-6) {
			span = 0.0;
		}

		std::vector<TrimRange> ranges;
		if (span >= 0.999) {
			ranges.push_back({ 0.0, 1.0, false });
		} else if (span <= 0.0) {
			ranges.clear();
		} else if (openPath) {
			const PF_FpLong start = ClampF(localStart + info.offset, 0.0, 1.0);
			const PF_FpLong end = ClampF(localEnd + info.offset, 0.0, 1.0);
			if (end > start) {
				ranges.push_back({ start, end, false });
			}
		} else {
			const PF_FpLong start = Wrap01(localStart + info.offset);
			const PF_FpLong end = Wrap01(localEnd + info.offset);
			if (start <= end) {
				ranges.push_back({ start, end, false });
			} else {
				ranges.push_back({ start, 1.0, false });
				ranges.push_back({ 0.0, end, true });
			}
		}

		const PF_FpLong curveQualityScale = ClampF(64.0 / static_cast<PF_FpLong>(std::max<A_long>(4, info.curve_segments)), 0.25, 4.0);
		const PF_FpLong baseStep = 4.0;
		const PF_FpLong sampleStep = ClampF(baseStep * curveQualityScale, 1.0, 16.0);
		for (const TrimRange &range : ranges) {
			PathPolyline polyline;
			const bool fullClosedRange = !openPath && range.start == 0.0 && range.end == 1.0;
			polyline.closed = fullClosedRange;
			if (range.continue_previous && !polylines.empty()) {
				polyline = polylines.back();
				polylines.pop_back();
			}
			const PF_FpLong d0 = range.start * totalLength;
			const PF_FpLong d1 = range.end * totalLength;
			const A_long samples = std::max<A_long>(2, static_cast<A_long>(std::ceil((d1 - d0) / sampleStep)) + 1);
			for (A_long i = 0; !err && i < samples; ++i) {
				const PF_FpLong t = (samples <= 1) ? 0.0 : static_cast<PF_FpLong>(i) / static_cast<PF_FpLong>(samples - 1);
				PathPoint p;
				const PF_FpLong logicalDistance = Lerp(d0, d1, t);
				const PF_FpLong phaseOffset = (info.phase / (2.0 * 3.141592653589793)) * totalLength;
				PF_FpLong sampleDistance = logicalDistance + info.path_offset * totalLength + phaseOffset;
				sampleDistance = openPath ? ClampF(sampleDistance, 0.0, totalLength) : WrapDistance(sampleDistance, totalLength);
				ERR(EvalPathAt(in_data, suites, pathP, segments, sampleDistance, totalLength, p));
				if (!err) {
					if (fullClosedRange) {
						p.dist = logicalDistance;
						p.u = (totalLength > 0.0) ? ClampF(logicalDistance / totalLength, 0.0, 1.0) : 0.0;
					}
					p.turn_angle = 0.0;
					if (range.continue_previous && i == 0 && !polyline.points.empty()) {
						PathPoint &seam = polyline.points.back();
						seam.pos = { (seam.pos.x + p.pos.x) * 0.5, (seam.pos.y + p.pos.y) * 0.5 };
						seam.normal = { seam.normal.x + p.normal.x, seam.normal.y + p.normal.y };
						Normalize(seam.normal);
						continue;
					}
					polyline.points.push_back(p);
				}
			}
			if (!err && polyline.points.size() >= 2) {
				if (polyline.closed) {
					MakeClosedSeamContinuous(polyline, true);
				}
				ApplyPathTurnAngles(polyline, info);
				polylines.push_back(polyline);
			}
		}
	}

	return err;
}

static void ComposeColor(PF_FpLong srcAlpha, PF_FpLong &r, PF_FpLong &g, PF_FpLong &b, PF_FpLong &a, PF_FpLong dstR, PF_FpLong dstG, PF_FpLong dstB, PF_FpLong dstA, A_long mode)
{
	srcAlpha = ClampF(srcAlpha, 0.0, 1.0);
	r = ClampF(r, 0.0, 1.0);
	g = ClampF(g, 0.0, 1.0);
	b = ClampF(b, 0.0, 1.0);

	if (mode == 2) {
		a = ClampF(dstA + srcAlpha, 0.0, 1.0);
		r = ClampF(dstR + r * srcAlpha, 0.0, 1.0);
		g = ClampF(dstG + g * srcAlpha, 0.0, 1.0);
		b = ClampF(dstB + b * srcAlpha, 0.0, 1.0);
	} else if (mode == 3) {
		a = srcAlpha + dstA * (1.0 - srcAlpha);
		r = 1.0 - (1.0 - dstR) * (1.0 - r * srcAlpha);
		g = 1.0 - (1.0 - dstG) * (1.0 - g * srcAlpha);
		b = 1.0 - (1.0 - dstB) * (1.0 - b * srcAlpha);
	} else if (mode == 4) {
		a = srcAlpha + dstA * (1.0 - srcAlpha);
		r = dstR * (1.0 - srcAlpha + r * srcAlpha);
		g = dstG * (1.0 - srcAlpha + g * srcAlpha);
		b = dstB * (1.0 - srcAlpha + b * srcAlpha);
	} else if (mode == 5) {
		a = std::max(dstA, srcAlpha);
		r = std::max(dstR, r * srcAlpha);
		g = std::max(dstG, g * srcAlpha);
		b = std::max(dstB, b * srcAlpha);
	} else {
		a = srcAlpha + dstA * (1.0 - srcAlpha);
		r = r * srcAlpha + dstR * (1.0 - srcAlpha);
		g = g * srcAlpha + dstG * (1.0 - srcAlpha);
		b = b * srcAlpha + dstB * (1.0 - srcAlpha);
	}
}

template <typename PIX, typename CHAN, CHAN MAX_CHAN>
static void BlendPixel(PIX *px, PF_FpLong srcAlpha, PF_FpLong r, PF_FpLong g, PF_FpLong b, A_long mode)
{
	if (mode == 1 && px->alpha == 0) {
		px->alpha = static_cast<CHAN>(ClampF(srcAlpha, 0.0, 1.0) * static_cast<PF_FpLong>(MAX_CHAN) + 0.5);
		px->red = static_cast<CHAN>(ClampF(r * srcAlpha, 0.0, 1.0) * static_cast<PF_FpLong>(MAX_CHAN) + 0.5);
		px->green = static_cast<CHAN>(ClampF(g * srcAlpha, 0.0, 1.0) * static_cast<PF_FpLong>(MAX_CHAN) + 0.5);
		px->blue = static_cast<CHAN>(ClampF(b * srcAlpha, 0.0, 1.0) * static_cast<PF_FpLong>(MAX_CHAN) + 0.5);
		return;
	}

	const PF_FpLong dstR = static_cast<PF_FpLong>(px->red) / static_cast<PF_FpLong>(MAX_CHAN);
	const PF_FpLong dstG = static_cast<PF_FpLong>(px->green) / static_cast<PF_FpLong>(MAX_CHAN);
	const PF_FpLong dstB = static_cast<PF_FpLong>(px->blue) / static_cast<PF_FpLong>(MAX_CHAN);
	const PF_FpLong dstA = static_cast<PF_FpLong>(px->alpha) / static_cast<PF_FpLong>(MAX_CHAN);
	PF_FpLong outA = 0.0;

	ComposeColor(srcAlpha, r, g, b, outA, dstR, dstG, dstB, dstA, mode);
	px->alpha = static_cast<CHAN>(ClampF(outA, 0.0, 1.0) * static_cast<PF_FpLong>(MAX_CHAN) + 0.5);
	px->red = static_cast<CHAN>(ClampF(r, 0.0, 1.0) * static_cast<PF_FpLong>(MAX_CHAN) + 0.5);
	px->green = static_cast<CHAN>(ClampF(g, 0.0, 1.0) * static_cast<PF_FpLong>(MAX_CHAN) + 0.5);
	px->blue = static_cast<CHAN>(ClampF(b, 0.0, 1.0) * static_cast<PF_FpLong>(MAX_CHAN) + 0.5);
}

static void BlendPixelFloat(PF_PixelFloat *px, PF_FpLong srcAlpha, PF_FpLong r, PF_FpLong g, PF_FpLong b, A_long mode)
{
	if (mode == 1 && px->alpha <= 0.0f) {
		px->alpha = static_cast<PF_FpShort>(ClampF(srcAlpha, 0.0, 1.0));
		px->red = static_cast<PF_FpShort>(ClampF(r * srcAlpha, 0.0, 1.0));
		px->green = static_cast<PF_FpShort>(ClampF(g * srcAlpha, 0.0, 1.0));
		px->blue = static_cast<PF_FpShort>(ClampF(b * srcAlpha, 0.0, 1.0));
		return;
	}

	PF_FpLong outA = 0.0;
	ComposeColor(srcAlpha, r, g, b, outA, px->red, px->green, px->blue, px->alpha, mode);
	px->alpha = static_cast<PF_FpShort>(ClampF(outA, 0.0, 1.0));
	px->red = static_cast<PF_FpShort>(ClampF(r, 0.0, 1.0));
	px->green = static_cast<PF_FpShort>(ClampF(g, 0.0, 1.0));
	px->blue = static_cast<PF_FpShort>(ClampF(b, 0.0, 1.0));
}

template <typename PIX, typename CHAN, CHAN MAX_CHAN>
static void RasterizeSegmentT(
	PF_EffectWorld *world,
	const Vec2 &a,
	const Vec2 &b,
	PF_FpLong width,
	PF_FpLong opacity,
	PF_FpLong r0,
	PF_FpLong g0,
	PF_FpLong b0,
	PF_FpLong r1,
	PF_FpLong g1,
	PF_FpLong b1,
	A_long blendMode)
{
	const PF_FpLong vx = b.x - a.x;
	const PF_FpLong vy = b.y - a.y;
	const PF_FpLong len2 = vx * vx + vy * vy;
	if (len2 <= 1.0e-8 || width <= 0.0 || opacity <= 0.0) {
		return;
	}

	const PF_FpLong radius = width * 0.5;
	if (SegmentBoundsOutside(a, b, radius, world)) {
		return;
	}
	const A_long minX = ClampL(static_cast<A_long>(std::floor(std::min(a.x, b.x) - radius - 1.0)), 0, world->width - 1);
	const A_long maxX = ClampL(static_cast<A_long>(std::ceil(std::max(a.x, b.x) + radius + 1.0)), 0, world->width - 1);
	const A_long minY = ClampL(static_cast<A_long>(std::floor(std::min(a.y, b.y) - radius - 1.0)), 0, world->height - 1);
	const A_long maxY = ClampL(static_cast<A_long>(std::ceil(std::max(a.y, b.y) + radius + 1.0)), 0, world->height - 1);
	const A_long widthTrue = world->rowbytes / sizeof(PIX);
	const PF_FpLong invLen2 = 1.0 / len2;
	const PF_FpLong len = std::sqrt(len2);
	const PF_FpLong invLen = 1.0 / len;
	const PF_FpLong tx = vx * invLen;
	const PF_FpLong ty = vy * invLen;
	const PF_FpLong dr = r1 - r0;
	const PF_FpLong dg = g1 - g0;
	const PF_FpLong db = b1 - b0;

	for (A_long y = minY; y <= maxY; ++y) {
		A_long rowMinX = minX;
		A_long rowMaxX = maxX;
		if (!SegmentScanlineXRange(a, len, tx, ty, radius, minX, maxX, y, rowMinX, rowMaxX)) {
			continue;
		}
		PIX *row = reinterpret_cast<PIX *>(world->data) + y * widthTrue;
		const PF_FpLong py = static_cast<PF_FpLong>(y) + 0.5;
		for (A_long x = rowMinX; x <= rowMaxX; ++x) {
			const PF_FpLong px = static_cast<PF_FpLong>(x) + 0.5;
			const PF_FpLong t = ClampF(((px - a.x) * vx + (py - a.y) * vy) * invLen2, 0.0, 1.0);
			const PF_FpLong cx = a.x + vx * t;
			const PF_FpLong cy = a.y + vy * t;
			const PF_FpLong ddx = px - cx;
			const PF_FpLong ddy = py - cy;
			const PF_FpLong coverage = CoverageFromDist2(ddx * ddx + ddy * ddy, radius);
			if (coverage > 0.0) {
				BlendPixel<PIX, CHAN, MAX_CHAN>(
					&row[x],
					coverage * opacity,
					r0 + dr * t,
					g0 + dg * t,
					b0 + db * t,
					blendMode);
			}
		}
	}
}

template <typename PIX, typename CHAN, CHAN MAX_CHAN>
static void RasterizeSegmentVariableT(
	PF_EffectWorld *world,
	const LineAnimRenderInfo &info,
	const Vec2 &a,
	const Vec2 &b,
	PF_FpLong z0,
	PF_FpLong z1,
	PF_FpLong width0,
	PF_FpLong width1,
	PF_FpLong opacity,
	PF_FpLong r0,
	PF_FpLong g0,
	PF_FpLong b0,
	PF_FpLong r1,
	PF_FpLong g1,
	PF_FpLong b1,
	A_long blendMode)
{
	const PF_FpLong vx = b.x - a.x;
	const PF_FpLong vy = b.y - a.y;
	const PF_FpLong len2 = vx * vx + vy * vy;
	const PF_FpLong maxWidth = std::max(width0, width1);
	if (len2 <= 1.0e-8 || maxWidth <= 0.0 || opacity <= 0.0) {
		return;
	}

	const PF_FpLong radius = maxWidth * 0.5;
	if (SegmentBoundsOutside(a, b, radius, world)) {
		return;
	}
	const A_long minX = ClampL(static_cast<A_long>(std::floor(std::min(a.x, b.x) - radius - 1.0)), 0, world->width - 1);
	const A_long maxX = ClampL(static_cast<A_long>(std::ceil(std::max(a.x, b.x) + radius + 1.0)), 0, world->width - 1);
	const A_long minY = ClampL(static_cast<A_long>(std::floor(std::min(a.y, b.y) - radius - 1.0)), 0, world->height - 1);
	const A_long maxY = ClampL(static_cast<A_long>(std::ceil(std::max(a.y, b.y) + radius + 1.0)), 0, world->height - 1);
	const A_long widthTrue = world->rowbytes / sizeof(PIX);
	const PF_FpLong invLen2 = 1.0 / len2;
	const PF_FpLong dWidth = width1 - width0;
	const PF_FpLong dz = z1 - z0;
	const PF_FpLong dr = r1 - r0;
	const PF_FpLong dg = g1 - g0;
	const PF_FpLong db = b1 - b0;

	const PF_FpLong len    = std::sqrt(len2);
	const PF_FpLong invLen = 1.0 / len;
	const PF_FpLong tx = vx * invLen;
	const PF_FpLong ty = vy * invLen;

	const bool useGrunge = (info.grunge_amount > 1.0e-4);
	const A_u_long grungeSeed = static_cast<A_u_long>(info.noise_seed) * 0x9e3779b9U + 0x517cc1b7U;
	const PF_FpLong grungeSeedOff = a.x * 17.3 + a.y * 13.7;
	const A_u_long gSeed = grungeSeed + static_cast<A_u_long>(static_cast<A_long>(grungeSeedOff * 100.0));

	for (A_long y = minY; y <= maxY; ++y) {
		A_long rowMinX = minX;
		A_long rowMaxX = maxX;
		if (!SegmentScanlineXRange(a, len, tx, ty, radius, minX, maxX, y, rowMinX, rowMaxX)) {
			continue;
		}
		PIX *row = reinterpret_cast<PIX *>(world->data) + y * widthTrue;
		const PF_FpLong py = static_cast<PF_FpLong>(y) + 0.5;
		for (A_long x = rowMinX; x <= rowMaxX; ++x) {
			const PF_FpLong px = static_cast<PF_FpLong>(x) + 0.5;
			const PF_FpLong t = ClampF(((px - a.x) * vx + (py - a.y) * vy) * invLen2, 0.0, 1.0);
			const PF_FpLong cx = a.x + vx * t;
			const PF_FpLong cy = a.y + vy * t;
			const PF_FpLong localRadius = (width0 + dWidth * t) * 0.5;
			const PF_FpLong ddx = px - cx;
			const PF_FpLong ddy = py - cy;
			const PF_FpLong coverage = CoverageFromDist2(ddx * ddx + ddy * ddy, localRadius);
			if (coverage > 0.0) {
				PF_FpLong finalCov = coverage;
				if (useGrunge) {
					const PF_FpLong along = (px - a.x) * tx + (py - a.y) * ty;
					const PF_FpLong cross = (px - a.x) * (-ty) + (py - a.y) * tx;
					const PF_FpLong gc = GrungeCoverage(gSeed, along, cross, localRadius,
						info.grunge_amount, info.grunge_roughness,
						info.grunge_frequency, info.grunge_jitter);
					finalCov *= gc;
				}
				if (finalCov > 0.0) {
					BlendPixel<PIX, CHAN, MAX_CHAN>(&row[x], finalCov * opacity, r0 + dr * t, g0 + dg * t, b0 + db * t, blendMode);
				}
			}
		}
	}
}

template <typename PIX, typename CHAN, CHAN MAX_CHAN>
static void RasterizeCircleT(
	PF_EffectWorld *world,
	const LineAnimRenderInfo &info,
	const Vec2 &center,
	PF_FpLong viewZ,
	PF_FpLong width,
	PF_FpLong opacity,
	PF_FpLong r,
	PF_FpLong g,
	PF_FpLong b,
	A_long blendMode)
{
	if (width <= 0.0 || opacity <= 0.0) {
		return;
	}
	const PF_FpLong radius = width * 0.5;
	if (CircleBoundsOutside(center, radius, world)) {
		return;
	}
	const A_long minX = ClampL(static_cast<A_long>(std::floor(center.x - radius - 1.0)), 0, world->width - 1);
	const A_long maxX = ClampL(static_cast<A_long>(std::ceil(center.x + radius + 1.0)), 0, world->width - 1);
	const A_long minY = ClampL(static_cast<A_long>(std::floor(center.y - radius - 1.0)), 0, world->height - 1);
	const A_long maxY = ClampL(static_cast<A_long>(std::ceil(center.y + radius + 1.0)), 0, world->height - 1);
	const A_long widthTrue = world->rowbytes / sizeof(PIX);
	for (A_long y = minY; y <= maxY; ++y) {
		PIX *row = reinterpret_cast<PIX *>(world->data) + y * widthTrue;
		const PF_FpLong dy = static_cast<PF_FpLong>(y) + 0.5 - center.y;
		for (A_long x = minX; x <= maxX; ++x) {
			const PF_FpLong dx = static_cast<PF_FpLong>(x) + 0.5 - center.x;
			const PF_FpLong coverage = CoverageFromDist2(dx * dx + dy * dy, radius);
			if (coverage > 0.0) {
				BlendPixel<PIX, CHAN, MAX_CHAN>(&row[x], coverage * opacity, r, g, b, blendMode);
			}
		}
	}
}

template <typename PIX, typename CHAN, CHAN MAX_CHAN>
static void RasterizeTapeSegmentVariableT(
	PF_EffectWorld *world,
	const LineAnimRenderInfo &info,
	const Vec2 &a,
	const Vec2 &b,
	PF_FpLong z0,
	PF_FpLong z1,
	PF_FpLong width0,
	PF_FpLong width1,
	PF_FpLong opacity,
	PF_FpLong r0,
	PF_FpLong g0,
	PF_FpLong b0,
	PF_FpLong r1,
	PF_FpLong g1,
	PF_FpLong b1,
	A_long blendMode)
{
	const PF_FpLong vx = b.x - a.x;
	const PF_FpLong vy = b.y - a.y;
	const PF_FpLong len2 = vx * vx + vy * vy;
	const PF_FpLong len = std::sqrt(len2);
	const PF_FpLong maxWidth = std::max(width0, width1);
	if (len <= 1.0e-4 || maxWidth <= 0.0 || opacity <= 0.0) {
		return;
	}

	const PF_FpLong radius = maxWidth * 0.5;
	if (SegmentBoundsOutside(a, b, radius, world)) {
		return;
	}
	const A_long minX = ClampL(static_cast<A_long>(std::floor(std::min(a.x, b.x) - radius - 1.0)), 0, world->width - 1);
	const A_long maxX = ClampL(static_cast<A_long>(std::ceil(std::max(a.x, b.x) + radius + 1.0)), 0, world->width - 1);
	const A_long minY = ClampL(static_cast<A_long>(std::floor(std::min(a.y, b.y) - radius - 1.0)), 0, world->height - 1);
	const A_long maxY = ClampL(static_cast<A_long>(std::ceil(std::max(a.y, b.y) + radius + 1.0)), 0, world->height - 1);
	const A_long widthTrue = world->rowbytes / sizeof(PIX);
	const PF_FpLong tx = vx / len;
	const PF_FpLong ty = vy / len;
	const PF_FpLong invLen = 1.0 / len;
	const PF_FpLong dWidth = width1 - width0;
	const PF_FpLong dz = z1 - z0;
	const PF_FpLong dr = r1 - r0;
	const PF_FpLong dg = g1 - g0;
	const PF_FpLong db = b1 - b0;

	for (A_long y = minY; y <= maxY; ++y) {
		A_long rowMinX = minX;
		A_long rowMaxX = maxX;
		if (!SegmentScanlineXRange(a, len, tx, ty, radius, minX, maxX, y, rowMinX, rowMaxX)) {
			continue;
		}
		PIX *row = reinterpret_cast<PIX *>(world->data) + y * widthTrue;
		const PF_FpLong dyBase = static_cast<PF_FpLong>(y) + 0.5 - a.y;
		for (A_long x = rowMinX; x <= rowMaxX; ++x) {
			const PF_FpLong dx = static_cast<PF_FpLong>(x) + 0.5 - a.x;
			const PF_FpLong dy = dyBase;
			const PF_FpLong along = dx * tx + dy * ty;
			const PF_FpLong t = ClampF(along * invLen, 0.0, 1.0);
			const PF_FpLong cross = std::fabs(dx * (-ty) + dy * tx);
			const PF_FpLong localRadius = (width0 + dWidth * t) * 0.5;
			const PF_FpLong sideCoverage = localRadius + 0.5 - cross;
			const PF_FpLong startCoverage = along + radius + 0.5;
			const PF_FpLong endCoverage = len - along + radius + 0.5;
			const PF_FpLong coverage = ClampF(std::min(sideCoverage, std::min(startCoverage, endCoverage)), 0.0, 1.0);
			if (coverage > 0.0) {
				BlendPixel<PIX, CHAN, MAX_CHAN>(&row[x], coverage * opacity, r0 + dr * t, g0 + dg * t, b0 + db * t, blendMode);
			}
		}
	}
}

static void RasterizeSegmentFloat(
	PF_EffectWorld *world,
	const Vec2 &a,
	const Vec2 &b,
	PF_FpLong width,
	PF_FpLong opacity,
	PF_FpLong r0,
	PF_FpLong g0,
	PF_FpLong b0,
	PF_FpLong r1,
	PF_FpLong g1,
	PF_FpLong b1,
	A_long blendMode)
{
	const PF_FpLong vx = b.x - a.x;
	const PF_FpLong vy = b.y - a.y;
	const PF_FpLong len2 = vx * vx + vy * vy;
	if (len2 <= 1.0e-8 || width <= 0.0 || opacity <= 0.0) {
		return;
	}

	const PF_FpLong radius = width * 0.5;
	if (SegmentBoundsOutside(a, b, radius, world)) {
		return;
	}
	const A_long minX = ClampL(static_cast<A_long>(std::floor(std::min(a.x, b.x) - radius - 1.0)), 0, world->width - 1);
	const A_long maxX = ClampL(static_cast<A_long>(std::ceil(std::max(a.x, b.x) + radius + 1.0)), 0, world->width - 1);
	const A_long minY = ClampL(static_cast<A_long>(std::floor(std::min(a.y, b.y) - radius - 1.0)), 0, world->height - 1);
	const A_long maxY = ClampL(static_cast<A_long>(std::ceil(std::max(a.y, b.y) + radius + 1.0)), 0, world->height - 1);
	const A_long widthTrue = world->rowbytes / sizeof(PF_PixelFloat);
	const PF_FpLong invLen2 = 1.0 / len2;
	const PF_FpLong len = std::sqrt(len2);
	const PF_FpLong invLen = 1.0 / len;
	const PF_FpLong tx = vx * invLen;
	const PF_FpLong ty = vy * invLen;
	const PF_FpLong dr = r1 - r0;
	const PF_FpLong dg = g1 - g0;
	const PF_FpLong db = b1 - b0;

	for (A_long y = minY; y <= maxY; ++y) {
		A_long rowMinX = minX;
		A_long rowMaxX = maxX;
		if (!SegmentScanlineXRange(a, len, tx, ty, radius, minX, maxX, y, rowMinX, rowMaxX)) {
			continue;
		}
		PF_PixelFloat *row = reinterpret_cast<PF_PixelFloat *>(world->data) + y * widthTrue;
		const PF_FpLong py = static_cast<PF_FpLong>(y) + 0.5;
		for (A_long x = rowMinX; x <= rowMaxX; ++x) {
			const PF_FpLong px = static_cast<PF_FpLong>(x) + 0.5;
			const PF_FpLong t = ClampF(((px - a.x) * vx + (py - a.y) * vy) * invLen2, 0.0, 1.0);
			const PF_FpLong cx = a.x + vx * t;
			const PF_FpLong cy = a.y + vy * t;
			const PF_FpLong ddx = px - cx;
			const PF_FpLong ddy = py - cy;
			const PF_FpLong coverage = CoverageFromDist2(ddx * ddx + ddy * ddy, radius);
			if (coverage > 0.0) {
				BlendPixelFloat(
					&row[x],
					coverage * opacity,
					r0 + dr * t,
					g0 + dg * t,
					b0 + db * t,
					blendMode);
			}
		}
	}
}

static void RasterizeSegmentVariableFloat(
	PF_EffectWorld *world,
	const LineAnimRenderInfo &info,
	const Vec2 &a,
	const Vec2 &b,
	PF_FpLong z0,
	PF_FpLong z1,
	PF_FpLong width0,
	PF_FpLong width1,
	PF_FpLong opacity,
	PF_FpLong r0,
	PF_FpLong g0,
	PF_FpLong b0,
	PF_FpLong r1,
	PF_FpLong g1,
	PF_FpLong b1,
	A_long blendMode)
{
	const PF_FpLong vx = b.x - a.x;
	const PF_FpLong vy = b.y - a.y;
	const PF_FpLong len2 = vx * vx + vy * vy;
	const PF_FpLong maxWidth = std::max(width0, width1);
	if (len2 <= 1.0e-8 || maxWidth <= 0.0 || opacity <= 0.0) {
		return;
	}

	const PF_FpLong radius = maxWidth * 0.5;
	if (SegmentBoundsOutside(a, b, radius, world)) {
		return;
	}
	const A_long minX = ClampL(static_cast<A_long>(std::floor(std::min(a.x, b.x) - radius - 1.0)), 0, world->width - 1);
	const A_long maxX = ClampL(static_cast<A_long>(std::ceil(std::max(a.x, b.x) + radius + 1.0)), 0, world->width - 1);
	const A_long minY = ClampL(static_cast<A_long>(std::floor(std::min(a.y, b.y) - radius - 1.0)), 0, world->height - 1);
	const A_long maxY = ClampL(static_cast<A_long>(std::ceil(std::max(a.y, b.y) + radius + 1.0)), 0, world->height - 1);
	const A_long widthTrue = world->rowbytes / sizeof(PF_PixelFloat);
	const PF_FpLong invLen2 = 1.0 / len2;
	const PF_FpLong dWidth = width1 - width0;
	const PF_FpLong dz = z1 - z0;
	const PF_FpLong dr = r1 - r0;
	const PF_FpLong dg = g1 - g0;
	const PF_FpLong db = b1 - b0;

	const PF_FpLong len     = std::sqrt(len2);
	const PF_FpLong invLen  = 1.0 / len;
	// Tangent unit vector
	const PF_FpLong tx = vx * invLen;
	const PF_FpLong ty = vy * invLen;

	// Grunge parameters
	const bool useGrunge = (info.grunge_amount > 1.0e-4);
	const A_u_long grungeSeed = static_cast<A_u_long>(info.noise_seed) * 0x9e3779b9U + 0x517cc1b7U;
	// Use the world-space position of segment start as a consistent seed offset
	const PF_FpLong grungeSeedOff = a.x * 17.3 + a.y * 13.7;
	const A_u_long gSeed = grungeSeed + static_cast<A_u_long>(static_cast<A_long>(grungeSeedOff * 100.0));

	for (A_long y = minY; y <= maxY; ++y) {
		A_long rowMinX = minX;
		A_long rowMaxX = maxX;
		if (!SegmentScanlineXRange(a, len, tx, ty, radius, minX, maxX, y, rowMinX, rowMaxX)) {
			continue;
		}
		PF_PixelFloat *row = reinterpret_cast<PF_PixelFloat *>(world->data) + y * widthTrue;
		const PF_FpLong py = static_cast<PF_FpLong>(y) + 0.5;
		for (A_long x = rowMinX; x <= rowMaxX; ++x) {
			const PF_FpLong px = static_cast<PF_FpLong>(x) + 0.5;
			const PF_FpLong t = ClampF(((px - a.x) * vx + (py - a.y) * vy) * invLen2, 0.0, 1.0);
			const PF_FpLong cx = a.x + vx * t;
			const PF_FpLong cy = a.y + vy * t;
			const PF_FpLong localRadius = (width0 + dWidth * t) * 0.5;
			const PF_FpLong ddx = px - cx;
			const PF_FpLong ddy = py - cy;
			const PF_FpLong coverage = CoverageFromDist2(ddx * ddx + ddy * ddy, localRadius);
			if (coverage > 0.0) {
				PF_FpLong finalCov = coverage;
				if (useGrunge) {
					// along: signed distance along the tangent from segment start
					const PF_FpLong along = (px - a.x) * tx + (py - a.y) * ty;
					// cross: signed perpendicular distance from the center line
					const PF_FpLong cross = (px - a.x) * (-ty) + (py - a.y) * tx;
					const PF_FpLong gc = GrungeCoverage(gSeed, along, cross, localRadius,
						info.grunge_amount, info.grunge_roughness,
						info.grunge_frequency, info.grunge_jitter);
					finalCov *= gc;
				}
				if (finalCov > 0.0) {
					BlendPixelFloat(&row[x], finalCov * opacity, r0 + dr * t, g0 + dg * t, b0 + db * t, blendMode);
				}
			}
		}
	}
}

static void RasterizeCircleFloat(
	PF_EffectWorld *world,
	const LineAnimRenderInfo &info,
	const Vec2 &center,
	PF_FpLong viewZ,
	PF_FpLong width,
	PF_FpLong opacity,
	PF_FpLong r,
	PF_FpLong g,
	PF_FpLong b,
	A_long blendMode)
{
	if (width <= 0.0 || opacity <= 0.0) {
		return;
	}
	const PF_FpLong radius = width * 0.5;
	if (CircleBoundsOutside(center, radius, world)) {
		return;
	}
	const A_long minX = ClampL(static_cast<A_long>(std::floor(center.x - radius - 1.0)), 0, world->width - 1);
	const A_long maxX = ClampL(static_cast<A_long>(std::ceil(center.x + radius + 1.0)), 0, world->width - 1);
	const A_long minY = ClampL(static_cast<A_long>(std::floor(center.y - radius - 1.0)), 0, world->height - 1);
	const A_long maxY = ClampL(static_cast<A_long>(std::ceil(center.y + radius + 1.0)), 0, world->height - 1);
	const A_long widthTrue = world->rowbytes / sizeof(PF_PixelFloat);
	for (A_long y = minY; y <= maxY; ++y) {
		PF_PixelFloat *row = reinterpret_cast<PF_PixelFloat *>(world->data) + y * widthTrue;
		const PF_FpLong dy = static_cast<PF_FpLong>(y) + 0.5 - center.y;
		for (A_long x = minX; x <= maxX; ++x) {
			const PF_FpLong dx = static_cast<PF_FpLong>(x) + 0.5 - center.x;
			const PF_FpLong coverage = CoverageFromDist2(dx * dx + dy * dy, radius);
			if (coverage > 0.0) {
				BlendPixelFloat(&row[x], coverage * opacity, r, g, b, blendMode);
			}
		}
	}
}

static void RasterizeTapeSegmentVariableFloat(
	PF_EffectWorld *world,
	const LineAnimRenderInfo &info,
	const Vec2 &a,
	const Vec2 &b,
	PF_FpLong z0,
	PF_FpLong z1,
	PF_FpLong width0,
	PF_FpLong width1,
	PF_FpLong opacity,
	PF_FpLong r0,
	PF_FpLong g0,
	PF_FpLong b0,
	PF_FpLong r1,
	PF_FpLong g1,
	PF_FpLong b1,
	A_long blendMode)
{
	const PF_FpLong vx = b.x - a.x;
	const PF_FpLong vy = b.y - a.y;
	const PF_FpLong len2 = vx * vx + vy * vy;
	const PF_FpLong len = std::sqrt(len2);
	const PF_FpLong maxWidth = std::max(width0, width1);
	if (len <= 1.0e-4 || maxWidth <= 0.0 || opacity <= 0.0) {
		return;
	}

	const PF_FpLong radius = maxWidth * 0.5;
	if (SegmentBoundsOutside(a, b, radius, world)) {
		return;
	}
	const A_long minX = ClampL(static_cast<A_long>(std::floor(std::min(a.x, b.x) - radius - 1.0)), 0, world->width - 1);
	const A_long maxX = ClampL(static_cast<A_long>(std::ceil(std::max(a.x, b.x) + radius + 1.0)), 0, world->width - 1);
	const A_long minY = ClampL(static_cast<A_long>(std::floor(std::min(a.y, b.y) - radius - 1.0)), 0, world->height - 1);
	const A_long maxY = ClampL(static_cast<A_long>(std::ceil(std::max(a.y, b.y) + radius + 1.0)), 0, world->height - 1);
	const A_long widthTrue = world->rowbytes / sizeof(PF_PixelFloat);
	const PF_FpLong tx = vx / len;
	const PF_FpLong ty = vy / len;
	const PF_FpLong invLen = 1.0 / len;
	const PF_FpLong dWidth = width1 - width0;
	const PF_FpLong dz = z1 - z0;
	const PF_FpLong dr = r1 - r0;
	const PF_FpLong dg = g1 - g0;
	const PF_FpLong db = b1 - b0;

	for (A_long y = minY; y <= maxY; ++y) {
		A_long rowMinX = minX;
		A_long rowMaxX = maxX;
		if (!SegmentScanlineXRange(a, len, tx, ty, radius, minX, maxX, y, rowMinX, rowMaxX)) {
			continue;
		}
		PF_PixelFloat *row = reinterpret_cast<PF_PixelFloat *>(world->data) + y * widthTrue;
		const PF_FpLong dyBase = static_cast<PF_FpLong>(y) + 0.5 - a.y;
		for (A_long x = rowMinX; x <= rowMaxX; ++x) {
			const PF_FpLong dx = static_cast<PF_FpLong>(x) + 0.5 - a.x;
			const PF_FpLong dy = dyBase;
			const PF_FpLong along = dx * tx + dy * ty;
			const PF_FpLong t = ClampF(along * invLen, 0.0, 1.0);
			const PF_FpLong cross = std::fabs(dx * (-ty) + dy * tx);
			const PF_FpLong localRadius = (width0 + dWidth * t) * 0.5;
			const PF_FpLong sideCoverage = localRadius + 0.5 - cross;
			const PF_FpLong startCoverage = along + radius + 0.5;
			const PF_FpLong endCoverage = len - along + radius + 0.5;
			const PF_FpLong coverage = ClampF(std::min(sideCoverage, std::min(startCoverage, endCoverage)), 0.0, 1.0);
			if (coverage > 0.0) {
				BlendPixelFloat(&row[x], coverage * opacity, r0 + dr * t, g0 + dg * t, b0 + db * t, blendMode);
			}
		}
	}
}

static Vec3 OffsetPathPoint3D(
	const PathPoint &p,
	PF_FpLong normalOffset,
	PF_FpLong xOffset,
	PF_FpLong yOffset,
	PF_FpLong zOffset,
	const LineAnimRenderInfo &info)
{
	const PF_FpLong rampedOffset = normalOffset * BendRampAt(p.u, info);
	Vec2 tangent = { p.normal.y, -p.normal.x };
	Normalize(tangent);

	Vec2 lineXY = {
		p.normal.x * rampedOffset + xOffset,
		p.normal.y * rampedOffset + yOffset
	};

	if (UseSphere3D(info)) {
		const PF_FpLong localCenterX = info.twist_axis_x - info.comp_center_x - info.position_x;
		const PF_FpLong localCenterY = info.twist_axis_y - info.comp_center_y - info.position_y;
		const PF_FpLong localX = p.pos.x + lineXY.x;
		const PF_FpLong localY = p.pos.y + lineXY.y;
		const PF_FpLong radius = std::max<PF_FpLong>(1.0, info.twist_radius);
		const PF_FpLong zSpan = std::max<PF_FpLong>(1.0, info.sphere_z_span);
		const PF_FpLong latitude = ClampF(zOffset / zSpan, -1.0, 1.0) * 1.5707963267948966;
		const PF_FpLong xyScale = std::cos(latitude);
		const PF_FpLong sphereZ = std::sin(latitude) * radius;

		return {
			info.comp_center_x + info.position_x + localCenterX + (localX - localCenterX) * xyScale,
			info.comp_center_y + info.position_y + localCenterY + (localY - localCenterY) * xyScale,
			info.position_z + sphereZ
		};
	}

	PF_FpLong z = info.enable_3d ? zOffset : 0.0;

	if (info.enable_3d && std::fabs(info.line_z_spacing) > 1.0e-8) {
		const PF_FpLong pathAngle = p.turn_angle + info.phase;
		z += std::sin(pathAngle) * std::fabs(info.line_z_spacing);
	}

	if (std::fabs(info.twist) > 1.0e-8) {
		const PF_FpLong u = ClampF(p.u, 0.0, 1.0);
		const PF_FpLong envelope = info.twist_loop_ramp ? LoopTwistEnvelope(u) : u;
		const PF_FpLong angle = info.twist * envelope + info.phase;
		const PF_FpLong c = std::cos(angle);
		const PF_FpLong s = std::sin(angle);
		const PF_FpLong nComp = Dot(lineXY, p.normal);
		const PF_FpLong tComp = Dot(lineXY, tangent);
		const PF_FpLong rotatedN = nComp * c - z * s;
		z = nComp * s + z * c;
		lineXY = {
			p.normal.x * rotatedN + tangent.x * tComp,
			p.normal.y * rotatedN + tangent.y * tComp
		};
	}

	if (info.enable_3d) {
		return {
			info.comp_center_x + info.position_x + p.pos.x + lineXY.x,
			info.comp_center_y + info.position_y + p.pos.y + lineXY.y,
			info.position_z + z
		};
	}

	return { p.pos.x + lineXY.x, p.pos.y + lineXY.y, z };
}

static bool ProjectPoint3D(const Vec3 &pt, const LineAnimRenderInfo &info, ProjectedPoint &out)
{
	if (!info.enable_3d && std::fabs(pt.z) <= 1.0e-8) {
		out = { { pt.x, pt.y }, pt.z };
		return true;
	}

	if (info.enable_3d && info.camera_active) {
		const Vec3 view = TransformPoint(info.camera_matrix, pt);
		if (info.camera_dist > 1.0 && info.camera_dist + view.z <= 1.0) {
			return false;
		}
		const PF_FpLong perspective = (info.camera_dist > 1.0) ? SafePerspective(info.camera_dist, view.z) : 1.0;
		const PF_FpLong cx = (info.camera_image_width > 0) ? static_cast<PF_FpLong>(info.camera_image_width) * 0.5 : info.twist_axis_x;
		const PF_FpLong cy = (info.camera_image_height > 0) ? static_cast<PF_FpLong>(info.camera_image_height) * 0.5 : info.twist_axis_y;
		out = {
			{
				cx + (view.x - cx) * perspective,
				cy + (view.y - cy) * perspective
			},
			view.z
		};
		return std::isfinite(out.screen.x) && std::isfinite(out.screen.y) && std::isfinite(out.view_z);
	}

	if (std::fabs(pt.z) > 1.0e-8) {
		const PF_FpLong virtualCameraDist = std::max<PF_FpLong>(1200.0, info.twist_radius * 5.0);
		if (virtualCameraDist + pt.z <= 1.0) {
			return false;
		}
		const PF_FpLong perspective = SafePerspective(virtualCameraDist, pt.z);
		out = {
			{
				info.twist_axis_x + (pt.x - info.twist_axis_x) * perspective,
				info.twist_axis_y + (pt.y - info.twist_axis_y) * perspective
			},
			pt.z
		};
		return std::isfinite(out.screen.x) && std::isfinite(out.screen.y) && std::isfinite(out.view_z);
	}

	out = { { pt.x, pt.y }, pt.z };
	return true;
}

static bool ProjectPoint3D(const Vec3 &pt, const LineAnimRenderInfo &info, Vec2 &out)
{
	ProjectedPoint projected;
	if (!ProjectPoint3D(pt, info, projected)) {
		return false;
	}
	out = projected.screen;
	return true;
}

static bool OffsetPathPoint(
	const PathPoint &p,
	PF_FpLong normalOffset,
	PF_FpLong xOffset,
	PF_FpLong yOffset,
	PF_FpLong zOffset,
	const LineAnimRenderInfo &info,
	Vec2 &out)
{
	return ProjectPoint3D(OffsetPathPoint3D(p, normalOffset, xOffset, yOffset, zOffset, info), info, out);
}

static bool OffsetPathPointProjected(
	const PathPoint &p,
	PF_FpLong normalOffset,
	PF_FpLong xOffset,
	PF_FpLong yOffset,
	PF_FpLong zOffset,
	const LineAnimRenderInfo &info,
	ProjectedPoint &out)
{
	return ProjectPoint3D(OffsetPathPoint3D(p, normalOffset, xOffset, yOffset, zOffset, info), info, out);
}

static PathPoint InterpPoint(const PathPoint &a, const PathPoint &b, PF_FpLong t)
{
	PathPoint p;
	p.pos = { Lerp(a.pos.x, b.pos.x, t), Lerp(a.pos.y, b.pos.y, t) };
	p.normal = { Lerp(a.normal.x, b.normal.x, t), Lerp(a.normal.y, b.normal.y, t) };
	Normalize(p.normal);
	p.dist = Lerp(a.dist, b.dist, t);
	p.u = Lerp(a.u, b.u, t);
	p.turn_angle = Lerp(a.turn_angle, b.turn_angle, t);
	return p;
}

static void MakeClosedSeamContinuous(PathPolyline &polyline, bool force)
{
	if (polyline.points.size() < 2) {
		return;
	}

	PathPoint &first = polyline.points.front();
	PathPoint &last = polyline.points.back();
	const PF_FpLong dx = first.pos.x - last.pos.x;
	const PF_FpLong dy = first.pos.y - last.pos.y;
	if (!force && std::sqrt(dx * dx + dy * dy) > 1.0) {
		return;
	}

	Vec2 normal = { first.normal.x + last.normal.x, first.normal.y + last.normal.y };
	Normalize(normal);
	const Vec2 pos = { (first.pos.x + last.pos.x) * 0.5, (first.pos.y + last.pos.y) * 0.5 };
	first.pos = pos;
	last.pos = pos;
	first.normal = normal;
	last.normal = normal;
}

static void ApplyPathTurnAngles(PathPolyline &polyline, const LineAnimRenderInfo &info)
{
	if (polyline.points.empty()) {
		return;
	}

	PF_FpLong prevRaw = 0.0;
	PF_FpLong unwrapped = 0.0;
	bool havePrev = false;
	for (PathPoint &p : polyline.points) {
		const PF_FpLong dx = p.pos.x - info.twist_axis_x;
		const PF_FpLong dy = p.pos.y - info.twist_axis_y;
		const PF_FpLong radius2 = dx * dx + dy * dy;
		PF_FpLong raw = havePrev ? prevRaw : p.u * 6.283185307179586;
		if (radius2 > 1.0e-6) {
			raw = std::atan2(dy, dx);
		}

		if (!havePrev) {
			unwrapped = raw;
			havePrev = true;
		} else {
			PF_FpLong delta = raw - prevRaw;
			while (delta > 3.141592653589793) {
				delta -= 6.283185307179586;
			}
			while (delta < -3.141592653589793) {
				delta += 6.283185307179586;
			}
			unwrapped += delta;
		}

		p.turn_angle = unwrapped;
		prevRaw = raw;
	}
}

static void ApplyBendSmoothing(PathPolyline &polyline, const LineAnimRenderInfo &info, PF_FpLong signedOffset)
{
	const size_t count = polyline.points.size();
	if (count < 3 || info.bend_smooth <= 0.0 || info.line_count >= 150) {
		return;
	}

	const std::vector<PathPoint> original = polyline.points;
	const PF_FpLong radius = std::max<PF_FpLong>(1.0, info.bend_smooth);
	bool monotonicDist = true;
	for (size_t i = 1; i < count; ++i) {
		if (original[i].dist + 1.0e-6 < original[i - 1].dist) {
			monotonicDist = false;
			break;
		}
	}

	size_t windowStart = 0;
	for (size_t i = 0; i < count; ++i) {
		Vec2 normal = { 0.0, 0.0 };
		PF_FpLong totalWeight = 0.0;
		const PF_FpLong centerDist = original[i].dist;
		size_t begin = 0;
		size_t end = count;

		if (monotonicDist) {
			while (windowStart < count && original[windowStart].dist < centerDist - radius) {
				++windowStart;
			}
			begin = windowStart;
			end = begin;
			while (end < count && original[end].dist <= centerDist + radius) {
				++end;
			}
		}

		for (size_t j = begin; j < end; ++j) {
			const PF_FpLong d = std::fabs(original[j].dist - centerDist);
			if (d > radius) {
				continue;
			}
			const PF_FpLong w = 1.0 - (d / radius);
			const PF_FpLong sign = (Dot(original[i].normal, original[j].normal) < 0.0) ? -1.0 : 1.0;
			normal.x += original[j].normal.x * sign * w;
			normal.y += original[j].normal.y * sign * w;
			totalWeight += w;
		}

		if (totalWeight > 1.0e-6) {
			normal.x /= totalWeight;
			normal.y /= totalWeight;
			Normalize(normal);
			polyline.points[i].normal = normal;
		}
	}

	if (info.bend_overshoot <= 0.0 || std::fabs(signedOffset) <= 1.0e-6) {
		return;
	}

	const PF_FpLong globalPushLimit = std::min<PF_FpLong>(
		radius * 0.35 * info.bend_overshoot,
		std::max<PF_FpLong>(info.width, std::fabs(info.spacing) * 0.8 + info.width));
	for (size_t i = 1; i + 1 < count; ++i) {
		Vec2 in = {
			original[i].pos.x - original[i - 1].pos.x,
			original[i].pos.y - original[i - 1].pos.y
		};
		Vec2 out = {
			original[i + 1].pos.x - original[i].pos.x,
			original[i + 1].pos.y - original[i].pos.y
		};
		const PF_FpLong inLen = std::sqrt(in.x * in.x + in.y * in.y);
		const PF_FpLong outLen = std::sqrt(out.x * out.x + out.y * out.y);
		const PF_FpLong localLenLimit = std::max<PF_FpLong>(0.0, std::min(inLen, outLen) * 0.28);
		Normalize(in);
		Normalize(out);

		const PF_FpLong dot = ClampF(Dot(in, out), -1.0, 1.0);
		const PF_FpLong angle = std::acos(dot);
		if (angle < 0.18) {
			continue;
		}

		Vec2 n0 = { -in.y, in.x };
		Vec2 n1 = { -out.y, out.x };
		Vec2 miter = { n0.x + n1.x, n0.y + n1.y };
		Normalize(miter);
		if (std::fabs(Dot(miter, original[i].normal)) < 0.15) {
			continue;
		}

		const PF_FpLong cosHalf = std::max<PF_FpLong>(0.18, std::fabs(Dot(miter, n1)));
		PF_FpLong push = signedOffset * ((1.0 / cosHalf) - 1.0) * info.bend_overshoot;
		const PF_FpLong maxPush = std::min(globalPushLimit, localLenLimit);
		push = ClampF(push, -maxPush, maxPush);

		const PF_FpLong turnWeight = ClampF((angle - 0.18) / 1.8, 0.0, 1.0);
		polyline.points[i].pos.x += miter.x * push * turnWeight;
		polyline.points[i].pos.y += miter.y * push * turnWeight;
	}
}

static DashPattern MakeDashPattern(const LineAnimRenderInfo &info, A_long lineIndex, bool closed, PF_FpLong totalLength)
{
	DashPattern pattern = { info.dash_length, info.dash_gap, info.dash_offset };
	const PF_FpLong period = pattern.length + std::max<PF_FpLong>(0.0, pattern.gap);
	if (pattern.length <= 0.0 || period <= 1.0e-6) {
		return pattern;
	}

	if (closed && totalLength > 1.0e-6) {
		const A_long repeatCount = std::max<A_long>(1, static_cast<A_long>(std::floor(totalLength / period + 0.5)));
		const PF_FpLong fittedPeriod = totalLength / static_cast<PF_FpLong>(repeatCount);
		const PF_FpLong scale = fittedPeriod / period;
		pattern.length *= scale;
		pattern.gap *= scale;
		pattern.offset += pattern.length * 0.5;
	}

	const PF_FpLong adjustedPeriod = pattern.length + std::max<PF_FpLong>(0.0, pattern.gap);
	pattern.offset += static_cast<PF_FpLong>(lineIndex) * info.dash_line_phase * adjustedPeriod;
	return pattern;
}

static bool DashVisible(PF_FpLong distance, const DashPattern &pattern)
{
	if (pattern.length <= 0.0) {
		return true;
	}
	const PF_FpLong period = pattern.length + std::max<PF_FpLong>(0.0, pattern.gap);
	if (period <= 1.0e-6) {
		return true;
	}
	PF_FpLong m = std::fmod(distance + pattern.offset, period);
	if (m < 0.0) {
		m += period;
	}
	return m < pattern.length;
}

static PF_FpLong WaveOffsetAt(
	const PathPoint &p,
	bool closed,
	PF_FpLong totalLength,
	PF_FpLong waveAmp,
	PF_FpLong waveFreq,
	PF_FpLong wavePhase,
	const LineAnimRenderInfo &info)
{
	if (std::fabs(waveAmp) <= 1.0e-8) {
		return 0.0;
	}

	if (closed && totalLength > 1.0e-6) {
		const PF_FpLong nominalCycles = totalLength / std::max<PF_FpLong>(1.0, info.noise_scale);
		const A_long cycles = std::max<A_long>(1, static_cast<A_long>(std::floor(nominalCycles + 0.5)));
		return std::sin(ClampF(p.u, 0.0, 1.0) * 6.283185307179586 * static_cast<PF_FpLong>(cycles) + wavePhase) * waveAmp;
	}

	return std::sin(p.dist * waveFreq + wavePhase) * waveAmp;
}

static PF_FpLong AudioOffsetAt(const PathPoint &p, const LineAnimRenderInfo &info, A_long lineIndex)
{
	if (info.audio_amount <= 1.0e-6 || info.audio_sample_count < 2) {
		return 0.0;
	}

	const PF_FpLong sampleU = Wrap01(ClampF(p.u, 0.0, 1.0) * info.audio_detail +
		static_cast<PF_FpLong>(lineIndex) * info.audio_line_phase);
	const PF_FpLong pos = sampleU * static_cast<PF_FpLong>(info.audio_sample_count - 1);
	const A_long i0 = ClampL(static_cast<A_long>(std::floor(pos)), 0, info.audio_sample_count - 1);
	const A_long i1 = ClampL(i0 + 1, 0, info.audio_sample_count - 1);
	const PF_FpLong t = ClampF(pos - static_cast<PF_FpLong>(i0), 0.0, 1.0);
	return Lerp(info.audio_wave[i0], info.audio_wave[i1], t) * info.audio_amount;
}

static PF_FpLong AudioStrengthAt(const PathPoint &p, const LineAnimRenderInfo &info)
{
	if (info.audio_color_amount <= 1.0e-6 || info.audio_sample_count < 2) {
		return 0.0;
	}

	const PF_FpLong sampleU = Wrap01(ClampF(p.u, 0.0, 1.0) * info.audio_detail);
	const PF_FpLong pos = sampleU * static_cast<PF_FpLong>(info.audio_sample_count - 1);
	const A_long i0 = ClampL(static_cast<A_long>(std::floor(pos)), 0, info.audio_sample_count - 1);
	const A_long i1 = ClampL(i0 + 1, 0, info.audio_sample_count - 1);
	const PF_FpLong t = ClampF(pos - static_cast<PF_FpLong>(i0), 0.0, 1.0);
	return ClampF(std::fabs(Lerp(info.audio_wave[i0], info.audio_wave[i1], t)), 0.0, 1.0);
}

static PF_FpLong TaperAt(PF_FpLong u, const LineAnimRenderInfo &info)
{
	u = ClampF(u, 0.0, 1.0);
	PF_FpLong factor = 1.0;
	if (info.taper_start > 1.0e-6) {
		factor = std::min(factor, ClampF(u / info.taper_start, 0.0, 1.0));
	}
	if (info.taper_end > 1.0e-6) {
		factor = std::min(factor, ClampF((1.0 - u) / info.taper_end, 0.0, 1.0));
	}
	return Lerp(info.taper_min_width, 1.0, factor);
}

static PF_FpLong OffsetTaperU(PF_FpLong distance, PF_FpLong totalLength, const LineAnimRenderInfo &info)
{
	if (totalLength <= 1.0e-6) {
		return 0.0;
	}
	return ClampF((distance + info.taper_offset) / totalLength, 0.0, 1.0);
}

static PF_FpLong CubicBezier1D(PF_FpLong p0, PF_FpLong p1, PF_FpLong p2, PF_FpLong p3, PF_FpLong t)
{
	const PF_FpLong it = 1.0 - t;
	return it * it * it * p0 + 3.0 * it * it * t * p1 + 3.0 * it * t * t * p2 + t * t * t * p3;
}

static PF_FpLong BendRampBezierRawAt(PF_FpLong u, const LineAnimRenderInfo &info)
{
	const PF_FpLong x = ClampF(u, 0.0, 1.0) * 4.0;
	const A_long i = ClampL(static_cast<A_long>(std::floor(x)), 0, 3);
	const PF_FpLong t = ClampF(x - static_cast<PF_FpLong>(i), 0.0, 1.0);
	return ClampF(CubicBezier1D(info.bend_ramp[i], info.bend_handle_out[i], info.bend_handle_in[i], info.bend_ramp[i + 1], t), 0.0, 1.0);
}

static PF_FpLong BendRampAt(PF_FpLong u, const LineAnimRenderInfo &info)
{
	if (std::fabs(info.bend_ramp_amount) <= 1.0e-6) {
		return 1.0;
	}

	const PF_FpLong v = BendRampBezierRawAt(u, info);
	return std::max<PF_FpLong>(0.02, 1.0 + (v - 0.5) * 2.0 * info.bend_ramp_amount);
}

static PF_FpLong TapeWidthScaleAt(PF_FpLong u, PF_FpLong normalOffset, const LineAnimRenderInfo &info)
{
	if (!info.enable_3d || info.tape_flatness <= 1.0e-6) {
		return 1.0;
	}

	const PF_FpLong radius = std::max<PF_FpLong>(1.0, info.twist_radius);
	const PF_FpLong crossLinePhase = (normalOffset / radius) * 1.5707963267948966;
	const PF_FpLong angle = info.tape_twist * ClampF(u, 0.0, 1.0) + crossLinePhase;
	const PF_FpLong edgeOn = std::fabs(std::cos(angle));
	const PF_FpLong tapeScale = Lerp(0.08, 1.0, edgeOn);
	return Lerp(1.0, tapeScale, info.tape_flatness);
}

static PF_FpLong StrokeNoiseWidthScaleAt(const PathPoint &p, const LineAnimRenderInfo &info, A_u_long seed, PF_FpLong time)
{
	const PF_FpLong amount = info.noise_ramp * info.noise_width;
	if (amount <= 1.0e-6) {
		return 1.0;
	}

	const PF_FpLong scale = std::max<PF_FpLong>(1.0, info.noise_scale);
	const PF_FpLong pos = (p.dist / scale) * 6.283185307179586 + time;
	return ClampF(1.0 + SmoothNoise(seed, pos) * 0.85 * amount, 0.02, 12.0);
}

static void DrawSegmentPiece(
	PF_EffectWorld *output,
	PF_PixelFormat format,
	const LineAnimRenderInfo &info,
	const PathPoint &p0,
	const PathPoint &p1,
	PF_FpLong taperU0,
	PF_FpLong taperU1,
	PF_FpLong colorU0,
	PF_FpLong colorU1,
	PF_FpLong normalOffset0,
	PF_FpLong normalOffset1,
	PF_FpLong xOffset0,
	PF_FpLong xOffset1,
	PF_FpLong yOffset0,
	PF_FpLong yOffset1,
	PF_FpLong zOffset0,
	PF_FpLong zOffset1,
	PF_FpLong width,
	PF_FpLong widthScale0,
	PF_FpLong widthScale1,
	PF_FpLong opacity,
	PF_FpLong sr,
	PF_FpLong sg,
	PF_FpLong sb,
	PF_FpLong er,
	PF_FpLong eg,
	PF_FpLong eb,
	bool seamless)
{
	ProjectedPoint p0Projected;
	ProjectedPoint p1Projected;
	if (!OffsetPathPointProjected(p0, normalOffset0, xOffset0, yOffset0, zOffset0, info, p0Projected) ||
		!OffsetPathPointProjected(p1, normalOffset1, xOffset1, yOffset1, zOffset1, info, p1Projected)) {
		return;
	}
	Vec2 a = p0Projected.screen;
	Vec2 b = p1Projected.screen;
	const PF_FpLong w0 = width * widthScale0 * TaperAt(taperU0, info) * TapeWidthScaleAt(p0.u, normalOffset0, info);
	const PF_FpLong w1 = width * widthScale1 * TaperAt(taperU1, info) * TapeWidthScaleAt(p1.u, normalOffset1, info);
	PF_FpLong r0 = Lerp(sr, er, colorU0);
	PF_FpLong g0 = Lerp(sg, eg, colorU0);
	PF_FpLong b0 = Lerp(sb, eb, colorU0);
	PF_FpLong r1 = Lerp(sr, er, colorU1);
	PF_FpLong g1 = Lerp(sg, eg, colorU1);
	PF_FpLong b1 = Lerp(sb, eb, colorU1);
	if (info.audio_color_amount > 1.0e-6) {
		const PF_FpLong ar = static_cast<PF_FpLong>(info.audio_color.red) / static_cast<PF_FpLong>(PF_MAX_CHAN8);
		const PF_FpLong ag = static_cast<PF_FpLong>(info.audio_color.green) / static_cast<PF_FpLong>(PF_MAX_CHAN8);
		const PF_FpLong ab = static_cast<PF_FpLong>(info.audio_color.blue) / static_cast<PF_FpLong>(PF_MAX_CHAN8);
		const PF_FpLong s0 = AudioStrengthAt(p0, info) * info.audio_color_amount;
		const PF_FpLong s1 = AudioStrengthAt(p1, info) * info.audio_color_amount;
		r0 = Lerp(r0, ar, s0);
		g0 = Lerp(g0, ag, s0);
		b0 = Lerp(b0, ab, s0);
		r1 = Lerp(r1, ar, s1);
		g1 = Lerp(g1, ag, s1);
		b1 = Lerp(b1, ab, s1);
	}
	if (seamless) {
		const PF_FpLong dx = b.x - a.x;
		const PF_FpLong dy = b.y - a.y;
		const PF_FpLong len = std::sqrt(dx * dx + dy * dy);
		if (len > 1.0e-4) {
			const PF_FpLong overlap = std::min<PF_FpLong>(std::max(w0, w1) * 0.35, len * 0.35);
			const PF_FpLong ux = dx / len;
			const PF_FpLong uy = dy / len;
			a.x -= ux * overlap;
			a.y -= uy * overlap;
			b.x += ux * overlap;
			b.y += uy * overlap;
		}
	}

	if (info.tape_flatness > 1.0e-6) {
		if (format == PF_PixelFormat_ARGB128) {
			RasterizeTapeSegmentVariableFloat(output, info, a, b, p0Projected.view_z, p1Projected.view_z, w0, w1, opacity, r0, g0, b0, r1, g1, b1, info.blend_mode);
		} else if (format == PF_PixelFormat_ARGB64) {
			RasterizeTapeSegmentVariableT<PF_Pixel16, A_u_short, PF_MAX_CHAN16>(output, info, a, b, p0Projected.view_z, p1Projected.view_z, w0, w1, opacity, r0, g0, b0, r1, g1, b1, info.blend_mode);
		} else {
			RasterizeTapeSegmentVariableT<PF_Pixel8, A_u_char, PF_MAX_CHAN8>(output, info, a, b, p0Projected.view_z, p1Projected.view_z, w0, w1, opacity, r0, g0, b0, r1, g1, b1, info.blend_mode);
		}
	} else if (format == PF_PixelFormat_ARGB128) {
		RasterizeSegmentVariableFloat(output, info, a, b, p0Projected.view_z, p1Projected.view_z, w0, w1, opacity, r0, g0, b0, r1, g1, b1, info.blend_mode);
	} else if (format == PF_PixelFormat_ARGB64) {
		RasterizeSegmentVariableT<PF_Pixel16, A_u_short, PF_MAX_CHAN16>(output, info, a, b, p0Projected.view_z, p1Projected.view_z, w0, w1, opacity, r0, g0, b0, r1, g1, b1, info.blend_mode);
	} else {
		RasterizeSegmentVariableT<PF_Pixel8, A_u_char, PF_MAX_CHAN8>(output, info, a, b, p0Projected.view_z, p1Projected.view_z, w0, w1, opacity, r0, g0, b0, r1, g1, b1, info.blend_mode);
	}
}

static void DrawJointPiece(
	PF_EffectWorld *output,
	PF_PixelFormat format,
	const LineAnimRenderInfo &info,
	const PathPoint &p,
	PF_FpLong normalOffset,
	PF_FpLong xOffset,
	PF_FpLong yOffset,
	PF_FpLong zOffset,
	PF_FpLong width,
	PF_FpLong opacity,
	PF_FpLong sr,
	PF_FpLong sg,
	PF_FpLong sb,
	PF_FpLong er,
	PF_FpLong eg,
	PF_FpLong eb)
{
	ProjectedPoint projected;
	if (!OffsetPathPointProjected(p, normalOffset, xOffset, yOffset, zOffset, info, projected)) {
		return;
	}
	const Vec2 center = projected.screen;
	const PF_FpLong w = width * TaperAt(p.u, info) * TapeWidthScaleAt(p.u, normalOffset, info);
	const PF_FpLong r = Lerp(sr, er, p.u);
	const PF_FpLong g = Lerp(sg, eg, p.u);
	const PF_FpLong b = Lerp(sb, eb, p.u);
	if (format == PF_PixelFormat_ARGB128) {
		RasterizeCircleFloat(output, info, center, projected.view_z, w, opacity, r, g, b, info.blend_mode);
	} else if (format == PF_PixelFormat_ARGB64) {
		RasterizeCircleT<PF_Pixel16, A_u_short, PF_MAX_CHAN16>(output, info, center, projected.view_z, w, opacity, r, g, b, info.blend_mode);
	} else {
		RasterizeCircleT<PF_Pixel8, A_u_char, PF_MAX_CHAN8>(output, info, center, projected.view_z, w, opacity, r, g, b, info.blend_mode);
	}
}

static void DrawCurvedSegmentPiece(
	PF_EffectWorld *output,
	PF_PixelFormat format,
	const LineAnimRenderInfo &info,
	const PathPoint &p0,
	const PathPoint &p1,
	PF_FpLong taperU0,
	PF_FpLong taperU1,
	PF_FpLong colorU0,
	PF_FpLong colorU1,
	PF_FpLong normalOffset0,
	PF_FpLong normalOffset1,
	PF_FpLong xOffset0,
	PF_FpLong xOffset1,
	PF_FpLong yOffset0,
	PF_FpLong yOffset1,
	PF_FpLong zOffset0,
	PF_FpLong zOffset1,
	PF_FpLong width,
	A_u_long widthNoiseSeed,
	PF_FpLong widthNoiseTime,
	PF_FpLong opacity,
	PF_FpLong sr,
	PF_FpLong sg,
	PF_FpLong sb,
	PF_FpLong er,
	PF_FpLong eg,
	PF_FpLong eb,
	bool seamless)
{
	const PF_FpLong nDot = ClampF(Dot(p0.normal, p1.normal), -1.0, 1.0);
	const PF_FpLong normalAngle = std::acos(nDot);
	const PF_FpLong offsetScale = std::fabs(normalOffset0) + std::fabs(normalOffset1) + width;
	const PF_FpLong dist = std::fabs(p1.dist - p0.dist);
	const PF_FpLong curveQualityScale = ClampF(64.0 / static_cast<PF_FpLong>(std::max<A_long>(4, info.curve_segments)), 0.25, 4.0);
	const PF_FpLong angleStep = 5.0 * curveQualityScale;
	const PF_FpLong distanceStep = 3.0 * curveQualityScale;
	A_long pieces = 1 + static_cast<A_long>(std::ceil((normalAngle * offsetScale) / angleStep));
	pieces = std::max<A_long>(pieces, static_cast<A_long>(std::ceil(dist / distanceStep)));
	const A_long maxPieces = ClampL(static_cast<A_long>(std::ceil(static_cast<PF_FpLong>(info.curve_segments) * 0.5)), 2, 80);
	pieces = ClampL(pieces, 1, maxPieces);

	PathPoint prev = p0;
	PF_FpLong prevOffset = normalOffset0;
	PF_FpLong prevXOffset = xOffset0;
	PF_FpLong prevYOffset = yOffset0;
	PF_FpLong prevZOffset = zOffset0;
	PF_FpLong prevTaperU = taperU0;
	PF_FpLong prevColorU = colorU0;
	PF_FpLong prevWidthScale = StrokeNoiseWidthScaleAt(prev, info, widthNoiseSeed, widthNoiseTime);
	for (A_long i = 1; i <= pieces; ++i) {
		const PF_FpLong t = static_cast<PF_FpLong>(i) / static_cast<PF_FpLong>(pieces);
		PathPoint cur = InterpPoint(p0, p1, t);
		const PF_FpLong curOffset = Lerp(normalOffset0, normalOffset1, t);
		const PF_FpLong curXOffset = Lerp(xOffset0, xOffset1, t);
		const PF_FpLong curYOffset = Lerp(yOffset0, yOffset1, t);
		const PF_FpLong curZOffset = Lerp(zOffset0, zOffset1, t);
		const PF_FpLong curTaperU = Lerp(taperU0, taperU1, t);
		const PF_FpLong curColorU = Lerp(colorU0, colorU1, t);
		const PF_FpLong curWidthScale = StrokeNoiseWidthScaleAt(cur, info, widthNoiseSeed, widthNoiseTime);
		DrawSegmentPiece(output, format, info, prev, cur, prevTaperU, curTaperU, prevColorU, curColorU, prevOffset, curOffset, prevXOffset, curXOffset, prevYOffset, curYOffset, prevZOffset, curZOffset, width, prevWidthScale, curWidthScale, opacity, sr, sg, sb, er, eg, eb, seamless);
		prev = cur;
		prevOffset = curOffset;
		prevXOffset = curXOffset;
		prevYOffset = curYOffset;
		prevZOffset = curZOffset;
		prevTaperU = curTaperU;
		prevColorU = curColorU;
		prevWidthScale = curWidthScale;
	}
}

static void DrawDashedSegment(
	PF_EffectWorld *output,
	PF_PixelFormat format,
	const LineAnimRenderInfo &info,
	const DashPattern &dash,
	const PathPoint &p0,
	const PathPoint &p1,
	PF_FpLong normalOffset0,
	PF_FpLong normalOffset1,
	PF_FpLong xOffset0,
	PF_FpLong xOffset1,
	PF_FpLong yOffset0,
	PF_FpLong yOffset1,
	PF_FpLong zOffset0,
	PF_FpLong zOffset1,
	PF_FpLong width,
	A_u_long widthNoiseSeed,
	PF_FpLong widthNoiseTime,
	PF_FpLong opacity,
	PF_FpLong sr,
	PF_FpLong sg,
	PF_FpLong sb,
	PF_FpLong er,
	PF_FpLong eg,
	PF_FpLong eb)
{
	if (dash.length <= 0.0) {
		const PF_FpLong dist0 = p0.dist;
		const PF_FpLong dist1 = p1.dist;
		const PF_FpLong segLen = std::fabs(dist1 - dist0);
		const PF_FpLong uSpan = p1.u - p0.u;
		const PF_FpLong fullLength = (std::fabs(uSpan) > 1.0e-6) ? std::fabs(segLen / uSpan) : segLen;
		DrawCurvedSegmentPiece(
			output,
			format,
			info,
			p0,
			p1,
			OffsetTaperU(p0.dist, fullLength, info),
			OffsetTaperU(p1.dist, fullLength, info),
			p0.u,
			p1.u,
			normalOffset0,
			normalOffset1,
			xOffset0,
			xOffset1,
			yOffset0,
			yOffset1,
			zOffset0,
			zOffset1,
			width,
			widthNoiseSeed,
			widthNoiseTime,
			opacity,
			sr,
			sg,
			sb,
			er,
			eg,
			eb,
			true);
		return;
	}

	const PF_FpLong dist0 = p0.dist;
	const PF_FpLong dist1 = p1.dist;
	const PF_FpLong segLen = std::fabs(dist1 - dist0);
	if (segLen <= 1.0e-6) {
		return;
	}

	const PF_FpLong uSpan = p1.u - p0.u;
	const PF_FpLong fullLength = (std::fabs(uSpan) > 1.0e-6) ? std::fabs(segLen / uSpan) : segLen;

	const PF_FpLong period = dash.length + std::max<PF_FpLong>(0.0, dash.gap);
	if (period <= 1.0e-6) {
		DrawCurvedSegmentPiece(
			output,
			format,
			info,
			p0,
			p1,
			OffsetTaperU(p0.dist, fullLength, info),
			OffsetTaperU(p1.dist, fullLength, info),
			p0.u,
			p1.u,
			normalOffset0,
			normalOffset1,
			xOffset0,
			xOffset1,
			yOffset0,
			yOffset1,
			zOffset0,
			zOffset1,
			width,
			widthNoiseSeed,
			widthNoiseTime,
			opacity,
			sr,
			sg,
			sb,
			er,
			eg,
			eb,
			true);
		return;
	}

	PF_FpLong cursor = dist0;
	while (cursor < dist1 - 1.0e-6) {
		PF_FpLong m = std::fmod(cursor + dash.offset, period);
		if (m < 0.0) {
			m += period;
		}
		const bool visible = m < dash.length;
		const PF_FpLong boundary = cursor + (visible ? (dash.length - m) : (period - m));
		const PF_FpLong next = std::min(dist1, std::max(cursor + 0.01, boundary));
		if (visible || DashVisible((cursor + next) * 0.5, dash)) {
			const PF_FpLong t0 = ClampF((cursor - dist0) / segLen, 0.0, 1.0);
			const PF_FpLong t1 = ClampF((next - dist0) / segLen, 0.0, 1.0);
			const PF_FpLong taperU0 = OffsetTaperU(m, dash.length, info);
			const PF_FpLong taperU1 = OffsetTaperU(m + (next - cursor), dash.length, info);
			const PF_FpLong colorU0 = taperU0;
			const PF_FpLong colorU1 = taperU1;
			DrawCurvedSegmentPiece(
				output,
				format,
				info,
				InterpPoint(p0, p1, t0),
				InterpPoint(p0, p1, t1),
				taperU0,
				taperU1,
				colorU0,
				colorU1,
				Lerp(normalOffset0, normalOffset1, t0),
				Lerp(normalOffset0, normalOffset1, t1),
				Lerp(xOffset0, xOffset1, t0),
				Lerp(xOffset0, xOffset1, t1),
				Lerp(yOffset0, yOffset1, t0),
				Lerp(yOffset0, yOffset1, t1),
				Lerp(zOffset0, zOffset1, t0),
				Lerp(zOffset0, zOffset1, t1),
				width,
				widthNoiseSeed,
				widthNoiseTime,
				opacity,
				sr,
				sg,
				sb,
				er,
				eg,
				eb,
				false);
		}
		cursor = next;
	}
}
