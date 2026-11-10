#include "HaivenLyraDemoSubsystem.h"

#include "Blueprint/HaivenBlueprintRealtime.h"
#include "Blueprint/HaivenBlueprintRestOperations.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Async/Async.h"
#include "Blueprint/HaivenBlueprintTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "HaivenAnalytics.h"
#include "HaivenChat.h"
#include "HaivenClientTypes.h"
#include "HaivenLyraChatWidget.h"
#include "HaivenLyraDemoAnalytics.h"
#include "HaivenLyraDemoSettings.h"
#include "HaivenLyraHostLoss.h"
#include "HaivenLyraEntitlements.h"
#include "HaivenLyraMotdWidget.h"
#include "HaivenLyraPresentation.h"
#include "HaivenLyraSignIn.h"
#include "HaivenRewards.h"
#include "HaivenSubsystem.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogHaivenLyraDemo, Log, All);

/**
 * Enter opens the chat input while the game has keyboard focus. Lyra's input
 * is Enhanced Input driven by data assets, so a Slate pre-processor is the one
 * hook that needs no binary content.
 */
class FHaivenLyraChatInput : public IInputProcessor
{
public:
	explicit FHaivenLyraChatInput(UHaivenLyraDemoSubsystem* InOwner) : Owner(InOwner) {}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		UHaivenLyraDemoSubsystem* Demo = Owner.Get();
		if (!Demo || InKeyEvent.GetKey() != EKeys::Enter || Demo->IsChatFocused()) return false;
		return Demo->FocusChat();
	}

private:
	TWeakObjectPtr<UHaivenLyraDemoSubsystem> Owner;
};

namespace
{
	FText Describe(const FHaivenOutgoingChatMessage& Message)
	{
		switch (Message.State)
		{
		case EHaivenOutgoingChatState::Sending:
			return FText::Format(NSLOCTEXT("HaivenLyraDemo", "Sending", "you (sending): {0}"), FText::FromString(Message.Content));
		case EHaivenOutgoingChatState::Unconfirmed:
			return FText::Format(NSLOCTEXT("HaivenLyraDemo", "Unconfirmed", "you (resending on reconnect): {0}"), FText::FromString(Message.Content));
		case EHaivenOutgoingChatState::Accepted:
			return FText::Format(NSLOCTEXT("HaivenLyraDemo", "Accepted", "you: {0}"), FText::FromString(Message.Content));
		case EHaivenOutgoingChatState::Held:
			return FText::Format(NSLOCTEXT("HaivenLyraDemo", "Held", "you (held for review): {0}"), FText::FromString(Message.Content));
		case EHaivenOutgoingChatState::Removed:
			return FText::Format(NSLOCTEXT("HaivenLyraDemo", "Removed", "you (not shown: {1}): {0}"), FText::FromString(Message.Content), FText::FromString(Message.ModerationState));
		case EHaivenOutgoingChatState::Refused:
			return FText::Format(NSLOCTEXT("HaivenLyraDemo", "Refused", "you (refused: {1}): {0}"), FText::FromString(Message.Content), FText::FromString(Message.Error));
		}
		return FText::FromString(Message.Content);
	}
}

void UHaivenLyraDemoSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	HostLossState = MakeShared<FHaivenLyraHostLossState>();
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(
			this,
			&UHaivenLyraDemoSubsystem::HandleNetworkFailure);
	}
	ModerationStatus = HaivenLyraPresentation::ModerationReady;
	RewardStatus = GetDefault<UHaivenLyraDemoSettings>()->IsRewardConfigured()
		? HaivenLyraPresentation::RewardReady
		: HaivenLyraPresentation::RewardNotConfigured;
	const UGameInstance* GameInstance = GetLocalPlayer() ? GetLocalPlayer()->GetGameInstance() : nullptr;
	if (!GameInstance) return;
	Haiven = GameInstance->GetSubsystem<UHaivenSubsystem>();
	Events = GameInstance->GetSubsystem<UHaivenRealtimeEvents>();
	if (!Haiven) return;
	if (FHaivenLyraDemoAnalytics::IsEnabledValue(
		FPlatformMisc::GetEnvironmentVariable(TEXT("HAIVEN_DEMO_ANALYTICS"))))
	{
		DemoAnalytics = MakeShared<FHaivenLyraDemoAnalytics>();
	}

	BindHaiven();

	if (FSlateApplication::IsInitialized())
	{
		ChatInput = MakeShared<FHaivenLyraChatInput>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(ChatInput);
	}
	Connect();
}

void UHaivenLyraDemoSubsystem::Deinitialize()
{
	if (GEngine && NetworkFailureHandle.IsValid())
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
	}
	NetworkFailureHandle.Reset();
	HostLossState.Reset();
	if (ChatInput && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(ChatInput);
	}
	ChatInput.Reset();
	ClearExpiryCheck();
	CancelPendingRealtimeConnect();
	DetachAnalytics();
	UnbindHaiven();
	Super::Deinitialize();
}

