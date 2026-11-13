#pragma once

#include "Engine/EngineBaseTypes.h"
#include "Internationalization/Text.h"

/**
 * Process-local presentation state for a failed player-hosted match.
 *
 * Lyra owns network cleanup and frontend travel. The Haiven demo only keeps a
 * deterministic, idempotent explanation alive across that travel; it never
 * elects a replacement authority or changes the player's Haiven session.
 */
class FHaivenLyraHostLossState
{
public:
	bool Record(const bool bWasClient, const ENetworkFailure::Type FailureType)
	{
		if (!bWasClient || bActive) return false;
		bActive = true;
		bTimedOut = FailureType == ENetworkFailure::ConnectionTimeout;
		return true;
	}

	void Reset()
	{
		bActive = false;
		bTimedOut = false;
	}

	bool IsActive() const { return bActive; }

	FText StatusText() const
	{
		if (!bActive) return FText::GetEmpty();
		return bTimedOut
			? NSLOCTEXT("HaivenLyraDemo", "HostTimedOut", "MATCH: HOST CONNECTION TIMED OUT — MATCH ENDED")
			: NSLOCTEXT("HaivenLyraDemo", "HostDisconnected", "MATCH: HOST DISCONNECTED — MATCH ENDED");
	}

private:
	bool bActive = false;
	bool bTimedOut = false;
};
