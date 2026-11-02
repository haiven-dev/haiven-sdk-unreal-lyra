#include "HaivenLyraMotdWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/LocalPlayer.h"
#include "HaivenLyraDemoSubsystem.h"
#include "HaivenSubsystem.h"

namespace
{
	const FText LoadingCopy = NSLOCTEXT("HaivenLyraDemo", "MotdLoading", "Loading message of the day...");
	const FText AbsentCopy = NSLOCTEXT("HaivenLyraDemo", "MotdAbsent", "No message of the day.");
} // namespace

TSharedRef<SWidget> UHaivenLyraMotdWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		bUsesCodeBuiltTree = true;
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MotdRoot"));
		Root->SetWidthOverride(520.0f);
		Root->SetMinDesiredHeight(150.0f);

		UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MotdBackground"));
		Background->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.9f));
		Background->SetPadding(FMargin(16.0f));
		Root->AddChild(Background);

		UVerticalBox* Column =
			WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MotdColumn"));
		Background->SetContent(Column);

		Status = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Status"));
		Status->SetAutoWrapText(true);
		Column->AddChildToVerticalBox(Status);

		Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
		Title->SetAutoWrapText(true);
		Column->AddChildToVerticalBox(Title);

		Body = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Body"));
		Body->SetAutoWrapText(true);
		if (UVerticalBoxSlot* BodySlot = Column->AddChildToVerticalBox(Body))
		{
			BodySlot->SetPadding(FMargin(0.0f, 8.0f));
		}

		UHorizontalBox* Buttons =
			WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Buttons"));
		Column->AddChildToVerticalBox(Buttons);

		Action = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Action"));
		UTextBlock* ActionLabel =
			WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ActionLabel"));
		ActionLabel->SetText(NSLOCTEXT("HaivenLyraDemo", "MotdAction", "Learn more"));
		Action->SetContent(ActionLabel);
		Buttons->AddChildToHorizontalBox(Action);

		Dismiss = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Dismiss"));
		UTextBlock* DismissLabel =
			WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DismissLabel"));
		DismissLabel->SetText(NSLOCTEXT("HaivenLyraDemo", "MotdDismiss", "Dismiss"));
		Dismiss->SetContent(DismissLabel);
		Buttons->AddChildToHorizontalBox(Dismiss);

		WidgetTree->RootWidget = Root;
	}

	TSharedRef<SWidget> BuiltWidget = Super::RebuildWidget();
	if (Action) Action->OnClicked.AddUniqueDynamic(this, &UHaivenLyraMotdWidget::HandleActionClicked);
	if (Dismiss) Dismiss->OnClicked.AddUniqueDynamic(this, &UHaivenLyraMotdWidget::HandleDismissClicked);
	ApplyPresentation();
	return BuiltWidget;
}

void UHaivenLyraMotdWidget::PresentProjection(const FHaivenLyraMotdProjection& Projection)
{
	CurrentProjection = Projection;
	ActionUrl = Projection.Message.IsSet() && Projection.Message.GetValue().Url.IsSet()
		? Projection.Message.GetValue().Url.GetValue()
		: FString();
	ApplyPresentation();
}

void UHaivenLyraMotdWidget::PresentLoading()
{
	LocalState.LoadStarted(TEXT("widget"));
	PresentProjection(LocalState.GetProjection());
}

void UHaivenLyraMotdWidget::PresentAbsent()
{
	LocalState.RealtimeClear();
	PresentProjection(LocalState.GetProjection());
}

void UHaivenLyraMotdWidget::PresentMessage(const FHaivenMotd& Message)
{
	LocalState.RealtimePresent(Message);
	PresentProjection(LocalState.GetProjection());
}

void UHaivenLyraMotdWidget::PresentFailure()
{
	LocalState.PresentFailure();
	PresentProjection(LocalState.GetProjection());
}

FString UHaivenLyraMotdWidget::GetPresentationStateName() const
{
	return HaivenLyraMotdStatusName(CurrentProjection.Status);
}

FText UHaivenLyraMotdWidget::GetDisplayedStatus() const
{
	return Status ? Status->GetText() : FText::GetEmpty();
}

FText UHaivenLyraMotdWidget::GetDisplayedTitle() const
{
	return Title ? Title->GetText() : FText::GetEmpty();
}

FText UHaivenLyraMotdWidget::GetDisplayedBody() const
{
	return Body ? Body->GetText() : FText::GetEmpty();
}

bool UHaivenLyraMotdWidget::HasDesignerBindings() const
{
	return Status != nullptr && Title != nullptr && Body != nullptr && Action != nullptr && Dismiss != nullptr;
}

void UHaivenLyraMotdWidget::ApplyPresentation()
{
	if (!HasDesignerBindings()) return;
	SetVisibility(CurrentProjection.Visible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	const bool bPresent = CurrentProjection.Status == EHaivenLyraMotdPresentationState::Present;
	Status->SetVisibility(bPresent ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	Title->SetVisibility(bPresent ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Body->SetVisibility(bPresent ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Dismiss->SetVisibility(bPresent ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Action->SetVisibility(bPresent && !ActionUrl.IsEmpty() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	switch (CurrentProjection.Status)
	{
	case EHaivenLyraMotdPresentationState::Loading: Status->SetText(LoadingCopy); break;
	case EHaivenLyraMotdPresentationState::Absent: Status->SetText(AbsentCopy); break;
	case EHaivenLyraMotdPresentationState::Present:
		if (CurrentProjection.Message.IsSet())
		{
			Title->SetText(FText::FromString(CurrentProjection.Message.GetValue().Title));
			Body->SetText(FText::FromString(CurrentProjection.Message.GetValue().Body));
		}
		break;
	case EHaivenLyraMotdPresentationState::Failure:
		Status->SetText(
			FText::FromString(CurrentProjection.Error.IsSet() ? CurrentProjection.Error.GetValue() : FString()));
		break;
	}
}

void UHaivenLyraMotdWidget::HandleActionClicked()
{
	if (ActionUrl.IsEmpty()) return;
	ULocalPlayer* Player = GetOwningLocalPlayer();
	if (UHaivenLyraDemoSubsystem* Demo = Player ? Player->GetSubsystem<UHaivenLyraDemoSubsystem>() : nullptr)
	{
		Demo->RequestMotdAction(ActionUrl);
	}
}

void UHaivenLyraMotdWidget::HandleDismissClicked()
{
	ULocalPlayer* Player = GetOwningLocalPlayer();
	if (UHaivenLyraDemoSubsystem* Demo = Player ? Player->GetSubsystem<UHaivenLyraDemoSubsystem>() : nullptr)
	{
		Demo->DismissMotd();
	}
}
