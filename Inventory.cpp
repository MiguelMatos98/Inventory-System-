#include "Inventory.h"

UInventory::UInventory(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer),
      MaxRows(3),
      MaxColumns(4),
      Canvas(nullptr),
      Background(nullptr),
      BackgroundSlot(nullptr),
      BackgroundVerticalBox(nullptr),
      Title(nullptr),
      TitleVerticalBoxSlot(nullptr),
      Grid(nullptr),
      GridVerticalBoxSlot(nullptr),
      GridSlot(nullptr),
      PoppedOutItemWidget(nullptr),
      HoveredSlotIndex(INDEX_NONE),
      OriginSlotIndex(INDEX_NONE),
      PoppedOutItem(FItem()),
      MouseScreenSpacePosition(FVector2D::ZeroVector),
      MouseWidgetLocalPosition(FVector2D::ZeroVector),
      DragState(EDragState::None),
      bIsMouseInsideInventory(false)
{
    // Set menber array's size to 12 (3x4)
    Items.SetNum(MaxRows * MaxColumns);
    Slots.SetNum(MaxRows * MaxColumns);
}

void UInventory::NativeOnInitialized()
{
    // Initialize widget memnbers and layout before inventory construction 

    Super::NativeOnInitialized();

    if (!WidgetTree)
    {
        #if WITH_EDITOR
             UE_LOG(LogTemp, Error, TEXT("WidgetTree is null"));
        #else
             UE_LOG(LogTemp, Fatal, TEXT("WidgetTree is null"));
        #endif

        return;
    }

    Canvas = NewObject<UCanvasPanel>(this);

    // It's mandatory to set the first widget element as the root widget of the widget's tree
    WidgetTree->RootWidget = Canvas;

    // Create vertical Box to hold background border and create a gray backround boder
    BackgroundVerticalBox = NewObject<UVerticalBox>(this);
    Background = NewObject<UBorder>(this);
    Background->SetBrushColor(FLinearColor::Gray);
    Background->SetPadding(FMargin(7.5f, 0.0f, 7.5f, 0.0f));

    // Name the inventory "Inventory"
    Title = NewObject<UTextBlock>(this);
    Title->SetText(FText::FromString(TEXT("Inventory")));

    // Need a vertical bocx slot for title middle adjustment
    TitleVerticalBoxSlot = BackgroundVerticalBox->AddChildToVerticalBox(Title);
    TitleVerticalBoxSlot->SetHorizontalAlignment(HAlign_Center);
    TitleVerticalBoxSlot->SetVerticalAlignment(VAlign_Top);
    TitleVerticalBoxSlot->SetPadding(FMargin(10, 10, 10, 10));

    // Create a grid for the inevntory and readjust grid alignment within baackground's vertical box 
    Grid = NewObject<UUniformGridPanel>(this);
    Grid->SetSlotPadding(FMargin(7.0f, 7.0f, 7.0f, 7.0f));
    GridVerticalBoxSlot = BackgroundVerticalBox->AddChildToVerticalBox(Grid);
    GridVerticalBoxSlot->SetHorizontalAlignment(HAlign_Fill);
    GridVerticalBoxSlot->SetVerticalAlignment(VAlign_Fill);

    // Setting BackgourndBorder content to what is inside the BackgroundBorder's VerticalBox and position/anchoring
    Background->SetContent(BackgroundVerticalBox);
    BackgroundSlot = Canvas->AddChildToCanvas(Background);
    BackgroundSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
    BackgroundSlot->SetAlignment(FVector2D(1.0f, 0.0f));
    BackgroundSlot->SetOffsets(FMargin(-10.0f, 11.0f, 485.0f, 419.0f));

    // Call create method to colonize the inventory with slots
    Create();
}

void UInventory::NativeConstruct()
{
    Super::NativeConstruct();

    // Refresh inventory before it gets added to viewport
    RefreshInventory();
}

