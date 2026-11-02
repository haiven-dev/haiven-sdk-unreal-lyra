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
