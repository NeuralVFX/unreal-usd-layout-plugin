// Copyright 2020 NeuralVFX, Inc. All Rights Reserved.

#include "LayoutWidget.h"
#include "Engine/Selection.h"
#include "Asset.h"
#include "MeshAsset.h"
#include "AnimatedSkelMeshAsset.h"
#include "StaticMeshAsset.h"
#include "Camera.h"
#include "AnimatedMeshAsset.h"
#include "AssetBox.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "HAL/FileManager.h"
#include "IDesktopPlatform.h"
#include "DesktopPlatformModule.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "ISequencer.h"
#include "Helpers.h"
#include "Engine/StaticMeshActor.h"
#include "USDLayout.h"
#include <Engine/World.h>


ULayoutWidget::ULayoutWidget()
{
}


ULayoutWidget::~ULayoutWidget()
{
}



void ULayoutWidget::GenerateAssetGroup(UVerticalBox*& VerticalBox, UButton* LoadAllButton, UButton* UnLoadAllButton, FString GroupName, int top, int scroll_size)
{
	UCanvasPanel* Root = Cast<UCanvasPanel>(GetRootWidget());

	FLinearColor Black = FLinearColor(0, 0, 0);
	FLinearColor White = FLinearColor(1, 1, 1);
	USizeBox* Size = NewObject<USizeBox>();

	///////////////////////////////////////
	// 1. Section Title Label (20% larger text -> Size 10, White)
	///////////////////////////////////////
	UTextBlock* GroupTitleText = UHelpers::MakeTextBlock(GroupName, 10, White);
	UCanvasPanelSlot* TitleSlot = Root->AddChildToCanvas(GroupTitleText);
	TitleSlot->SetSize(FVector2D(580, 20));
	TitleSlot->SetPosition(FVector2D(10, top));

	///////////////////////////////////////
	// 2. Setup Column Labels (Shifted down slightly to clear title)
	///////////////////////////////////////
	UHorizontalBox* TopBox = NewObject<UHorizontalBox>();

	Size = NewObject<USizeBox>();
	Size->SetWidthOverride(70);
	Size->SetContent(UHelpers::MakeTextBlock("Is Loaded", 8, White));
	UHorizontalBoxSlot* UISlot_Hor = TopBox->AddChildToHorizontalBox(Size);
	UISlot_Hor->SetPadding(FMargin(15, 0));

	Size = NewObject<USizeBox>();
	Size->SetWidthOverride(155);
	Size->SetContent(UHelpers::MakeTextBlock("Content Name", 8, White));
	TopBox->AddChildToHorizontalBox(Size);

	Size = NewObject<USizeBox>();
	Size->SetWidthOverride(155);
	Size->SetContent(UHelpers::MakeTextBlock("Scene Name", 8, White));
	TopBox->AddChildToHorizontalBox(Size);

	Size = NewObject<USizeBox>();
	Size->SetWidthOverride(155);
	Size->SetContent(UHelpers::MakeTextBlock("Object Type", 8, White));
	TopBox->AddChildToHorizontalBox(Size);

	UBorder* TopBorder = NewObject<UBorder>();
	TopBorder->SetContent(TopBox);

	FSlateBrush HeaderRoundedBrush;
	HeaderRoundedBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	HeaderRoundedBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	HeaderRoundedBrush.OutlineSettings.CornerRadii = FVector4(5.0f, 5.0f, 5.0f, 5.0f);

	HeaderRoundedBrush.TintColor = FSlateColor(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f));
	TopBorder->SetBrush(HeaderRoundedBrush);

	UCanvasPanelSlot* UISlot = Root->AddChildToCanvas(TopBorder);
	UISlot->SetSize(FVector2D(580, 20));
	UISlot->SetPosition(FVector2D(10, top + 22));

	///////////////////////////////////////
	// 3. Setup Scroll Box to hold layout assets
	///////////////////////////////////////
	UScrollBox* ScrollBox = NewObject<UScrollBox>();

	// --- Remove the default scroll shadows ---
	ScrollBox->WidgetStyle.TopShadowBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	ScrollBox->WidgetStyle.BottomShadowBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	ScrollBox->WidgetStyle.LeftShadowBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	ScrollBox->WidgetStyle.RightShadowBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	// -----------------------------------------

	VerticalBox = NewObject<UVerticalBox>();
	ScrollBox->AddChild(VerticalBox);

	UBorder* Border = NewObject<UBorder>();
	Border->SetContent(ScrollBox);

	// --- Remove default border padding so content reaches the edges ---
	Border->SetPadding(FMargin(0.0f));

	// --- Clip the scrolling content so it doesn't poke outside the rounded corners ---
	Border->SetClipping(EWidgetClipping::ClipToBounds);

	// --- Create a Rounded Brush for the Spreadsheet Area ---
	FSlateBrush RoundedBrush;
	RoundedBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	RoundedBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	RoundedBrush.OutlineSettings.CornerRadii = FVector4(5.0f, 5.0f, 5.0f, 5.0f);
	RoundedBrush.TintColor = FSlateColor(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f));
	Border->SetBrush(RoundedBrush);

	UISlot = Root->AddChildToCanvas(Border);
	UISlot->SetSize(FVector2D(580, scroll_size));
	UISlot->SetPosition(FVector2D(10, top + 45));

	///////////////////////////////////////
	// 4. Setup Horizontal Box to hold GUI buttons
	///////////////////////////////////////
	UHorizontalBox* ButBox = NewObject<UHorizontalBox>();
	UBorder* ButBorder = NewObject<UBorder>();
	ButBorder->SetContent(ButBox);
	ButBorder->SetBrushColor(FLinearColor(.05, .05, .05, 0.0));

	UISlot = Root->AddChildToCanvas(ButBorder);
	UISlot->SetSize(FVector2D(580, 30));
	UISlot->SetPosition(FVector2D(10, top + scroll_size + 55));

	Size = NewObject<USizeBox>();
	Size->SetWidthOverride(255);
	Size->SetContent(LoadAllButton);
	UISlot_Hor = ButBox->AddChildToHorizontalBox(Size);
	UISlot_Hor->SetPadding(FMargin(13, 2));

	Size = NewObject<USizeBox>();
	Size->SetWidthOverride(255);
	Size->SetContent(UnLoadAllButton);
	UISlot_Hor = ButBox->AddChildToHorizontalBox(Size);
	UISlot_Hor->SetPadding(FMargin(13, 2));

	FButtonStyle ButtonStyle = LoadAllButton->GetStyle();
	ButtonStyle.Normal.OutlineSettings.Color = ButtonStyle.Hovered.OutlineSettings.Color = ButtonStyle.Pressed.OutlineSettings.Color = FLinearColor::Black;
	LoadAllButton->SetStyle(ButtonStyle);
	UnLoadAllButton->SetStyle(ButtonStyle);
	LoadAllButton->SetContent(UHelpers::MakeTextBlock("Load All Assets", 8, White));
	LoadAllButton->SetBackgroundColor(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f));
	UnLoadAllButton->SetContent(UHelpers::MakeTextBlock("Un-Load All Assets", 8, White));
	UnLoadAllButton->SetBackgroundColor(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f));
}


void ULayoutWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Setup text
	FLinearColor Black = FLinearColor(0, 0, 0);
	FLinearColor White = FLinearColor(1, 1, 1);
	USizeBox* Size = NewObject<USizeBox>();

	UCanvasPanel* Root = Cast<UCanvasPanel>(GetRootWidget());


	// Setup horizontal box to hold GUI buttons
	UHorizontalBox* ButBox = NewObject<UHorizontalBox>();
	UBorder* ButBorder = NewObject<UBorder>();
	ButBorder->SetContent(ButBox);
	ButBorder->SetBrushColor(FLinearColor(.05, .05, .05, 0.0));

	// Add button area to GUI
	UCanvasPanelSlot* UISlot = Root->AddChildToCanvas(ButBorder);
	UISlot->SetSize(FVector2D(580, 30));
	UISlot->SetPosition(FVector2D(10, 10));

	// Setup initial buttons
	UButton* LoadUSDButton = NewObject<UButton>();
	LoadUSDButton->OnClicked.AddDynamic(this, &ULayoutWidget::LoadUSD);
	Size = NewObject<USizeBox>();
	Size->SetWidthOverride(535);
	Size->SetContent(LoadUSDButton);
	UHorizontalBoxSlot* UISlot_Hor = ButBox->AddChildToHorizontalBox(Size);
	UISlot_Hor->SetPadding(FMargin(13, 2));

	FButtonStyle ButtonStyle = LoadUSDButton->GetStyle();
	ButtonStyle.Normal.OutlineSettings.Color = ButtonStyle.Hovered.OutlineSettings.Color = ButtonStyle.Pressed.OutlineSettings.Color = FLinearColor::Black;
	LoadUSDButton->SetStyle(ButtonStyle);
	LoadUSDButton->SetContent(UHelpers::MakeTextBlock("Load USD", 8, White));
	LoadUSDButton->SetBackgroundColor(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f));

	// 1. Camera Assets Group
	UButton* LoadAllCamBtn = NewObject<UButton>();
	UButton* UnLoadAllCamBtn = NewObject<UButton>();
	LoadAllCamBtn->OnClicked.AddDynamic(this, &ULayoutWidget::LoadAllCam);
	UnLoadAllCamBtn->OnClicked.AddDynamic(this, &ULayoutWidget::UnLoadAllCam);
	GenerateAssetGroup(CamVerticalBox, LoadAllCamBtn, UnLoadAllCamBtn, "Cameras", 50, 30);

	// 2. Skeletal Mesh Assets Group
	UButton* LoadAllSkelBtn = NewObject<UButton>();
	UButton* UnLoadAllSkelBtn = NewObject<UButton>();
	LoadAllSkelBtn->OnClicked.AddDynamic(this, &ULayoutWidget::LoadAllSkel);
	UnLoadAllSkelBtn->OnClicked.AddDynamic(this, &ULayoutWidget::UnLoadAllSkel);
	GenerateAssetGroup(SkelVerticalBox, LoadAllSkelBtn, UnLoadAllSkelBtn, "Skeletal Meshes", 170, 120);

	// 3. Static Mesh Assets Group
	UButton* LoadAllStaticBtn = NewObject<UButton>();
	UButton* UnLoadAllStaticBtn = NewObject<UButton>();
	LoadAllStaticBtn->OnClicked.AddDynamic(this, &ULayoutWidget::LoadAllStatic);
	UnLoadAllStaticBtn->OnClicked.AddDynamic(this, &ULayoutWidget::UnLoadAllStatic);
	GenerateAssetGroup(StaticVerticalBox, LoadAllStaticBtn, UnLoadAllStaticBtn, "Static Meshes", 380, 120);




}





