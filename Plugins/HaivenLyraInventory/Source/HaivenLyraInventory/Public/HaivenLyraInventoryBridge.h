// The server half of the entitlement grant. Lyra's inventory manager lives on
// the player controller and its add functions are BlueprintAuthorityOnly, so a
// client that learns of an entitlement has to ask the server to add the item.
// This component is that channel: the server adds it to every player
// controller at login, and it replicates to the owning client.
//
// Remote clients are rejected because the current Haiven API response carries
// no server-verifiable grant proof. A shipped multiplayer game must verify a
// player-bound grant through its trusted backend before granting anything.

#pragma once

#include "Components/ActorComponent.h"

#include "HaivenLyraInventoryBridge.generated.h"

UCLASS()
class HAIVENLYRAINVENTORY_API UHaivenLyraInventoryBridge : public UActorComponent
{
	GENERATED_BODY()

public:
	UHaivenLyraInventoryBridge();

	/** Grants a locally owned listen-host item; remote requests fail closed. */
	UFUNCTION(Server, Reliable)
	void ServerGrantEntitledItem(const FString& Code);

protected:
	virtual void BeginPlay() override;

private:
	/** Adds the mapped item now; the inventory component must already exist. */
	void Grant(const FString& Code);
	void GrantAfterExperienceLoaded(const class ULyraExperienceDefinition* Experience, FString Code);

	/** Tells the owning client how the grant went, for its chat panel. */
	UFUNCTION(Client, Reliable)
	void ClientGrantResult(const FString& Code, const FString& Outcome);
};
