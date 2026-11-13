// The client half of the entitlement grant: listens for the demo's
// OnItemEntitled and forwards each code to the server through this player's
// UHaivenLyraInventoryBridge. Codes that arrive before the bridge has
// replicated wait until it begins play. A replacement controller also replays
// the current active projection so travel cannot lose an earlier entitlement.

#pragma once

#include "GameFramework/GameplayMessageSubsystem.h"
#include "Subsystems/LocalPlayerSubsystem.h"

#include "HaivenLyraInventoryClient.generated.h"

class UHaivenLyraInventoryBridge;
struct FGameplayTag;
struct FLyraVerbMessage;
enum class EHaivenLyraSignInState : uint8;

UCLASS()
class HAIVENLYRAINVENTORY_API UHaivenLyraInventoryClient : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Called by this player's bridge once it exists on the client. */
	void BridgeReady(UHaivenLyraInventoryBridge* Bridge);

	/** Shows a grant outcome in the demo's chat panel. */
	void ShowOutcome(const FString& Code, const FString& Outcome) const;

private:
	UFUNCTION()
	void HandleItemEntitled(const FString& Code);

	/** PLAY: host the default experience offline so this player enters the match. */
	UFUNCTION()
	void PlayGameplay();

	/**
	 * The match reward, Lyra's counterpart of Boss Room's victory reward: this
	 * player's first elimination in a session claims the configured RewardRef
	 * once. Signed out, the claim waits for sign-in. Lyra broadcasts
	 * eliminations on the server, so this runs for the host and offline play.
	 */
	void HandleElimination(FGameplayTag Channel, const FLyraVerbMessage& Message);

	UFUNCTION()
	void HandleSignInChanged(EHaivenLyraSignInState State);

	void ClaimMatchReward();

	UHaivenLyraInventoryBridge* FindBridge() const;

	TArray<FString> Pending;
	FGameplayMessageListenerHandle EliminationListener;
	bool bMatchRewardEarned = false;
	bool bMatchRewardWaitingForSignIn = false;
};