void ULayoutWidget::SaveUSD()
{
	// Get all selected actors
	TArray<AStaticMeshActor*> ActorArray;
	USelection* SelectedActors = GEditor->GetSelectedActors();

	// Loop through actors, add to array if StaticMeshActor
	for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		AActor* Actor = Cast<AActor>(*Iter);
		if (Actor)
		{
			AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Actor);
			if (MeshActor)
			{
				ActorArray.Add(MeshActor);
			}
		}
	}
	
	// If actors found, write JSON file
	if (ActorArray.Num() > 0)
	{
		FString OutFile = SaveUSDFile();
		if (!OutFile.IsEmpty())
		{

			UUSDLayout::WriteUSDLayoutData(OutFile, ActorArray, GetSequenceActor());

		}
	}
}


void ULayoutWidget::LoadUSD()
{
	// Clear assets from GUI
	CamVerticalBox->ClearChildren();
	CamAssetBoxArray.Empty();

	// Clear assets from GUI
	SkelVerticalBox->ClearChildren();
	SkelAssetBoxArray.Empty();

	// Clear assets from GUI
	StaticVerticalBox->ClearChildren();
	StaticAssetBoxArray.Empty();

	// Clear assets from GUI
	//StaticAnimVerticalBox->ClearChildren();
	//StaticAnimAssetBoxArray.Empty();

	// Get Sequencer
	ALevelSequenceActor* CurrentSequencer = GetSequenceActor();
	
	// Don't try anything if there is no sequencer
	if (CurrentSequencer == nullptr)
	{
		return;
	}

	// Open JSON file
	FString FileName = LoadUSDFile();


	pxr::UsdStageRefPtr Stage =  UUSDLayout::ReadUSDLayoutData(FileName);


	// Loop through StaticMesh array, create asset, add to GUI

	// 1. Get the prim by its path
	pxr::UsdPrim StaticMeshesGroup = Stage->GetPrimAtPath(pxr::SdfPath("/Root/StaticMeshes"));

	if (StaticMeshesGroup)
	{
		// 1. Process Static Meshes
		for (pxr::UsdPrim Prim : StaticMeshesGroup.GetChildren())
		{


			// Create asset
			UStaticMeshAsset* NewAsset = NewObject<UStaticMeshAsset>();
			NewAsset->Init(CurrentSequencer, Stage, Prim);

			// Create asset box for GUI
			UAssetBox* AssetBoxA = NewObject<UAssetBox>();
			AssetBoxA->Init(Cast<UAsset>(NewAsset));

			// Add asset box to GUI
			StaticVerticalBox->AddChildToVerticalBox(AssetBoxA);
			StaticAssetBoxArray.Add(AssetBoxA);

		}

		// 2. Process Animated Static Meshes
		pxr::UsdPrim AnimatedStaticMeshesGroup = Stage->GetPrimAtPath(pxr::SdfPath("/Root/AnimatedStaticMeshes"));
		for (pxr::UsdPrim Prim : AnimatedStaticMeshesGroup.GetChildren())
		{


			// Create asset
			UAnimatedMeshAsset* NewAsset = NewObject<UAnimatedMeshAsset>();
			NewAsset->Init(CurrentSequencer, Stage, Prim);

			// Create asset box for GUI
			UAssetBox* AssetBoxA = NewObject<UAssetBox>();
			AssetBoxA->Init(Cast<UAsset>(NewAsset));

			// Add asset box to GUI
			StaticVerticalBox->AddChildToVerticalBox(AssetBoxA);
			StaticAssetBoxArray.Add(AssetBoxA);
		}

		// 3. Process Animated Skeletal Meshes
		pxr::UsdPrim SkeletalMeshesGroup = Stage->GetPrimAtPath(pxr::SdfPath("/Root/SkeletalMeshes"));
		if (SkeletalMeshesGroup)
		{
			for (pxr::UsdPrim Prim : SkeletalMeshesGroup.GetChildren())
			{
				// Create asset
				UAnimatedSkelMeshAsset* NewAsset = NewObject<UAnimatedSkelMeshAsset>();
				NewAsset->Init(CurrentSequencer, Stage, Prim);

				// Create asset box for GUI
				UAssetBox* AssetBox = NewObject<UAssetBox>();
				AssetBox->Init(Cast<UAsset>(NewAsset));

				// Add asset box to GUI
				SkelVerticalBox->AddChildToVerticalBox(AssetBox);
				SkelAssetBoxArray.Add(AssetBox);
			}
		}
		// 3. Process Animated Skeletal Meshes
		pxr::UsdPrim CamerasGroup = Stage->GetPrimAtPath(pxr::SdfPath("/Root/Cameras"));
		if (CamerasGroup)
		{
			for (pxr::UsdPrim Prim : CamerasGroup.GetChildren())
			{
				// Create asset
				UCamera* NewAsset = NewObject<UCamera>();
				NewAsset->Init(CurrentSequencer, Stage, Prim);

				// Create asset box for GUI
				UAssetBox* AssetBox = NewObject<UAssetBox>();
				AssetBox->Init(Cast<UAsset>(NewAsset));

				// Add asset box to GUI
				CamVerticalBox->AddChildToVerticalBox(AssetBox);
				CamAssetBoxArray.Add(AssetBox);
			}
		}
	}

}




