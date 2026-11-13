#include "HaivenLyraDemoSettings.h"

#include "HaivenLyraChatWidget.h"
#include "HaivenLyraMotdWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogHaivenLyraDemoSettings, Log, All);

const TCHAR* const UHaivenLyraDemoSettings::DefaultLoginRedirectUrl = TEXT("http://127.0.0.1:28823/haiven/callback");

FString UHaivenLyraDemoSettings::ResolveLoginRedirectUrl() const
{
	return LoginRedirectUrl.IsEmpty() ? FString(DefaultLoginRedirectUrl) : LoginRedirectUrl;
}

FString UHaivenLyraDemoSettings::ResolveLoginExchangeUrl() const
{
	FString Base = BaseUrl;
	Base.RemoveFromEnd(TEXT("/"));
	return Base + TEXT("/auth/handoff/exchange");
}

TArray<FString> UHaivenLyraDemoSettings::ApplyEnvironmentOverrides(TFunctionRef<FString(const TCHAR*)> Read)
{
	const TPair<const TCHAR*, FString*> Overrides[] = {
		{ TEXT("HAIVEN_DEMO_BASE_URL"), &BaseUrl },
		{ TEXT("HAIVEN_DEMO_REALTIME_URL"), &RealtimeUrl },
		{ TEXT("HAIVEN_DEMO_APP_ID"), &AppId },
		{ TEXT("HAIVEN_DEMO_INTEGRATION_ID"), &IntegrationId },
		{ TEXT("HAIVEN_DEMO_CHANNEL_ID"), &ChannelId },
		{ TEXT("HAIVEN_DEMO_REWARD_REF"), &RewardRef },
		// Later names win: the haiven-lyra assembler's names over Boss Room's and legacy ones.
		{ TEXT("HAIVEN_DEMO_LOGIN_ENTRY_URL"), &LoginEntryUrl },
		{ TEXT("HAIVEN_DEMO_SIGN_IN_URL"), &LoginEntryUrl },
		{ TEXT("HAIVEN_DEMO_REDIRECT_URI"), &LoginRedirectUrl },
		{ TEXT("HAIVEN_DEMO_LOGIN_REDIRECT_URL"), &LoginRedirectUrl },
	};
	TArray<FString> Applied;
	for (const TPair<const TCHAR*, FString*>& Override : Overrides)
	{
		const FString Value = Read(Override.Key).TrimStartAndEnd();
		if (Value.IsEmpty()) continue;
		*Override.Value = Value;
		Applied.Add(Override.Key);
	}
	// Names only: values can identify a private stack and never go to the log.
	if (Applied.Num() > 0)
	{
		UE_LOG(LogHaivenLyraDemoSettings, Log, TEXT("Haiven Lyra Demo settings from the environment: %s"),
			*FString::Join(Applied, TEXT(", ")));
	}
	return Applied;
}

bool UHaivenLyraDemoSettings::IsSupportedChatWidgetClass(const UClass* Candidate)
{
	return Candidate
		&& Candidate->IsChildOf(UHaivenLyraChatWidget::StaticClass())
		&& !Candidate->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists);
}

TSubclassOf<UHaivenLyraChatWidget> UHaivenLyraDemoSettings::ResolveChatWidgetClass() const
{
	if (ChatWidgetClass.IsNull())
	{
		return UHaivenLyraChatWidget::StaticClass();
	}

	UClass* Candidate = ChatWidgetClass.LoadSynchronous();
	if (IsSupportedChatWidgetClass(Candidate))
	{
		return Candidate;
	}

	UE_LOG(
		LogHaivenLyraDemoSettings,
		Warning,
		TEXT("ChatWidgetClass '%s' is unavailable or is not a usable UHaivenLyraChatWidget subclass; using the code-built panel."),
		*ChatWidgetClass.ToSoftObjectPath().ToString());
	return UHaivenLyraChatWidget::StaticClass();
}

bool UHaivenLyraDemoSettings::IsSupportedMotdWidgetClass(const UClass* Candidate)
{
	return Candidate
		&& Candidate->IsChildOf(UHaivenLyraMotdWidget::StaticClass())
		&& !Candidate->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists);
}

TSubclassOf<UHaivenLyraMotdWidget> UHaivenLyraDemoSettings::ResolveMotdWidgetClass() const
{
	if (MotdWidgetClass.IsNull()) return UHaivenLyraMotdWidget::StaticClass();

	UClass* Candidate = MotdWidgetClass.LoadSynchronous();
	if (IsSupportedMotdWidgetClass(Candidate)) return Candidate;

	UE_LOG(
		LogHaivenLyraDemoSettings,
		Warning,
		TEXT("MotdWidgetClass '%s' is unavailable or is not a usable UHaivenLyraMotdWidget subclass; using the code-built panel."),
		*MotdWidgetClass.ToSoftObjectPath().ToString());
	return UHaivenLyraMotdWidget::StaticClass();
}
