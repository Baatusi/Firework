// FireworkItem.h: 单簇静态烟花的数据模型与绘制接口。
//

#pragma once

class CFireworkItem : public CObject
{
	DECLARE_SERIAL(CFireworkItem)

public:
	enum FireworkType
	{
		Radial = 0,
		Ring = 1,
		DoubleLayer = 2
	};

	CFireworkItem();
	CFireworkItem(
		CPoint center,
		int radius,
		int particleCount,
		int type,
		COLORREF color,
		COLORREF innerColor,
		DWORD fadeDurationMs,
		int seed);
	virtual ~CFireworkItem() = default;

	virtual void Serialize(CArchive& ar) override;

	void Draw(CDC* pDC, COLORREF backgroundColor = RGB(255, 255, 255)) const;
	void DrawWithOpacity(CDC* pDC, COLORREF backgroundColor, double opacity) const;
	bool HitTest(CPoint point) const;
	CRect GetBounds() const;
	DWORD GetFadeElapsedMs() const noexcept;
	double GetFadeOpacity() const noexcept;
	bool IsFading() const noexcept;
	bool IsFadeComplete() const noexcept;

	CPoint GetCenter() const noexcept { return m_ptCenter; }
	int GetRadius() const noexcept { return m_nRadius; }
	int GetParticleCount() const noexcept { return m_nParticleNum; }
	int GetType() const noexcept { return m_nType; }
	int GetSeed() const noexcept { return m_nSeed; }
	COLORREF GetColor() const noexcept { return m_crColor; }
	COLORREF GetInnerColor() const noexcept { return m_crInnerColor; }
	DWORD GetFadeDurationMs() const noexcept { return m_nFadeDurationMs; }
	// 仅用于本次编辑会话的绘制顺序，不写入文件。
	ULONGLONG GetEditOrder() const noexcept { return m_editOrder; }
	void SetEditOrder(ULONGLONG order) noexcept { m_editOrder = order; }

protected:
	ULONGLONG m_editOrder = 0;
	CPoint m_ptCenter;     // 烟花中心位置
	int m_nRadius;         // 烟花最大半径
	int m_nParticleNum;    // 粒子数量
	int m_nType;           // FireworkType 中定义的烟花类型
	int m_nSeed;           // 用于重现粒子分布的随机种子
	COLORREF m_crColor;      // 烟花主颜色；双层型中作为外层颜色
	COLORREF m_crInnerColor; // 双层型烟花的内层颜色
	DWORD m_nFadeDurationMs; // 淡出总时长；0 表示不淡出
	DWORD m_nFadeElapsedMs;  // 本次运行开始前已经经过的淡出时间
	ULONGLONG m_nFadeStartTick; // 本次运行的淡出计时起点
};
