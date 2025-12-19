#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDevice.h"

/**
 * Floodlight is the Domain Error version of Contracts. The rationale is that while contract errors are programmer
 * errors and we force a crash, domain errors are not programmer errors but domain-user errors, and forcing a crash
 * may not be what we want.
 *
 * The philosophy is the same, turning bugs into magnesium flares. We want to have immediate action, we want the
 * system to be loud, bright and to demand attention. It's not a single UE_LOG line sent to the output log that the
 * developer may not even notice if she's playing in fullscreen. These are magnesium flares lighting up the night sky.
 * 
 * Logs are the most ignorable errors ever. This is: STOP DOING WHAT YOU'RE DOING -- ERROR FOUND
 * 
 * Floodlight takes on a three-pronged attack to Domain errors without forcing crashes, namely:
 * 
 * - Error budget. Floodlight gives devs a configurable amount of points, 15 by default. A Warning is worth 1 point,
 *			while an Error is worth 3 (both configurable too). When the error budget reaches the limit, the game
 *			crashes.
 * - Screen Flashes and lists. When errors/warnings are logged, there's an immediate screen flash that makes it almost
 *			impossible to ignore, and then there's a permanent overlay on top with the errors list.
 * - UE_LOG interception. We can configure intercepted log categories to send all logs with a certain verbosity level
 *			to Floodlight, automatically.
 */

/**
 * Domain Error Severity Levels
 */
enum class ELifeDomainErrorSeverity : uint8
{
	Warning,    // Yellow flash, 1 budget point
	Error,      // Red flash, 3 budget points
	Critical    // Immediate crash (bypasses budget)
};

/**
 * Represents a single domain error occurrence
 */
struct FLifeDomainError
{
	FLifeDomainError() = default;
    
	FLifeDomainError(const FString& InMessage, const FString& InContext, ELifeDomainErrorSeverity InSeverity)
		: Message(InMessage)
		, Context(InContext)
		, Timestamp(FDateTime::Now())
		, Severity(InSeverity)
	{}
    
	FString GetSeverityString() const
	{
		switch (Severity)
		{
		case ELifeDomainErrorSeverity::Warning: return TEXT("WARNING");
		case ELifeDomainErrorSeverity::Error: return TEXT("ERROR");
		case ELifeDomainErrorSeverity::Critical: return TEXT("CRITICAL");
		default: return TEXT("UNKNOWN");
		}
	}
    
	FLinearColor GetSeverityColor() const
	{
		switch (Severity)
		{
		case ELifeDomainErrorSeverity::Warning: return FLinearColor::Yellow;
		case ELifeDomainErrorSeverity::Error: return FLinearColor::Red;
		case ELifeDomainErrorSeverity::Critical: return FLinearColor(1.0f, 0.0f, 1.0f);
		default: return FLinearColor::White;
		}
	}
	
	FString Message;
	FString Context;        // Function, file, line
	FDateTime Timestamp;
	ELifeDomainErrorSeverity Severity;
	int32 OccurrenceCount = 1;
};

/**
 * Custom Output Device that intercepts logs and turns them into domain errors
 */
class FLifeDomainErrorOutputDevice : public FOutputDevice
{
	friend class FLifeDomainErrorFloodlight;
public:
	FLifeDomainErrorOutputDevice();
	virtual ~FLifeDomainErrorOutputDevice() override;
    
	// FOutputDevice interface
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override;
    
private:
	// Categories to intercept
	TSet<FName> InterceptedCategories;
    
	void ProcessInterceptedLog(const FString& Message, ELogVerbosity::Type Verbosity, const FName& Category);
};

/**
 * Main Domain Error Floodlight System
 * Manages error budget, visual feedback, and error tracking
 */
class SKYLIFEGUARD_API FLifeDomainErrorFloodlight
{
public:
    // Configuration
    struct FConfig
    {
        int32 MaxBudget = 10;           // Total budget before crash
        int32 WarningCost = 1;          // Budget points per warning
        int32 ErrorCost = 3;            // Budget points per error
        float FlashDuration = 2.0f;     // How long the screen flashes (seconds)
        float FlashFrequency = 8.0f;    // Flash oscillation frequency
        bool bPauseOnError = false;     // Whether to pause game on errors
        bool bPlaySounds = true;        // Whether to play alert sounds
    };
    
