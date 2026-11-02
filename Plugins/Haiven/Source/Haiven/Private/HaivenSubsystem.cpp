#include "HaivenSubsystem.h"

#include "Async/Async.h"
#include "HaivenAnalytics.h"
#include "HaivenHandoff.h"
#include "HaivenLoopbackReceiver.h"
#include "HaivenLoopbackRedirect.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HAL/PlatformProcess.h"
#include "Vocabulary/MessageType.h"

#define UI UI_ST
THIRD_PARTY_INCLUDES_START
#include <openssl/rand.h>
THIRD_PARTY_INCLUDES_END
#undef UI

namespace
{
	constexpr int32 MaxDiagnosticFrameTypeCharacters = 64;
	const FString UnrecognizedDiagnosticFrameType(TEXT("<unrecognized>"));

	FString RedactDiagnosticFrameType(const FString& Type)
	{
		// The wire vocabulary is open for routing, but diagnostics must not retain
		// arbitrary server- or caller-controlled strings. Known protocol names are
		// short; reject oversized input before making another copy of it.
		if (Type.Len() > MaxDiagnosticFrameTypeCharacters) return UnrecognizedDiagnosticFrameType;
		FTCHARToUTF8 Utf8(*Type);
		const haiven::wire::MessageType MessageType(std::string(Utf8.Get(), Utf8.Length()));
		return MessageType.recognised() ? Type : UnrecognizedDiagnosticFrameType;
	}

	bool SecureRandomBytes(TArray<uint8>& Out, int32 Count)
	{
		Out.SetNumUninitialized(Count);
		return RAND_bytes(Out.GetData(), Out.Num()) == 1;
	}
}

void UHaivenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Handoff = MakeUnique<Haiven::FHandoffCore>();
	ResetChat();
	Rewards = MakeUnique<FHaivenRewards>();
	ResetIdentity();
}

void UHaivenSubsystem::Deinitialize()
{
	HostAccessToken.Reset();
	HostAccessTokenExpiry = FDateTime();
	RecentRealtimeFrames.Reset();
	if (Handoff) Handoff->ClearCredential();
	InvalidateCredentialBoundary();
	ShutdownNetworking();
	StopLoopbackReceiver();
	if (ActiveRequest.IsValid()) ActiveRequest->CancelRequest();
	ActiveRequest.Reset();
	Handoff.Reset();
	Chat.Reset();
	Rewards.Reset();
	Identity.Reset();
	IdentityBrowserAdapter = nullptr;
	Super::Deinitialize();
}

bool UHaivenSubsystem::BeginGameLogin(const FHaivenGameLoginOptions& Options, FString& AuthorizationUrl, FString& Error)
{
	CancelGameLogin();
	return BeginGameLoginTransaction(Options, AuthorizationUrl, Error);
}

bool UHaivenSubsystem::BeginLoopbackGameLogin(const FHaivenGameLoginOptions& Options, FString& AuthorizationUrl, FString& Error)
{
	CancelGameLogin();
	Haiven::LoopbackRedirect::FEndpoint Endpoint;
	if (!Haiven::LoopbackRedirect::Parse(Options.RedirectUrl, Endpoint, Error))
	{
		RecordFailure(EHaivenHealthArea::Login, Error);
		return false;
	}
	TWeakObjectPtr<UHaivenSubsystem> WeakThis(this);
	LoopbackReceiver = FHaivenLoopbackReceiver::Start(Endpoint, [WeakThis](const FString& CallbackUrl)
	{
		return WeakThis.IsValid() && WeakThis->ConsumeLoopbackCallback(CallbackUrl);
	}, Error);
	if (!LoopbackReceiver.IsValid())
	{
		RecordFailure(EHaivenHealthArea::Login, Error);
		return false;
	}
	if (!BeginGameLoginTransaction(Options, AuthorizationUrl, Error))
	{
		StopLoopbackReceiver();
		return false;
	}
	// The verifier refuses a late callback by itself; the deadline also closes
	// the port and tells the game, so an abandoned browser tab does not leave
	// the login waiting forever.
	LoopbackDeadline = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateWeakLambda(this, [this](float)
		{
			LoopbackDeadline.Reset();
			if (!Handoff || !Handoff->HasPendingTransaction()) return false;
			CancelGameLogin();
			BroadcastFailureOnGameThread(TEXT("The sign-in was not completed in time. Start again to continue."));
			return false;
		}),
		FMath::Max(1.0f, Options.TransactionLifetimeSeconds));
	return true;
}

