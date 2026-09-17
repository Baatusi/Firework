// FireworkItem.cpp: CFireworkItem 类的实现。
//

#include "pch.h"
#include <cmath>
#include <cstdint>

#include "FireworkItem.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_SERIAL(CFireworkItem, CObject, VERSIONABLE_SCHEMA | 3)

namespace
{
	constexpr double kPi = 3.14159265358979323846;
	constexpr int kMinRadius = 8;
	constexpr int kMaxRadius = 2000;
	constexpr int kMinParticleCount = 6;
	constexpr int kMaxParticleCount = 720;
	constexpr DWORD kMinFadeDurationMs = 1000;
	constexpr DWORD kMaxFadeDurationMs = 600000;

	int ClampInt(int value, int minimum, int maximum)
	{
		if (value < minimum)
			return minimum;
		if (value > maximum)
			return maximum;
		return value;
	}

	class CDeterministicRandom
	{
	public:
		explicit CDeterministicRandom(int seed) noexcept
			: m_state(static_cast<std::uint32_t>(seed))
		{
		}

		int Range(int minimum, int maximum) noexcept
		{
			if (maximum <= minimum)
				return minimum;

			const std::uint32_t span = static_cast<std::uint32_t>(maximum - minimum + 1);
			return minimum + static_cast<int>(Next() % span);
		}

		double Unit() noexcept
		{
			return static_cast<double>(Next()) / static_cast<double>(UINT32_MAX);
		}

	private:
		std::uint32_t Next() noexcept
		{
			m_state = m_state * 1664525u + 1013904223u;
			return m_state;
		}

		std::uint32_t m_state;
	};

	CPoint PointOnCircle(CPoint center, double radius, double angle)
	{
		return CPoint(
			center.x + static_cast<LONG>(std::lround(radius * std::cos(angle))),
			center.y + static_cast<LONG>(std::lround(radius * std::sin(angle))));
	}

	BYTE LightenChannel(BYTE value)
	{
		return static_cast<BYTE>(ClampInt(static_cast<int>(value) + 80, 0, 255));
	}

	COLORREF MakeHighlightColor(COLORREF color)
	{
		return RGB(
			LightenChannel(GetRValue(color)),
			LightenChannel(GetGValue(color)),
			LightenChannel(GetBValue(color)));
	}

	DWORD NormalizeFadeDuration(DWORD durationMs)
	{
		if (durationMs == 0)
			return 0;
		if (durationMs < kMinFadeDurationMs)
			return kMinFadeDurationMs;
		if (durationMs > kMaxFadeDurationMs)
			return kMaxFadeDurationMs;
		return durationMs;
	}

	BYTE BlendChannel(BYTE source, BYTE background, double opacity)
	{
		const double blended =
			background + (static_cast<double>(source) - background) * opacity;
		return static_cast<BYTE>(ClampInt(
			static_cast<int>(std::lround(blended)), 0, 255));
	}

	COLORREF BlendWithBackground(COLORREF color, COLORREF background, double opacity)
	{
		return RGB(
			BlendChannel(GetRValue(color), GetRValue(background), opacity),
			BlendChannel(GetGValue(color), GetGValue(background), opacity),
			BlendChannel(GetBValue(color), GetBValue(background), opacity));
	}

	void DrawRadialLayer(
		CDC* pDC,
		CPoint center,
		int radius,
		int particleCount,
		double startRadiusRatio,
		CDeterministicRandom& random,
		CPen& mainPen,
		CPen& highlightPen,
		CBrush& particleBrush)
	{
		if (particleCount <= 0)
			return;

		const double angleStep = 2.0 * kPi / particleCount;
		for (int index = 0; index < particleCount; ++index)
		{
			const double jitter = (random.Unit() - 0.5) * angleStep * 0.55;
			const double angle = index * angleStep + jitter;
			const double endRatio = random.Range(78, 100) / 100.0;
			const double startRatio = startRadiusRatio + random.Range(0, 8) / 100.0;
			const CPoint start = PointOnCircle(center, radius * startRatio, angle);
			const CPoint end = PointOnCircle(center, radius * endRatio, angle);
			const CPoint highlightStart(
				start.x + (end.x - start.x) * 3 / 4,
				start.y + (end.y - start.y) * 3 / 4);

			pDC->SelectObject(&mainPen);
			pDC->MoveTo(start);
			pDC->LineTo(end);

			pDC->SelectObject(&highlightPen);
			pDC->MoveTo(highlightStart);
			pDC->LineTo(end);

			if ((index % 4) == 0)
			{
				pDC->SelectObject(&particleBrush);
				pDC->Ellipse(end.x - 2, end.y - 2, end.x + 3, end.y + 3);
			}
		}
	}
}

