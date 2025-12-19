# Lifeguard

Lifeguard is a high-performance (zero-cost on shipping builds), development-time runtime validation framework for UE5. I wrote it to move away from defensive programming and into assertive programming.

Defensive programming tries to protect the runtime environment against programmer and domain errors. Assertive programmer forces a crash on programmer errors and lights up domain errors, so they're found as early as possible.

To do this, it sort of hijacks UE5's reflection system to be able to inspect classes at runtime.

Assertive programmer treats architectural assumptions as contracts. If a contract is broken, the system demands immediate attention.

The Lifeguard system consists of three pieces: **Contracts**, **Checklists**, and **Domain Checks**.

## Installation

Place it in the `Plugins` folder, refresh your solution and compile. Add add "Lifeguard" to `PublicDependencyModuleNames` in Project.Build.cs.

## Performance

This library uses a lot of macros that compile down to noops in shipping builds. It's a library for debugging during development only.

## Contracts

- **Invariant**: Check something that should be true for some object.
- **Precondition**: Check something that should be true at the beginning of some function.
- **Postcondition**: Check something that should be true at the end of some function.
- **Scoped Postcondition**: Declares a postcondition that will be checked automatically when the current scope exits (via return, break, or exception).
- **Architecture Condition**: Check an architectural condition. Something that was promised to us by some third party (like Epic when using engine classes). Not a part of the traditional DbC paradigm but used to check things we have no control over but should be true.

### Class Invariants

The `LG_CLASS_INVARIANTS(Obj)` macro checks if the given object passes the class invariants checks. A class invariant is any member UPROPERTY that must be valid for the object to be in a valid state. Not all members of a class with a contract may be invariant, as it may be perfectly valid for some to be null or in an unset state.

All properties with the tag meta=(Invariant) must be valid. Invariants can be of the following types. If you need custom validity checks use custom invariant functions.

- `Invariant=MemSafe` - Pointers of all kinds: Weak Pointers, Soft Pointers, Class Pointers, Interface Pointers point to a valid address.
- `Invariant=MemSafeContainer` - Container (`TOptional`, `TArray`, `TMap`, `TSet`), where if it contains pointer value(s), those values must be valid. Supports: `TArray<T*>`, `TSet<T*>`, T`Map<K,V>` (checks both K and V if pointers), `TOptional<T*>` Also supports `TWeakObjectPtr`, `TSoftObjectPtr`, `TSubclassOf`, `TScriptInterface` as element types.
- `Invariant=ID` - Integer IDs whose value cannot be `INDEX_NONE`
- `Invariant=Gte0` - Numeric $\geq$ 0
- `Invariant=Gt0` - Numeric $>$ 0
- `Invariant=Lte0` - Numeric $\leq$ 0
- `Invariant=Lt0` - Numeric $<$ 0
- `Invariant=Range[a,b]` - Numeric in range. Can mix () and [] for inclusive/exclusive bound checks
- `Invariant=Name` - FNames that cannot be NAME_NONE
- `Invariant=True` - Must be true (bool)
- `Invariant=False` - Must be false (bool)

For nonstandard properties (like structs, or custom objects):

- `Invariant=FunctionName` - with signature `bool FunctionName() const` inside the class
- `Invariant=Contract*` - a pointer which must be valid, and which must also pass invariant validation

### Code Examples

```cpp
UCLASS()
class MYGAME_API AHeroCharacter : public ACharacter
{
    GENERATED_BODY()

public:

    // Ensuring memory safety and data integrity via metadata
    UPROPERTY(EditAnywhere, meta=(Invariant=MemSafe))
    UHeroDataAsset* HeroData;

    UPROPERTY(EditAnywhere, meta=(Invariant="Range[1, 100]"))
    int32 BaseHealth = 100;

    UPROPERTY(VisibleAnywhere, meta=(Invariant=Name))
    FName InternalID;

    // Custom validation logic
    UPROPERTY(meta=(Invariant=ValidateWeaponSetup))
    AWeapon* PrimaryWeapon;

    // If the optional is set, pointer must be valid
    UPROPERTY(meta=(Invariant=MemSafeContainer))
    TOptional<AWeapon*> SideWeapon;

protected:

    virtual void BeginPlay() override {
        Super::BeginPlay();
        LG_CLASS_INVARIANTS(this); // Validates all the above in one line
    }

private:

    bool ValidateWeaponSetup() const { return PrimaryWeapon != nullptr && PrimaryWeapon->IsReadyToUse(); }
};
```

