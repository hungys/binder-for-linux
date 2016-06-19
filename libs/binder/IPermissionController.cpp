/*
 * Copyright (C) 2005 The Android Open Source Project
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

#define LOG_TAG "PermissionController"

#include <binder/IPermissionController.h>

#include <utils/Log.h>
#include <binder/Parcel.h>
#include <utils/String8.h>

#include <private/binder/Static.h>

namespace android {

// ----------------------------------------------------------------------

class BpPermissionController : public BpInterface<IPermissionController>
{
public:
    BpPermissionController(const sp<IBinder>& impl)
        : BpInterface<IPermissionController>(impl)
    {
    }

    virtual bool checkPermission(const String16& permission, int32_t pid, int32_t uid)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IPermissionController::getInterfaceDescriptor());
        data.writeString16(permission);
        data.writeInt32(pid);
        data.writeInt32(uid);
        remote()->transact(CHECK_PERMISSION_TRANSACTION, data, &reply);
        // fail on exception
        if (reply.readExceptionCode() != 0) return 0;
        return reply.readInt32() != 0;
    }

    virtual void getPackagesForUid(const uid_t uid, Vector<String16>& packages)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IPermissionController::getInterfaceDescriptor());
        data.writeInt32(uid);
        remote()->transact(GET_PACKAGES_FOR_UID_TRANSACTION, data, &reply);
        // fail on exception
        if (reply.readExceptionCode() != 0) {
            return;
        }
        const int32_t size = reply.readInt32();
        if (size <= 0) {
            return;
        }
        for (int i = 0; i < size; i++) {
            packages.push(reply.readString16());
        }
    }

    virtual bool isRuntimePermission(const String16& permission)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IPermissionController::getInterfaceDescriptor());
        data.writeString16(permission);
        remote()->transact(IS_RUNTIME_PERMISSION_TRANSACTION, data, &reply);
        // fail on exception
        if (reply.readExceptionCode() != 0) return false;
        return reply.readInt32() != 0;
    }
};

IMPLEMENT_META_INTERFACE(PermissionController, "android.os.IPermissionController");

// ----------------------------------------------------------------------

status_t BnPermissionController::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch(code) {
        case CHECK_PERMISSION_TRANSACTION: {
            CHECK_INTERFACE(IPermissionController, data, reply);
            String16 permission = data.readString16();
            int32_t pid = data.readInt32();
            int32_t uid = data.readInt32();
            bool res = checkPermission(permission, pid, uid);
            reply->writeNoException();
            reply->writeInt32(res ? 1 : 0);
            return NO_ERROR;
        } break;

        case GET_PACKAGES_FOR_UID_TRANSACTION: {
            CHECK_INTERFACE(IPermissionController, data, reply);
            int32_t uid = data.readInt32();
            Vector<String16> packages;
            getPackagesForUid(uid, packages);
            reply->writeNoException();
            size_t size = packages.size();
            reply->writeInt32(size);
            for (size_t i = 0; i < size; i++) {
                reply->writeString16(packages[i]);
            }
            return NO_ERROR;
        } break;

        case IS_RUNTIME_PERMISSION_TRANSACTION: {
            CHECK_INTERFACE(IPermissionController, data, reply);
            String16 permission = data.readString16();
            const bool res = isRuntimePermission(permission);
            reply->writeNoException();
            reply->writeInt32(res ? 1 : 0);
            return NO_ERROR;
        } break;

        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}

}; // namespace android
