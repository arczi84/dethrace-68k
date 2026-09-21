#include <proto/exec.h>
#include <libraries/minigl.h>
#include <libraries/minigl_dispatch.h>

/* Implemented by the tiny compiler-specific import stub. */
extern const MGLDispatchTable *MiniGLGetDispatchTableLVO(void);

BOOL MiniGLOpen(void)
{
    const MGLDispatchTable *dispatch;

    if (MiniGLBase && MiniGLDispatch)
        return TRUE;

    if (!MiniGLBase) {
        /* The dispatch ABI/structure checks below are the real compatibility
         * guard.  Do not reject a compatible minigl.library solely because of
         * its Amiga library version number. */
        MiniGLBase = OpenLibrary(MINIGLNAME, 0);
        if (!MiniGLBase)
            return FALSE;
    }

    dispatch = MiniGLGetDispatchTableLVO();
	if (!dispatch)
{
    CloseLibrary(MiniGLBase);
    MiniGLBase = NULL;
    return 0;
}
	if (dispatch->abiVersion != MINIGL_DISPATCH_ABI_VERSION ||
    dispatch->structSize < sizeof(MGLDispatchTable))
    {
        CloseLibrary(MiniGLBase);
        MiniGLBase = 0;
        MiniGLDispatch = 0;
        return FALSE;
    }

    MiniGLDispatch = dispatch;
    return TRUE;
}

void MiniGLClose(void)
{
    MiniGLDispatch = 0;

    if (MiniGLBase) {
        CloseLibrary(MiniGLBase);
        MiniGLBase = 0;
    }
}
