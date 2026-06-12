static PF_FpLong MeasurePolylines(const std::vector<PathPolyline> &polylines)
{
	PF_FpLong total = 0.0;
	for (const PathPolyline &polyline : polylines) {
		for (size_t i = 1; i < polyline.points.size(); ++i) {
			const Vec2 &a = polyline.points[i - 1].pos;
			const Vec2 &b = polyline.points[i].pos;
			const PF_FpLong dx = b.x - a.x;
			const PF_FpLong dy = b.y - a.y;
			total += std::sqrt(dx * dx + dy * dy);
		}
	}
	return total;
}

static bool SamplePolylinesAt(const std::vector<PathPolyline> &polylines, PF_FpLong u, PF_FpLong total, PathPoint &out)
{
	const PathPoint *first = nullptr;
	const PathPoint *last = nullptr;
	for (const PathPolyline &polyline : polylines) {
		if (!polyline.points.empty()) {
			if (!first) {
				first = &polyline.points.front();
			}
			last = &polyline.points.back();
		}
	}

	if (total <= 1.0e-6) {
		if (first) {
			out = *first;
			return true;
		}
		return false;
	}

	PF_FpLong target = ClampF(u, 0.0, 1.0) * total;
	for (const PathPolyline &polyline : polylines) {
		for (size_t i = 1; i < polyline.points.size(); ++i) {
			const PathPoint &a = polyline.points[i - 1];
			const PathPoint &b = polyline.points[i];
			const PF_FpLong dx = b.pos.x - a.pos.x;
			const PF_FpLong dy = b.pos.y - a.pos.y;
			const PF_FpLong segLen = std::sqrt(dx * dx + dy * dy);
			if (segLen <= 1.0e-6) {
				continue;
			}
			if (target <= segLen) {
				out = InterpPoint(a, b, ClampF(target / segLen, 0.0, 1.0));
				return true;
			}
			target -= segLen;
		}
	}

	if (last) {
		out = *last;
		return true;
	}
	return false;
}

static bool SamplePolylinesAt(const std::vector<PathPolyline> &polylines, PF_FpLong u, PathPoint &out)
{
	return SamplePolylinesAt(polylines, u, MeasurePolylines(polylines), out);
}

static bool PointOnSegment(const Vec2 &p, const Vec2 &a, const Vec2 &b)
{
	const PF_FpLong vx = b.x - a.x;
	const PF_FpLong vy = b.y - a.y;
	const PF_FpLong wx = p.x - a.x;
	const PF_FpLong wy = p.y - a.y;
	const PF_FpLong cross = vx * wy - vy * wx;
	const PF_FpLong len2 = vx * vx + vy * vy;
	const PF_FpLong eps = std::max<PF_FpLong>(1.0e-7, len2 * 1.0e-10);
	if (cross * cross > eps) {
		return false;
	}
	const PF_FpLong dot = wx * vx + wy * vy;
	return dot >= -1.0e-6 && dot <= len2 + 1.0e-6;
}

static bool PointInsideClosedPolylines(const std::vector<PathPolyline> &polylines, const Vec2 &p)
{
	bool inside = false;
	for (const PathPolyline &polyline : polylines) {
		if (!polyline.closed || polyline.points.size() < 3) {
			continue;
		}
		const size_t count = polyline.points.size();
		for (size_t i = 0, j = count - 1; i < count; j = i++) {
			const Vec2 &a = polyline.points[j].pos;
			const Vec2 &b = polyline.points[i].pos;
			if (PointOnSegment(p, a, b)) {
				return true;
			}
			const bool crossesY = (a.y > p.y) != (b.y > p.y);
			if (crossesY) {
				const PF_FpLong xAtY = a.x + (p.y - a.y) * (b.x - a.x) / std::max<PF_FpLong>(1.0e-12, b.y - a.y);
				if (p.x < xAtY) {
					inside = !inside;
				}
			}
		}
	}
	return inside;
}

