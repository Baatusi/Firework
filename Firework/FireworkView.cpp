
// FireworkView.cpp: CFireworkView 类的实现
//

#include "pch.h"
#include "framework.h"
// SHARED_HANDLERS 可以在实现预览、缩略图和搜索筛选器句柄的
// ATL 项目中进行定义，并允许与该项目共享文档代码。
#ifndef SHARED_HANDLERS
#include "Firework.h"
#endif

#include "afxdialogex.h"
#include <climits>
#include <memory>
#include <mmsystem.h>
#include <new>

#pragma comment(lib, "winmm.lib")

#include "FireworkItem.h"
#include "FireworkDoc.h"
#include "FireworkView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
	constexpr UINT_PTR kFadeTimerId = 1;
	constexpr UINT_PTR kPlaybackTimerId = 2;
	constexpr UINT kFadeTimerIntervalMs = 100;
	constexpr UINT kPlaybackTimerIntervalMs = 50;
	constexpr DWORD kMinPlaybackAppearanceIntervalMs = 500;
	constexpr DWORD kMaxPlaybackAppearanceIntervalMs = 1200;
	constexpr DWORD kMinPlaybackFadeDurationMs = 10000;
	constexpr DWORD kMaxPlaybackFadeDurationMs = 30000;
	constexpr ULONGLONG kPlaybackEndDelayAfterLastAppearanceMs = 10000;
	constexpr DWORD kMinFadeDurationMs = 1000;
	constexpr DWORD kMaxFadeDurationMs = 600000;
	constexpr UINT kDefaultFadeSeconds = 10;
	constexpr int kMinFireworkRadius = 40;
	constexpr int kMaxFireworkRadius = 500;
	constexpr int kDefaultMaxFireworkRadius = 100;

	class CFadeSettingsDialog : public CDialogEx
	{
	public:
		CFadeSettingsDialog(DWORD durationMs, CWnd* pParent)
			: CDialogEx(IDD_FADE_SETTINGS, pParent)
			, m_fadeSeconds(durationMs == 0
				? kDefaultFadeSeconds
				: static_cast<UINT>((durationMs + 999) / 1000))
			, m_neverFade(durationMs == 0)
		{
		}

		DWORD GetFadeDurationMs() const noexcept
		{
			return m_neverFade ? 0 : m_fadeSeconds * 1000;
		}

	protected:
		virtual void DoDataExchange(CDataExchange* pDX) override
		{
			CDialogEx::DoDataExchange(pDX);
			DDX_Check(pDX, IDC_FADE_NEVER, m_neverFade);
			if (!pDX->m_bSaveAndValidate || !m_neverFade)
			{
				DDX_Text(pDX, IDC_FADE_SECONDS, m_fadeSeconds);
				if (pDX->m_bSaveAndValidate)
					DDV_MinMaxUInt(pDX, m_fadeSeconds, 1, 600);
			}
		}

		virtual BOOL OnInitDialog() override
		{
			const BOOL result = CDialogEx::OnInitDialog();
			UpdateFadeControls();
			return result;
		}

		afx_msg void OnToggleNeverFade()
		{
			m_neverFade = IsDlgButtonChecked(IDC_FADE_NEVER) == BST_CHECKED;
			UpdateFadeControls();
		}

		void UpdateFadeControls()
		{
			CWnd* const pSecondsEdit = GetDlgItem(IDC_FADE_SECONDS);
			if (pSecondsEdit != nullptr)
				pSecondsEdit->EnableWindow(!m_neverFade);
		}

		UINT m_fadeSeconds;
		BOOL m_neverFade;

		DECLARE_MESSAGE_MAP()
	};

	BEGIN_MESSAGE_MAP(CFadeSettingsDialog, CDialogEx)
		ON_BN_CLICKED(IDC_FADE_NEVER, &CFadeSettingsDialog::OnToggleNeverFade)
	END_MESSAGE_MAP()

	class CFireworkSizeSettingsDialog : public CDialogEx
	{
	public:
		CFireworkSizeSettingsDialog(int maxRadius, CWnd* pParent)
			: CDialogEx(IDD_FIREWORK_SIZE_SETTINGS, pParent)
			, m_maxRadius(static_cast<UINT>(maxRadius))
		{
		}

		int GetMaxRadius() const noexcept
		{
			return static_cast<int>(m_maxRadius);
		}

	protected:
		virtual void DoDataExchange(CDataExchange* pDX) override
		{
			CDialogEx::DoDataExchange(pDX);
			DDX_Text(pDX, IDC_FIREWORK_MAX_RADIUS, m_maxRadius);
			if (pDX->m_bSaveAndValidate)
			{
				DDV_MinMaxUInt(
					pDX,
					m_maxRadius,
					static_cast<UINT>(kMinFireworkRadius),
					static_cast<UINT>(kMaxFireworkRadius));
			}
		}

		UINT m_maxRadius;
	};
}