CFireworkItem::CFireworkItem()
	: m_ptCenter(0, 0)
	, m_nRadius(60)
	, m_nParticleNum(36)
	, m_nType(Radial)
	, m_nSeed(1)
	, m_crColor(RGB(255, 180, 40))
	, m_crInnerColor(RGB(80, 180, 255))
	, m_nFadeDurationMs(0)
	, m_nFadeElapsedMs(0)
	, m_nFadeStartTick(::GetTickCount64())
{
}

CFireworkItem::CFireworkItem(
	CPoint center,
	int radius,
	int particleCount,
	int type,
	COLORREF color,
	COLORREF innerColor,
	DWORD fadeDurationMs,
	int seed)
	: m_ptCenter(center)
	, m_nRadius(ClampInt(radius, kMinRadius, kMaxRadius))
	, m_nParticleNum(ClampInt(particleCount, kMinParticleCount, kMaxParticleCount))
	, m_nType(ClampInt(type, static_cast<int>(Radial), static_cast<int>(DoubleLayer)))
	, m_nSeed(seed)
	, m_crColor(color & 0x00FFFFFF)
	, m_crInnerColor(innerColor & 0x00FFFFFF)
	, m_nFadeDurationMs(NormalizeFadeDuration(fadeDurationMs))
	, m_nFadeElapsedMs(0)
	, m_nFadeStartTick(::GetTickCount64())
{
}

void CFireworkItem::Serialize(CArchive& ar)
{
	CObject::Serialize(ar);

	if (ar.IsStoring())
	{
		ar << m_ptCenter.x;
		ar << m_ptCenter.y;
		ar << m_nRadius;
		ar << m_nParticleNum;
		ar << m_nType;
		ar << m_nSeed;
		ar << static_cast<DWORD>(m_crColor);
		ar << static_cast<DWORD>(m_crInnerColor);
		ar << m_nFadeDurationMs;
		ar << GetFadeElapsedMs();
	}
	else
	{
		const UINT schema = ar.GetObjectSchema();
		if (schema < 1 || schema > 3)
			AfxThrowArchiveException(CArchiveException::badSchema);

		DWORD color = 0;
		DWORD innerColor = 0;
		ar >> m_ptCenter.x;
		ar >> m_ptCenter.y;
		ar >> m_nRadius;
		ar >> m_nParticleNum;
		ar >> m_nType;
		ar >> m_nSeed;
		ar >> color;
		if (schema >= 2)
			ar >> innerColor;
		else
			innerColor = color;

		DWORD fadeDurationMs = 0;
		DWORD fadeElapsedMs = 0;
		if (schema >= 3)
		{
			ar >> fadeDurationMs;
			ar >> fadeElapsedMs;
		}

		m_nRadius = ClampInt(m_nRadius, kMinRadius, kMaxRadius);
		m_nParticleNum = ClampInt(m_nParticleNum, kMinParticleCount, kMaxParticleCount);
		m_nType = ClampInt(m_nType, static_cast<int>(Radial), static_cast<int>(DoubleLayer));
		m_crColor = color & 0x00FFFFFF;
		m_crInnerColor = innerColor & 0x00FFFFFF;
		m_nFadeDurationMs = NormalizeFadeDuration(fadeDurationMs);
		m_nFadeElapsedMs = m_nFadeDurationMs == 0
			? 0
			: min(fadeElapsedMs, m_nFadeDurationMs);
		m_nFadeStartTick = ::GetTickCount64();
	}
}

DWORD CFireworkItem::GetFadeElapsedMs() const noexcept
{
	if (m_nFadeDurationMs == 0)
		return 0;

	const ULONGLONG runtimeElapsed = ::GetTickCount64() - m_nFadeStartTick;
	const ULONGLONG totalElapsed =
		static_cast<ULONGLONG>(m_nFadeElapsedMs) + runtimeElapsed;
	return static_cast<DWORD>(min(
		totalElapsed, static_cast<ULONGLONG>(m_nFadeDurationMs)));
}

double CFireworkItem::GetFadeOpacity() const noexcept
{
	if (m_nFadeDurationMs == 0)
		return 1.0;

	return 1.0 - static_cast<double>(GetFadeElapsedMs()) / m_nFadeDurationMs;
}

bool CFireworkItem::IsFading() const noexcept
{
	return m_nFadeDurationMs != 0 && !IsFadeComplete();
}

bool CFireworkItem::IsFadeComplete() const noexcept
{
	return m_nFadeDurationMs != 0 && GetFadeElapsedMs() >= m_nFadeDurationMs;
}

void CFireworkItem::Draw(CDC* pDC, COLORREF backgroundColor) const
{
	DrawWithOpacity(pDC, backgroundColor, GetFadeOpacity());
}

