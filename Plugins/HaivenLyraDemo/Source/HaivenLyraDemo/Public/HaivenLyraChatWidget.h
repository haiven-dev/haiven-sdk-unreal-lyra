// The demo's HUD panel: a status header (player, sign-in, connection, last
// message's moderation outcome, reward), the chat log and input, and the Sign in
// with Haiven and Claim reward actions. The base class builds its widget tree in code, so the plugin carries
// no binary content. A host project may select a UMG subclass with matching
// optional bindings while retaining this class as the fallback.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"

#include "HaivenLyraChatWidget.generated.h"

class UButton;
class UEditableTextBox;
class UScrollBox;
class UTextBlock;

UCLASS(Blueprintable)
class HAIVENLYRADEMO_API UHaivenLyraChatWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Appends one line, for a message from the channel or a status note. */
	void AddLine(const FText& Line);

	/**
	 * Shows a line keyed by a message this player sent, replacing the earlier
	 * line for the same key, so one message moves through sending, accepted,
	 * held or refused in place.
	 */
	void SetKeyedLine(const FString& Key, const FText& Line);

	/** Gives the text input keyboard focus. */
	void FocusInput();

	/** Shows the HUD status lines in the optional Status text. */
	void SetStatus(const FText& Lines);

	/** Shows or hides the optional ClaimReward button, and enables it while no claim is in flight. */
	void SetRewardAction(bool bVisible, bool bEnabled);

	/** Shows or hides the optional SignIn button and sets what its label says it does. */
	void SetSignInAction(bool bVisible, const FText& Label);

	/** True when the SignIn button exists and is shown. */
	UFUNCTION(BlueprintPure, Category = "Haiven|Lyra Demo")
	bool IsSignInActionAvailable() const;

	/** The SignIn button's label last set: Sign in with Haiven, Cancel sign-in, Sign out or Retry sign-in. */
	UFUNCTION(BlueprintPure, Category = "Haiven|Lyra Demo")
	FText GetSignInActionLabel() const { return SignInActionLabel; }

	/** The status lines last shown, for tests and designer logic. */
	UFUNCTION(BlueprintPure, Category = "Haiven|Lyra Demo")
	FText GetDisplayedStatus() const { return DisplayedStatus; }

	/** True when the ClaimReward button exists, is shown and can be pressed. */
	UFUNCTION(BlueprintPure, Category = "Haiven|Lyra Demo")
	bool IsRewardActionAvailable() const;

	/** True when the required Log and Input controls are available. */
	bool HasDesignerBindings() const { return Log != nullptr && Input != nullptr; }

	/** True when this instance is using the source-only native presentation. */
	bool UsesCodeBuiltTree() const { return bUsesCodeBuiltTree; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** Optional UMG binding. A custom Widget Blueprint names its ScrollBox Log. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Haiven|Chat", meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> Log;

	/** Optional UMG binding. A custom Widget Blueprint names its EditableTextBox Input. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Haiven|Chat", meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> Input;

	/** Optional UMG binding. A TextBlock named Status shows the player, connection, moderation and reward lines. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Haiven|Chat", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Status;

	/** Optional UMG binding. A Button named ClaimReward claims the configured reward. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Haiven|Chat", meta = (BindWidgetOptional))
	TObjectPtr<UButton> ClaimReward;

	/** Optional UMG binding. A Button named SignIn signs in, cancels, signs out or retries, by state. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Haiven|Chat", meta = (BindWidgetOptional))
	TObjectPtr<UButton> SignIn;

	/** Optional UMG binding. A TextBlock named SignInLabel shows what SignIn does now. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Haiven|Chat", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SignInLabel;

private:
	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleClaimRewardClicked();

	UFUNCTION()
	void HandleSignInClicked();

	void ApplyRewardAction();
	void ApplySignInAction();

	UTextBlock* MakeLine(const FText& Line);

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UTextBlock>> KeyedLines;

	FText DisplayedStatus;
	FText SignInActionLabel;
	bool bSignInVisible = false;
	bool bRewardVisible = false;
	bool bRewardEnabled = false;
	bool bUsesCodeBuiltTree = false;
};
