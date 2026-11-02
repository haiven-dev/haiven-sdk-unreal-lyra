#include "HaivenLyraInventoryClient.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "HaivenLyraDemoSubsystem.h"
#include "HaivenLyraInventoryBridge.h"

void UHaivenLyraInventoryClient::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UHaivenLyraDemoSubsystem* Demo = Collection.InitializeDependency<UHaivenLyraDemoSubsystem>())
	{
		Demo->OnItemEntitled.AddDynamic(this, &UHaivenLyraInventoryClient::HandleItemEntitled);
	}
}

void UHaivenLyraInventoryClient::Deinitialize()
{
	if (UHaivenLyraDemoSubsystem* Demo = GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UHaivenLyraDemoSubsystem>() : nullptr)
	{
		Demo->OnItemEntitled.RemoveAll(this);
	}
	Pending.Reset();
	Super::Deinitialize();
}

UHaivenLyraInventoryBridge* UHaivenLyraInventoryClient::FindBridge() const
{
	const APlayerController* Controller = GetLocalPlayer() ? GetLocalPlayer()->GetPlayerController(nullptr) : nullptr;
	return Controller ? Controller->FindComponentByClass<UHaivenLyraInventoryBridge>() : nullptr;
}

void UHaivenLyraInventoryClient::HandleItemEntitled(const FString& Code)
{
	if (UHaivenLyraInventoryBridge* Bridge = FindBridge())
	{
		Bridge->ServerGrantEntitledItem(Code);
		return;
	}
	Pending.AddUnique(Code);
}

void UHaivenLyraInventoryClient::BridgeReady(UHaivenLyraInventoryBridge* Bridge)
{
	// Travel gives each new controller a new bridge, so whatever waited goes through it.
	for (const FString& Code : Pending)
	{
		Bridge->ServerGrantEntitledItem(Code);
	}
	Pending.Reset();
}

void UHaivenLyraInventoryClient::ShowOutcome(const FString& Code, const FString& Outcome) const
{
	if (const UHaivenLyraDemoSubsystem* Demo = GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UHaivenLyraDemoSubsystem>() : nullptr)
	{
		Demo->AddNote(FText::Format(NSLOCTEXT("HaivenLyraInventory", "Outcome", "{0}: {1}"),
			FText::FromString(Code), FText::FromString(Outcome)));
	}
}
