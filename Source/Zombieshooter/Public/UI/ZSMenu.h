#pragma once

#include "CoreMinimal.h"

/**
 * Controller-first list menus (bunker stations, pause menu). A page is a title plus selectable items; selecting
 * an item runs its callback (which may push another page). The player controller owns the page stack and the input;
 * AZSHUD draws the top page. Replaced by CommonUI screens in the UI art pass (Milestone 14) without changing callers.
 */
struct FZSMenuItem
{
	FText Label;
	/** Right-aligned value, e.g. a price or "EQUIPPED". */
	FText Value;
	/** Shown in the detail panel while the item is highlighted. */
	FText Detail;
	bool bEnabled = true;
	TFunction<void()> OnSelect;

	FZSMenuItem() = default;
	FZSMenuItem(const FText& InLabel, TFunction<void()> InSelect, const FText& InValue = FText::GetEmpty(), const FText& InDetail = FText::GetEmpty(), bool bInEnabled = true)
		: Label(InLabel), Value(InValue), Detail(InDetail), bEnabled(bInEnabled), OnSelect(MoveTemp(InSelect)) {}
};

struct FZSMenuPage
{
	FText Title;
	FText Subtitle;
	TArray<FZSMenuItem> Items;
	int32 Index = 0;
	/** Rebuilds Items (e.g. after buying something). Called when the page is pushed and after every selection. */
	TFunction<void(FZSMenuPage&)> Refresh;
	/** True for the pause menu (the game is paused while it's open). */
	bool bPausesGame = false;
	/** No detail panel (e.g. the locker, where the preview is on the right). */
	bool bHideDetail = false;
	/** Called when the page leaves the stack (back or close-all). */
	TFunction<void()> OnClosed;
};
