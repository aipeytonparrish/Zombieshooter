#pragma once

#include "CoreMinimal.h"

class AZSWeaponBase;
struct FZSWeaponStats;

enum class EZSAttachmentSlot : uint8
{
	Optic,
	Muzzle,
	Underbarrel,
	Magazine,
	Stock
};

/**
 * Weapons and attachments the profile can own, with prices and unlock levels (FZSWeaponRow / FZSAttachmentRow
 * DataTables later). Attachments are bought for a specific gun and stay on it - and are lost with it.
 */
namespace ZSWeaponCatalog
{
	struct FEntry
	{
		FName ItemId;
		TSubclassOf<AZSWeaponBase> Class;
		FText DisplayName;
		bool bStarter = false;      // always for sale in the market
		bool bPrimary = true;       // primary vs sidearm slot
		int32 Price = 1000;
		int32 RequiredLevel = 1;
		TArray<FName> Attachments;  // compatible attachment ids
		FText Description;
	};

	struct FAttachment
	{
		FName Id;
		FText DisplayName;
		EZSAttachmentSlot Slot = EZSAttachmentSlot::Muzzle;
		int32 Price = 500;
		int32 RequiredLevel = 1;
		FText Description;
		/** Applies the attachment to a weapon's stats. */
		void (*Apply)(FZSWeaponStats& Stats) = nullptr;
	};

	ZOMBIESHOOTER_API const TArray<FEntry>& All();
	ZOMBIESHOOTER_API const FEntry* Find(FName ItemId);
	ZOMBIESHOOTER_API FText DisplayName(FName ItemId);

	ZOMBIESHOOTER_API const TArray<FAttachment>& Attachments();
	ZOMBIESHOOTER_API const FAttachment* FindAttachment(FName Id);
	ZOMBIESHOOTER_API FText SlotName(EZSAttachmentSlot Slot);

	/** Max attachments on one gun. */
	constexpr int32 MaxAttachments = 5;
}
