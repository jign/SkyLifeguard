#pragma once

#include <concepts>
#include <ranges>
#include <type_traits>

#include "LifeLogChannels.h"
#include "LifeContracts.h"

#define VERBOSE_CHECKLISTS 1

/*
 * Checklists are our way to ensure complex systems are initialized in order. Checklists are good and simple, and one
 * may argue they're good because they're simple. Checklists allows us to define the steps needed to complete some
 * action, and if any step is wrong or our of order, we crash.
 * 
 * Checklists are a battle tested method used in life-critical domains such as aviation and high speed railways, they're
 * a fairly secure method to ensure required steps are done. This system takes away cognitive load from the user by
 * automatically ensuring steps are done in order. There's no more "I forgot to set the pawn data before spawning it"
 * because now if you have a checklist, the game will crash and tell you exactly what you forgot to do.
 */


namespace LifeCheck
{
    // Concept: Type must expose:
    //   static constexpr const TCHAR* ChecklistName;
    //   static constexpr Steps range whose value_type convertible to const TCHAR*.
    template<typename T>
    concept HasChecklistDefinition =
        requires {
            { T::ChecklistName } -> std::convertible_to<const TCHAR*>;
            { T::Steps };
        } &&
        std::ranges::range<decltype(T::Steps)> &&
        std::convertible_to<std::ranges::range_value_t<decltype(T::Steps)>, const TCHAR*>;
}

// EXAMPLE CHECKLIST TO USE AS REFERENCE
// struct FLoadWorldChecklist
// {
//     static constexpr const TCHAR* ChecklistName = TEXT("LoadWorld");
//
//     // Individual steps
//     static constexpr const TCHAR* LoadWorldJson            = TEXT("load-world-json");
//     static constexpr const TCHAR* InitializeStreamingVolumes= TEXT("initialize-streaming-volumes");
//     static constexpr const TCHAR* RegisterWorldEvents      = TEXT("register-world-events");
//
//     // Ordered list of steps referencing the symbols above
//     static constexpr std::array<const TCHAR*, 3> Steps = {
//         LoadWorldJson,
//         InitializeStreamingVolumes,
//         RegisterWorldEvents
//     };
// };

/**
 * Checklists provide automated step by step tracking of complex chains where one step must complete before another
 * one does, but we cannot keep the logic contained within one class or function. In cases where such control is
 * possible then using this system is clearly overkill. But sometimes the chain of steps is so intrinsically complicated
 * that we can't see at a glance what needs to be done and in which order. This is particularly true of game init
 * chains that override game engine code and is called in highly indirect ways.
 */
struct FLifeChecklistRegistry
{
	struct FLifeChecklistState
	{
		TArray<FName> Steps;
		int32 LastFinishedStepIndex = INDEX_NONE;
		bool bIsDone = false;
	};

	static FLifeChecklistRegistry& Get()
	{
		static FLifeChecklistRegistry Instance;
		return Instance;
	}

	template<LifeCheck::HasChecklistDefinition Checklist>
	void Register()
	{
        const FName ChecklistFName(Checklist::ChecklistName);
        if (Checklists.Contains(ChecklistFName)) {
            return; // Already registered
        }

        FLifeChecklistState State;
        for (const TCHAR* Step : Checklist::Steps) {
            State.Steps.Add(FName(Step));
        }

#if WITH_EDITOR
        // (Optional) Dev-time uniqueness validation
        {
            TSet<FName> Seen;
            for (const auto& S : State.Steps) {
                if (Seen.Contains(S)) {
                    UE_LOG(LogLife, Fatal, TEXT("Checklist '%s' has duplicate step '%s'"),
                        *ChecklistFName.ToString(), *S.ToString());
                }
                Seen.Add(S);
            }
        }
#endif

        Checklists.Add(ChecklistFName, MoveTemp(State));
	}

