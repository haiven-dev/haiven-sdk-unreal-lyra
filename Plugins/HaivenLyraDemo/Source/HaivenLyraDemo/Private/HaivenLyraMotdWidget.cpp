#include "HaivenLyraMotdWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SafeZone.h"
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
	const FLinearColor Navy(0.015f, 0.045f, 0.09f, 0.96f);
	const FLinearColor Cyan(0.08f, 0.72f, 0.95f, 1.0f);
	const FLinearColor CyanSoft(0.12f, 0.34f, 0.48f, 1.0f);
	const FLinearColor White(0.94f, 0.98f, 1.0f, 1.0f);
	const FLinearColor Muted(0.54f, 0.67f, 0.76f, 1.0f);

	void StyleText(UTextBlock* Text, const int32 Size, const FLinearColor& Color)
	{
		if (!Text) return;
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
	}

	UTextBlock* Label(UWidgetTree* Tree, const FName Name, const FText& Copy, const int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetText(Copy);
		Text->SetAutoWrapText(true);
		StyleText(Text, Size, Color);
		return Text;
	}

	template <typename TButtonPtr, typename TLabelPtr>
	USizeBox* ButtonFrame(
		UWidgetTree* Tree,
		const FName FrameName,
		TButtonPtr& Button,
		const FName ButtonName,
		TLabelPtr& ButtonLabel,
		const FName LabelName,
		const FText& Copy,
		const FVector2D Size,
		const bool bPrimary = false)
	{
		USizeBox* Frame = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FrameName);
		Frame->SetWidthOverride(Size.X);
		Frame->SetHeightOverride(Size.Y);
		Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		ButtonLabel = Label(Tree, LabelName, Copy, 15, bPrimary ? Navy : White);
		ButtonLabel->SetJustification(ETextJustify::Center);
		Button->SetBackgroundColor(bPrimary ? Cyan : CyanSoft);
		Button->SetContent(ButtonLabel);
		Frame->SetContent(Button);
		return Frame;
	}
}

TSharedRef<SWidget> UHaivenLyraMotdWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		bUsesCodeBuiltTree = true;
		USafeZone* Root = WidgetTree->ConstructWidget<USafeZone>(USafeZone::StaticClass(), TEXT("MotdSafeArea"));
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MotdLayers"));
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Layers->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Root->SetContent(Layers);
		UCanvasPanel* HandleLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MotdHandleLayer"));
		UCanvasPanel* DialogLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MotdDialogLayer"));
		HandleLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		DialogLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (UOverlaySlot* Slot = Layers->AddChildToOverlay(HandleLayer))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		if (UOverlaySlot* Slot = Layers->AddChildToOverlay(DialogLayer))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}

		USizeBox* HandleFrame = ButtonFrame(
			WidgetTree, TEXT("MotdHandleFrame"), Handle, TEXT("MotdHandle"), HandleLabel, TEXT("MotdHandleLabel"),
			NSLOCTEXT("HaivenLyraDemo", "MotdHandle", "MOTD  • NEW"), FVector2D(160, 52));
		if (UCanvasPanelSlot* Slot = HandleLayer->AddChildToCanvas(HandleFrame))
		{
			Slot->SetAnchors(FAnchors(1.0f, 0.0f));
			Slot->SetAlignment(FVector2D(1.0f, 0.0f));
			Slot->SetPosition(FVector2D(-24.0f, 216.0f));
			Slot->SetSize(FVector2D(160.0f, 52.0f));
		}

		Drawer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MotdDrawer"));
		Drawer->SetBrushColor(Navy);
		Drawer->SetPadding(FMargin(16.0f));
		if (UCanvasPanelSlot* Slot = DialogLayer->AddChildToCanvas(Drawer))
		{
			Slot->SetAnchors(FAnchors(1.0f, 0.0f));
			Slot->SetAlignment(FVector2D(1.0f, 0.0f));
			Slot->SetPosition(FVector2D(-200.0f, 96.0f));
			Slot->SetSize(FVector2D(500.0f, 300.0f));
		}

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MotdColumn"));
		Drawer->SetContent(Column);
		UHorizontalBox* Heading = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MotdHeading"));
		if (UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Heading)) Slot->SetPadding(FMargin(0, 0, 0, 12));
		UTextBlock* Eyebrow = Label(WidgetTree, TEXT("MotdEyebrow"), NSLOCTEXT("HaivenLyraDemo", "MotdEyebrow", "MESSAGE OF THE DAY"), 14, Cyan);
		if (UHorizontalBoxSlot* Slot = Heading->AddChildToHorizontalBox(Eyebrow))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		UTextBlock* CollapseLabel = nullptr;
		USizeBox* CollapseFrame = ButtonFrame(
			WidgetTree, TEXT("MotdCollapseFrame"), Collapse, TEXT("MotdClose"), CollapseLabel, TEXT("MotdCloseLabel"),
			NSLOCTEXT("HaivenLyraDemo", "MotdClose", "CLOSE"), FVector2D(96, 44));
		Heading->AddChildToHorizontalBox(CollapseFrame);

		Status = Label(WidgetTree, TEXT("Status"), LoadingCopy, 16, Muted);
		Column->AddChildToVerticalBox(Status);
		Title = Label(WidgetTree, TEXT("Title"), FText::GetEmpty(), 24, White);
		if (UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Title)) Slot->SetPadding(FMargin(0, 0, 0, 8));
		Body = Label(WidgetTree, TEXT("Body"), FText::GetEmpty(), 17, Muted);
		if (UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Body))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetPadding(FMargin(0, 0, 0, 16));
		}

		UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MotdActions"));
		Column->AddChildToVerticalBox(Buttons);
		UTextBlock* ActionLabel = nullptr;
		USizeBox* ActionFrame = ButtonFrame(
			WidgetTree, TEXT("ActionFrame"), Action, TEXT("Action"), ActionLabel, TEXT("ActionLabel"),
			NSLOCTEXT("HaivenLyraDemo", "MotdAction", "LEARN MORE"), FVector2D(160, 44), true);
		if (UHorizontalBoxSlot* Slot = Buttons->AddChildToHorizontalBox(ActionFrame)) Slot->SetPadding(FMargin(0, 0, 8, 0));
		UTextBlock* DismissLabel = nullptr;
		USizeBox* DismissFrame = ButtonFrame(
			WidgetTree, TEXT("DismissFrame"), Dismiss, TEXT("Dismiss"), DismissLabel, TEXT("DismissLabel"),
			NSLOCTEXT("HaivenLyraDemo", "MotdDismiss", "DISMISS"), FVector2D(132, 44));
		Buttons->AddChildToHorizontalBox(DismissFrame);

		Handle->SetVisibility(ESlateVisibility::Collapsed);
		Drawer->SetVisibility(ESlateVisibility::Collapsed);
		WidgetTree->RootWidget = Root;
	}

	TSharedRef<SWidget> BuiltWidget = Super::RebuildWidget();
	if (Action) Action->OnClicked.AddUniqueDynamic(this, &UHaivenLyraMotdWidget::HandleActionClicked);
	if (Dismiss) Dismiss->OnClicked.AddUniqueDynamic(this, &UHaivenLyraMotdWidget::HandleDismissClicked);
	if (Handle) Handle->OnClicked.AddUniqueDynamic(this, &UHaivenLyraMotdWidget::HandleToggleClicked);
	if (Collapse) Collapse->OnClicked.AddUniqueDynamic(this, &UHaivenLyraMotdWidget::HandleCollapseClicked);
	ApplyPresentation();
	return BuiltWidget;
}

