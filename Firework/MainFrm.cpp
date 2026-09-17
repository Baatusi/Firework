
// MainFrm.cpp: CMainFrame 类的实现
//

#include "pch.h"
#include "framework.h"
#include "Firework.h"

#include "MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CMainFrame

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
	ON_WM_CREATE()
END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,           // 状态行指示器
	ID_INDICATOR_CAPS,
	ID_INDICATOR_NUM,
	ID_INDICATOR_SCRL,
};

// CMainFrame 构造/析构

CMainFrame::CMainFrame() noexcept
{
	// TODO: 在此添加成员初始化代码
}

CMainFrame::~CMainFrame()
{
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	if (!m_wndToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC) ||
		!m_wndToolBar.LoadToolBar(IDR_MAINFRAME))
	{
		TRACE0("未能创建工具栏\n");
		return -1;      // 未能创建
	}

	if (!m_wndStatusBar.Create(this))
	{
		TRACE0("未能创建状态栏\n");
		return -1;      // 未能创建
	}
	m_wndStatusBar.SetIndicators(indicators, sizeof(indicators)/sizeof(UINT));

	CMenu playbackMenu;
	CMenu* const pMainMenu = GetMenu();
	if (pMainMenu == nullptr || !playbackMenu.CreatePopupMenu() ||
		!playbackMenu.AppendMenu(MF_STRING, ID_PLAYBACK_SELECT_MUSIC, _T("选择音乐(&M)...")) ||
		!playbackMenu.AppendMenu(MF_SEPARATOR) ||
		!playbackMenu.AppendMenu(MF_STRING, ID_PLAYBACK_START, _T("播放(&P)")) ||
		!playbackMenu.AppendMenu(MF_STRING, ID_PLAYBACK_START_SILENT, _T("无音乐播放(&N)")) ||
		!playbackMenu.AppendMenu(MF_STRING, ID_PLAYBACK_STOP, _T("停止(&S)")))
	{
		TRACE0("未能创建放烟花菜单\n");
		return -1;
	}

	const int menuCount = pMainMenu->GetMenuItemCount();
	const int insertPosition = menuCount > 0 ? menuCount - 1 : 0;
	if (!pMainMenu->InsertMenu(
		insertPosition,
		MF_BYPOSITION | MF_POPUP,
		reinterpret_cast<UINT_PTR>(playbackMenu.GetSafeHmenu()),
		_T("放烟花(&P)")))
	{
		TRACE0("未能把放烟花菜单加入主菜单栏\n");
		return -1;
	}
	playbackMenu.Detach();
	DrawMenuBar();

	// TODO: 如果不需要可停靠工具栏，则删除这三行
	m_wndToolBar.EnableDocking(CBRS_ALIGN_ANY);
	EnableDocking(CBRS_ALIGN_ANY);
	DockControlBar(&m_wndToolBar);


	return 0;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CFrameWnd::PreCreateWindow(cs) )
		return FALSE;
	// TODO: 在此处通过修改
	//  CREATESTRUCT cs 来修改窗口类或样式

	return TRUE;
}

// CMainFrame 诊断

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CFrameWnd::Dump(dc);
}
#endif //_DEBUG


// CMainFrame 消息处理程序