void UHaivenLyraDemoSubsystem::BindHaiven()
{
	if (Events)
	{
		Events->OnChatMessage.AddUniqueDynamic(this, &UHaivenLyraDemoSubsystem::HandleChatMessage);
		Events->OnEntitlementsUpdated.AddUniqueDynamic(this, &UHaivenLyraDemoSubsystem::HandleEntitlementsUpdated);
	}
	if (!Haiven) return;
	Haiven->OnOutgoingChatChanged.AddUniqueDynamic(this, &UHaivenLyraDemoSubsystem::HandleOutgoingChat);
	Haiven->OnRealtimeStateChanged.AddUniqueDynamic(this, &UHaivenLyraDemoSubsystem::HandleRealtimeState);
	Haiven->OnMotdChanged.AddUniqueDynamic(this, &UHaivenLyraDemoSubsystem::HandleMotdChanged);
	Haiven->OnMotdLoadFailed.AddUniqueDynamic(this, &UHaivenLyraDemoSubsystem::HandleMotdLoadFailed);
	Haiven->OnGameLoginCompleted.AddUniqueDynamic(this, &UHaivenLyraDemoSubsystem::HandleGameLoginCompleted);
	Haiven->OnGameLoginFailed.AddUniqueDynamic(this, &UHaivenLyraDemoSubsystem::HandleGameLoginFailed);
	Haiven->OnAuthExpired.AddUniqueDynamic(this, &UHaivenLyraDemoSubsystem::HandleAuthExpired);
}

void UHaivenLyraDemoSubsystem::UnbindHaiven()
{
	if (Events)
	{
		Events->OnChatMessage.RemoveAll(this);
		Events->OnEntitlementsUpdated.RemoveAll(this);
	}
	if (Haiven)
	{
		Haiven->OnOutgoingChatChanged.RemoveAll(this);
		Haiven->OnRealtimeStateChanged.RemoveAll(this);
		Haiven->OnMotdChanged.RemoveAll(this);
		Haiven->OnMotdLoadFailed.RemoveAll(this);
		Haiven->OnGameLoginCompleted.RemoveAll(this);
		Haiven->OnGameLoginFailed.RemoveAll(this);
		Haiven->OnAuthExpired.RemoveAll(this);
	}
}

void UHaivenLyraDemoSubsystem::Connect()
{
	CancelPendingRealtimeConnect();
	const UHaivenLyraDemoSettings* Settings = GetDefault<UHaivenLyraDemoSettings>();
	ChannelId = Settings->ChannelId;

	if (Settings->BaseUrl.IsEmpty())
	{
		MotdState.PresentFailure();
		ApplyMotdPresentation();
		UE_LOG(LogHaivenLyraDemo, Warning, TEXT("Haiven Lyra Demo is not configured: set BaseUrl in Project Settings."));
		return;
	}

	FHaivenClientOptions Options;
	Options.BaseUrl = Settings->BaseUrl;
	Options.RealtimeUrl = Settings->RealtimeUrl;
	Options.AppId = Settings->AppId;
	Haiven->Configure(Options);

	// MOTD is a public request. Start it before checking any bearer or realtime
	// setting so signed-out players get the same presentation surface.
	if (Settings->IntegrationId.IsEmpty())
	{
		MotdState.PresentFailure();
		ApplyMotdPresentation();
		UE_LOG(LogHaivenLyraDemo, Warning, TEXT("Haiven Lyra Demo MOTD is not configured: set IntegrationId in Project Settings."));
	}
	else
	{
		StartMotdRead();
	}

	if (!Settings->IsRealtimeConfigured())
	{
		UE_LOG(LogHaivenLyraDemo, Warning, TEXT("Haiven Lyra Demo chat is not configured: set BaseUrl, RealtimeUrl, AppId and ChannelId in Project Settings."));
		return;
	}

	// One connection serves every local player; the first player to arrive opens
	// it, and a later one shares its sign-in.
	if (Haiven->GetRealtimeState() != EHaivenRealtimeState::Disconnected)
	{
		FString Shared;
		if (Haiven->GetAccessToken(Shared)) EnterSignedIn(Haiven->GetHealth().AccessTokenExpiresAtUtc);
		return;
	}

	// The shortcut for rehearsals: a bearer minted elsewhere. Sign in with Haiven
	// replaces it, and neither is ever written to disk or a log.
	const FString Token = FPlatformMisc::GetEnvironmentVariable(TEXT("HAIVEN_DEMO_ACCESS_TOKEN"));
	if (Token.IsEmpty())
	{
		UE_LOG(LogHaivenLyraDemo, Log, TEXT("HAIVEN_DEMO_ACCESS_TOKEN is not set; chat connects after Sign in with Haiven."));
		return;
	}

	// Environment shortcuts must carry separate expiry metadata. The bearer is
	// opaque: do not decode a JWT-shaped value or assume a lifetime.
	const FDateTime Now = FDateTime::UtcNow();
	FDateTime ExpiresAtUtc;
	const FString Expiry = FPlatformMisc::GetEnvironmentVariable(TEXT("HAIVEN_DEMO_ACCESS_TOKEN_EXPIRES_AT"));
	if (!HaivenLyraSignIn::DemoTokenExpiryMetadata(Expiry, ExpiresAtUtc))
	{
		SetSignInState(EHaivenLyraSignInState::Failed,
			TEXT("HAIVEN_DEMO_ACCESS_TOKEN_EXPIRES_AT must be an ISO-8601 UTC instant. The bearer remains opaque."));
		return;
	}
	if (ExpiresAtUtc <= Now)
	{
		SetSignInState(EHaivenLyraSignInState::Expired,
			TEXT("HAIVEN_DEMO_ACCESS_TOKEN has expired. Sign in with Haiven, or relaunch with fresh token metadata."));
		return;
	}
	Haiven->SetAccessToken(Token, ExpiresAtUtc);
	// SetAccessToken is an account boundary: the plugin drops every REST read
	// started before it, the MOTD read above included, without completing it.
	// The MOTD is public, so read it again rather than stay on "loading".
	if (!Settings->IntegrationId.IsEmpty()) StartMotdRead();
	EnterSignedIn(ExpiresAtUtc);
	// Connect on the next tick, not here: this runs while the engine starts or
	// PIE loads its world, and that load holds the game thread for seconds. The
	// server closes a socket whose first frame (auth, sent from the game thread)
	// has not arrived within 5 seconds, and the plugin does not reconnect.
	PendingRealtimeConnect = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
	{
		PendingRealtimeConnect.Reset();
		// Only if nothing has replaced or cleared the token in the meantime.
		FString Current;
		if (Haiven && Haiven->GetRealtimeState() == EHaivenRealtimeState::Disconnected && Haiven->GetAccessToken(Current))
		{
			if (DemoAnalytics) DemoAnalytics->Connecting();
			Haiven->ConnectRealtime();
		}
		return false;
	}));
}

