
// FireworkDoc.h: CFireworkDoc 类的接口
//


#pragma once

#include <afxtempl.h>
#include <memory>
#include <vector>

class CFireworkItem;

class CFireworkDoc : public CDocument
{
protected: // 仅从序列化创建
	CFireworkDoc() noexcept;
	DECLARE_DYNCREATE(CFireworkDoc)

// 特性
public:
	INT_PTR GetFireworkCount() const noexcept;
	CFireworkItem* GetFireworkAt(INT_PTR index);
	const CFireworkItem* GetFireworkAt(INT_PTR index) const;
	COLORREF GetBackgroundColor() const noexcept { return m_backgroundColor; }
	CString GetMusicPathForPlayback() const;

// 操作
public:
	// 文档拥有成功加入集合的对象，并负责在删除、清空或关闭时释放它们。
	BOOL AddFirework(CFireworkItem* pFirework);
	BOOL RemoveFireworkAt(INT_PTR index);
	BOOL RemoveExpiredFireworks();
	BOOL HasActiveFadingFireworks() const;
	void ClearFireworks();
	void SetBackgroundColor(COLORREF color);
	BOOL SetMusicSourcePath(const CString& path);
	BOOL CanUndo() const;
	BOOL CanRedo() const;
	void Undo();
	void Redo();

// 重写
public:
	virtual BOOL OnNewDocument();
	virtual BOOL OnSaveDocument(LPCTSTR lpszPathName);
	virtual void Serialize(CArchive& ar);
	virtual void DeleteContents();
#ifdef SHARED_HANDLERS
	virtual void InitializeSearchContent();
	virtual void OnDrawThumbnail(CDC& dc, LPRECT lprcBounds);
#endif // SHARED_HANDLERS

// 实现
public:
	virtual ~CFireworkDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	CArray<CFireworkItem*, CFireworkItem*> m_fireworks;
	COLORREF m_backgroundColor;
	CString m_musicRelativePath;
	CString m_musicSourcePath;
	using EditState = std::vector<std::shared_ptr<CFireworkItem>>;
	std::vector<EditState> m_undoHistory;
	std::vector<EditState> m_redoHistory;
	ULONGLONG m_nextEditOrder = 0;
	EditState CaptureEditState() const;
	void RecordEdit();
	BOOL CanRestoreEditState(const EditState& state) const;
	void RestoreEditState(const EditState& state);

	void DeleteAllFireworks() noexcept;

// 生成的消息映射函数
protected:
	DECLARE_MESSAGE_MAP()

#ifdef SHARED_HANDLERS
	// 用于为搜索处理程序设置搜索内容的 Helper 函数
	void SetSearchContent(const CString& value);
#endif // SHARED_HANDLERS
};
