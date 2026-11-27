#pragma once

#include "CoreMinimal.h"

class USkeletalMesh;
class UAnimSequence;
class USkinnedMeshComponent;
struct FZSCharacterLook;

/**
 * Character creator content: survivor bodies (Survivors Mega Pack, each on its own skeleton with its own idle / run),
 * first-person arm sets (AAA FPS Gloves 1-11, all on the UE5 Manny skeleton) and a tint palette for the arms.
 * Outfits are never lost on a failed run.
 */
namespace ZSCharacterCatalog
{
	struct FBody
	{
		FText DisplayName;
		const TCHAR* Mesh;
		const TCHAR* Idle;
		const TCHAR* Run;
		/** Arms set used when "match arms" is on. */
		int32 MatchingArms = 0;
		int32 MatchingTint = 0;
		int32 RequiredLevel = 1;
	};

	struct FArms
	{
		FText DisplayName;
		const TCHAR* Mesh;
		int32 RequiredLevel = 1;
	};

	struct FTint
	{
		FText DisplayName;
		FLinearColor Color;   // alpha 0 = keep the material's own colours
		int32 RequiredPrestige = 0;
	};

	ZOMBIESHOOTER_API const TArray<FBody>& Bodies();
	ZOMBIESHOOTER_API const TArray<FArms>& Arms();
	ZOMBIESHOOTER_API const TArray<FTint>& Tints();

	/** Arms set / tint actually used for a look (resolves "match arms"). */
	ZOMBIESHOOTER_API int32 ResolveArms(const FZSCharacterLook& Look);
	ZOMBIESHOOTER_API int32 ResolveTint(const FZSCharacterLook& Look);

	ZOMBIESHOOTER_API USkeletalMesh* LoadBody(int32 Index);
	ZOMBIESHOOTER_API USkeletalMesh* LoadArms(int32 Index);
	ZOMBIESHOOTER_API UAnimSequence* LoadIdle(int32 Body);
	ZOMBIESHOOTER_API UAnimSequence* LoadRun(int32 Body);

	/** Applies the tint to every material slot of an arms mesh component ("Tint A" on the gloves' materials). */
	ZOMBIESHOOTER_API void ApplyTint(USkinnedMeshComponent* Comp, int32 Tint);
}
