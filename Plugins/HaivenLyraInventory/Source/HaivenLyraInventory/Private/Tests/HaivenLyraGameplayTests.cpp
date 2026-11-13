#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CommonSessionSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "GameModes/LyraUserFacingExperienceDefinition.h"
#include "HaivenLyraGameplay.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHaivenLyraGameplaySelectionTest,
	"Haiven.LyraDemo.GameplaySelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHaivenLyraGameplaySelectionTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("a shooter map is not the front end"), HaivenLyraGameplay::IsFrontEndMap(TEXT("L_ShooterGym")));
	TestTrue(TEXT("a PIE front-end map is still the front end"),
		HaivenLyraGameplay::IsFrontEndMap(TEXT("UEDPIE_0_L_LyraFrontEnd")));

	const FPrimaryAssetId MapId(FPrimaryAssetType(TEXT("Map")), TEXT("L_ShooterGym"));
	const FPrimaryAssetId ExperienceId(
		FPrimaryAssetType(TEXT("LyraExperienceDefinition")), TEXT("B_ShooterGame_Elimination"));

	ULyraUserFacingExperienceDefinition* FrontEnd = NewObject<ULyraUserFacingExperienceDefinition>(
		GetTransientPackage(), TEXT("B_LyraFrontEnd_Experience"));
	FrontEnd->bIsDefaultExperience = true;
	FrontEnd->MapID = MapId;
	FrontEnd->ExperienceID = ExperienceId;

	ULyraUserFacingExperienceDefinition* Shown = NewObject<ULyraUserFacingExperienceDefinition>(
		GetTransientPackage(), TEXT("B_ShownExperience"));
	Shown->bShowInFrontEnd = true;
	Shown->MapID = MapId;
	Shown->ExperienceID = ExperienceId;

	ULyraUserFacingExperienceDefinition* DefaultExperience = NewObject<ULyraUserFacingExperienceDefinition>(
		GetTransientPackage(), TEXT("B_DefaultExperience"));
	DefaultExperience->bIsDefaultExperience = true;
	DefaultExperience->bShowInFrontEnd = false;
	DefaultExperience->MapID = MapId;
	DefaultExperience->ExperienceID = ExperienceId;
	DefaultExperience->ExtraArgs.Add(TEXT("Keep"), TEXT("1"));

	TArray<UObject*> Loaded = {FrontEnd, Shown, DefaultExperience};
	TestEqual(TEXT("PLAY prefers the default experience over the front end and a shown tile"),
		HaivenLyraGameplay::SelectGameplayExperience(Loaded),
		static_cast<const ULyraUserFacingExperienceDefinition*>(DefaultExperience));

	Loaded = {FrontEnd, Shown};
	TestEqual(TEXT("without a default, PLAY uses the first front-end tile"),
		HaivenLyraGameplay::SelectGameplayExperience(Loaded),
		static_cast<const ULyraUserFacingExperienceDefinition*>(Shown));

	Loaded = {FrontEnd};
	TestNull(TEXT("the front end itself is not a match"), HaivenLyraGameplay::SelectGameplayExperience(Loaded));

	UCommonSession_HostSessionRequest* Request = HaivenLyraGameplay::MakeOfflineHostRequest(*DefaultExperience, *DefaultExperience);
	if (!TestNotNull(TEXT("offline host request"), Request)) return false;
	TestEqual(TEXT("PLAY does not open an Epic lobby"), Request->OnlineMode, ECommonSessionOnlineMode::Offline);
	TestFalse(TEXT("an offline match does not create a lobby"), Request->bUseLobbies);
	TestFalse(TEXT("an offline match does not publish presence"), Request->bUsePresence);
	TestEqual(TEXT("the request keeps the experience map"), Request->MapID, MapId);
	TestEqual(TEXT("the URL carries the gameplay experience"),
		Request->ExtraArgs.FindRef(TEXT("Experience")), ExperienceId.PrimaryAssetName.ToString());
	TestEqual(TEXT("experience extra arguments are kept"), Request->ExtraArgs.FindRef(TEXT("Keep")), FString(TEXT("1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHaivenLyraMatchRewardTest,
	"Haiven.LyraDemo.MatchReward",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHaivenLyraMatchRewardTest::RunTest(const FString& Parameters)
{
	APlayerState* Local = NewObject<APlayerState>(GetTransientPackage(), TEXT("HaivenLocalPlayerState"));
	APlayerState* Bot = NewObject<APlayerState>(GetTransientPackage(), TEXT("HaivenBotPlayerState"));
	TestTrue(TEXT("eliminating a bot earns the match reward"),
		HaivenLyraGameplay::IsRewardingElimination(Local, Local, Bot));
	TestFalse(TEXT("being eliminated earns nothing"), HaivenLyraGameplay::IsRewardingElimination(Local, Bot, Local));
	TestFalse(TEXT("another player's elimination earns nothing"),
		HaivenLyraGameplay::IsRewardingElimination(Local, Bot, Bot));
	TestFalse(TEXT("eliminating yourself earns nothing"), HaivenLyraGameplay::IsRewardingElimination(Local, Local, Local));
	TestFalse(TEXT("an unknown instigator earns nothing"), HaivenLyraGameplay::IsRewardingElimination(Local, nullptr, Bot));
	TestFalse(TEXT("no local player state earns nothing"), HaivenLyraGameplay::IsRewardingElimination(nullptr, nullptr, Bot));
	return true;
}

#endif