static bool SegmentIntersectionT(const Vec2 &a, const Vec2 &b, const Vec2 &c, const Vec2 &d, PF_FpLong &t)
{
	const Vec2 r = { b.x - a.x, b.y - a.y };
	const Vec2 s = { d.x - c.x, d.y - c.y };
	const PF_FpLong denom = Cross(r, s);
	if (std::fabs(denom) <= 1.0e-9) {
		return false;
	}
	const Vec2 ca = { c.x - a.x, c.y - a.y };
	const PF_FpLong u = Cross(ca, r) / denom;
	t = Cross(ca, s) / denom;
	return t > 1.0e-6 && t < 1.0 - 1.0e-6 && u >= -1.0e-6 && u <= 1.0 + 1.0e-6;
}

static void ClipSegmentToClosedPolylines(
	const std::vector<PathPolyline> &boundary,
	const PathPoint &a,
	const PathPoint &b,
	std::vector<std::pair<PathPoint, PathPoint>> &segments)
{
	segments.clear();
	std::vector<PF_FpLong> cuts;
	cuts.reserve(16);
	cuts.push_back(0.0);
	cuts.push_back(1.0);
	bool haveClosedBoundary = false;

	for (const PathPolyline &polyline : boundary) {
		if (!polyline.closed || polyline.points.size() < 3) {
			continue;
		}
		haveClosedBoundary = true;
		const size_t count = polyline.points.size();
		for (size_t i = 0, j = count - 1; i < count; j = i++) {
			PF_FpLong t = 0.0;
			if (SegmentIntersectionT(a.pos, b.pos, polyline.points[j].pos, polyline.points[i].pos, t)) {
				cuts.push_back(t);
			}
		}
	}
	if (!haveClosedBoundary) {
		segments.push_back({ a, b });
		return;
	}

	std::sort(cuts.begin(), cuts.end());
	cuts.erase(std::unique(cuts.begin(), cuts.end(), [](PF_FpLong lhs, PF_FpLong rhs) {
		return std::fabs(lhs - rhs) <= 1.0e-5;
	}), cuts.end());

	for (size_t i = 1; i < cuts.size(); ++i) {
		const PF_FpLong t0 = ClampF(cuts[i - 1], 0.0, 1.0);
		const PF_FpLong t1 = ClampF(cuts[i], 0.0, 1.0);
		if (t1 <= t0 + 1.0e-6) {
			continue;
		}
		const PF_FpLong midT = (t0 + t1) * 0.5;
		const PathPoint mid = InterpPoint(a, b, midT);
		if (PointInsideClosedPolylines(boundary, mid.pos)) {
			segments.push_back({ InterpPoint(a, b, t0), InterpPoint(a, b, t1) });
		}
	}
}

static PathPolyline BuildNoisyWebPolyline(
	const LineAnimRenderInfo &info,
	const PathPoint &start,
	const Vec2 &target,
	A_u_long seedBase,
	PF_FpLong time)
{
	PathPolyline polyline;
	const PF_FpLong dx = target.x - start.pos.x;
	const PF_FpLong dy = target.y - start.pos.y;
	const PF_FpLong length = std::sqrt(dx * dx + dy * dy);
	if (length <= 1.0e-6) {
		return polyline;
	}

	const Vec2 tangent = { dx / length, dy / length };
	const Vec2 normal = { -tangent.y, tangent.x };
	const PF_FpLong noise = info.noise_ramp;
	const PF_FpLong waveAmp = std::max<PF_FpLong>(std::fabs(info.spacing), info.width) * 0.5 * noise * info.noise_wave;
	if (waveAmp <= 1.0e-6) {
		polyline.points.reserve(2);
		PathPoint p0 = start;
		PathPoint p1 = start;
		p0.normal = normal;
		p0.dist = 0.0;
		p0.u = 0.0;
		p1.pos = target;
		p1.normal = normal;
		p1.dist = length;
		p1.u = 1.0;
		polyline.points.push_back(p0);
		polyline.points.push_back(p1);
		return polyline;
	}

	const PF_FpLong waveFreq = 6.283185307179586 / std::max<PF_FpLong>(1.0, info.noise_scale);
	const PF_FpLong wavePhase = Hash01(seedBase + 53) * 6.283185307179586 + time;
	const PF_FpLong wavePhase2 = Hash01(seedBase + 57) * 6.283185307179586 + time * 0.73;
	const PF_FpLong densityScale = std::pow(std::max<PF_FpLong>(1.0, static_cast<PF_FpLong>(info.line_count) / 48.0), 0.65);
	const PF_FpLong sampleStep = ClampF(4.0 * densityScale, 4.0, 24.0);
	const A_long sampleCap = (info.line_count >= 384) ? 64 : ((info.line_count >= 192) ? 96 : 192);
	const A_long samples = ClampL(static_cast<A_long>(std::ceil(length / sampleStep)) + 1, 8, sampleCap);

	polyline.points.reserve(static_cast<size_t>(samples));
	for (A_long i = 0; i < samples; ++i) {
		const PF_FpLong t = static_cast<PF_FpLong>(i) / static_cast<PF_FpLong>(samples - 1);
		const PF_FpLong envelope = std::sin(t * 3.141592653589793);
		const PF_FpLong wave =
			std::sin(length * t * waveFreq + wavePhase) * 0.65 +
			std::sin(length * t * waveFreq * 1.73 + wavePhase2) * 0.25 +
			SmoothNoise(seedBase + 59, time * 0.91 + t * 3.141592653589793) * 0.1;
		const PF_FpLong offset = (i == 0 || i == samples - 1) ? 0.0 : wave * waveAmp * envelope;

		PathPoint p = start;
		p.pos = {
			start.pos.x + dx * t + normal.x * offset,
			start.pos.y + dy * t + normal.y * offset
		};
		p.normal = normal;
		p.dist = length * t;
		p.u = t;
		polyline.points.push_back(p);
	}

	polyline.points.front().pos = start.pos;
	polyline.points.back().pos = target;
	return polyline;
}

