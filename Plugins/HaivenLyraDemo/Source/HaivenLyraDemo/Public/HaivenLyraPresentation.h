// Finite, presenter-facing status spellings for the Lyra demo's HUD. They match
// the Boss Room demo's drawers so the same player journey reads the same in
// both engines. Pure functions: the subsystem owns the state, the widget only
// renders the strings, and automation tests cover the mapping.

#pragma once

#include "CoreMinimal.h"

enum class EHaivenOutgoingChatState : uint8;
enum class EHaivenLyraSignInState : uint8;
enum class EHaivenRealtimeState : uint8;
struct FHaivenRestCurrentUserSessionResponse;
struct FHaivenRewardClaimOutcome;

namespace HaivenLyraPresentation
{
	/** OFFLINE, CONNECTING, SIGNING IN or CONNECTED. */
	HAIVENLYRADEMO_API FString ConnectionStatus(EHaivenRealtimeState State);

	/** The last sent message's moderation outcome, for example ALLOWED or HELD FOR REVIEW. */
	HAIVENLYRADEMO_API FString ModerationStatus(EHaivenOutgoingChatState State);

	/** CLAIMED, ALREADY CLAIMED, NOT ELIGIBLE, ... or RETRY CLAIM / CLAIM FAILED. */
	HAIVENLYRADEMO_API FString RewardStatus(const FHaivenRewardClaimOutcome& Outcome);

	/** Display name, else username, else empty: the HUD then shows the account as unknown. */
	HAIVENLYRADEMO_API FString AccountName(const FHaivenRestCurrentUserSessionResponse& Session);

	/** SIGNED OUT, SIGNING IN, SIGNED IN, SIGN-IN FAILED or SESSION EXPIRED. */
	HAIVENLYRADEMO_API FString SignInStatus(EHaivenLyraSignInState State);

	/** The sign-in button's label: what pressing it does in this state. */
	HAIVENLYRADEMO_API FText SignInActionLabel(EHaivenLyraSignInState State);

	/** Two HUD lines: who is signed in, how, and the connection, then the last message and the reward. */
	HAIVENLYRADEMO_API FText StatusLines(
		const FString& Account,
		const FString& SignIn,
		const FString& Connection,
		const FString& Moderation,
		const FString& Reward);

	/** The status before any claim, and while one is in flight. */
	HAIVENLYRADEMO_API extern const TCHAR* const RewardReady;
	HAIVENLYRADEMO_API extern const TCHAR* const RewardClaiming;
	HAIVENLYRADEMO_API extern const TCHAR* const RewardNotConfigured;
	HAIVENLYRADEMO_API extern const TCHAR* const ModerationReady;
	HAIVENLYRADEMO_API extern const TCHAR* const AccountUnknown;
}
