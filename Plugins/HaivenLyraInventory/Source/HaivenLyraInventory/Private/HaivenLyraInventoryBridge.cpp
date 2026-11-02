#include "HaivenLyraInventoryBridge.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "HaivenLyraInventoryClient.h"
#include "HaivenLyraInventorySettings.h"
#include "Inventory/LyraInventoryItemDefinition.h"
#include "Inventory/LyraInventoryManagerComponent.h"

UHaivenLyraInventoryBridge::UHaivenLyraInventoryBridge()
{
	SetIsReplicatedByDefault(true);
}

void UHaivenLyraInventoryBridge::BeginPlay()
{
	Super::BeginPlay();
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (!Controller || !Controller->IsLocalController()) return;
	if (ULocalPlayer* Player = Controller->GetLocalPlayer())
	{
		if (UHaivenLyraInventoryClient* Client = Player->GetSubsystem<UHaivenLyraInventoryClient>())
		{
			Client->BridgeReady(this);
		}
	}
}

void UHaivenLyraInventoryBridge::ServerGrantEntitledItem_Implementation(const FString& Code)
{
	const TSoftClassPtr<ULyraInventoryItemDefinition>* Mapped =
		GetDefault<UHaivenLyraInventorySettings>()->ItemsByEntitlementCode.Find(Code);
	const TSubclassOf<ULyraInventoryItemDefinition> Definition = Mapped ? Mapped->LoadSynchronous() : nullptr;
	if (!Definition)
	{
		ClientGrantResult(Code, TEXT("no item mapped"));
		return;
	}

	ULyraInventoryManagerComponent* Inventory = GetOwner()->FindComponentByClass<ULyraInventoryManagerComponent>();
	if (!Inventory)
	{
		ClientGrantResult(Code, TEXT("no inventory on this controller"));
		return;
	}
	// One stack per entitlement: a re-read after reconnecting must not duplicate it.
	if (Inventory->FindFirstItemStackByDefinition(Definition))
	{
		ClientGrantResult(Code, TEXT("already in inventory"));
		return;
	}
	if (!Inventory->CanAddItemDefinition(Definition) || !Inventory->AddItemDefinition(Definition))
	{
		ClientGrantResult(Code, TEXT("inventory refused it"));
		return;
	}
	ClientGrantResult(Code, TEXT("added to inventory"));
}

void UHaivenLyraInventoryBridge::ClientGrantResult_Implementation(const FString& Code, const FString& Outcome)
{
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	ULocalPlayer* Player = Controller ? Controller->GetLocalPlayer() : nullptr;
	if (const UHaivenLyraInventoryClient* Client = Player ? Player->GetSubsystem<UHaivenLyraInventoryClient>() : nullptr)
	{
		Client->ShowOutcome(Code, Outcome);
	}
}