// CFireworkView

IMPLEMENT_DYNCREATE(CFireworkView, CView)

BEGIN_MESSAGE_MAP(CFireworkView, CView)
	ON_WM_LBUTTONDOWN()
	ON_WM_RBUTTONDOWN()
	ON_COMMAND(ID_FIREWORK_COLOR, &CFireworkView::OnFireworkChooseColor)
	ON_COMMAND(ID_FIREWORK_INNER_COLOR, &CFireworkView::OnFireworkChooseInnerColor)
	ON_COMMAND(
		ID_FIREWORK_BACKGROUND_COLOR,
		&CFireworkView::OnFireworkChooseBackgroundColor)
	ON_UPDATE_COMMAND_UI(
		ID_FIREWORK_INNER_COLOR,
		&CFireworkView::OnUpdateFireworkInnerColor)
	ON_COMMAND(ID_FIREWORK_FADE_SETTINGS, &CFireworkView::OnFireworkFadeSettings)
	ON_COMMAND(ID_FIREWORK_SIZE_SETTINGS, &CFireworkView::OnFireworkSizeSettings)
	ON_COMMAND_RANGE(
		ID_FIREWORK_TYPE_RADIAL,
		ID_FIREWORK_TYPE_DOUBLE_LAYER,
		&CFireworkView::OnFireworkType)
	ON_UPDATE_COMMAND_UI_RANGE(
		ID_FIREWORK_TYPE_RADIAL,
		ID_FIREWORK_TYPE_DOUBLE_LAYER,
		&CFireworkView::OnUpdateFireworkType)
	ON_COMMAND(ID_FIREWORK_CLEAR, &CFireworkView::OnFireworkClear)
	ON_UPDATE_COMMAND_UI(ID_FIREWORK_CLEAR, &CFireworkView::OnUpdateFireworkClear)
	ON_COMMAND(ID_PLAYBACK_SELECT_MUSIC, &CFireworkView::OnPlaybackSelectMusic)
	ON_COMMAND(ID_PLAYBACK_START, &CFireworkView::OnPlaybackStart)
	ON_COMMAND(ID_PLAYBACK_START_SILENT, &CFireworkView::OnPlaybackStartSilent)
	ON_UPDATE_COMMAND_UI(ID_PLAYBACK_START_SILENT, &CFireworkView::OnUpdatePlaybackStartSilent)
	ON_COMMAND(ID_EDIT_UNDO, &CFireworkView::OnEditUndo)
	ON_COMMAND(ID_EDIT_REDO, &CFireworkView::OnEditRedo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, &CFireworkView::OnUpdateEditUndo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_REDO, &CFireworkView::OnUpdateEditRedo)
	ON_COMMAND(ID_PLAYBACK_STOP, &CFireworkView::OnPlaybackStop)
	ON_UPDATE_COMMAND_UI(
		ID_PLAYBACK_SELECT_MUSIC,
		&CFireworkView::OnUpdatePlaybackSelectMusic)
	ON_UPDATE_COMMAND_UI(ID_PLAYBACK_START, &CFireworkView::OnUpdatePlaybackStart)
	ON_UPDATE_COMMAND_UI(ID_PLAYBACK_STOP, &CFireworkView::OnUpdatePlaybackStop)
	ON_UPDATE_COMMAND_UI(ID_FIREWORK_COLOR, &CFireworkView::OnUpdateEditingCommand)
	ON_UPDATE_COMMAND_UI(
		ID_FIREWORK_BACKGROUND_COLOR,
		&CFireworkView::OnUpdateEditingCommand)
	ON_UPDATE_COMMAND_UI(
		ID_FIREWORK_FADE_SETTINGS,
		&CFireworkView::OnUpdateEditingCommand)
	ON_UPDATE_COMMAND_UI(
		ID_FIREWORK_SIZE_SETTINGS,
		&CFireworkView::OnUpdateEditingCommand)
	ON_UPDATE_COMMAND_UI(ID_FILE_NEW, &CFireworkView::OnUpdateEditingCommand)
	ON_UPDATE_COMMAND_UI(ID_FILE_OPEN, &CFireworkView::OnUpdateEditingCommand)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE, &CFireworkView::OnUpdateEditingCommand)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_AS, &CFireworkView::OnUpdateEditingCommand)
	ON_WM_TIMER()
	ON_WM_ERASEBKGND()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

