// The client half of the entitlement grant: listens for the demo's
// OnItemEntitled and forwards each code to the server through this player's
// UHaivenLyraInventoryBridge. Codes that arrive before the bridge has
// replicated wait until it begins play. A replacement controller also replays
// the current active projection so travel cannot lose an earlier entitlement.

#pragma once

#include "Subsystems/LocalPlayerSubsystem.h"

#include "HaivenLyraInventoryClient.generated.h"

class UHaivenLyraInventoryBridge;

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

	UHaivenLyraInventoryBridge* FindBridge() const;

	TArray<FString> Pending;
};
