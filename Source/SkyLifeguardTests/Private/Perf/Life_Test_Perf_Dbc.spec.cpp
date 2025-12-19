#include "LifeContracts.h"
#include "Helpers/Life_Helper_InvariantMetrics.h"

BEGIN_DEFINE_SPEC(FLife_Test_Perf_Dbc_Spec, "SkyLifeguard.Perf.Contracts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

END_DEFINE_SPEC(FLife_Test_Perf_Dbc_Spec)


void FLife_Test_Perf_Dbc_Spec::Define()
{
	// Setup and teardown to ensure objects aren't GC'd during tests
    BeforeEach([this]() {
    });
	AfterEach([this]() {

	});

	Describe("UPROPERTY Invariants", [this]() {
        It("Performance of 75 properties", [this]()
        {
            ULifeTestInvariantPerfObj* Obj = NewObject<ULifeTestInvariantPerfObj>();
            Obj->AddToRoot(); // Prevent GC

            // Initialize pointers to avoid invariant failure
            Obj->Ptr00 = Obj; Obj->Ptr01 = Obj; Obj->Ptr02 = Obj; Obj->Ptr03 = Obj; Obj->Ptr04 = Obj;
            Obj->Ptr05 = Obj; Obj->Ptr06 = Obj; Obj->Ptr07 = Obj; Obj->Ptr08 = Obj; Obj->Ptr09 = Obj;
            Obj->Ptr10 = Obj; Obj->Ptr11 = Obj; Obj->Ptr12 = Obj; Obj->Ptr13 = Obj; Obj->Ptr14 = Obj;
            Obj->Ptr15 = Obj; Obj->Ptr16 = Obj; Obj->Ptr17 = Obj; Obj->Ptr18 = Obj; Obj->Ptr19 = Obj;
            Obj->Ptr20 = Obj; Obj->Ptr21 = Obj; Obj->Ptr22 = Obj; Obj->Ptr23 = Obj; Obj->Ptr24 = Obj;

            const int32 Iterations = 10000;
            double StartTime = FPlatformTime::Seconds();

            for (int32 i = 0; i < Iterations; ++i)
            {
                CLASS_INVARIANTS(Obj);
            }

            double EndTime = FPlatformTime::Seconds();

            double TotalTime = EndTime - StartTime;
            double AvgTime = TotalTime / Iterations;

            const auto Info = FString::Printf(TEXT("Invariant Check Performance: Total: %f s, Avg: %f s per call (%d iterations)"), TotalTime, AvgTime, Iterations);
        	
        	AddInfo(Info);
        	
            Obj->RemoveFromRoot();
        });
	});
}