    /** Returns true if the given step is the next expected step for the named checklist. */
    bool CanBeginStep(const FName& ChecklistName, const FName& StepName) const
    {
		checkf(Checklists.Contains(ChecklistName), TEXT("Checklist %s not registered"), *ChecklistName.ToString())
		
        const FLifeChecklistState* State = Checklists.Find(ChecklistName);
        
        const int32 ExpectedIndex = State->LastFinishedStepIndex + 1;
        if (ExpectedIndex < 0 || ExpectedIndex >= State->Steps.Num()) {
            return false;
        }
		
        return State->Steps[ExpectedIndex] == StepName;
    }

	/** Returns true if the given step has already been completed for the named checklist. */
    bool IsStepDone(const FName& ChecklistName, const FName& StepName) const
    {
        checkf(Checklists.Contains(ChecklistName), TEXT("Checklist %s not registered"), *ChecklistName.ToString())

        const FLifeChecklistState* State = Checklists.Find(ChecklistName);
        const int32 StepIndex = State->Steps.IndexOfByKey(StepName);
        if (StepIndex == INDEX_NONE) {
            return false;
        }
        // A step is considered done if its index is <= CurrentIndex or if the checklist is done
        return ((State->LastFinishedStepIndex != INDEX_NONE) && (StepIndex <= State->LastFinishedStepIndex)) || State->bIsDone;
    }

	/** Returns true if the named checklist is fully completed. */
    bool IsChecklistDone(const FName& ChecklistName) const
    {
		checkf(Checklists.Find(ChecklistName), TEXT("Checklist %s not registered"), *ChecklistName.ToString())
		
		const auto* const State = Checklists.Find(ChecklistName);
        return State->bIsDone;
    }

    /** Returns the last completed step name, or the strings "not started" / "completed". */
    FString GetLastCompletedStepName(const FName& ChecklistName) const
    {
        checkf(Checklists.Find(ChecklistName), TEXT("Checklist %s not registered"), *ChecklistName.ToString())

        const FLifeChecklistState* State = Checklists.Find(ChecklistName);

        if (State->bIsDone) {
            return FString(TEXT("completed"));
        }

        if (State->LastFinishedStepIndex == INDEX_NONE) {
            return FString(TEXT("not started"));
        }

        // Guard against out-of-range indexes
        if (State->LastFinishedStepIndex >= 0 && State->LastFinishedStepIndex < State->Steps.Num()) {
            return State->Steps[State->LastFinishedStepIndex].ToString();
        }

        return FString(TEXT("invalid"));
    }

	/** Mark the named checklist as done. */
    void SetChecklistDone(const FName& ChecklistName)
    {
		LG_PRECOND(Checklists.Find(ChecklistName))
		
        FLifeChecklistState* State = Checklists.Find(ChecklistName);
        State->bIsDone = true;

#if VERBOSE_CHECKLISTS
		UE_LOG(LogLife, Log, TEXT("Checklist %s done"), *ChecklistName.ToString());
#endif
    }

	void CheckStep(const FName& ChecklistName, const FName& StepName)
	{
		auto& State = Checklists[ChecklistName];
		const int32 ExpectedIndex = State.LastFinishedStepIndex + 1;
		if (State.Steps[ExpectedIndex] != StepName) {
			UE_LOG(LogLife, Fatal, TEXT("Checklist %s: expected '%s' but got '%s'"),
				*ChecklistName.ToString(),
				*State.Steps[ExpectedIndex].ToString(),
				*StepName.ToString());
		}
		State.LastFinishedStepIndex++;

#if VERBOSE_CHECKLISTS
		UE_LOG(LogLife, Log, TEXT("Checklist %s advanced to step %s [%2d/%2d]"), *ChecklistName.ToString(), *StepName.ToString(), State.LastFinishedStepIndex, State.Steps.Num());
#endif

		// If we just completed the last step, mark checklist done.
        if (State.LastFinishedStepIndex == State.Steps.Num() - 1) {
            SetChecklistDone(ChecklistName);
        }
	}

	/** Reset a single checklist (clears progress). Useful when starting a new PIE session. */
	void ResetChecklist(const FName& ChecklistName)
	{
		checkf(Checklists.Contains(ChecklistName), TEXT("Checklist %s not registered"), *ChecklistName.ToString())

		FLifeChecklistState* State = Checklists.Find(ChecklistName);
		State->LastFinishedStepIndex = INDEX_NONE;
		State->bIsDone = false;
	}