FReply UInventory::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
    {
        HoveredSlotIndex = FindHoveredSlot(InMouseEvent);

        // Checking whether hovered slot index is not invalid and it exist as a valid index for the items array 
        if (HoveredSlotIndex != INDEX_NONE && Items.IsValidIndex(HoveredSlotIndex))
        {
            // Then checking for item validity by checking whether it's object reference as been initialized 
            // (Which should be already intialized) 
            if (Items[HoveredSlotIndex].WorldObjectReference)
            {
                // Keeping track of original slot before drag
                OriginSlotIndex = HoveredSlotIndex;

                // Setting the item on the hoivered slot as the drag item
                PoppedOutItem = Items[HoveredSlotIndex];

                DragState = EDragState::Pressed; 

                // Retriving the low-level slate widget representation of this inevntory
                TSharedPtr<SWidget> RootSlate = GetCachedWidget();
                if (!RootSlate.IsValid())
                {
                    #if	WITH_EDITOR
                        UE_LOG(LogTemp, Error, TEXT("Couldn't get cached root slate widget!"));
                    #else
                        UE_LOG(LogTemp, Fatal, TEXT("Couldn't get cached root slate widget!"));
                    #endif

                    return FReply::Unhandled();
                }

                // Passing the the low-level slate widget of my inventory as reference in order to keep 
                // reciving mouse events (Even when mouse is outside the inventory bounds)
                return FReply::Handled().CaptureMouse(RootSlate.ToSharedRef());
            }
        }
    }
    return FReply::Unhandled();
}

FReply UInventory::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (DragState != EDragState::Pressed && DragState != EDragState::Dragging)
        return Super::NativeOnMouseMove(InGeometry, InMouseEvent);

    Super::NativeOnMouseMove(InGeometry, InMouseEvent);

    MouseScreenSpacePosition = InMouseEvent.GetScreenSpacePosition();
    MouseWidgetLocalPosition = InGeometry.AbsoluteToLocal(MouseScreenSpacePosition);

    // Updating whether the mouse is inside the inventory background
    bIsMouseInsideInventory = false;
    if (Background && Background->IsValidLowLevelFast())
    {
        const FGeometry& BackgroundGeom = Background->GetCachedGeometry();
        bIsMouseInsideInventory = BackgroundGeom.IsUnderLocation(MouseScreenSpacePosition);
    }

    // Checking for starting a drag (with movement threshold instead of requiring exit)
    if (DragState == EDragState::Pressed && OriginSlotIndex != INDEX_NONE && Slots.IsValidIndex(OriginSlotIndex))
    {
        const FVector2D& DeltaCursor = InMouseEvent.GetCursorDelta();
        if (DeltaCursor.SizeSquared() > FMath::Square(4.0f)) // drag threshold
        {
            // Transitioning to dragging
            UBorder* OriginBorder = Slots[OriginSlotIndex].Get();
            if (OriginBorder && OriginBorder->IsValidLowLevelFast())
            {
                if (USizeBox* Box = Cast<USizeBox>(OriginBorder->GetContent()))
                    Box->ClearChildren();

                DragState = EDragState::Dragging;

                PoppedOutItemWidget = NewObject<UOverlay>(this);
                if (!PoppedOutItemWidget)
                {
                    UE_LOG(LogTemp, Error, TEXT("Failed to create PoppedOutItemWidget"));
                    return FReply::Handled();
                }

                PoppedOutItemWidget->SetVisibility(ESlateVisibility::HitTestInvisible);

                UImage* Image = NewObject<UImage>(this);
                Image->SetVisibility(ESlateVisibility::Visible);
                Image->SetColorAndOpacity(FLinearColor::Blue);
                if (UOverlaySlot* ImageSlot = PoppedOutItemWidget->AddChildToOverlay(Image))
                {
                    ImageSlot->SetHorizontalAlignment(HAlign_Fill);
                    ImageSlot->SetVerticalAlignment(VAlign_Fill);
                }

                UTextBlock* Text = NewObject<UTextBlock>(this);
                Text->SetVisibility(ESlateVisibility::Visible);
                Text->SetText(FText::AsNumber(PoppedOutItem.Index));
                Text->SetColorAndOpacity(FLinearColor::Red);
                Text->SetJustification(ETextJustify::Center);
                Text->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 20));
                if (UOverlaySlot* TextSlot = PoppedOutItemWidget->AddChildToOverlay(Text))
                {
                    TextSlot->SetHorizontalAlignment(HAlign_Center);
                    TextSlot->SetVerticalAlignment(VAlign_Center);
                }

                if (Canvas && Canvas->IsValidLowLevelFast())
                {
                    if (UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(PoppedOutItemWidget))
                    {
                        CanvasSlot->SetSize(FVector2D(100.0f, 100.0f));
                        CanvasSlot->SetPosition(MouseWidgetLocalPosition - FVector2D(50.0f, 50.0f));
                        CanvasSlot->SetZOrder(100);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Error, TEXT("Failed to add popped-out widget to canvas"));
                    }
                }
            }
        }
    }

    // Handling dragging behavior
    if (DragState == EDragState::Dragging && PoppedOutItemWidget)
    {
        // Moving smooth the dragged widget
        if (UCanvasPanelSlot* DraggedItemWidgetSlot = Cast<UCanvasPanelSlot>(PoppedOutItemWidget->Slot))
        {
            FVector2D TargetPosition = MouseWidgetLocalPosition - FVector2D(50.0f, 50.0f);
            FVector2D CurrentPosition = DraggedItemWidgetSlot->GetPosition();
            FVector2D NewPosition = FMath::Vector2DInterpTo(CurrentPosition, TargetPosition, GetWorld()->DeltaTimeSeconds, 25.0f);
            DraggedItemWidgetSlot->SetPosition(NewPosition);
        }

        // Updating hovered slot
        HoveredSlotIndex = FindHoveredSlot(InMouseEvent);

        // Rearranging internally items while dragging
        if (bIsMouseInsideInventory && HoveredSlotIndex != INDEX_NONE)
        {
            InternallyRearrangeItems(InMouseEvent);
        }

        return FReply::Handled();
    }

    return FReply::Handled();
}

