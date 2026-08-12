// Copyright 2020 NeuralVFX, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
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

#include "USDIncludesStart.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/sdf/types.h"
#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/vt/array.h"
#include "USDIncludesEnd.h"
#include "LayoutWidget.generated.h"

/** GUI for JSON layout tool */
UCLASS(BlueprintType)
class LAYOUTEDITOR_API ULayoutWidget : public UEditorUtilityWidget
{
	GENERATED_BODY()

public:

	ULayoutWidget();
	~ULayoutWidget();

	/** GUI objects */
	class UVerticalBox* CamVerticalBox;
	class UVerticalBox* SkelVerticalBox;
	class UVerticalBox* StaticVerticalBox;
	class UVerticalBox* StaticAnimVerticalBox;


	/** Storage array for asset objects loaded from JSON file */
	TArray<class UAssetBox*> CamAssetBoxArray;
	TArray<class UAssetBox*> SkelAssetBoxArray;
	TArray<class UAssetBox*> StaticAssetBoxArray;
	TArray<class UAssetBox*> StaticAnimAssetBoxArray;


public:


	void GenerateAssetGroup(UVerticalBox*& VerticalBox, UButton* LoadAllButton, UButton* UnLoadAllButton, FString GroupName, int top, int scroll_size);

	/**
	 * Save layout of selected objects as JSON file.
	 */
	UFUNCTION(BlueprintCallable)
	void SaveUSD();

	/**
	 * Load layout from JSON file, brings up window to select JSON file.
	 */
	UFUNCTION(BlueprintCallable)
	void LoadUSD();

	/**
	 * Loop through and load all object in layout, update GUI.
	 */
	UFUNCTION(BlueprintCallable)
	void LoadAllCam();

	UFUNCTION(BlueprintCallable)
	void LoadAllSkel();

	UFUNCTION(BlueprintCallable)
	void LoadAllStatic();

	UFUNCTION(BlueprintCallable)
	void LoadAllStaticAnim();

	/**
	 * Loop through and unload all object in layout, update GUI.
	 */
	UFUNCTION(BlueprintCallable)
	void UnLoadAllCam();

	UFUNCTION(BlueprintCallable)
	void UnLoadAllSkel();

	UFUNCTION(BlueprintCallable)
	void UnLoadAllStatic();

	UFUNCTION(BlueprintCallable)
	void UnLoadAllStaticAnim();

	/**
	 * Opens GUI to select which USD file to load.
	 * @return Filename of selected file.
	 */
	UFUNCTION(BlueprintCallable)
	FString LoadUSDFile();

	/**
	 * Opens GUI to type USD filename.
	 * @return Filename which has been typed or selected.
	 */
	UFUNCTION(BlueprintCallable)
	FString SaveUSDFile();

	/**
	 * Find the Sequencer in the scene file.
	 * @return Sequencer object.
	 */
	UFUNCTION(BlueprintCallable)
	class ALevelSequenceActor* GetSequenceActor();

protected:

	/**
	 * Build Gui
	 */
	 virtual void NativePreConstruct() override;
	 
};