static bool WebTrimRange(
	const LineAnimRenderInfo &info,
	A_long lineIndex,
	PF_FpLong time,
	PF_FpLong &start,
	PF_FpLong &end,
	bool lockAnimatedNoise = false)
{
	const bool zeroTrim = std::fabs(info.web_end - info.web_start) < 1.0e-6;
	const bool keepTrimStable = zeroTrim || lockAnimatedNoise;
	const PF_FpLong headRand = keepTrimStable ? 0.0 : HashSigned(static_cast<A_u_long>(info.noise_seed + lineIndex * 47 + 101)) * 0.18 * info.trim_head_random;
	const PF_FpLong tailRand = keepTrimStable ? 0.0 : HashSigned(static_cast<A_u_long>(info.noise_seed + lineIndex * 53 + 103)) * 0.18 * info.trim_tail_random;
	const PF_FpLong trimNoise = info.noise_ramp * info.noise_trim;
	const PF_FpLong noiseStart = keepTrimStable ? 0.0 : SmoothNoise(static_cast<A_u_long>(info.noise_seed + lineIndex * 41 + 107), time * 0.61) * 0.025 * trimNoise;
	const PF_FpLong noiseEnd = keepTrimStable ? 0.0 : SmoothNoise(static_cast<A_u_long>(info.noise_seed + lineIndex * 43 + 109), time * 0.67) * 0.025 * trimNoise;

	start = info.web_start + headRand + noiseStart + info.web_offset;
	end = info.web_end + tailRand + noiseEnd + info.web_offset;

	if (info.web_direction == 2) {
		const PF_FpLong reversedStart = 1.0 - end;
		const PF_FpLong reversedEnd = 1.0 - start;
		start = reversedStart;
		end = reversedEnd;
	}

	start = ClampF(start, 0.0, 1.0);
	end = ClampF(end, 0.0, 1.0);
	if (end <= start) {
		return false;
	}
	return true;
}

static bool TrimmedPolylineIsLineIndependent(const LineAnimRenderInfo &info, bool openPath)
{
	const bool zeroTrim = std::fabs(info.end - info.start) < 1.0e-6;
	const bool fullClosedTrim = !openPath && info.start <= 1.0e-6 && info.end >= 1.0 - 1.0e-6;
	const bool noLineRandomTrim =
		info.trim_head_random <= 1.0e-6 &&
		info.trim_tail_random <= 1.0e-6 &&
		info.noise_ramp * info.noise_trim <= 1.0e-6;
	return zeroTrim || fullClosedTrim || noLineRandomTrim;
}

