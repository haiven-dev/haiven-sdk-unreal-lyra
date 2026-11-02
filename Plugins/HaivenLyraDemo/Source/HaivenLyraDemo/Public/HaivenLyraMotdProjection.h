// Shared MOTD presentation state for the shipped Lyra demo and its fixture
// runner.

#pragma once

#include "CoreMinimal.h"
#include "HaivenIdentity.h"

enum class EHaivenLyraMotdPresentationState : uint8
{
	Loading,
	Absent,
	Present,
	Failure,
};

HAIVENLYRADEMO_API FString HaivenLyraMotdStatusName(EHaivenLyraMotdPresentationState Status);

/** The normalized player-visible message. SDK presence flags and timestamps
 * stop here. */
struct HAIVENLYRADEMO_API FHaivenLyraMotdPresentationMessage
{
	FString Id;
	FString Title;
	FString Body;
	TOptional<FString> Url;
};

/**
 * The closed player-visible projection defined by
 * spec/presentation/motd-v1.json.
 *
 * Keep this aggregate to exactly these six fields. The fixture test
 * deliberately uses a six-name structured binding so adding another public
 * field fails to compile, and it round-trips these same fields through JSON on
 * every fixture.
 */
struct HAIVENLYRADEMO_API FHaivenLyraMotdProjection
{
	int64 Revision = 0;
	EHaivenLyraMotdPresentationState Status = EHaivenLyraMotdPresentationState::Absent;
	bool Visible = false;
	TOptional<FHaivenLyraMotdPresentationMessage> Message;
	TOptional<FString> Error;
	TOptional<FString> DismissedId;
};

/**
 * Request-aware state machine used by the live Lyra integration. The canonical
 * fixture runner calls this same type rather than maintaining a test oracle.
 */
class HAIVENLYRADEMO_API FHaivenLyraMotdState
{
public:
	const FHaivenLyraMotdProjection& GetProjection() const { return Projection; }

	/** Restore a visible projection; request correlation intentionally resets. */
	void Restore(const FHaivenLyraMotdProjection& Value);

	bool LoadStarted(const FString& RequestId);
	bool RestAbsent(const FString& RequestId);
	bool RestPresent(const FString& RequestId, const FHaivenMotd& Message);
	bool RestFailure(const FString& RequestId);
	bool RealtimePresent(const FHaivenMotd& Message);
	bool RealtimeClear();
	bool Dismiss();

	/** A host-side configuration failure uses the same fixed public copy. */
	bool PresentFailure();

private:
	bool IsCurrentRequest(const FString& RequestId) const;
	bool IsSuppressed(const FHaivenMotd& Message) const;
	bool PresentNormalized(const FHaivenMotd& Message);
	void ClearActiveRequest();
	void Commit(
		EHaivenLyraMotdPresentationState Status,
		bool Visible,
		const FHaivenLyraMotdPresentationMessage* Message = nullptr,
		const FString* Error = nullptr);

	FHaivenLyraMotdProjection Projection;
	FString ActiveRequestId;
	bool bHasActiveRequest = false;
};
