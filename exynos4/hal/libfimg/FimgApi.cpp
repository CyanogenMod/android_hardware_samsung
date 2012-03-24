/*
**
** Copyright 2009 Samsung Electronics Co, Ltd.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
**
*/

///////////////////////////////////////////////////
// include
///////////////////////////////////////////////////
#define LOG_NDEBUG 0
#define LOG_TAG "FimgApi"
#include <utils/Log.h>

#include "FimgApi.h"

//---------------------------------------------------------------------------//
// Global Function
//---------------------------------------------------------------------------//
#ifndef REAL_DEBUG
    void VOID_FUNC(const char* format, ...)
    {}
#endif

//---------------------------------------------------------------------------//
// FimgApi
//---------------------------------------------------------------------------//

//---------------------------------------------------------------------------//
// Method Function Implementation
//---------------------------------------------------------------------------//

FimgApi::FimgApi()
{
    m_flagCreate = false;
}

FimgApi::~FimgApi()
{
    if(m_flagCreate == true)
        PRINT("%s::this is not Destroyed fail \n", __func__);
}

bool FimgApi::Create(void)
{
    bool ret = false;

    if(t_Lock() == false) {
        PRINT("%s::t_Lock() fail \n", __func__);
        goto CREATE_DONE;
    }

    if(m_flagCreate == true) {
        PRINT("%s::Already Created fail \n", __func__);
        goto CREATE_DONE;
    }

    if(t_Create() == false) {
        PRINT("%s::t_Create() fail \n", __func__);
        goto CREATE_DONE;
    }

    m_flagCreate = true;

    ret = true;

CREATE_DONE :

    t_UnLock();

    return ret;
}

bool FimgApi::Destroy(void)
{
    bool ret = false;

    if(t_Lock() == false) {
        PRINT("%s::t_Lock() fail \n", __func__);
        goto DESTROY_DONE;
    }

    if(m_flagCreate == false) {
        PRINT("%s::Already Destroyed fail \n", __func__);
        goto DESTROY_DONE;
    }

    if(t_Destroy() == false) {
        PRINT("%s::t_Destroy() fail \n", __func__);
        goto DESTROY_DONE;
    }

    m_flagCreate = false;

    ret = true;

DESTROY_DONE :

    t_UnLock();

    return ret;
}

bool FimgApi::Stretch(FimgRect * src, FimgRect * dst, FimgClip *clip, FimgFlag * flag)
{
    bool ret = false;

    if(t_Lock() == false) {
        PRINT("%s::t_Lock() fail \n", __func__);
        goto STRETCH_DONE;
    }

    if(m_flagCreate == false) {
        PRINT("%s::This is not Created fail \n", __func__);
        goto STRETCH_DONE;
    }

    if(t_Stretch(src, dst, clip, flag) == false) {
        goto STRETCH_DONE;
    }

    ret = true;

STRETCH_DONE :

    t_UnLock();

    return ret;
}

bool FimgApi::Sync(void)
{
    bool ret = false;

    if(m_flagCreate == false) {
        PRINT("%s::This is not Created fail \n", __func__);
        goto SYNC_DONE;
    }

    if(t_Sync() == false) {
        goto SYNC_DONE;
    }

    ret = true;

SYNC_DONE :

    return ret;
}

bool FimgApi::t_Create(void)
{
    PRINT("%s::This is empty virtual function fail\n", __func__);
    return false;
}

bool FimgApi::t_Destroy(void)
{
    PRINT("%s::This is empty virtual function fail\n", __func__);
    return false;
}

bool FimgApi::t_Stretch(FimgRect * src, FimgRect * dst, FimgClip * clip, FimgFlag * flag)
{
    PRINT("%s::This is empty virtual function fail\n", __func__);
    return false;
}

bool FimgApi::t_Sync(void)
{
    PRINT("%s::This is empty virtual function fail\n", __func__);
    return false;
}

bool FimgApi::t_Lock(void)
{
    PRINT("%s::This is empty virtual function fail\n", __func__);
    return false;
}

bool FimgApi::t_UnLock(void)
{
    PRINT("%s::This is empty virtual function fail\n", __func__);
    return false;
}


//---------------------------------------------------------------------------//
// extern function
//---------------------------------------------------------------------------//
extern "C" int stretchFimgApi(FimgRect * src, FimgRect * dst, FimgClip * clip, FimgFlag * flag)
{
    FimgApi * fimgApi = createFimgApi();

    if(fimgApi == NULL) {
        PRINT("%s::createFimgApi() fail \n", __func__);
        return -1;
    }

    if(fimgApi->Stretch(src, dst, clip, flag) == false) {
        if(fimgApi != NULL)
            destroyFimgApi(fimgApi);

        return -1;
    }

    if(fimgApi != NULL)
        destroyFimgApi(fimgApi);

    return 0;
}

extern "C" int SyncFimgApi(void)
{
    FimgApi * fimgApi = createFimgApi();
    if(fimgApi == NULL) {
        PRINT("%s::createFimgApi() fail \n", __func__);
        return -1;
    }

    if(fimgApi->Sync() == false) {
        if(fimgApi != NULL)
            destroyFimgApi(fimgApi);

        return -1;
    }

    if(fimgApi != NULL)
        destroyFimgApi(fimgApi);

    return 0;
}