### Rationale (the Magnesium Flare doctrine)

UE5 codebases are littered with `if (Ptr) Ptr->DoSomething();` ie. defensive programming. Sometimes they go a step further and do something like `checkf(Ptr, "pointer is invalid")` or its less violent sibling `ensureMsgf`.

Lifeguard is based on checkf macros, but they raise the idea of DbC assertions to first-class citizens of the codebase.

My idea is that defensive programming the way it's taught is more like sweeping errors under the bug. DbC is not defensive, it's assertive. It is "if this pointer should never be null, then doing if(ptr) {do work} else return then what you're doing is leaving the game in an undefined state if the pointer is not valid. And the problem is the game may very well not crash. So then you've got an invisible bug. If you're going to have bugs, then you might as well give them a glowstick and a loudspeaker"

Sample code, before and after.

```cpp
APawn* ASkyGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer,
    const FTransform& SpawnTransform)
{
    UE_LOG(LogSkyInitialization, Log, TEXT("%hs -- spawning default pawn at transform"), __FUNCTION__);

    FActorSpawnParameters SpawnInfo;
    SpawnInfo.Instigator = GetInstigator();
    SpawnInfo.ObjectFlags |= RF_Transient;    // Never save the default player pawns into a map.
    SpawnInfo.bDeferConstruction = true;
    if (const auto PawnClass = GetDefaultPawnClassForController(NewPlayer)) {
        if (const auto SpawnedPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnInfo)) {
            UE_LOG(LogSkyInitialization, Log, TEXT("%hs started spawning pawn"), __FUNCTION__);
            if (const auto PawnExtComp = USkyPawnExtensionComp::FindPawnExtensionComponent(SpawnedPawn)) {
                if (const auto PawnData = GetPawnDataForController(NewPlayer)) {
                    PawnExtComp->SetPawnData(PawnData);
                    UE_LOG(LogSkyInitialization, Log, TEXT("%hs setting PawnData to PawnExtComp"), __FUNCTION__);
                }
                else {
                    UE_LOG(LogSkyInitialization, Error, TEXT("%hs Game mode was unable to set PawnData on the spawned pawn [%s]."), __FUNCTION__, *GetNameSafe(SpawnedPawn));
                }
            }
            SpawnedPawn->FinishSpawning(SpawnTransform);
            UE_LOG(LogSkyInitialization, Log, TEXT("%hs finished spawning pawn"), __FUNCTION__);
            return SpawnedPawn;
        }
        else {
            UE_LOG(LogSkyInitialization, Error, TEXT("%hs Game mode was unable to spawn Pawn of class [%s] at [%s]."), __FUNCTION__, *GetNameSafe(PawnClass), *SpawnTransform.ToHumanReadableString());
        }
    }
    else {
        UE_LOG(LogSkyInitialization, Error, TEXT("Game mode was unable to spawn Pawn due to NULL pawn class."));
    }
    return nullptr;
}
```

After

```cpp
APawn* ASkyGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer,
    const FTransform& SpawnTransform)
{
    LG_PRECOND(NewPlayer)
    LG_SCOPED_CHECKLIST_STEP(FSkyGameModeInitChecklist::ChecklistName, FSkyGameModeInitChecklist::SpawnPawn);
    
    UE_LOG(LogSkyInitialization, Log, TEXT("%hs -- spawning default pawn at transform"), __FUNCTION__);
    
    FActorSpawnParameters SpawnInfo;
    SpawnInfo.Instigator = GetInstigator();
    SpawnInfo.ObjectFlags |= RF_Transient; // Never save the default player pawns into a map.
    SpawnInfo.bDeferConstruction = true;

    const auto PawnClass = GetDefaultPawnClassForController(NewPlayer);
    LG_INVARIANT(PawnClass);
    const auto SpawnedPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnInfo);
    LG_INVARIANT(SpawnedPawn);
    const auto PawnExtComp = USkyPawnExtensionComp::FindPawnExtensionComponent(SpawnedPawn)
    LG_INVARIANT(PawnExtComp);
    const auto PawnData = GetPawnDataForController(NewPlayer)
    LG_INVARIANT(PawnData);
    
    UE_LOG(LogSkyInitialization, Log, TEXT("%hs started spawning pawn"), __FUNCTION__);
    
    PawnExtComp->SetPawnData(PawnData);
    UE_LOG(LogSkyInitialization, Log, TEXT("%hs setting PawnData to PawnExtComp"), __FUNCTION__);

    SpawnedPawn->FinishSpawning(SpawnTransform);
    UE_LOG(LogSkyInitialization, Log, TEXT("%hs finished spawning pawn"), __FUNCTION__);

    return SpawnedPawn;
}
```

