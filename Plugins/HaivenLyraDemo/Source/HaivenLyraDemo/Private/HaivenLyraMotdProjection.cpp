#include "HaivenLyraMotdProjection.h"

#include "HaivenSubsystem.h"

namespace
{
	const FString FallbackTitle = TEXT("Message of the day");
	const FString FailureCopy = TEXT("Unable to load message of the day.");

	FHaivenLyraMotdPresentationMessage Normalize(const FHaivenMotd& Value)
	{
		FHaivenLyraMotdPresentationMessage Result;
		Result.Id = Value.Id;
		Result.Title = Value.bHasTitle && !Value.Title.IsEmpty() ? Value.Title : FallbackTitle;
		Result.Body = Value.Body;
		if (Value.bHasUrl && UHaivenSubsystem::IsValidMotdActionUrl(Value.Url))
		{
			Result.Url.Emplace(Value.Url);
		}
		return Result;
	}

	FHaivenLyraMotdPresentationMessage Normalize(const FHaivenLyraMotdPresentationMessage& Value)
	{
		FHaivenLyraMotdPresentationMessage Result = Value;
		if (Result.Title.IsEmpty()) Result.Title = FallbackTitle;
		if (Result.Url.IsSet() && !UHaivenSubsystem::IsValidMotdActionUrl(Result.Url.GetValue())) Result.Url.Reset();
		return Result;
	}
} // namespace

FString HaivenLyraMotdStatusName(EHaivenLyraMotdPresentationState Status)
{
	switch (Status)
	{
	case EHaivenLyraMotdPresentationState::Loading: return TEXT("loading");
	case EHaivenLyraMotdPresentationState::Absent: return TEXT("absent");
	case EHaivenLyraMotdPresentationState::Present: return TEXT("present");
	case EHaivenLyraMotdPresentationState::Failure: return TEXT("failure");
	}
	return TEXT("failure");
}

void FHaivenLyraMotdState::Restore(const FHaivenLyraMotdProjection& Value)
{
	Projection = Value;
	if (Projection.Message.IsSet()) Projection.Message = Normalize(Projection.Message.GetValue());
	ClearActiveRequest();
}

bool FHaivenLyraMotdState::LoadStarted(const FString& RequestId)
{
	if (bHasActiveRequest && ActiveRequestId == RequestId &&
		Projection.Status == EHaivenLyraMotdPresentationState::Loading)
	{
		return false;
	}
	bHasActiveRequest = true;
	ActiveRequestId = RequestId;
	Commit(EHaivenLyraMotdPresentationState::Loading, true);
	return true;
}

bool FHaivenLyraMotdState::RestAbsent(const FString& RequestId)
{
	if (!IsCurrentRequest(RequestId)) return false;
	ClearActiveRequest();
	Commit(EHaivenLyraMotdPresentationState::Absent, false);
	return true;
}

bool FHaivenLyraMotdState::RestPresent(const FString& RequestId, const FHaivenMotd& Message)
{
	if (!IsCurrentRequest(RequestId)) return false;
	ClearActiveRequest();
	if (IsSuppressed(Message))
	{
		// A terminal REST response must settle its loading state even when the
		// returned id remains dismissed.
		if (Projection.Status == EHaivenLyraMotdPresentationState::Loading)
		{
			Commit(EHaivenLyraMotdPresentationState::Absent, false);
			return true;
		}
		return false;
	}
	return PresentNormalized(Message);
}

bool FHaivenLyraMotdState::RestFailure(const FString& RequestId)
{
	if (!IsCurrentRequest(RequestId)) return false;
	ClearActiveRequest();
	return PresentFailure();
}

bool FHaivenLyraMotdState::RealtimePresent(const FHaivenMotd& Message)
{
	// A suppressed id is not accepted state and therefore cannot supersede an
	// older in-flight REST request.
	if (IsSuppressed(Message)) return false;
	ClearActiveRequest();
	return PresentNormalized(Message);
}

bool FHaivenLyraMotdState::RealtimeClear()
{
	ClearActiveRequest();
	if (Projection.Status == EHaivenLyraMotdPresentationState::Absent && !Projection.Visible &&
		!Projection.Message.IsSet() && !Projection.Error.IsSet())
	{
		return false;
	}
	Commit(EHaivenLyraMotdPresentationState::Absent, false);
	return true;
}

bool FHaivenLyraMotdState::Dismiss()
{
	if (Projection.Status != EHaivenLyraMotdPresentationState::Present || !Projection.Visible ||
		!Projection.Message.IsSet())
	{
		return false;
	}
	Projection.DismissedId.Emplace(Projection.Message.GetValue().Id);
	ClearActiveRequest();
	Commit(EHaivenLyraMotdPresentationState::Absent, false);
	return true;
}

bool FHaivenLyraMotdState::PresentFailure()
{
	if (Projection.Status == EHaivenLyraMotdPresentationState::Failure && Projection.Visible &&
		Projection.Error.IsSet() && Projection.Error.GetValue() == FailureCopy)
	{
		return false;
	}
	Commit(EHaivenLyraMotdPresentationState::Failure, true, nullptr, &FailureCopy);
	return true;
}

bool FHaivenLyraMotdState::IsCurrentRequest(const FString& RequestId) const
{
	return bHasActiveRequest && ActiveRequestId == RequestId;
}

bool FHaivenLyraMotdState::IsSuppressed(const FHaivenMotd& Message) const
{
	return (Projection.DismissedId.IsSet() && Projection.DismissedId.GetValue() == Message.Id) ||
		   (Projection.Status == EHaivenLyraMotdPresentationState::Present && Projection.Message.IsSet() &&
			Projection.Message.GetValue().Id == Message.Id);
}

bool FHaivenLyraMotdState::PresentNormalized(const FHaivenMotd& Message)
{
	if (IsSuppressed(Message)) return false;
	const FHaivenLyraMotdPresentationMessage Normalized = Normalize(Message);
	Commit(EHaivenLyraMotdPresentationState::Present, true, &Normalized);
	return true;
}

void FHaivenLyraMotdState::ClearActiveRequest()
{
	bHasActiveRequest = false;
	ActiveRequestId.Reset();
}

void FHaivenLyraMotdState::Commit(EHaivenLyraMotdPresentationState Status, bool Visible,
								  const FHaivenLyraMotdPresentationMessage* Message, const FString* Error)
{
	++Projection.Revision;
	Projection.Status = Status;
	Projection.Visible = Visible;
	if (Message)
		Projection.Message.Emplace(*Message);
	else
		Projection.Message.Reset();
	if (Error)
		Projection.Error.Emplace(*Error);
	else
		Projection.Error.Reset();
}
