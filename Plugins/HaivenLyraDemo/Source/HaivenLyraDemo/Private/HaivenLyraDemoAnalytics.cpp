#include "HaivenLyraDemoAnalytics.h"

#include "HAL/PlatformTime.h"

namespace
{
	constexpr double MaxPlaytimeSeconds = 86400.0;
}

FHaivenLyraDemoAnalytics::FHaivenLyraDemoAnalytics(FEnqueue InEnqueue, FNowSeconds InNowSeconds)
	: Enqueue(MoveTemp(InEnqueue)), Clock(MoveTemp(InNowSeconds))
{
}

bool FHaivenLyraDemoAnalytics::IsEnabledValue(const FString& Value)
{
	const FString Normalized = Value.TrimStartAndEnd().ToLower();
	return Normalized == TEXT("true") || Normalized == TEXT("1")
		|| Normalized == TEXT("yes") || Normalized == TEXT("on");
}

void FHaivenLyraDemoAnalytics::SetEnqueue(FEnqueue InEnqueue)
{
	Enqueue = MoveTemp(InEnqueue);
}

void FHaivenLyraDemoAnalytics::GameLaunched()
{
	if (bLaunched) return;
	bLaunched = true;
	Emit(TEXT("game.launch"));
}

void FHaivenLyraDemoAnalytics::Connecting()
{
	GameLaunched();
	bConnecting = true;
}

void FHaivenLyraDemoAnalytics::SessionStarted()
{
	GameLaunched();
	bConnecting = false;
	if (!bInSession)
	{
		bInSession = true;
		SessionStartedAtSeconds = NowSeconds();
		Emit(TEXT("session.start"));
	}
	Emit(TEXT("sdk.health"), TEXT("{\"status\":\"ok\"}"));
}

void FHaivenLyraDemoAnalytics::Disconnected()
{
	if (bConnecting) Emit(TEXT("sdk.health"), TEXT("{\"status\":\"error\"}"));
	bConnecting = false;
	SessionEnded();
}

void FHaivenLyraDemoAnalytics::SessionEnded()
{
	if (!bInSession) return;
	bInSession = false;
	const double EndedAtSeconds = NowSeconds();
	Emit(TEXT("session.end"));
	const double Seconds = FMath::Min(
		MaxPlaytimeSeconds,
		FMath::RoundToDouble(FMath::Max(0.0, EndedAtSeconds - SessionStartedAtSeconds) * 1000.0) / 1000.0);
	if (Seconds > 0.0)
	{
		Emit(TEXT("playtime"), FString::Printf(TEXT("{\"seconds\":%.3f}"), Seconds));
	}
}

void FHaivenLyraDemoAnalytics::Emit(const FString& Type, const FString& PropertiesJson)
{
	if (Enqueue) Enqueue(Type, PropertiesJson);
}

double FHaivenLyraDemoAnalytics::NowSeconds() const
{
	return Clock ? Clock() : FPlatformTime::Seconds();
}
