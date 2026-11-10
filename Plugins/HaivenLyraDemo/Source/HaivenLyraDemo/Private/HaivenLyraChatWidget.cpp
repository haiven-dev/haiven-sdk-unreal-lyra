#include "HaivenLyraChatWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SafeZone.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/LocalPlayer.h"
#include "HaivenLyraDemoSubsystem.h"

namespace
{
	const FLinearColor Navy(0.015f, 0.045f, 0.09f, 0.96f);
	const FLinearColor Cyan(0.08f, 0.72f, 0.95f, 1.0f);
	const FLinearColor CyanSoft(0.12f, 0.34f, 0.48f, 1.0f);
	const FLinearColor White(0.94f, 0.98f, 1.0f, 1.0f);
	const FLinearColor Muted(0.54f, 0.67f, 0.76f, 1.0f);
	const FLinearColor InputHintInk(0.16f, 0.28f, 0.38f, 1.0f);

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

	void StyleButton(UButton* Button, UTextBlock* LabelText, const bool bPrimary = false)
	{
		if (!Button || !LabelText) return;
		Button->SetBackgroundColor(bPrimary ? Cyan : CyanSoft);
		StyleText(LabelText, 15, bPrimary ? Navy : White);
		LabelText->SetJustification(ETextJustify::Center);
	}

	template <typename TButtonPtr, typename TLabelPtr>
	USizeBox* FramedButton(
		UWidgetTree* Tree,
		const FName FrameName,
		TButtonPtr& OutButton,
		const FName ButtonName,
		TLabelPtr& OutLabel,
		const FName LabelName,
		const FText& Copy,
		const FVector2D Size,
		const bool bPrimary = false)
	{
		USizeBox* Frame = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FrameName);
		Frame->SetWidthOverride(Size.X);
		Frame->SetHeightOverride(Size.Y);
		OutButton = Tree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		OutLabel = Label(Tree, LabelName, Copy, 15, White);
		OutButton->SetContent(OutLabel);
		StyleButton(OutButton, OutLabel, bPrimary);
		Frame->SetContent(OutButton);
		return Frame;
	}

	template <typename TTextPtr>
	UBorder* Chip(UWidgetTree* Tree, const FName Name, TTextPtr& OutText, const FName TextName, const FText& Copy)
	{
		UBorder* Frame = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Frame->SetBrushColor(FLinearColor(0.03f, 0.16f, 0.25f, 0.96f));
		Frame->SetPadding(FMargin(10.0f, 7.0f));
		OutText = Label(Tree, TextName, Copy, 13, White);
		Frame->SetContent(OutText);
		return Frame;
	}

	void FillOverlaySlot(UOverlaySlot* Slot)
	{
		if (!Slot) return;
		Slot->SetHorizontalAlignment(HAlign_Fill);
		Slot->SetVerticalAlignment(VAlign_Fill);
	}
}