bool UHaivenSubsystem::BeginGameLoginTransaction(const FHaivenGameLoginOptions& Options, FString& AuthorizationUrl, FString& Error)
{
	if (!Handoff) Handoff = MakeUnique<Haiven::FHandoffCore>();

	TArray<uint8> StateEntropy;
	TArray<uint8> VerifierEntropy;
	if (!SecureRandomBytes(StateEntropy, 32) || !SecureRandomBytes(VerifierEntropy, 32))
	{
		Error = TEXT("The platform cryptographic random source failed.");
		return false;
	}

	Haiven::FHandoffConfig Config;
	Config.EntryUrl = Options.EntryUrl;
	Config.ExchangeUrl = Options.ExchangeUrl;
	Config.IntegrationId = Options.IntegrationId;
	Config.RedirectUrl = Options.RedirectUrl;
	Config.InstructionsId = Options.InstructionsId;
	Config.ScreenHint = Options.ScreenHint;
	Config.TransactionLifetime = FTimespan::FromSeconds(Options.TransactionLifetimeSeconds);
	FString State;
	if (!Handoff->Begin(Config, StateEntropy, VerifierEntropy, FDateTime::UtcNow(), AuthorizationUrl, State, Error))
	{
		RecordFailure(EHaivenHealthArea::Login, Error);
		return false;
	}

	FString LaunchError;
	if (LoginUrlLauncher)
	{
		if (!LoginUrlLauncher(AuthorizationUrl, LaunchError) && LaunchError.IsEmpty())
		{
			LaunchError = TEXT("the launcher declined the URL");
		}
	}
	else
	{
		FPlatformProcess::LaunchURL(*AuthorizationUrl, nullptr, &LaunchError);
	}
	if (!LaunchError.IsEmpty())
	{
		Handoff->Cancel();
		Error = TEXT("The platform URL launcher failed: ") + LaunchError;
		RecordFailure(EHaivenHealthArea::Login, Error);
		return false;
	}
	UE_LOG(LogHaiven, Log, TEXT("Game login started; waiting for the browser callback."));
	return true;
}

void UHaivenSubsystem::HandleGameLoginCallback(const FString& CallbackUrl)
{
	if (!IsInGameThread())
	{
		TWeakObjectPtr<UHaivenSubsystem> WeakThis(this);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, CallbackUrl]()
		{
			if (WeakThis.IsValid()) WeakThis->HandleGameLoginCallback(CallbackUrl);
		});
		return;
	}
	ConsumeGameLoginCallback(CallbackUrl, false);
}

bool UHaivenSubsystem::ConsumeLoopbackCallback(const FString& CallbackUrl)
{
	check(IsInGameThread());
	// Any local page can send this browser to the loopback port; a request that
	// is not this transaction's must not end or fail the player's sign-in.
	if (!Handoff || !Handoff->HasPendingTransaction()) return false;
	const bool bAccepted = ConsumeGameLoginCallback(CallbackUrl, true);
	if (!Handoff || !Handoff->HasPendingTransaction()) StopLoopbackReceiver();
	return bAccepted;
}

bool UHaivenSubsystem::ConsumeGameLoginCallback(const FString& CallbackUrl, bool bQuietWhilePending)
{
	if (!Handoff)
	{
		BroadcastFailureOnGameThread(TEXT("The Haiven subsystem is not initialized."));
		return false;
	}

	Haiven::FHandoffExchange Exchange;
	FString Error;
	if (!Handoff->ConsumeCallback(CallbackUrl, FDateTime::UtcNow(), Exchange, Error))
	{
		if (bQuietWhilePending && Handoff->HasPendingTransaction())
		{
			UE_LOG(LogHaiven, Log, TEXT("Ignored a loopback request that does not belong to the current game login."));
			return false;
		}
		BroadcastFailureOnGameThread(Error);
		// The transaction was consumed: the browser did reach this login.
		return true;
	}

	ActiveRequest = FHttpModule::Get().CreateRequest();
	ActiveRequest->SetURL(Exchange.ExchangeUrl);
	// ExchangeUrl belongs to the active configuration; ToFormBody deliberately
	// contains exactly the four contract fields and no client secret.
	ActiveRequest->SetVerb(TEXT("POST"));
	ActiveRequest->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
	ActiveRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));
	FString FormBody = Exchange.ToFormBody();
	ActiveRequest->SetContentAsString(FormBody);
	FMemory::Memzero(FormBody.GetCharArray().GetData(), FormBody.GetCharArray().Num() * sizeof(TCHAR));
	FormBody.Reset();
	FMemory::Memzero(Exchange.CodeVerifier.GetCharArray().GetData(), Exchange.CodeVerifier.GetCharArray().Num() * sizeof(TCHAR));
	Exchange.CodeVerifier.Reset();
	TWeakObjectPtr<UHaivenSubsystem> WeakThis(this);
	ActiveRequest->OnProcessRequestComplete().BindLambda(
		[WeakThis](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded)
		{
			if (!WeakThis.IsValid() || WeakThis->ActiveRequest != Request) return;
			const int32 StatusCode = Response.IsValid() ? Response->GetResponseCode() : 0;
			const FString Body = Response.IsValid() ? Response->GetContentAsString() : FString();
			const FString TransportError = Response.IsValid() ? FString() : TEXT("no response");
			WeakThis->FinishExchange(bSucceeded, StatusCode, Body, TransportError);
		});
	if (!ActiveRequest->ProcessRequest())
	{
		ActiveRequest.Reset();
		BroadcastFailureOnGameThread(TEXT("The handoff exchange request could not be started."));
	}
	return true;
}