    // Initialization
    static void Initialize(const FConfig& InConfig = FConfig());
    static void Shutdown();
    
    // Error Reporting
    static void ReportWarning(const FString& Message, const FString& Context = TEXT(""));
    static void ReportError(const FString& Message, const FString& Context = TEXT(""));
    static void ReportCritical(const FString& Message, const FString& Context = TEXT(""));
    
    // Manual control
    static void ClearAllErrors();
    static void AcknowledgeError(int32 Index);
	
	// Console command handlers (used by FAutoConsoleCommand)
	static void Console_ClearAll();
	static void Console_TestFlash(const TArray<FString>& Args);
	static void Console_Acknowledge(const TArray<FString>& Args);
	// Emits real domain warnings/errors (affect budget). Usage: Floodlight.EmitError warning|error [count] [message...]
	static void Console_EmitError(const TArray<FString>& Args);
    
    // Getters
    static int32 GetCurrentBudget() { return CurrentBudget; }
    static int32 GetMaxBudget() { return Config.MaxBudget; }
    static const TArray<FLifeDomainError>& GetActiveErrors() { return ActiveErrors; }
    static bool HasActiveErrors() { return ActiveErrors.Num() > 0; }
    
    // Tick (call from game viewport client or HUD)
    static void Tick(float DeltaTime);
    
    // Drawing (call from HUD or debug canvas)
    static void DrawOverlay(UCanvas* Canvas);
    
    // Output device integration
    static void RegisterInterceptCategory(const FName& Category);
    static void UnregisterInterceptCategory(const FName& Category);
    
private:
    static FConfig Config;
    static TArray<FLifeDomainError> ActiveErrors;
    static int32 CurrentBudget;
    static float FlashTimer;
    static bool bInitialized;
    static TUniquePtr<FLifeDomainErrorOutputDevice> OutputDevice;
    
    static void ReportInternal(const FString& Message, const FString& Context, ELifeDomainErrorSeverity Severity);
    static void ConsumeBudget(int32 Amount);
    static void TriggerFlash(ELifeDomainErrorSeverity Severity);
    static void PlayAlertSound(ELifeDomainErrorSeverity Severity);
    
    friend class FLifeDomainErrorOutputDevice;
	
	// Optional severity override used by console test flash (when set, DrawOverlay will use it
	// even if there are no active errors)
	static TOptional<ELifeDomainErrorSeverity> TestFlashSeverity;
};

#define LG_DOMAIN_WARNING(Format, ...) \
	FLifeDomainErrorFloodlight::ReportWarning(FString::Printf(Format, ##__VA_ARGS__), \
	FString::Printf(TEXT("%hs @ %s:%d"), __FUNCTION__, TEXT(__FILE__), __LINE__))

#define LG_DOMAIN_ERROR(Format, ...) \
	FLifeDomainErrorFloodlight::ReportError(FString::Printf(Format, ##__VA_ARGS__), \
	FString::Printf(TEXT("%hs @ %s:%d"), __FUNCTION__, TEXT(__FILE__), __LINE__))

#define LG_DOMAIN_CRITICAL(Format, ...) \
	FLifeDomainErrorFloodlight::ReportCritical(FString::Printf(Format, ##__VA_ARGS__), \
	FString::Printf(TEXT("%hs @ %s:%d"), __FUNCTION__, TEXT(__FILE__), __LINE__))


/**
 * Checks a condition. If it fails (is false), reports a domain error and executes the following code block.
 * Severity must be one of: Warning, Error, Critical.
 * 
 * Usage:
 * DOMAIN_CHECK(bIsValid, Error)
 * {
 *     return; // Recovery code
 * }
 */
#define LG_DOMAIN_CHK_FAILDO(Condition, Severity) \
	if (bool bLifeCheckPassed = !!(Condition); bLifeCheckPassed) {} else \
	if (([&](){ \
		FLifeDomainErrorFloodlight::Report##Severity(TEXT("Check failed: ") TEXT(#Condition), \
		FString::Printf(TEXT("%hs @ %s:%d"), __FUNCTION__, TEXT(__FILE__), __LINE__)); \
		return true; \
	})())

/**
 * Checks a condition with a custom message. If it fails, reports a domain error and executes the following code block.
 * 
 * Usage:
 * LG_DOMAIN_CHECKF(bIsValid, Error, TEXT("Object %s is invalid"), *GetName())
 * {
 *     return; // Recovery code
 * }
 */
#define LG_DOMAIN_CHECKF(Condition, Severity, Format, ...) \
	if (bool bLifeCheckPassed = !!(Condition); bLifeCheckPassed) {} else \
	if (([&](){ \
		FLifeDomainErrorFloodlight::Report##Severity(FString::Printf(Format, ##__VA_ARGS__), \
		FString::Printf(TEXT("%hs @ %s:%d"), __FUNCTION__, TEXT(__FILE__), __LINE__)); \
		return true; \
	})())

/**
 * Checks a condition. If it fails, reports a domain error and returns (void).
 * Usage: LG_DOMAIN_CHECK_RET_VOID(MyPtr != nullptr, Error);
 */
#define LG_DOMAIN_CHECK_RET_VOID(Condition, Severity) \
	if (bool bLifeCheckPassed = !!(Condition); bLifeCheckPassed) {} else \
	{ \
		FLifeDomainErrorFloodlight::Report##Severity(TEXT("Check failed: ") TEXT(#Condition), \
		FString::Printf(TEXT("%hs @ %s:%d"), __FUNCTION__, TEXT(__FILE__), __LINE__)); \
		return; \
	}

