#include "HeartRateManager.h"
#import <TargetConditionals.h>

#if TARGET_OS_IOS || TARGET_OS_MACCATALYST
#import <HealthKit/HealthKit.h>

@interface HeartRateObserver : NSObject
@property (nonatomic, strong) HKHealthStore* healthStore;
@property (nonatomic, strong) HKAnchoredObjectQuery* heartRateQuery;
@property (nonatomic, assign) std::atomic<double>* heartRateBpm;
@property (nonatomic, assign) std::atomic<bool>* authorized;
@property (nonatomic, assign) std::atomic<bool>* available;
@end

@implementation HeartRateObserver

- (instancetype)initWithAtomics:(std::atomic<double>*)bpm
                           auth:(std::atomic<bool>*)auth
                          avail:(std::atomic<bool>*)avail
{
    self = [super init];
    if (self)
    {
        _heartRateBpm = bpm;
        _authorized = auth;
        _available = avail;

        if ([HKHealthStore isHealthDataAvailable])
        {
            _healthStore = [[HKHealthStore alloc] init];
            avail->store(true);
        }
        else
        {
            avail->store(false);
        }
    }
    return self;
}

- (void)requestAuthorization
{
    NSLog(@"HealthKit: requestAuthorization called, healthStore=%@, available=%d",
          _healthStore, [HKHealthStore isHealthDataAvailable]);

    if (!_healthStore)
    {
        NSLog(@"HealthKit: No health store — health data not available on this device");
        return;
    }

    HKQuantityType* heartRateType = [HKQuantityType quantityTypeForIdentifier:HKQuantityTypeIdentifierHeartRate];
    if (!heartRateType)
    {
        NSLog(@"HealthKit: Failed to create heart rate quantity type");
        return;
    }

    NSSet<HKObjectType*>* readTypes = [NSSet setWithObject:heartRateType];
    NSLog(@"HealthKit: Requesting authorization for heart rate read access...");

    [_healthStore requestAuthorizationToShareTypes:nil readTypes:readTypes completion:^(BOOL success, NSError* error) {
        if (success)
        {
            self->_authorized->store(true);
            NSLog(@"HealthKit: Authorization granted! Starting observation...");
            [self startObserving];
        }
        else
        {
            self->_authorized->store(false);
            if (error)
                NSLog(@"HealthKit: Auth error: %@", error.localizedDescription);
            else
                NSLog(@"HealthKit: Auth denied (no error object)");
        }
    }];
}

- (void)startObserving
{
    if (!_healthStore || !_authorized->load()) return;
    if (_heartRateQuery != nil) return;  // already observing

    NSLog(@"HealthKit: Starting heart rate observation...");

    HKQuantityType* heartRateType = [HKQuantityType quantityTypeForIdentifier:HKQuantityTypeIdentifierHeartRate];

    // Use anchored query to get new samples as they arrive
    HKQueryAnchor* anchor = [HKQueryAnchor anchorWithValue:HKAnchoredObjectQueryNoAnchor];

    _heartRateQuery = [[HKAnchoredObjectQuery alloc]
        initWithType:heartRateType
           predicate:nil
              anchor:anchor
               limit:HKObjectQueryNoLimit
      resultsHandler:^(HKAnchoredObjectQuery* query, NSArray<HKSample*>* samples,
                       NSArray<HKDeletedObject*>* deleted, HKQueryAnchor* newAnchor, NSError* error)
    {
        [self processHeartRateSamples:samples];
    }];

    // Update handler for ongoing samples
    _heartRateQuery.updateHandler = ^(HKAnchoredObjectQuery* query, NSArray<HKSample*>* added,
                                      NSArray<HKDeletedObject*>* deleted, HKQueryAnchor* newAnchor, NSError* error)
    {
        [self processHeartRateSamples:added];
    };

    [_healthStore executeQuery:_heartRateQuery];
}

- (void)processHeartRateSamples:(NSArray<HKSample*>*)samples
{
    if (samples.count == 0) return;

    // Get the most recent sample
    HKQuantitySample* latest = nil;
    for (HKSample* sample in samples)
    {
        if ([sample isKindOfClass:[HKQuantitySample class]])
        {
            HKQuantitySample* qs = (HKQuantitySample*)sample;
            if (latest == nil || [qs.endDate compare:latest.endDate] == NSOrderedDescending)
                latest = qs;
        }
    }

    if (latest)
    {
        HKUnit* bpmUnit = [[HKUnit countUnit] unitDividedByUnit:[HKUnit minuteUnit]];
        double bpm = [latest.quantity doubleValueForUnit:bpmUnit];
        _heartRateBpm->store(bpm);
    }
}

- (void)stopObserving
{
    if (_healthStore && _heartRateQuery)
    {
        [_healthStore stopQuery:_heartRateQuery];
        _heartRateQuery = nil;
    }
}

@end

// C++ wrapper implementation
HeartRateManager::HeartRateManager()
{
    auto* observer = [[HeartRateObserver alloc] initWithAtomics:&heartRateBpm auth:&authorized avail:&available];
    impl = (__bridge_retained void*)observer;
}

HeartRateManager::~HeartRateManager()
{
    if (impl)
    {
        auto* observer = (__bridge_transfer HeartRateObserver*)impl;
        [observer stopObserving];
        (void)observer;  // ARC releases
    }
}

void HeartRateManager::requestAuthorization()
{
    if (impl)
        [(__bridge HeartRateObserver*)impl requestAuthorization];
}

void HeartRateManager::startObserving()
{
    if (impl)
        [(__bridge HeartRateObserver*)impl startObserving];
}

void HeartRateManager::stopObserving()
{
    if (impl)
        [(__bridge HeartRateObserver*)impl stopObserving];
}

#else
// Non-iOS stubs
HeartRateManager::HeartRateManager() {}
HeartRateManager::~HeartRateManager() {}
void HeartRateManager::requestAuthorization() {}
void HeartRateManager::startObserving() {}
void HeartRateManager::stopObserving() {}
#endif
