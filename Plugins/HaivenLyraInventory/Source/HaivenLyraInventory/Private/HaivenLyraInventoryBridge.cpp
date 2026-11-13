#include "HaivenLyraInventoryBridge.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameModes/LyraExperienceManagerComponent.h"
#include "HaivenLyraInventoryClient.h"
#include "HaivenLyraInventorySettings.h"
#include "Inventory/LyraInventoryItemDefinition.h"
#include "Inventory/LyraInventoryManagerComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogHaivenLyraInventory, Log, All);

namespace
{
const TCHAR* NetModeName(const ENetMode Mode)
{
	switch (Mode)
	{
	case NM_Standalone: return TEXT("NM_Standalone");
	case NM_DedicatedServer: return TEXT("NM_DedicatedServer");
	case NM_ListenServer: return TEXT("NM_ListenServer");
	case NM_Client: return TEXT("NM_Client");
	default: return TEXT("NM_Unknown");
	}
}

bool IsEosSmoke()
{
	static const bool bEnabled = []
	{
		FString Role;
		return FParse::Value(FCommandLine::Get(), TEXT("HaivenEosSmokeRole="), Role);
	}();
	return bEnabled;
}
}

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
			if (IsEosSmoke())
			{
				UE_LOG(LogHaivenLyraInventory, Display, TEXT("HAIVEN_EOS_SMOKE bridge_ready net_mode=%s"),
					NetModeName(GetWorld() ? GetWorld()->GetNetMode() : NM_Standalone));
			}
			Client->BridgeReady(this);
		}
	}
}

void UHaivenLyraInventoryBridge::ServerGrantEntitledItem_Implementation(const FString& Code)
{
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (!Controller || !Controller->IsLocalController())
	{
		ClientGrantResult(Code, TEXT("remote grant requires server verification"));
		return;
	}

	// The bridge arrives at login, but the Experience adds the inventory
	// component only once it has loaded. A grant that comes first (the socket
	// connects during the load) waits for the Experience instead of failing.
	if (!GetOwner()->FindComponentByClass<ULyraInventoryManagerComponent>())
	{
		const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
		ULyraExperienceManagerComponent* Experience =
			GameState ? GameState->FindComponentByClass<ULyraExperienceManagerComponent>() : nullptr;
		if (Experience && !Experience->IsExperienceLoaded())
		{
			Experience->CallOrRegister_OnExperienceLoaded(FOnLyraExperienceLoaded::FDelegate::CreateUObject(
				this, &UHaivenLyraInventoryBridge::GrantAfterExperienceLoaded, Code));
			return;
		}
	}
	Grant(Code);
}

void UHaivenLyraInventoryBridge::GrantAfterExperienceLoaded(const ULyraExperienceDefinition* Experience, FString Code)
{
	Grant(Code);
}

void UHaivenLyraInventoryBridge::Grant(const FString& Code)
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
	if (IsEosSmoke())
	{
		UE_LOG(LogHaivenLyraInventory, Display,
			TEXT("HAIVEN_EOS_SMOKE inventory_grant code=%s outcome=%s net_mode=%s"), *Code, *Outcome,
			NetModeName(GetWorld() ? GetWorld()->GetNetMode() : NM_Standalone));
	}
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	ULocalPlayer* Player = Controller ? Controller->GetLocalPlayer() : nullptr;
	if (const UHaivenLyraInventoryClient* Client = Player ? Player->GetSubsystem<UHaivenLyraInventoryClient>() : nullptr)
	{
		Client->ShowOutcome(Code, Outcome);
	}
}