void UHaivenSubsystem::CancelGameLogin()
{
	if (ActiveRequest.IsValid()) ActiveRequest->CancelRequest();
	ActiveRequest.Reset();
	if (Handoff) Handoff->Cancel();
	StopLoopbackReceiver();
}

void UHaivenSubsystem::StopLoopbackReceiver()
{
	if (LoopbackDeadline.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(LoopbackDeadline);
		LoopbackDeadline.Reset();
	}
	LoopbackReceiver.Reset();
}

bool UHaivenSubsystem::IsGameLoginInProgress() const
{
	return ActiveRequest.IsValid() || (Handoff && Handoff->HasPendingTransaction());
}

void UHaivenSubsystem::SetLoginUrlLauncher(TFunction<bool(const FString& Url, FString& Error)> Launcher)
{
	LoginUrlLauncher = MoveTemp(Launcher);
}

void UHaivenSubsystem::Logout()
{
	if (!IsInGameThread())
	{
		TWeakObjectPtr<UHaivenSubsystem> WeakThis(this);
		AsyncTask(ENamedThreads::GameThread, [WeakThis]()
		{
			if (WeakThis.IsValid()) WeakThis->Logout();
		});
		return;
	}
	CancelGameLogin();
	HostAccessToken.Reset();
	HostAccessTokenExpiry = FDateTime();
	if (Handoff) Handoff->ClearCredential();
	InvalidateCredentialBoundary();
}

void UHaivenSubsystem::RegisterAnalytics(const TSharedRef<FHaivenAnalytics, ESPMode::ThreadSafe>& Instance)
{
	check(IsInGameThread());
	AnalyticsInstances.RemoveAll([](const TWeakPtr<FHaivenAnalytics, ESPMode::ThreadSafe>& Existing)
	{
		return !Existing.IsValid();
	});
	AnalyticsInstances.Add(Instance);
}

void UHaivenSubsystem::InvalidateCredentialBoundary()
{
	check(IsInGameThread());
	TArray<FHaivenHttpComplete> CancelledHttp = InvalidateAuthenticatedHttpRequests();
	if (Identity) Identity->ResetAccount();
	// Unsent/in-flight chat and reward work belongs to the previous bearer.
	ResetChat();
	if (Rewards) Rewards->Clear();
	for (const TWeakPtr<FHaivenAnalytics, ESPMode::ThreadSafe>& WeakAnalytics : AnalyticsInstances)
	{
		if (const TSharedPtr<FHaivenAnalytics, ESPMode::ThreadSafe> Instance = WeakAnalytics.Pin())
			Instance->Shutdown();
	}
	AnalyticsInstances.Reset();
	RecentRealtimeFrames.Reset();
	DisconnectRealtime();
	FHaivenHttpResult Cancelled;
	Cancelled.Error = TEXT("cancelled");
	for (FHaivenHttpComplete& Completion : CancelledHttp) Completion.ExecuteIfBound(Cancelled);
}

bool UHaivenSubsystem::GetPendingRewardClaimId(const FString& IntegrationId, const FString& RewardRef, FString& ClaimId) const
{
	return Rewards && Rewards->GetPendingClaimId(IntegrationId, RewardRef, ClaimId);
}

bool UHaivenSubsystem::GetAccessToken(FString& AccessToken) const
{
	if (!HostAccessToken.IsEmpty() && HostAccessTokenExpiry > FDateTime::UtcNow())
	{
		AccessToken = HostAccessToken;
		return true;
	}
	return Handoff && Handoff->GetAccessToken(FDateTime::UtcNow(), AccessToken);
}