void UHaivenLyraMotdWidget::PresentProjection(const FHaivenLyraMotdProjection& Projection)
{
	CurrentProjection = Projection;
	ActionUrl = Projection.Message.IsSet() && Projection.Message.GetValue().Url.IsSet()
		? Projection.Message.GetValue().Url.GetValue()
		: FString();
	if (Projection.Status == EHaivenLyraMotdPresentationState::Present && !bExpanded) bUnread = true;
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

FText UHaivenLyraMotdWidget::GetDisplayedStatus() const { return Status ? Status->GetText() : FText::GetEmpty(); }
FText UHaivenLyraMotdWidget::GetDisplayedTitle() const { return Title ? Title->GetText() : FText::GetEmpty(); }
FText UHaivenLyraMotdWidget::GetDisplayedBody() const { return Body ? Body->GetText() : FText::GetEmpty(); }

bool UHaivenLyraMotdWidget::HasDesignerBindings() const
{
	return Status != nullptr && Title != nullptr && Body != nullptr && Action != nullptr && Dismiss != nullptr;
}

void UHaivenLyraMotdWidget::ApplyPresentation()
{
	if (!HasDesignerBindings()) return;
	const bool bHasPresentation = CurrentProjection.Visible;
	const bool bPresent = CurrentProjection.Status == EHaivenLyraMotdPresentationState::Present;
	if (Handle) Handle->SetVisibility(bHasPresentation ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (Drawer) Drawer->SetVisibility(bHasPresentation && bExpanded ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (HandleLabel) HandleLabel->SetText(FText::FromString(bUnread ? TEXT("MOTD  • NEW") : TEXT("MOTD")));
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
		Status->SetText(FText::FromString(CurrentProjection.Error.IsSet() ? CurrentProjection.Error.GetValue() : FString()));
		break;
	}
}

void UHaivenLyraMotdWidget::HandleToggleClicked()
{
	if (bExpanded) CollapseDrawer();
	else
	{
		bExpanded = true;
		bUnread = false;
		ULocalPlayer* Player = GetOwningLocalPlayer();
		if (UHaivenLyraDemoSubsystem* Demo = Player ? Player->GetSubsystem<UHaivenLyraDemoSubsystem>() : nullptr) Demo->CollapseMainPanels();
		ApplyPresentation();
	}
}

void UHaivenLyraMotdWidget::HandleCollapseClicked() { CollapseDrawer(); }

void UHaivenLyraMotdWidget::CollapseDrawer()
{
	bExpanded = false;
	ApplyPresentation();
}

void UHaivenLyraMotdWidget::HandleActionClicked()
{
	if (ActionUrl.IsEmpty()) return;
	ULocalPlayer* Player = GetOwningLocalPlayer();
	if (UHaivenLyraDemoSubsystem* Demo = Player ? Player->GetSubsystem<UHaivenLyraDemoSubsystem>() : nullptr) Demo->RequestMotdAction(ActionUrl);
}

void UHaivenLyraMotdWidget::HandleDismissClicked()
{
	bExpanded = false;
	ULocalPlayer* Player = GetOwningLocalPlayer();
	if (UHaivenLyraDemoSubsystem* Demo = Player ? Player->GetSubsystem<UHaivenLyraDemoSubsystem>() : nullptr) Demo->DismissMotd();
}
