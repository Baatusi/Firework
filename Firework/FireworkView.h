
// FireworkView.h: CFireworkView 类的接口
//

#pragma once

#include <random>
#include <vector>

class CFireworkDoc;

class CFireworkView : public CView
{
protected: // 仅从序列化创建
	CFireworkView() noexcept;
	DECLARE_DYNCREATE(CFireworkView)

// 特性
public:
	CFireworkDoc* GetDocument() const;
	COLORREF GetCurrentColor() const noexcept { return m_currentColor; }
	COLORREF GetCurrentInnerColor() const noexcept { return m_currentInnerColor; }
	COLORREF GetBackgroundColor() const noexcept;
	DWORD GetCurrentFadeDurationMs() const noexcept { return m_currentFadeDurationMs; }
	int GetCurrentType() const noexcept { return m_currentType; }
	int GetMaxRadius() const noexcept { return m_maxRadius; }

// 操作
public:
	void SetCurrentColor(COLORREF color) noexcept;
	void SetCurrentInnerColor(COLORREF color) noexcept;
	void SetBackgroundColor(COLORREF color) noexcept;
	void SetCurrentFadeDurationMs(DWORD durationMs) noexcept;
	BOOL SetCurrentType(int type) noexcept;
	BOOL SetMaxRadius(int maxRadius) noexcept;

// 重写
public:
	virtual void OnDraw(CDC* pDC);  // 重写以绘制该视图
	virtual void OnInitialUpdate();
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:

// 实现
public:
	virtual ~CFireworkView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	struct PlaybackEntry
	{
		PlaybackEntry(
			INT_PTR index, ULONGLONG appearanceMs, DWORD fadeMs) noexcept
			: fireworkIndex(index)
			, appearanceTimeMs(appearanceMs)
			, fadeDurationMs(fadeMs)
		{
		}

		INT_PTR fireworkIndex;
		ULONGLONG appearanceTimeMs;
		DWORD fadeDurationMs;
	};

	COLORREF m_currentColor;
	COLORREF m_currentInnerColor;
	DWORD m_currentFadeDurationMs;
	int m_currentType;
	int m_maxRadius;
	std::mt19937 m_randomEngine;
	std::mt19937 m_playbackRandomEngine;
	bool m_fadeTimerRunning;
	bool m_playbackTimerRunning;
	bool m_isPlaying;
	ULONGLONG m_playbackStartTick;
	INT_PTR m_playbackFireworkCount;
	ULONGLONG m_playbackCompletionTimeMs;
	std::vector<PlaybackEntry> m_playbackPlan;

	void UpdateFadeTimer();
	void FinishPlayback(bool showCompletionMessage);
	void StartPlayback(bool withMusic);

// 生成的消息映射函数
protected:
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnFireworkChooseColor();
	afx_msg void OnFireworkChooseInnerColor();
	afx_msg void OnFireworkChooseBackgroundColor();
	afx_msg void OnUpdateFireworkInnerColor(CCmdUI* pCmdUI);
	afx_msg void OnFireworkFadeSettings();
	afx_msg void OnFireworkSizeSettings();
	afx_msg void OnFireworkType(UINT commandId);
	afx_msg void OnUpdateFireworkType(CCmdUI* pCmdUI);
	afx_msg void OnFireworkClear();
	afx_msg void OnUpdateFireworkClear(CCmdUI* pCmdUI);
	afx_msg void OnPlaybackSelectMusic();
	afx_msg void OnPlaybackStart();
	afx_msg void OnPlaybackStartSilent();
	afx_msg void OnEditUndo();
	afx_msg void OnEditRedo();
	afx_msg void OnUpdateEditUndo(CCmdUI* pCmdUI);
	afx_msg void OnUpdateEditRedo(CCmdUI* pCmdUI);
	afx_msg void OnPlaybackStop();
	afx_msg void OnUpdatePlaybackSelectMusic(CCmdUI* pCmdUI);
	afx_msg void OnUpdatePlaybackStart(CCmdUI* pCmdUI);
	afx_msg void OnUpdatePlaybackStartSilent(CCmdUI* pCmdUI);
	afx_msg void OnUpdatePlaybackStop(CCmdUI* pCmdUI);
	afx_msg void OnUpdateEditingCommand(CCmdUI* pCmdUI);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnDestroy();
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG  // FireworkView.cpp 中的调试版本
inline CFireworkDoc* CFireworkView::GetDocument() const
   { return reinterpret_cast<CFireworkDoc*>(m_pDocument); }
#endif

