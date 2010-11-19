#ifndef mifit_ui_CMolwViewScene_h
#define mifit_ui_CMolwViewScene_h

#include <math/Vector3.h>
#include <opengl/Scene.h>
#include <string>
#include <vector>

class MISurface;
class GLRenderer;
class Displaylist;
class PLINE;
class Stack;
class ViewPoint;
class ViewPointSettings;
namespace mi
{
    namespace opengl
    {
        class Axes;
        class Camera;
        class Frustum;
        class Viewport;
        class ViewportRelativeViewpoint;
        namespace interact
        {
            class TargetFeedback;
        }
    }
}

class CMolwViewScene : public mi::opengl::Scene
{

    friend class CMolwView;
    friend class MIGLWidget;

    mi::opengl::Viewport *viewport;

    mi::opengl::Frustum *frustum;

    mi::opengl::Camera *camera;

    mi::opengl::Camera *frontCamera;

    mi::opengl::interact::TargetFeedback *targetFeedback;

    float glUnitsPerPixel;

    bool showAtomLabels;

    GLRenderer *renderer;

    Displaylist *models;

    Molecule *pentamerModel;

    Stack *atomStack;

    ViewPoint *viewpoint;
    ViewPointSettings *viewpointSettings;

    std::map<void*, mi::opengl::Axes*> axes;

    static std::string labelsFontFile;

    bool ShowLabels;
    bool ShowContacts;
    bool ShowGnomon;
    bool topView;
    bool showUnitCell;
    bool showSymmetryAsBackbone;

    std::vector<PLINE> torsionArrow;

    mi::math::Vector3<float> *corners;

    float targetFeedbackSize;

public:

    CMolwViewScene();
    virtual ~CMolwViewScene();

    virtual void setViewport(mi::opengl::Viewport *viewport);

    virtual void initializeForRender();

    virtual void preCameraRender();

    virtual void render();

    virtual void renderOverlay();

    void setCorners(mi::math::Vector3<float> *corners);

    float getTargetSize();
    void setTargetSize(float size);

};

#endif // ifndef mifit_ui_CMolwViewScene_h
