#include "topdown/TopdownRenderTexture.h"

#include "raylib.h"
#include "rlgl.h"
#include "external/glfw/include/GLFW/glfw3.h"

RenderTexture2D LoadTopdownRenderTextureWithStencil(int width, int height)
{
    RenderTexture2D target = {};

    target.id = rlLoadFramebuffer();
    if (target.id > 0) {
        rlEnableFramebuffer(target.id);

        target.texture.id = rlLoadTexture(
                nullptr,
                width,
                height,
                PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
                1);
        target.texture.width = width;
        target.texture.height = height;
        target.texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        target.texture.mipmaps = 1;

        rlFramebufferAttach(
                target.id,
                target.texture.id,
                RL_ATTACHMENT_COLOR_CHANNEL0,
                RL_ATTACHMENT_TEXTURE2D,
                0);

        unsigned int depthStencilId = 0;
        glGenRenderbuffers(1, &depthStencilId);
        glBindRenderbuffer(GL_RENDERBUFFER, depthStencilId);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthStencilId);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilId);

        target.depth.id = depthStencilId;
        target.depth.width = width;
        target.depth.height = height;
        target.depth.format = 19;
        target.depth.mipmaps = 1;

        if (rlFramebufferComplete(target.id)) {
            TraceLog(LOG_INFO, "FBO: [ID %i] Topdown render texture with stencil loaded successfully", target.id);
        } else {
            TraceLog(
                    LOG_WARNING,
                    "FBO: [ID %i] Topdown render texture with stencil could not be created",
                    target.id);
        }

        rlDisableFramebuffer();
    } else {
        TraceLog(LOG_WARNING, "FBO: Framebuffer object can not be created");
    }

    return target;
}

bool TopdownRenderTextureHasStencil(const RenderTexture2D& target)
{
    glBindFramebuffer(GL_FRAMEBUFFER, target.id);

    GLint objectType = GL_NONE;
    GLint objectName = 0;
    glGetFramebufferAttachmentParameteriv(
            GL_FRAMEBUFFER,
            GL_STENCIL_ATTACHMENT,
            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
            &objectType);
    glGetFramebufferAttachmentParameteriv(
            GL_FRAMEBUFFER,
            GL_STENCIL_ATTACHMENT,
            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
            &objectName);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    const bool hasObjectType = (objectType == GL_RENDERBUFFER) || (objectType == GL_TEXTURE);
    return hasObjectType && (objectName != 0);
}