TSharedRef<SWidget> UHaivenLyraChatWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		bUsesCodeBuiltTree = true;
		USafeZone* Root = WidgetTree->ConstructWidget<USafeZone>(USafeZone::StaticClass(), TEXT("HaivenSafeArea"));
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("HaivenLayers"));
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Layers->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Root->SetContent(Layers);

		UCanvasPanel* HandleLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HaivenHandleLayer"));
		UCanvasPanel* DialogLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HaivenDialogLayer"));
		HandleLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		DialogLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		FillOverlaySlot(Layers->AddChildToOverlay(HandleLayer));
		FillOverlaySlot(Layers->AddChildToOverlay(DialogLayer));

		UTextBlock* ChatHandleLabel = nullptr;
		USizeBox* ChatHandleFrame = FramedButton(
			WidgetTree, TEXT("ChatHandleFrame"), ChatHandle, TEXT("ChatHandle"), ChatHandleLabel,
			TEXT("ChatHandleLabel"), NSLOCTEXT("HaivenLyraDemo", "ChatHandle", "CHAT"), FVector2D(160.0f, 52.0f));
		if (UCanvasPanelSlot* Slot = HandleLayer->AddChildToCanvas(ChatHandleFrame))
		{
			Slot->SetAnchors(FAnchors(1.0f, 0.0f));
			Slot->SetAlignment(FVector2D(1.0f, 0.0f));
			Slot->SetPosition(FVector2D(-24.0f, 96.0f));
			Slot->SetSize(FVector2D(160.0f, 52.0f));
		}

		USizeBox* RewardsHandleFrame = FramedButton(
			WidgetTree, TEXT("RewardsHandleFrame"), RewardsHandle, TEXT("RewardsHandle"), RewardsHandleLabel,
			TEXT("RewardsHandleLabel"), NSLOCTEXT("HaivenLyraDemo", "RewardsHandle", "REWARDS  •"), FVector2D(160.0f, 52.0f));
		if (UCanvasPanelSlot* Slot = HandleLayer->AddChildToCanvas(RewardsHandleFrame))
		{
			Slot->SetAnchors(FAnchors(1.0f, 0.0f));
			Slot->SetAlignment(FVector2D(1.0f, 0.0f));
			Slot->SetPosition(FVector2D(-24.0f, 156.0f));
			Slot->SetSize(FVector2D(160.0f, 52.0f));
		}

		UTextBlock* PlayHandleLabel = nullptr;
		USizeBox* PlayHandleFrame = FramedButton(
			WidgetTree, TEXT("PlayHandleFrame"), PlayHandle, TEXT("PlayHandle"), PlayHandleLabel,
			TEXT("PlayHandleLabel"), NSLOCTEXT("HaivenLyraDemo", "PlayHandle", "PLAY"), FVector2D(160.0f, 52.0f), true);
		if (UCanvasPanelSlot* Slot = HandleLayer->AddChildToCanvas(PlayHandleFrame))
		{
			Slot->SetAnchors(FAnchors(0.0f, 0.0f));
			Slot->SetAlignment(FVector2D(0.0f, 0.0f));
			Slot->SetPosition(FVector2D(24.0f, 96.0f));
			Slot->SetSize(FVector2D(160.0f, 52.0f));
		}

		ChatDrawer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ChatDrawer"));
		ChatDrawer->SetBrushColor(Navy);
		ChatDrawer->SetPadding(FMargin(16.0f));
		if (UCanvasPanelSlot* Slot = DialogLayer->AddChildToCanvas(ChatDrawer))
		{
			Slot->SetAnchors(FAnchors(1.0f, 0.0f));
			Slot->SetAlignment(FVector2D(1.0f, 0.0f));
			Slot->SetPosition(FVector2D(-200.0f, 96.0f));
			Slot->SetSize(FVector2D(500.0f, 536.0f));
		}

		UVerticalBox* ChatColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ChatColumn"));
		ChatDrawer->SetContent(ChatColumn);
		UHorizontalBox* ChatHeading = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ChatHeading"));
		if (UVerticalBoxSlot* Slot = ChatColumn->AddChildToVerticalBox(ChatHeading)) Slot->SetPadding(FMargin(0, 0, 0, 16));
		UTextBlock* ChatTitle = Label(WidgetTree, TEXT("ChatTitle"), NSLOCTEXT("HaivenLyraDemo", "ChatTitle", "HAIVEN CHAT"), 22, White);
		if (UHorizontalBoxSlot* Slot = ChatHeading->AddChildToHorizontalBox(ChatTitle))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		UTextBlock* ChatCloseLabel = nullptr;
		USizeBox* ChatCloseFrame = FramedButton(
			WidgetTree, TEXT("ChatCloseFrame"), ChatClose, TEXT("ChatClose"), ChatCloseLabel,
			TEXT("ChatCloseLabel"), NSLOCTEXT("HaivenLyraDemo", "Close", "CLOSE"), FVector2D(96, 44));
		ChatHeading->AddChildToHorizontalBox(ChatCloseFrame);

		UHorizontalBox* PrimaryChips = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PrimaryStatusChips"));
		if (UVerticalBoxSlot* Slot = ChatColumn->AddChildToVerticalBox(PrimaryChips)) Slot->SetPadding(FMargin(0, 0, 0, 8));
		if (UHorizontalBoxSlot* Slot = PrimaryChips->AddChildToHorizontalBox(
			Chip(WidgetTree, TEXT("PlayerChipFrame"), PlayerChip, TEXT("PlayerChip"), FText::FromString(TEXT("PLAYER\nUNKNOWN")))))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetPadding(FMargin(0, 0, 4, 0));
		}
		if (UHorizontalBoxSlot* Slot = PrimaryChips->AddChildToHorizontalBox(
			Chip(WidgetTree, TEXT("ConnectionChipFrame"), ConnectionChip, TEXT("ConnectionChip"), FText::FromString(TEXT("HAIVEN\nOFFLINE")))))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetPadding(FMargin(4, 0, 0, 0));
		}

		UHorizontalBox* StateChips = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("StateStatusChips"));
		if (UVerticalBoxSlot* Slot = ChatColumn->AddChildToVerticalBox(StateChips)) Slot->SetPadding(FMargin(0, 0, 0, 16));
		if (UHorizontalBoxSlot* Slot = StateChips->AddChildToHorizontalBox(
			Chip(WidgetTree, TEXT("ModerationChipFrame"), ModerationChip, TEXT("ModerationChip"), FText::FromString(TEXT("MODERATION\nREADY")))))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetPadding(FMargin(0, 0, 4, 0));
		}
		if (UHorizontalBoxSlot* Slot = StateChips->AddChildToHorizontalBox(
			Chip(WidgetTree, TEXT("RewardChipFrame"), RewardChip, TEXT("RewardChip"), FText::FromString(TEXT("REWARD\nNOT CONFIGURED")))))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetPadding(FMargin(4, 0, 0, 0));
		}

		Status = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Status"));
		Status->SetVisibility(ESlateVisibility::Collapsed);
		ChatColumn->AddChildToVerticalBox(Status);

		Log = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Log"));
		if (UVerticalBoxSlot* Slot = ChatColumn->AddChildToVerticalBox(Log))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetPadding(FMargin(0, 0, 0, 12));
		}

		UBorder* InputFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InputFrame"));
		InputFrame->SetBrushColor(CyanSoft);
		InputFrame->SetPadding(FMargin(2.0f));
		USizeBox* InputSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("InputSize"));
		InputSize->SetHeightOverride(48.0f);
		UOverlay* InputOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("InputOverlay"));
		Input = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Input"));
		Input->SetHintText(FText::GetEmpty());
		// UE 5.6.1 has no SetWidgetStyle/GetWidgetStyle. Assign the public property there.
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
		FEditableTextBoxStyle InputStyle = Input->GetWidgetStyle();
		InputStyle.TextStyle.Font.Size = 15;
		Input->SetWidgetStyle(InputStyle);