void ULayoutWidget::LoadAllCam()
{
	// Attempt to load each asset, update GUI
	for (UAssetBox* AssetBoxA : CamAssetBoxArray)
	{
		AssetBoxA->Asset->Load();
		AssetBoxA->EvaluateState();
	}

	// Update sequencer GUI
	ALevelSequenceActor* CurrentSequencer = GetSequenceActor();
	UHelpers::UpdateSequencer(CurrentSequencer);
}

void ULayoutWidget::LoadAllSkel()
{
	// Attempt to load each asset, update GUI
	for (UAssetBox* AssetBoxA : SkelAssetBoxArray)
	{
		AssetBoxA->Asset->Load();
		AssetBoxA->EvaluateState();
	}

	// Update sequencer GUI
	ALevelSequenceActor* CurrentSequencer = GetSequenceActor();
	UHelpers::UpdateSequencer(CurrentSequencer);
}

void ULayoutWidget::LoadAllStatic()
{
	// Attempt to load each asset, update GUI
	for (UAssetBox* AssetBoxA : StaticAssetBoxArray)
	{
		AssetBoxA->Asset->Load();
		AssetBoxA->EvaluateState();
	}

	// Update sequencer GUI
	ALevelSequenceActor* CurrentSequencer = GetSequenceActor();
	UHelpers::UpdateSequencer(CurrentSequencer);
}

