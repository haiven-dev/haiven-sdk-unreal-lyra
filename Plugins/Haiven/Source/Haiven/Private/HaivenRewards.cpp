#include "HaivenRewards.h"

#include "Blueprint/HaivenBlueprintRestConvert.h"
#include "HaivenSubsystem.h"

#include <array>
#include <cstdint>
#include <string>

// Named rather than anonymous: a unity build joins this file with the plugin's
// others, and an anonymous helper of the same name there would collide.
namespace HaivenRewardsDetail
{
	std::string ToUtf8(const FString& Value)
	{
		FTCHARToUTF8 Converted(*Value);
		return std::string(Converted.Get(), Converted.Length());
	}

	FString FromUtf8(const std::string& Value)
	{
		return FString(UTF8_TO_TCHAR(Value.c_str()));
	}

	/** A UUIDv7: uniqueness is all a claim id needs, so a GUID's bytes are the random part. */
	std::string NewClaimId()
	{
		const FTimespan SinceEpoch = FDateTime::UtcNow() - FDateTime(1970, 1, 1);
		const FGuid Guid = FGuid::NewGuid();
		const uint32 Words[4] = {Guid.A, Guid.B, Guid.C, Guid.D};
		std::array<std::uint8_t, 10> Random{};
		for (int32 Index = 0; Index < 10; ++Index)
			Random[Index] = static_cast<std::uint8_t>((Words[Index / 4] >> (8 * (Index % 4))) & 0xFF);
		return Haiven::FormatUuidV7(static_cast<std::uint64_t>(SinceEpoch.GetTotalMilliseconds()), Random);
	}
}

FHaivenRewards::FHaivenRewards()
	: FHaivenRewards([]() { return HaivenRewardsDetail::NewClaimId(); })
{
}

FHaivenRewards::FHaivenRewards(Haiven::FRewardClaims::FClaimIdSource NewClaimId)
	: Core(MoveTemp(NewClaimId))
{
}

bool FHaivenRewards::ClaimRequest(const FString& IntegrationId, const FString& RewardRef, FHaivenRestRequest& OutRequest, FString& OutError)
{
	using namespace HaivenRewardsDetail;
	// Checked here, as the core checks them, so nothing below throws.
	const std::string Integration = ToUtf8(IntegrationId);
	const std::string Reward = ToUtf8(RewardRef);
	if (Integration.empty())
	{
		OutError = TEXT("An integration id is required.");
		return false;
	}
	if (Reward.empty() || Haiven::Utf8CodePointCount(Reward) > 128)
	{
		OutError = TEXT("A reward ref must be 1 to 128 characters.");
		return false;
	}
	const Haiven::Rest::FRestRequest Request = Core.ClaimRequest(Integration, Reward);
	OutRequest.Method = FromUtf8(Request.Method);
	OutRequest.Path = FromUtf8(Request.Path);
	OutRequest.Body = FromUtf8(Request.Body);
	OutRequest.ContentType = FromUtf8(Request.ContentType);
	OutRequest.ResponseSchema = FromUtf8(Request.ResponseSchema);
	OutRequest.bNavigation = Request.bNavigation;
	return true;
}

TOptional<FHaivenRewardClaimOutcome> FHaivenRewards::HandleClaimReply(
	uint64 RequestSessionGeneration,
	const FString& IntegrationId,
	const FString& RewardRef,
	const FString& RequestClaimId,
	const FHaivenHttpResult& Result)
{
	using namespace HaivenRewardsDetail;
	// A transport failure has no status; the core reads status 0 as one.
	const int Status = Result.StatusCode;
	const std::optional<Haiven::FRewardClaimOutcome> CoreOutcome = Core.HandleClaimReply(
		RequestSessionGeneration, ToUtf8(IntegrationId), ToUtf8(RewardRef), ToUtf8(RequestClaimId), Status, ToUtf8(Result.Body));
	if (!CoreOutcome.has_value()) return TOptional<FHaivenRewardClaimOutcome>();
	FHaivenRewardClaimOutcome Outcome = ToBlueprint(*CoreOutcome);
	if (Status == 0 && !Result.Error.IsEmpty()) Outcome.Error = Result.Error;
	return Outcome;
}

bool FHaivenRewards::GetPendingClaimId(const FString& IntegrationId, const FString& RewardRef, FString& OutClaimId) const
{
	using namespace HaivenRewardsDetail;
	const std::optional<std::string> Pending = Core.Pending(ToUtf8(IntegrationId), ToUtf8(RewardRef));
	if (!Pending.has_value()) return false;
	OutClaimId = FromUtf8(*Pending);
	return true;
}

void FHaivenRewards::Clear()
{
	Core.Clear();
}

FHaivenRewardClaimOutcome FHaivenRewards::ToBlueprint(const Haiven::FRewardClaimOutcome& Outcome)
{
	using namespace HaivenRewardsDetail;
	FHaivenRewardClaimOutcome Out;
	Out.Result = ToBlueprint(Outcome.Result);
	Out.StatusCode = Outcome.Status;
	Out.bHasClaim = Outcome.Claim.has_value();
	if (Out.bHasClaim) Out.Claim = Haiven::Blueprint::FromCpp(*Outcome.Claim);
	Out.Confirmation = ToBlueprint(Outcome.Confirmation);
	Out.ErrorCode = FromUtf8(Outcome.ErrorCode);
	Out.Error = FromUtf8(Outcome.Error);
	Out.bRetryable = Outcome.bRetryable;
	return Out;
}

