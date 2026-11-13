#include "HaivenLyraInventoryClient.h"

#include "CommonSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameModes/LyraUserFacingExperienceDefinition.h"
#include "GameFramework/PlayerState.h"
#include "HaivenLyraDemoSettings.h"
#include "HaivenLyraDemoSubsystem.h"
#include "Messages/LyraVerbMessage.h"
#include "Messages/LyraVerbMessageHelpers.h"
#include "HaivenLyraGameplay.h"
#include "HaivenLyraInventoryBridge.h"
#include "Engine/StreamableManager.h"
#include "System/LyraAssetManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHaivenLyraGameplay, Log, All);

namespace
{
	void Tell(const UHaivenLyraDemoSubsystem* Demo, const FText& Line)
	{
		if (Demo) Demo->AddNote(Line);
	}
}

void UHaivenLyraInventoryClient::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UHaivenLyraDemoSubsystem* Demo = Collection.InitializeDependency<UHaivenLyraDemoSubsystem>())
	{
		Demo->OnItemEntitled.AddDynamic(this, &UHaivenLyraInventoryClient::HandleItemEntitled);
		Demo->OnPlayGameplayRequested.AddDynamic(this, &UHaivenLyraInventoryClient::PlayGameplay);
		Demo->OnSignInStateChanged.AddDynamic(this, &UHaivenLyraInventoryClient::HandleSignInChanged);
	}
	UGameInstance* GameInstance = GetLocalPlayer() ? GetLocalPlayer()->GetGameInstance() : nullptr;
	UGameplayMessageSubsystem* Messages = GameInstance ? GameInstance->GetSubsystem<UGameplayMessageSubsystem>() : nullptr;
	const FGameplayTag Elimination = FGameplayTag::RequestGameplayTag(TEXT("Lyra.Elimination.Message"), false);
	if (Messages && Elimination.IsValid())
	{
		EliminationListener = Messages->RegisterListener(Elimination, this, &UHaivenLyraInventoryClient::HandleElimination);
	}
}

void UHaivenLyraInventoryClient::Deinitialize()
{
	if (UHaivenLyraDemoSubsystem* Demo = GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UHaivenLyraDemoSubsystem>() : nullptr)
	{
		Demo->OnItemEntitled.RemoveAll(this);
		Demo->OnPlayGameplayRequested.RemoveAll(this);
		Demo->OnSignInStateChanged.RemoveAll(this);
	}
	if (EliminationListener.IsValid()) EliminationListener.Unregister();
	Pending.Reset();
	Super::Deinitialize();
}

void UHaivenLyraInventoryClient::PlayGameplay()
{
	ULocalPlayer* Player = GetLocalPlayer();
	APlayerController* Controller = Player ? Player->GetPlayerController(nullptr) : nullptr;
	UWorld* World = Controller ? Controller->GetWorld() : nullptr;
	UHaivenLyraDemoSubsystem* Demo = Player ? Player->GetSubsystem<UHaivenLyraDemoSubsystem>() : nullptr;
	if (!Controller || !World)
	{
		Tell(Demo, NSLOCTEXT("HaivenLyraInventory", "PlayNoPlayer", "PLAY is waiting for the local player."));
		return;
	}
	if (!HaivenLyraGameplay::IsFrontEndMap(World->GetMapName()))
	{
		Tell(Demo, NSLOCTEXT("HaivenLyraInventory", "PlayAlready", "Already in a match."));
		return;
	}
	if (World->GetNetMode() == NM_Client)
	{
		Tell(Demo, NSLOCTEXT("HaivenLyraInventory", "PlayClient", "This client cannot start the match."));
		return;
	}
	if (!World->NextURL.IsEmpty())
	{
		Tell(Demo, NSLOCTEXT("HaivenLyraInventory", "PlayStarting", "Starting the match."));
		return;
	}

	ULyraAssetManager& AssetManager = ULyraAssetManager::Get();
	const FPrimaryAssetType ExperienceType(ULyraUserFacingExperienceDefinition::StaticClass()->GetFName());
	if (TSharedPtr<FStreamableHandle> Handle = AssetManager.LoadPrimaryAssetsWithType(ExperienceType))
	{
		Handle->WaitUntilComplete();
	}
	TArray<UObject*> Loaded;
	AssetManager.GetPrimaryAssetObjectList(ExperienceType, Loaded);
	const ULyraUserFacingExperienceDefinition* Experience = HaivenLyraGameplay::SelectGameplayExperience(Loaded);
	UGameInstance* GameInstance = World->GetGameInstance();
	UCommonSessionSubsystem* Sessions = GameInstance ? GameInstance->GetSubsystem<UCommonSessionSubsystem>() : nullptr;
	if (!Experience || !Sessions)
	{
		Tell(Demo, NSLOCTEXT("HaivenLyraInventory", "PlayNoExperience",
			"No Lyra gameplay experience is loaded. Install the Fab content, then press PLAY."));
		UE_LOG(LogHaivenLyraGameplay, Warning, TEXT("PLAY found no offline experience (loaded=%d, sessions=%d)."),
			Loaded.Num(), Sessions != nullptr);
		return;
	}

	UCommonSession_HostSessionRequest* Request = HaivenLyraGameplay::MakeOfflineHostRequest(*Experience, *Sessions);
	FText Error;
	if (!Request || !Request->ValidateAndLogErrors(Error))
	{
		Tell(Demo, FText::Format(
			NSLOCTEXT("HaivenLyraInventory", "PlayInvalid", "Could not start gameplay: {0}"),
			Error.IsEmpty() ? NSLOCTEXT("HaivenLyraInventory", "PlayInvalidBare", "the match request was rejected.") : Error));
		return;
	}

	const FText Title = Experience->TileTitle.IsEmpty()
		? FText::FromString(Experience->GetPrimaryAssetId().PrimaryAssetName.ToString())
		: Experience->TileTitle;
	Tell(Demo, FText::Format(NSLOCTEXT("HaivenLyraInventory", "PlayGo", "Starting {0}."), Title));
	UE_LOG(LogHaivenLyraGameplay, Display, TEXT("PLAY hosting offline experience %s map %s"),
		*Experience->GetPrimaryAssetId().ToString(), *Request->MapID.ToString());
	Sessions->HostSession(Controller, Request);
}

