/*
 * Compatibility entries absent from the newer static MiniGL backend used by
 * Dethrace.  The shared-library ABI still publishes these legacy demo/helper
 * calls, although the game never uses them.
 */
#include <mgl/gl.h>

void MGLExit(GLcontext context) { (void)context; }
void MGLIdleFunc(GLcontext context, IdleFn callback) { (void)context; (void)callback; }
void MGLKeyFunc(GLcontext context, KeyHandlerFn callback) { (void)context; (void)callback; }
void MGLMainLoop(GLcontext context) { (void)context; }
void MGLMouseFunc(GLcontext context, MouseHandlerFn callback) { (void)context; (void)callback; }
void MGLResizeContext(GLcontext context, GLsizei width, GLsizei height)
{
    (void)context;
    (void)width;
    (void)height;
}
void MGLPrintMatrix(GLcontext context, int mode) { (void)context; (void)mode; }
void MGLPrintMatrixStack(GLcontext context, int mode) { (void)context; (void)mode; }
void MGLSpecialFunc(GLcontext context, SpecialHandlerFn callback) { (void)context; (void)callback; }
void MGLWriteShotPPM(GLcontext context, char *filename) { (void)context; (void)filename; }
