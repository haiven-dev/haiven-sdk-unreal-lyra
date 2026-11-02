// Maps Haiven item entitlement codes to Lyra item definitions, in
// DefaultGame.ini under [/Script/HaivenLyraInventory.HaivenLyraInventorySettings].
// An entitlement code with no mapping is announced in chat but grants nothing.

#pragma once

#include "Engine/DeveloperSettings.h"

#include "HaivenLyraInventorySettings.generated.h"

class ULyraInventoryItemDefinition;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Haiven Lyra Inventory"))
class HAIVENLYRAINVENTORY_API UHaivenLyraInventorySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Entitlement code (for example tag/hero) to the item definition it grants. */
	UPROPERTY(Config, EditAnywhere, Category = "Haiven")
	TMap<FString, TSoftClassPtr<ULyraInventoryItemDefinition>> ItemsByEntitlementCode;
};
