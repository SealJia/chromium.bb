//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FenceSyncGL.h: Defines the class interface for FenceSyncGL.

#ifndef LIBANGLE_RENDERER_GL_FENCESYNCGL_H_
#define LIBANGLE_RENDERER_GL_FENCESYNCGL_H_

#include "libANGLE/renderer/FenceSyncImpl.h"

namespace rx
{

class FenceSyncGL : public FenceSyncImpl
{
  public:
    FenceSyncGL();
    ~FenceSyncGL() override;

    gl::Error set() override;
    gl::Error clientWait(GLbitfield flags, GLuint64 timeout, GLenum *outResult) override;
    gl::Error serverWait(GLbitfield flags, GLuint64 timeout) override;
    gl::Error getStatus(GLint *outResult) override;
};

}

#endif // LIBANGLE_RENDERER_GL_FENCESYNCGL_H_