void ULayoutWidget::LoadAllStaticAnim()
{
	// Attempt to load each asset, update GUI
	for (UAssetBox* AssetBoxA : StaticAnimAssetBoxArray)
	{
		AssetBoxA->Asset->Load();
		AssetBoxA->EvaluateState();
	}

	// Update sequencer GUI
	ALevelSequenceActor* CurrentSequencer = GetSequenceActor();
	UHelpers::UpdateSequencer(CurrentSequencer);
}


void ULayoutWidget::UnLoadAllCam()
{
	// Attempt to unload each asset
	for (UAssetBox* AssetBoxA : CamAssetBoxArray)
	{
		AssetBoxA->Asset->Unload();
		AssetBoxA->EvaluateState();
	}

	// Update sequencer GUI
	ALevelSequenceActor* CurrentSequencer = GetSequenceActor();
	UHelpers::UpdateSequencer(CurrentSequencer);
}

void ULayoutWidget::UnLoadAllSkel()
{
	// Attempt to unload each asset
	for (UAssetBox* AssetBoxA : SkelAssetBoxArray)
	{
		AssetBoxA->Asset->Unload();
		AssetBoxA->EvaluateState();
	}

	// Update sequencer GUI
	ALevelSequenceActor* CurrentSequencer = GetSequenceActor();
	UHelpers::UpdateSequencer(CurrentSequencer);
}

void ULayoutWidget::UnLoadAllStatic()
{
	// Attempt to unload each asset
	for (UAssetBox* AssetBoxA : StaticAssetBoxArray)
	{
		AssetBoxA->Asset->Unload();
		AssetBoxA->EvaluateState();
	}

	// Update sequencer GUI
	ALevelSequenceActor* CurrentSequencer = GetSequenceActor();
	UHelpers::UpdateSequencer(CurrentSequencer);
}

void ULayoutWidget::UnLoadAllStaticAnim()
{
	// Attempt to unload each asset
	for (UAssetBox* AssetBoxA : StaticAnimAssetBoxArray)
	{
		AssetBoxA->Asset->Unload();
		AssetBoxA->EvaluateState();
	}

	// Update sequencer GUI
	ALevelSequenceActor* CurrentSequencer = GetSequenceActor();
	UHelpers::UpdateSequencer(CurrentSequencer);
}

FString ULayoutWidget::LoadUSDFile()
{
	TArray<FString> OutFileNames;
	FString OutName;

	// Setup GUI related arguments
	const void* ParentWindowPtr = UHelpers::GetParentWindow();
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

	// Let user select file from browser GUI
	if (DesktopPlatform)
	{
		uint32 SelectionFlag = 1; 
		DesktopPlatform->OpenFileDialog(ParentWindowPtr,
			FString("Load USD"),
			FPaths::ProjectPluginsDir() / "unreal-json-layout-plugin/Saved",
			FString(""),
			TEXT("(Layout Files)|*.usda;)"),
			SelectionFlag,
			OutFileNames);
	}
	
	// Fetch output
	if (OutFileNames.Num() > 0)
	{
		OutName = OutFileNames.Pop();
	}

	return OutName;
}


FString ULayoutWidget::SaveUSDFile()
{
	TArray<FString> OutFileNames;
	FString OutName;

	// Setup GUI related arguments
	const void* ParentWindowPtr = UHelpers::GetParentWindow();
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

	// Let user create or select file from browser GUI
	if (DesktopPlatform)
	{
		uint32 SelectionFlag = 1;
		DesktopPlatform->SaveFileDialog(ParentWindowPtr,
			FString("Save USD"),
			FPaths::ProjectPluginsDir() / "unreal-json-layout-plugin/Saved",
			FString(""),
			TEXT("(Layout Files)|*.usda;)"),
			SelectionFlag,
			OutFileNames);
	}

	// Fetch output
	if (OutFileNames.Num() > 0)
	{
		OutName = OutFileNames.Pop();
	}

	return OutName;
}


ALevelSequenceActor* ULayoutWidget::GetSequenceActor()
{
	// Get list of all LevelSequenceActors
	ALevelSequenceActor* CurrentSequencer = nullptr;
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(),
		ALevelSequenceActor::StaticClass(),
		FoundActors);

	// Return first one found
	if (FoundActors.Num() > 0)
	{
		CurrentSequencer = Cast<ALevelSequenceActor>(FoundActors[0]);
	}

	return CurrentSequencer;
}