/**
 * Checks a condition. If it fails, reports a domain error and returns (void).
 * Usage: LG_DOMAIN_CHECK_RET_VOID(MyPtr != nullptr, Error);
 */
#define LG_DOMAIN_CHECK_RET_VOID_MSG(Condition, Severity, Message, ...) \
if (bool bLifeCheckPassed = !!(Condition); bLifeCheckPassed) {} else \
	{ \
		FLifeDomainErrorFloodlight::Report##Severity(FString::Printf(Message, ##__VA_ARGS__), \
		FString::Printf(TEXT("%hs @ %s:%d"), __FUNCTION__, TEXT(__FILE__), __LINE__)); \
		return; \
	}

/**
 * Checks a condition. If it fails, reports a domain error and returns the specified value.
 * Usage: LG_DOMAIN_CHECK_RET(MyPtr != nullptr, Error, false);
 */
#define LG_DOMAIN_CHECK_RET(Condition, Severity, ReturnValue) \
	if (bool bLifeCheckPassed = !!(Condition); bLifeCheckPassed) {} else \
	{ \
		FLifeDomainErrorFloodlight::Report##Severity(TEXT("Check failed: ") TEXT(#Condition), \
		FString::Printf(TEXT("%hs @ %s:%d"), __FUNCTION__, TEXT(__FILE__), __LINE__)); \
		return ReturnValue; \
	}

/**
 * Checks a condition with a custom message. If it fails, reports a domain error and returns the specified value.
 * Usage: LG_DOMAIN_CHECK_RETF(MyPtr != nullptr, Error, false, TEXT("Ptr was null"));
 */
#define LG_DOMAIN_CHECK_RETF(Condition, Severity, ReturnValue, Format, ...) \
	if (bool bLifeCheckPassed = !!(Condition); bLifeCheckPassed) {} else \
	{ \
		FLifeDomainErrorFloodlight::Report##Severity(FString::Printf(Format, ##__VA_ARGS__), \
		FString::Printf(TEXT("%hs @ %s:%d"), __FUNCTION__, TEXT(__FILE__), __LINE__)); \
		return ReturnValue; \
	}

// Scoped domain error context (for grouping related errors)
class FLifeScopedDomainErrorContext
{
public:
	explicit FLifeScopedDomainErrorContext(const FString& InContext)
		: Context(InContext)
	{
		PreviousContext = CurrentContext;
		CurrentContext = Context;
	}
    
	~FLifeScopedDomainErrorContext()
	{
		CurrentContext = PreviousContext;
	}
    
	static FString GetCurrentContext() { return CurrentContext; }
    
private:
	FString Context;
	FString PreviousContext;
	static FString CurrentContext;
};

#define LG_DOMAIN_ERROR_CONTEXT(ContextName) \
	FLifeScopedDomainErrorContext ANONYMOUS_VARIABLE(DomainErrorContext)(ContextName)