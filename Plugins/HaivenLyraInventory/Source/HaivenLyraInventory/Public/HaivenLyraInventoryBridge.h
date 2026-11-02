// The server half of the entitlement grant. Lyra's inventory manager lives on
// the player controller and its add functions are BlueprintAuthorityOnly, so a
// client that learns of an entitlement has to ask the server to add the item.
// This component is that channel: the server adds it to every player
// controller at login, and it replicates to the owning client.
//
// DEMO ONLY: the server trusts the client's claim that the account holds the
// entitlement. A shipped game must verify entitlements server-side, from its
// own backend session, before granting anything.

#pragma once

#include "Components/ActorComponent.h"

#include "HaivenLyraInventoryBridge.generated.h"

UCLASS()
class HAIVENLYRAINVENTORY_API UHaivenLyraInventoryBridge : public UActorComponent
{
	GENERATED_BODY()

public:
	UHaivenLyraInventoryBridge();

	/** Asks the server to grant the item mapped to Code. */
	UFUNCTION(Server, Reliable)
	void ServerGrantEntitledItem(const FString& Code);

protected:
	virtual void BeginPlay() override;

private:
	/** Tells the owning client how the grant went, for its chat panel. */
	UFUNCTION(Client, Reliable)
	void ClientGrantResult(const FString& Code, const FString& Outcome);
};