void UHaivenLyraDemoSubsystem::CancelPendingRealtimeConnect()
{
	if (!PendingRealtimeConnect.IsValid()) return;
	FTSTicker::GetCoreTicker().RemoveTicker(PendingRealtimeConnect);
	PendingRealtimeConnect.Reset();
}

void UHaivenLyraDemoSubsystem::StartMotdRead()
{
	MotdRequestId = FString::Printf(TEXT("lyra-motd-%llu"), ++MotdRequestSequence);
	MotdState.LoadStarted(MotdRequestId);
	ApplyMotdPresentation();
	Haiven->LoadMotd(GetDefault<UHaivenLyraDemoSettings>()->IntegrationId);
}

bool UHaivenLyraDemoSubsystem::CanSignIn() const
{
	return Haiven && GetDefault<UHaivenLyraDemoSettings>()->IsSignInConfigured();
}

void UHaivenLyraDemoSubsystem::SignIn()
{
	if (!Haiven) return;
	const UHaivenLyraDemoSettings* Settings = GetDefault<UHaivenLyraDemoSettings>();
	if (!Settings->IsSignInConfigured())
	{
		SetSignInState(EHaivenLyraSignInState::Failed,
			TEXT("Sign in with Haiven is not configured: set BaseUrl, IntegrationId and LoginEntryUrl in Project Settings."));
		Note(FText::FromString(SignInError));
		return;
	}
	FString AuthorizationUrl;
	FString Error;
	if (!Haiven->BeginLoopbackGameLogin(HaivenLyraSignIn::LoginOptions(*Settings), AuthorizationUrl, Error))
	{
		SetSignInState(EHaivenLyraSignInState::Failed, Error);
		Note(FText::Format(NSLOCTEXT("HaivenLyraDemo", "SignInNotStarted", "sign-in could not start: {0}"), FText::FromString(Error)));
		return;
	}
	SetSignInState(EHaivenLyraSignInState::SigningIn);
	Note(NSLOCTEXT("HaivenLyraDemo", "SignInStarted", "Haiven will open to finish sign-in. Return to the game when it is complete."));
}

void UHaivenLyraDemoSubsystem::CancelSignIn()
{
	if (SignInState != EHaivenLyraSignInState::SigningIn) return;
	if (Haiven) Haiven->CancelGameLogin();
	FString Kept;
	if (Haiven && Haiven->GetAccessToken(Kept)) SetSignInState(EHaivenLyraSignInState::SignedIn);
	else SetSignInState(EHaivenLyraSignInState::SignedOut);
	Note(NSLOCTEXT("HaivenLyraDemo", "SignInCancelled", "Sign-in was cancelled. No account was connected."));
}

void UHaivenLyraDemoSubsystem::SignOut()
{
	ClearExpiryCheck();
	CancelPendingRealtimeConnect();
	DetachAnalytics();
	if (Haiven) Haiven->Logout();
	AccountName.Reset();
	ActiveEntitledItems.Reset();
	AnnouncedItems.Reset();
	SignInExpiresAtUtc = FDateTime();
	SetSignInState(EHaivenLyraSignInState::SignedOut);
	Note(NSLOCTEXT("HaivenLyraDemo", "SignedOut", "Signed out of Haiven on this device."));
}

void UHaivenLyraDemoSubsystem::PressSignInAction()
{
	switch (SignInState)
	{
	case EHaivenLyraSignInState::SigningIn: CancelSignIn(); return;
	case EHaivenLyraSignInState::SignedIn: SignOut(); return;
	case EHaivenLyraSignInState::SignedOut:
	case EHaivenLyraSignInState::Failed:
	case EHaivenLyraSignInState::Expired:
		SignIn();
		return;
	}
}

FString UHaivenLyraDemoSubsystem::GetSignInStatus() const
{
	return HaivenLyraPresentation::SignInStatus(SignInState);
}

