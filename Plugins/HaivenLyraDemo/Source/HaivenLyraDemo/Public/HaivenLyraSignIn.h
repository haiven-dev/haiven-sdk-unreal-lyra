// Pure pieces of the demo's sign-in: the login options it hands the Haiven
// plugin, and explicit expiry metadata for its environment shortcut. The
// login itself (PKCE, state, the loopback redirect and the exchange) is the
// plugin's UHaivenSubsystem::BeginLoopbackGameLogin.

#pragma once

#include "CoreMinimal.h"
#include "HaivenSubsystem.h"

class UHaivenLyraDemoSettings;

namespace HaivenLyraSignIn
{
	/**
	 * Parses HAIVEN_DEMO_ACCESS_TOKEN_EXPIRES_AT as an ISO-8601 UTC instant.
	 * The bearer itself stays opaque and is never decoded for claims.
	 */
	HAIVENLYRADEMO_API bool DemoTokenExpiryMetadata(const FString& Value, FDateTime& OutExpiresAtUtc);

	/** The game login for the configured integration, entry page and loopback redirect. */
	HAIVENLYRADEMO_API FHaivenGameLoginOptions LoginOptions(const UHaivenLyraDemoSettings& Settings);
}
