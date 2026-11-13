#include "HaivenLyraSignIn.h"

#include "HaivenLyraDemoSettings.h"

namespace HaivenLyraSignIn
{
	bool DemoTokenExpiryMetadata(const FString& Value, FDateTime& OutExpiresAtUtc)
	{
		return !Value.IsEmpty() && FDateTime::ParseIso8601(*Value, OutExpiresAtUtc);
	}

	FHaivenGameLoginOptions LoginOptions(const UHaivenLyraDemoSettings& Settings)
	{
		FHaivenGameLoginOptions Options;
		Options.EntryUrl = Settings.LoginEntryUrl;
		Options.ExchangeUrl = Settings.ResolveLoginExchangeUrl();
		Options.IntegrationId = Settings.IntegrationId;
		Options.RedirectUrl = Settings.ResolveLoginRedirectUrl();
		// A player who walks away leaves the port open no longer than this.
		Options.TransactionLifetimeSeconds = 600.0f;
		return Options;
	}
}