#else
		Input->WidgetStyle.TextStyle.Font.Size = 15;
#endif
		Input->SetForegroundColor(InputHintInk);
		if (UOverlaySlot* Slot = InputOverlay->AddChildToOverlay(Input))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		InputHint = Label(
			WidgetTree, TEXT("InputHint"), NSLOCTEXT("HaivenLyraDemo", "InputHintCopy", "MESSAGE  ·  ENTER TO SEND"), 12, InputHintInk);
		InputHint->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* Slot = InputOverlay->AddChildToOverlay(InputHint))
		{
			Slot->SetHorizontalAlignment(HAlign_Left);
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(12.0f, 0.0f));
		}
		InputSize->SetContent(InputOverlay);
		InputFrame->SetContent(InputSize);
		ChatColumn->AddChildToVerticalBox(InputFrame);

		UHorizontalBox* ChatActions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ChatActions"));
		if (UVerticalBoxSlot* Slot = ChatColumn->AddChildToVerticalBox(ChatActions)) Slot->SetPadding(FMargin(0, 12, 0, 0));
		USizeBox* SignInFrame = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SignInFrame"));
		SignInFrame->SetHeightOverride(44.0f);
		SignIn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SignIn"));
		SignInLabel = Label(WidgetTree, TEXT("SignInLabel"), FText::GetEmpty(), 15, White);
		SignIn->SetContent(SignInLabel);
		StyleButton(SignIn, SignInLabel);
		SignInFrame->SetContent(SignIn);
		ChatActions->AddChildToHorizontalBox(SignInFrame);

		RewardsDrawer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RewardsDrawer"));
		RewardsDrawer->SetBrushColor(Navy);
		RewardsDrawer->SetPadding(FMargin(16.0f));
		if (UCanvasPanelSlot* Slot = DialogLayer->AddChildToCanvas(RewardsDrawer))
		{
			Slot->SetAnchors(FAnchors(1.0f, 0.0f));
			Slot->SetAlignment(FVector2D(1.0f, 0.0f));
			Slot->SetPosition(FVector2D(-200.0f, 96.0f));
			Slot->SetSize(FVector2D(500.0f, 536.0f));
		}
		UVerticalBox* RewardsColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RewardsColumn"));
		RewardsDrawer->SetContent(RewardsColumn);
		UHorizontalBox* RewardsHeading = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RewardsHeading"));
		if (UVerticalBoxSlot* Slot = RewardsColumn->AddChildToVerticalBox(RewardsHeading)) Slot->SetPadding(FMargin(0, 0, 0, 16));
		UTextBlock* RewardsTitle = Label(WidgetTree, TEXT("RewardsTitle"), NSLOCTEXT("HaivenLyraDemo", "RewardsTitle", "REWARDS & INVENTORY"), 22, White);
		if (UHorizontalBoxSlot* Slot = RewardsHeading->AddChildToHorizontalBox(RewardsTitle))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		UTextBlock* RewardsCloseLabel = nullptr;
		USizeBox* RewardsCloseFrame = FramedButton(
			WidgetTree, TEXT("RewardsCloseFrame"), RewardsClose, TEXT("RewardsClose"), RewardsCloseLabel,
			TEXT("RewardsCloseLabel"), NSLOCTEXT("HaivenLyraDemo", "RewardsClose", "CLOSE"), FVector2D(96, 44));
		RewardsHeading->AddChildToHorizontalBox(RewardsCloseFrame);

		UBorder* RewardState = Chip(WidgetTree, TEXT("RewardStateFrame"), RewardDrawerChip, TEXT("RewardDrawerChip"), FText::FromString(TEXT("REWARD\nREADY")));
		if (UVerticalBoxSlot* Slot = RewardsColumn->AddChildToVerticalBox(RewardState)) Slot->SetPadding(FMargin(0, 0, 0, 16));
		RewardsColumn->AddChildToVerticalBox(Label(
			WidgetTree, TEXT("InventoryHeading"), NSLOCTEXT("HaivenLyraDemo", "InventoryHeading", "RECENT GRANTS"), 14, Muted));
		RewardsLog = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("RewardsLog"));
		if (UVerticalBoxSlot* Slot = RewardsColumn->AddChildToVerticalBox(RewardsLog))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetPadding(FMargin(0, 8, 0, 16));
		}
		UTextBlock* ClaimLabel = nullptr;
		USizeBox* ClaimFrame = FramedButton(
			WidgetTree, TEXT("ClaimRewardFrame"), ClaimReward, TEXT("ClaimReward"), ClaimLabel,
			TEXT("ClaimRewardLabel"), NSLOCTEXT("HaivenLyraDemo", "ClaimReward", "CLAIM REWARD"), FVector2D(220, 48), true);
		RewardsColumn->AddChildToVerticalBox(ClaimFrame);

		ChatDrawer->SetVisibility(ESlateVisibility::Collapsed);
		RewardsDrawer->SetVisibility(ESlateVisibility::Collapsed);
		WidgetTree->RootWidget = Root;
	}

	TSharedRef<SWidget> BuiltWidget = Super::RebuildWidget();
	if (Input)
	{
		Input->OnTextCommitted.AddUniqueDynamic(this, &UHaivenLyraChatWidget::HandleTextCommitted);
		Input->OnTextChanged.AddUniqueDynamic(this, &UHaivenLyraChatWidget::HandleTextChanged);
	}
	if (ClaimReward) ClaimReward->OnClicked.AddUniqueDynamic(this, &UHaivenLyraChatWidget::HandleClaimRewardClicked);
	if (SignIn) SignIn->OnClicked.AddUniqueDynamic(this, &UHaivenLyraChatWidget::HandleSignInClicked);
	if (ChatHandle) ChatHandle->OnClicked.AddUniqueDynamic(this, &UHaivenLyraChatWidget::HandleChatHandleClicked);
	if (RewardsHandle) RewardsHandle->OnClicked.AddUniqueDynamic(this, &UHaivenLyraChatWidget::HandleRewardsHandleClicked);
	if (PlayHandle) PlayHandle->OnClicked.AddUniqueDynamic(this, &UHaivenLyraChatWidget::HandlePlayHandleClicked);
	if (ChatClose) ChatClose->OnClicked.AddUniqueDynamic(this, &UHaivenLyraChatWidget::HandleChatCloseClicked);
	if (RewardsClose) RewardsClose->OnClicked.AddUniqueDynamic(this, &UHaivenLyraChatWidget::HandleRewardsCloseClicked);
	if (Status) Status->SetText(DisplayedStatus);
	ApplyStatusChips();
	ApplyRewardAction();
	ApplySignInAction();
	return BuiltWidget;
}