FReply UInventory::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{ 
    // Return early if not dragging or pressing
    if (DragState != EDragState::Pressed && DragState != EDragState::Dragging)
        return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);

    // Remove popped out item widget from canvas if it exists
    if (PoppedOutItemWidget)
    {
        Canvas->RemoveChild(PoppedOutItemWidget);
        PoppedOutItemWidget = nullptr;
    }

    MouseScreenSpacePosition = InMouseEvent.GetScreenSpacePosition();
    HoveredSlotIndex = FindHoveredSlot(InMouseEvent);

    // When hovered slot is valid and also exists in items array
    if (HoveredSlotIndex != INDEX_NONE && Items.IsValidIndex(HoveredSlotIndex))
    {
        // Release item on empty slot  
        if (!Items[HoveredSlotIndex].WorldObjectReference)
        {
            Items[HoveredSlotIndex] = PoppedOutItem;
            Items[OriginSlotIndex] = FItem{};
        }
        // Release item on the same slot  
        else if (HoveredSlotIndex == OriginSlotIndex)
        {
            Items[OriginSlotIndex] = PoppedOutItem;
        }
        else
        {
            // Swap with occupied slot  
            Items[OriginSlotIndex] = Items[HoveredSlotIndex];
            Items[HoveredSlotIndex] = PoppedOutItem;
        }
    }
    else if (!bIsMouseInsideInventory)
    {
        // Spawn world object when dropped outside inventory, using deferred spawn
        if (UWorld* World = GetWorld(); World && OriginSlotIndex != INDEX_NONE && Items.IsValidIndex(OriginSlotIndex))
        {
            // Begin deferred spawn
            FTransform SpawnTransform = PoppedOutItem.WorldObjectTransform;
            AStaticMeshActor* MeshActor = World->SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

            if (MeshActor)
            {
                UStaticMeshComponent* MeshComp = MeshActor->GetStaticMeshComponent();
                MeshComp->SetMobility(EComponentMobility::Movable);

                // Set mesh
                if (UStaticMesh* Mesh = PoppedOutItem.StaticMesh.LoadSynchronous())
                {
                    MeshComp->SetStaticMesh(Mesh);
                }

                // Apply stored materials safely
                const int32 SlotCount = MeshComp->GetNumMaterials();
                for (int32 MaterialIndex = 0; MaterialIndex < PoppedOutItem.StoredMaterials.Num(); ++MaterialIndex)
                {
                    // Validate slot index and load soft pointer
                    if (MaterialIndex < SlotCount)
                    {
                        UMaterialInterface* Mat = PoppedOutItem.StoredMaterials[MaterialIndex].LoadSynchronous();
                        if (Mat)
                        {
                            MeshComp->SetMaterial(MaterialIndex, Mat);
                        }
                    }
                }

                // Finish spawning so BP construction scripts run AFTER our setup
                UGameplayStatics::FinishSpawningActor(MeshActor, SpawnTransform);
            }

            // Clear the original slot
            Items[OriginSlotIndex] = FItem{};
        }
    }
    else
    {
        // If dropped anywhere else inside inventory but not on a slot → restore
        if (OriginSlotIndex != INDEX_NONE && Items.IsValidIndex(OriginSlotIndex))
            Items[OriginSlotIndex] = PoppedOutItem;
    }

    // Reset state
    PoppedOutItem = FItem{};
    OriginSlotIndex = INDEX_NONE;
    DragState = EDragState::Dropped;
    bIsMouseInsideInventory = false;

    RefreshInventory();
    return FReply::Handled().ReleaseMouseCapture();
}

