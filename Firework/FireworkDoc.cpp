
// FireworkDoc.cpp: CFireworkDoc 类的实现
//

#include "pch.h"
#include "framework.h"
// SHARED_HANDLERS 可以在实现预览、缩略图和搜索筛选器句柄的
// ATL 项目中进行定义，并允许与该项目共享文档代码。
#ifndef SHARED_HANDLERS
#include "Firework.h"
#endif

#include "FireworkItem.h"
#include "FireworkDoc.h"

#include <propkey.h>
#include <algorithm>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
	constexpr DWORD kDocumentMagic = 0x314B5746; // "FWK1"
	constexpr DWORD kDocumentFormatVersion = 2;
	constexpr DWORD kMaxFireworkCount = 1000;
	constexpr ULONGLONG kMaxTotalParticleCount = 100000;
	constexpr COLORREF kDefaultBackgroundColor = RGB(255, 255, 255);
	constexpr int kMaxMusicRelativePathLength = 1024;

	CString GetDirectoryName(const CString& path)
	{
		const int separator = max(path.ReverseFind(_T('\\')), path.ReverseFind(_T('/')));
		return separator < 0 ? CString() : path.Left(separator);
	}

	CString GetFileStem(const CString& path)
	{
		const int separator = max(path.ReverseFind(_T('\\')), path.ReverseFind(_T('/')));
		CString fileName = separator < 0 ? path : path.Mid(separator + 1);
		const int extension = fileName.ReverseFind(_T('.'));
		if (extension > 0)
			fileName = fileName.Left(extension);
		return fileName;
	}

	bool IsWaveFilePath(const CString& path)
	{
		const int extension = path.ReverseFind(_T('.'));
		return extension >= 0 &&
			path.Mid(extension).CompareNoCase(_T(".wav")) == 0;
	}

	bool IsExistingFile(const CString& path)
	{
		if (path.IsEmpty())
			return false;

		const DWORD attributes = ::GetFileAttributes(path);
		return attributes != INVALID_FILE_ATTRIBUTES &&
			(attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
	}

	bool IsSafeRelativeMusicPath(const CString& path)
	{
		if (path.IsEmpty())
			return true;
		if (path.GetLength() > kMaxMusicRelativePathLength ||
			path[0] == _T('\\') || path[0] == _T('/') ||
			path.Find(_T(':')) >= 0 || path.Find(_T("..")) >= 0 ||
			!IsWaveFilePath(path))
		{
			return false;
		}
		return true;
	}

	CString CombinePath(const CString& directory, const CString& relativePath)
	{
		if (directory.IsEmpty())
			return relativePath;
		if (directory.Right(1) == _T("\\") || directory.Right(1) == _T("/"))
			return directory + relativePath;
		return directory + _T("\\") + relativePath;
	}

	std::shared_ptr<CFireworkItem> ClonePermanentFirework(const CFireworkItem& item)
	{
		auto copy = std::make_shared<CFireworkItem>(item.GetCenter(), item.GetRadius(),
			item.GetParticleCount(), item.GetType(), item.GetColor(),
			item.GetInnerColor(), 0, item.GetSeed());
		copy->SetEditOrder(item.GetEditOrder());
		return copy;
	}
}

// CFireworkDoc

IMPLEMENT_DYNCREATE(CFireworkDoc, CDocument)

BEGIN_MESSAGE_MAP(CFireworkDoc, CDocument)
END_MESSAGE_MAP()


// CFireworkDoc 构造/析构

CFireworkDoc::CFireworkDoc() noexcept
	: m_backgroundColor(kDefaultBackgroundColor)
{
}

CFireworkDoc::~CFireworkDoc()
{
	DeleteAllFireworks();
}

BOOL CFireworkDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	// CDocument::OnNewDocument() 会调用 DeleteContents()；SDI 重用文档时，
	// 此处应当已经得到一个空的烟花集合。
	ASSERT(m_fireworks.IsEmpty());

	return TRUE;
}

