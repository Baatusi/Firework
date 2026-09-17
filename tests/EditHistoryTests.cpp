#include "pch.h"
#include "FireworkDoc.h"
#include "FireworkItem.h"
#include <iostream>
#include <stdexcept>

class TestDoc : public CFireworkDoc {};
class ExpiredItem : public CFireworkItem
{
public:
	ExpiredItem() : CFireworkItem(CPoint(0, 0), 40, 24, 0, RGB(255, 0, 0), 0, 1000, 9)
	{
		m_nFadeElapsedMs = 1000;
	}
};

void Check(bool condition, const char* message)
{
	if (!condition) throw std::runtime_error(message);
}

CFireworkItem* Add(TestDoc& doc, int seed, DWORD fade = 0)
{
	auto item = std::make_unique<CFireworkItem>(CPoint(seed, 20), 80, 24,
		CFireworkItem::DoubleLayer, RGB(255, 0, 0), RGB(0, 0, 255), fade, seed);
	auto* result = item.get();
	Check(doc.AddFirework(item.get()) != FALSE, "add failed");
	item.release();
	return result;
}

int main()
{
	if (!AfxWinInit(GetModuleHandle(nullptr), nullptr, GetCommandLine(), 0)) return 2;
	try
	{
		TestDoc doc;
		Check(!doc.CanUndo() && !doc.CanRedo(), "empty history");
		Add(doc, 1);
		doc.Undo();
		Check(doc.GetFireworkCount() == 0 && doc.CanRedo(), "undo drawing");
		doc.Redo();
		Check(doc.GetFireworkCount() == 1 && doc.GetFireworkAt(0)->GetSeed() == 1 &&
			doc.GetFireworkAt(0)->GetInnerColor() == RGB(0, 0, 255), "redo exact appearance");
		auto* fading = Add(doc, 2, 10000);
		Add(doc, 3);
		doc.Undo();
		Check(doc.GetFireworkCount() == 2 && doc.GetFireworkAt(1) == fading, "preserve fading instance");
		doc.Undo();
		Check(doc.GetFireworkCount() == 1 && doc.GetFireworkAt(0) == fading, "timed drawing is not undoable");
		doc.Redo();
		doc.Redo();
		Check(doc.GetFireworkAt(0)->GetSeed() == 1 && doc.GetFireworkAt(1) == fading &&
			doc.GetFireworkAt(2)->GetSeed() == 3, "preserve overlap order");
		doc.RemoveFireworkAt(0);
		doc.Undo();
		Check(doc.GetFireworkAt(0)->GetSeed() == 1, "undo right-click deletion");
		doc.ClearFireworks();
		doc.Undo();
		Check(doc.GetFireworkCount() == 2 && doc.GetFireworkAt(1)->GetSeed() == 3,
			"undo clear restores only permanent fireworks");
		doc.Redo();
		Check(doc.GetFireworkCount() == 0, "redo clear");
		doc.Undo();
		Add(doc, 4);
		Check(!doc.CanRedo(), "new drawing invalidates redo");
		doc.DeleteContents();
		Check(!doc.CanUndo() && !doc.CanRedo(), "new document resets history");
		Add(doc, 5, 10000);
		Check(!doc.CanUndo(), "timed-only drawing has no undo");
		doc.DeleteContents();
		Add(doc, 6);
		auto expired = std::make_unique<ExpiredItem>();
		Check(doc.AddFirework(expired.get()) != FALSE, "add expired");
		expired.release();
		Check(doc.RemoveExpiredFireworks() != FALSE, "expire timed firework");
		doc.Undo();
		doc.Redo();
		Check(doc.GetFireworkCount() == 1 && doc.GetFireworkAt(0)->GetSeed() == 6,
			"expired fireworks never resurrect");
		CMemFile file;
		{ CArchive archive(&file, CArchive::store); doc.Serialize(archive); archive.Close(); }
		file.SeekToBegin();
		{ CArchive archive(&file, CArchive::load); doc.Serialize(archive); archive.Close(); }
		Check(!doc.CanUndo() && !doc.CanRedo() && doc.GetFireworkCount() == 1, "load resets history");
		Add(doc, 7);
		doc.Undo();
		Check(doc.GetFireworkCount() == 1 && doc.GetFireworkAt(0)->GetSeed() == 6, "edit loaded document");
		std::cout << "PASS: edit history, timed fade isolation, overlap order, reset and serialization\n";
		return 0;
	}
	catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