void UInventory::AddItem(AActor* ItemActor)
{
    if (!ItemActor) return;

    int32 EmptySlot = FindFirstEmptySlot();
    if (EmptySlot == INDEX_NONE)
    {
    
        // When there's no empty slot inventory is full 
        #if	WITH_EDITOR
             UE_LOG(LogTemp, Error, TEXT("No free slots because inventory is full!"));
        #endif

        return;
    }
    
    // Keep tarack of all existing indeces in the items array 
    TSet<int32> UsedIndices;
    for (const FItem& Item : Items)
    {
        if (Item.Index != INDEX_NONE)
        {
            UsedIndices.Add(Item.Index);
        }
    }

    // Valid index is use for not duplicating indexes 
    // giving always the next availble index that is not pressent on any inventory item
    // (Remenber we do this by keeping track of the item array indexes with the set of used indices)
    int32 ValidIndex = 0;
    while (UsedIndices.Contains(ValidIndex)) // if 0,1,2 then continue
    {
        ++ValidIndex;
    }

    // Since empty slot is valid then assign this new element accordingly 
    FItem& NewItem = Items[EmptySlot];

    NewItem.WorldObjectReference = ItemActor->GetClass();

    NewItem.WorldObjectTransform = ItemActor->GetActorTransform();

    // Assigning the next available valid index as a unique index for that item
    NewItem.Index = ValidIndex;

    // Storing meshes and its multiple materials 
    if (UStaticMeshComponent* MeshComponent = ItemActor->FindComponentByClass<UStaticMeshComponent>())
    {
        if (MeshComponent->GetStaticMesh())
        {
            NewItem.StaticMesh = TSoftObjectPtr<UStaticMesh>(MeshComponent->GetStaticMesh());
        }

        for (int32 i = 0; i < MeshComponent->GetNumMaterials(); ++i)
        {
            UMaterialInterface* MaterialInterface = MeshComponent->GetMaterial(i);
            if (IsValid(MaterialInterface))
            {
                NewItem.StoredMaterials.Add(TSoftObjectPtr<UMaterialInterface>(MaterialInterface));
            }
        }
    }

    RefreshInventory();

    ItemActor->Destroy();
}