CString CFireworkDoc::GetMusicPathForPlayback() const
{
	if (!m_musicSourcePath.IsEmpty())
		return m_musicSourcePath;
	if (m_musicRelativePath.IsEmpty() || !IsSafeRelativeMusicPath(m_musicRelativePath))
		return CString();

	const CString documentDirectory = GetDirectoryName(GetPathName());
	if (documentDirectory.IsEmpty())
		return CString();
	return CombinePath(documentDirectory, m_musicRelativePath);
}

INT_PTR CFireworkDoc::GetFireworkCount() const noexcept
{
	return m_fireworks.GetSize();
}

CFireworkItem* CFireworkDoc::GetFireworkAt(INT_PTR index)
{
	if (index < 0 || index >= m_fireworks.GetSize())
	{
		ASSERT(FALSE);
		return nullptr;
	}

	return m_fireworks.GetAt(index);
}

const CFireworkItem* CFireworkDoc::GetFireworkAt(INT_PTR index) const
{
	if (index < 0 || index >= m_fireworks.GetSize())
	{
		ASSERT(FALSE);
		return nullptr;
	}

	return m_fireworks.GetAt(index);
}

BOOL CFireworkDoc::AddFirework(CFireworkItem* pFirework)
{
	if (pFirework == nullptr)
	{
		ASSERT(FALSE);
		return FALSE;
	}

	if (m_fireworks.GetSize() >= kMaxFireworkCount)
		return FALSE;

	ULONGLONG totalParticleCount = 0;

	// 同一指针不得由文档重复持有，否则清理集合时会发生重复释放。
	for (INT_PTR index = 0; index < m_fireworks.GetSize(); ++index)
	{
		const CFireworkItem* const pExisting = m_fireworks.GetAt(index);
		if (pExisting == pFirework)
		{
			ASSERT(FALSE);
			return FALSE;
		}

		if (pExisting != nullptr)
			totalParticleCount += static_cast<ULONGLONG>(pExisting->GetParticleCount());
	}

	totalParticleCount += static_cast<ULONGLONG>(pFirework->GetParticleCount());
	if (totalParticleCount > kMaxTotalParticleCount)
		return FALSE;

	// 先预留空间，避免记录成功后插入失败。
	m_fireworks.SetSize(m_fireworks.GetSize() + 1);
	m_fireworks.RemoveAt(m_fireworks.GetSize() - 1);
	if (pFirework->GetFadeDurationMs() == 0)
		RecordEdit();
	else
		m_redoHistory.clear();
	pFirework->SetEditOrder(m_nextEditOrder++);
	m_fireworks.Add(pFirework);
	SetModifiedFlag(TRUE);
	UpdateAllViews(nullptr);
	return TRUE;
}

BOOL CFireworkDoc::RemoveFireworkAt(INT_PTR index)
{
	if (index < 0 || index >= m_fireworks.GetSize())
		return FALSE;

	CFireworkItem* const pFirework = m_fireworks.GetAt(index);
	if (pFirework->GetFadeDurationMs() == 0)
		RecordEdit();
	else
		m_redoHistory.clear();
	m_fireworks.RemoveAt(index);
	delete pFirework;

	SetModifiedFlag(TRUE);
	UpdateAllViews(nullptr);
	return TRUE;
}

BOOL CFireworkDoc::RemoveExpiredFireworks()
{
	BOOL removedAny = FALSE;
	for (INT_PTR index = m_fireworks.GetSize(); index > 0; --index)
	{
		CFireworkItem* const pFirework = m_fireworks.GetAt(index - 1);
		if (pFirework != nullptr && pFirework->IsFadeComplete())
		{
			m_fireworks.RemoveAt(index - 1);
			delete pFirework;
			removedAny = TRUE;
		}
	}

	if (removedAny)
	{
		SetModifiedFlag(TRUE);
		UpdateAllViews(nullptr);
	}
	return removedAny;
}

