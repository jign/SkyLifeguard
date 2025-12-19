#include "LifeFloodlight.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

// Static member initialization
FLifeDomainErrorFloodlight::FConfig FLifeDomainErrorFloodlight::Config;
TArray<FLifeDomainError> FLifeDomainErrorFloodlight::ActiveErrors;
int32 FLifeDomainErrorFloodlight::CurrentBudget = 0;
float FLifeDomainErrorFloodlight::FlashTimer = 0.0f;
bool FLifeDomainErrorFloodlight::bInitialized = false;
TUniquePtr<FLifeDomainErrorOutputDevice> FLifeDomainErrorFloodlight::OutputDevice = nullptr;
FString FLifeScopedDomainErrorContext::CurrentContext;
TOptional<ELifeDomainErrorSeverity> FLifeDomainErrorFloodlight::TestFlashSeverity;

// Console command instances (persist for lifetime)
static TUniquePtr<FAutoConsoleCommand> GLifeFloodlightCmd_ClearAll;
static TUniquePtr<FAutoConsoleCommand> GLifeFloodlightCmd_TestFlash;
static TUniquePtr<FAutoConsoleCommand> GLifeFloodlightCmd_Acknowledge;
static TUniquePtr<FAutoConsoleCommand> GLifeFloodlightCmd_EmitError;

FLifeDomainErrorOutputDevice::FLifeDomainErrorOutputDevice()
{
	// Add to GLog's output device chain
	GLog->AddOutputDevice(this);
}

FLifeDomainErrorOutputDevice::~FLifeDomainErrorOutputDevice()
{
	// Remove from GLog's output device chain
	GLog->RemoveOutputDevice(this);
}

void FLifeDomainErrorOutputDevice::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category)
{
	// Only intercept if this category is registered
	if (!InterceptedCategories.Contains(Category)) {
		return;
	}
    
	ProcessInterceptedLog(V, Verbosity, Category);
}

void FLifeDomainErrorOutputDevice::ProcessInterceptedLog(const FString& Message, ELogVerbosity::Type Verbosity,
	const FName& Category)
{
	// Convert log verbosity to domain error severity
	ELifeDomainErrorSeverity Severity;
    
	switch (Verbosity)
	{
	case ELogVerbosity::Warning:
		Severity = ELifeDomainErrorSeverity::Warning;
		break;
	case ELogVerbosity::Error:
		Severity = ELifeDomainErrorSeverity::Error;
		break;
	case ELogVerbosity::Fatal:
		Severity = ELifeDomainErrorSeverity::Error;
		break;
	default:
		return; // Don't intercept other verbosities
	}
    
	FString Context = FString::Printf(TEXT("Log Category: %s"), *Category.ToString());
	FLifeDomainErrorFloodlight::ReportInternal(Message, Context, Severity);
}

