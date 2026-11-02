#include "HaivenLyraSignIn.h"

#include "Dom/JsonObject.h"
#include "HaivenLyraDemoSettings.h"
#include "Misc/Base64.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace HaivenLyraSignIn
{
	const FTimespan UnknownTokenLifetime = FTimespan::FromMinutes(30.0);

	FDateTime DemoTokenExpiry(const FString& Token, const FDateTime& Now, bool& bFromToken)
	{
		bFromToken = false;
		TArray<FString> Parts;
		if (Token.ParseIntoArray(Parts, TEXT("."), false) == 3 && !Parts[1].IsEmpty())
		{
			FString Payload = Parts[1];
			while (Payload.Len() % 4 != 0) Payload.AppendChar(TEXT('='));
			FString Json;
			TSharedPtr<FJsonObject> Claims;
			double Exp = 0.0;
			if (FBase64::Decode(Payload, Json, EBase64Mode::UrlSafe)
				&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Claims)
				&& Claims.IsValid()
				&& Claims->TryGetNumberField(TEXT("exp"), Exp)
				&& Exp > 0.0)
			{
				bFromToken = true;
				return FDateTime::FromUnixTimestamp(static_cast<int64>(Exp));
			}
		}
		return Now + UnknownTokenLifetime;
	}

	FHaivenGameLoginOptions LoginOptions(const UHaivenLyraDemoSettings& Settings)
	{
		FHaivenGameLoginOptions Options;
		Options.EntryUrl = Settings.LoginEntryUrl;
		Options.ExchangeUrl = Settings.ResolveLoginExchangeUrl();
		Options.IntegrationId = Settings.IntegrationId;
		Options.RedirectUrl = Settings.ResolveLoginRedirectUrl();
		// A player who walks away leaves the port open no longer than this.
		Options.TransactionLifetimeSeconds = 600.0f;
		return Options;
	}
}
