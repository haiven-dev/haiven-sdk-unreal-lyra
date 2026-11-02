// Editor smoke tests for the demo plugin: the classes register, the settings
// gate, the code-built panel keeps one line per sent message and carries the
// status, Sign in and Claim reward controls, the HUD's finite status spellings,
// and the Sign in with Haiven state machine.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "IPAddress.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Base64.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "HaivenLyraChatWidget.h"
#include "HaivenLyraDemoSettings.h"
#include "HaivenLyraDemoSubsystem.h"
#include "HaivenLyraDemoTestChatWidget.h"
#include "HaivenLyraEntitlements.h"
#include "HaivenChat.h"
#include "HaivenClientTypes.h"
#include "HaivenLyraMotdWidget.h"
#include "HaivenLyraPresentation.h"
#include "HaivenLyraSignIn.h"
#include "HaivenRewards.h"
#include "HaivenSubsystem.h"
#include "Subsystems/LocalPlayerSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHaivenLyraDemoClassesTest,
	"Haiven.LyraDemo.Classes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHaivenLyraDemoClassesTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("the subsystem is a local-player subsystem"),
		UHaivenLyraDemoSubsystem::StaticClass()->IsChildOf(ULocalPlayerSubsystem::StaticClass()));

	UHaivenLyraDemoSettings* Settings = NewObject<UHaivenLyraDemoSettings>();
	TestFalse(TEXT("empty settings are not configured"), Settings->IsConfigured());
	Settings->BaseUrl = TEXT("https://api.example.test/v0");
	Settings->AppId = TEXT("app");
	Settings->ChannelId = TEXT("general");
	TestFalse(TEXT("a missing realtime URL is not configured: the plugin never derives it"), Settings->IsConfigured());
	Settings->RealtimeUrl = TEXT("wss://api.example.test/v0/ws");
	TestTrue(TEXT("chat can be configured independently of public MOTD"), Settings->IsRealtimeConfigured());
	TestFalse(TEXT("the public integration id is required for the full demo"), Settings->IsConfigured());
	Settings->IntegrationId = TEXT("integration-public");
	TestTrue(TEXT("base URL and public integration id configure MOTD"), Settings->IsMotdConfigured());
	TestTrue(TEXT("MOTD and realtime settings configure the full demo"), Settings->IsConfigured());
	TestFalse(TEXT("no reward ref hides the reward claim"), Settings->IsRewardConfigured());
	Settings->RewardRef = TEXT("lyra_demo_reward");
	TestTrue(TEXT("integration id and reward ref configure the reward claim"), Settings->IsRewardConfigured());
	Settings->IntegrationId.Reset();
	TestFalse(TEXT("a reward is claimed for an integration, so it needs the integration id"), Settings->IsRewardConfigured());
	Settings->IntegrationId = TEXT("integration-public");

	TestEqual(
		TEXT("an unset widget class resolves to the code-built fallback"),
		Settings->ResolveChatWidgetClass().Get(),
		UHaivenLyraChatWidget::StaticClass());
	TestTrue(
		TEXT("the code-built widget is a supported configured class"),
		UHaivenLyraDemoSettings::IsSupportedChatWidgetClass(UHaivenLyraChatWidget::StaticClass()));
	TestFalse(
		TEXT("an unrelated UMG class is rejected"),
		UHaivenLyraDemoSettings::IsSupportedChatWidgetClass(UUserWidget::StaticClass()));
	TestFalse(
		TEXT("a null class is rejected"),
		UHaivenLyraDemoSettings::IsSupportedChatWidgetClass(nullptr));
	TestEqual(
		TEXT("an unset MOTD class resolves to its native fallback"),
		Settings->ResolveMotdWidgetClass().Get(),
		UHaivenLyraMotdWidget::StaticClass());
	TestTrue(TEXT("the native MOTD class is supported"),
		UHaivenLyraDemoSettings::IsSupportedMotdWidgetClass(UHaivenLyraMotdWidget::StaticClass()));
	TestFalse(TEXT("an unrelated MOTD class is rejected"),
		UHaivenLyraDemoSettings::IsSupportedMotdWidgetClass(UUserWidget::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHaivenLyraDemoPanelTest,
	"Haiven.LyraDemo.Panel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHaivenLyraDemoPanelTest::RunTest(const FString& Parameters)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("editor world"), World)) return false;

	UHaivenLyraChatWidget* Panel = CreateWidget<UHaivenLyraChatWidget>(World, UHaivenLyraChatWidget::StaticClass());
	if (!TestNotNull(TEXT("panel"), Panel)) return false;
	Panel->TakeWidget();

	UScrollBox* Log = Panel->WidgetTree ? Cast<UScrollBox>(Panel->WidgetTree->FindWidget(TEXT("Log"))) : nullptr;
	if (!TestNotNull(TEXT("the code-built tree has a log"), Log)) return false;
	TestNotNull(TEXT("the code-built tree has an input"), Panel->WidgetTree->FindWidget(TEXT("Input")));
	TestTrue(TEXT("the fallback satisfies the designer binding contract"), Panel->HasDesignerBindings());

	Panel->AddLine(FText::FromString(TEXT("Haiven chat connected.")));
	Panel->SetKeyedLine(TEXT("client-1"), FText::FromString(TEXT("you (sending): hi")));
	Panel->SetKeyedLine(TEXT("client-1"), FText::FromString(TEXT("you: hi")));
	Panel->SetKeyedLine(TEXT("client-2"), FText::FromString(TEXT("you (held for review): hey")));
	TestEqual(TEXT("one line per note and per sent message"), Log->GetChildrenCount(), 3);

	UTextBlock* Status = Cast<UTextBlock>(Panel->WidgetTree->FindWidget(TEXT("Status")));
	if (!TestNotNull(TEXT("the code-built tree has a status header"), Status)) return false;
	const FText Lines = HaivenLyraPresentation::StatusLines(TEXT("Ada"), TEXT("SIGNED IN"), TEXT("CONNECTED"), TEXT("ALLOWED"), TEXT("CLAIMED"));
	Panel->SetStatus(Lines);
	TestEqual(TEXT("the status header shows the lines it is given"), Status->GetText().ToString(), Lines.ToString());
	TestEqual(TEXT("the shown status is readable back"), Panel->GetDisplayedStatus().ToString(), Lines.ToString());

	UButton* Claim = Cast<UButton>(Panel->WidgetTree->FindWidget(TEXT("ClaimReward")));
	if (!TestNotNull(TEXT("the code-built tree has a Claim reward button"), Claim)) return false;
	TestFalse(TEXT("the claim action starts hidden until a reward is configured"), Panel->IsRewardActionAvailable());
	TestTrue(TEXT("a hidden claim action collapses"), Claim->GetVisibility() == ESlateVisibility::Collapsed);
	Panel->SetRewardAction(true, true);
	TestTrue(TEXT("a configured reward with no claim in flight can be claimed"), Panel->IsRewardActionAvailable());
	TestTrue(TEXT("the button is enabled"), Claim->GetIsEnabled());
	Panel->SetRewardAction(true, false);
	TestFalse(TEXT("a claim in flight disables the action"), Panel->IsRewardActionAvailable());
	TestTrue(TEXT("an in-flight claim keeps the button visible"), Claim->GetVisibility() == ESlateVisibility::Visible);
	TestFalse(TEXT("an in-flight claim disables the button"), Claim->GetIsEnabled());

	UButton* SignIn = Cast<UButton>(Panel->WidgetTree->FindWidget(TEXT("SignIn")));
	UTextBlock* SignInLabel = Cast<UTextBlock>(Panel->WidgetTree->FindWidget(TEXT("SignInLabel")));
	if (!TestNotNull(TEXT("the code-built tree has a Sign in button"), SignIn)) return false;
	if (!TestNotNull(TEXT("the Sign in button has a label"), SignInLabel)) return false;
	TestFalse(TEXT("the sign-in action starts hidden until sign-in is configured"), Panel->IsSignInActionAvailable());
	TestTrue(TEXT("a hidden sign-in action collapses"), SignIn->GetVisibility() == ESlateVisibility::Collapsed);
	const FText Retry = HaivenLyraPresentation::SignInActionLabel(EHaivenLyraSignInState::Failed);
	Panel->SetSignInAction(true, Retry);
	TestTrue(TEXT("a configured sign-in can be pressed"), Panel->IsSignInActionAvailable());
	TestEqual(TEXT("the label says what pressing does"), SignInLabel->GetText().ToString(), Retry.ToString());
	TestEqual(TEXT("the label is readable back"), Panel->GetSignInActionLabel().ToString(), Retry.ToString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHaivenLyraDemoPresentationTest,
	"Haiven.LyraDemo.Presentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHaivenLyraDemoPresentationTest::RunTest(const FString& Parameters)
{
	using namespace HaivenLyraPresentation;

	TestEqual(TEXT("disconnected"), ConnectionStatus(EHaivenRealtimeState::Disconnected), FString(TEXT("OFFLINE")));
	TestEqual(TEXT("connecting"), ConnectionStatus(EHaivenRealtimeState::Connecting), FString(TEXT("CONNECTING")));
	TestEqual(TEXT("authenticating"), ConnectionStatus(EHaivenRealtimeState::Authenticating), FString(TEXT("SIGNING IN")));
	TestEqual(TEXT("ready"), ConnectionStatus(EHaivenRealtimeState::Ready), FString(TEXT("CONNECTED")));

	// The same spellings as the Boss Room demo's moderation drawer.
	TestEqual(TEXT("sending"), ModerationStatus(EHaivenOutgoingChatState::Sending), FString(TEXT("CHECKING")));
	TestEqual(TEXT("unconfirmed"), ModerationStatus(EHaivenOutgoingChatState::Unconfirmed), FString(TEXT("WAITING FOR CONNECTION")));
	TestEqual(TEXT("accepted"), ModerationStatus(EHaivenOutgoingChatState::Accepted), FString(TEXT("ALLOWED")));
	TestEqual(TEXT("held"), ModerationStatus(EHaivenOutgoingChatState::Held), FString(TEXT("HELD FOR REVIEW")));
	TestEqual(TEXT("removed"), ModerationStatus(EHaivenOutgoingChatState::Removed), FString(TEXT("NOT SHOWN")));
	TestEqual(TEXT("refused"), ModerationStatus(EHaivenOutgoingChatState::Refused), FString(TEXT("REFUSED")));

	auto Reward = [](EHaivenRewardClaimResult Result, bool bRetryable = false)
	{
		FHaivenRewardClaimOutcome Outcome;
		Outcome.Result = Result;
		Outcome.bRetryable = bRetryable;
		return RewardStatus(Outcome);
	};
	TestEqual(TEXT("claimed"), Reward(EHaivenRewardClaimResult::Claimed), FString(TEXT("CLAIMED")));
	TestEqual(TEXT("a retry the service had already taken reads as claimed"),
		Reward(EHaivenRewardClaimResult::AlreadyRecorded), FString(TEXT("CLAIMED")));
	TestEqual(TEXT("already claimed this period"), Reward(EHaivenRewardClaimResult::AlreadyClaimed), FString(TEXT("ALREADY CLAIMED")));
	TestEqual(TEXT("not eligible"), Reward(EHaivenRewardClaimResult::NotEligible), FString(TEXT("NOT ELIGIBLE")));
	TestEqual(TEXT("refund in progress"), Reward(EHaivenRewardClaimResult::RefundInProgress), FString(TEXT("TEMPORARILY UNAVAILABLE")));
	TestEqual(TEXT("claim conflict"), Reward(EHaivenRewardClaimResult::ClaimConflict), FString(TEXT("CLAIM CONFLICT")));
	TestEqual(TEXT("a retryable failure invites a retry"), Reward(EHaivenRewardClaimResult::Failed, true), FString(TEXT("RETRY CLAIM")));
	TestEqual(TEXT("a final failure says so"), Reward(EHaivenRewardClaimResult::Failed, false), FString(TEXT("CLAIM FAILED")));
	TestEqual(TEXT("not found is a failure"), Reward(EHaivenRewardClaimResult::NotFound), FString(TEXT("CLAIM FAILED")));

	FHaivenRestCurrentUserSessionResponse Session;
	TestEqual(TEXT("no name is unknown"), AccountName(Session), FString());
	Session.bHasUsername = true;
	Session.Username = TEXT("ada");
	TestEqual(TEXT("the username stands in for a missing display name"), AccountName(Session), FString(TEXT("ada")));
	Session.bHasDisplayName = true;
	Session.DisplayName = TEXT("Ada L.");
	TestEqual(TEXT("the display name wins"), AccountName(Session), FString(TEXT("Ada L.")));

	TestEqual(TEXT("signed out"), SignInStatus(EHaivenLyraSignInState::SignedOut), FString(TEXT("SIGNED OUT")));
	TestEqual(TEXT("signing in"), SignInStatus(EHaivenLyraSignInState::SigningIn), FString(TEXT("SIGNING IN")));
	TestEqual(TEXT("signed in"), SignInStatus(EHaivenLyraSignInState::SignedIn), FString(TEXT("SIGNED IN")));
	TestEqual(TEXT("failed"), SignInStatus(EHaivenLyraSignInState::Failed), FString(TEXT("SIGN-IN FAILED")));
	TestEqual(TEXT("expired"), SignInStatus(EHaivenLyraSignInState::Expired), FString(TEXT("SESSION EXPIRED")));
	TestEqual(TEXT("signed out invites sign-in"),
		SignInActionLabel(EHaivenLyraSignInState::SignedOut).ToString(), FString(TEXT("Sign in with Haiven")));
	TestEqual(TEXT("signing in can be cancelled"),
		SignInActionLabel(EHaivenLyraSignInState::SigningIn).ToString(), FString(TEXT("Cancel sign-in")));
	TestEqual(TEXT("signed in can sign out"),
		SignInActionLabel(EHaivenLyraSignInState::SignedIn).ToString(), FString(TEXT("Sign out")));
	TestEqual(TEXT("a failure invites a retry"),
		SignInActionLabel(EHaivenLyraSignInState::Failed).ToString(), FString(TEXT("Retry sign-in")));
	TestEqual(TEXT("an expiry invites a retry"),
		SignInActionLabel(EHaivenLyraSignInState::Expired).ToString(), FString(TEXT("Retry sign-in")));

	const FString Lines = StatusLines(FString(), TEXT("SIGNING IN"), TEXT("CONNECTED"), TEXT("HELD FOR REVIEW"), TEXT("READY TO CLAIM")).ToString();
	TestTrue(TEXT("an unknown account is named as such"), Lines.Contains(AccountUnknown));
	TestTrue(TEXT("the lines carry the sign-in state"), Lines.Contains(TEXT("Sign-in: SIGNING IN")));
	TestTrue(TEXT("the lines carry the connection"), Lines.Contains(TEXT("Haiven: CONNECTED")));
	TestTrue(TEXT("the lines carry the moderation outcome"), Lines.Contains(TEXT("Last message: HELD FOR REVIEW")));
	TestTrue(TEXT("the lines carry the reward state"), Lines.Contains(TEXT("Reward: READY TO CLAIM")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHaivenLyraDemoWidgetClassSelectionTest,
	"Haiven.LyraDemo.WidgetClassSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHaivenLyraDemoWidgetClassSelectionTest::RunTest(const FString& Parameters)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("editor world"), World)) return false;

	UHaivenLyraDemoSettings* Settings = NewObject<UHaivenLyraDemoSettings>();
	Settings->ChatWidgetClass = UHaivenLyraDemoTestChatWidget::StaticClass();
	const TSubclassOf<UHaivenLyraChatWidget> ResolvedClass = Settings->ResolveChatWidgetClass();
	TestEqual(
		TEXT("the configured supported subclass is selected"),
		ResolvedClass.Get(),
		UHaivenLyraDemoTestChatWidget::StaticClass());

	UHaivenLyraChatWidget* Panel = CreateWidget<UHaivenLyraChatWidget>(World, ResolvedClass);
	if (!TestNotNull(TEXT("configured panel"), Panel)) return false;
	TestTrue(TEXT("the widget factory creates the selected subclass"), Panel->IsA<UHaivenLyraDemoTestChatWidget>());
	Panel->TakeWidget();
	TestTrue(TEXT("a native subclass retains the code-built fallback tree"), Panel->HasDesignerBindings());
	TestTrue(TEXT("the selected native subclass reports fallback geometry"), Panel->UsesCodeBuiltTree());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHaivenLyraDemoMotdPanelTest,
	"Haiven.LyraDemo.MotdPanel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHaivenLyraDemoMotdPanelTest::RunTest(const FString& Parameters)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("editor world"), World)) return false;

	UHaivenLyraMotdWidget* Panel = CreateWidget<UHaivenLyraMotdWidget>(World, UHaivenLyraMotdWidget::StaticClass());
	if (!TestNotNull(TEXT("MOTD panel"), Panel)) return false;
	Panel->TakeWidget();
	TestTrue(TEXT("native MOTD fallback has every designer binding"), Panel->HasDesignerBindings());
	TestTrue(TEXT("native MOTD fallback reports code-built presentation"), Panel->UsesCodeBuiltTree());

	Panel->PresentLoading();
	TestEqual(TEXT("loading state has shared spelling"), Panel->GetPresentationStateName(), FString(TEXT("loading")));
	Panel->PresentAbsent();
	TestEqual(TEXT("absent state has shared spelling"), Panel->GetPresentationStateName(), FString(TEXT("absent")));

	FHaivenMotd Message;
	Message.Id = TEXT("11111111-1111-4111-8111-111111111111");
	Message.Body = TEXT("<b>plain text, not markup</b>");
	Message.bHasUrl = true;
	Message.Url = TEXT("https://haiven.gg/news");
	Panel->PresentMessage(Message);
	TestEqual(TEXT("present state has shared spelling"), Panel->GetPresentationStateName(), FString(TEXT("present")));
	TestEqual(TEXT("missing title uses the shared fallback"), Panel->GetDisplayedTitle().ToString(), FString(TEXT("Message of the day")));
	TestEqual(TEXT("body remains plain text"), Panel->GetDisplayedBody().ToString(), Message.Body);
	TestTrue(TEXT("validated HTTPS action is exposed"), Panel->HasAction());

	Message.Id = TEXT("22222222-2222-4222-8222-222222222222");
	Message.Url = TEXT("http://haiven.gg/news");
	Panel->PresentMessage(Message);
	TestFalse(TEXT("an invalid action is stripped without rejecting the message"), Panel->HasAction());
	Panel->PresentFailure();
	TestEqual(TEXT("failure state has shared spelling"), Panel->GetPresentationStateName(), FString(TEXT("failure")));
	TestEqual(TEXT("failure copy is fixed and transport-free"),
		Panel->GetDisplayedStatus().ToString(), FString(TEXT("Unable to load message of the day.")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHaivenLyraDemoMotdWidgetClassSelectionTest,
	"Haiven.LyraDemo.MotdWidgetClassSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHaivenLyraDemoMotdWidgetClassSelectionTest::RunTest(const FString& Parameters)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("editor world"), World)) return false;

	UHaivenLyraDemoSettings* Settings = NewObject<UHaivenLyraDemoSettings>();
	Settings->MotdWidgetClass = UHaivenLyraDemoTestMotdWidget::StaticClass();
	const TSubclassOf<UHaivenLyraMotdWidget> ResolvedClass = Settings->ResolveMotdWidgetClass();
	TestEqual(TEXT("configured supported MOTD subclass is selected"),
		ResolvedClass.Get(), UHaivenLyraDemoTestMotdWidget::StaticClass());

	UHaivenLyraMotdWidget* Panel = CreateWidget<UHaivenLyraMotdWidget>(World, ResolvedClass);
	if (!TestNotNull(TEXT("configured MOTD panel"), Panel)) return false;
	Panel->TakeWidget();
	TestTrue(TEXT("selected subclass retains native fallback bindings"), Panel->HasDesignerBindings());
	TestTrue(TEXT("selected native subclass reports fallback geometry"), Panel->UsesCodeBuiltTree());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHaivenLyraDemoEntitlementsTest,
	"Haiven.LyraDemo.Entitlements",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHaivenLyraDemoEntitlementsTest::RunTest(const FString& Parameters)
{
	auto Entitlement = [](const TCHAR* Code, const TCHAR* Kind, const TCHAR* State)
	{
		FHaivenRestEntitlementResponse Result;
		Result.bHasCode = true;
		Result.Code = Code;
		Result.bHasKind = true;
		Result.Kind = Kind;
		Result.bHasState = true;
		Result.State = State;
		return Result;
	};

	FHaivenRestAccountEntitlementsResponse Response;
	TestEqual(TEXT("an absent list grants nothing"),
		HaivenLyraEntitlements::NewlyActiveItems(Response, {}).Num(), 0);

	Response.bHasEntitlements = true;
	Response.Entitlements = {
		Entitlement(TEXT("avatar_frame/founder_supporter"), TEXT("item"), TEXT("active")),
		Entitlement(TEXT("haiven_subscription"), TEXT("subscription"), TEXT("active")),
		Entitlement(TEXT("founder_legend"), TEXT("founder"), TEXT("active")),
		Entitlement(TEXT("tag/hero"), TEXT("item"), TEXT("active")),
		Entitlement(TEXT("tag/old"), TEXT("item"), TEXT("revoked")),
		Entitlement(TEXT("tag/hero"), TEXT("item"), TEXT("active")),
	};
	TestEqual(TEXT("only active items, in order, once each"),
		HaivenLyraEntitlements::NewlyActiveItems(Response, {}),
		TArray<FString>{TEXT("avatar_frame/founder_supporter"), TEXT("tag/hero")});

	const TSet<FString> Granted{TEXT("avatar_frame/founder_supporter")};
	TestEqual(TEXT("items already granted this session are skipped"),
		HaivenLyraEntitlements::NewlyActiveItems(Response, Granted),
		TArray<FString>{TEXT("tag/hero")});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHaivenLyraDemoSignInTest,
	"Haiven.LyraDemo.SignIn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace HaivenLyraDemoSignInDetail
{
	/** A JWT-shaped string with the given claims and no real signature: never a usable token. */
	FString FakeJwt(const FString& Claims)
	{
		auto Segment = [](const FString& Json)
		{
			FString Encoded = FBase64::Encode(Json, EBase64Mode::UrlSafe);
			Encoded.RemoveFromEnd(TEXT("=="));
			Encoded.RemoveFromEnd(TEXT("="));
			return Encoded;
		};
		return Segment(TEXT("{\"alg\":\"none\"}")) + TEXT(".") + Segment(Claims) + TEXT(".unsigned");
	}

	/** A loopback port nothing holds, below the ephemeral ranges. */
	uint32 FreeLoopbackPort()
	{
		ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		for (uint32 Port = 29001; Sockets && Port < 32000; ++Port)
		{
			FSocket* Probe = Sockets->CreateSocket(NAME_Stream, TEXT("HaivenLyraLoopbackProbe"), false);
			if (!Probe) return 0;
			TSharedRef<FInternetAddr> Address = Sockets->CreateInternetAddr();
			Address->SetLoopbackAddress();
			Address->SetPort(static_cast<int32>(Port));
			const bool bFree = Probe->Bind(*Address);
			Probe->Close();
			Sockets->DestroySocket(Probe);
			if (bFree) return Port;
		}
		return 0;
	}
}

bool FHaivenLyraDemoSignInTest::RunTest(const FString& Parameters)
{
	using namespace HaivenLyraDemoSignInDetail;
	using namespace HaivenLyraPresentation;

	// Settings and the login options handed to the plugin.
	UHaivenLyraDemoSettings* Fresh = NewObject<UHaivenLyraDemoSettings>();
	TestFalse(TEXT("sign-in needs settings"), Fresh->IsSignInConfigured());
	TestEqual(TEXT("an empty redirect uses the default loopback redirect"),
		Fresh->ResolveLoginRedirectUrl(), FString(UHaivenLyraDemoSettings::DefaultLoginRedirectUrl));
	Fresh->BaseUrl = TEXT("https://api.example.test/v0/");
	Fresh->IntegrationId = TEXT("lyra-test");
	Fresh->LoginEntryUrl = TEXT("https://app.example.test/connect/game");
	TestTrue(TEXT("API, integration and login page configure sign-in"), Fresh->IsSignInConfigured());
	TestEqual(TEXT("the exchange is under the API base"),
		Fresh->ResolveLoginExchangeUrl(), FString(TEXT("https://api.example.test/v0/auth/handoff/exchange")));
	const FHaivenGameLoginOptions Options = HaivenLyraSignIn::LoginOptions(*Fresh);
	TestEqual(TEXT("options: entry"), Options.EntryUrl, Fresh->LoginEntryUrl);
	TestEqual(TEXT("options: integration"), Options.IntegrationId, Fresh->IntegrationId);
	TestEqual(TEXT("options: redirect"), Options.RedirectUrl, Fresh->ResolveLoginRedirectUrl());
	TestTrue(TEXT("options: no screen hint the API would refuse"), Options.ScreenHint.IsEmpty());

	// HAIVEN_DEMO_ACCESS_TOKEN's lifetime comes from the token, not an assumed hour.
	const FDateTime Now(2026, 10, 20, 12, 0, 0);
	bool bFromToken = false;
	const int64 Exp = Now.ToUnixTimestamp() + 25 * 60;
	TestEqual(TEXT("a JWT's exp claim is its expiry"),
		HaivenLyraSignIn::DemoTokenExpiry(FakeJwt(FString::Printf(TEXT("{\"exp\":%lld,\"token_use\":\"access\"}"), Exp)), Now, bFromToken),
		FDateTime::FromUnixTimestamp(Exp));
	TestTrue(TEXT("the expiry came from the token"), bFromToken);
	TestEqual(TEXT("an opaque token gets the staging lifetime"),
		HaivenLyraSignIn::DemoTokenExpiry(TEXT("opaque-demo-value"), Now, bFromToken), Now + FTimespan::FromMinutes(30.0));
	TestFalse(TEXT("an opaque token's expiry is assumed"), bFromToken);
	HaivenLyraSignIn::DemoTokenExpiry(TEXT("a.!!!.c"), Now, bFromToken);
	TestFalse(TEXT("an unreadable payload falls back"), bFromToken);
	HaivenLyraSignIn::DemoTokenExpiry(FakeJwt(TEXT("{\"sub\":\"x\"}")), Now, bFromToken);
	TestFalse(TEXT("a JWT without exp falls back"), bFromToken);

	// The state machine, against a real Haiven subsystem with no browser.
	UHaivenLyraDemoSettings* Settings = GetMutableDefault<UHaivenLyraDemoSettings>();
	const TArray<FString> Saved = {
		Settings->BaseUrl, Settings->RealtimeUrl, Settings->AppId, Settings->ChannelId,
		Settings->IntegrationId, Settings->LoginEntryUrl, Settings->LoginRedirectUrl };
	const FString SavedToken = FPlatformMisc::GetEnvironmentVariable(TEXT("HAIVEN_DEMO_ACCESS_TOKEN"));
	const uint32 Port = FreeLoopbackPort();
	if (!TestTrue(TEXT("a free loopback port"), Port != 0)) return false;
	const FString Redirect = FString::Printf(TEXT("http://127.0.0.1:%u/haiven/lyra-test"), Port);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UHaivenSubsystem* Haiven = NewObject<UHaivenSubsystem>(GameInstance);
	// A local-player subsystem lives within a local player; Initialize is not run.
	ULocalPlayer* Player = NewObject<ULocalPlayer>(GEngine);
	UHaivenLyraDemoSubsystem* Demo = NewObject<UHaivenLyraDemoSubsystem>(Player);
	ON_SCOPE_EXIT
	{
		Demo->ClearExpiryCheck();
		Demo->UnbindHaiven();
		Haiven->Logout();
		Settings->BaseUrl = Saved[0];
		Settings->RealtimeUrl = Saved[1];
		Settings->AppId = Saved[2];
		Settings->ChannelId = Saved[3];
		Settings->IntegrationId = Saved[4];
		Settings->LoginEntryUrl = Saved[5];
		Settings->LoginRedirectUrl = Saved[6];
		FPlatformMisc::SetEnvironmentVar(TEXT("HAIVEN_DEMO_ACCESS_TOKEN"), *SavedToken);
	};
	Settings->BaseUrl = TEXT("http://127.0.0.1:9/v0");
	Settings->RealtimeUrl.Reset();
	Settings->IntegrationId = TEXT("lyra-test");
	Settings->LoginEntryUrl = TEXT("https://app.example.test/connect/game");
	Settings->LoginRedirectUrl = Redirect;
	Demo->Haiven = Haiven;
	Demo->BindHaiven();
	int32 Launches = 0;
	FString Launched;
	Haiven->SetLoginUrlLauncher([&Launches, &Launched](const FString& Url, FString&)
	{
		++Launches;
		Launched = Url;
		return true;
	});

	TestTrue(TEXT("starts signed out"), Demo->GetSignInState() == EHaivenLyraSignInState::SignedOut);
	TestTrue(TEXT("sign-in is offered"), Demo->CanSignIn());
	Demo->PressSignInAction();
	TestTrue(TEXT("pressing sign-in starts it"), Demo->GetSignInState() == EHaivenLyraSignInState::SigningIn);
	TestEqual(TEXT("the browser opens once"), Launches, 1);
	TestTrue(TEXT("the browser opens the configured login page"), Launched.StartsWith(Settings->LoginEntryUrl + TEXT("?")));
	TestTrue(TEXT("for the configured integration"), Launched.Contains(TEXT("integration_id=lyra-test")));
	TestTrue(TEXT("with the loopback redirect"),
		Launched.Contains(FString::Printf(TEXT("redirect_uri=http%%3A%%2F%%2F127.0.0.1%%3A%u%%2Fhaiven%%2Flyra-test"), Port)));
	TestTrue(TEXT("with the plugin's PKCE"), Launched.Contains(TEXT("code_challenge_method=S256")));
	TestTrue(TEXT("the plugin holds the transaction"), Haiven->IsGameLoginInProgress());
	TestEqual(TEXT("signing in reads as such"), Demo->GetSignInStatus(), FString(TEXT("SIGNING IN")));

	Demo->PressSignInAction();
	TestTrue(TEXT("pressing again cancels"), Demo->GetSignInState() == EHaivenLyraSignInState::SignedOut);
	TestFalse(TEXT("cancelling ends the plugin transaction"), Haiven->IsGameLoginInProgress());

	Demo->SignIn();
	Haiven->OnGameLoginFailed.Broadcast(TEXT("The handoff callback state is missing or does not match."));
	TestTrue(TEXT("a failure that left the login pending is not this login's"),
		Demo->GetSignInState() == EHaivenLyraSignInState::SigningIn);

	FHaivenGameCredential Credential;
	Credential.TokenType = TEXT("Bearer");
	Credential.ExpiresAtUtc = FDateTime::UtcNow() + FTimespan::FromMinutes(30.0);
	Haiven->OnGameLoginCompleted.Broadcast(Credential);
	TestTrue(TEXT("a completed login signs in"), Demo->GetSignInState() == EHaivenLyraSignInState::SignedIn);
	TestEqual(TEXT("the expiry is the exchange's"), Demo->GetSignInExpiresAtUtc(), Credential.ExpiresAtUtc);
	TestTrue(TEXT("the status header shows it"), Demo->GetStatusText().ToString().Contains(TEXT("Sign-in: SIGNED IN")));
	Demo->UpdateExpiry(Credential.ExpiresAtUtc - FTimespan::FromSeconds(1.0));
	TestTrue(TEXT("still signed in before the expiry"), Demo->GetSignInState() == EHaivenLyraSignInState::SignedIn);
	Demo->UpdateExpiry(Credential.ExpiresAtUtc);
	TestTrue(TEXT("expired at the expiry"), Demo->GetSignInState() == EHaivenLyraSignInState::Expired);
	TestFalse(TEXT("expiry explains itself"), Demo->GetSignInError().IsEmpty());
	TestFalse(TEXT("expiry ends the plugin's sign-in too"), Haiven->IsGameLoginInProgress());

	Demo->PressSignInAction();
	TestTrue(TEXT("retry after expiry signs in again"), Demo->GetSignInState() == EHaivenLyraSignInState::SigningIn);
	Haiven->CancelGameLogin();
	const FString Refused = TEXT("The handoff callback failed: access_denied");
	Haiven->OnGameLoginFailed.Broadcast(Refused);
	TestTrue(TEXT("a finished failure fails the sign-in"), Demo->GetSignInState() == EHaivenLyraSignInState::Failed);
	TestEqual(TEXT("the failure says why"), Demo->GetSignInError(), Refused);
	TestEqual(TEXT("the failure reads as such"), Demo->GetSignInStatus(), FString(TEXT("SIGN-IN FAILED")));

	Demo->SignIn();
	Haiven->OnGameLoginCompleted.Broadcast(Credential);
	Demo->PressSignInAction();
	TestTrue(TEXT("pressing while signed in signs out"), Demo->GetSignInState() == EHaivenLyraSignInState::SignedOut);
	FString Token;
	TestFalse(TEXT("signing out clears the bearer"), Haiven->GetAccessToken(Token));

	Settings->LoginEntryUrl.Reset();
	TestFalse(TEXT("no login page, no sign-in"), Demo->CanSignIn());
	Demo->SignIn();
	TestTrue(TEXT("an unconfigured sign-in fails visibly"), Demo->GetSignInState() == EHaivenLyraSignInState::Failed);
	TestEqual(TEXT("an unconfigured sign-in never opens the browser"), Launches, 4);

	// The HAIVEN_DEMO_ACCESS_TOKEN shortcut keeps working, on the token's own expiry.
	Settings->RealtimeUrl = TEXT("ws://127.0.0.1:9/v0/ws");
	Settings->AppId = TEXT("lyra-test");
	Settings->ChannelId = TEXT("general");
	const int64 Past = FDateTime::UtcNow().ToUnixTimestamp() - 60;
	FPlatformMisc::SetEnvironmentVar(TEXT("HAIVEN_DEMO_ACCESS_TOKEN"), *FakeJwt(FString::Printf(TEXT("{\"exp\":%lld}"), Past)));
	Demo->Connect();
	TestTrue(TEXT("an expired demo token is reported, not used"), Demo->GetSignInState() == EHaivenLyraSignInState::Expired);
	TestFalse(TEXT("an expired demo token is never handed to the plugin"), Haiven->GetAccessToken(Token));
	const int64 Future = FDateTime::UtcNow().ToUnixTimestamp() + 20 * 60;
	FPlatformMisc::SetEnvironmentVar(TEXT("HAIVEN_DEMO_ACCESS_TOKEN"), *FakeJwt(FString::Printf(TEXT("{\"exp\":%lld}"), Future)));
	Demo->Connect();
	TestTrue(TEXT("a live demo token signs in"), Demo->GetSignInState() == EHaivenLyraSignInState::SignedIn);
	TestEqual(TEXT("the demo expiry is the token's exp"), Demo->GetSignInExpiresAtUtc(), FDateTime::FromUnixTimestamp(Future));
	TestEqual(TEXT("the plugin's expiry is the token's exp, not an hour"),
		Haiven->GetHealth().AccessTokenExpiresAtUtc, FDateTime::FromUnixTimestamp(Future));
	Haiven->DisconnectRealtime();
	return true;
}

#endif