// CFireworkView 构造/析构

CFireworkView::CFireworkView() noexcept
	: m_currentColor(RGB(255, 180, 40))
	, m_currentInnerColor(RGB(80, 180, 255))
	, m_currentFadeDurationMs(0)
	, m_currentType(CFireworkItem::Radial)
	, m_maxRadius(kDefaultMaxFireworkRadius)
	, m_randomEngine(static_cast<std::mt19937::result_type>(
		::GetTickCount64() ^
		(static_cast<ULONGLONG>(::GetCurrentProcessId()) << 32)))
	, m_playbackRandomEngine(static_cast<std::mt19937::result_type>(
		(::GetTickCount64() >> 1) ^
		(static_cast<ULONGLONG>(::GetCurrentProcessId()) << 16) ^
		0x9E3779B9u))
	, m_fadeTimerRunning(false)
	, m_playbackTimerRunning(false)
	, m_isPlaying(false)
	, m_playbackStartTick(0)
	, m_playbackFireworkCount(0)
	, m_playbackCompletionTimeMs(0)
{
}

CFireworkView::~CFireworkView()
{
}

BOOL CFireworkView::PreCreateWindow(CREATESTRUCT& cs)
{
	return CView::PreCreateWindow(cs);
}

// CFireworkView 绘图

