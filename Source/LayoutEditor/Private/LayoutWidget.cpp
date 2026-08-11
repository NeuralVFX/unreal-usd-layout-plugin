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
#include "Runtime/LevelSequence/Public/LevelSequenceActor.h"
#include "Components/PanelWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
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


void ULayoutWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	UCanvasPanel* Root = Cast<UCanvasPanel>(GetRootWidget());

	// Setup scroll box to hold layout assets
	UScrollBox* ScrollBox = NewObject<UScrollBox>();
	VerticalBox = NewObject<UVerticalBox>();
	ScrollBox->AddChild(VerticalBox);

	UBorder* Border =NewObject<UBorder>();
	Border->SetContent(ScrollBox);
	Border->SetBrushColor(FLinearColor(.1, .1, .1));
	
	// Add scroll box to GUI
	UCanvasPanelSlot* UISlot = Root->AddChildToCanvas(Border);
	UISlot->SetSize(FVector2D(580, 400));
	UISlot->SetPosition(FVector2D(10, 30));

	// Setup horizontal box to hold GUI buttons
	UHorizontalBox * BotBox = NewObject<UHorizontalBox>();
	UBorder* BotBorder = NewObject<UBorder>();
	BotBorder->SetContent(BotBox);
	BotBorder->SetBrushColor(FLinearColor(.1, .1, .1));

	// Add button area to GUI
	UISlot = Root->AddChildToCanvas(BotBorder);
	UISlot->SetSize(FVector2D(580, 30));
	UISlot->SetPosition(FVector2D(10, 440));

	// Setup buttons
	UButton*  LoadAllButton = NewObject<UButton>();
	LoadAllButton->OnClicked.AddDynamic(this, &ULayoutWidget::LoadAll);
	UHorizontalBoxSlot* UISlot_Hor = BotBox->AddChildToHorizontalBox(LoadAllButton);
	UISlot_Hor->SetPadding(FMargin(13, 2));

	UButton* UnLoadAllButton = NewObject<UButton>();
	UnLoadAllButton->OnClicked.AddDynamic(this, &ULayoutWidget::UnLoadAll);
	UISlot_Hor = BotBox->AddChildToHorizontalBox(UnLoadAllButton);
	UISlot_Hor->SetPadding(FMargin(13, 2));

	UButton* LoadUSDButton = NewObject<UButton>();
	LoadUSDButton->OnClicked.AddDynamic(this, &ULayoutWidget::LoadUSD);
	UISlot_Hor = BotBox->AddChildToHorizontalBox(LoadUSDButton);
	UISlot_Hor->SetPadding(FMargin(13, 2));

	UButton* SaveUSDButton = NewObject<UButton>();
	SaveUSDButton->OnClicked.AddDynamic(this, &ULayoutWidget::SaveUSD);
	UISlot_Hor = BotBox->AddChildToHorizontalBox(SaveUSDButton);
	UISlot_Hor->SetPadding(FMargin(13, 2));

	// Setup text
	FLinearColor Black = FLinearColor(0, 0,0 );
	FLinearColor White = FLinearColor(1, 1, 1);

	UTextBlock* LoadAllText = UHelpers::MakeTextBlock("Load All Assets", 8, Black);
	UTextBlock* UnLoadAllText = UHelpers::MakeTextBlock("UnLoad All Assets", 8, Black);
	UTextBlock* IsLoadedText = UHelpers::MakeTextBlock("Is Loaded", 8, White);
	UTextBlock* SceneNameText = UHelpers::MakeTextBlock("Scene Name", 8, White);
	UTextBlock* ContentNameText = UHelpers::MakeTextBlock("Content Name", 8,White);
	UTextBlock* ObjectTypeText = UHelpers::MakeTextBlock("Object Type", 8, White);
	UTextBlock* LoadJsonText = UHelpers::MakeTextBlock("Load Layout File", 8, Black);
	UTextBlock* SaveJsonText = UHelpers::MakeTextBlock("Save Layout File", 8, Black);

	// Set all button text
	LoadAllButton->SetContent(LoadAllText);
	UnLoadAllButton->SetContent(UnLoadAllText);
	SaveUSDButton->SetContent(SaveJsonText);
	LoadUSDButton->SetContent(LoadJsonText);

	// Setup column label array
	UHorizontalBox* TopBox=  NewObject<UHorizontalBox>();

	USizeBox* Size = NewObject<USizeBox>();
	Size->SetWidthOverride(70);
	Size->SetContent(IsLoadedText);
	UISlot_Hor = TopBox->AddChildToHorizontalBox(Size);
	UISlot_Hor->SetPadding(FMargin(15, 0));

	// Add text to column label area
	Size = NewObject<USizeBox>();
	Size->SetWidthOverride(155);
	Size->SetContent(ContentNameText);
	UISlot_Hor = TopBox->AddChildToHorizontalBox(Size);

	Size = NewObject<USizeBox>();
	Size->SetWidthOverride(155);
	Size->SetContent(SceneNameText);
	UISlot_Hor = TopBox->AddChildToHorizontalBox(Size);

	Size = NewObject<USizeBox>();
	Size->SetWidthOverride(155);
	Size->SetContent(ObjectTypeText);
	UISlot_Hor = TopBox->AddChildToHorizontalBox(Size);

	UBorder* TopBorder = NewObject<UBorder>();
	TopBorder->SetContent(TopBox);
	TopBorder->SetBrushColor(FLinearColor(.1, .1, .1));

	// Add column label area to GUI
	UISlot = Root->AddChildToCanvas(TopBorder);
	UISlot->SetSize(FVector2D(580, 20));
	UISlot->SetPosition(FVector2D(10, 5));
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
	VerticalBox->ClearChildren();
	AssetBoxArray.Empty();

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
			VerticalBox->AddChildToVerticalBox(AssetBoxA);
			AssetBoxArray.Add(AssetBoxA);

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
			VerticalBox->AddChildToVerticalBox(AssetBoxA);
			AssetBoxArray.Add(AssetBoxA);
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
				VerticalBox->AddChildToVerticalBox(AssetBox);
				AssetBoxArray.Add(AssetBox);
			}
		}
		// 3. Process Cameras
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
				VerticalBox->AddChildToVerticalBox(AssetBox);
				AssetBoxArray.Add(AssetBox);
			}
		}
	}

}




void ULayoutWidget::LoadAll()
{
	// Attempt to load each asset, update GUI
	for (UAssetBox* AssetBoxA : AssetBoxArray)
	{
		AssetBoxA->Asset->Load();
		AssetBoxA->EvaluateState();
	}

	// Update sequencer GUI
	ALevelSequenceActor* CurrentSequencer = GetSequenceActor();
	UHelpers::UpdateSequencer(CurrentSequencer);
}


void ULayoutWidget::UnLoadAll()
{
	// Attempt to unload each asset
	for (UAssetBox* AssetBoxA : AssetBoxArray)
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
