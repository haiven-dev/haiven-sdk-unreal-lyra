// Turns an entitlement read into the item grants the game has not made yet.
// Pure, so the rule the demo depends on is testable without a server.

#pragma once

#include "Blueprint/HaivenBlueprintRestTypes.h"

namespace HaivenLyraEntitlements
{
	/** The entitlement kind that maps to an inventory item (haiven-api KindItem). */
	inline const TCHAR* ItemKind = TEXT("item");

	/** The only state the account API returns for a held benefit (haiven-api StateActive). */
	inline const TCHAR* ActiveState = TEXT("active");

	/**
	 * Codes of active item entitlements in Response that are not in AlreadyGranted,
	 * in response order and without duplicates. Subscriptions and founder status
	 * are not items, so they never become grants.
	 */
	HAIVENLYRADEMO_API TArray<FString> NewlyActiveItems(
		const FHaivenRestAccountEntitlementsResponse& Response,
		const TSet<FString>& AlreadyGranted);
}
