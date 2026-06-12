#include "kg_strings.h"

#include "AEFX_SuiteHelper.h"

#include <algorithm>
#include <cmath>
#include <new>
#include <utility>
#include <vector>

namespace {

constexpr A_short LINE_COLOR_UI_WIDTH = 260;
constexpr A_short LINE_COLOR_UI_HEIGHT = 34;
constexpr A_short BEND_RAMP_UI_WIDTH = 300;
constexpr A_short BEND_RAMP_UI_HEIGHT = 110;
constexpr PF_UFixed AUDIO_SAMPLE_RATE = 0xac440000;
constexpr A_long PATH_PARAMS[LNA_MAX_PATHS] = {
	LNA_PATH,
	LNA_PATH_2,
	LNA_PATH_3,
	LNA_PATH_4,
	LNA_PATH_5,
	LNA_PATH_6,
	LNA_PATH_7,
	LNA_PATH_8
};

struct Vec2 {
	PF_FpLong x;
	PF_FpLong y;
};

struct Vec3 {
	PF_FpLong x;
	PF_FpLong y;
	PF_FpLong z;
};

struct ProjectedPoint {
	Vec2 screen;
	PF_FpLong view_z;
};

static Vec3 TransformPoint(const A_Matrix4 &m, const Vec3 &p)
{
	const PF_FpLong x = p.x * m.mat[0][0] + p.y * m.mat[1][0] + p.z * m.mat[2][0] + m.mat[3][0];
	const PF_FpLong y = p.x * m.mat[0][1] + p.y * m.mat[1][1] + p.z * m.mat[2][1] + m.mat[3][1];
	const PF_FpLong z = p.x * m.mat[0][2] + p.y * m.mat[1][2] + p.z * m.mat[2][2] + m.mat[3][2];
	const PF_FpLong w = p.x * m.mat[0][3] + p.y * m.mat[1][3] + p.z * m.mat[2][3] + m.mat[3][3];
	if (std::fabs(w) > 1.0e-8 && std::fabs(w - 1.0) > 1.0e-8) {
		return { x / w, y / w, z / w };
	}
	return { x, y, z };
}

static bool InvertMatrix4(const A_Matrix4 &m, A_Matrix4 &out)
{
	PF_FpLong a[4][8] = {};
	for (A_long r = 0; r < 4; ++r) {
		for (A_long c = 0; c < 4; ++c) {
			a[r][c] = m.mat[r][c];
		}
		a[r][r + 4] = 1.0;
	}

	for (A_long c = 0; c < 4; ++c) {
		A_long pivot = c;
		PF_FpLong pivotAbs = std::fabs(a[c][c]);
		for (A_long r = c + 1; r < 4; ++r) {
			const PF_FpLong candidateAbs = std::fabs(a[r][c]);
			if (candidateAbs > pivotAbs) {
				pivot = r;
				pivotAbs = candidateAbs;
			}
		}
		if (pivotAbs <= 1.0e-12) {
			return false;
		}
		if (pivot != c) {
			for (A_long k = 0; k < 8; ++k) {
				std::swap(a[c][k], a[pivot][k]);
			}
		}

		const PF_FpLong invPivot = 1.0 / a[c][c];
		for (A_long k = 0; k < 8; ++k) {
			a[c][k] *= invPivot;
		}
		for (A_long r = 0; r < 4; ++r) {
			if (r == c) {
				continue;
			}
			const PF_FpLong factor = a[r][c];
			if (std::fabs(factor) <= 1.0e-20) {
				continue;
			}
			for (A_long k = 0; k < 8; ++k) {
				a[r][k] -= factor * a[c][k];
			}
		}
	}

	for (A_long r = 0; r < 4; ++r) {
		for (A_long c = 0; c < 4; ++c) {
			out.mat[r][c] = a[r][c + 4];
		}
	}
	return true;
}

struct PathPoint {
	Vec2 pos;
	Vec2 normal;
	PF_FpLong dist;
	PF_FpLong u;
	PF_FpLong turn_angle;
};

struct PathPolyline {
	std::vector<PathPoint> points;
	bool closed = false;
};

struct WebPolylineRecord {
	PathPolyline polyline;
	A_long line_index;
	PF_FpLong trim_start;
	PF_FpLong trim_end;
	PF_FpLong z_start;
	PF_FpLong z_end;
};

struct SegInfo {
	PF_PathSegPrepPtr prep;
	PF_FpLong length;
};

struct TrimRange {
	PF_FpLong start;
	PF_FpLong end;
	bool continue_previous;
};

struct DashPattern {
	PF_FpLong length;
	PF_FpLong gap;
	PF_FpLong offset;
};

static PF_FpLong ClampF(PF_FpLong v, PF_FpLong lo, PF_FpLong hi)
{
	return std::max(lo, std::min(hi, v));
}

static PF_FpLong SafePerspective(PF_FpLong dist, PF_FpLong z)
{
	const PF_FpLong denom = dist + z;
	if (denom <= 1.0e-6) {
		return 1.0;
	}
	return ClampF(dist / denom, 0.05, 20.0);
}

static PF_FpLong LoopTwistEnvelope(PF_FpLong u)
{
	const PF_FpLong cu = ClampF(u, 0.0, 1.0);
	return (cu <= 0.5) ? cu * 2.0 : (1.0 - cu) * 2.0;
}

static A_long ClampL(A_long v, A_long lo, A_long hi)
{
	return std::max(lo, std::min(hi, v));
}

static PF_FpLong Hash01(A_u_long v)
{
	v ^= v >> 16;
	v *= 0x7feb352dU;
	v ^= v >> 15;
	v *= 0x846ca68bU;
	v ^= v >> 16;
	return static_cast<PF_FpLong>(v & 0x00ffffffU) / static_cast<PF_FpLong>(0x01000000U);
}

static PF_FpLong HashSigned(A_u_long v)
{
	return Hash01(v) * 2.0 - 1.0;
}

static PF_FpLong Wrap01(PF_FpLong v)
{
	v = std::fmod(v, 1.0);
	return (v < 0.0) ? v + 1.0 : v;
}

static PF_FpLong WrapDistance(PF_FpLong distance, PF_FpLong totalLength)
{
	if (totalLength <= 0.0) {
		return 0.0;
	}
	distance = std::fmod(distance, totalLength);
	return (distance < 0.0) ? distance + totalLength : distance;
}

static PF_FpLong Lerp(PF_FpLong a, PF_FpLong b, PF_FpLong t)
{
	return a + (b - a) * t;
}

static PF_FpLong SmoothStep(PF_FpLong t)
{
	t = ClampF(t, 0.0, 1.0);
	return t * t * (3.0 - 2.0 * t);
}

static PF_FpLong CoverageFromDist2(PF_FpLong dist2, PF_FpLong radius)
{
	const PF_FpLong outer = radius + 0.5;
	if (dist2 >= outer * outer) {
		return 0.0;
	}
	const PF_FpLong inner = std::max<PF_FpLong>(0.0, radius - 0.5);
	if (dist2 <= inner * inner) {
		return 1.0;
	}
	return ClampF(outer - std::sqrt(dist2), 0.0, 1.0);
}

static bool SegmentBoundsOutside(const Vec2 &a, const Vec2 &b, PF_FpLong radius, const PF_EffectWorld *world)
{
	return std::max(a.x, b.x) + radius + 1.0 < 0.0 ||
		std::min(a.x, b.x) - radius - 1.0 > static_cast<PF_FpLong>(world->width - 1) ||
		std::max(a.y, b.y) + radius + 1.0 < 0.0 ||
		std::min(a.y, b.y) - radius - 1.0 > static_cast<PF_FpLong>(world->height - 1);
}

static bool CircleBoundsOutside(const Vec2 &center, PF_FpLong radius, const PF_EffectWorld *world)
{
	return center.x + radius + 1.0 < 0.0 ||
		center.x - radius - 1.0 > static_cast<PF_FpLong>(world->width - 1) ||
		center.y + radius + 1.0 < 0.0 ||
		center.y - radius - 1.0 > static_cast<PF_FpLong>(world->height - 1);
}

static bool IntersectPixelCenterInterval(PF_FpLong coeff, PF_FpLong base, PF_FpLong lo, PF_FpLong hi, PF_FpLong &minPx, PF_FpLong &maxPx)
{
	constexpr PF_FpLong kEpsilon = 1.0e-8;
	if (std::fabs(coeff) <= kEpsilon) {
		return base >= lo && base <= hi;
	}

	PF_FpLong a = (lo - base) / coeff;
	PF_FpLong b = (hi - base) / coeff;
	if (a > b) {
		std::swap(a, b);
	}
	minPx = std::max(minPx, a);
	maxPx = std::min(maxPx, b);
	return minPx <= maxPx;
}

static bool SegmentScanlineXRange(
	const Vec2 &a,
	PF_FpLong len,
	PF_FpLong tx,
	PF_FpLong ty,
	PF_FpLong radius,
	A_long minX,
	A_long maxX,
	A_long y,
	A_long &rowMinX,
	A_long &rowMaxX)
{
	const PF_FpLong py = static_cast<PF_FpLong>(y) + 0.5;
	const PF_FpLong dy = py - a.y;
	const PF_FpLong paddedRadius = radius + 0.5;
	PF_FpLong minPx = static_cast<PF_FpLong>(minX) + 0.5;
	PF_FpLong maxPx = static_cast<PF_FpLong>(maxX) + 0.5;

	if (!IntersectPixelCenterInterval(-ty, a.x * ty + dy * tx, -paddedRadius, paddedRadius, minPx, maxPx)) {
		return false;
	}
	if (!IntersectPixelCenterInterval(tx, -a.x * tx + dy * ty, -paddedRadius, len + paddedRadius, minPx, maxPx)) {
		return false;
	}

	rowMinX = ClampL(static_cast<A_long>(std::ceil(minPx - 0.5)), minX, maxX);
	rowMaxX = ClampL(static_cast<A_long>(std::floor(maxPx - 0.5)), minX, maxX);
	return rowMinX <= rowMaxX;
}

static PF_FpLong SmoothNoise(A_u_long seed, PF_FpLong time)
{
	const PF_FpLong a = HashSigned(seed);
	const PF_FpLong b = HashSigned(seed + 0x9e3779b9U);
	const PF_FpLong c = HashSigned(seed + 0x85ebca6bU);
	return (std::sin(time + a * 6.283185307179586) +
			0.5 * std::sin(time * 1.731 + b * 6.283185307179586) +
			0.25 * std::sin(time * 2.417 + c * 6.283185307179586)) / 1.75;
}

static PF_FpLong CurrentSeconds(PF_InData *in_data)
{
	return in_data->time_scale ? static_cast<PF_FpLong>(in_data->current_time) / static_cast<PF_FpLong>(in_data->time_scale) : 0.0;
}

// ─── Grunge (brush stroke) noise ─────────────────────────────────────────────
// 1-D value noise with quintic interpolation between random lattice values.
static PF_FpLong GrungeNoise1D(A_u_long seed, PF_FpLong x)
{
	const PF_FpLong xi = std::floor(x);
	const PF_FpLong xf = x - xi;
	const A_u_long ix = static_cast<A_u_long>(static_cast<A_long>(xi));
	const PF_FpLong v0 = HashSigned(seed + ix);
	const PF_FpLong v1 = HashSigned(seed + ix + 1u);
	const PF_FpLong t = xf * xf * xf * (xf * (xf * 6.0 - 15.0) + 10.0);
	return Lerp(v0, v1, t);
}

// Fractal Brownian Motion sampled along the tangent axis.
static PF_FpLong GrungeFBM(A_u_long seed, PF_FpLong along, PF_FpLong crossN, PF_FpLong roughness, A_long octaves)
{
	PF_FpLong value  = 0.0;
	PF_FpLong amp    = 0.5;
	PF_FpLong freq   = 1.0;
	PF_FpLong maxAmp = 0.0;
	const PF_FpLong persist    = Lerp(0.70, 0.35, roughness);
	const PF_FpLong lacunarity = Lerp(1.80, 2.50, roughness);
	for (A_long i = 0; i < octaves; ++i) {
		const PF_FpLong sampleX = along * freq + crossN * freq * 0.18;
		value  += GrungeNoise1D(seed + static_cast<A_u_long>(i) * 0x9e3779b9U, sampleX) * amp;
		maxAmp += amp;
		amp    *= persist;
		freq   *= lacunarity;
	}
	return (maxAmp > 1.0e-8) ? value / maxAmp : 0.0;
}

// Per-pixel coverage modifier for the grunge / brush-stroke effect.
// `along`   = distance along the tangent from segment start (pixels)
// `cross`   = signed perpendicular distance from the center line (pixels)
// `radius`  = half-width of the stroke at this point
// Returns a [0..1] alpha that creates torn, fibrous brush edges.
static PF_FpLong GrungeCoverage(
	A_u_long seed,
	PF_FpLong along,
	PF_FpLong cross,
	PF_FpLong radius,
	PF_FpLong amount,
	PF_FpLong roughness,
	PF_FpLong frequency,
	PF_FpLong jitter)
{
	if (radius < 1.0e-6 || amount < 1.0e-6) {
		return 1.0;
	}
	const PF_FpLong nCross   = cross / radius;
	const PF_FpLong absCross = std::fabs(nCross);
	const PF_FpLong freqScale = std::max<PF_FpLong>(1.0, frequency);
	const A_long octaves = 2 + static_cast<A_long>(std::floor(roughness * 4.0 + 0.5));
	const PF_FpLong fbm = GrungeFBM(seed, along / freqScale, nCross, roughness, octaves);
	const PF_FpLong edgeDisplace = fbm * amount;
	const PF_FpLong effectiveEdge = 1.0 + edgeDisplace;
	const PF_FpLong edgeDist  = effectiveEdge - absCross;
	const PF_FpLong aaWidth   = 0.5 / std::max<PF_FpLong>(radius, 0.5);
	PF_FpLong edgeCoverage = ClampF(edgeDist / aaWidth, 0.0, 1.0);
	if (jitter > 1.0e-6) {
		const PF_FpLong jFBM = GrungeFBM(seed + 0xdeadbeefU, along / (freqScale * 1.7), nCross * 0.5, roughness, 2);
		edgeCoverage *= 1.0 - jitter * ClampF(-jFBM, 0.0, 1.0);
	}
	return ClampF(edgeCoverage, 0.0, 1.0);
}
// ─────────────────────────────────────────────────────────────────────────────

static void Normalize(Vec2 &v)
{
	const PF_FpLong len = std::sqrt(v.x * v.x + v.y * v.y);
	if (len > 1.0e-6) {
		v.x /= len;
		v.y /= len;
	} else {
		v.x = 1.0;
		v.y = 0.0;
	}
}

static void MakeClosedSeamContinuous(PathPolyline &polyline, bool force = false);
static void ApplyPathTurnAngles(PathPolyline &polyline, const LineAnimRenderInfo &info);
static PF_FpLong BendRampAt(PF_FpLong u, const LineAnimRenderInfo &info);
static PF_Err BuildAudioReactiveData(PF_InData *in_data, LineAnimRenderInfo *info);

static bool UseSphere3D(const LineAnimRenderInfo &info)
{
	return info.enable_3d && info.shape_3d == 2;
}

static PF_FpLong Dot(const Vec2 &a, const Vec2 &b)
{
	return a.x * b.x + a.y * b.y;
}

static PF_FpLong Cross(const Vec2 &a, const Vec2 &b)
{
	return a.x * b.y - a.y * b.x;
}

static void SetFullRect(PF_LRect &r, A_long width, A_long height)
{
	r.left = 0;
	r.top = 0;
	r.right = width;
	r.bottom = height;
}
