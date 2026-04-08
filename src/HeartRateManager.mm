#include "HeartRateManager.h"
#import <TargetConditionals.h>

#if TARGET_OS_IOS || TARGET_OS_MACCATALYST
#import <HealthKit/HealthKit.h>

// Simple polling approach — query latest HR sample every few seconds.
// No long-running queries, no Obj-C observer objects, no lifetime issues.

static HKHealthStore* g_healthStore = nil;
static std::atomic<bool> g_observing { false };
static std::atomic<bool> g_authorized { false };
static std::atomic<double>* g_bpmOut = nullptr;

static void pollLatestHeartRate()
{
    if (!g_healthStore || !g_bpmOut) return;

    HKQuantityType* hrType = [HKQuantityType quantityTypeForIdentifier:HKQuantityTypeIdentifierHeartRate];
    NSSortDescriptor* sortByDate = [[NSSortDescriptor alloc] initWithKey:HKSampleSortIdentifierEndDate ascending:NO];

    HKSampleQuery* query = [[HKSampleQuery alloc]
        initWithSampleType:hrType
                 predicate:nil
                     limit:1
           sortDescriptors:@[sortByDate]
            resultsHandler:^(HKSampleQuery* q, NSArray<__kindof HKSample*>* results, NSError* error)
    {
        if (error || !results || results.count == 0) return;

        HKQuantitySample* sample = (HKQuantitySample*)results.firstObject;
        if (![sample isKindOfClass:[HKQuantitySample class]]) return;

        HKUnit* bpmUnit = [[HKUnit countUnit] unitDividedByUnit:[HKUnit minuteUnit]];
        double bpm = [sample.quantity doubleValueForUnit:bpmUnit];
        if (g_bpmOut) g_bpmOut->store(bpm);
    }];

    [g_healthStore executeQuery:query];
}

HeartRateManager::HeartRateManager()
{
    g_bpmOut = &heartRateBpm;

    if ([HKHealthStore isHealthDataAvailable])
    {
        if (!g_healthStore)
            g_healthStore = [[HKHealthStore alloc] init];
        available.store(true);
    }
    else
    {
        available.store(false);
    }
}

HeartRateManager::~HeartRateManager()
{
    g_observing.store(false);
    g_bpmOut = nullptr;
}

void HeartRateManager::requestAuthorization()
{
    if (!g_healthStore) return;

    HKQuantityType* hrType = [HKQuantityType quantityTypeForIdentifier:HKQuantityTypeIdentifierHeartRate];
    if (!hrType) return;

    std::atomic<bool>* authPtr = &authorized;

    [g_healthStore requestAuthorizationToShareTypes:nil
                                         readTypes:[NSSet setWithObject:hrType]
                                        completion:^(BOOL success, NSError* error)
    {
        NSLog(@"HealthKit auth: success=%d error=%@", success, error);
        if (success)
        {
            g_authorized.store(true);
            if (authPtr) authPtr->store(true);

            // Start polling on background thread
            if (!g_observing.load())
            {
                g_observing.store(true);
                dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
                    while (g_observing.load())
                    {
                        @autoreleasepool {
                            pollLatestHeartRate();
                        }
                        [NSThread sleepForTimeInterval:5.0];
                    }
                });
            }
        }
    }];
}

void HeartRateManager::startObserving()
{
    if (!g_authorized.load() || g_observing.load()) return;

    g_observing.store(true);
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        while (g_observing.load())
        {
            @autoreleasepool {
                pollLatestHeartRate();
            }
            [NSThread sleepForTimeInterval:5.0];
        }
    });
}

void HeartRateManager::stopObserving()
{
    g_observing.store(false);
}

#else
HeartRateManager::HeartRateManager() {}
HeartRateManager::~HeartRateManager() {}
void HeartRateManager::requestAuthorization() {}
void HeartRateManager::startObserving() {}
void HeartRateManager::stopObserving() {}
#endif
