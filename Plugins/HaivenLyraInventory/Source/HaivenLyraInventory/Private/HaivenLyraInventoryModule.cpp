#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "HaivenLyraInventoryBridge.h"
#include "Modules/ModuleManager.h"

/** Adds the grant bridge to every player controller as it logs in on the server. */
class FHaivenLyraInventoryModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		PostLogin = FGameModeEvents::GameModePostLoginEvent.AddStatic(&FHaivenLyraInventoryModule::AddBridge);
	}

	virtual void ShutdownModule() override
	{
		FGameModeEvents::GameModePostLoginEvent.Remove(PostLogin);
	}

private:
	static void AddBridge(AGameModeBase* GameMode, APlayerController* NewPlayer)
	{
		if (!NewPlayer || NewPlayer->FindComponentByClass<UHaivenLyraInventoryBridge>()) return;
		UHaivenLyraInventoryBridge* Bridge = NewObject<UHaivenLyraInventoryBridge>(NewPlayer, TEXT("HaivenLyraInventoryBridge"));
		Bridge->RegisterComponent();
	}

	FDelegateHandle PostLogin;
};

IMPLEMENT_MODULE(FHaivenLyraInventoryModule, HaivenLyraInventory)