### Performance

On a 10-year old Intel i7, the average time for a full class invariants check on an object with 75 invariants is 0.000024s, or 24 microseconds. All perf tests are included. Remember that they do nothing on shipping builds, so the cost is 0.

## Checklists

Checklists are our way to ensure complex systems are initialized in order. Checklists are good and simple, and one may argue they're good because they're simple. Checklists allows us to define the steps needed to complete some action, and if any step is wrong or our of order, we crash.

Checklists are a battle tested method used in life-critical domains such as aviation and high speed railways, they're a fairly secure method to ensure required steps are done. This system takes away cognitive load from the user by automatically ensuring steps are done in order. There's no more "I forgot to set the pawn data before spawning it" because now if you have a checklist, the game will crash and tell you exactly what you forgot to do.

### Examples

Checklists are defined via structs.

```cpp
struct FLoadWorldChecklist
{
    static constexpr const TCHAR* ChecklistName = TEXT("LoadWorld");

    // Individual steps
    static constexpr const TCHAR* LoadWorldJson = TEXT("load-world-json");
    static constexpr const TCHAR* InitializeStreamingVolumes = TEXT("initialize-streaming-volumes");
    static constexpr const TCHAR* RegisterWorldEvents = TEXT("register-world-events");

    // Ordered list of steps referencing the symbols above
    static constexpr std::array<const TCHAR*, 3> Steps = {
        LoadWorldJson,
        InitializeStreamingVolumes,
        RegisterWorldEvents
    };
};
```

They're registered on module startup and cleaned up on shutdown.

```cpp
void FSkyDogmaCoreModule::StartupModule()
{
    FLifeChecklistRegistry::Get().Register<FDogmaGameModeChecklist>();
    FLifeChecklistRegistry::Get().Register<FDogmaSpawnPawnChecklist>();
    FLifeChecklistRegistry::Get().Register<FDogmaPopulateWorldChecklist>();
    FLifeChecklistRegistry::Get().Register<FDogmaInitEnginesChecklist>();
    FLifeChecklistRegistry::Get().Register<FDogmaAtlasInitChecklist>();
    FLifeChecklistRegistry::Get().Register<FDogmaAtlasOrbsInitChecklist>();
    FLifeChecklistRegistry::Get().Register<FDogmaDomiInitChecklist>();
    FLifeChecklistRegistry::Get().Register<FDogmaDomiCharaInitChecklist>();
    FLifeChecklistRegistry::Get().Register<FDogmaMercatorInitChecklist>();
    FLifeChecklistRegistry::Get().Register<FDogmaPulseInitChecklist>();
    FLifeChecklistRegistry::Get().Register<FDogmaMasterInitChecklist>();
}
```

```cpp
void FSkyDogmaCoreModule::ShutdownModule()
{
    FLifeChecklistRegistry::Get().ResetAllForPie();
}
```

Using them is simple. Reset them and use them inside scopes. The checklist will check that all previous steps have been completed, if not the game will crash telling you the last completed step and the attempted step. When the scope exists, we mark the checklist step done bya RAII. If it's the last step, the checklist is marked as done.

If you need to use a checklist more than once, reset it.

