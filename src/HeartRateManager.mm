#include "HeartRateManager.h"
#import <TargetConditionals.h>

#if TARGET_OS_IOS || TARGET_OS_MACCATALYST
#import <HealthKit/HealthKit.h>

// Stored globally per HeartRateManager instance so ARC manages lifetime
static NSMutableDictionary<NSValue*, id>* g_observers = nil;

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
            NSLog(@"HealthKit: Health data IS available, store created");
        }
        else
        {
            avail->store(false);
            NSLog(@"HealthKit: Health data NOT available on this device");
        }
    }
    return self;
}

- (void)requestAuthorization
{
    if (!_healthStore) return;

    HKQuantityType* heartRateType = [HKQuantityType quantityTypeForIdentifier:HKQuantityTypeIdentifierHeartRate];
    if (!heartRateType) return;

    NSSet<HKObjectType*>* readTypes = [NSSet setWithObject:heartRateType];
    NSLog(@"HealthKit: Requesting authorization...");

    [_healthStore requestAuthorizationToShareTypes:nil readTypes:readTypes completion:^(BOOL success, NSError* error) {
        NSLog(@"HealthKit: Auth completion — success=%d, error=%@", success, error);
        if (success)
        {
            self->_authorized->store(true);
            dispatch_async(dispatch_get_main_queue(), ^{
                [self startObserving];
            });
        }
        else
        {
            self->_authorized->store(false);
        }
    }];
}

- (void)startObserving
{
    if (!_healthStore || !_authorized->load()) return;
    if (_heartRateQuery != nil) return;

    NSLog(@"HealthKit: Starting heart rate observation...");

    HKQuantityType* heartRateType = [HKQuantityType quantityTypeForIdentifier:HKQuantityTypeIdentifierHeartRate];

    NSDate* recent = [NSDate dateWithTimeIntervalSinceNow:-300];
    NSPredicate* predicate = [HKQuery predicateForSamplesWithStartDate:recent
                                                              endDate:nil
                                                              options:HKQueryOptionStrictStartDate];

    _heartRateQuery = [[HKAnchoredObjectQuery alloc]
        initWithType:heartRateType
           predicate:predicate
              anchor:nil
               limit:5
      resultsHandler:^(HKAnchoredObjectQuery* query, NSArray<HKSample*>* samples,
                       NSArray<HKDeletedObject*>* deleted, HKQueryAnchor* newAnchor, NSError* error)
    {
        if (!error && samples)
            [self processHeartRateSamples:samples];
    }];

    _heartRateQuery.updateHandler = ^(HKAnchoredObjectQuery* query, NSArray<HKSample*>* added,
                                      NSArray<HKDeletedObject*>* deleted, HKQueryAnchor* newAnchor, NSError* error)
    {
        if (!error && added)
            [self processHeartRateSamples:added];
    };

    @try {
        [_healthStore executeQuery:_heartRateQuery];
        NSLog(@"HealthKit: Query started OK");
    } @catch (NSException* e) {
        NSLog(@"HealthKit: Query failed: %@", e.reason);
        _heartRateQuery = nil;
    }
}

- (void)processHeartRateSamples:(NSArray<HKSample*>*)samples
{
    if (!samples || samples.count == 0) return;

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

    if (latest && _heartRateBpm)
    {
        HKUnit* bpmUnit = [[HKUnit countUnit] unitDividedByUnit:[HKUnit minuteUnit]];
        double bpm = [latest.quantity doubleValueForUnit:bpmUnit];
        _heartRateBpm->store(bpm);
        NSLog(@"HealthKit: Heart rate = %.0f BPM", bpm);
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

// C++ wrapper — uses global dictionary to let ARC manage the observer lifetime
HeartRateManager::HeartRateManager()
{
    if (!g_observers)
        g_observers = [NSMutableDictionary new];

    HeartRateObserver* observer = [[HeartRateObserver alloc] initWithAtomics:&heartRateBpm auth:&authorized avail:&available];
    NSValue* key = [NSValue valueWithPointer:this];
    g_observers[key] = observer;
    impl = (__bridge void*)observer;
}

HeartRateManager::~HeartRateManager()
{
    if (impl)
    {
        HeartRateObserver* observer = (__bridge HeartRateObserver*)impl;
        [observer stopObserving];

        NSValue* key = [NSValue valueWithPointer:this];
        [g_observers removeObjectForKey:key];
        impl = nullptr;
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
