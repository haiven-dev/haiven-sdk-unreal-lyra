#pragma once

#include "CoreMinimal.h"

/** Boss Room-compatible, privacy-safe analytics lifecycle for the Lyra demo. */
class FHaivenLyraDemoAnalytics
{
public:
	using FEnqueue = TFunction<bool(const FString& Type, const FString& PropertiesJson)>;
	using FNowSeconds = TFunction<double()>;

	explicit FHaivenLyraDemoAnalytics(FEnqueue InEnqueue = {}, FNowSeconds InNowSeconds = {});

	/** Analytics is off unless the value is exactly true, 1, yes, or on (case-insensitive). */
	static bool IsEnabledValue(const FString& Value);

	void SetEnqueue(FEnqueue InEnqueue);
	void GameLaunched();
	void Connecting();
	void SessionStarted();
	void Disconnected();
	void SessionEnded();

	bool IsInSession() const { return bInSession; }

private:
	void Emit(const FString& Type, const FString& PropertiesJson = TEXT("{}"));
	double NowSeconds() const;

	FEnqueue Enqueue;
	FNowSeconds Clock;
	double SessionStartedAtSeconds = 0.0;
	bool bLaunched = false;
	bool bConnecting = false;
	bool bInSession = false;
};