void CFireworkItem::DrawWithOpacity(
	CDC* pDC, COLORREF backgroundColor, double opacity) const
{
	if (pDC == nullptr || pDC->GetSafeHdc() == nullptr)
		return;

	opacity = max(0.0, min(opacity, 1.0));
	if (opacity <= 0.0)
		return;

	const int savedState = pDC->SaveDC();
	if (savedState == 0)
		return;

	const COLORREF visibleMainColor =
		BlendWithBackground(m_crColor, backgroundColor, opacity);
	const COLORREF visibleHighlightColor = BlendWithBackground(
		MakeHighlightColor(m_crColor), backgroundColor, opacity);
	CPen mainPen(PS_SOLID, 1, visibleMainColor);
	CPen highlightPen(PS_SOLID, 2, visibleHighlightColor);
	CBrush particleBrush(visibleHighlightColor);
	CDeterministicRandom random(m_nSeed);

	pDC->SetBkMode(TRANSPARENT);
	pDC->SelectStockObject(NULL_BRUSH);

	switch (m_nType)
	{
	case Ring:
	{
		const double angleStep = 2.0 * kPi / m_nParticleNum;
		for (int index = 0; index < m_nParticleNum; ++index)
		{
			const double endAngle =
				index * angleStep + (random.Unit() - 0.5) * angleStep * 0.30;
			const double startAngle =
				endAngle - random.Range(2, 7) * kPi / 180.0;
			const double startRadius =
				m_nRadius * random.Range(66, 76) / 100.0;
			const double endRadius =
				m_nRadius * random.Range(88, 100) / 100.0;
			const CPoint start = PointOnCircle(m_ptCenter, startRadius, startAngle);
			const CPoint end = PointOnCircle(m_ptCenter, endRadius, endAngle);
			const CPoint highlightStart(
				start.x + (end.x - start.x) * 2 / 3,
				start.y + (end.y - start.y) * 2 / 3);

			pDC->SelectObject(&mainPen);
			pDC->MoveTo(start);
			pDC->LineTo(end);

			pDC->SelectObject(&highlightPen);
			pDC->MoveTo(highlightStart);
			pDC->LineTo(end);

			pDC->SelectObject(&particleBrush);
			const int headRadius = (index % 4) == 0 ? 2 : 1;
			pDC->Ellipse(
				end.x - headRadius,
				end.y - headRadius,
				end.x + headRadius + 1,
				end.y + headRadius + 1);

			// 在拖尾附近点缀少量小火星，打破过于规整的圆周轮廓。
			if ((index % 5) == 0)
			{
				const double sparkAngle =
					endAngle + (random.Unit() - 0.5) * angleStep * 0.8;
				const double sparkRadius =
					m_nRadius * random.Range(78, 96) / 100.0;
				const CPoint spark = PointOnCircle(m_ptCenter, sparkRadius, sparkAngle);
				pDC->Ellipse(spark.x - 1, spark.y - 1, spark.x + 2, spark.y + 2);
			}
		}
		break;
	}

	case DoubleLayer:
	{
		const COLORREF visibleInnerColor =
			BlendWithBackground(m_crInnerColor, backgroundColor, opacity);
		const COLORREF visibleInnerHighlightColor = BlendWithBackground(
			MakeHighlightColor(m_crInnerColor), backgroundColor, opacity);
		CPen innerMainPen(PS_SOLID, 1, visibleInnerColor);
		CPen innerHighlightPen(PS_SOLID, 2, visibleInnerHighlightColor);
		CBrush innerParticleBrush(visibleInnerHighlightColor);
		const int outerCount = (m_nParticleNum + 1) / 2;
		const int innerCount = m_nParticleNum - outerCount;
		DrawRadialLayer(
			pDC, m_ptCenter, m_nRadius, outerCount, 0.14,
			random, mainPen, highlightPen, particleBrush);
		DrawRadialLayer(
			pDC, m_ptCenter, m_nRadius * 3 / 5, innerCount, 0.20,
			random, innerMainPen, innerHighlightPen, innerParticleBrush);
		break;
	}

	case Radial:
	default:
		DrawRadialLayer(
			pDC, m_ptCenter, m_nRadius, m_nParticleNum, 0.12,
			random, mainPen, highlightPen, particleBrush);
		break;
	}

	pDC->RestoreDC(savedState);
}

bool CFireworkItem::HitTest(CPoint point) const
{
	constexpr LONGLONG hitTolerance = 6;
	const LONGLONG dx = static_cast<LONGLONG>(point.x) - m_ptCenter.x;
	const LONGLONG dy = static_cast<LONGLONG>(point.y) - m_ptCenter.y;
	const LONGLONG hitRadius = static_cast<LONGLONG>(m_nRadius) + hitTolerance;
	return dx * dx + dy * dy <= hitRadius * hitRadius;
}

CRect CFireworkItem::GetBounds() const
{
	constexpr int hitTolerance = 6;
	const int extent = m_nRadius + hitTolerance;
	return CRect(
		m_ptCenter.x - extent,
		m_ptCenter.y - extent,
		m_ptCenter.x + extent + 1,
		m_ptCenter.y + extent + 1);
}
