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

	/** Camos: a pattern overlay on the gun (M_ZS_Camo), unlocked by the gun's level. "NONE" is the factory finish. */
	struct FCamo
	{
		FName Id;
		FText DisplayName;
		int32 RequiredLevel = 1;
		FLinearColor A, B, C;       // pattern colours (dark, mid, light)
		float Scale = 1.f;          // pattern size
		float Metal = 0.f;          // 1 = polished metal (gold)
		float Emissive = 0.f;       // glow (dark matter)
	};
	ZOMBIESHOOTER_API const TArray<FCamo>& Camos();
	ZOMBIESHOOTER_API const FCamo* FindCamo(FName Id);

	/** Max attachments on one gun. */
	constexpr int32 MaxAttachments = 5;
}
