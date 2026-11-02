#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "HaivenLyraMotdProjection.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	const TCHAR* FixtureSet = TEXT("motd-display-v1");
	const TCHAR* Receipt = TEXT("MOTD_FIXTURE_PASS motd-display-v1 11");
	const TCHAR* const ProjectionFields[] = {
		TEXT("revision"), TEXT("status"), TEXT("visible"), TEXT("message"), TEXT("error"), TEXT("dismissedId")};

	// These structured bindings are compile-time inventory assertions. Either
	// aggregate gaining or losing a field makes this translation unit fail.
	void AssertClosedProjectionShape(FHaivenLyraMotdProjection& Value)
	{
		auto& [Revision, Status, Visible, Message, Error, DismissedId] = Value;
		(void)Revision;
		(void)Status;
		(void)Visible;
		(void)Message;
		(void)Error;
		(void)DismissedId;
	}

	void AssertClosedMessageShape(FHaivenLyraMotdPresentationMessage& Value)
	{
		auto& [Id, Title, Body, Url] = Value;
		(void)Id;
		(void)Title;
		(void)Body;
		(void)Url;
	}

	bool ReadMessage(const TSharedPtr<FJsonObject>& Object, FHaivenMotd& Out, FString& Error)
	{
		if (!Object.IsValid() || !Object->TryGetStringField(TEXT("id"), Out.Id) ||
			!Object->TryGetStringField(TEXT("body"), Out.Body))
		{
			Error = TEXT("message requires string id and body");
			return false;
		}
		Out.bHasTitle = Object->HasTypedField<EJson::String>(TEXT("title"));
		if (Out.bHasTitle) Object->TryGetStringField(TEXT("title"), Out.Title);
		Out.bHasUrl = Object->HasTypedField<EJson::String>(TEXT("url"));
		if (Out.bHasUrl) Object->TryGetStringField(TEXT("url"), Out.Url);
		return true;
	}

	bool ReadProjection(const TSharedPtr<FJsonObject>& Object, FHaivenLyraMotdProjection& Out, FString& Error)
	{
		if (!Object.IsValid() || Object->Values.Num() != 6)
		{
			Error = TEXT("projection must contain exactly six fields");
			return false;
		}
		for (const TCHAR* Field : ProjectionFields)
		{
			if (!Object->Values.Contains(Field))
			{
				Error = FString::Printf(TEXT("projection is missing closed field: %s"), Field);
				return false;
			}
		}
		if (!Object->HasTypedField<EJson::Number>(TEXT("revision")) ||
			!Object->TryGetBoolField(TEXT("visible"), Out.Visible))
		{
			Error = TEXT("projection requires numeric revision and boolean visible");
			return false;
		}
		Out.Revision = static_cast<int64>(Object->GetNumberField(TEXT("revision")));

		FString State;
		if (!Object->TryGetStringField(TEXT("status"), State))
		{
			Error = TEXT("projection requires status");
			return false;
		}
		if (State == TEXT("loading"))
			Out.Status = EHaivenLyraMotdPresentationState::Loading;
		else if (State == TEXT("absent"))
			Out.Status = EHaivenLyraMotdPresentationState::Absent;
		else if (State == TEXT("present"))
			Out.Status = EHaivenLyraMotdPresentationState::Present;
		else if (State == TEXT("failure"))
			Out.Status = EHaivenLyraMotdPresentationState::Failure;
		else
		{
			Error = TEXT("projection status is outside the closed vocabulary");
			return false;
		}

		if (Object->HasTypedField<EJson::Object>(TEXT("message")))
		{
			const TSharedPtr<FJsonObject> Message = Object->GetObjectField(TEXT("message"));
			if (Message->Values.Num() < 3 || Message->Values.Num() > 4 ||
				!Message->HasTypedField<EJson::String>(TEXT("id")) ||
				!Message->HasTypedField<EJson::String>(TEXT("title")) ||
				!Message->HasTypedField<EJson::String>(TEXT("body")) ||
				(Message->Values.Num() == 4 && !Message->HasTypedField<EJson::String>(TEXT("url"))))
			{
				Error = TEXT("message projection must contain id, title, body, and optional url");
				return false;
			}
			FHaivenLyraMotdPresentationMessage Projected;
			Message->TryGetStringField(TEXT("id"), Projected.Id);
			Message->TryGetStringField(TEXT("title"), Projected.Title);
			Message->TryGetStringField(TEXT("body"), Projected.Body);
			if (Message->HasTypedField<EJson::String>(TEXT("url")))
				Projected.Url.Emplace(Message->GetStringField(TEXT("url")));
			else if (Message->Values.Contains(TEXT("url")))
			{
				Error = TEXT("message projection url must be a string when present");
				return false;
			}
			Out.Message.Emplace(MoveTemp(Projected));
		}
		else if (!Object->HasTypedField<EJson::Null>(TEXT("message")))
		{
			Error = TEXT("projection message must be an object or null");
			return false;
		}

		else
			Out.Message.Reset();

		if (Object->HasTypedField<EJson::String>(TEXT("error")))
			Out.Error.Emplace(Object->GetStringField(TEXT("error")));
		else if (!Object->HasTypedField<EJson::Null>(TEXT("error")))
		{
			Error = TEXT("projection error must be a string or null");
			return false;
		}
		else
			Out.Error.Reset();

		if (Object->HasTypedField<EJson::String>(TEXT("dismissedId")))
			Out.DismissedId.Emplace(Object->GetStringField(TEXT("dismissedId")));
		else if (!Object->HasTypedField<EJson::Null>(TEXT("dismissedId")))
		{
			Error = TEXT("projection dismissedId must be a string or null");
			return false;
		}
		else
			Out.DismissedId.Reset();
		return true;
	}

	TSharedRef<FJsonObject> ProjectionJson(const FHaivenLyraMotdProjection& Projection)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("revision"), static_cast<double>(Projection.Revision));
		Object->SetStringField(TEXT("status"), HaivenLyraMotdStatusName(Projection.Status));
		Object->SetBoolField(TEXT("visible"), Projection.Visible);
		if (Projection.Message.IsSet())
		{
			const FHaivenLyraMotdPresentationMessage& Message = Projection.Message.GetValue();
			TSharedRef<FJsonObject> MessageObject = MakeShared<FJsonObject>();
			MessageObject->SetStringField(TEXT("id"), Message.Id);
			MessageObject->SetStringField(TEXT("title"), Message.Title);
			MessageObject->SetStringField(TEXT("body"), Message.Body);
			if (Message.Url.IsSet()) MessageObject->SetStringField(TEXT("url"), Message.Url.GetValue());
			Object->SetObjectField(TEXT("message"), MessageObject);
		}
		else
			Object->SetField(TEXT("message"), MakeShared<FJsonValueNull>());
		if (Projection.Error.IsSet())
			Object->SetStringField(TEXT("error"), Projection.Error.GetValue());
		else
			Object->SetField(TEXT("error"), MakeShared<FJsonValueNull>());
		if (Projection.DismissedId.IsSet())
			Object->SetStringField(TEXT("dismissedId"), Projection.DismissedId.GetValue());
		else
			Object->SetField(TEXT("dismissedId"), MakeShared<FJsonValueNull>());
		return Object;
	}

	bool RoundTripProjection(const FHaivenLyraMotdProjection& Projection, FHaivenLyraMotdProjection& Out,
							 FString& Error)
	{
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		if (!FJsonSerializer::Serialize(ProjectionJson(Projection), Writer))
		{
			Error = TEXT("could not serialize public projection");
			return false;
		}
		TSharedPtr<FJsonObject> Object;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Object))
		{
			Error = TEXT("could not deserialize public projection");
			return false;
		}
		return ReadProjection(Object, Out, Error);
	}

	FString Difference(const FHaivenLyraMotdProjection& Actual, const FHaivenLyraMotdProjection& Expected)
	{
		if (Actual.Revision != Expected.Revision)
			return FString::Printf(TEXT("revision %lld != %lld"), Actual.Revision, Expected.Revision);
		if (Actual.Status != Expected.Status)
			return FString::Printf(TEXT("status %s != %s"), *HaivenLyraMotdStatusName(Actual.Status),
								   *HaivenLyraMotdStatusName(Expected.Status));
		if (Actual.Visible != Expected.Visible) return TEXT("visible differs");
		if (Actual.Message.IsSet() != Expected.Message.IsSet()) return TEXT("message presence differs");
		if (Actual.Message.IsSet())
		{
			const FHaivenLyraMotdPresentationMessage& ActualMessage = Actual.Message.GetValue();
			const FHaivenLyraMotdPresentationMessage& ExpectedMessage = Expected.Message.GetValue();
			if (ActualMessage.Id != ExpectedMessage.Id) return TEXT("message id differs");
			if (ActualMessage.Title != ExpectedMessage.Title) return TEXT("message title differs");
			if (ActualMessage.Body != ExpectedMessage.Body) return TEXT("message body differs");
			if (ActualMessage.Url.IsSet() != ExpectedMessage.Url.IsSet()) return TEXT("message url presence differs");
			if (ActualMessage.Url.IsSet() && ActualMessage.Url.GetValue() != ExpectedMessage.Url.GetValue())
				return TEXT("message url differs");
		}
		if (Actual.Error.IsSet() != Expected.Error.IsSet()) return TEXT("error presence differs");
		if (Actual.Error.IsSet() && Actual.Error.GetValue() != Expected.Error.GetValue()) return TEXT("error differs");
		if (Actual.DismissedId.IsSet() != Expected.DismissedId.IsSet()) return TEXT("dismissedId presence differs");
		if (Actual.DismissedId.IsSet() && Actual.DismissedId.GetValue() != Expected.DismissedId.GetValue())
			return TEXT("dismissedId differs");
		return FString();
	}

	bool ApplyInput(FHaivenLyraMotdState& State, const TSharedPtr<FJsonObject>& Input, FString& Error)
	{
		FString Type;
		if (!Input.IsValid() || !Input->TryGetStringField(TEXT("type"), Type))
		{
			Error = TEXT("step input requires a type");
			return false;
		}
		if (Type == TEXT("load_started"))
		{
			State.LoadStarted(Input->GetStringField(TEXT("requestId")));
		}
		else if (Type == TEXT("rest_absent"))
		{
			if (static_cast<int32>(Input->GetNumberField(TEXT("httpStatus"))) != 204)
			{
				Error = TEXT("rest_absent must carry HTTP 204");
				return false;
			}
			State.RestAbsent(Input->GetStringField(TEXT("requestId")));
		}
		else if (Type == TEXT("rest_present") || Type == TEXT("realtime_present"))
		{
			FHaivenMotd Message;
			if (!ReadMessage(Input->GetObjectField(TEXT("message")), Message, Error)) return false;
			if (Type == TEXT("rest_present"))
				State.RestPresent(Input->GetStringField(TEXT("requestId")), Message);
			else
				State.RealtimePresent(Message);
		}
		else if (Type == TEXT("rest_failure"))
		{
			// privateDetail is deliberately not admitted to the player projection.
			State.RestFailure(Input->GetStringField(TEXT("requestId")));
		}
		else if (Type == TEXT("realtime_clear"))
			State.RealtimeClear();
		else if (Type == TEXT("dismiss"))
			State.Dismiss();
		else
		{
			Error = TEXT("unknown input type: ") + Type;
			return false;
		}
		return true;
	}

	bool RunSteps(const TSharedPtr<FJsonObject>& Fixture, FHaivenLyraMotdState& State, FString& Error)
	{
		const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
		if (!Fixture->TryGetArrayField(TEXT("steps"), Steps) || Steps == nullptr)
		{
			Error = TEXT("fixture has no steps");
			return false;
		}
		for (int32 Index = 0; Index < Steps->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject> Step = (*Steps)[Index]->AsObject();
			if (!Step.IsValid() || !ApplyInput(State, Step->GetObjectField(TEXT("input")), Error)) return false;
			FHaivenLyraMotdProjection Expected;
			if (!ReadProjection(Step->GetObjectField(TEXT("expect")), Expected, Error)) return false;
			FHaivenLyraMotdProjection SerializedActual;
			if (!RoundTripProjection(State.GetProjection(), SerializedActual, Error)) return false;
			const FString Mismatch = Difference(SerializedActual, Expected);
			if (!Mismatch.IsEmpty())
			{
				Error = FString::Printf(TEXT("step %d: %s"), Index + 1, *Mismatch);
				return false;
			}
		}
		return true;
	}

	bool PrepareFixture(const TSharedPtr<FJsonObject>& Fixture, const TMap<FString, TSharedPtr<FJsonObject>>& ById,
						FHaivenLyraMotdState& State, TSet<FString>& Visiting, FString& Error)
	{
		if (Fixture->HasTypedField<EJson::Object>(TEXT("initial")))
		{
			FHaivenLyraMotdProjection Initial;
			if (!ReadProjection(Fixture->GetObjectField(TEXT("initial")), Initial, Error)) return false;
			State.Restore(Initial);
			return true;
		}

		FString InitialFixture;
		if (!Fixture->TryGetStringField(TEXT("initial_fixture"), InitialFixture))
		{
			Error = TEXT("fixture requires initial or initial_fixture");
			return false;
		}
		if (Visiting.Contains(InitialFixture))
		{
			Error = TEXT("initial_fixture cycle at ") + InitialFixture;
			return false;
		}
		const TSharedPtr<FJsonObject>* Referenced = ById.Find(InitialFixture);
		if (Referenced == nullptr)
		{
			Error = TEXT("missing initial_fixture: ") + InitialFixture;
			return false;
		}
		Visiting.Add(InitialFixture);
		const bool bPrepared =
			PrepareFixture(*Referenced, ById, State, Visiting, Error) && RunSteps(*Referenced, State, Error);
		Visiting.Remove(InitialFixture);
		return bPrepared;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHaivenLyraMotdRequestSuppressionTest, "Haiven.LyraDemo.MotdRequestSuppression",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHaivenLyraMotdRequestSuppressionTest::RunTest(const FString& Parameters)
{
	FHaivenMotd MessageA;
	MessageA.Id = TEXT("motd-a");
	MessageA.bHasTitle = true;
	MessageA.Title = TEXT("Launch notes");
	MessageA.Body = TEXT("Season two starts today.");

	FHaivenLyraMotdProjection Initial;
	Initial.Revision = 4;
	Initial.Status = EHaivenLyraMotdPresentationState::Present;
	Initial.Visible = true;
	FHaivenLyraMotdPresentationMessage InitialMessage;
	InitialMessage.Id = MessageA.Id;
	InitialMessage.Title = MessageA.Title;
	InitialMessage.Body = MessageA.Body;
	Initial.Message.Emplace(MoveTemp(InitialMessage));
	FHaivenLyraMotdState State;
	State.Restore(Initial);
	TestTrue(TEXT("dismiss accepts the visible id"), State.Dismiss());
	TestTrue(TEXT("a REST load starts after dismissal"), State.LoadStarted(TEXT("request-r")));
	TestFalse(TEXT("suppressed realtime id is a no-op"), State.RealtimePresent(MessageA));
	TestEqual(TEXT("suppressed realtime keeps the REST request loading"),
			  HaivenLyraMotdStatusName(State.GetProjection().Status), FString(TEXT("loading")));
	TestEqual(TEXT("suppressed realtime does not advance revision"), State.GetProjection().Revision, int64(6));
	TestTrue(TEXT("the correlated suppressed REST result settles loading"),
			 State.RestPresent(TEXT("request-r"), MessageA));
	TestEqual(TEXT("the suppressed terminal result settles absent"),
			  HaivenLyraMotdStatusName(State.GetProjection().Status), FString(TEXT("absent")));
	TestEqual(TEXT("the terminal result advances revision once"), State.GetProjection().Revision, int64(7));
	TestTrue(TEXT("dismissal remains sticky"), State.GetProjection().DismissedId.IsSet());
	if (State.GetProjection().DismissedId.IsSet())
		TestEqual(TEXT("dismissal keeps the exact id"), State.GetProjection().DismissedId.GetValue(), MessageA.Id);

	FHaivenLyraMotdState Overlap;
	Overlap.LoadStarted(TEXT("request-1"));
	Overlap.LoadStarted(TEXT("request-2"));
	TestFalse(TEXT("an older same-domain result is stale"), Overlap.RestPresent(TEXT("request-1"), MessageA));
	TestEqual(TEXT("stale result leaves the newer request loading"),
			  HaivenLyraMotdStatusName(Overlap.GetProjection().Status), FString(TEXT("loading")));
	TestTrue(TEXT("the newest request may complete"), Overlap.RestPresent(TEXT("request-2"), MessageA));
	TestEqual(TEXT("the newest result presents"), HaivenLyraMotdStatusName(Overlap.GetProjection().Status),
			  FString(TEXT("present")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHaivenLyraMotdCanonicalFixturesTest, "Haiven.LyraDemo.MotdCanonicalFixtures",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHaivenLyraMotdCanonicalFixturesTest::RunTest(const FString& Parameters)
{
	FHaivenLyraMotdProjection ShapeCheckedProjection;
	FHaivenLyraMotdPresentationMessage ShapeCheckedMessage;
	AssertClosedProjectionShape(ShapeCheckedProjection);
	AssertClosedMessageShape(ShapeCheckedMessage);

	const FString Path = FPaths::ProjectContentDir() / TEXT("HaivenTestFixtures/spec/presentation/motd-v1.json");
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		AddError(TEXT("canonical MOTD fixture file is missing: ") + Path);
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		AddError(TEXT("canonical MOTD fixture file is not valid JSON"));
		return false;
	}
	FString SetName;
	const TArray<TSharedPtr<FJsonValue>>* Fixtures = nullptr;
	if (!Root->TryGetStringField(TEXT("fixture_set"), SetName) || SetName != FixtureSet ||
		!Root->TryGetArrayField(TEXT("fixtures"), Fixtures) || Fixtures == nullptr || Fixtures->Num() != 11)
	{
		AddError(TEXT("canonical MOTD fixture set must be motd-display-v1 with 11 fixtures"));
		return false;
	}

	TMap<FString, TSharedPtr<FJsonObject>> ById;
	for (const TSharedPtr<FJsonValue>& Value : *Fixtures)
	{
		const TSharedPtr<FJsonObject> Fixture = Value->AsObject();
		FString Id;
		if (!Fixture.IsValid() || !Fixture->TryGetStringField(TEXT("id"), Id) || Id.IsEmpty() || ById.Contains(Id))
		{
			AddError(TEXT("fixture ids must be non-empty and unique"));
			return false;
		}
		ById.Add(Id, Fixture);
	}

	for (const TSharedPtr<FJsonValue>& Value : *Fixtures)
	{
		const TSharedPtr<FJsonObject> Fixture = Value->AsObject();
		const FString Id = Fixture->GetStringField(TEXT("id"));
		FHaivenLyraMotdState State;
		TSet<FString> Visiting{Id};
		FString Error;
		if (!PrepareFixture(Fixture, ById, State, Visiting, Error) || !RunSteps(Fixture, State, Error))
		{
			AddError(FString::Printf(TEXT("fixture %s failed: %s"), *Id, *Error));
			return false;
		}
	}

	UE_LOG(LogTemp, Display, TEXT("%s"), Receipt);
	return true;
}

#endif