void UHaivenSubsystem::FinishExchange(bool bTransportSucceeded, int32 StatusCode, const FString& Body, const FString& TransportError)
{
	if (!IsInGameThread())
	{
		TWeakObjectPtr<UHaivenSubsystem> WeakThis(this);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bTransportSucceeded, StatusCode, Body, TransportError]()
		{
			if (WeakThis.IsValid()) WeakThis->FinishExchange(bTransportSucceeded, StatusCode, Body, TransportError);
		});
		return;
	}
	ActiveRequest.Reset();
	if (!bTransportSucceeded || StatusCode < 200 || StatusCode >= 300)
	{
		BroadcastFailureOnGameThread(FString::Printf(TEXT("Handoff exchange failed with HTTP %d: %s"), StatusCode, *TransportError));
		return;
	}

	Haiven::FHandoffCredential Credential;
	FString Error;
	if (!Handoff || !Handoff->StoreExchangeResponse(Body, FDateTime::UtcNow(), Credential, Error))
	{
		BroadcastFailureOnGameThread(Error);
		return;
	}
	// A successful handoff is a credential replacement boundary. Make its
	// credential active and invalidate every cache/read owned by the prior user.
	HostAccessToken.Reset();
	HostAccessTokenExpiry = Credential.ExpiresAt;
	InvalidateCredentialBoundary();

	FHaivenGameCredential BlueprintCredential;
	BlueprintCredential.AccessToken = Credential.AccessToken;
	BlueprintCredential.TokenType = Credential.TokenType;
	BlueprintCredential.ExpiresAtUtc = Credential.ExpiresAt;
	UE_LOG(LogHaiven, Log, TEXT("Game login completed; the credential expires at %s UTC."), *Credential.ExpiresAt.ToIso8601());
	OnGameLoginCompleted.Broadcast(BlueprintCredential);
}

void UHaivenSubsystem::BroadcastFailureOnGameThread(const FString& Error)
{
	TWeakObjectPtr<UHaivenSubsystem> WeakThis(this);
	AsyncTask(ENamedThreads::GameThread, [WeakThis, Error]()
	{
		if (!WeakThis.IsValid()) return;
		WeakThis->RecordFailure(EHaivenHealthArea::Login, Error);
		WeakThis->OnGameLoginFailed.Broadcast(Error);
	});
}

void UHaivenSubsystem::RecordFailure(EHaivenHealthArea Area, const FString& Error)
{
	LastErrorArea = Area;
	LastError = Error;
	LastErrorAtUtc = FDateTime::UtcNow();
	UE_LOG(LogHaiven, Warning, TEXT("%s: %s"), *UEnum::GetDisplayValueAsText(Area).ToString(), *Error);
}

void UHaivenSubsystem::RecordRealtimeFrame(EHaivenRealtimeFrameDirection Direction, const FString& Type)
{
	constexpr int32 MaxRecentRealtimeFrames = 32;
	FHaivenRecentRealtimeFrame& Frame = RecentRealtimeFrames.AddDefaulted_GetRef();
	Frame.Direction = Direction;
	Frame.Type = RedactDiagnosticFrameType(Type);
	Frame.SeenAtUtc = FDateTime::UtcNow();
	if (RecentRealtimeFrames.Num() > MaxRecentRealtimeFrames)
	{
		RecentRealtimeFrames.RemoveAt(0, RecentRealtimeFrames.Num() - MaxRecentRealtimeFrames, EAllowShrinking::No);
	}
}

FHaivenHealth UHaivenSubsystem::GetHealth() const
{
	FHaivenHealth Health;
	Health.RealtimeState = RealtimeState;
	FString Token;
	Health.bHasAccessToken = GetAccessToken(Token);
	Health.bHasAccessTokenExpiry = HostAccessTokenExpiry > FDateTime();
	Health.AccessTokenExpiresAtUtc = HostAccessTokenExpiry;
	Health.bLoginInProgress = IsGameLoginInProgress();
	Health.RealtimeReadyCount = RealtimeReadyCount;
	Health.RealtimeDropCount = RealtimeDropCount;
	Health.bHasLastClose = bHasLastClose;
	Health.LastCloseCode = LastCloseCode;
	Health.LastCloseReason = LastCloseReason;
	Health.LastErrorArea = LastErrorArea;
	Health.LastError = LastError;
	Health.LastErrorAtUtc = LastErrorAtUtc;
	Health.PendingHttpRequests = HttpRequests.Num();
	Health.PendingRealtimeRequests = RealtimeRequests.Num();
	Health.RecentRealtimeFrames = RecentRealtimeFrames;
	if (Chat) Health.TallyChatOutbox(Chat->Outbox());
	return Health;
}
