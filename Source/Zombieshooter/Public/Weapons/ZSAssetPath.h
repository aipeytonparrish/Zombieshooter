#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"

namespace ZSAssets
{
	/** "/Game/Folder/Asset" -> soft pointer to "/Game/Folder/Asset.Asset". */
	template <typename T>
	TSoftObjectPtr<T> Soft(const FString& PackagePath)
	{
		int32 Slash = INDEX_NONE;
		PackagePath.FindLastChar(TEXT('/'), Slash);
		return TSoftObjectPtr<T>(FSoftObjectPath(PackagePath + TEXT(".") + PackagePath.Mid(Slash + 1)));
	}
}