void UHaivenLyraChatWidget::SetSignInAction(const bool bVisible, const FText& LabelText)
{
	bSignInVisible = bVisible;
	SignInActionLabel = LabelText;
	ApplySignInAction();
}

void UHaivenLyraChatWidget::ApplySignInAction()
{
	if (SignInLabel) SignInLabel->SetText(SignInActionLabel);
	if (SignIn) SignIn->SetVisibility(bSignInVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

bool UHaivenLyraChatWidget::IsSignInActionAvailable() const
{
	return SignIn != nullptr && bSignInVisible;
}

void UHaivenLyraChatWidget::HandleSignInClicked()
{
	ULocalPlayer* Player = GetOwningLocalPlayer();
	if (UHaivenLyraDemoSubsystem* Demo = Player ? Player->GetSubsystem<UHaivenLyraDemoSubsystem>() : nullptr) Demo->PressSignInAction();
}

void UHaivenLyraChatWidget::SetStatus(const FText& Lines)
{
	DisplayedStatus = Lines;
	if (Status) Status->SetText(Lines);
	ApplyStatusChips();
}

void UHaivenLyraChatWidget::ApplyStatusChips()
{
	FString Copy = DisplayedStatus.ToString();
	Copy.ReplaceInline(TEXT("\n"), TEXT("  |  "));
	TArray<FString> Parts;
	Copy.ParseIntoArray(Parts, TEXT("  |  "), true);
	for (FString Part : Parts)
	{
		Part.TrimStartAndEndInline();
		if (Part.StartsWith(TEXT("Player:")) && PlayerChip)
		{
			Part.RemoveFromStart(TEXT("Player:"));
			PlayerChip->SetText(FText::FromString(FString::Printf(TEXT("PLAYER\n%s"), *Part.TrimStartAndEnd())));
		}
		else if (Part.StartsWith(TEXT("Haiven:")) && ConnectionChip)
		{
			Part.RemoveFromStart(TEXT("Haiven:"));
			ConnectionChip->SetText(FText::FromString(FString::Printf(TEXT("HAIVEN\n%s"), *Part.TrimStartAndEnd())));
		}
		else if (Part.StartsWith(TEXT("Last message:")) && ModerationChip)
		{
			Part.RemoveFromStart(TEXT("Last message:"));
			ModerationChip->SetText(FText::FromString(FString::Printf(TEXT("MODERATION\n%s"), *Part.TrimStartAndEnd())));
		}
		else if (Part.StartsWith(TEXT("Reward:")) && RewardChip)
		{
			Part.RemoveFromStart(TEXT("Reward:"));
			const FText Value = FText::FromString(FString::Printf(TEXT("REWARD\n%s"), *Part.TrimStartAndEnd()));
			RewardChip->SetText(Value);
			if (RewardDrawerChip) RewardDrawerChip->SetText(Value);
		}
	}
}

void UHaivenLyraChatWidget::SetRewardAction(const bool bVisible, const bool bEnabled)
{
	bRewardVisible = bVisible;
	bRewardEnabled = bEnabled;
	ApplyRewardAction();
}

void UHaivenLyraChatWidget::ApplyRewardAction()
{
	if (ClaimReward)
	{
		ClaimReward->SetVisibility(bRewardVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		ClaimReward->SetIsEnabled(bRewardEnabled);
	}
	if (RewardsHandleLabel) RewardsHandleLabel->SetText(FText::FromString(bRewardEnabled ? TEXT("REWARDS  •") : TEXT("REWARDS")));
}

bool UHaivenLyraChatWidget::IsRewardActionAvailable() const
{
	return ClaimReward != nullptr && bRewardVisible && bRewardEnabled;
}

void UHaivenLyraChatWidget::HandleClaimRewardClicked()
{
	ULocalPlayer* Player = GetOwningLocalPlayer();
	if (UHaivenLyraDemoSubsystem* Demo = Player ? Player->GetSubsystem<UHaivenLyraDemoSubsystem>() : nullptr) Demo->ClaimReward();
}

UTextBlock* UHaivenLyraChatWidget::MakeLine(const FText& Line)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(Line);
	Text->SetAutoWrapText(true);
	StyleText(Text, 15, White);
	Log->AddChild(Text);
	Log->ScrollToEnd();
	return Text;
}

void UHaivenLyraChatWidget::AddRewardsLine(const FText& Line)
{
	if (!RewardsLog) return;
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(Line);
	Text->SetAutoWrapText(true);
	StyleText(Text, 15, White);
	RewardsLog->AddChild(Text);
	RewardsLog->ScrollToEnd();
}

void UHaivenLyraChatWidget::AddLine(const FText& Line)
{
	if (Log) MakeLine(Line);
	const FString Copy = Line.ToString().ToLower();
	if (Copy.Contains(TEXT("reward")) || Copy.Contains(TEXT("unlocked:")) || Copy.Contains(TEXT("inventory")) || Copy.Contains(TEXT("tag/"))) AddRewardsLine(Line);
}

void UHaivenLyraChatWidget::SetKeyedLine(const FString& Key, const FText& Line)
{
	if (!Log) return;
	if (TObjectPtr<UTextBlock>* Existing = KeyedLines.Find(Key))
	{
		(*Existing)->SetText(Line);
		return;
	}
	KeyedLines.Add(Key, MakeLine(Line));
}

void UHaivenLyraChatWidget::OpenChatDrawer()
{
	if (ChatDrawer) ChatDrawer->SetVisibility(ESlateVisibility::Visible);
	if (RewardsDrawer) RewardsDrawer->SetVisibility(ESlateVisibility::Collapsed);
	ULocalPlayer* Player = GetOwningLocalPlayer();
	if (UHaivenLyraDemoSubsystem* Demo = Player ? Player->GetSubsystem<UHaivenLyraDemoSubsystem>() : nullptr) Demo->CollapseMotdPanel();
}

void UHaivenLyraChatWidget::OpenRewardsDrawer()
{
	if (ChatDrawer) ChatDrawer->SetVisibility(ESlateVisibility::Collapsed);
	if (RewardsDrawer) RewardsDrawer->SetVisibility(ESlateVisibility::Visible);
	ULocalPlayer* Player = GetOwningLocalPlayer();
	if (UHaivenLyraDemoSubsystem* Demo = Player ? Player->GetSubsystem<UHaivenLyraDemoSubsystem>() : nullptr) Demo->CollapseMotdPanel();
}

void UHaivenLyraChatWidget::CollapseDrawers()
{
	if (ChatDrawer) ChatDrawer->SetVisibility(ESlateVisibility::Collapsed);
	if (RewardsDrawer) RewardsDrawer->SetVisibility(ESlateVisibility::Collapsed);
}

void UHaivenLyraChatWidget::HandlePlayHandleClicked()
{
	CollapseDrawers();
	ULocalPlayer* Player = GetOwningLocalPlayer();
	if (UHaivenLyraDemoSubsystem* Demo = Player ? Player->GetSubsystem<UHaivenLyraDemoSubsystem>() : nullptr)
	{
		Demo->RequestPlayGameplay();
	}
}

void UHaivenLyraChatWidget::HandleChatHandleClicked()
{
	if (ChatDrawer && ChatDrawer->GetVisibility() == ESlateVisibility::Visible) CollapseDrawers(); else OpenChatDrawer();
}

void UHaivenLyraChatWidget::HandleRewardsHandleClicked()
{
	if (RewardsDrawer && RewardsDrawer->GetVisibility() == ESlateVisibility::Visible) CollapseDrawers(); else OpenRewardsDrawer();
}

void UHaivenLyraChatWidget::HandleChatCloseClicked() { CollapseDrawers(); }
void UHaivenLyraChatWidget::HandleRewardsCloseClicked() { CollapseDrawers(); }

void UHaivenLyraChatWidget::FocusInput()
{
	OpenChatDrawer();
	if (Input) Input->SetKeyboardFocus();
}

void UHaivenLyraChatWidget::HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	ULocalPlayer* Player = GetOwningLocalPlayer();
	UHaivenLyraDemoSubsystem* Demo = Player ? Player->GetSubsystem<UHaivenLyraDemoSubsystem>() : nullptr;
	if (!Demo) return;
	if (CommitMethod == ETextCommit::OnEnter && !Text.IsEmptyOrWhitespace())
	{
		Demo->Send(Text.ToString());
		if (Input) Input->SetText(FText::GetEmpty());
	}
	else if (CommitMethod == ETextCommit::OnCleared || CommitMethod == ETextCommit::OnEnter)
	{
		Demo->ReleaseChat();
	}
}

void UHaivenLyraChatWidget::HandleTextChanged(const FText& Text)
{
	if (InputHint)
	{
		InputHint->SetVisibility(Text.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
