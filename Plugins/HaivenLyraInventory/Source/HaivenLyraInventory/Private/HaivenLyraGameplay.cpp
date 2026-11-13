#include "HaivenLyraGameplay.h"

#include "CommonSessionSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "GameModes/LyraUserFacingExperienceDefinition.h"

namespace
{
	bool IsPlayable(const ULyraUserFacingExperienceDefinition* Experience)
	{
		if (!Experience || !Experience->MapID.IsValid() || !Experience->ExperienceID.IsValid()) return false;
		const FString ObjectName = Experience->GetName();
		const FString AssetName = Experience->GetPrimaryAssetId().PrimaryAssetName.ToString();
		return !ObjectName.Contains(TEXT("FrontEnd"), ESearchCase::IgnoreCase)
			&& !AssetName.Contains(TEXT("FrontEnd"), ESearchCase::IgnoreCase);
	}
}

bool HaivenLyraGameplay::IsFrontEndMap(const FString& MapName)
{
	return MapName.Contains(TEXT("FrontEnd"), ESearchCase::IgnoreCase);
}

bool HaivenLyraGameplay::IsRewardingElimination(
	const APlayerState* Local,
	const APlayerState* Killer,
	const UObject* Target)
{
	return Local && Killer == Local && Target != Local;
}

const ULyraUserFacingExperienceDefinition* HaivenLyraGameplay::SelectGameplayExperience(
	const TArray<UObject*>& Loaded)
{
	const ULyraUserFacingExperienceDefinition* Shown = nullptr;
	for (UObject* Object : Loaded)
	{
		const ULyraUserFacingExperienceDefinition* Experience = Cast<ULyraUserFacingExperienceDefinition>(Object);
		if (!IsPlayable(Experience)) continue;
		if (Experience->bIsDefaultExperience) return Experience;
		if (!Shown && Experience->bShowInFrontEnd) Shown = Experience;
	}
	return Shown;
}

UCommonSession_HostSessionRequest* HaivenLyraGameplay::MakeOfflineHostRequest(
	const ULyraUserFacingExperienceDefinition& Experience,
	UObject& Outer)
{
	// LyraGame does not export a way to build this request as offline.
	UCommonSession_HostSessionRequest* Request = NewObject<UCommonSession_HostSessionRequest>(&Outer);
	Request->OnlineMode = ECommonSessionOnlineMode::Offline;
	Request->bUseLobbies = false;
	Request->bUseLobbiesVoiceChat = false;
	Request->bUsePresence = false;
	Request->MapID = Experience.MapID;
	Request->ModeNameForAdvertisement = Experience.GetPrimaryAssetId().PrimaryAssetName.ToString();
	Request->ExtraArgs = Experience.ExtraArgs;
	Request->ExtraArgs.Add(TEXT("Experience"), Experience.ExperienceID.PrimaryAssetName.ToString());
	Request->MaxPlayerCount = Experience.MaxPlayerCount;
	return Request;
}
