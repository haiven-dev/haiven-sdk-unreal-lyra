#include "HaivenLyraEntitlements.h"

TArray<FString> HaivenLyraEntitlements::NewlyActiveItems(
	const FHaivenRestAccountEntitlementsResponse& Response,
	const TSet<FString>& AlreadyGranted)
{
	TArray<FString> Codes;
	if (!Response.bHasEntitlements) return Codes;
	for (const FHaivenRestEntitlementResponse& Entitlement : Response.Entitlements)
	{
		if (!Entitlement.bHasCode || Entitlement.Code.IsEmpty()) continue;
		if (!Entitlement.bHasKind || Entitlement.Kind != ItemKind) continue;
		if (!Entitlement.bHasState || Entitlement.State != ActiveState) continue;
		if (AlreadyGranted.Contains(Entitlement.Code)) continue;
		Codes.AddUnique(Entitlement.Code);
	}
	return Codes;
}
