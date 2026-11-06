#include "CommonSessionSubsystem.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameModes/LyraUserFacingExperienceDefinition.h"
#include "HaivenLyraInventoryBridge.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "System/LyraAssetManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHaivenLyraInventorySmoke, Log, All);

/** Adds the grant bridge to every player controller as it logs in on the server. */
class FHaivenLyraInventoryModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		PostLogin = FGameModeEvents::GameModePostLoginEvent.AddStatic(&FHaivenLyraInventoryModule::AddBridge);
		if (FParse::Value(FCommandLine::Get(), TEXT("HaivenEosSmokeRole="), SmokeRole) &&
			FParse::Value(FCommandLine::Get(), TEXT("HaivenEosSmokeExperience="), SmokeExperience))
		{
			SmokeTicker = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateRaw(this, &FHaivenLyraInventoryModule::TickSmoke));
		}
	}

	virtual void ShutdownModule() override
	{
		FGameModeEvents::GameModePostLoginEvent.Remove(PostLogin);
		if (SmokeTicker.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(SmokeTicker);
			SmokeTicker.Reset();
		}
	}

private:
	static void AddBridge(AGameModeBase* GameMode, APlayerController* NewPlayer)
	{
		if (!NewPlayer || NewPlayer->FindComponentByClass<UHaivenLyraInventoryBridge>()) return;
		UHaivenLyraInventoryBridge* Bridge = NewObject<UHaivenLyraInventoryBridge>(NewPlayer, TEXT("HaivenLyraInventoryBridge"));
		Bridge->RegisterComponent();
	}

	bool TickSmoke(float)
	{
		if (!GEngine) return true;
		UWorld* World = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		APlayerController* Controller = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
		ULocalPlayer* Player = Controller ? Controller->GetLocalPlayer() : nullptr;
		if (!Player || !Controller || !Player->GetPreferredUniqueNetId().IsValid()) return true;

		FPrimaryAssetId ExperienceId = FPrimaryAssetId::ParseTypeAndName(SmokeExperience);
		if (!ExperienceId.PrimaryAssetType.IsValid())
		{
			ExperienceId = FPrimaryAssetId(
				FPrimaryAssetType(ULyraUserFacingExperienceDefinition::StaticClass()->GetFName()),
				FName(*SmokeExperience));
		}
		ULyraAssetManager& AssetManager = ULyraAssetManager::Get();
		if (TSharedPtr<FStreamableHandle> Handle = AssetManager.LoadPrimaryAsset(ExperienceId))
		{
			Handle->WaitUntilComplete();
		}
		const ULyraUserFacingExperienceDefinition* Experience =
			Cast<ULyraUserFacingExperienceDefinition>(AssetManager.GetPrimaryAssetObject(ExperienceId));
		UCommonSessionSubsystem* Sessions = GameInstance->GetSubsystem<UCommonSessionSubsystem>();
		UCommonSession_HostSessionRequest* Request = Experience && Sessions
			? Sessions->CreateOnlineHostSessionRequest()
			: nullptr;
		if (Experience && Sessions && !Request)
		{
			Request = NewObject<UCommonSession_HostSessionRequest>();
			Request->OnlineMode = ECommonSessionOnlineMode::Online;
			Request->bUseLobbies = true;
			Request->bUseLobbiesVoiceChat = false;
			Request->bUsePresence = !IsRunningDedicatedServer();
		}
		if (Experience && Request)
		{
			// LyraGame does not export CreateHostingRequest to external modules.
			Request->MapID = Experience->MapID;
			Request->ModeNameForAdvertisement = Experience->GetPrimaryAssetId().PrimaryAssetName.ToString();
			Request->ExtraArgs = Experience->ExtraArgs;
			Request->ExtraArgs.Add(TEXT("Experience"), Experience->ExperienceID.PrimaryAssetName.ToString());
			Request->MaxPlayerCount = Experience->MaxPlayerCount;
		}
		if (!Experience || !Sessions || !Request)
		{
			UE_LOG(LogHaivenLyraInventorySmoke, Error,
				TEXT("HAIVEN_EOS_SMOKE session_action_failed role=%s experience=%s"),
				*SmokeRole, *ExperienceId.ToString());
			return false;
		}

		if (SmokeRole.Equals(TEXT("host"), ESearchCase::IgnoreCase))
		{
			UE_LOG(LogHaivenLyraInventorySmoke, Display,
				TEXT("HAIVEN_EOS_SMOKE session_action role=host experience=%s"),
				*ExperienceId.ToString());
			Sessions->HostSession(Controller, Request);
		}
		else if (SmokeRole.Equals(TEXT("guest"), ESearchCase::IgnoreCase))
		{
			UE_LOG(LogHaivenLyraInventorySmoke, Display,
				TEXT("HAIVEN_EOS_SMOKE session_action role=guest experience=%s"),
				*ExperienceId.ToString());
			Sessions->QuickPlaySession(Controller, Request);
		}
		else
		{
			UE_LOG(LogHaivenLyraInventorySmoke, Error,
				TEXT("HAIVEN_EOS_SMOKE session_action_failed unknown_role=%s"), *SmokeRole);
		}
		return false;
	}

	FDelegateHandle PostLogin;
	FTSTicker::FDelegateHandle SmokeTicker;
	FString SmokeRole;
	FString SmokeExperience;
};

IMPLEMENT_MODULE(FHaivenLyraInventoryModule, HaivenLyraInventory)