BOOL CFireworkDoc::HasActiveFadingFireworks() const
{
	for (INT_PTR index = 0; index < m_fireworks.GetSize(); ++index)
	{
		const CFireworkItem* const pFirework = m_fireworks.GetAt(index);
		// 已到期但尚未来得及清理的对象也需要启动一次计时器，
		// 以便载入文档后在第一个计时周期将其移除。
		if (pFirework != nullptr && pFirework->GetFadeDurationMs() != 0)
			return TRUE;
	}
	return FALSE;
}

void CFireworkDoc::ClearFireworks()
{
	if (m_fireworks.IsEmpty())
		return;

	if (!CaptureEditState().empty())
		RecordEdit();
	else
		m_redoHistory.clear();
	for (INT_PTR index = 0; index < m_fireworks.GetSize(); ++index)
		delete m_fireworks.GetAt(index);
	m_fireworks.RemoveAll();
	SetModifiedFlag(TRUE);
	UpdateAllViews(nullptr);
}

CFireworkDoc::EditState CFireworkDoc::CaptureEditState() const
{
	EditState state;
	for (INT_PTR index = 0; index < m_fireworks.GetSize(); ++index)
	{
		const auto* item = m_fireworks.GetAt(index);
		if (item->GetFadeDurationMs() == 0)
			state.push_back(ClonePermanentFirework(*item));
	}
	return state;
}

void CFireworkDoc::RecordEdit()
{
	m_undoHistory.push_back(CaptureEditState());
	if (m_undoHistory.size() > 100)
		m_undoHistory.erase(m_undoHistory.begin());
	m_redoHistory.clear();
}

BOOL CFireworkDoc::CanRestoreEditState(const EditState& state) const
{
	size_t count = state.size();
	ULONGLONG particles = 0;
	for (const auto& item : state)
		particles += item->GetParticleCount();
	for (INT_PTR index = 0; index < m_fireworks.GetSize(); ++index)
	{
		const auto* item = m_fireworks.GetAt(index);
		if (item->GetFadeDurationMs() != 0)
		{
			++count;
			particles += item->GetParticleCount();
		}
	}
	return count <= kMaxFireworkCount && particles <= kMaxTotalParticleCount;
}

BOOL CFireworkDoc::CanUndo() const
{
	return !m_undoHistory.empty() && CanRestoreEditState(m_undoHistory.back());
}

BOOL CFireworkDoc::CanRedo() const
{
	return !m_redoHistory.empty() && CanRestoreEditState(m_redoHistory.back());
}

void CFireworkDoc::RestoreEditState(const EditState& state)
{
	// 定时消散对象保留原实例和计时起点，绝不通过历史记录复活。
	std::vector<std::unique_ptr<CFireworkItem>> copies;
	std::vector<CFireworkItem*> restored;
	for (const auto& item : state)
	{
		auto copy = std::make_unique<CFireworkItem>(item->GetCenter(), item->GetRadius(),
			item->GetParticleCount(), item->GetType(), item->GetColor(),
			item->GetInnerColor(), 0, item->GetSeed());
		copy->SetEditOrder(item->GetEditOrder());
		restored.push_back(copy.get());
		copies.push_back(std::move(copy));
	}
	for (INT_PTR index = 0; index < m_fireworks.GetSize(); ++index)
		if (m_fireworks.GetAt(index)->GetFadeDurationMs() != 0)
			restored.push_back(m_fireworks.GetAt(index));
	std::sort(restored.begin(), restored.end(), [](const auto* left, const auto* right) {
		return left->GetEditOrder() < right->GetEditOrder();
	});
	const INT_PTR oldCount = m_fireworks.GetSize();
	if (restored.size() > static_cast<size_t>(oldCount))
		m_fireworks.SetSize(static_cast<INT_PTR>(restored.size()));
	for (INT_PTR index = 0; index < oldCount; ++index)
		if (m_fireworks.GetAt(index)->GetFadeDurationMs() == 0)
			delete m_fireworks.GetAt(index);
	m_fireworks.SetSize(static_cast<INT_PTR>(restored.size()));
	for (INT_PTR index = 0; index < m_fireworks.GetSize(); ++index)
		m_fireworks.SetAt(index, restored[static_cast<size_t>(index)]);
	for (auto& copy : copies)
		copy.release();
}

