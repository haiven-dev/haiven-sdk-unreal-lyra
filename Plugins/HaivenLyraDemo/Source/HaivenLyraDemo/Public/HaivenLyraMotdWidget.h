// Source-only MOTD presentation. Hosts may replace the native widget tree with
// a UMG subclass while retaining the state and security behavior here.

#pragma once

#include "Blueprint/UserWidget.h"
#include "HaivenLyraMotdProjection.h"

#include "HaivenLyraMotdWidget.generated.h"

class UButton;
class UBorder;
class UTextBlock;

UCLASS(Blueprintable)
class HAIVENLYRADEMO_API UHaivenLyraMotdWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Render the shipped request-aware projection without reinterpreting it. */
	void PresentProjection(const FHaivenLyraMotdProjection& Projection);

	// Convenience methods retained for host subclasses and focused UI tests.
	void PresentLoading();
	void PresentAbsent();
	void PresentMessage(const FHaivenMotd& Message);
	void PresentFailure();

	/** Collapses the MOTD drawer while retaining its handle. */
	void CollapseDrawer();

	/** Stable cross-engine state spelling used by tests and designer logic. */
	UFUNCTION(BlueprintPure, Category = "Haiven|MOTD")
	FString GetPresentationStateName() const;

	UFUNCTION(BlueprintPure, Category = "Haiven|MOTD")
	bool HasAction() const { return !ActionUrl.IsEmpty(); }

	UFUNCTION(BlueprintPure, Category = "Haiven|MOTD")
	FText GetDisplayedStatus() const;

	UFUNCTION(BlueprintPure, Category = "Haiven|MOTD")
	FText GetDisplayedTitle() const;

	UFUNCTION(BlueprintPure, Category = "Haiven|MOTD")
	FText GetDisplayedBody() const;

	/** True when every required host-owned UMG binding is available. */
	bool HasDesignerBindings() const;
	bool UsesCodeBuiltTree() const { return bUsesCodeBuiltTree; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Haiven|MOTD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Status;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Haiven|MOTD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Title;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Haiven|MOTD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Body;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Haiven|MOTD", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Action;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Haiven|MOTD", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Dismiss;

private:
	UFUNCTION()
	void HandleActionClicked();

	UFUNCTION()
	void HandleDismissClicked();

	UFUNCTION()
	void HandleToggleClicked();

	UFUNCTION()
	void HandleCollapseClicked();

	void ApplyPresentation();

	FHaivenLyraMotdState LocalState;
	FHaivenLyraMotdProjection CurrentProjection;
	FString ActionUrl;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Handle;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Collapse;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Drawer;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HandleLabel;

	bool bExpanded = false;
	bool bUnread = false;
	bool bUsesCodeBuiltTree = false;
};
