// Wires the Haiven plugin to the demo HUD for one local player: Sign in with
// Haiven, account, message of the day, moderated chat, a subscriber reward
// claim, and the item entitlements the inventory plugin turns into Lyra items.
//
// Everything here goes through the Haiven plugin's public API: the
// UHaivenSubsystem for configuration, the connection and chat sends,
// UHaivenRealtimeEvents for typed chat.message events, and the plugin's
// Blueprint async actions for GET /users/me, the entitlements read and the
// reward claim. It never touches the generated internals or makes HTTP calls of
// its own, so it doubles as a check that the published surface is enough to
// integrate Haiven into a real game.
//
// It is a local-player subsystem, so it attaches to every Lyra Experience,
// front end included, without editing an Experience or HUD asset. Every action
// and status below is also a Blueprint node under "Haiven|Lyra Demo".

#pragma once

#include "Containers/Ticker.h"
#include "Delegates/Delegate.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "HaivenLyraMotdProjection.h"
#include "HaivenRewards.h"

#include "HaivenLyraDemoSubsystem.generated.h"

class FHaivenLyraChatInput;
class FHaivenLyraDemoAnalytics;
class FHaivenLyraHostLossState;
class FHaivenAnalytics;
class UHaivenLyraChatWidget;
class UHaivenLyraMotdWidget;
class UHaivenRealtimeEvents;
class UHaivenSubsystem;
class UNetDriver;
class UWorld;
enum class EHaivenRealtimeState : uint8;
struct FHaivenChatMessageV0;
struct FHaivenEntitlementsUpdatedV0;
struct FHaivenGameCredential;
struct FHaivenHttpResult;
struct FHaivenOutgoingChatMessage;
struct FHaivenMotd;
struct FHaivenRestAccountEntitlementsResponse;
struct FHaivenRestCurrentUserSessionResponse;

/** An active item entitlement this session has not granted yet; Code is the entitlement code. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHaivenLyraItemEntitled, const FString&, Code);
/** Host-owned navigation boundary. The demo never opens the URL itself. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHaivenLyraMotdActionRequested, const FString&, HttpsUrl);
/** The player, connection, moderation or reward status changed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHaivenLyraStatusChanged);

/** Where this player is in signing in to Haiven. */
UENUM(BlueprintType)
enum class EHaivenLyraSignInState : uint8
{
	/** No bearer: press Sign in with Haiven, or launch with HAIVEN_DEMO_ACCESS_TOKEN. */
	SignedOut UMETA(DisplayName = "Signed Out"),
	/** The browser is open on Haiven's sign-in page; the game waits for its redirect. */
	SigningIn UMETA(DisplayName = "Signing In"),
	/** A bearer is held in memory until its expiry. */
	SignedIn UMETA(DisplayName = "Signed In"),
	/** The last sign-in did not finish; Get Sign In Error says why. Retry signs in again. */
	Failed UMETA(DisplayName = "Failed"),
	/** The bearer expired. The handoff grants no refresh, so retry signs in again. */
	Expired UMETA(DisplayName = "Expired"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHaivenLyraSignInChanged, EHaivenLyraSignInState, State);

UCLASS()
class HAIVENLYRADEMO_API UHaivenLyraDemoSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	/**
	 * Opens the system browser on Haiven's sign-in page for IntegrationId and
	 * waits on the loopback LoginRedirectUrl. On success the bearer lives in the
	 * Haiven subsystem (never on disk or in a log) and chat connects.
	 */
	UFUNCTION(BlueprintCallable, Category = "Haiven|Lyra Demo|Sign In", meta = (DisplayName = "Sign In With Haiven"))
	void SignIn();

	/** Abandons a sign-in in progress; the player keeps any bearer they already had. */
	UFUNCTION(BlueprintCallable, Category = "Haiven|Lyra Demo|Sign In")
	void CancelSignIn();

	/** Clears the bearer on this device and closes chat. */
	UFUNCTION(BlueprintCallable, Category = "Haiven|Lyra Demo|Sign In")
	void SignOut();

	/** What the panel's sign-in button does: sign in, cancel, sign out or retry, by state. */
	UFUNCTION(BlueprintCallable, Category = "Haiven|Lyra Demo|Sign In")
	void PressSignInAction();

	UFUNCTION(BlueprintPure, Category = "Haiven|Lyra Demo|Sign In")
	EHaivenLyraSignInState GetSignInState() const { return SignInState; }

	/** SIGNED OUT, SIGNING IN, SIGNED IN, SIGN-IN FAILED or SESSION EXPIRED. */
	UFUNCTION(BlueprintPure, Category = "Haiven|Lyra Demo|Sign In")
	FString GetSignInStatus() const;

	/** Why the last sign-in failed or ended; empty otherwise. Never contains a token or code. */
	UFUNCTION(BlueprintPure, Category = "Haiven|Lyra Demo|Sign In")
	FString GetSignInError() const { return SignInError; }