void CFireworkDoc::Undo()
{
	if (!CanUndo())
		return;
	m_redoHistory.push_back(CaptureEditState());
	try { RestoreEditState(m_undoHistory.back()); }
	catch (...) { m_redoHistory.pop_back(); throw; }
	m_undoHistory.pop_back();
	SetModifiedFlag(TRUE);
	UpdateAllViews(nullptr);
}

void CFireworkDoc::Redo()
{
	if (!CanRedo())
		return;
	m_undoHistory.push_back(CaptureEditState());
	try { RestoreEditState(m_redoHistory.back()); }
	catch (...) { m_undoHistory.pop_back(); throw; }
	m_redoHistory.pop_back();
	SetModifiedFlag(TRUE);
	UpdateAllViews(nullptr);
}

void CFireworkDoc::SetBackgroundColor(COLORREF color)
{
	const COLORREF normalizedColor = color & 0x00FFFFFF;
	if (m_backgroundColor == normalizedColor)
		return;

	m_backgroundColor = normalizedColor;
	SetModifiedFlag(TRUE);
	UpdateAllViews(nullptr);
}

BOOL CFireworkDoc::SetMusicSourcePath(const CString& path)
{
	if (!IsWaveFilePath(path) || !IsExistingFile(path))
		return FALSE;

	if (GetMusicPathForPlayback().CompareNoCase(path) == 0)
		return TRUE;

	m_musicSourcePath = path;
	m_musicRelativePath.Empty();
	SetModifiedFlag(TRUE);
	return TRUE;
}

void CFireworkDoc::DeleteAllFireworks() noexcept
{
	m_undoHistory.clear();
	m_redoHistory.clear();
	m_nextEditOrder = 0;
	for (INT_PTR index = m_fireworks.GetSize(); index > 0; --index)
		delete m_fireworks.GetAt(index - 1);

	m_fireworks.RemoveAll();
}

void CFireworkDoc::DeleteContents()
{
	DeleteAllFireworks();
	m_backgroundColor = kDefaultBackgroundColor;
	m_musicRelativePath.Empty();
	m_musicSourcePath.Empty();
	CDocument::DeleteContents();
}