int32 UInventory::FindHoveredSlot(const FPointerEvent& InMouseEvent)
{
    MouseScreenSpacePosition = InMouseEvent.GetScreenSpacePosition();

    int32 NearestSlotIndex = INDEX_NONE;

    float MinimumDistanceToSlot = FLT_MAX;

    for (int32 Row = 0; Row < int32(MaxRows); ++Row)
    {
        for (int32 Column = 0; Column < int32(MaxColumns); ++Column)
        {
            // Keep thatc of current hovered slot for debugging purpuses
            const int32 CurrentHoveredSlot = Row * MaxColumns + Column;

            // Cheking whether current hovered slot is a vvalid slot index and it exist as an index on the slot array 
            if (!Slots.IsValidIndex(CurrentHoveredSlot) || !Slots[CurrentHoveredSlot])
            {
                #if	WITH_EDITOR
                     UE_LOG(LogTemp, Error, TEXT("Hovered slot index %d is invalid on FindHoveredSlot()"), CurrentHoveredSlot);
                #else
                     UE_LOG(LogTemp, Error, TEXT("Hovered slot index %d is invalid on FindHoveredSlot()"), CurrentHoveredSlot);
                #endif

                continue;
            }

            // Trsack slot geometry in order to check whther mouse is within slot bounds
            const FGeometry SlotGeometry = Slots[CurrentHoveredSlot]->GetCachedGeometry();
            const FVector2D SlotAbsoluteTopLeft = SlotGeometry.LocalToAbsolute(FVector2D::ZeroVector);
            const FVector2D SlotAbsoluteSize = SlotGeometry.GetLocalSize();
            const FVector2D SlotAbsoluteBottomRight = SlotAbsoluteTopLeft + SlotAbsoluteSize;

            // Check whether the mouse position is anywhere inside mouse bounds 
            if (MouseScreenSpacePosition.X >= SlotAbsoluteTopLeft.X && MouseScreenSpacePosition.X <= SlotAbsoluteBottomRight.X &&
                MouseScreenSpacePosition.Y >= SlotAbsoluteTopLeft.Y && MouseScreenSpacePosition.Y <= SlotAbsoluteBottomRight.Y)
            {
                const FVector2D& HalfSlotScale = SlotAbsoluteSize * 0.5f;

                const FVector2D& SlotCenter = SlotAbsoluteTopLeft + HalfSlotScale;

                const float Distance = FVector2D::Distance(MouseScreenSpacePosition, SlotCenter);

                // When distance between mouse position and center slot is less than FLT_MAX
                // then just return the current hover slot as the neares slot
                if (Distance < MinimumDistanceToSlot)
                {
                    NearestSlotIndex = CurrentHoveredSlot;

                    #if	WITH_EDITOR
                         UE_LOG(LogTemp, Log, TEXT("Hovered slot index %d has mouse hovering over at a distance of %f"), CurrentHoveredSlot, Distance);
                    #endif
                }
            }
        }
    }

    return NearestSlotIndex;
}

void UInventory::RefreshInventory()
{
    // Forcing widget layout update
    if (Grid)   Grid->ForceLayoutPrepass();
    if (Canvas) Canvas->ForceLayoutPrepass();

    // iterate through every slot 
    for (int32 SlotIndex = 0; SlotIndex < Items.Num(); ++SlotIndex)
    {
        if (!Slots.IsValidIndex(SlotIndex))
            continue;

        // Get slot and its size 
        UBorder* SlotBorder = Slots[SlotIndex].Get();
        if (!SlotBorder) continue;

        USizeBox* SizeBox = Cast<USizeBox>(SlotBorder->GetContent());
        if (!SizeBox) continue;

        // Clear slot
        SizeBox->ClearChildren();

        // Skip the original slot when it empty due item dragging to not recreate an item
        // on the mouse and on on the grid
        if (DragState == EDragState::Dragging && SlotIndex == OriginSlotIndex)
            continue;

        // Recreate all items
        const FItem& Item = Items[SlotIndex];
        if (Item.WorldObjectReference)
            CreateItemIcon(SlotIndex);

        SlotBorder->SetVisibility(ESlateVisibility::Visible);

        // Force slot border widget layout update
        SlotBorder->ForceLayoutPrepass();
    }
}