void CFireworkView::OnDraw(CDC* pDC)
{
	CFireworkDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (pDoc == nullptr || pDC == nullptr)
		return;

	auto drawDocumentFireworks = [pDoc](CDC* pTargetDC, COLORREF backgroundColor)
	{
		for (INT_PTR index = 0; index < pDoc->GetFireworkCount(); ++index)
		{
			const CFireworkItem* const pFirework = pDoc->GetFireworkAt(index);
			if (pFirework != nullptr)
				pFirework->Draw(pTargetDC, backgroundColor);
		}
	};
	auto drawCurrentScene = [this, pDoc, &drawDocumentFireworks](
		CDC* pTargetDC, COLORREF backgroundColor)
	{
		if (!m_isPlaying)
		{
			drawDocumentFireworks(pTargetDC, backgroundColor);
			return;
		}

		const ULONGLONG elapsedMs = ::GetTickCount64() - m_playbackStartTick;
		const INT_PTR count = pDoc->GetFireworkCount();
		for (const PlaybackEntry& entry : m_playbackPlan)
		{
			if (entry.fireworkIndex < 0 || entry.fireworkIndex >= count ||
				elapsedMs < entry.appearanceTimeMs)
				continue;

			const ULONGLONG visibleElapsedMs = elapsedMs - entry.appearanceTimeMs;
			if (visibleElapsedMs >= entry.fadeDurationMs)
				continue;

			const CFireworkItem* const pFirework =
				pDoc->GetFireworkAt(entry.fireworkIndex);
			if (pFirework != nullptr)
			{
				const double opacity = 1.0 -
					static_cast<double>(visibleElapsedMs) / entry.fadeDurationMs;
				pFirework->DrawWithOpacity(pTargetDC, backgroundColor, opacity);
			}
		}
	};

	if (pDC->IsPrinting())
	{
		drawDocumentFireworks(pDC, RGB(255, 255, 255));
		return;
	}

	CRect clientRect;
	GetClientRect(&clientRect);
	if (clientRect.IsRectEmpty())
		return;

	const COLORREF backgroundColor = pDoc->GetBackgroundColor();
	CDC bufferDC;
	CBitmap bufferBitmap;
	if (!bufferDC.CreateCompatibleDC(pDC) ||
		!bufferBitmap.CreateCompatibleBitmap(
			pDC, clientRect.Width(), clientRect.Height()))
	{
		pDC->FillSolidRect(&clientRect, backgroundColor);
		drawCurrentScene(pDC, backgroundColor);
		return;
	}

	CBitmap* const pOldBitmap = bufferDC.SelectObject(&bufferBitmap);
	bufferDC.FillSolidRect(&clientRect, backgroundColor);
	drawCurrentScene(&bufferDC, backgroundColor);
	pDC->BitBlt(
		clientRect.left, clientRect.top,
		clientRect.Width(), clientRect.Height(),
		&bufferDC, 0, 0, SRCCOPY);
	bufferDC.SelectObject(pOldBitmap);
}

void CFireworkView::OnInitialUpdate()
{
	CView::OnInitialUpdate();
	UpdateFadeTimer();
}

void CFireworkView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	CView::OnUpdate(pSender, lHint, pHint);
	const CFireworkDoc* const pDoc = GetDocument();
	if (m_isPlaying &&
		(pDoc == nullptr || pDoc->GetFireworkCount() != m_playbackFireworkCount))
	{
		FinishPlayback(false);
	}
	UpdateFadeTimer();
}

void CFireworkView::SetCurrentColor(COLORREF color) noexcept
{
	m_currentColor = color & 0x00FFFFFF;
}

void CFireworkView::SetCurrentInnerColor(COLORREF color) noexcept
{
	m_currentInnerColor = color & 0x00FFFFFF;
}

COLORREF CFireworkView::GetBackgroundColor() const noexcept
{
	const CFireworkDoc* const pDoc = GetDocument();
	return pDoc == nullptr ? RGB(255, 255, 255) : pDoc->GetBackgroundColor();
}

void CFireworkView::SetBackgroundColor(COLORREF color) noexcept
{
	CFireworkDoc* const pDoc = GetDocument();
	if (pDoc != nullptr)
		pDoc->SetBackgroundColor(color);
}

void CFireworkView::SetCurrentFadeDurationMs(DWORD durationMs) noexcept
{
	if (durationMs == 0)
	{
		m_currentFadeDurationMs = 0;
		return;
	}

	m_currentFadeDurationMs = static_cast<DWORD>(max(
		kMinFadeDurationMs, min(durationMs, kMaxFadeDurationMs)));
}

BOOL CFireworkView::SetCurrentType(int type) noexcept
{
	if (type < static_cast<int>(CFireworkItem::Radial) ||
		type > static_cast<int>(CFireworkItem::DoubleLayer))
	{
		return FALSE;
	}

	m_currentType = type;
	return TRUE;
}

BOOL CFireworkView::SetMaxRadius(int maxRadius) noexcept
{
	if (maxRadius < kMinFireworkRadius || maxRadius > kMaxFireworkRadius)
		return FALSE;

	m_maxRadius = maxRadius;
	return TRUE;
}