	/** When the current bearer expires, from exchange or explicit host metadata. */
	UFUNCTION(BlueprintPure, Category = "Haiven|Lyra Demo|Sign In")
	FDateTime GetSignInExpiresAtUtc() const { return SignInExpiresAtUtc; }

	/** True when Sign in with Haiven has an integration, login page and API to use. */
	UFUNCTION(BlueprintPure, Category = "Haiven|Lyra Demo|Sign In")
	bool CanSignIn() const;

	UPROPERTY(BlueprintAssignable, Category = "Haiven|Lyra Demo|Sign In")
	FHaivenLyraSignInChanged OnSignInStateChanged;

	/** Sends one message to the configured channel and shows it as sending. */
	UFUNCTION(BlueprintCallable, Category = "Haiven|Lyra Demo|Chat", meta = (DisplayName = "Send Chat Message"))
	void Send(const FString& Content);

	/** Moves keyboard focus to the chat input, as Enter does during play. False when there is no panel. */
	UFUNCTION(BlueprintCallable, Category = "Haiven|Lyra Demo|Chat")
	bool FocusChat();

	/** Returns keyboard focus to the game after a message is sent. */
	UFUNCTION(BlueprintCallable, Category = "Haiven|Lyra Demo|Chat")
	void ReleaseChat();

	/** Collapses the chat and rewards drawers, retaining their edge handles. */
	void CollapseMainPanels();

	/** Collapses the MOTD drawer, retaining its edge handle when a message exists. */
	void CollapseMotdPanel();

	UFUNCTION(BlueprintPure, Category = "Haiven|Lyra Demo|Chat")
	bool IsChatFocused() const { return bChatFocused; }

	/** Shows a status line in the chat panel, for integrations layered on this demo. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Haiven|Lyra Demo")
	void AddNote(const FText& Line) const { Note(Line); }

	/** Reads the account's entitlements and announces item entitlements not granted yet. */
	UFUNCTION(BlueprintCallable, Category = "Haiven|Lyra Demo|Inventory")
	void RefreshEntitlements();

	/**
	 * Claims the configured RewardRef for IntegrationId, once per press: a
	 * press while a claim is in flight does nothing. A retry after a failure
	 * reuses the claim id, so it never claims twice. Any item the reward grants
	 * arrives through entitlements.updated and the inventory plugin.
	 */
	UFUNCTION(BlueprintCallable, Category = "Haiven|Lyra Demo|Rewards")
	void ClaimReward();

	/** Validates again, then delegates an HTTPS action to the host. */
	UFUNCTION(BlueprintCallable, Category = "Haiven|Lyra Demo|MOTD")
	void RequestMotdAction(const FString& HttpsUrl);

	UFUNCTION(BlueprintCallable, Category = "Haiven|Lyra Demo|MOTD")
	void DismissMotd();

	/** The signed-in player's display name or username; empty until GET /users/me answers. */
	UFUNCTION(BlueprintPure, Category = "Haiven|Lyra Demo|Status")
	FString GetAccountName() const { return AccountName; }

	/** OFFLINE, CONNECTING, SIGNING IN or CONNECTED. */
	UFUNCTION(BlueprintPure, Category = "Haiven|Lyra Demo|Status")
	FString GetConnectionStatus() const;

	/** The last sent message's moderation outcome: READY, CHECKING, ALLOWED, HELD FOR REVIEW, NOT SHOWN, REFUSED. */
	UFUNCTION(BlueprintPure, Category = "Haiven|Lyra Demo|Status")
	FString GetModerationStatus() const { return ModerationStatus; }

	/** READY TO CLAIM, CLAIMING..., CLAIMED, ALREADY CLAIMED, NOT ELIGIBLE, ..., or NOT CONFIGURED. */
	UFUNCTION(BlueprintPure, Category = "Haiven|Lyra Demo|Status")
	FString GetRewardStatus() const { return RewardStatus; }

	/** True when a reward is configured and no claim is in flight. */
	UFUNCTION(BlueprintPure, Category = "Haiven|Lyra Demo|Rewards")
	bool CanClaimReward() const;

	/** The panel's two status lines. */
	UFUNCTION(BlueprintPure, Category = "Haiven|Lyra Demo|Status")
	FText GetStatusText() const;

	/** Empty during normal play; explicit terminal match state after the listen host is lost. */
	UFUNCTION(BlueprintPure, Category = "Haiven|Lyra Demo|Status")
	FText GetHostLossStatus() const;

	/** Item entitlement codes active in the most recent account projection. */
	UFUNCTION(BlueprintPure, Category = "Haiven|Lyra Demo|Inventory")
	TArray<FString> GetEntitledItems() const { return ActiveEntitledItems.Array(); }

