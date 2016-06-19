/*
 * Copyright 2015 The Android Open Source Project
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

#include <binder/IProcessInfoService.h>
#include <binder/Parcel.h>
#include <utils/Errors.h>
#include <sys/types.h>

namespace android {

// ----------------------------------------------------------------------

class BpProcessInfoService : public BpInterface<IProcessInfoService> {
public:
    BpProcessInfoService(const sp<IBinder>& impl)
        : BpInterface<IProcessInfoService>(impl) {}

    virtual status_t getProcessStatesFromPids(size_t length, /*in*/ int32_t* pids,
            /*out*/ int32_t* states)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IProcessInfoService::getInterfaceDescriptor());
        data.writeInt32Array(length, pids);
        data.writeInt32(length); // write length of output array, used by java AIDL stubs
        status_t err = remote()->transact(GET_PROCESS_STATES_FROM_PIDS, data, &reply);
        if (err != NO_ERROR || ((err = reply.readExceptionCode()) != NO_ERROR)) {
            return err;
        }
        int32_t replyLen = reply.readInt32();
        if (static_cast<size_t>(replyLen) != length) {
            return NOT_ENOUGH_DATA;
        }
        if (replyLen > 0 && (err = reply.read(states, length * sizeof(*states))) != NO_ERROR) {
            return err;
        }
        return reply.readInt32();
    }

};

IMPLEMENT_META_INTERFACE(ProcessInfoService, "android.os.IProcessInfoService");

// ----------------------------------------------------------------------

status_t BnProcessInfoService::onTransact( uint32_t code, const Parcel& data, Parcel* reply,
        uint32_t flags) {
    switch(code) {
        case GET_PROCESS_STATES_FROM_PIDS: {
            CHECK_INTERFACE(IProcessInfoService, data, reply);
            int32_t arrayLen = data.readInt32();
            if (arrayLen <= 0) {
                reply->writeNoException();
                reply->writeInt32(0);
                reply->writeInt32(NOT_ENOUGH_DATA);
                return NO_ERROR;
            }

            size_t len = static_cast<size_t>(arrayLen);
            int32_t pids[len];
            status_t res = data.read(pids, len * sizeof(*pids));

            // Ignore output array length returned in the parcel here, as the states array must
            // always be the same length as the input PIDs array.
            int32_t states[len];
            for (size_t i = 0; i < len; i++) states[i] = -1;
            if (res == NO_ERROR) {
                res = getProcessStatesFromPids(len, /*in*/ pids, /*out*/ states);
            }
            reply->writeNoException();
            reply->writeInt32Array(len, states);
            reply->writeInt32(res);
            return NO_ERROR;
        } break;
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}

// ----------------------------------------------------------------------

}; // namespace android
