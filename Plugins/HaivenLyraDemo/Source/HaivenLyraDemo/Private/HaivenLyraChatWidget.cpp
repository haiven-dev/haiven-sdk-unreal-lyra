#include "HaivenLyraChatWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/LocalPlayer.h"
#include "HaivenLyraDemoSubsystem.h"

TSharedRef<SWidget> UHaivenLyraChatWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		bUsesCodeBuiltTree = true;
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Root"));
		Root->SetWidthOverride(420.0f);
		Root->SetHeightOverride(320.0f);

		UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
		Background->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
		Background->SetPadding(FMargin(8.0f));
		Root->AddChild(Background);

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
		Background->SetContent(Column);

		Status = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Status"));
		Status->SetAutoWrapText(true);
		if (UVerticalBoxSlot* StatusSlot = Column->AddChildToVerticalBox(Status))
		{
			StatusSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}

		UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Actions"));
		if (UVerticalBoxSlot* ActionsSlot = Column->AddChildToVerticalBox(Actions))
		{
			ActionsSlot->SetHorizontalAlignment(HAlign_Left);
			ActionsSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}

		SignIn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SignIn"));
		SignInLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SignInLabel"));
		SignIn->SetContent(SignInLabel);
		if (UHorizontalBoxSlot* SignInSlot = Actions->AddChildToHorizontalBox(SignIn))
		{
			SignInSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
		}

		ClaimReward = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ClaimReward"));
		UTextBlock* ClaimLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ClaimRewardLabel"));
		ClaimLabel->SetText(NSLOCTEXT("HaivenLyraDemo", "ClaimReward", "Claim reward"));
		ClaimReward->SetContent(ClaimLabel);
		Actions->AddChildToHorizontalBox(ClaimReward);

		Log = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Log"));
		if (UVerticalBoxSlot* LogSlot = Column->AddChildToVerticalBox(Log))
		{
			LogSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		Input = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Input"));
		Input->SetHintText(NSLOCTEXT("HaivenLyraDemo", "InputHint", "Say something (Enter to send)"));
		Column->AddChildToVerticalBox(Input);

		WidgetTree->RootWidget = Root;
	}
	TSharedRef<SWidget> BuiltWidget = Super::RebuildWidget();
	if (Input)
	{
		Input->OnTextCommitted.AddUniqueDynamic(this, &UHaivenLyraChatWidget::HandleTextCommitted);
	}
	if (ClaimReward)
	{
		ClaimReward->OnClicked.AddUniqueDynamic(this, &UHaivenLyraChatWidget::HandleClaimRewardClicked);
	}
	if (SignIn)
	{
		SignIn->OnClicked.AddUniqueDynamic(this, &UHaivenLyraChatWidget::HandleSignInClicked);
	}
	if (Status) Status->SetText(DisplayedStatus);
	ApplyRewardAction();
	ApplySignInAction();
	return BuiltWidget;
}

void UHaivenLyraChatWidget::SetSignInAction(const bool bVisible, const FText& Label)
{
	bSignInVisible = bVisible;
	SignInActionLabel = Label;
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
	if (UHaivenLyraDemoSubsystem* Demo = Player ? Player->GetSubsystem<UHaivenLyraDemoSubsystem>() : nullptr)
	{
		Demo->PressSignInAction();
	}
}

void UHaivenLyraChatWidget::SetStatus(const FText& Lines)
{
	DisplayedStatus = Lines;
	if (Status) Status->SetText(Lines);
}

void UHaivenLyraChatWidget::SetRewardAction(const bool bVisible, const bool bEnabled)
{
	bRewardVisible = bVisible;
	bRewardEnabled = bEnabled;
	ApplyRewardAction();
}

void UHaivenLyraChatWidget::ApplyRewardAction()
{
	if (!ClaimReward) return;
	ClaimReward->SetVisibility(bRewardVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	ClaimReward->SetIsEnabled(bRewardEnabled);
}

bool UHaivenLyraChatWidget::IsRewardActionAvailable() const
{
	return ClaimReward != nullptr && bRewardVisible && bRewardEnabled;
}

void UHaivenLyraChatWidget::HandleClaimRewardClicked()
{
	ULocalPlayer* Player = GetOwningLocalPlayer();
	if (UHaivenLyraDemoSubsystem* Demo = Player ? Player->GetSubsystem<UHaivenLyraDemoSubsystem>() : nullptr)
	{
		Demo->ClaimReward();
	}
}

UTextBlock* UHaivenLyraChatWidget::MakeLine(const FText& Line)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(Line);
	Text->SetAutoWrapText(true);
	Log->AddChild(Text);
	Log->ScrollToEnd();
	return Text;
}

void UHaivenLyraChatWidget::AddLine(const FText& Line)
{
	if (Log) MakeLine(Line);
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

void UHaivenLyraChatWidget::FocusInput()
{
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
		// Escape, or Enter on an empty line, closes the input without sending.
		Demo->ReleaseChat();
	}
}