void UHaivenLyraInventoryClient::HandleElimination(FGameplayTag Channel, const FLyraVerbMessage& Message)
{
	if (bMatchRewardEarned) return;
	ULocalPlayer* Player = GetLocalPlayer();
	const APlayerController* Controller = Player ? Player->GetPlayerController(nullptr) : nullptr;
	const APlayerState* Local = Controller ? Controller->PlayerState.Get() : nullptr;
	const APlayerState* Killer = ULyraVerbMessageHelpers::GetPlayerStateFromObject(Message.Instigator);
	if (!HaivenLyraGameplay::IsRewardingElimination(Local, Killer, Message.Target)) return;

	bMatchRewardEarned = true;
	UHaivenLyraDemoSubsystem* Demo = Player->GetSubsystem<UHaivenLyraDemoSubsystem>();
	if (!Demo) return;
	if (Demo->GetSignInState() != EHaivenLyraSignInState::SignedIn)
	{
		bMatchRewardWaitingForSignIn = true;
		Tell(Demo, NSLOCTEXT("HaivenLyraInventory", "MatchRewardWaits",
			"First elimination! Sign in with Haiven to claim your match reward."));
		return;
	}
	ClaimMatchReward();
}

void UHaivenLyraInventoryClient::HandleSignInChanged(EHaivenLyraSignInState State)
{
	if (!bMatchRewardWaitingForSignIn || State != EHaivenLyraSignInState::SignedIn) return;
	bMatchRewardWaitingForSignIn = false;
	ClaimMatchReward();
}

void UHaivenLyraInventoryClient::ClaimMatchReward()
{
	UHaivenLyraDemoSubsystem* Demo = GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UHaivenLyraDemoSubsystem>() : nullptr;
	if (!Demo) return;
	if (!GetDefault<UHaivenLyraDemoSettings>()->IsRewardConfigured())
	{
		Tell(Demo, NSLOCTEXT("HaivenLyraInventory", "MatchRewardNone",
			"First elimination! No Haiven reward is configured (set RewardRef)."));
		return;
	}
	Tell(Demo, NSLOCTEXT("HaivenLyraInventory", "MatchRewardClaim",
		"First elimination! Claiming your Haiven match reward."));
	UE_LOG(LogHaivenLyraGameplay, Display, TEXT("match reward: first elimination, claiming the configured reward"));
	Demo->ClaimReward();
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
	// Travel gives each controller a new inventory instance. Replay the current
	// account projection as well as events that arrived before this bridge.
	TArray<FString> Codes = Pending;
	if (const UHaivenLyraDemoSubsystem* Demo = GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UHaivenLyraDemoSubsystem>() : nullptr)
	{
		for (const FString& Code : Demo->GetEntitledItems()) Codes.AddUnique(Code);
	}
	Pending.Reset();
	for (const FString& Code : Codes)
	{
		Bridge->ServerGrantEntitledItem(Code);
	}
}

void UHaivenLyraInventoryClient::ShowOutcome(const FString& Code, const FString& Outcome) const
{
	if (const UHaivenLyraDemoSubsystem* Demo = GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UHaivenLyraDemoSubsystem>() : nullptr)
	{
		Demo->AddNote(FText::Format(NSLOCTEXT("HaivenLyraInventory", "Outcome", "{0}: {1}"),
			FText::FromString(Code), FText::FromString(Outcome)));
	}
}
