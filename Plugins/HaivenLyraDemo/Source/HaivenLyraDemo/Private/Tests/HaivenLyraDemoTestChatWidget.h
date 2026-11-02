#pragma once

#include "HaivenLyraChatWidget.h"
#include "HaivenLyraMotdWidget.h"

#include "HaivenLyraDemoTestChatWidget.generated.h"

/** Native subclass used only to prove configured class selection in automation. */
UCLASS()
class UHaivenLyraDemoTestChatWidget final : public UHaivenLyraChatWidget
{
	GENERATED_BODY()
};

UCLASS()
class UHaivenLyraDemoTestMotdWidget final : public UHaivenLyraMotdWidget
{
	GENERATED_BODY()
};