	/** Fires after each finished claim, whichever way it ended. */
	UPROPERTY(BlueprintAssignable, Category = "Haiven|Lyra Demo|Rewards")
	FHaivenRewardClaimed OnRewardClaimed;

	/** Fires when any status getter above would return something new. */
	UPROPERTY(BlueprintAssignable, Category = "Haiven|Lyra Demo|Status")
	FHaivenLyraStatusChanged OnStatusChanged;

	/**
	 * Fires once per item entitlement per session: on the first read after
	 * connecting, and after each entitlements.updated push. The game decides
	 * what an entitlement code grants.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Haiven|Lyra Demo")
	FHaivenLyraItemEntitled OnItemEntitled;

	UPROPERTY(BlueprintAssignable, Category = "Haiven|Lyra Demo|MOTD")
	FHaivenLyraMotdActionRequested OnMotdActionRequested;

private:
	friend class FHaivenLyraDemoSignInTest;

	UFUNCTION()
	void HandleGameLoginCompleted(const FHaivenGameCredential& Credential);

	UFUNCTION()
	void HandleGameLoginFailed(const FString& Error);

	UFUNCTION()
	void HandleAuthExpired();

	UFUNCTION()
	void HandleChatMessage(const FHaivenChatMessageV0& Message);

	UFUNCTION()
	void HandleOutgoingChat(const FHaivenOutgoingChatMessage& Message);

	UFUNCTION()
	void HandleRealtimeState(EHaivenRealtimeState State);

	UFUNCTION()
	void HandleEntitlementsUpdated(const FHaivenEntitlementsUpdatedV0& Payload);

	UFUNCTION()
	void HandleEntitlements(const FHaivenHttpResult& Result, const FHaivenRestAccountEntitlementsResponse& Response);

	UFUNCTION()
	void HandleEntitlementsFailed(const FHaivenHttpResult& Result);

	UFUNCTION()
	void HandleMotdChanged(bool bHasMessage, const FHaivenMotd& Message);

	UFUNCTION()
	void HandleMotdLoadFailed();

	UFUNCTION()
	void HandleAccount(const FHaivenHttpResult& Result, const FHaivenRestCurrentUserSessionResponse& Response);

	UFUNCTION()
	void HandleAccountFailed(const FHaivenHttpResult& Result);

	UFUNCTION()
	void HandleRewardOutcome(const FHaivenRewardClaimOutcome& Outcome);

	void BindHaiven();
	void UnbindHaiven();
	void Connect();
	void AttachAnalytics();
	void DetachAnalytics();
	void SetSignInState(EHaivenLyraSignInState State, const FString& Error = FString());
	void EnterSignedIn(const FDateTime& ExpiresAtUtc);
	void ScheduleExpiryCheck();
	void ClearExpiryCheck();
	/** Drop a launch-token realtime connect that has not run yet. */
	void CancelPendingRealtimeConnect();
	/** Ends a signed-in session whose bearer has expired at Now. */
	void UpdateExpiry(const FDateTime& Now);
	void Expire();
	void RefreshAccount();
	void Note(const FText& Line) const;
	void ApplyMotdPresentation() const;
	/** Start the public MOTD read and show its loading state. */
	void StartMotdRead();
	void ApplyStatus();
	void HandleNetworkFailure(
		UWorld* World,
		UNetDriver* NetDriver,
		ENetworkFailure::Type FailureType,
		const FString& ErrorString);

	UPROPERTY(Transient)
	TObjectPtr<UHaivenSubsystem> Haiven;

	UPROPERTY(Transient)
	TObjectPtr<UHaivenRealtimeEvents> Events;

	UPROPERTY(Transient)
	TObjectPtr<UHaivenLyraChatWidget> Panel;

	UPROPERTY(Transient)
	TObjectPtr<UHaivenLyraMotdWidget> MotdPanel;

	TSharedPtr<FHaivenLyraChatInput> ChatInput;
	TSharedPtr<FHaivenLyraDemoAnalytics> DemoAnalytics;
	TSharedPtr<FHaivenLyraHostLossState> HostLossState;
	TSharedPtr<FHaivenAnalytics, ESPMode::ThreadSafe> AnalyticsBridge;
	FString ChannelId;
	FString AccountName;
	FString ModerationStatus;
	FString RewardStatus;
	bool bClaimInFlight = false;
	TSet<FString> ActiveEntitledItems;
	TSet<FString> AnnouncedItems;
	FHaivenLyraMotdState MotdState;
	FString MotdRequestId;
	uint64 MotdRequestSequence = 0;
	bool bChatFocused = false;
	EHaivenLyraSignInState SignInState = EHaivenLyraSignInState::SignedOut;
	FString SignInError;
	FDateTime SignInExpiresAtUtc;
	FTSTicker::FDelegateHandle ExpiryCheck;
	FTSTicker::FDelegateHandle PendingRealtimeConnect;
	FDelegateHandle NetworkFailureHandle;
};
