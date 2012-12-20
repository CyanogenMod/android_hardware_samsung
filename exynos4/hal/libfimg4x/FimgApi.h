/*
**
** Copyright 2009 Samsung Electronics Co, Ltd.
** Copyright 2008, The Android Open Source Project
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

#ifndef FIMG_API_H
#define FIMG_API_H

#include <utils/Log.h>
#include "SkMatrix.h"
#include "sec_g2d_4x.h"

#undef REAL_DEBUG
#undef ANDROID_LOG

#if defined(REAL_DEBUG)
#ifdef ANDROID_LOG
#define PRINT  LOGE
#define PRINTD LOGD
#else
#define PRINT  printf
#define PRINTD printf
#endif
#else
void VOID_FUNC(const char *format, ...);

#define PRINT  VOID_FUNC
#define PRINTD VOID_FUNC
#endif

#ifdef __cplusplus

struct blit_op_table {
    int op;
    const char *str;
};

extern struct blit_op_table optbl[];

class FimgApi
{
public:
#endif

#ifdef __cplusplus
private :
    bool    m_flagCreate;

protected :
    FimgApi();
    FimgApi(const FimgApi& rhs) {}
    virtual ~FimgApi();

public:
    bool        Create(void);
    bool        Destroy(void);
    inline bool FlagCreate(void) { return m_flagCreate; }
    bool        Stretch(struct fimg2d_blit *cmd);
    bool        Sync(void);

protected:
    virtual bool t_Create(void);
    virtual bool t_Destroy(void);
    virtual bool t_Stretch(struct fimg2d_blit *cmd);
    virtual bool t_Sync(void);
    virtual bool t_Lock(void);
    virtual bool t_UnLock(void);

};
#endif

#ifdef __cplusplus
extern "C"
#endif
struct FimgApi *createFimgApi();

#ifdef __cplusplus
extern "C"
#endif
void destroyFimgApi(FimgApi *ptrFimgApi);

#ifdef __cplusplus
extern "C"
#endif
int stretchFimgApi(struct fimg2d_blit *cmd);
#ifdef __cplusplus
extern "C"
#endif
int SyncFimgApi(void);

void printDataBlit(char *title, struct fimg2d_blit *cmd);
void printDataBlitRotate(int rotate);
void printDataBlitImage(char *title, struct fimg2d_image *image);
void printDataBlitRect(char *title, struct fimg2d_rect *rect);
void printDataBlitScale(struct fimg2d_scale *scaling);
void printDataMatrix(int matrixType);

#endif //FIMG_API_H