BOOL CFireworkDoc::OnSaveDocument(LPCTSTR lpszPathName)
{
	if (lpszPathName == nullptr || *lpszPathName == _T('\0'))
		return FALSE;

	const CString oldMusicRelativePath = m_musicRelativePath;
	const CString oldMusicSourcePath = m_musicSourcePath;
	if (!m_musicRelativePath.IsEmpty() || !m_musicSourcePath.IsEmpty())
	{
		const CString sourcePath = GetMusicPathForPlayback();
		if (!IsWaveFilePath(sourcePath) || !IsExistingFile(sourcePath))
		{
			AfxMessageBox(
				_T("音乐文件不存在或不是有效的 WAV 文件，文档未保存。请重新选择音乐。"),
				MB_ICONEXCLAMATION | MB_OK);
			return FALSE;
		}

		const CString documentPath(lpszPathName);
		const CString documentDirectory = GetDirectoryName(documentPath);
		const CString documentStem = GetFileStem(documentPath);
		if (documentDirectory.IsEmpty() || documentStem.IsEmpty())
		{
			AfxMessageBox(_T("无法确定文档的音乐资源目录。"), MB_ICONERROR | MB_OK);
			return FALSE;
		}

		const CString assetFolderName = documentStem + _T(".assets");
		const CString assetDirectory = CombinePath(documentDirectory, assetFolderName);
		const DWORD assetAttributes = ::GetFileAttributes(assetDirectory);
		if (assetAttributes == INVALID_FILE_ATTRIBUTES)
		{
			if (!::CreateDirectory(assetDirectory, nullptr))
			{
				AfxMessageBox(_T("无法创建音乐资源目录，文档未保存。"), MB_ICONERROR | MB_OK);
				return FALSE;
			}
		}
		else if ((assetAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
		{
			AfxMessageBox(_T("音乐资源目录名称被同名文件占用，文档未保存。"), MB_ICONERROR | MB_OK);
			return FALSE;
		}

		const CString relativePath = assetFolderName + _T("\\music.wav");
		const CString destinationPath = CombinePath(documentDirectory, relativePath);
		if (sourcePath.CompareNoCase(destinationPath) != 0 &&
			!::CopyFile(sourcePath, destinationPath, FALSE))
		{
			AfxMessageBox(_T("无法复制 WAV 音乐文件，文档未保存。"), MB_ICONERROR | MB_OK);
			return FALSE;
		}

		m_musicRelativePath = relativePath;
		m_musicSourcePath = destinationPath;
	}

	if (CDocument::OnSaveDocument(lpszPathName))
		return TRUE;

	m_musicRelativePath = oldMusicRelativePath;
	m_musicSourcePath = oldMusicSourcePath;
	return FALSE;
}




// CFireworkDoc 序列化

void CFireworkDoc::Serialize(CArchive& ar)
{
	CDocument::Serialize(ar);

	if (ar.IsStoring())
	{
		const INT_PTR count = m_fireworks.GetSize();
		if (count < 0 || static_cast<ULONGLONG>(count) > kMaxFireworkCount)
			AfxThrowArchiveException(CArchiveException::badIndex);

		ULONGLONG totalParticleCount = 0;
		for (INT_PTR index = 0; index < count; ++index)
		{
			const CFireworkItem* const pFirework = m_fireworks.GetAt(index);
			if (pFirework == nullptr)
				AfxThrowArchiveException(CArchiveException::badClass);

			totalParticleCount += static_cast<ULONGLONG>(pFirework->GetParticleCount());
			if (totalParticleCount > kMaxTotalParticleCount)
				AfxThrowArchiveException(CArchiveException::badIndex);
		}

		ar << kDocumentMagic;
		ar << kDocumentFormatVersion;
		ar << static_cast<DWORD>(m_backgroundColor);
		ar << m_musicRelativePath;
		ar << static_cast<DWORD>(count);
		for (INT_PTR index = 0; index < count; ++index)
		{
			CFireworkItem* const pFirework = m_fireworks.GetAt(index);
			if (pFirework == nullptr)
				AfxThrowArchiveException(CArchiveException::badClass);

			// 通过 CObject 指针写入运行时类和对象架构信息；对象自身负责属性序列化。
			ar << static_cast<CObject*>(pFirework);
		}
	}
	else
	{
		DeleteAllFireworks();
		m_backgroundColor = kDefaultBackgroundColor;
		m_musicRelativePath.Empty();
		m_musicSourcePath.Empty();

		try
		{
			DWORD firstValue = 0;
			ar >> firstValue;

			DWORD count = firstValue;
			if (firstValue == kDocumentMagic)
			{
				DWORD formatVersion = 0;
				DWORD backgroundColor = 0;
				ar >> formatVersion;
				if (formatVersion < 1 || formatVersion > kDocumentFormatVersion)
					AfxThrowArchiveException(CArchiveException::badSchema);

				ar >> backgroundColor;
				m_backgroundColor = backgroundColor & 0x00FFFFFF;
				if (formatVersion >= 2)
				{
					ar >> m_musicRelativePath;
					if (!IsSafeRelativeMusicPath(m_musicRelativePath))
						AfxThrowArchiveException(CArchiveException::badSchema);
				}
				ar >> count;
			}

			if (count > kMaxFireworkCount)
				AfxThrowArchiveException(CArchiveException::badIndex);

			ULONGLONG totalParticleCount = 0;
			for (DWORD index = 0; index < count; ++index)
			{
				CObject* pObject = nullptr;
				ar >> pObject;

				CFireworkItem* const pFirework = DYNAMIC_DOWNCAST(CFireworkItem, pObject);
				if (pFirework == nullptr)
				{
					delete pObject;
					AfxThrowArchiveException(CArchiveException::badClass);
				}

				bool duplicatePointer = false;
				for (INT_PTR existingIndex = 0;
					existingIndex < m_fireworks.GetSize(); ++existingIndex)
				{
					if (m_fireworks.GetAt(existingIndex) == pFirework)
					{
						duplicatePointer = true;
						break;
					}
				}

				if (duplicatePointer)
					AfxThrowArchiveException(CArchiveException::badClass);

				totalParticleCount += static_cast<ULONGLONG>(pFirework->GetParticleCount());
				if (totalParticleCount > kMaxTotalParticleCount)
				{
					delete pFirework;
					AfxThrowArchiveException(CArchiveException::badIndex);
				}

				try
				{
					m_fireworks.Add(pFirework);
					pFirework->SetEditOrder(m_nextEditOrder++);
				}
				catch (...)
				{
					delete pFirework;
					throw;
				}
			}
		}
		catch (...)
		{
			// 文件截断、类型不匹配或分配失败时不保留半载入状态。
			DeleteAllFireworks();
			m_backgroundColor = kDefaultBackgroundColor;
			m_musicRelativePath.Empty();
			m_musicSourcePath.Empty();
			throw;
		}
	}
}

#ifdef SHARED_HANDLERS

// 缩略图的支持
void CFireworkDoc::OnDrawThumbnail(CDC& dc, LPRECT lprcBounds)
{
	// 修改此代码以绘制文档数据
	dc.FillSolidRect(lprcBounds, RGB(255, 255, 255));

	CString strText = _T("TODO: implement thumbnail drawing here");
	LOGFONT lf;

	CFont* pDefaultGUIFont = CFont::FromHandle((HFONT) GetStockObject(DEFAULT_GUI_FONT));
	pDefaultGUIFont->GetLogFont(&lf);
	lf.lfHeight = 36;

	CFont fontDraw;
	fontDraw.CreateFontIndirect(&lf);

	CFont* pOldFont = dc.SelectObject(&fontDraw);
	dc.DrawText(strText, lprcBounds, DT_CENTER | DT_WORDBREAK);
	dc.SelectObject(pOldFont);
}

// 搜索处理程序的支持
void CFireworkDoc::InitializeSearchContent()
{
	CString strSearchContent;
	// 从文档数据设置搜索内容。
	// 内容部分应由“;”分隔

	// 例如:     strSearchContent = _T("point;rectangle;circle;ole object;")；
	SetSearchContent(strSearchContent);
}

void CFireworkDoc::SetSearchContent(const CString& value)
{
	if (value.IsEmpty())
	{
		RemoveChunk(PKEY_Search_Contents.fmtid, PKEY_Search_Contents.pid);
	}
	else
	{
		CMFCFilterChunkValueImpl *pChunk = nullptr;
		ATLTRY(pChunk = new CMFCFilterChunkValueImpl);
		if (pChunk != nullptr)
		{
			pChunk->SetTextValue(PKEY_Search_Contents, value, CHUNK_TEXT);
			SetChunkValue(pChunk);
		}
	}
}

#endif // SHARED_HANDLERS

// CFireworkDoc 诊断

#ifdef _DEBUG
void CFireworkDoc::AssertValid() const
{
	CDocument::AssertValid();

	for (INT_PTR index = 0; index < m_fireworks.GetSize(); ++index)
	{
		CFireworkItem* const pFirework = m_fireworks.GetAt(index);
		ASSERT(pFirework != nullptr);
		ASSERT_VALID(pFirework);
	}
}

void CFireworkDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
	dc << _T("\n烟花数量 = ") << m_fireworks.GetSize();
}
#endif //_DEBUG


// CFireworkDoc 命令
