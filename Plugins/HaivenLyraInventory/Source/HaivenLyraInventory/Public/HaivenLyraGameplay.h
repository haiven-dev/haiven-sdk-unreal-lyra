// Chooses and requests the local match PLAY starts. Lyra's own
// CreateHostingRequest always asks for an online lobby, which needs an Epic
// login. Offline travel is the path that does not.

#pragma once

#include "CoreMinimal.h"

class UCommonSession_HostSessionRequest;
class ULyraUserFacingExperienceDefinition;
class UObject;

namespace HaivenLyraGameplay
{
	/** True for the front-end map, including Unreal's PIE name prefix. */
	HAIVENLYRAINVENTORY_API bool IsFrontEndMap(const FString& MapName);

	/**
	 * The match PLAY hosts: the default quick-play experience, otherwise the
	 * first experience shown on the front end. A front-end experience is never
	 * selected. Null when nothing playable is loaded.
	 */
	HAIVENLYRAINVENTORY_API const ULyraUserFacingExperienceDefinition* SelectGameplayExperience(
		const TArray<UObject*>& Loaded);

	/** Copies the experience into an offline host request owned by Outer. */
	HAIVENLYRAINVENTORY_API UCommonSession_HostSessionRequest* MakeOfflineHostRequest(
		const ULyraUserFacingExperienceDefinition& Experience,
		UObject& Outer);
}
