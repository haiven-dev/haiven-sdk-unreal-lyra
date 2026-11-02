// Project settings for the Lyra demo, stored in DefaultGame.ini under
// [/Script/HaivenLyraDemo.HaivenLyraDemoSettings]. The access token is never a
// setting: it comes from Sign in with Haiven, held in memory only, or from the
// HAIVEN_DEMO_ACCESS_TOKEN environment variable, so no credential lands in a
// committed config file.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"

#include "HaivenLyraDemoSettings.generated.h"

class UHaivenLyraChatWidget;
class UHaivenLyraMotdWidget;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Haiven Lyra Demo"))
class HAIVENLYRADEMO_API UHaivenLyraDemoSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** REST base URL including the version prefix, for example https://api.haiven.gg/v0. */
	UPROPERTY(Config, EditAnywhere, Category = "Haiven")
	FString BaseUrl;

	/**
	 * Realtime socket URL, for example wss://api.haiven.gg/v0/ws. The Haiven
	 * plugin does not derive it from BaseUrl and accepts ws:// only on loopback.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Haiven")
	FString RealtimeUrl;

	/** The integration's app id. */
	UPROPERTY(Config, EditAnywhere, Category = "Haiven")
	FString AppId;

	/**
	 * Public integration id: the pre-sign-in message-of-the-day read, the reward
	 * claim, and the integration Sign in with Haiven logs in to.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Haiven|MOTD")
	FString IntegrationId;

	/**
	 * The Haiven page that starts a game login, for example
	 * https://app.haiven.gg/connect/game. Leave empty to hide Sign in with Haiven;
	 * HAIVEN_DEMO_ACCESS_TOKEN still works without it.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Haiven|Sign In")
	FString LoginEntryUrl;

	/**
	 * The loopback redirect the game listens on while the browser signs in,
	 * registered exactly on the integration's allowedRedirectUris. Empty uses
	 * DefaultLoginRedirectUrl.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Haiven|Sign In")
	FString LoginRedirectUrl;

	/** The chat channel the panel shows and sends to. */
	UPROPERTY(Config, EditAnywhere, Category = "Haiven|Chat")
	FString ChannelId;

	/** Adds the chat panel to every local player's viewport in game worlds. */
	UPROPERTY(Config, EditAnywhere, Category = "Haiven|Chat")
	bool bShowChatPanel = true;

	/**
	 * Optional host-owned UMG subclass used for the chat panel. Leave unset to
	 * use the demo's code-built panel. The class must inherit
	 * UHaivenLyraChatWidget and bind widgets named Log and Input; invalid,
	 * unavailable or incompatible classes fall back to the code-built panel.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Haiven|Chat")
	TSoftClassPtr<UHaivenLyraChatWidget> ChatWidgetClass;

	/**
	 * The subscriber reward the panel's Claim reward action claims, the game's
	 * own reward_ref (1 to 128 characters). It is claimed for IntegrationId.
	 * Leave empty to hide the action.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Haiven|Rewards")
	FString RewardRef;

	/** Adds the public message-of-the-day panel for every local player. */
	UPROPERTY(Config, EditAnywhere, Category = "Haiven|MOTD")
	bool bShowMotdPanel = true;

	/** Optional host-owned UMG subclass; unusable classes use the native fallback. */
	UPROPERTY(Config, EditAnywhere, Category = "Haiven|MOTD")
	TSoftClassPtr<UHaivenLyraMotdWidget> MotdWidgetClass;

	/** Resolves ChatWidgetClass, returning the code-built class when it is unusable. */
	TSubclassOf<UHaivenLyraChatWidget> ResolveChatWidgetClass() const;

	/** Class-only validation shared by runtime resolution and automation tests. */
	static bool IsSupportedChatWidgetClass(const UClass* Candidate);

	TSubclassOf<UHaivenLyraMotdWidget> ResolveMotdWidgetClass() const;
	static bool IsSupportedMotdWidgetClass(const UClass* Candidate);

	/** Public MOTD is intentionally available without a bearer or socket URL. */
	bool IsMotdConfigured() const { return !BaseUrl.IsEmpty() && !IntegrationId.IsEmpty(); }
	bool IsRealtimeConfigured() const
	{
		return !BaseUrl.IsEmpty() && !RealtimeUrl.IsEmpty() && !AppId.IsEmpty() && !ChannelId.IsEmpty();
	}

	/** The redirect used when LoginRedirectUrl is empty. */
	static const TCHAR* const DefaultLoginRedirectUrl;

	/** LoginRedirectUrl, or DefaultLoginRedirectUrl when it is empty. */
	FString ResolveLoginRedirectUrl() const;

	/** POST /auth/handoff/exchange under BaseUrl. */
	FString ResolveLoginExchangeUrl() const;

	/** Sign in with Haiven needs the API, the integration and the login page. */
	bool IsSignInConfigured() const
	{
		return !BaseUrl.IsEmpty() && !IntegrationId.IsEmpty() && !LoginEntryUrl.IsEmpty();
	}

	/** A reward claim needs the integration it is claimed for and the reward itself. */
	bool IsRewardConfigured() const { return !IntegrationId.IsEmpty() && !RewardRef.IsEmpty(); }

	/** Whether every setting the demo needs to connect is present. */
	bool IsConfigured() const
	{
		return IsMotdConfigured() && IsRealtimeConfigured();
	}
};