static bool TrimWebSegment(
	const LineAnimRenderInfo &info,
	A_long lineIndex,
	PF_FpLong time,
	const PathPoint &a,
	const PathPoint &b,
	PathPoint &outA,
	PathPoint &outB,
	bool lockAnimatedNoise = false)
{
	PF_FpLong start = 0.0;
	PF_FpLong end = 1.0;
	if (!WebTrimRange(info, lineIndex, time, start, end, lockAnimatedNoise)) {
		return false;
	}

	const PF_FpLong segStart = std::min(a.u, b.u);
	const PF_FpLong segEnd = std::max(a.u, b.u);
	const PF_FpLong clippedStart = std::max(start, segStart);
	const PF_FpLong clippedEnd = std::min(end, segEnd);
	if (clippedEnd <= clippedStart) {
		return false;
	}

	const PF_FpLong span = std::max<PF_FpLong>(1.0e-6, b.u - a.u);
	outA = InterpPoint(a, b, ClampF((clippedStart - a.u) / span, 0.0, 1.0));
	outB = InterpPoint(a, b, ClampF((clippedEnd - a.u) / span, 0.0, 1.0));
	return true;
}

static bool SamplePolylinePointAtU(const PathPolyline &polyline, PF_FpLong u, PathPoint &out)
{
	if (polyline.points.empty()) {
		return false;
	}
	if (u <= polyline.points.front().u) {
		out = polyline.points.front();
		return true;
	}
	for (size_t i = 1; i < polyline.points.size(); ++i) {
		const PathPoint &a = polyline.points[i - 1];
		const PathPoint &b = polyline.points[i];
		if (u <= b.u) {
			const PF_FpLong span = std::max<PF_FpLong>(1.0e-6, b.u - a.u);
			out = InterpPoint(a, b, ClampF((u - a.u) / span, 0.0, 1.0));
			return true;
		}
	}
	out = polyline.points.back();
	return true;
}

static PF_FpLong WebPointZAt(const WebPolylineRecord &record, const PathPoint &point)
{
	return Lerp(record.z_start, record.z_end, ClampF(point.u, 0.0, 1.0));
}

static void PixelToUnitRGB(const PF_Pixel &px, PF_FpLong &r, PF_FpLong &g, PF_FpLong &b)
{
	r = static_cast<PF_FpLong>(px.red) / static_cast<PF_FpLong>(PF_MAX_CHAN8);
	g = static_cast<PF_FpLong>(px.green) / static_cast<PF_FpLong>(PF_MAX_CHAN8);
	b = static_cast<PF_FpLong>(px.blue) / static_cast<PF_FpLong>(PF_MAX_CHAN8);
}

struct ColorStop {
	PF_FpLong pos;
	A_long index;
};

static void SortedColorStops(const LineAnimRenderInfo &info, ColorStop stops[5])
{
	for (A_long i = 0; i < 5; ++i) {
		stops[i].pos = ClampF(info.line_color_stops[i], 0.0, 1.0);
		stops[i].index = i;
	}
	std::sort(stops, stops + 5, [](const ColorStop &a, const ColorStop &b) {
		if (std::fabs(a.pos - b.pos) <= 1.0e-6) {
			return a.index < b.index;
		}
		return a.pos < b.pos;
	});
}

static void GradientColorAt(const LineAnimRenderInfo &info, PF_FpLong u, PF_FpLong &r, PF_FpLong &g, PF_FpLong &b)
{
	ColorStop stops[5];
	SortedColorStops(info, stops);
	const PF_FpLong cu = ClampF(u, 0.0, 1.0);
	A_long left = 0;
	A_long right = 0;
	for (A_long i = 1; i < 5; ++i) {
		if (cu >= stops[i].pos) {
			left = i;
			right = i;
		} else {
			right = i;
			break;
		}
	}
	if (left == right) {
		PixelToUnitRGB(info.line_colors[stops[left].index], r, g, b);
		return;
	}
	const PF_FpLong span = std::max<PF_FpLong>(1.0e-6, stops[right].pos - stops[left].pos);
	const PF_FpLong t = SmoothStep((cu - stops[left].pos) / span);
	PF_FpLong r0 = 0.0, g0 = 0.0, b0 = 0.0;
	PF_FpLong r1 = 0.0, g1 = 0.0, b1 = 0.0;
	PixelToUnitRGB(info.line_colors[stops[left].index], r0, g0, b0);
	PixelToUnitRGB(info.line_colors[stops[right].index], r1, g1, b1);
	r = Lerp(r0, r1, t);
	g = Lerp(g0, g1, t);
	b = Lerp(b0, b1, t);
}