void CFireworkView::OnLButtonDown(UINT nFlags, CPoint point)
{
	UNREFERENCED_PARAMETER(nFlags);
	if (m_isPlaying)
		return;

	CFireworkDoc* const pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (pDoc == nullptr)
		return;

	constexpr int kMinParticleCount = 24;
	constexpr int kMaxParticleCount = 72;

	std::uniform_int_distribution<int> radiusDistribution(
		kMinFireworkRadius, m_maxRadius);
	std::uniform_int_distribution<int> particleDistribution(
		kMinParticleCount, kMaxParticleCount);
	std::uniform_int_distribution<int> seedDistribution(1, INT_MAX);

	std::unique_ptr<CFireworkItem> pFirework(new CFireworkItem(
		point,
		radiusDistribution(m_randomEngine),
		particleDistribution(m_randomEngine),
		m_currentType,
		m_currentColor,
		m_currentInnerColor,
		m_currentFadeDurationMs,
		seedDistribution(m_randomEngine)));

	if (pDoc->AddFirework(pFirework.get()))
		pFirework.release();
}

void CFireworkView::OnRButtonDown(UINT nFlags, CPoint point)
{
	UNREFERENCED_PARAMETER(nFlags);
	if (m_isPlaying)
		return;

	CFireworkDoc* const pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (pDoc == nullptr)
		return;

	// 反向检查，使重叠区域优先删除最后绘制的烟花。
	for (INT_PTR index = pDoc->GetFireworkCount(); index > 0; --index)
	{
		const CFireworkItem* const pFirework = pDoc->GetFireworkAt(index - 1);
		if (pFirework != nullptr && pFirework->HitTest(point))
		{
			pDoc->RemoveFireworkAt(index - 1);
			break;
		}
	}
}

void CFireworkView::OnFireworkChooseColor()
{
	if (m_isPlaying)
		return;
	CColorDialog dialog(m_currentColor, CC_FULLOPEN, this);
	if (dialog.DoModal() == IDOK)
		SetCurrentColor(dialog.GetColor());
}

void CFireworkView::OnFireworkChooseInnerColor()
{
	if (m_isPlaying || m_currentType != CFireworkItem::DoubleLayer)
		return;

	CColorDialog dialog(m_currentInnerColor, CC_FULLOPEN, this);
	if (dialog.DoModal() == IDOK)
		SetCurrentInnerColor(dialog.GetColor());
}

void CFireworkView::OnFireworkChooseBackgroundColor()
{
	if (m_isPlaying)
		return;
	CColorDialog dialog(GetBackgroundColor(), CC_FULLOPEN, this);
	if (dialog.DoModal() == IDOK)
		SetBackgroundColor(dialog.GetColor());
}

void CFireworkView::OnUpdateFireworkInnerColor(CCmdUI* pCmdUI)
{
	if (pCmdUI != nullptr)
		pCmdUI->Enable(!m_isPlaying && m_currentType == CFireworkItem::DoubleLayer);
}

void CFireworkView::OnFireworkFadeSettings()
{
	if (m_isPlaying)
		return;
	CFadeSettingsDialog dialog(m_currentFadeDurationMs, this);
	if (dialog.DoModal() == IDOK)
		SetCurrentFadeDurationMs(dialog.GetFadeDurationMs());
}

void CFireworkView::OnFireworkSizeSettings()
{
	if (m_isPlaying)
		return;
	CFireworkSizeSettingsDialog dialog(m_maxRadius, this);
	if (dialog.DoModal() == IDOK)
		SetMaxRadius(dialog.GetMaxRadius());
}

