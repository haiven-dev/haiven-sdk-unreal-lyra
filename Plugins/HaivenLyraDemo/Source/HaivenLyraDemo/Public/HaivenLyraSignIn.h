// Pure pieces of the demo's sign-in: the login options it hands the Haiven
// plugin, and how long a HAIVEN_DEMO_ACCESS_TOKEN shortcut is good for. The
// login itself (PKCE, state, the loopback redirect and the exchange) is the
// plugin's UHaivenSubsystem::BeginLoopbackGameLogin.

#pragma once

#include "CoreMinimal.h"
#include "HaivenSubsystem.h"

class UHaivenLyraDemoSettings;

namespace HaivenLyraSignIn
{
	/** Staging's access-token lifetime, assumed only when a token carries no readable expiry. */
	HAIVENLYRADEMO_API extern const FTimespan UnknownTokenLifetime;

	/**
	 * When an environment-supplied access token expires: its JWT exp claim, read
	 * without verification because the server is the authority and only the
	 * local deadline is needed, or Now + UnknownTokenLifetime for an opaque one.
	 * bFromToken says which. The token is never logged or kept.
	 */
	HAIVENLYRADEMO_API FDateTime DemoTokenExpiry(const FString& Token, const FDateTime& Now, bool& bFromToken);

	/** The game login for the configured integration, entry page and loopback redirect. */
	HAIVENLYRADEMO_API FHaivenGameLoginOptions LoginOptions(const UHaivenLyraDemoSettings& Settings);
}
