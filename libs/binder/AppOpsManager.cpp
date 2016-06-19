/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <binder/AppOpsManager.h>
#include <binder/Binder.h>
#include <binder/IServiceManager.h>

#include <utils/SystemClock.h>

namespace android {

static String16 _appops("appops");
static pthread_mutex_t gTokenMutex = PTHREAD_MUTEX_INITIALIZER;
static sp<IBinder> gToken;

static const sp<IBinder>& getToken(const sp<IAppOpsService>& service) {
    pthread_mutex_lock(&gTokenMutex);
    if (gToken == NULL) {
        gToken = service->getToken(new BBinder());
    }
    pthread_mutex_unlock(&gTokenMutex);
    return gToken;
}

AppOpsManager::AppOpsManager()
{
}

sp<IAppOpsService> AppOpsManager::getService()
{
    int64_t startTime = 0;
    mLock.lock();
    sp<IAppOpsService> service = mService;
    while (service == NULL || !IInterface::asBinder(service)->isBinderAlive()) {
        sp<IBinder> binder = defaultServiceManager()->checkService(_appops);
        if (binder == NULL) {
            // Wait for the app ops service to come back...
            if (startTime == 0) {
                startTime = uptimeMillis();
                ALOGI("Waiting for app ops service");
            } else if ((uptimeMillis()-startTime) > 10000) {
                ALOGW("Waiting too long for app ops service, giving up");
                return NULL;
            }
            sleep(1);
        } else {
            service = interface_cast<IAppOpsService>(binder);
            mService = service;
        }
    }
    mLock.unlock();
    return service;
}

int32_t AppOpsManager::checkOp(int32_t op, int32_t uid, const String16& callingPackage)
{
    sp<IAppOpsService> service = getService();
    return service != NULL ? service->checkOperation(op, uid, callingPackage) : MODE_IGNORED;
}

int32_t AppOpsManager::noteOp(int32_t op, int32_t uid, const String16& callingPackage) {
    sp<IAppOpsService> service = getService();
    return service != NULL ? service->noteOperation(op, uid, callingPackage) : MODE_IGNORED;
}

int32_t AppOpsManager::startOp(int32_t op, int32_t uid, const String16& callingPackage) {
    sp<IAppOpsService> service = getService();
    return service != NULL ? service->startOperation(getToken(service), op, uid, callingPackage)
            : MODE_IGNORED;
}

void AppOpsManager::finishOp(int32_t op, int32_t uid, const String16& callingPackage) {
    sp<IAppOpsService> service = getService();
    if (service != NULL) {
        service->finishOperation(getToken(service), op, uid, callingPackage);
    }
}

void AppOpsManager::startWatchingMode(int32_t op, const String16& packageName,
        const sp<IAppOpsCallback>& callback) {
    sp<IAppOpsService> service = getService();
    if (service != NULL) {
        service->startWatchingMode(op, packageName, callback);
    }
}

void AppOpsManager::stopWatchingMode(const sp<IAppOpsCallback>& callback) {
    sp<IAppOpsService> service = getService();
    if (service != NULL) {
        service->stopWatchingMode(callback);
    }
}

int32_t AppOpsManager::permissionToOpCode(const String16& permission) {
    sp<IAppOpsService> service = getService();
    if (service != NULL) {
        return service->permissionToOpCode(permission);
    }
    return -1;
}


}; // namespace android