void CFireworkView::OnFireworkType(UINT commandId)
{
	if (m_isPlaying)
		return;
	int type = CFireworkItem::Radial;
	switch (commandId)
	{
	case ID_FIREWORK_TYPE_RING:
		type = CFireworkItem::Ring;
		break;
	case ID_FIREWORK_TYPE_DOUBLE_LAYER:
		type = CFireworkItem::DoubleLayer;
		break;
	case ID_FIREWORK_TYPE_RADIAL:
	default:
		type = CFireworkItem::Radial;
		break;
	}

	SetCurrentType(type);
}

void CFireworkView::OnUpdateFireworkType(CCmdUI* pCmdUI)
{
	if (pCmdUI == nullptr)
		return;

	UINT selectedCommand = ID_FIREWORK_TYPE_RADIAL;
	if (m_currentType == CFireworkItem::Ring)
		selectedCommand = ID_FIREWORK_TYPE_RING;
	else if (m_currentType == CFireworkItem::DoubleLayer)
		selectedCommand = ID_FIREWORK_TYPE_DOUBLE_LAYER;

	pCmdUI->SetRadio(pCmdUI->m_nID == selectedCommand);
	pCmdUI->Enable(!m_isPlaying);
}

void CFireworkView::OnFireworkClear()
{
	if (m_isPlaying)
		return;
	CFireworkDoc* const pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (pDoc != nullptr)
		pDoc->ClearFireworks();
}

void CFireworkView::OnUpdateFireworkClear(CCmdUI* pCmdUI)
{
	if (pCmdUI == nullptr)
		return;

	const CFireworkDoc* const pDoc = GetDocument();
	pCmdUI->Enable(!m_isPlaying && pDoc != nullptr && pDoc->GetFireworkCount() > 0);
}

void CFireworkView::OnPlaybackSelectMusic()
{
	if (m_isPlaying)
		return;

	CFileDialog dialog(
		TRUE,
		_T("wav"),
		nullptr,
		OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY,
		_T("WAV 音频文件 (*.wav)|*.wav||"),
		this);
	if (dialog.DoModal() != IDOK)
		return;

	CFireworkDoc* const pDoc = GetDocument();
	if (pDoc == nullptr || !pDoc->SetMusicSourcePath(dialog.GetPathName()))
	{
		AfxMessageBox(_T("无法使用所选 WAV 音乐文件。"), MB_ICONERROR | MB_OK);
	}
}

void CFireworkView::OnPlaybackStart()
{
	StartPlayback(true);
}

void CFireworkView::OnPlaybackStartSilent()
{
	StartPlayback(false);
}

