#include "HaivenLyraPresentation.h"

#include "Blueprint/HaivenBlueprintRestTypes.h"
#include "HaivenChat.h"
#include "HaivenClientTypes.h"
#include "HaivenLyraDemoSubsystem.h"
#include "HaivenRewards.h"

namespace HaivenLyraPresentation
{
	const TCHAR* const RewardReady = TEXT("READY TO CLAIM");
	const TCHAR* const RewardClaiming = TEXT("CLAIMING...");
	const TCHAR* const RewardNotConfigured = TEXT("NOT CONFIGURED");
	const TCHAR* const ModerationReady = TEXT("READY");
	const TCHAR* const AccountUnknown = TEXT("unknown player");

	FString ConnectionStatus(const EHaivenRealtimeState State)
	{
		switch (State)
		{
		case EHaivenRealtimeState::Disconnected: return TEXT("OFFLINE");
		case EHaivenRealtimeState::Connecting: return TEXT("CONNECTING");
		case EHaivenRealtimeState::Authenticating: return TEXT("SIGNING IN");
		case EHaivenRealtimeState::Ready: return TEXT("CONNECTED");
		}
		return TEXT("OFFLINE");
	}

	FString ModerationStatus(const EHaivenOutgoingChatState State)
	{
		switch (State)
		{
		case EHaivenOutgoingChatState::Sending: return TEXT("CHECKING");
		case EHaivenOutgoingChatState::Unconfirmed: return TEXT("WAITING FOR CONNECTION");
		case EHaivenOutgoingChatState::Accepted: return TEXT("ALLOWED");
		case EHaivenOutgoingChatState::Held: return TEXT("HELD FOR REVIEW");
		case EHaivenOutgoingChatState::Removed: return TEXT("NOT SHOWN");
		case EHaivenOutgoingChatState::Refused: return TEXT("REFUSED");
		}
		return ModerationReady;
	}

	FString RewardStatus(const FHaivenRewardClaimOutcome& Outcome)
	{
		switch (Outcome.Result)
		{
		case EHaivenRewardClaimResult::Claimed:
		case EHaivenRewardClaimResult::AlreadyRecorded:
			return TEXT("CLAIMED");
		case EHaivenRewardClaimResult::AlreadyClaimed: return TEXT("ALREADY CLAIMED");
		case EHaivenRewardClaimResult::NotEligible: return TEXT("NOT ELIGIBLE");
		case EHaivenRewardClaimResult::RefundInProgress: return TEXT("TEMPORARILY UNAVAILABLE");
		case EHaivenRewardClaimResult::ClaimConflict: return TEXT("CLAIM CONFLICT");
		case EHaivenRewardClaimResult::NotFound:
		case EHaivenRewardClaimResult::Failed:
			break;
		}
		return Outcome.bRetryable ? TEXT("RETRY CLAIM") : TEXT("CLAIM FAILED");
	}

	FString AccountName(const FHaivenRestCurrentUserSessionResponse& Session)
	{
		if (Session.bHasDisplayName && !Session.DisplayName.IsEmpty()) return Session.DisplayName;
		if (Session.bHasUsername && !Session.Username.IsEmpty()) return Session.Username;
		return FString();
	}

	FString SignInStatus(const EHaivenLyraSignInState State)
	{
		switch (State)
		{
		case EHaivenLyraSignInState::SignedOut: return TEXT("SIGNED OUT");
		case EHaivenLyraSignInState::SigningIn: return TEXT("SIGNING IN");
		case EHaivenLyraSignInState::SignedIn: return TEXT("SIGNED IN");
		case EHaivenLyraSignInState::Failed: return TEXT("SIGN-IN FAILED");
		case EHaivenLyraSignInState::Expired: return TEXT("SESSION EXPIRED");
		}
		return TEXT("SIGNED OUT");
	}

	FText SignInActionLabel(const EHaivenLyraSignInState State)
	{
		switch (State)
		{
		case EHaivenLyraSignInState::SignedOut: return NSLOCTEXT("HaivenLyraDemo", "SignInAction", "Sign in with Haiven");
		case EHaivenLyraSignInState::SigningIn: return NSLOCTEXT("HaivenLyraDemo", "CancelSignInAction", "Cancel sign-in");
		case EHaivenLyraSignInState::SignedIn: return NSLOCTEXT("HaivenLyraDemo", "SignOutAction", "Sign out");
		case EHaivenLyraSignInState::Failed:
		case EHaivenLyraSignInState::Expired:
			return NSLOCTEXT("HaivenLyraDemo", "RetrySignInAction", "Retry sign-in");
		}
		return NSLOCTEXT("HaivenLyraDemo", "SignInAction", "Sign in with Haiven");
	}

	FText StatusLines(const FString& Account, const FString& SignIn, const FString& Connection, const FString& Moderation, const FString& Reward)
	{
		return FText::Format(
			NSLOCTEXT("HaivenLyraDemo", "StatusLines", "Player: {0}  |  Sign-in: {1}  |  Haiven: {2}\nLast message: {3}  |  Reward: {4}"),
			FText::FromString(Account.IsEmpty() ? FString(AccountUnknown) : Account),
			FText::FromString(SignIn),
			FText::FromString(Connection),
			FText::FromString(Moderation),
			FText::FromString(Reward));
	}
}