	/** Reset all registered checklists (intended for PIE session reset). */
	void ResetAllForPie()
	{
		for (auto& Pair : Checklists) {
			Pair.Value.LastFinishedStepIndex = INDEX_NONE;
			Pair.Value.bIsDone = false;
		}
	}
	
private:
	TMap<FName, FLifeChecklistState> Checklists;
};

/**
 * RAII scope to check that we can begin a step and then it checks it automatically when it goes out of scope.
 */
struct FLifeChecklistScope
{
	FLifeChecklistScope(const FName& InChecklistName, const FName& InStepName)
		: ChecklistName(InChecklistName), StepName(InStepName)
	{
		checkf(FLifeChecklistRegistry::Get().CanBeginStep(ChecklistName, StepName), 
			TEXT("Checklist %s: cannot begin step %s - checklist is at step [%s]"), 
			*ChecklistName.ToString(), *StepName.ToString(),
			*FLifeChecklistRegistry::Get().GetLastCompletedStepName(ChecklistName))
	}

	~FLifeChecklistScope()
	{
		FLifeChecklistRegistry::Get().CheckStep(ChecklistName, StepName);
	}
	
	FName ChecklistName;
	FName StepName;
};

// Internal helpers to enforce acceptable arguments for LG_SCOPED_CHECKLIST_STEP.
// Accept either:
//   1) FName lvalue variables (rejects temporaries like FName("foo"))
//   2) const TCHAR* (e.g. static constexpr checklist / step symbols)
// Intentionally no overload for FName const& so rvalues are blocked.
inline void LifeRequireChecklistArg(FName&){}

inline void LifeRequireChecklistArg(const TCHAR*) {}

// Normalize to FName
inline FName LifeChecklistToFName(FName& Name) { return Name; }
inline FName LifeChecklistToFName(const TCHAR* Str) { return FName(Str); }

// Usage:
//   FName LoadWorldChecklistName(FLoadWorldChecklist::ChecklistName);
//   FName StepName(FLoadWorldChecklist::LoadWorldJson);
//   LG_SCOPED_CHECKLIST_STEP(LoadWorldChecklistName, StepName);
//
// Or directly with static constexpr definitions:
//   LG_SCOPED_CHECKLIST_STEP(FLoadWorldChecklist::ChecklistName, FLoadWorldChecklist::LoadWorldJson);
//
// NOTE: Passing temporaries like FName("foo") will fail to compile.
#define LIFE_CHECK_CONCAT_IMPL(a,b) a##b
#define LIFE_CHECK_CONCAT(a,b) LIFE_CHECK_CONCAT_IMPL(a,b)
#define LG_SCOPED_CHECKLIST_STEP(ChecklistArg, StepArg) \
    LifeRequireChecklistArg(ChecklistArg); \
    LifeRequireChecklistArg(StepArg); \
    FLifeChecklistScope LIFE_CHECK_CONCAT(ChecklistScope_, __LINE__){ LifeChecklistToFName(ChecklistArg), LifeChecklistToFName(StepArg) }

#define LG_RESET_CHECKLIST(ChecklistArg) \
	LifeRequireChecklistArg(ChecklistArg); \
	FLifeChecklistRegistry::Get().ResetChecklist(LifeChecklistToFName(ChecklistArg));

#define LG_ENSURE_CHECKLIST_DONE(ChecklistArg) \
    LifeRequireChecklistArg(ChecklistArg); \
    LG_CONTRACT_CHECK_MSG( \
        FLifeChecklistRegistry::Get().IsChecklistDone(LifeChecklistToFName(ChecklistArg)), \
        FString::Printf(TEXT("Checklist '%s' not done (current: %s)"), \
            *LifeChecklistToFName(ChecklistArg).ToString(), \
            *FLifeChecklistRegistry::Get().GetLastCompletedStepName(LifeChecklistToFName(ChecklistArg))) \
    )