static void LineColorAt(const LineAnimRenderInfo &info, A_long line, A_long lineCount, PF_FpLong baseR, PF_FpLong baseG, PF_FpLong baseB, PF_FpLong &r, PF_FpLong &g, PF_FpLong &b)
{
	r = baseR;
	g = baseG;
	b = baseB;
	if (info.line_color_amount <= 1.0e-6) {
		return;
	}

	PF_FpLong u = (lineCount <= 1) ? 0.5 : static_cast<PF_FpLong>(line) / static_cast<PF_FpLong>(lineCount - 1);
	u = Wrap01(u + info.line_color_offset + HashSigned(static_cast<A_u_long>(info.noise_seed * 977 + line * 37)) * info.line_color_random);
	PF_FpLong gr = 0.0, gg = 0.0, gb = 0.0;
	GradientColorAt(info, u, gr, gg, gb);
	r = Lerp(baseR, gr, info.line_color_amount);
	g = Lerp(baseG, gg, info.line_color_amount);
	b = Lerp(baseB, gb, info.line_color_amount);
}

static bool PolylineBounds(const std::vector<PathPolyline> &polylines, PF_FpLong &minX, PF_FpLong &minY, PF_FpLong &maxX, PF_FpLong &maxY)
{
	bool havePoint = false;
	minX = minY = 0.0;
	maxX = maxY = 0.0;
	for (const PathPolyline &polyline : polylines) {
		for (const PathPoint &p : polyline.points) {
			if (!havePoint) {
				minX = maxX = p.pos.x;
				minY = maxY = p.pos.y;
				havePoint = true;
			} else {
				minX = std::min(minX, p.pos.x);
				minY = std::min(minY, p.pos.y);
				maxX = std::max(maxX, p.pos.x);
				maxY = std::max(maxY, p.pos.y);
			}
		}
	}
	return havePoint;
}

static bool HasClosedPolyline(const std::vector<PathPolyline> &polylines)
{
	for (const PathPolyline &polyline : polylines) {
		if (polyline.closed && polyline.points.size() >= 3) {
			return true;
		}
	}
	return false;
}

static PathPoint MakeWebAxisPoint(const Vec2 &center, PF_FpLong radius, PF_FpLong baseAngle, PF_FpLong t, const LineAnimRenderInfo &info)
{
	const PF_FpLong angle = baseAngle + info.twist * ClampF(t, 0.0, 1.0);
	const Vec2 radial = { std::cos(angle), std::sin(angle) };
	PathPoint p;
	p.pos = {
		center.x + radial.x * radius,
		center.y + radial.y * radius
	};
	p.normal = radial;
	p.dist = t;
	p.u = ClampF(t, 0.0, 1.0);
	p.turn_angle = angle;
	return p;
}

static bool RandomPointInPolylines(
	const std::vector<PathPolyline> &polylines,
	PF_FpLong minX,
	PF_FpLong minY,
	PF_FpLong maxX,
	PF_FpLong maxY,
	A_u_long seed,
	bool requireInside,
	Vec2 &out)
{
	for (A_long attempt = 0; attempt < 96; ++attempt) {
		const PF_FpLong x = Lerp(minX, maxX, Hash01(seed + static_cast<A_u_long>(attempt * 17 + 3)));
		const PF_FpLong y = Lerp(minY, maxY, Hash01(seed + static_cast<A_u_long>(attempt * 19 + 5)));
		Vec2 p = { x, y };
		if (!requireInside || PointInsideClosedPolylines(polylines, p)) {
			out = p;
			return true;
		}
	}
	out = { (minX + maxX) * 0.5, (minY + maxY) * 0.5 };
	return !requireInside;
}