void UInventory::InternallyRearrangeItems(const FPointerEvent& MouseEvent)
{
    // Returning early if none of these 2 states are true
    if (DragState != EDragState::Dragging && DragState != EDragState::Pressed)
    {
        UE_LOG(LogTemp, Warning, TEXT("Item needs to selected and moving for executiong interior drag"));

        #if	WITH_EDITOR
            UE_LOG(LogTemp, Error, TEXT("Item needs to selected and moving for executiong interior drag"));
        #else
            UE_LOG(LogTemp, Fatal, TEXT("Item needs to selected and moving for executiong interior drag"));
        #endif
        
        return;
    }

    // Update hovere slot 
    HoveredSlotIndex = FindHoveredSlot(MouseEvent);

    // Checking whether hovered slot index is invalid and it doesn't exist as a valid index for the items array 
    if (HoveredSlotIndex == INDEX_NONE || !Items.IsValidIndex(HoveredSlotIndex))
    {
        #if	WITH_EDITOR
             UE_LOG(LogTemp, Error, TEXT("Hovered slot index %d is invalid on UpdateInteriorDrag()"), HoveredSlotIndex);
        #else
             UE_LOG(LogTemp, Fatal, TEXT("Hovered slot index %d is invalid on UpdateInteriorDrag()"), HoveredSlotIndex);
        #endif
        
        return;
    }

    // In case where item has not left origin slot yet the return early no need to perfmor swap early
    if (HoveredSlotIndex == OriginSlotIndex)
    {
        #if	WITH_EDITOR
             UE_LOG(LogTemp, Log, TEXT("When hovered slot index %d is the same as original slot index then don't perform interior "), HoveredSlotIndex, OriginSlotIndex);
        #endif
        return;
    }

    // Perform interior swap in case where theres an item on the lot or when it's empty
    if (Items[HoveredSlotIndex].WorldObjectReference)
    { 
        Items[OriginSlotIndex] = Items[HoveredSlotIndex];

        Items[HoveredSlotIndex] = PoppedOutItem;

        #if	WITH_EDITOR
             UE_LOG(LogTemp, Log, TEXT("Swapped item %d with item in slot %d on UpdateInteriorDrag()"), PoppedOutItem.Index, HoveredSlotIndex);
        #endif
    }
    else
    {
        Items[OriginSlotIndex] = FItem();

        Items[HoveredSlotIndex] = PoppedOutItem;

        #if	WITH_EDITOR
             UE_LOG(LogTemp, Log, TEXT("Moved item %d to empty slot %d on UpdateInteriorDrag()"), PoppedOutItem.Index, HoveredSlotIndex);
        #endif
    }

   // After swap update origin slot to be the new hovered slot 
   OriginSlotIndex = HoveredSlotIndex;

   RefreshInventory();
}

void UInventory::CreateItemIcon(uint32 SlotIndex)
{
    // Check whether slot index is a valid index in both arrays
    if (!Items.IsValidIndex(SlotIndex) || !Slots.IsValidIndex(SlotIndex))
        return;

    // Get the SizeBox from the border
    TObjectPtr<USizeBox> SizeBox = Cast<USizeBox>(Slots[SlotIndex]->GetContent());
    if (!SizeBox)
        return;

    // Ensuring that slot contains an item and update it
    UOverlay* IconOverlay = Cast<UOverlay>(SizeBox->GetContent());
    if (!IconOverlay)
    {
        IconOverlay = NewObject<UOverlay>(this);
        IconOverlay->SetVisibility(ESlateVisibility::Visible);
        SizeBox->SetContent(IconOverlay);
    }
    else
        IconOverlay->ClearChildren();

    // Create blue icon and align it
    UImage* ItemIcon = NewObject<UImage>(this);
    ItemIcon->SetColorAndOpacity(FLinearColor::Blue);
    ItemIcon->SetVisibility(ESlateVisibility::Visible);

    if (UOverlaySlot* ImageSlot = IconOverlay->AddChildToOverlay(ItemIcon))
    {
        ImageSlot->SetHorizontalAlignment(HAlign_Fill);
        ImageSlot->SetVerticalAlignment(VAlign_Fill);
    }

    // When there's already an existing item on the inventory slot
    if (Items[SlotIndex].WorldObjectReference)
    {
        UTextBlock* CounterText = NewObject<UTextBlock>(this);
        CounterText->SetVisibility(ESlateVisibility::Visible);

        if (UOverlaySlot* TextSlot = IconOverlay->AddChildToOverlay(CounterText))
        {
            TextSlot->SetHorizontalAlignment(HAlign_Center);
            TextSlot->SetVerticalAlignment(VAlign_Center);  // or VAlign_Center if preferred
        }

        CounterText->SetText(FText::AsNumber(Items[SlotIndex].Index));
        CounterText->SetColorAndOpacity(FLinearColor::Red);
        CounterText->SetJustification(ETextJustify::Center);
        CounterText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 20));
    }
}