void UHaivenLyraDemoSubsystem::HandleGameLoginCompleted(const FHaivenGameCredential& Credential)
{
	// The bearer stays in the Haiven subsystem; only its expiry is kept here.
	EnterSignedIn(Credential.ExpiresAtUtc);
	Note(NSLOCTEXT("HaivenLyraDemo", "SignInCompleted", "Signed in with Haiven."));
	if (Haiven && GetDefault<UHaivenLyraDemoSettings>()->IsRealtimeConfigured()
		&& Haiven->GetRealtimeState() == EHaivenRealtimeState::Disconnected)
	{
		if (DemoAnalytics) DemoAnalytics->Connecting();
		Haiven->ConnectRealtime();
	}
}

void UHaivenLyraDemoSubsystem::HandleGameLoginFailed(const FString& Error)
{
	// A failure that left the login pending (another login's callback) is not this one's.
	if (SignInState != EHaivenLyraSignInState::SigningIn || (Haiven && Haiven->IsGameLoginInProgress())) return;
	SetSignInState(EHaivenLyraSignInState::Failed, Error);
	Note(FText::Format(NSLOCTEXT("HaivenLyraDemo", "SignInFailed", "sign-in failed: {0}"), FText::FromString(Error)));
}

void UHaivenLyraDemoSubsystem::HandleAuthExpired()
{
	if (SignInState != EHaivenLyraSignInState::SignedIn) return;
	// The listener runs inside the socket's re-auth; end the session after it unwinds.
	TWeakObjectPtr<UHaivenLyraDemoSubsystem> WeakThis(this);
	AsyncTask(ENamedThreads::GameThread, [WeakThis]()
	{
		if (WeakThis.IsValid() && WeakThis->SignInState == EHaivenLyraSignInState::SignedIn) WeakThis->Expire();
	});
}

void UHaivenLyraDemoSubsystem::EnterSignedIn(const FDateTime& ExpiresAtUtc)
{
	SignInExpiresAtUtc = ExpiresAtUtc;
	SetSignInState(EHaivenLyraSignInState::SignedIn);
	ScheduleExpiryCheck();
	AttachAnalytics();
}

void UHaivenLyraDemoSubsystem::ScheduleExpiryCheck()
{
	ClearExpiryCheck();
	const double Seconds = FMath::Max(0.5, (SignInExpiresAtUtc - FDateTime::UtcNow()).GetTotalSeconds() + 0.5);
	ExpiryCheck = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateWeakLambda(this, [this](float)
		{
			ExpiryCheck.Reset();
			UpdateExpiry(FDateTime::UtcNow());
			if (SignInState == EHaivenLyraSignInState::SignedIn) ScheduleExpiryCheck();
			return false;
		}),
		static_cast<float>(Seconds));
}

void UHaivenLyraDemoSubsystem::ClearExpiryCheck()
{
	if (!ExpiryCheck.IsValid()) return;
	FTSTicker::GetCoreTicker().RemoveTicker(ExpiryCheck);
	ExpiryCheck.Reset();
}

void UHaivenLyraDemoSubsystem::UpdateExpiry(const FDateTime& Now)
{
	if (SignInState == EHaivenLyraSignInState::SignedIn && Now >= SignInExpiresAtUtc) Expire();
}

void UHaivenLyraDemoSubsystem::Expire()
{
	// No refresh exists for a game login: clear the bearer, close chat, ask again.
	ClearExpiryCheck();
	DetachAnalytics();
	if (Haiven) Haiven->Logout();
	AccountName.Reset();
	ActiveEntitledItems.Reset();
	AnnouncedItems.Reset();
	SetSignInState(EHaivenLyraSignInState::Expired, TEXT("Your Haiven session expired. Sign in again to continue."));
	Note(FText::FromString(SignInError));
}

void UHaivenLyraDemoSubsystem::SetSignInState(EHaivenLyraSignInState State, const FString& Error)
{
	SignInState = State;
	SignInError = Error;
	ApplyStatus();
	OnSignInStateChanged.Broadcast(State);
}

void UHaivenLyraDemoSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);
	if (!NewPlayerController || !NewPlayerController->IsLocalController()) return;
	if (HostLossState && NewPlayerController->GetWorld()
		&& NewPlayerController->GetWorld()->GetNetMode() == NM_Client)
	{
		// A new client world is a new match; a stale terminal banner must not follow it.
		HostLossState->Reset();
	}
	const UHaivenLyraDemoSettings* Settings = GetDefault<UHaivenLyraDemoSettings>();
	if (Settings->bShowChatPanel)
	{
		// Travel removes viewport widgets, so each new controller gets a fresh panel.
		const TSubclassOf<UHaivenLyraChatWidget> RequestedClass = Settings->ResolveChatWidgetClass();
		const bool bRequestedDesignerClass = RequestedClass.Get() != UHaivenLyraChatWidget::StaticClass();
		Panel = CreateWidget<UHaivenLyraChatWidget>(NewPlayerController, RequestedClass);
		bool bRejectedDesignerBindings = false;
		if (Panel && bRequestedDesignerClass)
		{
			Panel->TakeWidget();
			if (!Panel->HasDesignerBindings())
			{
				UE_LOG(
					LogHaivenLyraDemo,
					Warning,
					TEXT("Chat widget class '%s' does not bind a ScrollBox named Log and an EditableTextBox named Input; using the code-built panel."),
					*RequestedClass->GetPathName());
				Panel = nullptr;
				bRejectedDesignerBindings = true;
			}
		}
		if (!Panel && bRequestedDesignerClass)
		{
			if (!bRejectedDesignerBindings)
			{
				UE_LOG(
					LogHaivenLyraDemo,
					Warning,
					TEXT("Could not create chat widget class '%s'; using the code-built panel."),
					*RequestedClass->GetPathName());
			}
			Panel = CreateWidget<UHaivenLyraChatWidget>(NewPlayerController, UHaivenLyraChatWidget::StaticClass());
		}
		if (Panel)
		{
			Panel->AddToPlayerScreen(100);
			if (Panel->UsesCodeBuiltTree())
			{
				const float ViewportScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(NewPlayerController), 0.01f);
				Panel->SetAnchorsInViewport(FAnchors(0.0f, 0.0f));
				Panel->SetPositionInViewport(FVector2D::ZeroVector, false);
				Panel->SetAlignmentInViewport(FVector2D::ZeroVector);
				Panel->SetDesiredSizeInViewport(UWidgetLayoutLibrary::GetViewportSize(NewPlayerController) / ViewportScale);
			}
			if (Haiven) HandleRealtimeState(Haiven->GetRealtimeState());
			ApplyStatus();
			if (HostLossState && HostLossState->IsActive())
			{
				const FText Status = HostLossState->StatusText();
				Note(Status);
				FString SmokeRole;
				if (FParse::Value(FCommandLine::Get(), TEXT("HaivenEosSmokeRole="), SmokeRole)
					&& SmokeRole.Equals(TEXT("guest"), ESearchCase::IgnoreCase))
				{
					UE_LOG(LogHaivenLyraDemo, Display,
						TEXT("HAIVEN_EOS_SMOKE host_loss_recovered status=\"%s\""),
						*Status.ToString());
				}
			}
		}
	}
	bChatFocused = false;

	if (Settings->bShowMotdPanel)
	{
		const TSubclassOf<UHaivenLyraMotdWidget> RequestedClass = Settings->ResolveMotdWidgetClass();
		const bool bRequestedDesignerClass = RequestedClass.Get() != UHaivenLyraMotdWidget::StaticClass();
		MotdPanel = CreateWidget<UHaivenLyraMotdWidget>(NewPlayerController, RequestedClass);
		bool bRejectedDesignerBindings = false;
		if (MotdPanel && bRequestedDesignerClass)
		{
			MotdPanel->TakeWidget();
			if (!MotdPanel->HasDesignerBindings())
			{
				UE_LOG(
					LogHaivenLyraDemo,
					Warning,
					TEXT("MOTD widget class '%s' does not bind Status, Title, Body, Action and Dismiss controls; using the code-built panel."),
					*RequestedClass->GetPathName());
				MotdPanel = nullptr;
				bRejectedDesignerBindings = true;
			}
		}
		if (!MotdPanel && bRequestedDesignerClass)
		{
			if (!bRejectedDesignerBindings)
			{
				UE_LOG(LogHaivenLyraDemo, Warning, TEXT("Could not create MOTD widget class '%s'; using the code-built panel."),
					*RequestedClass->GetPathName());
			}
			MotdPanel = CreateWidget<UHaivenLyraMotdWidget>(NewPlayerController, UHaivenLyraMotdWidget::StaticClass());
		}
		if (MotdPanel)
		{
			// Keep the MOTD surface beneath the main transparent overlay. Its root is
			// SelfHitTestInvisible, but the lower z-order also guarantees the Chat and
			// Rewards rail controls remain the first interactive targets.
			MotdPanel->AddToPlayerScreen(90);
			if (MotdPanel->UsesCodeBuiltTree())
			{
				const float ViewportScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(NewPlayerController), 0.01f);
				MotdPanel->SetAnchorsInViewport(FAnchors(0.0f, 0.0f));
				MotdPanel->SetPositionInViewport(FVector2D::ZeroVector, false);
				MotdPanel->SetAlignmentInViewport(FVector2D::ZeroVector);
				MotdPanel->SetDesiredSizeInViewport(UWidgetLayoutLibrary::GetViewportSize(NewPlayerController) / ViewportScale);
			}
			ApplyMotdPresentation();
		}
	}
}

void UHaivenLyraDemoSubsystem::CollapseMainPanels()
{
	if (Panel) Panel->CollapseDrawers();
}

void UHaivenLyraDemoSubsystem::CollapseMotdPanel()
{
	if (MotdPanel) MotdPanel->CollapseDrawer();
}

void UHaivenLyraDemoSubsystem::Send(const FString& Content)
{
	if (!Haiven || ChannelId.IsEmpty()) return;
	FHaivenOutgoingChatMessage Message;
	FString Error;
	if (!Haiven->SendChatMessage(ChannelId, Content, false, FString(), Message, Error))
	{
		Note(FText::Format(NSLOCTEXT("HaivenLyraDemo", "SendFailed", "not sent: {0}"), FText::FromString(Error)));
		return;
	}
	HandleOutgoingChat(Message);
	ReleaseChat();
}

void UHaivenLyraDemoSubsystem::RequestPlayGameplay()
{
	CollapseMainPanels();
	if (!OnPlayGameplayRequested.IsBound())
	{
		Note(NSLOCTEXT("HaivenLyraDemo", "PlayUnavailable", "PLAY needs the Lyra inventory plugin to start a match."));
		return;
	}
	OnPlayGameplayRequested.Broadcast();
}