```cpp

namespace
{
	constexpr std::array Checklists = {
		FDogmaGameModeChecklist::ChecklistName,
		FDogmaInitEnginesChecklist::ChecklistName,
		FDogmaAtlasInitChecklist::ChecklistName,
		FDogmaAtlasOrbsInitChecklist::ChecklistName,
		FDogmaDomiInitChecklist::ChecklistName,
		FDogmaMercatorInitChecklist::ChecklistName,
		FDogmaPulseInitChecklist::ChecklistName,
		FDogmaMasterInitChecklist::ChecklistName,
		FDogmaSpawnPawnChecklist::ChecklistName,
		FDogmaPopulateWorldChecklist::ChecklistName
	};
}

// reset checklists
for (const auto& Checklist : Checklists) {
    LG_RESET_CHECKLIST(Checklist);
}

// use scopes

{
    LG_SCOPED_CHECKLIST_STEP(FDogmaInitEnginesChecklist::ChecklistName, FDogmaInitEnginesChecklist::InitAtlas);
    
    const auto AtlasInitData = WorldTemplate->AtlasInitData.LoadSynchronous();
    const auto AtlasInitializer = NewObject<UAtlasInitializer>();
    AtlasInitializer->SetResources(AtlasInitData, this);
    AtlasInitializer->InitializeAtlas();
}

{
    LG_SCOPED_CHECKLIST_STEP(FDogmaInitEnginesChecklist::ChecklistName, FDogmaInitEnginesChecklist::InitDomi);
    
    const auto DomiSys = GameInstance->GetSubsystem<UDomiSys>();
    LG_INVARIANT(DomiSys)
    
    const auto DomiInitData = WorldTemplate->DomiInitData.LoadSynchronous();
    const auto DomiInitializer = NewObject<UDomiInitializer>();
    DomiInitializer->InjectDependencies(DomiSys, WdCoreSys, World);
    DomiInitializer->InjectInitData(DomiInitData);
    DomiInitializer->InitializeDomi();
}

```

## Domain Checks

Floodlight is the Domain Error version of Contracts. The rationale is that while contract errors are programmer errors and we force a crash, domain errors are not programmer errors but domain-user errors.

The philosophy is the same, turning hidden bugs into magnesium flares. We want to have immediate action, we want the system to be loud, bright and to demand attention. It's not a single UE_LOG line sent to the output log that the developer may not even notice if she's playing in fullscreen. These are magnesium flares lighting up the night sky.

Logs are the most ignorable errors ever. On the other hand, the Floodlight philosophy is STOP DOING WHAT YOU'RE DOING -- ERROR FOUND

Floodlight takes on a three-pronged attack to Domain errors without forcing crashes, namely:

- Error budget. Floodlight gives devs a configurable amount of points, 15 by default. A Warning is worth 1 point, while an Error is worth 3 (both configurable too). When the error budget reaches the limit, the game crashes.
- Screen Flashes and lists. When errors/warnings are logged, there's an immediate screen flash that makes it almost impossible to ignore, and then there's a permanent overlay on top with the errors list.
- UE_LOG interception. We can configure intercepted log categories to send all logs with a certain verbosity level to Floodlight, automatically.
- A TODO("msg") macro that emits a runtime UE_LOG warning with function/file/line. Use with a string literal: TODO("Finish implementing") or simply TODO_FN which logs a warning like this "UFooClass::BarFun"

In order to intercept logs, on your module startup add

```cpp
FLifeDomainErrorFloodlight::RegisterInterceptCategory("LogSkybloom");
```

You also need to have some sort of HUD setup to draw the overlay. For example:

```cpp
void ASkyHud::DrawHUD()
{
	Super::DrawHUD();
	
	FLifeDomainErrorFloodlight::DrawOverlay(Canvas);
}
```

## Open Issues

### The domain check slate overlay is ugly
The point was to make an overlay that you simply couldn't miss. However, it would be nice if it could be made prettier.

### Class Invariant Arrays
While we support containers for pointers and pointer-like types we don't support for example

```cpp
UPROPERTY(meta=(Invariant="Range[0,1]))
TArray<float> Probabilities;
```

### Invariant=Contract* Loop Guard
We should probably add a loop guard to prevent stack overflows in the case of cyclic checks.

### Floodlight Macros Naming Consistency
The macros in the Floodlight module have quite inconsistent names. For example `LG_DOMAIN_CHECKF` vs `LG_DOMAIN_CHECK_RET_VOID_MSG`. It would be nice to have more consistent naming.


## Legal Disclaimer

This is an independent project and is not affiliated with, sponsored by, or endorsed by Epic Games, Inc.

This software is provided "as is", without warranty of any kind, express or implied. Use of this framework in a production environment is at the developer's own risk.