static void DrawWebPatternLines(
	PF_EffectWorld *output,
	PF_PixelFormat format,
	const LineAnimRenderInfo &info,
	const std::vector<PathPolyline> &boundary,
	PF_FpLong time,
	PF_FpLong r,
	PF_FpLong g,
	PF_FpLong b,
	PF_FpLong er,
	PF_FpLong eg,
	PF_FpLong eb)
{
	if (info.web_pattern <= 1 || info.web_pattern_count <= 0 || boundary.empty()) {
		return;
	}

	PF_FpLong minX = 0.0, minY = 0.0, maxX = 0.0, maxY = 0.0;
	if (!PolylineBounds(boundary, minX, minY, maxX, maxY)) {
		return;
	}

	LineAnimRenderInfo patternInfo = info;
	patternInfo.line_z_spacing = 0.0;
	patternInfo.twist = 0.0;

	const Vec2 center = { (minX + maxX) * 0.5, (minY + maxY) * 0.5 };
	const PF_FpLong radiusX = std::max<PF_FpLong>(1.0, (maxX - minX) * 0.5);
	const PF_FpLong radiusY = std::max<PF_FpLong>(1.0, (maxY - minY) * 0.5);
	const PF_FpLong radius = std::max<PF_FpLong>(1.0, (radiusX + radiusY) * 0.5);
	const A_long count = ClampL(info.web_pattern_count, 0, 256);
	const bool randomInMask = info.web_pattern == 3;
	const bool requireInside = randomInMask && HasClosedPolyline(boundary);
	const DashPattern dash = { 0.0, 0.0, 0.0 };

	for (A_long line = 0; line < count; ++line) {
		const PF_FpLong evenAngle = (count <= 0) ? 0.0 : (6.283185307179586 * static_cast<PF_FpLong>(line) / static_cast<PF_FpLong>(count)) + info.phase;
		PF_FpLong baseAngle = evenAngle;
		PF_FpLong crossRadius = radius;
		if (randomInMask) {
			Vec2 randomPoint;
			const A_u_long seed = static_cast<A_u_long>(info.noise_seed * 65537 + line * 4099 + 137);
			if (RandomPointInPolylines(boundary, minX, minY, maxX, maxY, seed, requireInside, randomPoint)) {
				const PF_FpLong dx = randomPoint.x - center.x;
				const PF_FpLong dy = randomPoint.y - center.y;
				const PF_FpLong randomRadius = std::max<PF_FpLong>(1.0, std::sqrt(dx * dx + dy * dy));
				baseAngle = Lerp(evenAngle, std::atan2(dy, dx), info.web_pattern_random);
				crossRadius = Lerp(radius, randomRadius, info.web_pattern_random);
			}
		}

		PF_FpLong lr = r, lg = g, lb = b;
		PF_FpLong ler = er, leg = eg, leb = eb;
		LineColorAt(info, line, count, r, g, b, lr, lg, lb);
		LineColorAt(info, line, count, er, eg, eb, ler, leg, leb);

		for (const PathPolyline &polyline : boundary) {
			if (polyline.points.size() < 2) {
				continue;
			}
			for (size_t i = 1; i < polyline.points.size(); ++i) {
				PathPoint p0;
				PathPoint p1;
				if (!TrimWebSegment(info, line, time, polyline.points[i - 1], polyline.points[i], p0, p1, false)) {
					continue;
				}

				const PF_FpLong angle0 = baseAngle + info.twist * ClampF(p0.u, 0.0, 1.0);
				const PF_FpLong angle1 = baseAngle + info.twist * ClampF(p1.u, 0.0, 1.0);
				const PF_FpLong normalOffset0 = std::cos(angle0) * crossRadius;
				const PF_FpLong normalOffset1 = std::cos(angle1) * crossRadius;
				const PF_FpLong zOffset0 = std::sin(angle0) * crossRadius;
				const PF_FpLong zOffset1 = std::sin(angle1) * crossRadius;
				const PF_FpLong a0 = AudioOffsetAt(p0, info, line);
				const PF_FpLong a1 = AudioOffsetAt(p1, info, line);
				DrawDashedSegment(
					output,
					format,
					patternInfo,
					dash,
					p0,
					p1,
					normalOffset0 + a0,
					normalOffset1 + a1,
					0.0,
					0.0,
					0.0,
					0.0,
					zOffset0,
					zOffset1,
					info.width,
					static_cast<A_u_long>(info.noise_seed * 131071 + line * 257 + 29),
					time * 0.83,
					info.opacity * info.web_amount,
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