void FLifeDomainErrorFloodlight::Initialize(const FConfig& InConfig)
{
	if (bInitialized) {
		UE_LOG(LogTemp, Warning, TEXT("FDomainErrorFloodlight already initialized"));
		return;
	}
    
	Config = InConfig;
	CurrentBudget = 0;
	FlashTimer = 0.0f;
	ActiveErrors.Empty();
	TestFlashSeverity.Reset();
    
	// Create output device
	OutputDevice = MakeUnique<FLifeDomainErrorOutputDevice>();
	
	// Register console commands
	GLifeFloodlightCmd_ClearAll = MakeUnique<FAutoConsoleCommand>(
		TEXT("Floodlight.ClearAllErrors"),
		TEXT("Clears all domain errors"),
		FConsoleCommandDelegate::CreateStatic(&FLifeDomainErrorFloodlight::Console_ClearAll)
	);

	GLifeFloodlightCmd_TestFlash = MakeUnique<FAutoConsoleCommand>(
		TEXT("Floodlight.TestFlash"),
		TEXT("Triggers a test flash. Usage: Floodlight.TestFlash warning|error|critical"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&FLifeDomainErrorFloodlight::Console_TestFlash)
	);

	GLifeFloodlightCmd_Acknowledge = MakeUnique<FAutoConsoleCommand>(
		TEXT("Floodlight.AcknowledgeError"),
		TEXT("Acknowledges an active error by index. Usage: Floodlight.AcknowledgeError <index>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&FLifeDomainErrorFloodlight::Console_Acknowledge)
	);
	
	GLifeFloodlightCmd_EmitError = MakeUnique<FAutoConsoleCommand>(
		TEXT("Floodlight.EmitError"),
		TEXT("Emits one or more real domain warnings/errors (affects budget). Usage: Floodlight.EmitError warning|error [count] [message...]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&FLifeDomainErrorFloodlight::Console_EmitError)
	);
    
	bInitialized = true;
    
	UE_LOG(LogTemp, Log, TEXT("FDomainErrorFloodlight initialized with budget: %d"), Config.MaxBudget);

}

void FLifeDomainErrorFloodlight::Shutdown()
{
	if (!bInitialized) {
		return;
	}
    
	OutputDevice.Reset();
	ActiveErrors.Empty();
	TestFlashSeverity.Reset();
	
	// Tear down console commands
	GLifeFloodlightCmd_ClearAll.Reset();
	GLifeFloodlightCmd_TestFlash.Reset();
	GLifeFloodlightCmd_Acknowledge.Reset();
	GLifeFloodlightCmd_EmitError.Reset();
	
	bInitialized = false;
    
	UE_LOG(LogTemp, Log, TEXT("FDomainErrorFloodlight shut down"));
}

void FLifeDomainErrorFloodlight::ReportWarning(const FString& Message, const FString& Context)
{
	ReportInternal(Message, Context, ELifeDomainErrorSeverity::Warning);
}

void FLifeDomainErrorFloodlight::ReportError(const FString& Message, const FString& Context)
{
	ReportInternal(Message, Context, ELifeDomainErrorSeverity::Error);
}

void FLifeDomainErrorFloodlight::ReportCritical(const FString& Message, const FString& Context)
{
	ReportInternal(Message, Context, ELifeDomainErrorSeverity::Critical);
}

void FLifeDomainErrorFloodlight::ClearAllErrors()
{
	ActiveErrors.Empty();
	FlashTimer = 0.0f;
	TestFlashSeverity.Reset();
    
	if (GEngine && GEngine->GameViewport) {
		// TODO if any overlay on the viewport, clear it
	}
    
	UE_LOG(LogTemp, Log, TEXT("All domain errors cleared"));
}

void FLifeDomainErrorFloodlight::AcknowledgeError(int32 Index)
{
	if (ActiveErrors.IsValidIndex(Index))
	{
		ActiveErrors.RemoveAt(Index);
	}
}

void FLifeDomainErrorFloodlight::Console_ClearAll()
{
	ClearAllErrors();
	UE_LOG(LogTemp, Log, TEXT("Floodlight.ClearAllErrors executed"));
}

void FLifeDomainErrorFloodlight::Console_TestFlash(const TArray<FString>& Args)
{
	if (!bInitialized) {
		UE_LOG(LogTemp, Warning, TEXT("LifeFloodlight not initialized"));
		return;
	}

	if (Args.Num() == 0) {
		UE_LOG(LogTemp, Warning, TEXT("Usage: Floodlight.TestFlash warning|error|critical"));
		return;
	}

	FString Arg = Args[0].ToLower();
	ELifeDomainErrorSeverity Severity = ELifeDomainErrorSeverity::Warning;
	if (Arg == TEXT("warning"))
	{
		Severity = ELifeDomainErrorSeverity::Warning;
	}
	else if (Arg == TEXT("error"))
	{
		Severity  = ELifeDomainErrorSeverity::Error;
	}
	else if (Arg == TEXT("critical"))
	{
		// Do not actually invoke the critical fatal path; just simulate the visual flash
		Severity = ELifeDomainErrorSeverity::Critical;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Unknown severity '%s'. Use warning|error|critical"), *Arg);
		return;
	}
	
	TestFlashSeverity = Severity;
	TriggerFlash(Severity);

	// Optionally play alert sound (TriggerFlash + PlayAlertSound are private but accessible here)
	if (Config.bPlaySounds)
	{
		if (Arg == TEXT("warning"))
			PlayAlertSound(ELifeDomainErrorSeverity::Warning);
		else if (Arg == TEXT("error"))
			PlayAlertSound(ELifeDomainErrorSeverity::Error);
		else
			PlayAlertSound(ELifeDomainErrorSeverity::Critical);
	}

	UE_LOG(LogTemp, Log, TEXT("Floodlight.TestFlash %s executed"), *Arg);
}

void FLifeDomainErrorFloodlight::Console_Acknowledge(const TArray<FString>& Args)
{
	if (Args.Num() == 0) {
		UE_LOG(LogTemp, Warning, TEXT("Usage: Floodlight.AcknowledgeError <index>"));
		return;
	}

	int32 Index = FCString::Atoi(*Args[0]);
	if (!ActiveErrors.IsValidIndex(Index)) {
		UE_LOG(LogTemp, Warning, TEXT("Invalid error index: %d"), Index);
		return;
	}

	AcknowledgeError(Index);
	UE_LOG(LogTemp, Log, TEXT("Acknowledged domain error index %d"), Index);
}

void FLifeDomainErrorFloodlight::Console_EmitError(const TArray<FString>& Args)
{
	if (!bInitialized) {
		UE_LOG(LogTemp, Warning, TEXT("LifeFloodlight not initialized"));
		return;
	}

	if (Args.Num() == 0) {
		UE_LOG(LogTemp, Warning, TEXT("Usage: Floodlight.EmitError warning|error [count]"));
		return;
	}

	FString SeverityArg = Args[0].ToLower();
	int32 Count = 1;
	if (Args.Num() >= 2) {
		Count = FMath::Max(1, FCString::Atoi(*Args[1]));
	}
	FString Message = FString::Printf(TEXT("Console-generated %s"), *SeverityArg);


	if (SeverityArg == TEXT("warning"))
	{
		for (int32 i = 0; i < Count; ++i)
		{
			ReportWarning(Message, TEXT("Console.EmitError"));
		}
	}
	else if (SeverityArg == TEXT("error"))
	{
		for (int32 i = 0; i < Count; ++i)
		{
			ReportError(Message, TEXT("Console.EmitError"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Unknown severity '%s'. Use warning|error"), *SeverityArg);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Floodlight.EmitError %s x%d executed: %s"), *SeverityArg, Count, *Message);

}

void FLifeDomainErrorFloodlight::Tick(float DeltaTime)
{
	if (!bInitialized || (ActiveErrors.Num() == 0 && !TestFlashSeverity.IsSet())) {
		return;
	}
    
	// Update flash timer
	if (FlashTimer > 0.0f) {
		FlashTimer -= DeltaTime;
		if (FlashTimer < 0.0f || FMath::IsNearlyZero(FlashTimer, 0.01)) FlashTimer = 0.0f;
		if (FMath::IsNearlyZero(FlashTimer, 0.01) && TestFlashSeverity.IsSet()) {
			TestFlashSeverity.Reset();
		}
	}
}

void FLifeDomainErrorFloodlight::DrawOverlay(UCanvas* Canvas)
{
	#if !UE_BUILD_SHIPPING
    
    if (!bInitialized || !Canvas || (ActiveErrors.Num() == 0 && !TestFlashSeverity.IsSet())) {
        return;
    }

    const float ScreenWidth = Canvas->SizeX;
    const float ScreenHeight = Canvas->SizeY;
	
	// Determine effective severity (test override wins)
	TOptional<ELifeDomainErrorSeverity> EffectiveSeverity = TestFlashSeverity;
	if (!EffectiveSeverity.IsSet() && ActiveErrors.Num() > 0)
	{
		// derive from active errors (most severe)
		ELifeDomainErrorSeverity Found = ELifeDomainErrorSeverity::Warning;
		for (const FLifeDomainError& Error : ActiveErrors) {
			if (Error.Severity == ELifeDomainErrorSeverity::Error) {
				Found = ELifeDomainErrorSeverity::Error;
				break;
			}
			if (Error.Severity == ELifeDomainErrorSeverity::Critical) {
				Found = ELifeDomainErrorSeverity::Critical;
				break;
			}
		}
		EffectiveSeverity = Found;
	}

	// Full-screen flash (draw first so UI is on top)
	if (FlashTimer > 0.0f && EffectiveSeverity.IsSet())
	{
		// Determine color based on effective severity
		FLinearColor FlashColor = FLinearColor::Black;
		switch (EffectiveSeverity.GetValue())
		{
		case ELifeDomainErrorSeverity::Warning: FlashColor = FLinearColor::Yellow; break;
		case ELifeDomainErrorSeverity::Error: FlashColor = FLinearColor::Red; break;
		case ELifeDomainErrorSeverity::Critical: FlashColor = FLinearColor(1.0f, 0.0f, 1.0f); break;
		default: FlashColor = FLinearColor::Black; break;
		}

		// Oscillate alpha so the flash "pulses"
		float Osc = FMath::Sin((Config.FlashFrequency * (Config.FlashDuration - FlashTimer)) * 2.0f * PI);
		float Alpha = FMath::Clamp(0.25f + FMath::Abs(Osc) * 0.35f, 0.0f, 0.85f);
		FlashColor.A = Alpha;

		FCanvasTileItem FullScreenTile(FVector2D(0.0f, 0.0f),
									   FVector2D(ScreenWidth, ScreenHeight),
									   FlashColor);
		FullScreenTile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(FullScreenTile);
	}
    
	// Now we need to check if there's any real errors, previous part included flash testing functionality
	if (ActiveErrors.Num() == 0) 
		return;
	
    // Draw budget bar at top
    {
        const float BudgetBarHeight = 60.0f;
        const float BudgetBarY = 10.0f;
        
        // Background
        FCanvasTileItem BackgroundTile(
            FVector2D(10.0f, BudgetBarY),
            FVector2D(ScreenWidth - 20.0f, BudgetBarHeight),
            FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
        BackgroundTile.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(BackgroundTile);
        
        // Budget fill
        float BudgetPercent = FMath::Clamp((float)CurrentBudget / (float)Config.MaxBudget, 0.0f, 1.0f);
        FLinearColor BudgetColor = FLinearColor::LerpUsingHSV(
            FLinearColor::Green, 
            FLinearColor::Red, 
            BudgetPercent);
        
        FCanvasTileItem BudgetFill(
            FVector2D(15.0f, BudgetBarY + 5.0f),
            FVector2D((ScreenWidth - 30.0f) * BudgetPercent, BudgetBarHeight - 10.0f),
            BudgetColor);
        BudgetFill.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(BudgetFill);
        
        // Text
        FString BudgetText = FString::Printf(TEXT("⚠ ERROR BUDGET: %d/%d ⚠"), CurrentBudget, Config.MaxBudget);
        Canvas->SetDrawColor(FColor::White);
        float TextWidth, TextHeight;
        Canvas->TextSize(GEngine->GetLargeFont(), BudgetText, TextWidth, TextHeight);
        Canvas->DrawText(GEngine->GetLargeFont(), BudgetText, 
            (ScreenWidth - TextWidth) * 0.5f, BudgetBarY + 15.0f);
    }
    
    // Draw error list
    {
        const float ListY = 80.0f;
        const float ListHeight = FMath::Min(400.0f, ScreenHeight - 200.0f);
        const float ItemHeight = 80.0f;
        
        // Background
        FCanvasTileItem ListBackground(
            FVector2D(10.0f, ListY),
            FVector2D(ScreenWidth - 20.0f, ListHeight),
            FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
        ListBackground.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(ListBackground);
        
        // Errors
        float CurrentY = ListY + 10.0f;
        int32 MaxVisible = FMath::FloorToInt((ListHeight - 20.0f) / ItemHeight);
        int32 DisplayCount = FMath::Min(ActiveErrors.Num(), MaxVisible);
        
        for (int32 i = 0; i < DisplayCount; ++i)
        {
            const FLifeDomainError& Error = ActiveErrors[i];
            
            // Severity badge
            FLinearColor SeverityColor = Error.GetSeverityColor();
            FCanvasTileItem SeverityBadge(
                FVector2D(20.0f, CurrentY),
                FVector2D(10.0f, ItemHeight - 10.0f),
                SeverityColor);
            Canvas->DrawItem(SeverityBadge);
            
            // Error text
            Canvas->SetDrawColor(FColor::White);
            FString ErrorText = FString::Printf(TEXT("[%s] %s"), 
                *Error.GetSeverityString(), 
                *Error.Message);
            
            if (Error.OccurrenceCount > 1)
            {
                ErrorText += FString::Printf(TEXT(" (x%d)"), Error.OccurrenceCount);
            }
            
            Canvas->DrawText(GEngine->GetMediumFont(), ErrorText, 40.0f, CurrentY + 5.0f);
            
            // Context (smaller)
            Canvas->SetDrawColor(FColor(200, 200, 200));
            Canvas->DrawText(GEngine->GetSmallFont(), Error.Context, 40.0f, CurrentY + 30.0f);
            
            // Timestamp
            FString TimeStr = Error.Timestamp.ToString(TEXT("%H:%M:%S"));
            Canvas->DrawText(GEngine->GetSmallFont(), TimeStr, 40.0f, CurrentY + 50.0f);
            
            CurrentY += ItemHeight;
        }
        
        // Show "... and N more" if truncated
        if (ActiveErrors.Num() > MaxVisible)
        {
            Canvas->SetDrawColor(FColor::Yellow);
            FString MoreText = FString::Printf(TEXT("... and %d more errors"), 
                ActiveErrors.Num() - MaxVisible);
            Canvas->DrawText(GEngine->GetMediumFont(), MoreText, 20.0f, CurrentY);
        }
    }
    
    // Instructions
    {
        Canvas->SetDrawColor(FColor::White);
        FString Instructions = TEXT("Press F9 to clear errors | F10 to reset budget");
        Canvas->DrawText(GEngine->GetSmallFont(), Instructions, 20.0f, ScreenHeight - 40.0f);
    }
    
    #endif
}

void FLifeDomainErrorFloodlight::RegisterInterceptCategory(const FName& Category)
{
	if (OutputDevice)
	{
		OutputDevice->InterceptedCategories.Add(Category);
		UE_LOG(LogTemp, Log, TEXT("Registered domain error interception for category: %s"), *Category.ToString());
	}
}

void FLifeDomainErrorFloodlight::UnregisterInterceptCategory(const FName& Category)
{
	if (OutputDevice)
	{
		OutputDevice->InterceptedCategories.Remove(Category);
	}
}

void FLifeDomainErrorFloodlight::ReportInternal(const FString& Message, const FString& Context,
	ELifeDomainErrorSeverity Severity)
{
	#if !UE_BUILD_SHIPPING
    
    if (!bInitialized)
    {
        // Fallback to regular logging if not initialized
        UE_LOG(LogTemp, Error, TEXT("Domain Error (System Not Initialized): %s"), *Message);
        return;
    }
    
    // Critical errors bypass the budget system and crash immediately
    if (Severity == ELifeDomainErrorSeverity::Critical)
    {
        UE_LOG(LogTemp, Fatal, TEXT("CRITICAL DOMAIN ERROR: %s\nContext: %s"), *Message, *Context);
        checkf(false, TEXT("Critical Domain Error: %s"), *Message);
        return;
    }
    
    // Check for duplicate error (increment count instead of adding new)
    for (FLifeDomainError& Error : ActiveErrors)
    {
        if (Error.Message == Message && Error.Severity == Severity)
        {
            Error.OccurrenceCount++;
            Error.Timestamp = FDateTime::Now();
            
            // Still consume budget for repeated errors
            int32 Cost = (Severity == ELifeDomainErrorSeverity::Warning) ? Config.WarningCost : Config.ErrorCost;
            ConsumeBudget(Cost);
            
            return;
        }
    }
    
    // Add new error
    FLifeDomainError NewError(Message, Context, Severity);
    ActiveErrors.Add(NewError);
    
    // Log to output
    ELogVerbosity::Type LogVerbosity = (Severity == ELifeDomainErrorSeverity::Warning) 
        ? ELogVerbosity::Warning 
        : ELogVerbosity::Error;
    
    UE_LOG(LogTemp, Error, TEXT("[DOMAIN %s] %s\n  Context: %s\n  Budget: %d/%d"), 
        *NewError.GetSeverityString(), 
        *Message, 
        *Context,
        CurrentBudget + ((Severity == ELifeDomainErrorSeverity::Warning) ? Config.WarningCost : Config.ErrorCost),
        Config.MaxBudget);
    
    // Trigger visual feedback
    TriggerFlash(Severity);
    
    // Play sound
    if (Config.bPlaySounds)
    {
        PlayAlertSound(Severity);
    }
    
    // Consume budget
    int32 Cost = (Severity == ELifeDomainErrorSeverity::Warning) ? Config.WarningCost : Config.ErrorCost;
    ConsumeBudget(Cost);
    
    // Pause game if configured
    if (Config.bPauseOnError && Severity == ELifeDomainErrorSeverity::Error)
    {
        if (UWorld* World = GEngine->GetWorldFromContextObject(GEngine, EGetWorldErrorMode::ReturnNull))
        {
            UGameplayStatics::SetGamePaused(World, true);
        }
    }
    
    #endif

}

void FLifeDomainErrorFloodlight::ConsumeBudget(int32 Amount)
{
	CurrentBudget += Amount;
    
	if (CurrentBudget >= Config.MaxBudget)
	{
		// Budget exhausted - crash
		UE_LOG(LogTemp, Fatal, TEXT("DOMAIN ERROR BUDGET EXHAUSTED (%d/%d)"), CurrentBudget, Config.MaxBudget);
		checkf(false, TEXT("Domain Error Budget Exhausted! Too many domain errors (%d/%d). Fix your content/configuration!"), 
			CurrentBudget, Config.MaxBudget);
	}
}

void FLifeDomainErrorFloodlight::TriggerFlash(ELifeDomainErrorSeverity Severity)
{
	// Warnings don't flash (or flash very briefly)
	FlashTimer = (Severity == ELifeDomainErrorSeverity::Warning) ? 0.5f : Config.FlashDuration;
}

void FLifeDomainErrorFloodlight::PlayAlertSound(ELifeDomainErrorSeverity Severity)
{
	// TODO: Implement sound playback
	// UGameplayStatics::PlaySound2D(...);
}
