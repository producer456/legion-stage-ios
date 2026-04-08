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

static int g_pollCount = 0;

static void pollLatestHeartRate()
{
    if (!g_healthStore || !g_bpmOut) return;

    g_pollCount++;

    HKQuantityType* hrType = [HKQuantityType quantityTypeForIdentifier:HKQuantityTypeIdentifierHeartRate];
    NSSortDescriptor* sortByDate = [[NSSortDescriptor alloc] initWithKey:HKSampleSortIdentifierEndDate ascending:NO];

    // Get the most recent sample — no time predicate so we always get something
    HKSampleQuery* query = [[HKSampleQuery alloc]
        initWithSampleType:hrType
                 predicate:nil
                     limit:1
           sortDescriptors:@[sortByDate]
            resultsHandler:^(HKSampleQuery* q, NSArray<__kindof HKSample*>* results, NSError* error)
    {
        if (error) { NSLog(@"HealthKit poll error: %@", error); return; }
        if (!results || results.count == 0) { NSLog(@"HealthKit poll: no results"); return; }

        HKQuantitySample* sample = (HKQuantitySample*)results.firstObject;
        if (![sample isKindOfClass:[HKQuantitySample class]]) return;

        HKUnit* bpmUnit = [[HKUnit countUnit] unitDividedByUnit:[HKUnit minuteUnit]];
        double bpm = [sample.quantity doubleValueForUnit:bpmUnit];

        // Log the sample age so we can see if we're getting fresh data
        NSTimeInterval age = [[NSDate date] timeIntervalSinceDate:sample.endDate];
        NSLog(@"HealthKit poll #%d: HR=%.0f BPM, sample age=%.0fs", g_pollCount, bpm, age);

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

        // Check if already authorized and auto-start polling
        HKQuantityType* hrType = [HKQuantityType quantityTypeForIdentifier:HKQuantityTypeIdentifierHeartRate];
        HKAuthorizationStatus status = [g_healthStore authorizationStatusForType:hrType];
        NSLog(@"HealthKit: init auth status = %ld", (long)status);

        // status == HKAuthorizationStatusSharingAuthorized means we can write (not relevant)
        // For reading, HealthKit doesn't reveal read auth status (privacy).
        // So just try an initial poll — if it returns data, we're authorized.
        pollLatestHeartRate();

        // Wait briefly then check if we got data — if so, start continuous polling
        std::atomic<bool>* authPtr = &authorized;
        HeartRateManager* mgr = this;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            if (g_bpmOut && g_bpmOut->load() > 0.0)
            {
                NSLog(@"HealthKit: Auto-detected authorization, starting polling");
                g_authorized.store(true);
                if (authPtr) authPtr->store(true);
                mgr->startObserving();
            }
            else
            {
                NSLog(@"HealthKit: No data from initial poll — user needs to tap Connect Watch");
            }
        });
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
                        [NSThread sleepForTimeInterval:3.0];
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
            [NSThread sleepForTimeInterval:3.0];
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