bool UHaivenLyraDemoSubsystem::FocusChat()
{
	APlayerController* Controller = GetLocalPlayer() ? GetLocalPlayer()->GetPlayerController(nullptr) : nullptr;
	if (!Panel || !Panel->IsInViewport() || !Controller) return false;
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(Panel->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Controller->SetInputMode(Mode);
	Controller->SetShowMouseCursor(true);
	Panel->FocusInput();
	bChatFocused = true;
	return true;
}

void UHaivenLyraDemoSubsystem::ReleaseChat()
{
	APlayerController* Controller = GetLocalPlayer() ? GetLocalPlayer()->GetPlayerController(nullptr) : nullptr;
	if (Controller)
	{
		Controller->SetInputMode(FInputModeGameOnly());
		Controller->SetShowMouseCursor(false);
	}
	bChatFocused = false;
}

void UHaivenLyraDemoSubsystem::HandleChatMessage(const FHaivenChatMessageV0& Message)
{
	if (Message.ChannelId != ChannelId || !Message.bHasContent) return;
	// Our own sends already have a line, keyed by client message id.
	if (Message.bHasClientMessageId && Haiven)
	{
		for (const FHaivenOutgoingChatMessage& Outgoing : Haiven->GetChatOutbox())
		{
			if (Outgoing.ClientMessageId == Message.ClientMessageId) return;
		}
	}
	FString Name = TEXT("someone");
	if (Message.bHasAuthor)
	{
		Name = !Message.Author.DisplayName.IsEmpty() ? Message.Author.DisplayName : Message.Author.Username;
	}
	Note(FText::Format(NSLOCTEXT("HaivenLyraDemo", "Incoming", "{0}: {1}"), FText::FromString(Name), FText::FromString(Message.Content)));
}

void UHaivenLyraDemoSubsystem::HandleOutgoingChat(const FHaivenOutgoingChatMessage& Message)
{
	if (Message.ChannelId != ChannelId) return;
	if (Panel) Panel->SetKeyedLine(Message.ClientMessageId, Describe(Message));
	ModerationStatus = HaivenLyraPresentation::ModerationStatus(Message.State);
	ApplyStatus();
}

void UHaivenLyraDemoSubsystem::HandleRealtimeState(EHaivenRealtimeState State)
{
	switch (State)
	{
	case EHaivenRealtimeState::Ready:
		if (DemoAnalytics) DemoAnalytics->SessionStarted();
		Note(NSLOCTEXT("HaivenLyraDemo", "Ready", "Haiven chat connected. Press Enter to talk."));
		RefreshAccount();
		RefreshEntitlements();
		break;
	case EHaivenRealtimeState::Disconnected:
		if (DemoAnalytics) DemoAnalytics->Disconnected();
		Note(NSLOCTEXT("HaivenLyraDemo", "Offline", "Haiven chat offline."));
		break;
	default:
		break;
	}
	ApplyStatus();
}

void UHaivenLyraDemoSubsystem::RefreshAccount()
{
	if (!Haiven || !GetLocalPlayer()) return;
	UHaivenRestGetUsersMeAsyncAction* Read = UHaivenRestGetUsersMeAsyncAction::RestGetUsersMe(GetLocalPlayer(), Haiven);
	if (!Read) return;
	Read->Completed.AddDynamic(this, &UHaivenLyraDemoSubsystem::HandleAccount);
	Read->Failed.AddDynamic(this, &UHaivenLyraDemoSubsystem::HandleAccountFailed);
	Read->Activate();
}

void UHaivenLyraDemoSubsystem::HandleAccount(const FHaivenHttpResult& Result, const FHaivenRestCurrentUserSessionResponse& Response)
{
	const FString Name = HaivenLyraPresentation::AccountName(Response);
	if (Name == AccountName) return;
	AccountName = Name;
	if (!Name.IsEmpty())
	{
		Note(FText::Format(NSLOCTEXT("HaivenLyraDemo", "SignedIn", "signed in as {0}"), FText::FromString(Name)));
	}
	ApplyStatus();
}

void UHaivenLyraDemoSubsystem::HandleAccountFailed(const FHaivenHttpResult& Result)
{
	Note(FText::Format(NSLOCTEXT("HaivenLyraDemo", "AccountFailed", "account unavailable: {0}"),
		FText::FromString(Result.Error.IsEmpty() ? FString::FromInt(Result.StatusCode) : Result.Error)));
}

void UHaivenLyraDemoSubsystem::ClaimReward()
{
	const UHaivenLyraDemoSettings* Settings = GetDefault<UHaivenLyraDemoSettings>();
	if (!Settings->IsRewardConfigured())
	{
		Note(NSLOCTEXT("HaivenLyraDemo", "RewardNotConfigured", "reward not configured: set IntegrationId and RewardRef in Project Settings."));
		return;
	}
	if (bClaimInFlight || !Haiven || !GetLocalPlayer()) return;
	UHaivenClaimRewardAsyncAction* Claim =
		UHaivenClaimRewardAsyncAction::ClaimReward(GetLocalPlayer(), Haiven, Settings->IntegrationId, Settings->RewardRef);
	if (!Claim) return;
	Claim->Claimed.AddDynamic(this, &UHaivenLyraDemoSubsystem::HandleRewardOutcome);
	Claim->Refused.AddDynamic(this, &UHaivenLyraDemoSubsystem::HandleRewardOutcome);
	Claim->Failed.AddDynamic(this, &UHaivenLyraDemoSubsystem::HandleRewardOutcome);
	bClaimInFlight = true;
	RewardStatus = HaivenLyraPresentation::RewardClaiming;
	ApplyStatus();
	// May finish synchronously (for example, when nobody is signed in); the handler resets the state.
	Claim->Activate();
}

void UHaivenLyraDemoSubsystem::HandleRewardOutcome(const FHaivenRewardClaimOutcome& Outcome)
{
	bClaimInFlight = false;
	RewardStatus = HaivenLyraPresentation::RewardStatus(Outcome);
	const bool bClaimed = Outcome.Result == EHaivenRewardClaimResult::Claimed
		|| Outcome.Result == EHaivenRewardClaimResult::AlreadyRecorded;
	if (bClaimed)
	{
		Note(FText::Format(NSLOCTEXT("HaivenLyraDemo", "RewardClaimed", "reward claimed: {0}"),
			FText::FromString(GetDefault<UHaivenLyraDemoSettings>()->RewardRef)));
	}
	else
	{
		Note(FText::Format(NSLOCTEXT("HaivenLyraDemo", "RewardOutcome", "reward claim: {0}"),
			FText::FromString(RewardStatus.ToLower())));
	}
	ApplyStatus();
	OnRewardClaimed.Broadcast(Outcome);
}

void UHaivenLyraDemoSubsystem::AttachAnalytics()
{
	if (!DemoAnalytics || !Haiven) return;
	if (AnalyticsBridge)
	{
		DemoAnalytics->SessionEnded();
		DemoAnalytics->SetEnqueue({});
		AnalyticsBridge->Shutdown();
		AnalyticsBridge.Reset();
	}

	const UHaivenLyraDemoSettings* Settings = GetDefault<UHaivenLyraDemoSettings>();
	if (Settings->IntegrationId.IsEmpty()) return;
	Haiven::RestTypes::AnalyticsContext Context;
	Context.Environment = "development";
	Context.IntegrationId = TCHAR_TO_UTF8(*Settings->IntegrationId);
	Context.Platform = "unreal";
	Haiven::RestTypes::AnalyticsSDK Sdk;
	Sdk.Name = "haiven-unreal";
	Sdk.Version = "0.0.0";
	Context.Sdk = MoveTemp(Sdk);
	AnalyticsBridge = FHaivenAnalytics::CreateForSubsystem(Haiven, MoveTemp(Context));
	TWeakPtr<FHaivenAnalytics, ESPMode::ThreadSafe> WeakBridge = AnalyticsBridge;
	DemoAnalytics->SetEnqueue([WeakBridge](const FString& Type, const FString& PropertiesJson)
	{
		const TSharedPtr<FHaivenAnalytics, ESPMode::ThreadSafe> Bridge = WeakBridge.Pin();
		return Bridge && Bridge->TryEnqueue(Type, PropertiesJson);
	});
	DemoAnalytics->GameLaunched();
	const EHaivenRealtimeState State = Haiven->GetRealtimeState();
	if (State == EHaivenRealtimeState::Ready) DemoAnalytics->SessionStarted();
	else if (State == EHaivenRealtimeState::Connecting || State == EHaivenRealtimeState::Authenticating)
		DemoAnalytics->Connecting();
}

void UHaivenLyraDemoSubsystem::DetachAnalytics()
{
	if (DemoAnalytics) DemoAnalytics->SessionEnded();
	if (DemoAnalytics) DemoAnalytics->SetEnqueue({});
	if (AnalyticsBridge)
	{
		TSharedPtr<FHaivenAnalytics, ESPMode::ThreadSafe> Bridge = MoveTemp(AnalyticsBridge);
		// Keep the bridge alive through its asynchronous best-effort flush. Logout
		// may still shut it down at the credential boundary, which is intentionally
		// allowed to win over telemetry during account changes.
		Bridge->Flush([Bridge] { Bridge->Shutdown(); });
	}
}

FString UHaivenLyraDemoSubsystem::GetConnectionStatus() const
{
	return HaivenLyraPresentation::ConnectionStatus(Haiven ? Haiven->GetRealtimeState() : EHaivenRealtimeState::Disconnected);
}

bool UHaivenLyraDemoSubsystem::CanClaimReward() const
{
	return Haiven && !bClaimInFlight && GetDefault<UHaivenLyraDemoSettings>()->IsRewardConfigured();
}

FText UHaivenLyraDemoSubsystem::GetStatusText() const
{
	const FText Base = HaivenLyraPresentation::StatusLines(
		AccountName,
		GetSignInStatus(),
		GetConnectionStatus(),
		ModerationStatus,
		RewardStatus);
	const FText HostLoss = GetHostLossStatus();
	return HostLoss.IsEmpty()
		? Base
		: FText::Format(NSLOCTEXT("HaivenLyraDemo", "StatusWithHostLoss", "{0}\n{1}"), Base, HostLoss);
}

FText UHaivenLyraDemoSubsystem::GetHostLossStatus() const
{
	return HostLossState ? HostLossState->StatusText() : FText::GetEmpty();
}

void UHaivenLyraDemoSubsystem::HandleNetworkFailure(
	UWorld* World,
	UNetDriver* NetDriver,
	ENetworkFailure::Type FailureType,
	const FString& ErrorString)
{
	(void)NetDriver;
	(void)ErrorString;
	const bool bWasClient = World && World->GetNetMode() == NM_Client;
	if (!HostLossState || !HostLossState->Record(bWasClient, FailureType)) return;

	// Do not log ErrorString: an online provider may include connection metadata.
	UE_LOG(LogHaivenLyraDemo, Warning, TEXT("listen host disconnected; match ended (network failure %d)"),
		static_cast<int32>(FailureType));
	ApplyStatus();
	Note(HostLossState->StatusText());
}

void UHaivenLyraDemoSubsystem::ApplyStatus()
{
	// Verbose, so off by default: keep headless diagnostics state-only and never
	// serialize the player-facing header, which includes the signed-in account name.
	UE_LOG(LogHaivenLyraDemo, Verbose,
		TEXT("status: sign-in=%s | realtime=%s | moderation=%s | reward=%s"),
		*GetSignInStatus(), *GetConnectionStatus(), *ModerationStatus, *RewardStatus);
	if (Panel)
	{
		Panel->SetStatus(GetStatusText());
		Panel->SetRewardAction(GetDefault<UHaivenLyraDemoSettings>()->IsRewardConfigured(), CanClaimReward());
		Panel->SetSignInAction(
			CanSignIn() || SignInState == EHaivenLyraSignInState::SignedIn,
			HaivenLyraPresentation::SignInActionLabel(SignInState));
	}
	OnStatusChanged.Broadcast();
}

void UHaivenLyraDemoSubsystem::Note(const FText& Line) const
{
	if (Panel) Panel->AddLine(Line);
}

void UHaivenLyraDemoSubsystem::HandleMotdChanged(bool bHasMessage, const FHaivenMotd& Message)
{
	if (bHasMessage) MotdState.RealtimePresent(Message);
	else MotdState.RealtimeClear();
	ApplyMotdPresentation();
}

void UHaivenLyraDemoSubsystem::HandleMotdLoadFailed()
{
	MotdState.RestFailure(MotdRequestId);
	ApplyMotdPresentation();
}

void UHaivenLyraDemoSubsystem::ApplyMotdPresentation() const
{
	const FHaivenLyraMotdProjection& Projection = MotdState.GetProjection();
	UE_LOG(LogHaivenLyraDemo, Verbose, TEXT("motd: %s%s"), *HaivenLyraMotdStatusName(Projection.Status),
		Projection.Message.IsSet() ? *(TEXT(" ") + Projection.Message->Title) : TEXT(""));
	if (!MotdPanel) return;
	MotdPanel->PresentProjection(MotdState.GetProjection());
}

void UHaivenLyraDemoSubsystem::RequestMotdAction(const FString& HttpsUrl)
{
	if (UHaivenSubsystem::IsValidMotdActionUrl(HttpsUrl)) OnMotdActionRequested.Broadcast(HttpsUrl);
}

void UHaivenLyraDemoSubsystem::DismissMotd()
{
	if (MotdState.Dismiss()) ApplyMotdPresentation();
	if (Haiven) Haiven->DismissMotd();
}

void UHaivenLyraDemoSubsystem::RefreshEntitlements()
{
	if (!Haiven || !GetLocalPlayer()) return;
	UHaivenRestGetUsersMeEntitlementsAsyncAction* Read =
		UHaivenRestGetUsersMeEntitlementsAsyncAction::RestGetUsersMeEntitlements(GetLocalPlayer(), Haiven);
	if (!Read) return;
	Read->Completed.AddDynamic(this, &UHaivenLyraDemoSubsystem::HandleEntitlements);
	Read->Failed.AddDynamic(this, &UHaivenLyraDemoSubsystem::HandleEntitlementsFailed);
	Read->Activate();
}

void UHaivenLyraDemoSubsystem::HandleEntitlementsUpdated(const FHaivenEntitlementsUpdatedV0& Payload)
{
	// The push is a refresh hint: it carries a projection version, not the entitlements.
	RefreshEntitlements();
}

void UHaivenLyraDemoSubsystem::HandleEntitlements(const FHaivenHttpResult& Result, const FHaivenRestAccountEntitlementsResponse& Response)
{
	ActiveEntitledItems.Reset();
	for (const FString& Code : HaivenLyraEntitlements::ActiveItems(Response))
	{
		ActiveEntitledItems.Add(Code);
	}
	for (const FString& Code : HaivenLyraEntitlements::NewlyActiveItems(Response, AnnouncedItems))
	{
		AnnouncedItems.Add(Code);
		Note(FText::Format(NSLOCTEXT("HaivenLyraDemo", "Entitled", "unlocked: {0}"), FText::FromString(Code)));
		OnItemEntitled.Broadcast(Code);
	}
}

void UHaivenLyraDemoSubsystem::HandleEntitlementsFailed(const FHaivenHttpResult& Result)
{
	Note(FText::Format(NSLOCTEXT("HaivenLyraDemo", "EntitlementsFailed", "entitlements unavailable: {0}"),
		FText::FromString(Result.Error.IsEmpty() ? FString::FromInt(Result.StatusCode) : Result.Error)));
}