int32 UInventory::FindFirstEmptySlot() const
{
    for (int32 SlotIndex = 0; SlotIndex < Items.Num(); SlotIndex++)
    {
        // When no slot has item the just return that index
        if (!Items[SlotIndex].WorldObjectReference) return SlotIndex;
    }
    return INDEX_NONE;
}

void UInventory::Create()
{
    if (!Grid || !WidgetTree)
    {
        #if	WITH_EDITOR
             UE_LOG(LogTemp, Error, TEXT("Either the Grid or WidgetTree are not properly initialized!"));
        #else
             UE_LOG(LogTemp, Fatal, TEXT("Either the Grid or WidgetTree are not properly initialized!"));
        #endif

        return;
    }

    // Clear inventory before drawing anything
    Grid->ClearChildren();

    Slots.Empty();
    
    Items.Empty();


    // Initialize both array's size to 12 (gotta do this here since the constructor executes before begin play)
    Items.SetNum(MaxRows * MaxColumns);
    Slots.SetNum(MaxRows * MaxColumns);

    // Populate slots within the inventory  
    for (int32 Rows = 0; Rows < (int32)MaxRows; ++Rows)
    {
        for (int32 Columns = 0; Columns < (int32)MaxColumns; ++Columns)
        {
            int32 CurrentHoveredSlot = Rows * MaxColumns + Columns;

            UBorder* SlotBorder = NewObject<UBorder>(this);
            SlotBorder->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f));
            SlotBorder->SetVisibility(ESlateVisibility::Visible);

            USizeBox* SizeBox = NewObject<USizeBox>(this);
            SizeBox->SetWidthOverride(100.0f);
            SizeBox->SetHeightOverride(100.0f);

            SlotBorder->SetContent(SizeBox);

            // Add slots visauls to the grid and set their alignment 
            GridSlot = Grid->AddChildToUniformGrid(SlotBorder, Rows, Columns);
            GridSlot->SetHorizontalAlignment(HAlign_Center);
            GridSlot->SetVerticalAlignment(VAlign_Center);

            // Populate slots array with the grey colored border
            Slots[CurrentHoveredSlot] = SlotBorder;


            // Forcing slot border and slot size box widgets layout update
            SlotBorder->ForceLayoutPrepass();
            SizeBox->ForceLayoutPrepass();
        }
    }


    // Forcing grid widget layout update
    if (Grid) Grid->ForceLayoutPrepass();
}

void UInventory::Open()
{
    SetVisibility(ESlateVisibility::Visible);
}

void UInventory::Close()
{
    SetVisibility(ESlateVisibility::Collapsed);
}

bool UInventory::IsInventoryFull() const
{
    return (FindFirstEmptySlot() == INDEX_NONE);
}

const TArray<FItem>& UInventory::GetItems() const
{
    return Items;
}

TArray<TObjectPtr<UBorder>> UInventory::GetSlots() const
{
    return Slots;
}

TObjectPtr<UUniformGridPanel> UInventory::GetGrid() const
{
    return Grid.Get();
}