void CFireworkView::StartPlayback(bool withMusic)
{
	if (m_isPlaying)
		return;

	CFireworkDoc* const pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (pDoc == nullptr)
		return;

	pDoc->RemoveExpiredFireworks();
	if (pDoc->HasActiveFadingFireworks())
	{
		AfxMessageBox(
			_T("当前仍有烟花正在淡出，请等待淡出结束后再播放。"),
			MB_ICONINFORMATION | MB_OK);
		return;
	}

	const INT_PTR count = pDoc->GetFireworkCount();
	if (count <= 0)
	{
		AfxMessageBox(_T("当前没有可播放的烟花。"), MB_ICONINFORMATION | MB_OK);
		return;
	}

	CString musicPath;
	if (withMusic)
	{
		musicPath = pDoc->GetMusicPathForPlayback();
		const DWORD musicAttributes = musicPath.IsEmpty()
			? INVALID_FILE_ATTRIBUTES
			: ::GetFileAttributes(musicPath);
		if (musicAttributes == INVALID_FILE_ATTRIBUTES ||
			(musicAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			AfxMessageBox(
				_T("尚未选择有效的 WAV 音乐，或音乐文件已经丢失。请先通过“放烟花 → 选择音乐”重新选择。"),
				MB_ICONEXCLAMATION | MB_OK);
			return;
		}
	}

	std::vector<PlaybackEntry> playbackPlan;
	ULONGLONG appearanceTimeMs = 0;
	try
	{
		playbackPlan.reserve(static_cast<size_t>(count));
		std::uniform_int_distribution<DWORD> appearanceIntervalDistribution(
			kMinPlaybackAppearanceIntervalMs,
			kMaxPlaybackAppearanceIntervalMs);
		std::uniform_int_distribution<DWORD> fadeDurationDistribution(
			kMinPlaybackFadeDurationMs,
			kMaxPlaybackFadeDurationMs);

		for (INT_PTR index = 0; index < count; ++index)
		{
			appearanceTimeMs += appearanceIntervalDistribution(m_playbackRandomEngine);
			const DWORD fadeDurationMs =
				fadeDurationDistribution(m_playbackRandomEngine);
			playbackPlan.emplace_back(index, appearanceTimeMs, fadeDurationMs);
		}
	}
	catch (const std::bad_alloc&)
	{
		AfxMessageBox(_T("内存不足，无法生成本轮烟花播放计划。"), MB_ICONERROR | MB_OK);
		return;
	}

	if (SetTimer(kPlaybackTimerId, kPlaybackTimerIntervalMs, nullptr) == 0)
	{
		AfxMessageBox(_T("无法启动烟花播放计时器。"), MB_ICONERROR | MB_OK);
		return;
	}

	if (withMusic && !::PlaySound(
		musicPath,
		nullptr,
		SND_FILENAME | SND_ASYNC | SND_LOOP | SND_NODEFAULT))
	{
		KillTimer(kPlaybackTimerId);
		AfxMessageBox(_T("WAV 音乐无法播放，请重新选择音乐文件。"), MB_ICONERROR | MB_OK);
		return;
	}

	m_playbackTimerRunning = true;
	m_isPlaying = true;
	m_playbackStartTick = ::GetTickCount64();
	m_playbackFireworkCount = count;
	m_playbackCompletionTimeMs =
		appearanceTimeMs + kPlaybackEndDelayAfterLastAppearanceMs;
	m_playbackPlan.swap(playbackPlan);
	Invalidate(FALSE);
}

void CFireworkView::OnPlaybackStop()
{
	FinishPlayback(false);
}

void CFireworkView::OnUpdatePlaybackSelectMusic(CCmdUI* pCmdUI)
{
	if (pCmdUI != nullptr)
		pCmdUI->Enable(!m_isPlaying);
}

void CFireworkView::OnUpdatePlaybackStart(CCmdUI* pCmdUI)
{
	if (pCmdUI == nullptr)
		return;

	const CFireworkDoc* const pDoc = GetDocument();
	pCmdUI->Enable(!m_isPlaying && pDoc != nullptr && pDoc->GetFireworkCount() > 0);
}

void CFireworkView::OnUpdatePlaybackStop(CCmdUI* pCmdUI)
{
	if (pCmdUI != nullptr)
		pCmdUI->Enable(m_isPlaying);
}

void CFireworkView::OnUpdatePlaybackStartSilent(CCmdUI* pCmdUI)
{
	// 独立入口无需选择音乐；空画布点击后由 StartPlayback 提示先绘制烟花。
	if (pCmdUI != nullptr)
		pCmdUI->Enable(!m_isPlaying && GetDocument() != nullptr);
}

void CFireworkView::OnUpdateEditingCommand(CCmdUI* pCmdUI)
{
	if (pCmdUI != nullptr)
		pCmdUI->Enable(!m_isPlaying);
}

void CFireworkView::OnEditUndo()
{
	if (!m_isPlaying && GetDocument() != nullptr)
		GetDocument()->Undo();
}

void CFireworkView::OnEditRedo()
{
	if (!m_isPlaying && GetDocument() != nullptr)
		GetDocument()->Redo();
}

void CFireworkView::OnUpdateEditUndo(CCmdUI* pCmdUI)
{
	if (pCmdUI != nullptr)
		pCmdUI->Enable(!m_isPlaying && GetDocument() != nullptr && GetDocument()->CanUndo());
}

void CFireworkView::OnUpdateEditRedo(CCmdUI* pCmdUI)
{
	if (pCmdUI != nullptr)
		pCmdUI->Enable(!m_isPlaying && GetDocument() != nullptr && GetDocument()->CanRedo());
}

void CFireworkView::FinishPlayback(bool showCompletionMessage)
{
	if (!m_isPlaying && !m_playbackTimerRunning)
		return;

	if (m_playbackTimerRunning && GetSafeHwnd() != nullptr)
		KillTimer(kPlaybackTimerId);
	m_playbackTimerRunning = false;
	m_isPlaying = false;
	m_playbackStartTick = 0;
	m_playbackFireworkCount = 0;
	m_playbackCompletionTimeMs = 0;
	m_playbackPlan.clear();
	::PlaySound(nullptr, nullptr, 0);

	if (GetSafeHwnd() != nullptr)
	{
		Invalidate(FALSE);
		UpdateWindow();
	}
	if (showCompletionMessage)
		AfxMessageBox(_T("烟花播放完成。"), MB_ICONINFORMATION | MB_OK);
}

void CFireworkView::UpdateFadeTimer()
{
	if (GetSafeHwnd() == nullptr)
	{
		m_fadeTimerRunning = false;
		return;
	}

	const CFireworkDoc* const pDoc = GetDocument();
	const bool needsTimer =
		pDoc != nullptr && pDoc->HasActiveFadingFireworks() != FALSE;

	if (needsTimer && !m_fadeTimerRunning)
	{
		m_fadeTimerRunning =
			SetTimer(kFadeTimerId, kFadeTimerIntervalMs, nullptr) != 0;
	}
	else if (!needsTimer && m_fadeTimerRunning)
	{
		KillTimer(kFadeTimerId);
		m_fadeTimerRunning = false;
	}
}

void CFireworkView::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == kPlaybackTimerId)
	{
		if (!m_isPlaying)
		{
			if (m_playbackTimerRunning)
				KillTimer(kPlaybackTimerId);
			m_playbackTimerRunning = false;
			return;
		}

		const ULONGLONG elapsedMs = ::GetTickCount64() - m_playbackStartTick;
		if (elapsedMs >= m_playbackCompletionTimeMs)
		{
			FinishPlayback(true);
			return;
		}

		Invalidate(FALSE);
		return;
	}

	if (nIDEvent == kFadeTimerId)
	{
		CFireworkDoc* const pDoc = GetDocument();
		if (pDoc != nullptr)
		{
			pDoc->RemoveExpiredFireworks();
			if (pDoc->HasActiveFadingFireworks())
				Invalidate(FALSE);
			else
				UpdateFadeTimer();
		}
		return;
	}

	CView::OnTimer(nIDEvent);
}

BOOL CFireworkView::OnEraseBkgnd(CDC* pDC)
{
	UNREFERENCED_PARAMETER(pDC);
	return TRUE;
}

void CFireworkView::OnDestroy()
{
	if (m_playbackTimerRunning)
	{
		KillTimer(kPlaybackTimerId);
		m_playbackTimerRunning = false;
	}
	if (m_isPlaying)
	{
		m_isPlaying = false;
		m_playbackStartTick = 0;
		m_playbackFireworkCount = 0;
		m_playbackCompletionTimeMs = 0;
		m_playbackPlan.clear();
		::PlaySound(nullptr, nullptr, 0);
	}

	if (m_fadeTimerRunning)
	{
		KillTimer(kFadeTimerId);
		m_fadeTimerRunning = false;
	}

	CView::OnDestroy();
}


// CFireworkView 诊断

#ifdef _DEBUG
void CFireworkView::AssertValid() const
{
	CView::AssertValid();
}

void CFireworkView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CFireworkDoc* CFireworkView::GetDocument() const // 非调试版本是内联的
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CFireworkDoc)));
	return (CFireworkDoc*)m_pDocument;
}
#endif //_DEBUG


// CFireworkView 消息处理程序