EHaivenRewardClaimResult FHaivenRewards::ToBlueprint(Haiven::ERewardClaimResult Result)
{
	switch (Result)
	{
	case Haiven::ERewardClaimResult::Claimed: return EHaivenRewardClaimResult::Claimed;
	case Haiven::ERewardClaimResult::AlreadyRecorded: return EHaivenRewardClaimResult::AlreadyRecorded;
	case Haiven::ERewardClaimResult::NotEligible: return EHaivenRewardClaimResult::NotEligible;
	case Haiven::ERewardClaimResult::AlreadyClaimed: return EHaivenRewardClaimResult::AlreadyClaimed;
	case Haiven::ERewardClaimResult::ClaimConflict: return EHaivenRewardClaimResult::ClaimConflict;
	case Haiven::ERewardClaimResult::RefundInProgress: return EHaivenRewardClaimResult::RefundInProgress;
	case Haiven::ERewardClaimResult::NotFound: return EHaivenRewardClaimResult::NotFound;
	case Haiven::ERewardClaimResult::Failed: break;
	}
	return EHaivenRewardClaimResult::Failed;
}

EHaivenRewardConfirmation FHaivenRewards::ToBlueprint(Haiven::ERewardConfirmation Confirmation)
{
	switch (Confirmation)
	{
	case Haiven::ERewardConfirmation::ClientReported: return EHaivenRewardConfirmation::ClientReported;
	case Haiven::ERewardConfirmation::ServerConfirmed: return EHaivenRewardConfirmation::ServerConfirmed;
	case Haiven::ERewardConfirmation::Unrecognized: break;
	}
	return EHaivenRewardConfirmation::Unrecognized;
}

UHaivenClaimRewardAsyncAction* UHaivenClaimRewardAsyncAction::ClaimReward(
	UObject* WorldContextObject,
	UHaivenSubsystem* Subsystem,
	const FString& IntegrationId,
	const FString& RewardRef)
{
	UHaivenClaimRewardAsyncAction* Action = NewObject<UHaivenClaimRewardAsyncAction>();
	Action->Owner = Subsystem;
	Action->Integration = IntegrationId;
	Action->Reward = RewardRef;
	Action->RegisterWithGameInstance(WorldContextObject);
	return Action;
}

void UHaivenClaimRewardAsyncAction::Activate()
{
	FHaivenRewards* Rewards = Owner ? Owner->GetRewards() : nullptr;
	if (!Rewards)
	{
		Fail(TEXT("The Haiven subsystem is unavailable."));
		return;
	}
	FHaivenRestRequest Request;
	FString Error;
	if (!Rewards->ClaimRequest(Integration, Reward, Request, Error))
	{
		Fail(Error);
		return;
	}
	if (!Rewards->GetPendingClaimId(Integration, Reward, ClaimId))
	{
		Fail(TEXT("The reward claim did not retain its request id."));
		return;
	}
	SessionGeneration = Rewards->GetSessionGeneration();
	Handle = Owner->Request(Request.Method, Request.Path, Request.Body, FHaivenHttpComplete::CreateUObject(this, &UHaivenClaimRewardAsyncAction::Finish));
}

void UHaivenClaimRewardAsyncAction::Cancel()
{
	// The pending claim id is kept: the request may still have been recorded.
	if (Owner && Handle.IsValid()) Owner->CancelHttpRequest(Handle);
	SetReadyToDestroy();
}

void UHaivenClaimRewardAsyncAction::Finish(const FHaivenHttpResult& Result)
{
	FHaivenRewards* Rewards = Owner ? Owner->GetRewards() : nullptr;
	if (!Rewards)
	{
		Fail(TEXT("The Haiven subsystem is unavailable."));
		return;
	}
	// Logout clears rewards and advances their session before the next player can claim.
	// Reject this completion before it can mutate that player's pending id or broadcast.
	if (SessionGeneration != Rewards->GetSessionGeneration())
	{
		Fail(TEXT("The reward claim was invalidated because the player signed out."));
		return;
	}
	const TOptional<FHaivenRewardClaimOutcome> MaybeOutcome = Rewards->HandleClaimReply(
		SessionGeneration, Integration, Reward, ClaimId, Result);
	if (!MaybeOutcome.IsSet())
	{
		Fail(TEXT("The reward claim was invalidated because the player signed out."));
		return;
	}
	const FHaivenRewardClaimOutcome& Outcome = MaybeOutcome.GetValue();
	switch (Outcome.Result)
	{
	case EHaivenRewardClaimResult::Claimed:
	case EHaivenRewardClaimResult::AlreadyRecorded:
		Owner->OnRewardClaimed.Broadcast(Outcome);
		Rewards = Owner ? Owner->GetRewards() : nullptr;
		if (!Rewards || SessionGeneration != Rewards->GetSessionGeneration())
		{
			Fail(TEXT("The reward claim was invalidated because the player signed out."));
			return;
		}
		Claimed.Broadcast(Outcome);
		break;
	case EHaivenRewardClaimResult::NotEligible:
	case EHaivenRewardClaimResult::AlreadyClaimed:
	case EHaivenRewardClaimResult::ClaimConflict:
	case EHaivenRewardClaimResult::RefundInProgress:
		Refused.Broadcast(Outcome);
		break;
	default:
		Failed.Broadcast(Outcome);
		break;
	}
	SetReadyToDestroy();
}

void UHaivenClaimRewardAsyncAction::Fail(const FString& Error)
{
	FHaivenRewardClaimOutcome Outcome;
	Outcome.Error = Error;
	Failed.Broadcast(Outcome);
	SetReadyToDestroy();
}
