#include <QtDeclarative/QDeclarativeEngine>
#include <QtDeclarative/QDeclarativeContext>
#include <QtDeclarative/QDeclarativeComponent>

#include <vector>
#include <algorithm>
#include <cstring>

#include <math/Vector3.h>
#include <math/Matrix3.h>
#include <math/Quaternion.h>
#include <opengl/Camera.h>
#include <opengl/Frustum.h>
#include <opengl/OpenGL.h>
#include <opengl/QuatUtil.h>
#include <opengl/Sphere.h>
#include <opengl/StereoView.h>
#include <opengl/Viewport.h>
#include <opengl/interact/MousePicker.h>
#include <opengl/interact/TargetFeedback.h>

#ifndef _WIN32
#include <unistd.h>
#define strnicmp strncasecmp
#else
#define strncasecmp strnicmp
#endif

#include <QAbstractAnimation>
#include <QDialog>
#include <QDir>
#include <QString>
#include <QMenuBar>
#include <QResizeEvent>
#include <QCloseEvent>
#include <QApplication>
#include <QClipboard>
#include <QPrinter>
#include <QPrintDialog>
#include <QPainter>
#include <QRect>
#include <QVBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMessageBox>
#include <QLabel>
#include <QInputDialog>
#include <QSettings>
#include <QFileDialog>
#include <QGraphicsItem>
#include <QGraphicsObject>
#include <QDebug>

#include <math/mathlib.h>
#include <chemlib/chemlib.h>
#include <chemlib/Residue.h>
#include <conflib/conflib.h>
#include <map/maplib.h>
#include "core/corelib.h"
#include "core/ViewPointIO.h"
#include <util/utillib.h>
#include "ui/MIColorPickerDlg.h"
#include "ui/MIDialog.h"

#include "jobs/jobslib.h"
#include "GenericDataDialog.h"
#include "AddResidueDialog.h"

#include "CMolwViewScene.h"
#include "CMolwViewAnnotationPickingRenderable.h"
#include "CMolwViewAtomPickingRenderable.h"
#include "CMolwViewBondPickingRenderable.h"
#include "CMolwViewSlabPickingRenderable.h"

#include "RamaPlot.h"

#include "Application.h"
#include "Displaylist.h"
#include "EMap.h"
#include "GLOverviewCanvas.h"
#include "GLRenderer.h"
#include "MIMainWindow.h"
#include "MapSettings.h"
#include "MoleculeXmlHandler.h"
#include "MIGLWidget.h"
#include "Xguicryst.h"
#include "asplib.h"
#include "molw.h"
#include "surf.h"
#include "tools.h"
#include "PhaseFileLoadDialog.h"
#include "RefinementOptionsDialog.h"
#include "ui/LSQFitDialog.h"
#include "ui/SelectCrystal.h"
#include "ViewPointSettings.h"

using namespace chemlib;
using namespace mi::math;
using namespace mi::opengl::interact;
using namespace mi::opengl;
using namespace std;

#define SHOW_RENDER_TIME 0

// surface types of current atoms
#define NO_SURFACE 0
#define VDW_SURFACE 1
#define EXTENDED_SURFACE 2
#define PROBE_SURFACE 3

//enumeration of RightMouseMode
#define CENTER 0
#define ROTATE 1
#define TRANSLATE 2
#define BONDTWIST 3

namespace {
    enum SolidSurfaceMenuId
    {
        ID_SOLIDSURFACE_BUILD,
        ID_SOLIDSURFACE_COLOR,
        ID_SOLIDSURFACE_COLOR_BY_ATOM,
        ID_SOLIDSURFACE_CLEAR,
        ID_SOLIDSURFACE_MOLECULAR,
        ID_SOLIDSURFACE_ACCESSIBLE,
        ID_SOLIDSURFACE_ATOMS_MODE,
        ID_SOLIDSURFACE_RESIDUE_MODE,
        ID_SOLIDSURFACE_RESIDUES_MODE,
        ID_SOLIDSURFACE_PEPTIDE_MODE,
        ID_SOLIDSURFACE_MOLECULE_MODE,
        ID_SOLIDSURFACE_QUALITY_0,
        ID_SOLIDSURFACE_QUALITY_1,
        ID_SOLIDSURFACE_QUALITY_2,
        ID_SOLIDSURFACE_RESMOOTH,

        ID_SOLIDSURFACE_USESURF_0,
        ID_SOLIDSURFACE_USESURF_1,
        ID_SOLIDSURFACE_USESURF_2,
        ID_SOLIDSURFACE_USESURF_3,
        ID_SOLIDSURFACE_USESURF_4,
        ID_SOLIDSURFACE_USESURF_5,
        ID_SOLIDSURFACE_USESURF_6,
        ID_SOLIDSURFACE_USESURF_7,
        ID_SOLIDSURFACE_USESURF_8,
        ID_SOLIDSURFACE_USESURF_9,

        ID_SOLIDSURFACE_ALPHA,
        ID_SOLIDSURFACE_MINCOLOR,
        ID_SOLIDSURFACE_MEDCOLOR,
        ID_SOLIDSURFACE_MAXCOLOR,
        ID_SOLIDSURFACE_MINVAL,
        ID_SOLIDSURFACE_MEDVAL,
        ID_SOLIDSURFACE_MAXVAL,
        ID_SOLIDSURFACE_CALCDIST,
        ID_SOLIDSURFACE_GRADCOLOR,
    };

    QAction* solidsurf_menu_action(QMenu *menu, QActionGroup *group, int actionId, const char *text)
    {
        QAction *a = menu->addAction(QObject::tr(text));
        a->setData(actionId);
        group->addAction(a);
        return a;
    }


    class ViewPointAnimation : public QAbstractAnimation
    {
    public:
        ViewPointAnimation(ViewPoint *viewpoint, MIGLWidget *parent);
        virtual ~ViewPointAnimation();
        virtual int duration() const;
        void setCenterEnd(const Vector3<float> &center);
        void setViewEnd(const Quaternion<float> &view);
        void setScaleEnd(qreal scale);
    protected:
        virtual void updateCurrentTime(int currentTime);
    private:
        MIGLWidget *glWidget;
        ViewPoint *viewpoint;
        Vector3<float> centerStart;
        Vector3<float> centerEnd;
        Vector3<float> centerVector;
        Quaternion<float> viewStart;
        Quaternion<float> viewEnd;
        qreal scaleStart;
        qreal scaleEnd;
    };

    ViewPointAnimation::ViewPointAnimation(ViewPoint *viewpoint, MIGLWidget *parent)
        : QAbstractAnimation(parent), glWidget(parent), viewpoint(viewpoint),
          scaleStart(1), scaleEnd(1)
    {
        if (viewpoint)
        {
            centerStart = viewpoint->center();
            centerEnd = centerStart;
            viewStart = viewpoint->orientation();
            viewEnd = viewStart;
            scaleStart = viewpoint->scale();
            scaleEnd = scaleStart;
        }
        centerVector = centerEnd - centerStart;
    }

    ViewPointAnimation::~ViewPointAnimation()
    {
        viewpoint->setCenter(centerEnd);
        viewpoint->setView(viewEnd);
        viewpoint->setScale(scaleEnd);
        glWidget->doRefresh();
    }

    int ViewPointAnimation::duration() const
    {
        return 500;
    }

    void ViewPointAnimation::setCenterEnd(const Vector3<float> &center)
    {
        centerEnd = center;
        centerVector = centerEnd - centerStart;
    }

    void ViewPointAnimation::setViewEnd(const Quaternion<float> &view)
    {
        viewEnd = view;
    }

    void ViewPointAnimation::setScaleEnd(qreal scale)
    {
        scaleEnd = scale;
    }

    void ViewPointAnimation::updateCurrentTime(int currentTime)
    {
        float percent = static_cast<float>(currentTime) / duration();
        Vector3<float> delta(centerVector);
        delta *= percent;
        Quaternion<float> q(viewStart);
        q.interpolate(viewEnd, percent);
        qreal scale = scaleStart + (scaleEnd - scaleStart) * percent;
        viewpoint->setCenter(centerStart + delta);
        viewpoint->setView(q);
        viewpoint->setScale(scale);
        glWidget->doRefresh();
    }
}

class MyMIMolOptCheckPoint
    : public MIMolOptCheckPoint
{
public:
    MyMIMolOptCheckPoint(MIGLWidget *parent,
                         ViewPoint *viewpoint)
        : _parent(parent),
          _viewpoint(viewpoint)
    {
    }

    bool operator()(MIMoleculeBase*)
    {
        //Molecule *mol=dynamic_cast<Molecule*>(m);
        _parent->UpdateCurrent();
        _parent->ReDraw();
        return true;
    }

private:
    MIGLWidget *_parent;
    ViewPoint *_viewpoint;
};

namespace
{
    void readViewPointSettings(ViewPoint *viewpoint, ViewPointSettings *viewpointSettings)
    {
        QSettings settings;
        viewpoint->setPerspective(settings.value("View Parameters/perspective", 0).toInt() / 100.0f);
        viewpointSettings->setDepthCuedLineWidth(settings.value("View Parameters/depthcue_width", true).toBool());
        viewpointSettings->setDepthCuedColors(settings.value("View Parameters/depthcue_colors", true).toBool());
        viewpointSettings->setDimNonactiveModels(settings.value("View Parameters/dimNonactiveModels", true).toBool());
        viewpointSettings->setAmountToDimNonactiveModels(settings.value("View Parameters/amountToDimNonactiveModels", 50).toInt() / 100.0f);
        viewpointSettings->SetBallandStick(settings.value("View Parameters/ballandstick", 0).toInt());
        viewpointSettings->SetLineThickness(settings.value("View Parameters/lineThickness", 1).toInt());
        viewpointSettings->SetBallSize(settings.value("View Parameters/ballsize", 10).toInt());
        viewpointSettings->SetCylinderSize(settings.value("View Parameters/cylindersize", 33).toInt());
    }

    void writeViewPointSettings(ViewPoint *viewpoint, ViewPointSettings *viewpointSettings)
    {
        QSettings settings;
        settings.setValue("View Parameters/perspective", static_cast<int>(viewpoint->perspective()*100.0f));
        settings.setValue("View Parameters/depthcue_width", viewpointSettings->isDepthCuedLineWidth());
        settings.setValue("View Parameters/depthcue_colors", viewpointSettings->isDepthCuedColors());
        settings.setValue("View Parameters/dimNonactiveModels", viewpointSettings->isDimNonactiveModels());
        settings.setValue("View Parameters/amountToDimNonactiveModels", static_cast<int>(viewpointSettings->getAmountToDimNonactiveModels()*100.0f));
        settings.setValue("View Parameters/ballandstick", viewpointSettings->GetBallandStick());
        settings.setValue("View Parameters/ballsize", viewpointSettings->GetBallSize());
        settings.setValue("View Parameters/cylindersize", viewpointSettings->GetCylinderSize());
        settings.setValue("View Parameters/lineThickness", viewpointSettings->GetLineThickness());
    }

} // anonymous namespace


static void SplitPath(const std::string &origPath,
                      std::string *dirname,
                      std::string *fname,
                      std::string *ext)
{
    *dirname = std::string("");
    *fname = std::string("");
    *ext = std::string("");

    QFileInfo qfile(origPath.c_str());
    *dirname = qfile.path().toStdString();
    *fname = qfile.fileName().toStdString();
    *ext = qfile.suffix().toStdString();
}

static bool checkmappath(char *file)
{
    if (!QFileInfo(file).exists())
    {
        std::string path, name, ext;
        std::string origPath = file;
        SplitPath(origPath, &path, &name, &ext);
        QFileInfo newFile(QDir::currentPath(), QString((ext.size() ? name + '.' + ext : name).c_str()));
        sprintf(file, "%s", newFile.absoluteFilePath().toAscii().constData());
    }
    //check against new path if it was updated
    if (QFileInfo(file).exists())
    {
        return true;
    }
    // alert user - maybe they can find the file
    std::string filter = "*" ;
    filter += file_extension(file);
    // trap ridiculous filters
    if (filter.size() == 2 || filter.size() > 8)
    {
        filter = "*.*";
    }
    std::string filter_line;
    filter_line = ::format("Map files (%s)|%s|All files (*.*)|*.*", filter.c_str(), filter.c_str());
    const std::string &s = MIFileSelector("A map file appears to have moved or been deleted: Can you find the map file?",
                                          "", file, file_extension(file), filter_line.c_str(), MI_OPEN_MODE);
    if (s.size())
    {
        strcpy(file, s.c_str());
        return true;
    }
    return false;
}


class MainItem : public QGraphicsItem
{
public:
    MainItem(MIGLWidget *widget, QGraphicsItem *parent = 0)
        : QGraphicsItem(parent), glWidget(widget)
    {
        setFlag(ItemStacksBehindParent);
        setFlag(ItemIgnoresTransformations);
        setFlag(ItemIgnoresParentOpacity);
        setZValue(-std::numeric_limits<qreal>::max());
    }

    void setSize(const QSizeF &size)
    {
        prepareGeometryChange();
        this->size = size;
    }

    virtual QRectF boundingRect() const
    {
        return QRectF(QPointF(), size);
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0)
    {
        Q_UNUSED(option)
        Q_UNUSED(widget)
        glWidget->draw(painter);
    }

    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event)
    {
        glWidget->handleMousePress(event);
    }

    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event)
    {
        glWidget->handleMouseMove(event);
    }

    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
    {
        glWidget->handleMouseRelease(event);
    }

    virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
    {
        glWidget->handleMouseDoubleClick(event);
    }

private:
    MIGLWidget *glWidget;
    QSizeF size;
};

MIGLWidget::MIGLWidget()
    : QDeclarativeView(QUrl("qrc:/qml/root.qml"))
{
    setViewport(new QGLWidget);
    setViewportUpdateMode(FullViewportUpdate);
    PaletteColor c = Application::instance()->GetBackgroundColor();
    QDeclarativeView::scene()->setBackgroundBrush(QColor(c.red, c.green, c.blue));

    setResizeMode(SizeRootObjectToView);
    mainItem = new MainItem(this, rootObject());

    SetTitle(""); // so that Modify won't complain about missing [*]

    newfile = 0;
    CurrentAnnotation = NULL;
    Models = new Displaylist;

    viewpoint = new ViewPoint;
    viewpointSettings = new ViewPointSettings;
    readViewPointSettings(viewpoint, viewpointSettings);

    frustum = new Frustum();
    camera = new Camera();
    stereoView = new StereoView(frustum, camera);
    QSettings settings;
    stereoView->setStereo(settings.value("View Parameters/stereo", false).toBool());
    frustum->setPerspective(stereoView->isStereo());
    stereoView->setHardwareStereo(settings.value("View Parameters/hardwareStereo", true).toBool());

    scene = new CMolwViewScene();
    scene->frustum = frustum;
    scene->camera = camera;
    scene->renderer->setFontSize(12);
    scene->renderer->setFrustum(frustum);
    scene->renderer->setCamera(camera);
    scene->renderer->setLineWidthDepthCued(viewpointSettings->isDepthCuedLineWidth());
    scene->renderer->setDimNonactiveModels(viewpointSettings->isDimNonactiveModels());
    scene->renderer->setAmountToDimNonactiveModels(viewpointSettings->getAmountToDimNonactiveModels());

    mousePicker = new MousePicker();
    atomPickingRenderable = new CMolwViewAtomPickingRenderable(stereoView);
    atomPickingRenderable->setFrustum(frustum);
    atomPickingRenderable->setCamera(camera);
    bondPickingRenderable = new CMolwViewBondPickingRenderable(stereoView);
    bondPickingRenderable->setFrustum(frustum);
    bondPickingRenderable->setCamera(camera);
    slabPickingRenderable = new CMolwViewSlabPickingRenderable(stereoView);

    float white[] = { 1.0f, 1.0f, 1.0f };
    targetFeedback = new TargetFeedback(white);

    BoundingBox = NULL;
    AutoSave = true;
    DragStart = false;
    MouseStillTime = 0;
    MouseInWindow = false;
    ToolTipInterval = settings.value("View Parameters/ToolTipInterval", 190).toInt();
    PentamerModel = PentamerFrom = NULL;
    PentamerStart = NULL;
    AutoFit = false;
    SurfaceCurrent = NO_SURFACE;
    batonatom = NULL;
    AtomStack = new Stack;
    IsFullScreen = false;
    SelectType = SINGLEATOM;
    Update = 1;
    Rock = Roll = 0;
    RockAngle = 0.0F;
    ClockTick = 100;
    TimerOn = false;
    RightMouseMode = CENTER;
    fitres = NULL;
    FitToken = 0;
    fitmol = NULL;
    fromcenter = NULL;
    focusres = NULL;
    focusresDeleted = false;
    focusnode = NULL;
    Torsioning = 0;
    TopView = false;
    doubleclicked = false;
    MouseCaptured = false;
    ShowLabels = settings.value("View Parameters/ShowLabels", true).toBool();
    ShowContacts = settings.value("View Parameters/ShowContacts", true).toBool();
    AtomStack->setVisible(settings.value("View Parameters/ShowStack", true).toBool());
    ShowGnomon = settings.value("View Parameters/ShowGnomon", true).toBool();
    showUnitCell = settings.value("View Parameters/ShowUnitCell", false).toBool();
    scene->ShowLabels = ShowLabels;
    scene->ShowContacts = ShowContacts;
    scene->ShowGnomon = ShowGnomon;
    scene->showUnitCell = showUnitCell;
    RockRange = settings.value("View Parameters/RockRange", 800).toInt()/100.0F;
    RockIncrement = settings.value("View Parameters/RockIncrement", 150).toInt()/100.0F;
    RollIncrement = settings.value("View Parameters/RollIncrement", 150).toInt()/100.0F;
    ClusterSize = 10;
    DontConfirmWater = settings.value("View Parameters/DontConfirmWater", true).toBool();
    Blink = false;
    BlinkTime = settings.value("View Parameters/BlinkTime", 20).toInt();
    BlinkCounter = 0;
    WhenShownColor = Colors::YELLOW;
    WhenShownColorMethod = Colors::COLORC;
    WhenShownRadius = 2;
    SaveColors = true;
    is_drawing = false;
    link_symm = false;

    rootContext()->setContextProperty("root", rootObject());
    rootContext()->setContextProperty("stack", AtomStack);
    QDeclarativeComponent stackComponent(engine(), QUrl("qrc:/qml/stack.qml"));
    stackItem = qobject_cast<QGraphicsObject *>(stackComponent.create());
    if (stackItem)
        stackItem->setParentItem(rootObject());

    rootContext()->setContextProperty("messageVisible", QVariant(false));
    rootContext()->setContextProperty("message", QVariant(""));
    QDeclarativeComponent messageComponent(engine(), QUrl("qrc:/qml/message.qml"));
    messageItem = qobject_cast<QGraphicsObject *>(messageComponent.create());
    if (messageItem)
        messageItem->setParentItem(rootObject());

    popup_menu = new QMenu(this);
    connect(popup_menu, SIGNAL(aboutToShow()), this, SLOT(updatePopupMenu()));

    QMenu *dotSurfMenu = new QMenu(this);
    dotSurfMenu->setTitle(tr("Dot Surface"));
    connect(dotSurfMenu, SIGNAL(aboutToShow()), this, SLOT(updateDotSurfMenu()));

    dotSurfVdwAction = dotSurfMenu->addAction(tr("van der Waal Surface"), this, SLOT(OnVdwDotSurface()));
    dotSurfSolventExposedAction = dotSurfMenu->addAction(tr("Solvent Exposed Surface"), this, SLOT(OnSurfaceSolvent()));
    dotSurfAtomSphereAction = dotSurfMenu->addAction(tr("Sphere around atom"), this, SLOT(OnObjectSurfaceSpherearoundatom()));
    dotSurfResidueAction = dotSurfMenu->addAction(tr("Surface Residue"), this, SLOT(OnObjectSurfaceresidue()));
    dotSurfResiduesAction = dotSurfMenu->addAction(tr("Surface Residues"), this, SLOT(OnObjectSurfaceresidues()));
    dotSurfAtomAction = dotSurfMenu->addAction(tr("Surface Atom"), this, SLOT(OnObjectSurfaceatom()));
    dotSurfAtomsAction = dotSurfMenu->addAction(tr("Surface Atoms"), this, SLOT(OnObjectSurfaceAtoms()));
    dotSurfClearAction = dotSurfMenu->addAction(tr("Clear Surface"), this, SLOT(OnObjectSurfaceClearsurface()));

    fitResidueAction = popup_menu->addAction(tr("Fit Residue"), this, SLOT(OnFitResidue()), QKeySequence(tr("F", "Fit residue")));
    rotateAction = popup_menu->addAction(tr("Rotate"), this, SLOT(OnFitRotate()));
    rotateAction->setCheckable(true);
    translateAction = popup_menu->addAction(tr("Translate"), this, SLOT(OnFitTranslate()));
    translateAction->setCheckable(true);
    setTorsionAction = popup_menu->addAction(tr("Set Torsion"), this, SLOT(OnFitSetuptorsion()));
    acceptFitAction = popup_menu->addAction(tr("Accept Fit"), this, SLOT(OnFitApply()), QKeySequence(tr(";", "Accept fit")));
    cancelFitAction = popup_menu->addAction(tr("Cancel Fit"), this, SLOT(OnFitCancel()));
    replaceAndFitAction = popup_menu->addAction(tr("Replace and Fit"), this, SLOT(OnFitReplaceAndFit()), QKeySequence(tr("R", "Replace and fit")));
    deleteResidueAction = popup_menu->addAction(tr("Delete residue"), this, SLOT(OnFitDeleteresidue()), QKeySequence(tr("D", "Delete residue")));
    deleteAtomAction = popup_menu->addAction(tr("Delete atom"), this, SLOT(OnDeleteAtom()), QKeySequence(QKeySequence::Delete));
    popup_menu->addSeparator();
    refineRegionAction = popup_menu->addAction(tr("Refine Local Region"), this, SLOT(OnRefiRegion()));
    acceptRefineAction = popup_menu->addAction(tr("Accept Refine"), this, SLOT(OnRefiAccept()));
    cancelRefineAction = popup_menu->addAction(tr("Cancel Refine"), this, SLOT(OnRefiCancel()));
    popup_menu->addSeparator();
    saveAction = popup_menu->addAction(tr("Save"), this, SLOT(OnFileSave()));
    fullscreenAction = popup_menu->addAction(tr("Toggle Fullscreen"), this, SLOT(OnFullScreen()));
    fullscreenAction->setCheckable(true);
    clearLabelsAction = popup_menu->addAction(tr("Clear Labels"), this, SLOT(OnEditClearlabels()));

    popup_menu->addMenu(dotSurfMenu);

    solidsurf_current_surface = UINT_MAX;
    solidsurf_popup_menu = newSolidsurfMenu();
    connect(solidsurf_popup_menu, SIGNAL(aboutToShow()), this, SLOT(updateSolidsurfMenu()));
    solidsurf_popup_menu->setTitle(tr("Solid Surface"));
    popup_menu->addMenu(solidsurf_popup_menu);

    DoingRange = 0;

    scene->renderer->setQGLWidget(static_cast<QGLWidget*>(viewport()));

    annotationPickingRenderable = new CMolwViewAnnotationPickingRenderable(stereoView, frustum, this);
    annotationPickingRenderable->setFontSize(12);

    //printf("\n");
    // OnSize();
    m_timer = startTimer(ClockTick);
    TimerOn = true;

    Displaylist *displaylist = GetDisplaylist();
    connect(displaylist, SIGNAL(modelAdded(Molecule*)),
            this, SLOT(modelAdded(Molecule*)));
    connect(displaylist, SIGNAL(currentMoleculeChanged(Molecule*, Molecule*)),
            this, SLOT(currentModelChanged(Molecule*, Molecule*)));
    Displaylist::ModelList::iterator modelIter = displaylist->begin();
    while (modelIter != displaylist->end())
    {
        Molecule *model = *modelIter;
        ++modelIter;
        connectToModel(model);
    }

    connect(displaylist, SIGNAL(mapAdded(EMap*)),
            this, SLOT(mapAdded(EMap*)));

    Displaylist::MapList &maps = displaylist->getMaps();
    Displaylist::MapList::iterator mapIter = maps.begin();
    while (mapIter != maps.end())
    {
        EMap *map = *mapIter;
        ++mapIter;
        connectToMap(map);
    }

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_DeleteOnClose);

    // special top-level widget for fullscreen window
    fullScreenWidget = new QWidget;
    fullScreenLayout = new QVBoxLayout(fullScreenWidget);
    fullScreenLayout->setMargin(0);
    fullScreenLayout->setSpacing(0);

    QVBoxLayout *labelLayout = new QVBoxLayout();
    QLabel *label = new QLabel(tr("Press Esc to exit fullscreen mode."), fullScreenWidget);
    label->setStyleSheet("color: white;  background-color: black;");
    label->setAlignment(Qt::AlignCenter);
    labelLayout->addWidget(label);
    fullScreenLayout->addLayout(labelLayout);
}

MIGLWidget::~MIGLWidget()
{

    ViewController::instance()->viewDeactivated(this);

    delete Models;

    delete frustum;
    delete camera;
    delete targetFeedback;
    delete scene;
    delete mousePicker;

    writeViewPointSettings(viewpoint, viewpointSettings);
    delete viewpointSettings;
    delete viewpoint;
    viewpoint = NULL;
    delete AtomStack;
    AtomStack = NULL;
    if (SaveModels.size() > 0)
    {
        for (unsigned int i = 0; i < SaveModels.size(); i++)
        {
            QFile::remove(SaveModels[i].path.c_str());
        }
    }

    MISurfaceSetCurrentView(this);
    unsigned int count = MISurfaceCount();
    for (unsigned int i = 0; i < count; ++i)
    {
        MIDeleteSurface(0);
    }

}





void MIGLWidget::connectToModel(Molecule *model)
{
    connect(model, SIGNAL(annotationDeleted(Molecule*)),
            this, SLOT(annotationDeleted(Molecule*)));
    connect(model, SIGNAL(annotationAdded(Molecule*, Annotation*)),
            this, SLOT(annotationAdded(Molecule*, Annotation*)));
    Molecule::AnnotationList::iterator iter = model->getAnnotations().begin();
    while (iter != model->getAnnotations().end())
    {
        Annotation *annotation = *iter;
        ++iter;
        connect(annotation, SIGNAL(annotationChanged(Annotation*)),
                this, SLOT(annotationChanged(Annotation*)));
    }
    connect(model, SIGNAL(atomLabelChanged(Molecule*, ATOMLABEL*)),
            this, SLOT(atomLabelChanged(Molecule*, ATOMLABEL*)));
    // TODO check correctness of this connection
    connect(model, SIGNAL(atomLabelDeleted(Molecule*)),
            this, SLOT(annotationDeleted(Molecule*)));
    connect(model, SIGNAL(surfaceChanged(Molecule*)),
            this, SLOT(surfaceChanged(Molecule*)));

    connect(model, SIGNAL(atomChanged(chemlib::MIMoleculeBase*, chemlib::MIAtomList&)),
            this, SLOT(atomChanged(chemlib::MIMoleculeBase*, chemlib::MIAtomList&)));
    connect(model, SIGNAL(atomsDeleted(chemlib::MIMoleculeBase*)),
            this, SLOT(atomsDeleted(chemlib::MIMoleculeBase*)));
    connect(model, SIGNAL(moleculeChanged(chemlib::MIMoleculeBase*)),
            this, SLOT(modelChanged(chemlib::MIMoleculeBase*)));

    connect(model, SIGNAL(atomsToBeDeleted(chemlib::MIMoleculeBase*, chemlib::MIAtomList)),
            this, SLOT(modelAtomsToBeDeleted(chemlib::MIMoleculeBase*, chemlib::MIAtomList)));
    connect(model, SIGNAL(residuesToBeDeleted(chemlib::MIMoleculeBase*, std::vector<chemlib::Residue*>&)),
            this, SLOT(modelResiduesToBeDeleted(chemlib::MIMoleculeBase*, std::vector<chemlib::Residue*>&)));
    connect(model, SIGNAL(moleculeToBeDeleted(chemlib::MIMoleculeBase*)),
            this, SLOT(moleculeToBeDeleted(chemlib::MIMoleculeBase*)));

    connect(model, SIGNAL(symmetryToBeCleared(chemlib::MIMoleculeBase*)),
            this, SLOT(symmetryToBeCleared(chemlib::MIMoleculeBase*)));
}

void MIGLWidget::symmetryToBeCleared(MIMoleculeBase *mol)
{
    // treat as if all symmetry residues are being deleted
    std::vector<Residue*> deaders;
    for (ResidueListIterator res = mol->symmResiduesBegin();
         res != mol->symmResiduesEnd(); ++res)
    {
        deaders.push_back(res);
    }
    if (deaders.size())
        modelResiduesToBeDeleted(mol, deaders);
}


void MIGLWidget::modelAtomsToBeDeleted(MIMoleculeBase *mol, const MIAtomList &atoms)
{
    MIAtom_const_iter iter;
    for (iter = atoms.begin(); iter != atoms.end(); ++iter)
    {
        Purge(*iter);
    }
    AtomStack->atomsToBeDeleted(mol, atoms);
}


void MIGLWidget::modelResiduesToBeDeleted(MIMoleculeBase *mol, std::vector<Residue*> &res)
{
    std::vector<Residue*>::iterator iter;

    // set focus residue to one not in deleted set
    if (focusres && std::find(res.begin(), res.end(), focusres) != res.end())
    {
        Residue *fr = focusres;
        while (fr && std::find(res.begin(), res.end(), fr) != res.end())
        {
            fr = fr->next();
        }
        setFocusResidue(fr, false, true);
    }

    for (iter = res.begin(); iter != res.end(); ++iter)
    {
        Purge(*iter);
    }
    AtomStack->residuesToBeDeleted(mol, res);
}

void MIGLWidget::moleculeToBeDeleted(MIMoleculeBase *model)
{
    Logger::debug("MIGLWidget::deleting molecule");

    Molecule *mol = dynamic_cast<Molecule*>(model);
    if (mol)
    {
        Purge(mol);
    }
    AtomStack->moleculeToBeDeleted(model);
}

void MIGLWidget::doRefresh()
{
    QDeclarativeView::scene()->invalidate();
    QDeclarativeView::scene()->update();
    update();
    MIMainWindow::instance()->updateToolBar();
    MIMainWindow::instance()->updateNavigator();

#ifndef MIFIT_NO_RAMAPLOT
    if (RamaPlotMgr::instance()->IsShown())
        updateRamachandranPlot();
#endif
}

void MIGLWidget::modelChanged(MIMoleculeBase*)
{
    doRefresh();
}

void MIGLWidget::modelAdded(Molecule *model)
{
    connectToModel(model);
}

void MIGLWidget::currentModelChanged(Molecule*, Molecule *newModel)
{
    std::string text;
    if (newModel != NULL)
    {
        text = newModel->pathname;
    }
    MIMainWindowRightFooter(text.c_str());

    doRefresh();
}

void MIGLWidget::connectToMap(EMapBase *emap)
{
    connect(emap, SIGNAL(mapContourLevelsChanged(EMapBase*)),
            this, SLOT(mapContourLevelsChanged(EMapBase*)));
    connect(emap, SIGNAL(mapVisibilityChanged(EMapBase*)),
            this, SLOT(mapVisibilityChanged(EMapBase*)));
}

void MIGLWidget::mapAdded(EMap *emap)
{
    connectToMap(emap);
}

void MIGLWidget::annotationAdded(Molecule*, Annotation *annotation)
{
    connect(annotation, SIGNAL(annotationChanged(Annotation*)),
            this, SLOT(annotationChanged(Annotation*)));
    doRefresh();
}

void MIGLWidget::annotationDeleted(Molecule*)
{
    doRefresh();
}

void MIGLWidget::annotationChanged(Annotation*)
{
    doRefresh();
}

void MIGLWidget::atomLabelChanged(Molecule*, ATOMLABEL*)
{
    doRefresh();
}

void MIGLWidget::surfaceChanged(Molecule*)
{
    doRefresh();
}

void MIGLWidget::atomChanged(MIMoleculeBase*, MIAtomList&)
{
    doRefresh();
}

void MIGLWidget::atomsDeleted(MIMoleculeBase*)
{
    doRefresh();
}

void MIGLWidget::OnActivated()
{
    ViewController::instance()->viewActivated(this);
    Molecule *model = GetDisplaylist()->GetCurrentModel();
    if (model != NULL)
    {
        MIMainWindowRightFooter(model->pathname.c_str());
    }
    else
    {
        MIMainWindowRightFooter("");
    }

    MIMainWindow::instance()->updateNavigator();
#ifndef MIFIT_NO_RAMAPLOT
    updateRamachandranPlot();
#endif

    //NOTE: viewDeactivated is no longer sent (now handled by OnClose)
}

/////////////////////////////////////////////////////////////////////////////
// MIGLWidget drawing
void MIGLWidget::ReDraw()
{
    CheckCenter();
    mainItem->update();
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    MIMainWindow::instance()->updateNavigator();
}

void MIGLWidget::CheckCenter()
{
    Displaylist *Models = GetDisplaylist();
    if (!Models)
    {
        return;
    }
    if (viewpoint == NULL)
    {
        return;
    }
    for (int i = 0; i < Models->MapCount(); i++)
    {
        Models->GetMap(i)->CheckCenter(viewpoint->center()[0], viewpoint->center()[1], viewpoint->center()[2]);
    }
    std::list<Molecule*>::iterator node = Models->begin();
    std::list<Molecule*>::iterator end = Models->end();
    while (node != end)
    {
        if ((*node)->CheckCenter(viewpoint->center()[0], viewpoint->center()[1], viewpoint->center()[2]))
        {
            if ((*node)->symmResiduesBegin() != (*node)->symmResiduesEnd())
            {
                clearSymmAtoms();
            }
            (*node)->GenSymmAtoms(viewpoint);
            if (link_symm)
            {
                (*node)->SymmLink();
            }
        }
        node++;
    }
}


void MIGLWidget::draw(QPainter *painter)
{
    painter->save();
    painter->beginNativePainting();
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    is_drawing = true;
    Displaylist *Models = GetDisplaylist();

    if (NewFile())
    {
        ResetNewFile();

        if (Models->CurrentItem())
        {
            Molecule *model = Models->CurrentItem();
            //  new file - center on current (i.e new) object
            if (Models->CurrentItem()->modelnumber == 0)
            {
                // no number if not from .mlw file
                if (Models->NumberItems() == 1)
                {
                    std::string resshow;
                    std::string at("*");
                    model->Select(1, 1, 1, 1, resshow, at, NULL, NULL, 0, 0, 0, 0, 1);
                    float x, y, z;
                    Models->GetCurrentModel()->Center(x, y, z);
                    viewpoint->setCenter(Vector3<float>(x, y, z));
                    viewpoint->setSlab(-3.0F, 3.0F);
                    viewpoint->setScale(30);
                }
            }
            PaletteChanged = true;
        }
        CheckCenter();
        MIMainWindow::instance()->updateNavigator();
    }

    scene->initializeForRender();

    //float maxDistance = 10.0f;
    float focalLength = 50.0f;

    QSettings settings;
    bool stereo = settings.value("View Parameters/stereo", false).toBool();
    if (!stereo && equals(viewpoint->perspective(), 0.0f))
        frustum->setPerspective(false);
    else
    {
        frustum->setPerspective(true);
        focalLength -= 10.0f * viewpoint->perspective();
    }
    double fieldOfView = toDegrees(2.0 * std::atan(viewpoint->getheight() * 0.5 / focalLength));
    frustum->setFieldOfView(fieldOfView);
    frustum->setFocalLength(focalLength);
    frustum->setNearClipping(focalLength - viewpoint->frontClip());
    frustum->setFarClipping(focalLength - viewpoint->backClip());

    stereoView->setStereo(stereo);

    int width = size().width();
    int height = size().height();
    stereoView->setSize(width, height);
    frustum->updateFrustum(*stereoView->getViewport());

    if (TopView)
    {
        Vector3<float> *corners = frustum->getCorners();
        scene->setCorners(corners);
        slabPickingRenderable->setCorners(corners);
        frustum->setNearClipping(focalLength - frustum->getFrustumHeight(focalLength));
        frustum->setFarClipping(focalLength + frustum->getFrustumHeight(focalLength));
    }

    Vector3<float> target(viewpoint->center()[0], viewpoint->center()[1], viewpoint->center()[2]);
    Quaternion<float> q;
    q.set(viewpoint->orientation());
    q.normalize();
    q.inverse();
    static Quaternion<float> frontQuat(Vector3<float>(1.0f, 0.0f, 0.0f), toRadians(180.0f));
    static Quaternion<float> topQuat(Vector3<float>(1.0f, 0.0f, 0.0f), toRadians(90.0f));
    if (TopView)
    {
        Quaternion<float> q2(q);
        q2.multiply(frontQuat);
        scene->frontCamera->setEye(target);
        scene->frontCamera->setRotation(q2);
        Vector3<float> eye(scene->frontCamera->getTarget(-frustum->getFocalLength()));
        scene->frontCamera->setEye(eye);
        slabPickingRenderable->setViewpoint(*scene->frontCamera);
        q.multiply(topQuat);
    }
    else
        q.multiply(frontQuat);

    camera->setEye(target);
    camera->setRotation(q);
    Vector3<float> eye(camera->getTarget(-frustum->getFocalLength()));
    camera->setEye(eye);

    float heightAtTarget = frustum->getFrustumHeight(frustum->getFocalLength());
    float glUnitsPerPixel = heightAtTarget / (float) stereoView->getViewport()->getHeight();
    scene->glUnitsPerPixel = glUnitsPerPixel;
    scene->renderer->updateTextScale(glUnitsPerPixel);
    annotationPickingRenderable->updateTextScale(glUnitsPerPixel);

    scene->models = Models;
    scene->pentamerModel = PentamerModel;
    scene->atomStack = AtomStack;
    scene->viewpoint = viewpoint;
    scene->viewpointSettings = viewpointSettings;
    scene->torsionArrow = TorsionArrow;
    scene->topView = TopView;
    scene->renderer->setViewVector(camera->getViewVector());
    scene->showSymmetryAsBackbone = link_symm;
    MISurfaceSetCurrentView(this);
#if SHOW_RENDER_TIME
    time.start();
#endif
    stereoView->render(*scene);
#if SHOW_RENDER_TIME
    int elapsedTime = time.elapsed();
#endif

    //  prepareAtomPicking();
    //  stereoView->render(*atomPickingRenderable);
    //  prepareBondPicking();
    //  stereoView->render(*bondPickingRenderable);
    //  annotationPickingRenderable->setModels(Models);
    //  stereoView->render(*annotationPickingRenderable);

    // cleanup
    Update = 0;

#if SHOW_RENDER_TIME
    QString framesPerSecond = QString("render in %1 ms")
                              .arg(elapsedTime);
    Logger::debug(framesPerSecond.toAscii().constData());
    qDebug() << qPrintable(framesPerSecond);
#endif

    is_drawing = false;
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
    painter->endNativePainting();
    painter->restore();
}

/////////////////////////////////////////////////////////////////////////////
// MIGLWidget printing

void MIGLWidget::OnPrint()
{
    scene->renderer->setPickingEnabled(true);
    QPixmap imageToPrint = static_cast<QGLWidget*>(viewport())->renderPixmap(); //TODO: could alter canvas size here
    scene->renderer->setPickingEnabled(false);

    QPrinter prn;
    QPrintDialog printDialog( &prn, this );
    if (printDialog.exec())
    {
        QPainter painter(&prn);
        QRect rect = painter.viewport( );
        QSize size = imageToPrint.size( );

        size.scale( rect.size( ), Qt::KeepAspectRatio );
        painter.setViewport( rect.x( ), rect.y( ), size.width( ), size.height( ) );
        painter.setWindow( imageToPrint.rect( ) );
        painter.drawImage( 0, 0, imageToPrint.toImage( ) );
    }
}

static bool IsWindowEnabled()
{
    return true;
}

static bool DraggingSlab = false;
static bool DraggingRotate = false;

void MIGLWidget::OnMouseMove(QPoint point, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers)
{
    MouseStillTime = 0;
    MouseInWindow = true;
    if (Application::instance()->GetXfitMouseMode())
    {
        MouseMoveXfit(point, buttons, modifiers);
        return;
    }
    QPoint d;
    float xang = 0.0F, yang = 0.0F, zang = 0.0F;
    if (isVisible() && IsWindowEnabled() && MouseCaptured)
        d = point - mouse;
    else
        d = QPoint();

    if (modifiers & Qt::ControlModifier)
    {
        // restrict movement to x or y whichever is greater
        if (abs(d.x()) > abs(d.y()))
            d.ry() = 0;
        else
            d.rx() = 0;
    }
    if (d.x() != 0 || d.y() != 0)
    {
        if (DragStart == false)
        {
            viewpoint->Do();
            DragStart = true;
        }
    }
    if (buttons & Qt::LeftButton
        && !(buttons&Qt::RightButton)
        && (d.x() != 0 || d.y() != 0))
    {
        // Left Mouse Drag
        // 1 pixel to 1 degree mouse movement for now...
        // Check if shift down - zoom if it is
        // Check for clip plane drag if in Top View mode
        if (TopView && !DraggingRotate)
        {
            doSlabDrag(point.x(), point.y(), d.x(), d.y());
        }
        if (modifiers & Qt::ShiftModifier && !DraggingSlab)
        {
            SetCursor(imhScale);
            zang = 1.0F - (float)d.y()*0.01F;
            viewpoint->zoom(zang);
        }
        else if (buttons & Qt::LeftButton && !DraggingSlab)
        {
            DraggingRotate = true;
            if (mousestart.y() < 30)
            {
                SetCursor(imhZRotate);
                if (!TopView)
                    zang = (float)-d.x();
                else
                    yang = (float)-d.x();
            }
            else
            {
                SetCursor(imhRotate);
                if (!TopView)
                {
                    xang = (float)d.y();
                    yang = (float)-d.x();
                }
                else
                {
                    xang = (float)d.y();
                    zang = (float)-d.x();
                }
            }
            viewpoint->rotate(xang, yang, zang);
        }
        ReDraw();
    }
    else if (buttons & Qt::RightButton && !(buttons & Qt::LeftButton)
             && (d.x() != 0 || d.y() != 0))
        RightMouseDrag(d, xang, yang, zang);
    else
    {
        if (point.y() > 30)
            SetCursor(imhCross);
        else
            SetCursor(imhZCursor);
    }
    mouse = point;
}

void MIGLWidget::RightMouseDrag(QPoint d, float xang, float yang, float zang)
{
    // Right Mouse Drag
    switch (RightMouseMode)
    {
    case TRANSLATE:
        SetCursor(imhTranslate);
        if (IsFitting())
        {
            Vector3<float> dir(mapToCamera(d));
            fitmol->Translate(dir.x, dir.y, dir.z, &CurrentAtoms);
            UpdateCurrent();
            fitcenter.translate(dir.x, dir.y, dir.z);
        }
        break;
    case ROTATE:
        SetCursor(imhRotate);
        if (IsFitting())
        {
            if (mousestart.y() < 30)
            {
                zang = (float)-d.x();
                xang = (float)d.y();
                yang = 0.0F;
            }
            else
            {
                xang = (float)d.y();

                yang = (float)-d.x();
                zang = 0.0F;
            }
            fitmol->Rotate(xang/3.0F, yang/3.0F, zang/3.0F, fitcenter.x(),
                           fitcenter.y(), fitcenter.z(), viewpoint, &CurrentAtoms);
            UpdateCurrent();
        }
        break;
    case BONDTWIST:
        SetCursor(imhTorsion);
        if (Torsioning && IsFitting())
        {
            fitmol->RotateTorsion((float)d.x());
            char torval[100];
            if (fitmol->GetTorsionValue(torval, fitres))
                setMessage(torval);
            UpdateCurrent();
        }
        break;
    case CENTER:
    default:
    {
        int x = 0, y = 0, z = 0;
        if (!TopView)
        {
            if (mousestart.y() < 30)
            {
                SetCursor(imhSlabDrag);
                viewpoint->changeSlab(d.x());
                break;
            }
            else
            {
                SetCursor(imhCenter);
                x = d.x();
                y = d.y();
            }
        }
        else
        {
            SetCursor(imhCenter);
            x = d.x();
            z = -d.y();
        }
        viewpoint->moveBy(-mapToCamera(QPoint(x, y), z));
        break;
    }
    }
    ReDraw();
}

void MIGLWidget::MouseMoveXfit(QPoint point, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers)
{
    QPoint d;
    float xang = 0.0F, yang = 0.0F, zang = 0.0F;
    if (isVisible() && IsWindowEnabled() && MouseCaptured)
        d = point - mouse;
    else
        d = QPoint();

    if (modifiers & Qt::ControlModifier)
    {
        // restrict movement to x or y whichever is greater
        if (abs(d.x()) > abs(d.y()))
            d.ry() = 0;
        else
            d.rx() = 0;
    }
    if (d.x() != 0 || d.y() != 0)
    {
        if (DragStart == false)
        {
            viewpoint->Do();
            DragStart = true;
        }
    }
    if (buttons & Qt::LeftButton
        && !(buttons&Qt::RightButton)
        && !(buttons&Qt::MiddleButton)
        && (d.x() != 0 || d.y() != 0))
    {
        // Left Mouse Drag
        // 1 pixel to 1 degree mouse movement for now...
        // Check if shift down - zoom if it is
        // Check for clip plane drag if in Top View mode
        if (TopView && !DraggingRotate)
        {
            doSlabDrag(point.x(), point.y(), d.x(), d.y());
        }
        if (buttons & Qt::LeftButton && !DraggingSlab)
        {
            DraggingRotate = true;
            if (mousestart.y() < 30)
            {
                SetCursor(imhZRotate);
                if (!TopView)
                    zang = (float)-d.x();
                else
                    yang = (float)-d.x();
            }
            else
            {
                SetCursor(imhRotate);
                if (!TopView)
                {
                    xang = (float)d.y();
                    yang = (float)-d.x();
                }
                else
                {
                    xang = (float)d.y();
                    zang = (float)-d.x();
                }
            }
            viewpoint->rotate(xang, yang, zang);
        }
        ReDraw();
    }
    else if ((buttons & Qt::MiddleButton) && (buttons & Qt::LeftButton) && !(buttons & Qt::RightButton)
             && (d.x() != 0 || d.y() != 0))
    {
        SetCursor(imhScale);
        zang = 1.0F - (float)d.y()*0.01F;
        viewpoint->zoom(zang);
        ReDraw();
    }
    else if ((buttons & Qt::MiddleButton) && !(buttons & Qt::LeftButton) && !(buttons & Qt::RightButton)
             && (d.x() != 0 || d.y() != 0))
        RightMouseDrag(d, xang, yang, zang);
    else
    {
        if (point.y() > 30)
            SetCursor(imhCross);
        else
            SetCursor(imhZCursor);
    }
    mouse = point;
}

void MIGLWidget::OnLButtonDblClk()
{
    // Double click - center on double clicked atom
    // Note that two of these atoms are on the stack
    // use one for centering - leave the other
    if (!TopView)
    {
        Residue *res;
        Molecule *node;
        MIAtom *a;
        AtomStack->Pop(a, res, node);
        recenter(res, a);
    }
    doubleclicked = true;
}

void MIGLWidget::recenter(Residue *residue, MIAtom *atom)
{
    if (!TopView && residue != NULL)
    {
        if (atom != NULL)
        {
            if (atom == atom_from_name("CA", *residue))
            {
                viewpoint->Do();
                CenterAtResidue(residue);
            }
            else
            {
                viewpoint->Do();
                ViewPointAnimation *animation = new ViewPointAnimation(viewpoint, this);
                animation->setCenterEnd(atom->position());
                animation->start(QAbstractAnimation::DeleteWhenStopped);
                connect(animation, SIGNAL(finished()), this, SLOT(CheckCenter()));
            }
        }
        else
        {
            viewpoint->Do();
            CenterAtResidue(residue);
        }
        setFocusResidue(residue, false);
        doRefresh();
    }
}

void MIGLWidget::CenterAtResidue(const Residue *res)
{
    if (!Monomer::isValid(res))
        return;

    ViewPointAnimation *animation = new ViewPointAnimation(viewpoint, this);
    MIAtom *CA = atom_from_name("CA", *res);
    if (CA)
    {
        MIAtom *a2 = atom_from_name("CB", *res);
        if (!a2)
            a2 = atom_from_name("HA1", *res);

        MIAtom *N = atom_from_name("N", *res);
        animation->setCenterEnd(CA->position());
        if (a2)
        {
            Quaternion<float> orientation;
            Vector3<float> vector1(a2->position() - CA->position());
            if (N)
            {
                // Orient CB to top and N to left
                Vector3<float> vector2(N->position() - CA->position());
                Vector3<float> normalToCBAndN;
                normalToCBAndN.cross(vector1, vector2);
                Quaternion<float> viewQuat = QuatUtil::alignVectors(normalToCBAndN, Vector3<float>(0.0, 0.0, -1.0));
                Vector3<float> vector1b = viewQuat.rotate(vector1);
                orientation = QuatUtil::alignVectors(vector1b, Vector3<float>(0.0, -1.0, 0.0));
                orientation.multiply(viewQuat);
            } else
                // Orient CB to top
                orientation = QuatUtil::alignVectors(vector1, Vector3<float>(0.0, -1.0, 0.0));

            animation->setViewEnd(orientation);
        }
    }
    else
    {
        float x = 0, y = 0, z = 0;
        for (int i = 0; i < res->atomCount(); i++)
        {
            x += res->atom(i)->x();
            y += res->atom(i)->y();
            z += res->atom(i)->z();
        }
        x /= res->atomCount();
        y /= res->atomCount();
        z /= res->atomCount();
        animation->setCenterEnd(Vector3<float>(x, y, z));
    }
    connect(animation, SIGNAL(finished()), this, SLOT(CheckCenter()));
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

bool MIGLWidget::OnKeyDown(unsigned int nChar, Qt::KeyboardModifiers modifiers)
{
    if (MIBusyManager::instance()->Busy())
    {
        return false;
    }
    static float angle = 3.0F;
    static float fineangle = 1.0F;
    static float scale = 1.1F;
    static int inc = 0;
    bool handled = false;
    bool shift = (modifiers & Qt::ShiftModifier) != 0;
    bool control = (modifiers & Qt::ControlModifier) != 0;

    bool xfit_mouse = Application::instance()->GetXfitMouseMode();
    switch (nChar)
    {
    case '1':
        if (IsFitting())
        {
            FitTorsion("CHI1");
        }
        handled = true;
        break;
    case '2':
        if (IsFitting())
        {
            FitTorsion("CHI2");
        }
        handled = true;
        break;
    case '3':
        if (IsFitting())
        {
            FitTorsion("CHI3");
        }
        handled = true;
        break;
    case '4':
        if (IsFitting())
        {
            FitTorsion("CHI4");
        }
        handled = true;
        break;
    case '5':
        if (IsFitting())
        {
            FitTorsion("CHI5");
        }
        handled = true;
        break;
    case '6':
        if (IsFitting())
        {
            FitTorsion("CHI6");
        }
        handled = true;
        break;
    case '7':
        if (IsFitting())
        {
            FitTorsion("CHI7");
        }
        handled = true;
        break;
    case '8':
        if (IsFitting())
        {
            FitTorsion("CHI8");
        }
        handled = true;
        break;
    case '9':
        if (IsFitting())
        {
            if (!shift)
            {
                FitTorsion("CHI9");
            }
            else
            {
                FitTorsion("PHI");
            }
        }
        handled = true;
        break;
    case '0':
        if (IsFitting())
        {
            if (shift)
            {
                FitTorsion("PSI");
            }
        }
        handled = true;
        break;
    case '(':
        if (IsFitting())
        {
            FitTorsion("PHI");
        }
        handled = true;
        break;
    case ')':
        if (IsFitting())
        {
            FitTorsion("PSI");
        }
        handled = true;
        break;
    case 'a': // Decrease stereo angle
        if (stereoView->isStereo())
        {
            stereoView->setEyeSeparation(stereoView->getEyeSeparation() - 0.1);
            Logger::log("stereo eye separation: %0.1f", stereoView->getEyeSeparation());
            ReDraw();
        }
        handled = true;
        break;
    case 'A':
        if (stereoView->isStereo())
        {
            stereoView->setImageSeparation(stereoView->getImageSeparation() - 0.1);
            Logger::log("stereo image separation: %0.1f", stereoView->getImageSeparation());
        }
        handled = true;
        break;

    case 'b': // back one residue
        if ((focusres != NULL) && focusnode != NULL)
        {
            Residue *prev = focusres->prev();
            if (prev != NULL)
            {
                setFocusResidue(prev);
            }
        }
        handled = true;
        break;
    case 'c': // next conformer
        if (control)
        {
            OnEditCopy();
        }
        else
        {
            OnFitNextConfomer();
        }
        handled = true;
        break;
    case 'D':
    case 'd':
        OnFitDeleteresidue();
        handled = true;
        break;
    case 'e':
        handled = true;
        break;
    case 'f':
        fitResidue();
        handled = true;
        break;
    case 'I':
        OnViewSlabin();
        handled = true;
        break;
    case 'i':
        viewpoint->zoom(scale);
        handled = true;
        break;
    case 'H':
    case 'h':
        handled = true;
        break;
    case 'O':
        OnViewSlabout();
        handled = true;
        break;
    case 'o':
        viewpoint->zoom(1.0F/scale);
        handled = true;
        break;
    case 'R':
        if (IsFitting())
        {
            OnRefiRigidBody();
            refineConformer();
            OnRefiRigidBody();
            refineConformer();
            handled = true;
        }
    case 'r':
        if (control)
        {
            OnRefiResidue();
            handled = true;
        }
        else if (IsFitting())
        {
            OnRefiRigidBody();
            refineConformer();
            handled = true;
        }
        else
        {
            if (!AtomStack->empty())
            {
                replaceAndFitResidueWithPrompt();
                handled = true;
            }
        }
        break;
    case 's': // Increase stereo angle
        if (stereoView->isStereo())
        {
            stereoView->setEyeSeparation(stereoView->getEyeSeparation() + 0.1);
            Logger::log("stereo eye separation: %0.1f", stereoView->getEyeSeparation());
            ReDraw();
            handled = true;
        }
        break;
    case 'S':
        if (stereoView->isStereo())
        {
            if (control)
            {
                stereoView->setCrossEyed(!stereoView->isCrossEyed());
                if (stereoView->isCrossEyed())
                {
                    Logger::log("cross-eyed stereo on");
                }
                else
                {
                    Logger::log("wall-eyed stereo on");
                }
            }
            else
            {
                stereoView->setImageSeparation(stereoView->getImageSeparation() + 0.1);
                Logger::log("stereo image separation: %0.1f", stereoView->getImageSeparation());
            }
        }
        handled = true;
        break;

    case 'W': // add a water at cursor
        OnAddWaterAtCursor();
        handled = true;
        break;
    case 'x': {
        const float halfSqrt2 = std::sqrt(2)/2;
        viewpoint->setView(Quaternion<float>(0, -halfSqrt2, 0, halfSqrt2));
        handled = true;
    }
    break;
    case 'y': {
        const float halfSqrt2 = std::sqrt(2)/2;
        viewpoint->setView(Quaternion<float>(halfSqrt2, 0, 0, halfSqrt2));
        handled = true;
    }
    break;
    case 'z': {
        viewpoint->setView(Quaternion<float>(0.0f, 0.0f, 0.0f, 1.0f));
        handled = true;
    }
    break;
    case '[':
    case 312: // page up
        OnGotoNter();
        handled = true;
        break;
    case ']':
    case 313: // page down
        OnGotoCter();
        handled = true;
        break;
    case ';':
        OnFitApply();
        handled = true;
        break;
    case '>':
        //1if(focusres)
        InsertMRK(focusres, focusnode, 0);
        handled = true;
        break;
    case '<':
        if (focusres)
        {
            InsertMRK(focusres, focusnode, 1);
        }
        handled = true;
        break;
    case '|':
    case 92:
    {
        OnStereoToggle();
        handled = true;
    }
    break;
    case ' ':
        handleKey_space(true);
        handled = true;
        break;
    //to do add xfit mode for arrows
    case Qt::Key_Left: // left arrow
        if (xfit_mouse && IsFitting())
        {
            if (shift)
            {
                RightMouseDrag(QPoint(-1, 0), 0.0F, fineangle, 0.0F);
            }
            else
            {
                RightMouseDrag(QPoint(-10, 0), 0.0F, angle, 0.0F);
            }
        }
        else
        {
            if (control)
            {
                if (shift)
                {
                    RightMouseDrag(QPoint(-1, 0), 0.0F, fineangle, 0.0F);
                }
                else
                {
                    RightMouseDrag(QPoint(-10, 0), 0.0F, angle, 0.0F);
                }
            }
            else
            {
                if (shift)
                {
                    viewpoint->rotate(0.0F, fineangle, 0.0F);
                }
                else
                {
                    viewpoint->rotate(0.0F, angle, 0.0F);
                }
            }
        }
        ReDraw();
        handled = true;
        break;
    case Qt::Key_Up:  // up arrow
        if (xfit_mouse && IsFitting())
        {
            if (shift)
            {
                RightMouseDrag(QPoint(0, -2), -fineangle, 0.0F, 0.0F);
            }
            else
            {
                RightMouseDrag(QPoint(0, -2), -angle, 0.0F, 0.0F);
            }
        }
        else
        {
            if (control && shift)
            {
                viewpoint->zoom(1.0F + 2.0F*0.01F);
            }
            else if (control)
            {
                RightMouseDrag(QPoint(0, -2), -angle, 0.0F, 0.0F);
            }
            else
            {
                if (shift)
                {
                    viewpoint->rotate(-fineangle, 0.0F, 0.0F);
                }
                else
                {
                    viewpoint->rotate(-angle, 0.0F, 0.0F);
                }
            }
        }
        ReDraw();
        handled = true;
        break;
    case Qt::Key_Right: //right arrow
        if (xfit_mouse && IsFitting())
        {
            if (shift)
            {
                RightMouseDrag(QPoint(1, 0), 0.0F, -fineangle, 0.0F);
            }
            else
            {
                RightMouseDrag(QPoint(10, 0), 0.0F, -angle, 0.0F);
            }
        }
        else
        {
            if (control)
            {
                if (shift)
                {
                    RightMouseDrag(QPoint(1, 0), 0.0F, -fineangle, 0.0F);
                }
                else
                {
                    RightMouseDrag(QPoint(10, 0), 0.0F, -angle, 0.0F);
                }
            }
            else
            {
                if (shift)
                {
                    viewpoint->rotate(0.0F, -fineangle, 0.0F);
                }
                else
                {
                    viewpoint->rotate(0.0F, -angle, 0.0F);
                }
            }
        }
        ReDraw();
        handled = true;
        break;
    case Qt::Key_Down: // down arrow
        if (xfit_mouse && IsFitting())
        {
            if (shift)
            {
                RightMouseDrag(QPoint(0, 2), fineangle, 0.0F, 0.0F);
            }
            else
            {
                RightMouseDrag(QPoint(0, 2), angle, 0.0F, 0.0F);
            }
        }
        else
        {
            if (control && shift)
            {
                viewpoint->zoom(1.0F - 2.0F*0.01F);
            }
            else if (control)
            {
                RightMouseDrag(QPoint(0, 2), angle, 0.0F, 0.0F);
            }
            else
            {
                if (shift)
                {
                    viewpoint->rotate(fineangle, 0.0F, 0.0F);
                }
                else
                {
                    viewpoint->rotate(angle, 0.0F, 0.0F);
                }
            }
        }
        ReDraw();
        handled = true;
        break;
    case Qt::Key_Plus: // =/+ key
        inc++;
        if (inc > 1)
        {
            inc = 1;
        }
        handled = true;
        break;
    case Qt::Key_Minus: // -/_ key
        inc--;
        if (inc < -1)
        {
            inc = -1;
        }
        handled = true;
        break;
#ifndef __WINDOWS__
    case Qt::Key_Escape: // escape
        OnFullScreen();
        handled = true;
        break;
#endif
    case Qt::Key_Delete:
        deleteAtom();
        handled = true;
        break;
    }
    if (inc == 0)
    {
        angle = 3.0F;
        scale = 1.1F;
    }
    else if (inc > 0)
    {
        angle = 10.0F;
        scale = 1.3F;
    }
    else
    {
        angle = 0.5F;
        scale = 1.03F;
    }

    return handled;
}

void MIGLWidget::timerEvent(QTimerEvent*)
{
    static long ticks;
    if (Roll)
    {
        viewpoint->rotate(0.0F, RollIncrement, 0.0F);
        Update = 1;
    }
    if (Rock)
    {
        if (RockAngle >= RockRange)
        {
            RockIncrement = (float)-fabs(RockIncrement);
        }
        if (RockAngle <= -RockRange)
        {
            RockIncrement = (float)fabs(RockIncrement);
        }
        RockAngle += RockIncrement;
        viewpoint->rotate(0.0F, RockIncrement, 0.0F);

        Update = 1;
    }
    if (Blink && ticks%5 == 0)
    {
        Displaylist *Models = GetDisplaylist();
        if (Models->NumberItems() > 1)
        {
            BlinkCounter += 2;
            if (BlinkCounter >= BlinkTime)
            {
                BlinkCounter = 0;
                Update = 1;
                Molecule *first = Models->FirstItem();
                if (first != NULL)
                {
                    if (first->Visible())
                    {
                        first->Hide();
                    }
                    else
                    {
                        first->Show();
                    }
                    std::list<Molecule*>::iterator node1 = Models->begin();
                    node1++;
                    Molecule *second = *node1;
                    if (second != NULL)
                    {
                        if (first->Visible())
                        {
                            second->Hide();
                        }
                        else
                        {
                            second->Show();
                        }
                    }
                }
            }
        }
    }
    if (MouseInWindow)
    {
        MouseStillTime += ClockTick;
        if (MouseStillTime > ToolTipInterval)
        {
            OnToolTip();
            /*if(navigator) {
               navigator->ReDraw();
               }*/
        }
    }
    // auto-save the model every 5 minutes
    if (AutoSave && ticks%3000 == 0)
    {
        Molecule *model = GetDisplaylist()->GetCurrentModel();
        if (model)
        {
            if (model->IsCoordsChanged())
            {
                SaveModelFile(model, "Auto checkpoint");
                model->SetCoordsChanged(false);
            }
        }
    }
    ticks++;
    if (ticks >= INT_MAX)
    {
        ticks = 0;
    }
    if (Update)
    {
        ReDraw();
        // to let the event queue catch up if Draw takes a long time
#ifdef _WIN32
        Sleep(10);
#else
        usleep(10000);
#endif
    }
}


void MIGLWidget::resizeEvent(QResizeEvent *evt)
{
    int sx = evt->size().width();
    int sy = evt->size().height();
    mainItem->setSize(evt->size());

#ifdef __APPLE__
    QSettings settings;
    settings.setValue("ThreeDView/width", sx);
    settings.setValue("ThreeDView/height", sy);
#endif

    if (sx == 0 || sy == 0)
    {
        QDeclarativeView::resizeEvent(evt);
        return;
    }
    viewpoint->setSize(sx, sy);
    MIMainWindow::instance()->updateNavigator();
    QDeclarativeView::resizeEvent(evt);
}

void MIGLWidget::OnStereoToggle()
{
    GLboolean b;
    glGetBooleanv(GL_STEREO, &b);
    Logger::debug("gl stereo %d", (b == GL_TRUE));

    Application::instance()->toggleStereo();
    QSettings settings;
    bool stereo = settings.value("View Parameters/stereo", false).toBool();
    stereoView->setStereo(stereo);
    if (stereoView->isStereo())
    {
        if (stereoView->isHardwareStereo())
        {
            Logger::log("hardware stereo on");
        }
        else if (stereoView->isCrossEyed())
        {
            Logger::log("cross-eyed stereo on");
        }
        else
        {
            Logger::log("wall-eyed stereo on");
        }
    }
    else
    {
        Logger::log("stereo off");
    }
    ReDraw();
}

void MIGLWidget::OnHardwareStereo()
{
    Application::instance()->toggleHardwareStereo();
    doRefresh();
}

void MIGLWidget::OnDecreasePersp()
{
    viewpoint->setPerspective(viewpoint->perspective()-0.1F);
    doRefresh();
}

void MIGLWidget::OnIncreasePersp()
{
    viewpoint->setPerspective(viewpoint->perspective()+0.1F);
    doRefresh();
}

void MIGLWidget::OnViewOrthonormal()
{
    viewpoint->setPerspective(0.0F);
    doRefresh();
}

void MIGLWidget::OnUpdateDecreasePersp(QAction *action)
{
    action->setEnabled( ( viewpoint->perspective() > 0.001F));
}

void MIGLWidget::OnGeometryAngle()
{
    MIAtom *a1 = AtomStack->Pop();
    MIAtom *a2 = AtomStack->Pop();
    MIAtom *a3 = AtomStack->Pop();
    if (a1 == a2 || a1 == a3 || a2 == a3)
    {
        Logger::message("All 3 atoms in angle must be different");
        return;
    }
    if (a1 && a2 && a3)
    {
        float d = (float)CalcAtomAngle(*a1, *a2, *a3);
        QString message = QString("Angle %1-%2-%3 is %4")
                   .arg(a1->name()).arg(a2->name()).arg(a3->name())
                   .arg(d, 0, 'f', 3);
        setMessage(message);
        Logger::log(message.toAscii().constData());
        PaletteChanged = true;
        doRefresh();
    }
}

void MIGLWidget::OnGeometryDistance()
{
    MIAtom *a1 = AtomStack->Pop();
    MIAtom *a2 = AtomStack->Pop();
    if (a1 && a2)
    {
        float d = (float)AtomDist(*a1, *a2);
        QString message = QString("Distance %1-%2 is %4")
                   .arg(a1->name()).arg(a2->name())
                   .arg(d, 0, 'f', 3);
        setMessage(message);
        Logger::log(message.toAscii().constData());
        Displaylist *models = GetDisplaylist();
        if (models != NULL)
        {
            models->AddContact(a1, a2, d);
        }

        PaletteChanged = true;
        doRefresh();
    }
}

void MIGLWidget::OnGeometryTorsion()
{
    MIAtom *a1 = AtomStack->Pop();
    MIAtom *a2 = AtomStack->Pop();
    MIAtom *a3 = AtomStack->Pop();
    MIAtom *a4 = AtomStack->Pop();
    if (a1 && a2 && a3 && a4)
    {
        float d = CalcAtomTorsion(a1, a2, a3, a4);
        QString message = QString("Torsion %1-%2-%3-%4 is %5")
                   .arg(a1->name()).arg(a2->name()).arg(a3->name()).arg(a4->name())
                   .arg(d, 0, 'f', 3);
        setMessage(message);
        Logger::log(message.toAscii().constData());
        PaletteChanged = true;
        doRefresh();
    }
}

void MIGLWidget::OnViewSlabin()
{
    viewpoint->changeSlab(-2);
    doRefresh();

}

void MIGLWidget::OnViewSlabout()
{
    viewpoint->changeSlab(2);
    doRefresh();
}

void MIGLWidget::OnViewAtomstack()
{
    QSettings settings;
    AtomStack->setVisible(!AtomStack->isVisible());
    settings.setValue("View Parameters/ShowStack", AtomStack->isVisible());
    doRefresh();
}

void MIGLWidget::OnViewGnomon()
{
    ShowGnomon = !ShowGnomon;
    QSettings settings;
    settings.setValue("View Parameters/ShowGnomon", ShowGnomon ? 1 : 0);
    scene->ShowGnomon = ShowGnomon;
    doRefresh();
}

void MIGLWidget::OnViewUnitCell()
{
    showUnitCell = !showUnitCell;
    QSettings settings;
    settings.setValue("View Parameters/ShowInitCell", showUnitCell ? 1 : 0);
    scene->showUnitCell = showUnitCell;
    doRefresh();
}

void MIGLWidget::OnAnimateRoll()
{
    Roll = Roll^1;
    if (!TimerOn)
    {
        m_timer = startTimer(ClockTick);
        TimerOn = true;
    }
}

void MIGLWidget::OnAnimateRock()
{
    Rock = Rock^1;
    if (!TimerOn)
    {
        m_timer = startTimer(ClockTick);
        TimerOn = true;
    }
}

void MIGLWidget::setViewpointLineThickness(int thickness)
{
    viewpointSettings->SetLineThickness(thickness);
    doRefresh();
}

void MIGLWidget::OnRenderingDepthcuedcolors()
{
    viewpointSettings->setDepthCuedColors(!viewpointSettings->isDepthCuedColors());
    doRefresh();
}

void MIGLWidget::OnRenderingDepthcuedlinewidth()
{
    viewpointSettings->setDepthCuedLineWidth(!viewpointSettings->isDepthCuedLineWidth());
    scene->renderer->setLineWidthDepthCued(viewpointSettings->isDepthCuedLineWidth());
    doRefresh();
}

void MIGLWidget::OnDimNonactiveModels()
{
    viewpointSettings->setDimNonactiveModels(!viewpointSettings->isDimNonactiveModels());
    scene->renderer->setDimNonactiveModels(viewpointSettings->isDimNonactiveModels());
    doRefresh();
}

void MIGLWidget::OnAmountToDimNonactiveModels()
{
    bool ok;
    double percent = QInputDialog::getDouble(this, "Amount to dim non-active models", "Amount to dim non-active models (0.0 to 1.0)", viewpointSettings->getAmountToDimNonactiveModels(), 0.0, 1.0, 2, &ok);
    if (ok)
    {
        viewpointSettings->setAmountToDimNonactiveModels(percent);
        scene->renderer->setAmountToDimNonactiveModels(percent);
        doRefresh();
    }
}

void MIGLWidget::OnUpdateDimNonactiveModels(QAction *action)
{
    action->setChecked(viewpointSettings->isDimNonactiveModels());
}

void MIGLWidget::OnViewRotatey90()
{
    viewpoint->Do();
    viewpoint->rotate(0.0F, -90.0F, 0.0F);
    ReDraw();
}

void MIGLWidget::OnViewRotateyminus()
{
    viewpoint->Do();
    viewpoint->rotate(0.0F, 90.0F, 0.0F);
    ReDraw();
}

void MIGLWidget::OnUpdateAnimateRock(QAction *action)
{
    action->setChecked(Rock != 0);

}

void MIGLWidget::OnUpdateAnimateRoll(QAction *action)
{
    action->setChecked(Roll != 0);
}

void MIGLWidget::OnUpdateLinethicknessFour(QAction *action)
{
    action->setChecked(viewpointSettings->GetLineThickness() == 4);
    action->setEnabled((viewpointSettings->GetBallandStick() == ViewPointSettings::BALLANDSTICK
                   || viewpointSettings->GetBallandStick() == ViewPointSettings::STICKS));
}

void MIGLWidget::OnUpdateLinethicknessOne(QAction *action)
{
    action->setChecked(viewpointSettings->GetLineThickness() <= 1);
    action->setEnabled((viewpointSettings->GetBallandStick() == ViewPointSettings::BALLANDSTICK
                   || viewpointSettings->GetBallandStick() == ViewPointSettings::STICKS));
}

void MIGLWidget::OnUpdateLinethicknessThree(QAction *action)
{
    action->setChecked(viewpointSettings->GetLineThickness() == 3);
    action->setEnabled((viewpointSettings->GetBallandStick() == ViewPointSettings::BALLANDSTICK
                   || viewpointSettings->GetBallandStick() == ViewPointSettings::STICKS));
}

void MIGLWidget::OnUpdateLinethicknessTwo(QAction *action)
{
    action->setChecked(viewpointSettings->GetLineThickness() == 2);
    action->setEnabled((viewpointSettings->GetBallandStick() == ViewPointSettings::BALLANDSTICK
                   || viewpointSettings->GetBallandStick() == ViewPointSettings::STICKS));
}

void MIGLWidget::OnUpdateRenderingDepthcuedcolors(QAction *action)
{
    action->setChecked(viewpointSettings->isDepthCuedColors());
}

void MIGLWidget::OnUpdateRenderingDepthcuedlinewidth(QAction *action)
{
    action->setChecked(viewpointSettings->isDepthCuedLineWidth());
    action->setEnabled((viewpointSettings->GetBallandStick() == ViewPointSettings::BALLANDSTICK
                   || viewpointSettings->GetBallandStick() == ViewPointSettings::STICKS));
}

void MIGLWidget::OnUpdateStereoToggle(QAction *action)
{
    bool stereo = QSettings().value("View Parameters/stereo", false).toBool();
    action->setChecked(stereo);
}

void MIGLWidget::OnUpdateHardwareStereo(QAction *action)
{
    bool hardwareStereo = QSettings().value("View Parameters/hardwareStereo", false).toBool();
    action->setEnabled(Application::instance()->isHardwareStereoAvailable());
    action->setChecked(hardwareStereo);
}

void MIGLWidget::OnUpdateViewAtomstack(QAction *action)
{
    action->setChecked(AtomStack->isVisible() == 1);
}

void MIGLWidget::OnUpdateViewGnomon(QAction *action)
{
    action->setChecked(ShowGnomon);
}

void MIGLWidget::OnUpdateViewUnitCell(QAction *action)
{
    action->setChecked(showUnitCell);
}

void MIGLWidget::OnUpdateViewOrthonormal(QAction *action)
{
    action->setChecked(viewpoint->perspective() == 0.0);
}

void MIGLWidget::OnRenderingBallandstick()
{
    viewpointSettings->SetBallandStick();
    doRefresh();
}

void MIGLWidget::OnUpdateRenderingBallandstick(QAction *action)
{
    action->setChecked(viewpointSettings->GetBallandStick() == ViewPointSettings::BALLANDSTICK);
}

void MIGLWidget::OnGotoGotoxyz()
{
    gotoXyzWithPrompt();
}

void MIGLWidget::gotoXyzWithPrompt()
{
    float m_x = viewpoint->center()[0];
    float m_y = viewpoint->center()[1];
    float m_z = viewpoint->center()[2];

    char buf[1024];
    sprintf(buf, "%0.2f %0.2f %0.2f", m_x, m_y, m_z);
    QString str(buf);

    bool ok;
    str = QInputDialog::getText(this, "Go to x,y,z", "Enter the coordinate to center at (Angstroms):", QLineEdit::Normal, str, &ok);
    if (!ok)
    {
        return;
    }

    str.replace(',', " ");
    viewpoint->Do();
    if (sscanf(str.toAscii().constData(), "%f%f%f", &m_x, &m_y, &m_z) == 3)
    {
        moveTo(Vector3<float>(m_x, m_y, m_z));
    }
}

void MIGLWidget::moveTo(const Vector3<float> &pos)
{
    ViewPointAnimation *animation = new ViewPointAnimation(viewpoint, this);
    animation->setCenterEnd(pos);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
    connect(animation, SIGNAL(finished()), this, SLOT(CheckCenter()));
}

void MIGLWidget::OnViewClipplanes()
{
    GenericDataDialog dlg(this);
    dlg.setWindowTitle("Clipping Planes");
    dlg.addDoubleField("Front", viewpoint->frontClip());
    dlg.addDoubleField("Back", viewpoint->backClip());
    if (dlg.exec() == QDialog::Accepted)
    {
        viewpoint->setSlab(dlg.value(0).toDouble(), dlg.value(1).toDouble());
        PaletteChanged = true;
        doRefresh();
    }
}

void MIGLWidget::OnViewLabels()
{
    ShowLabels = !ShowLabels;
    QSettings().setValue("View Parameters/ShowLabels", ShowLabels ? 1 : 0);
    scene->ShowLabels = ShowLabels;
    doRefresh();
}

void MIGLWidget::OnUpdateViewLabels(QAction *action)
{
    action->setChecked(ShowLabels == 1);
}

void MIGLWidget::OnVdwDotSurface()
{
    Displaylist *Models = GetDisplaylist();
    Molecule *lastnode = Models->CurrentItem();
    if (!lastnode)
    {
        return;
    }

    static double dotsper = 3.0;
    static double boxsize = 20.0;
    static bool ignoreHidden = true;

    GenericDataDialog dlg(this);
    dlg.setWindowTitle("Surface");
    dlg.addDoubleField("Dots/Angstrom (1-10):", dotsper);
    dlg.addDoubleField("Bounding Box (1-100 Angstroms):", boxsize);
    dlg.addBoolField("Ignore Hidden Atoms:", ignoreHidden);
    if (dlg.exec() == QDialog::Accepted)
    {
        double newDotsper = dlg.value(0).toDouble();
        double newBoxsize = dlg.value(1).toDouble();
        if (newDotsper >= 1.0 && newDotsper <= 10.0
            && newBoxsize >= 1.0 && newBoxsize <= 100.0)
        {

            dotsper = newDotsper;
            boxsize = newBoxsize;
            ignoreHidden = dlg.value(2).toBool();
            int numDots = lastnode->SurfaceCenter(
                        viewpoint,
                        (dotsper > 0.0f ? static_cast<float>(1.0/dotsper) : 0.5f),
                        boxsize, ignoreHidden);
            QString message = QString("Number of dots: %1")
                    .arg(numDots);
            setMessage(message);
            _modified = true;
            Logger::log(message.toAscii().constData());
            PaletteChanged = true;
            doRefresh();
        }
    }
}

void MIGLWidget::OnSurfaceSolvent()
{
    Displaylist *Models = GetDisplaylist();
    Molecule *lastnode = Models->CurrentItem();
    if (!lastnode)
    {
        return;
    }

    static double dotsper = 3.0;

    GenericDataDialog dlg(this);
    dlg.setWindowTitle("Surface");
    dlg.addDoubleField("Dots/Angstrom (1-10):", dotsper);
    if (dlg.exec() == QDialog::Accepted)
    {
        double newDotsper = dlg.value(0).toDouble();
        if (newDotsper >= 1.0 && newDotsper <= 10.0)
        {
            dotsper = newDotsper;
            int numDots = lastnode->SolventSurface(viewpoint, static_cast<float>(1.0/dotsper));
            QString message = QString("Number of dots: %1")
                    .arg(numDots);
            setMessage(message);
            _modified = true;
            Logger::log(message.toAscii().constData());
            PaletteChanged = true;
            doRefresh();
        }
    }
}

void MIGLWidget::clearMessage()
{
    setMessage(QString::null);
}

void MIGLWidget::cancelFitting()
{
    clearFitTorsion();
    GetDisplaylist()->ClearCurrentSurface();
    // restore color and positions
    MI_ASSERT(FitToken != 0);
    if (FitToken != 0)
    {
        savefits.RestoreColor(FitToken, AtomType::FITATOM);
        savefits.Restore(FitToken);
    }

    MIAtomList().swap(CurrentAtoms); // was CurrentAtoms.clear();
    fitres = NULL;
    fitmol = NULL;
    _modified = true;
    RightMouseMode = CENTER;
    doRefresh();
}

void MIGLWidget::ApplyFit()
{
    //  if(MIBusyManager::instance()->Busy()) return;
    if (!IsFitting())
    {
        return;
    }
    clearFitTorsion();
    // Mark the model as dirty
    Modify(true);
    fitmol->SetCoordsChanged(true);
    // restore color
    MI_ASSERT(FitToken != 0);
    if (FitToken != 0)
    {
        savefits.RestoreColor(FitToken, AtomType::FITATOM);
    }
    FitToken = savefits.Save(CurrentAtoms, fitmol);
    _modified = true;
    fitres = NULL;
    MIAtomList().swap(CurrentAtoms); // was CurrentAtoms.clear();
    GetDisplaylist()->ClearCurrentSurface();
    fitmol = NULL;
    RightMouseMode = CENTER;
    doRefresh();
}

void MIGLWidget::ResetFit()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    if (!IsFitting())
    {
        return;
    }
    MI_ASSERT(FitToken != 0);
    if (FitToken != 0)
    {
        savefits.Restore(FitToken);
    }
    if (fromcenter)
    {
        fitcenter.copyShallow(*fromcenter);
    }
    RightMouseMode = CENTER;
    doRefresh();
}

/*
   void MIGLWidget::TranslateFit(float x, float y, float z)
   {
   MIAtom *a;
   for(int i=0; i<CurrentAtoms.size(); i++){
    a = (MIAtom *)CurrentAtoms[i];
    a->x += x;
    a->y += y;
    a->z += z;
   }
   }*/

void MIGLWidget::OnFitResidue()
{
    fitResidue();
}

void MIGLWidget::fitResidue()
{
    MIAtom *a;
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    if (FitOK())
    {
        AtomStack->Pop(a, fitres, fitmol);
        if (a && fitres)
        {
            SelectType = SINGLERESIDUE;
            if (SetupFit(fitres, fitres, fitmol, a->altloc()))
            {
                _modified = true;
                fitcenter.copyShallow(*a);
                fromcenter = a;
                doRefresh();
            }
        }
    }
}

void MIGLWidget::OnFitRotate()
{
    RightMouseMode = ROTATE;
}

void MIGLWidget::OnUpdateFitRotate(QAction *action)
{
    action->setChecked(RightMouseMode == ROTATE);
    action->setEnabled(IsFitting() == true && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnFitTorsion()
{
    RightMouseMode = BONDTWIST;
}

void MIGLWidget::OnUpdateFitTorsion(QAction *action)
{
    action->setChecked(RightMouseMode == BONDTWIST);
    action->setEnabled(IsFitting() == true && Torsioning > 0 && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnFitTranslate()
{
    RightMouseMode = TRANSLATE;
}

void MIGLWidget::OnUpdateFitTranslate(QAction *action)
{
    action->setChecked(RightMouseMode == TRANSLATE);
    action->setEnabled(IsFitting() == true && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnFitCentermode()
{
    RightMouseMode = CENTER;
}

void MIGLWidget::OnUpdateFitCentermode(QAction *action)
{
    action->setChecked(RightMouseMode == CENTER);
}

void MIGLWidget::OnFitCancel()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }

    if (!IsFitting())
    {
        return;
    }
    cancelFitting();
}

void MIGLWidget::OnFitReset()
{
    ResetFit();
}

void MIGLWidget::OnFitApply()
{
    ApplyFit();
}

void MIGLWidget::OnFitResidues()
{
    std::set<MIAtom*> atom_set;
    MIAtomList fit_atoms;

    MIAtom *a, *a1;
    Residue *popres;
    Molecule *m;

    if (MIBusyManager::instance()->Busy() || !FitOK())
    {
        return;
    }

    // build vector of atoms to fit
    AtomStack->Pop(a, popres, fitmol);
    if (!a || !popres || !fitmol)
    {
        return;
    }
    fitres = popres;
    while (popres != NULL)
    {
        atom_set.insert(popres->atoms().begin(), popres->atoms().end()); // add atoms in this residue to set
        AtomStack->Pop(a1, popres, m);
    }
    fit_atoms.insert(fit_atoms.end(), atom_set.begin(), atom_set.end());

    // setup fit
    SelectType = RESIDUES;
    if (SetupFit(fit_atoms, fitmol, a->altloc()))
    {
        _modified = true;
        fitcenter.copyShallow(*a);
        fromcenter = a;
        doRefresh();
    }
}

void MIGLWidget::OnFitRange()
{
    fitResidueRange();
}

void MIGLWidget::fitResidueRange()
{
    MIAtom *a2;
    MIAtom *a1;
    Residue *popres1, *popres2;
    Molecule *m1, *m2;
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    if (FitOK())
    {
        AtomStack->Pop(a1, popres1, m1);
        if (!a1 || !popres1 || !m1)
        {
            return;
        }
        AtomStack->Pop(a2, popres2, m2);
        if (!a2 || !popres2 || !m2)
        {
            return;
        }
        if (m1 != m2)
        {
            Logger::message("Both ends must be in the same molecule");
            return;
        }
        SelectType = RESIDUES;
        if (SetupFit(popres1, popres2, m1, a1->altloc()))
        {
            fitres = popres1;
            _modified = true;
            fitcenter.copyShallow(*a1);
            fromcenter = a1;
            doRefresh();
        }
    }
}

void MIGLWidget::OnUpdateFitRange(QAction *action)
{
    action->setEnabled(AtomStack->size() > 1  && MIBusyManager::instance()->Busy() == false);
    //action->setChecked(IsFitting()!=false && SelectType == RESIDUES);
}

void MIGLWidget::OnFitSingleatom()
{
    MIAtomList fit_atoms;
    MIAtom *a;
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    if (FitOK())
    {
        AtomStack->Pop(a, fitres, fitmol);
        if (a && fitres)
        {
            fit_atoms.push_back(a);
            SelectType = SINGLEATOM;
            if (SetupFit(fit_atoms, fitmol))
            {
                _modified = true;
                fitcenter.copyShallow(*a);
                fromcenter = a;
                RightMouseMode = TRANSLATE;
                doRefresh();
            }
        }
    }
}

void MIGLWidget::OnFitAtoms()
{
    MIAtom *a;
    MIAtom *afit;
    std::set<MIAtom*> fit_set;
    MIAtomList fit_atoms;

    Residue *popres;
    Molecule *popmol;
    if (MIBusyManager::instance()->Busy() || !FitOK())
    {
        return;
    }

    // build vector of atoms to fit
    AtomStack->Pop(a, fitres, fitmol);
    afit = a;
    if (!a || !fitres)
    {
        return;
    }
    while (a != NULL)
    {
        fit_set.insert(a);
        AtomStack->Pop(a, popres, popmol);
    }
    fit_atoms.insert(fit_atoms.end(), fit_set.begin(), fit_set.end());

    //setup fit
    SelectType = ATOMS;
    if (SetupFit(fit_atoms, fitmol))
    {
        _modified = true;
        fitcenter.copyShallow(*afit);
        fromcenter = afit;
        doRefresh();
    }
}

bool MIGLWidget::FitTorsion(const char *t)
{
    MIAtom *a1 = NULL;
    MIAtom *a2 = NULL;
    GeomRefiner *geomrefi = MIFitGeomRefiner();
    TORSION *chi;
    Residue *prev = 0;
    if (!fitres)
    {
        return false;
    }
    if (strcmp(t, "PHI") == 0)
    {
        prev = fitres->prev();
    }
    chi = geomrefi->dict.getTORSION(fitres, t, prev);
    if (!chi)
    {
        return false;
    }
    for (int i = 0; i < fitres->atomCount(); i++)
    {
        if (strcmp(chi->getAtom2()->name(), fitres->atom(i)->name()) == 0)
        {
            a1 = chi->getAtom2();
        }
        if (strcmp(chi->atom3->name(), fitres->atom(i)->name()) == 0)
        {
            a2 = chi->atom3;
        }
    }
    free(chi);
    if (!a1 || !a2)
    {
        return false;
    }
    if (strcmp(t, "PSI") == 0)
    {
        //reverse direction of torsion
        AtomStack->Push(a2, fitres, fitmol);
        AtomStack->Push(a1, fitres, fitmol);
    }
    else
    {
        AtomStack->Push(a1, fitres, fitmol);
        AtomStack->Push(a2, fitres, fitmol);
    }
    OnFitSetuptorsion();
    return true;
}

static int arrow2(vector<PLINE> &avu, float dx, float dy, float dz, float ox, float oy, float oz, int color);
static int arrow2(vector<LINE> &avu, float dx, float dy, float dz, float ox, float oy, float oz, int color);


//@{
// Makes an arrow (a collection of lines) at point o in the direction d with a color.
// See Colors.h for the color codes.
//@}
#define X 0
#define Y 1
#define Z 2

static int arrow2(vector<PLINE> &avu, float dx, float dy, float dz, float ox, float oy, float oz, int color)
{
    // use the existing arrow2 for LINE's and then copy to the PLINE
    PLINE line;
    vector<LINE> temp;
    arrow2(temp, dx, dy, dz, ox, oy, oz, color);
    if (temp.size() > 0)
    {
        for (size_t i = 0; i < temp.size(); i++)
        {
            line.p1.x = temp[i].x1;
            line.p1.y = temp[i].y1;
            line.p1.z = temp[i].z1;
            line.p1.color = temp[i].color;
            line.p2.x = temp[i].x2;
            line.p2.y = temp[i].y2;
            line.p2.z = temp[i].z2;
            line.p2.color = temp[i].color2;
            avu.push_back(line);
        }
    }
    return avu.size();
}

int arrow2(vector<LINE> &avu, float dx, float dy, float dz, float ox, float oy, float oz, int color)
{
    /* draw an arrow along vector dx,dy,dz
     * starting at ox,oy,oz in LINEs */
    float d, theta;
    float a[3], b[3], c[3];
    float mat[4][3];
    static float rtodeg;
    static int init = 0, i;
    float x, y, z;
    //extern float vectorangle();
    LINE line;
    if (init == 0)
    {
        rtodeg = 180.0/acos(-1.0);
        init = 1;
    }
    d = sqrt(dx*dx+dy*dy+dz*dz);
    /* draw arrow along x */
    /* arrow shaft */
    line.x1 = 0.0;
    line.y1 = 0.0;
    line.z1 = 0.0;
    line.x2 = d;
    line.y2 = 0.0;
    line.z2 = 0.0;
    avu.push_back(line);
    /* arrow head 0.1 of shaft length */
    line.x1 = d;
    line.y1 = 0.0;
    line.z1 = 0.0;
    line.x2 = d-d*0.1;
    line.y2 = -d*0.08;
    line.z2 = 0.0;
    avu.push_back(line);
    line.x1 = d;
    line.y1 = 0.0;
    line.z1 = 0.0;
    line.x2 = d-d*0.1;
    line.y2 = d*0.08;
    line.z2 = 0.0;
    avu.push_back(line);
    line.x1 = d;
    line.y1 = 0.0;
    line.z1 = 0.0;
    line.x2 = d-d*0.1;
    line.y2 = 0.0;
    line.z2 = -d*0.08;
    avu.push_back(line);
    line.x1 = d;
    line.y1 = 0.0;
    line.z1 = 0.0;
    line.x2 = d-d*0.1;
    line.y2 = 0.0;
    line.z2 = d*0.08;
    avu.push_back(line);
    /* build matrix for rotating from x to (dx,dy,dz) */
    /* find cross vector = axis about which to rotate */
    a[X] = dx;
    a[Y] = dy;
    a[Z] = dz;
    b[X] = d;
    b[Y] = 0.0;
    b[Z] = 0.0;
    cross(a, b, c);
    theta = vectorangle(b, a)*rtodeg;
    initrotate(0.0, 0.0, 0.0, c[0], c[1], c[2], -theta, mat);
    int nvu = avu.size();
    for (i = nvu-5; i < nvu; i++)
    {
        x = avu[i].x1;
        y = avu[i].y1;
        z = avu[i].z1;
        xl_rotate(x, y, z, &(avu[i].x1), &(avu[i].y1), &(avu[i].z1), mat);
        x = avu[i].x2;
        y = avu[i].y2;
        z = avu[i].z2;
        xl_rotate(x, y, z, &(avu[i].x2), &(avu[i].y2), &(avu[i].z2), mat);
        avu[i].x1 += ox;
        avu[i].x2 += ox;
        avu[i].y1 += oy;
        avu[i].y2 += oy;
        avu[i].z1 += oz;
        avu[i].z2 += oz;
        avu[i].color = color;
        avu[i].color2 = color;
    }
    return (5);
}

#undef X
#undef Y
#undef Z



void MIGLWidget::OnFitSetuptorsion()
{
    MIAtom *a1;
    MIAtom *a2;
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    if (IsFitting())
    {
        a2 = AtomStack->Pop();
        a1 = AtomStack->Pop();
        if (a1 && a2)
        {
            if ((Torsioning = fitmol->SetupTorsion(a1, a2, &CurrentAtoms)) != 0)
            {
                RightMouseMode = BONDTWIST;

                std::vector<PLINE>().swap(TorsionArrow); // was TorsionArrow.clear();
                arrow2(TorsionArrow, a2->x() - a1->x(), a2->y() - a1->y(), a2->z() - a1->z(),
                       a1->x(), a1->y(), a1->z(), Colors::WHITE);
                QString message = QString("%1 atoms under torsion")
                        .arg(Torsioning);
                setMessage(message);
                Logger::log(message.toAscii().constData());
            }
        }
        PaletteChanged = true;
        doRefresh();
    }
}

bool MIGLWidget::FitOK()
{
    if (CurrentAtoms.size() > 0)
    {
        if (AutoFit)
        {
            ApplyFit();
            return true;
        }

        int ret = QMessageBox::question(this, "Accept Fit", "There are atoms being fit already.  Do you want to apply the changes?", QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        switch (ret)
        {
        case QMessageBox::Cancel: return false;
            break;
        case QMessageBox::Yes:    ApplyFit();
            break;
        case QMessageBox::No:     cancelFitting();
            break;
        }
    }
    return true;
}

unsigned int MIGLWidget::SetupFit(const MIAtomList &atoms, Molecule *model, char altloc)
{
    // don't let symm atoms be fit - they are not permanent nor saveable!
    for (unsigned int j = 0; j < atoms.size(); ++j)
    {
        if (atoms[j]->type() & AtomType::SYMMATOM)
        {
            return 0;
        }
    }

    FitToken = savefits.Save(atoms, model);
    fitmol = model;
    MIAtomList().swap(CurrentAtoms); // was CurrentAtoms.clear();

    unsigned int natoms = 0;
    for (unsigned int j = 0; j < atoms.size(); j++)
    {
        if (altloc == ' ' || atoms[j]->altloc() == ' ' || atoms[j]->altloc() == altloc)
        {
            // fit only if altoc == ' ' or atom has the same altloc
            CurrentAtoms.push_back(atoms[j]);
            atoms[j]->setType(atoms[j]->type() | AtomType::FITATOM);
            natoms++;
        }
    }

    Logger::log("Fitting %d atoms\nSaved atoms in FitSaveSet %d", natoms, (int)FitToken);
    UpdateCurrent();
    return FitToken;
}

unsigned int MIGLWidget::SetupFit(Residue *res1, Residue *res2, Molecule *model, char altloc)
{
    Residue *res;
    Residue *start = NULL;
    int nres;
    if (!res1 && !res2)
    {
        if (model)
        {
            res = res1 = res2 = model->residuesBegin();
            while (res->next() != NULL)
            {
                res = res2 = res->next();
            }
        }
        else
        {
            return 0;
        }
    }
    if (!res1)
    {
        res1 = res2;
    }
    if (!res2)
    {
        res2 = res1;
    }
    /* find correct order */
    if (res1 == res2)
    {
        nres = 1;
        start = res1;
    }
    else
    {
        res = res1;
        nres = 0;
        while (res != NULL)
        {
            nres++;
            if (res == res2)
            {
                start = res1;
                break;
            }
            res = res->next();
        }
        if (!start)
        {
            res = res2;
            nres = 0;
            while (res != NULL)
            {
                nres++;
                if (res == res1)
                {
                    start = res2;
                    break;
                }
                res = res->next();
            }
        }
    }
    if (!start)
    {
        Logger::message("Can't make sense of start and end - not from same model?");
        return (0);
    }
    res = start;
    int i = 0, j;
    // don't let symm atoms be fit - they are not permanent nor saveable!
    while (res != NULL && i < nres)
    {
        for (j = 0; j < res->atomCount(); j++)
        {
            if (res->atom(j)->type() & AtomType::SYMMATOM)
            {
                return 0;
            }
        }
        i++;
        res = res->next();
    }

    FitToken = savefits.Save(start, nres, model);
    fitmol = model;
    MIAtomList().swap(CurrentAtoms); // was CurrentAtoms.clear();

    int natoms = 0;
    res = start;
    i = 0;
    while (res != NULL && i < nres)
    {
        for (j = 0; j < res->atomCount(); j++)
        {
            if (altloc == ' ' || res->atom(j)->altloc() == ' ' || res->atom(j)->altloc() == altloc)
            {
                // fit only if altoc == ' ' or atom has the same altloc
                CurrentAtoms.push_back(res->atom(j));
                res->atom(j)->setType(res->atom(j)->type() | AtomType::FITATOM);
                natoms++;
            }
        }
        i++;
        res = res->next();
    }
    Logger::log("Fitting %s %s with %d atoms\nSaved atoms in FitSaveSet %d", start->type().c_str(), start->name().c_str(), natoms, (int)FitToken);
    UpdateCurrent();
    return FitToken;
}

void MIGLWidget::OnUpdateFitSetuptorsion(QAction *action)
{
    action->setEnabled(IsFitting() == true && AtomStack->size() >= 2 && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnFitCleartorsion()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    clearFitTorsion();
}

void MIGLWidget::clearFitTorsion()
{
    if (fitmol != NULL && Torsioning > 0)
    {
        clearMessage();
        fitmol->ClearTorsion();
        Torsioning = 0;
        if (RightMouseMode == BONDTWIST)
        {
            RightMouseMode = CENTER;
        }
        std::vector<PLINE>().swap(TorsionArrow); // was TorsionArrow.clear();
    }
}

void MIGLWidget::OnUpdateFitCleartorsion(QAction *action)
{
    action->setEnabled(IsFitting() != false && IsTorsioning() != false && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnUpdateFitApply(QAction *action)
{
    action->setEnabled(IsFitting() != false && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnUpdateFitCancel(QAction *action)
{
    action->setEnabled(IsFitting() != false && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnUpdateFitReset(QAction *action)
{
    action->setEnabled(IsFitting() != false && MIBusyManager::instance()->Busy() == false);
}

bool MIGLWidget::promptForReplaceResidue(std::string &deft_str)
{
    if (MIBusyManager::instance()->Busy())
    {
        return false;
    }
    vector<std::string> resList = MIFitDictionary()->GetDictResList();
    if (resList.empty())
    {
        return false;
    }
    int choice = 0;
    QStringList choices;
    foreach (const std::string& res, resList)
    {
        choices += res.c_str();
        if (res == deft_str)
            choice = choices.size()-1;
    }

    GenericDataDialog dlg(this);
    dlg.setWindowTitle("Replace Residue");
    dlg.addComboField("Choose a residue type to replace with:", choices, choice);
    if (dlg.exec() != QDialog::Accepted)
    {
        return false;
    }
    deft_str = choices.at(dlg.value(0).toInt()).toStdString();
    return true;
}

void MIGLWidget::OnFitReplacewith()
{
    replaceResidueWithPrompt();
}

void MIGLWidget::replaceResidueWithPrompt()
{
    std::string value;
    Molecule *model;
    MIAtom *a;
    Residue *res;
    AtomStack->Peek(a, res, model);
    if (res)
        value = res->type();
    if (promptForReplaceResidue(value))
    {
        replaceResidue(value.c_str());
    }
}

static bool NextConformer(Residue *fitres, MIGLWidget *win)
{
    if (!win || !fitres)
        return false;

    int confomer = fitres->confomer() + 1;
    Residue *res = win->GetDictRes(fitres->type().c_str(), confomer);
    if (res->atomCount() < 1)
        return false;

    //move newres into position
    if (!MoveOnto(res, fitres))
        return false;
    fitres->setConfomer(confomer);

    //and copy over the coordinates
    for (int i = 0; i< fitres->atomCount(); ++i)
    {
        if (MIAtom::MIIsSideChainAtom(fitres->atom(i)))
        {
            MIAtom *new_atom = atom_from_name(fitres->atom(i)->name(), *res);
            if (new_atom)
                fitres->atom(i)->copyPosition(*new_atom);
        }
    }

    return true;
}

void MIGLWidget::OnUpdateFitNextConfomer(QAction *action)
{
    action->setEnabled(((focusres && focusnode && (IsFitting() == false))
                   || ((IsFitting() != false) && fitres && fitmol && (SelectType == SINGLERESIDUE))) && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnFitNextConfomer()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    bool was_fitting = false;

    if (IsFitting() && fitres && fitmol && SelectType == SINGLERESIDUE)
    {
        if (fitres == focusres && fitmol == focusnode)
        {
            if (NextConformer(fitres, this))
                doRefresh();
            return;
        }

        setFocusResidue(fitres, false);
        focusnode = fitmol;
        ApplyFit();
        was_fitting = true;
    }
    else
    {
        fitres = focusres;
        fitmol = focusnode;
    }
    if (focusnode && focusres)
    {
        MIAtom *a = focusres->atom(0);
        AtomStack->Push(a, focusres, focusnode);
        replaceResidue(focusres->type().c_str());
        if (was_fitting)
        {
            a = focusres->atom(0);
            AtomStack->Push(a, focusres, focusnode);
            fitResidue();
        }
    }
    // get the focus back from the treectrl
    setFocus(Qt::OtherFocusReason); // NOTE: interferes with active widget indication code
}

void MIGLWidget::replaceResidue(const char *residueType)
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    MIAtom *at;
    static Residue *lastres = NULL;

    AtomStack->Pop(at, fitres, fitmol);
    if (!at || !fitres || !fitmol)
    {
        return;
    }

    // must be at least 1 atom in each residue
    if (fitres->atomCount() < 1)
    {
        return;
    }
    int confomer = 0;
    // if the new type is the same as the old - get the next confomer
    if (std::string(residueType) == fitres->type())
    {
        confomer = fitres->confomer() + 1;
    }
    Residue *res = GetDictRes(residueType, confomer);
    if (!res)
    {
        std::string s = ::format("Type: %s not found in the dictionary", residueType);
        QMessageBox::warning(this, "Warning", s.c_str());
        return;
    }
    if (res->atomCount() < 1)
    {
        return;
    }

    // check point the model if new residue being confomer'd
    if (fitres != lastres)
    {
        std::string s = ::format("Before changing confomer of %s in %s", resid(fitres).c_str(), fitmol->compound.c_str());
        if (!SaveModelFile(fitmol, s.c_str()))
        {
            Logger::message("Warning: Unable to save model - will not be able to Undo");
        }
        lastres = fitres;
    }

    MIFitGeomRefiner()->Purge(fitres);

    // move a onto b
    if (MoveOnto(res, fitres))
    {
        // now replace the residue with new type
        Residue *saveRes = fitres;
        Molecule *saveMol = fitmol;
        fitmol->ReplaceRes(fitres, res);
        fitmol = saveMol;
        fitres = saveRes;
        fitres->setConfomer(confomer);

        int nlinks = fitmol->getnlinks();
        fitmol->Build();
        if (nlinks > 0)
        {
            fitmol->BuildLinks();
        }
        setFocusResidue(fitres, false, false);
        // Mark the model as dirty
        Modify(true);
        fitmol->SetModified();
        fitmol = NULL;
        fitres = NULL;
        PaletteChanged = true;
        ReDrawAll();
    }
}

void MIGLWidget::OnUpdateFitReplacewith(QAction *action)
{
    action->setEnabled(IsFitting() == false && !AtomStack->empty() && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnAddMarkAfter()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    InsertMRK(focusres, focusnode, 0);
}

void MIGLWidget::OnUpdateAddMarkAfter(QAction *action)
{
    action->setEnabled(focusres && focusnode && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnAddMarkBefore()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    InsertMRK(focusres, focusnode, 1);
}

void MIGLWidget::OnUpdateAddMarkBefore(QAction *action)
{
    action->setEnabled(focusres && focusnode && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnObjectsAllatoms()
{
    Displaylist *Models = GetDisplaylist();
    Molecule *node = Models->CurrentItem();
    if (!node)
    {
        return;
    }
    node->ShowAll();
    node->HVisible = 1;
    doRefresh();
}

void MIGLWidget::OnViewTopview()
{
    TopView = !TopView;
    if (TopView)
    {
        viewpoint->setScale(viewpoint->scale()/2.0);
    }
    else
    {
        viewpoint->setScale(viewpoint->scale()*2.0);
    }
    ReDraw();
}

void MIGLWidget::OnUpdateViewTopview(QAction *action)
{
    action->setChecked(TopView);
}

void MIGLWidget::OnRenderSpacefilling()
{
    viewpointSettings->SetSpaceFilling();
    doRefresh();
}

void MIGLWidget::OnUpdateRenderSpacefilling(QAction *action)
{
    action->setChecked(viewpointSettings->GetBallandStick() == ViewPointSettings::CPK);
}

void MIGLWidget::OnRenderSticks()
{
    viewpointSettings->SetSticks();
    doRefresh();
}

void MIGLWidget::OnUpdateRenderSticks(QAction *action)
{
    action->setChecked(viewpointSettings->GetBallandStick() == ViewPointSettings::STICKS);
}

void MIGLWidget::OnGeomBond()
{
    MIAtom *a1;
    MIAtom *a2;
    Molecule *mol1, *mol2;
    Residue *res1, *res2;
    AtomStack->Pop(a1, res1, mol1);
    AtomStack->Pop(a2, res2, mol2);
    if (!a2 || !res2 || !mol2 || !a1 || !res1 || !mol1)
    {
        Logger::message("First pick two atoms and then Add Bond");
        return;
    }
    if (mol1 != mol2)
    {
        Logger::message("Both ends of bonds must be in the same molecule");
        return;
    }
    mol1->AddBond(a1, a2);
    PaletteChanged = true;
    doRefresh();
}

void MIGLWidget::OnUpdateGeomBond(QAction *action)
{
    action->setEnabled(AtomStack->size() >= 2);
}

void MIGLWidget::OnGeomUnbond()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    MIAtom *a1;
    MIAtom *a2;
    Molecule *mol1, *mol2;
    Residue *res1, *res2;
    AtomStack->Pop(a1, res1, mol1);
    AtomStack->Pop(a2, res2, mol2);
    if (!a2 || !res2 || !mol2 || !a1 || !res1 || !mol1)
    {
        Logger::message("First pick two atoms and then Break Bond");
        return;
    }
    if (mol1 != mol2)
    {
        Logger::message("Both ends of bonds must be in the same molecule");
        return;
    }
    mol1->BreakBond(a1, a2);
    PaletteChanged = true;
    doRefresh();
}

void MIGLWidget::OnUpdateGeomUnbond(QAction *action)
{
    action->setEnabled(AtomStack->size() >= 2 && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnUpdateGeometryAngle(QAction *action)
{
    action->setEnabled(AtomStack->size() >= 3);
}

void MIGLWidget::OnUpdateGeometryDistance(QAction *action)
{
    action->setEnabled(AtomStack->size() >= 2);
}

void MIGLWidget::OnUpdateGeometryTorsion(QAction *action)
{
    action->setEnabled(AtomStack->size() >= 4);
}

void MIGLWidget::OnRenderBallsize()
{
    GenericDataDialog dlg(this);
    dlg.setWindowTitle("Ball/Cylinder Size");
    dlg.addDoubleField("Ball diameter (% of CPK):", viewpointSettings->GetBallSize());
    dlg.addDoubleField("Cylinder diameter (% of ball d):", viewpointSettings->GetCylinderSize());

    if (dlg.exec() == QDialog::Accepted)
    {
        viewpointSettings->SetBallSize(dlg.value(0).toDouble());
        viewpointSettings->SetCylinderSize(dlg.value(1).toDouble());
        PaletteChanged = true;
        doRefresh();
    }
}

void MIGLWidget::OnUpdateRenderBallsize(QAction *action)
{
    action->setEnabled(true);
    //action->setEnabled(viewpoint->GetBallandStick()==ViewPointSettings::BALLANDCYLINDER);
}

void MIGLWidget::OnRenderBallandcylinder()
{
    viewpointSettings->SetBallandCylinder();
    doRefresh();
}

void MIGLWidget::OnUpdateRenderBallandcylinder(QAction *action)
{
    action->setChecked(viewpointSettings->GetBallandStick() == ViewPointSettings::BALLANDCYLINDER);
}

void MIGLWidget::OnGotoZoomiin()
{
    viewpoint->Do();
    viewpoint->zoom(1.1F);
    doRefresh();
}

void MIGLWidget::OnGotoZoomout()
{
    viewpoint->Do();
    viewpoint->zoom(0.9F);
    doRefresh();
}

void MIGLWidget::OnGotoFittoscreen()
{
    Displaylist *Models = GetDisplaylist();
    if (Models->CurrentItem())
    {
        ViewPointAnimation *animation = new ViewPointAnimation(viewpoint, this);
        float x, y, z;
        Models->CurrentItem()->Center(x, y, z);
        viewpoint->Do();
        animation->setCenterEnd(Vector3<float>(x, y, z));
        float xmin, xmax, ymin, ymax, zmin, zmax;
        if (Models->CurrentItem()->VisibleBounds(camera, xmin, xmax, ymin, ymax, zmin, zmax) )
        {
            float fzmin = zmin;
            float fzmax = zmax;
            fzmax = (fzmax - fzmin)/2.0F + 2.0F;
            viewpoint->setSlab(-fzmax, fzmax);
            qreal sw = 0.9*width()/(xmax - xmin + 2);
            qreal sh = 0.9*height()/(ymax - ymin + 2);
            animation->setScaleEnd(std::min(sw, sh));
        }
        connect(animation, SIGNAL(finished()), this, SLOT(CheckCenter()));
        animation->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

void MIGLWidget::OnObjectClearstack()
{
    clearStack();
}

void MIGLWidget::clearStack()
{
    while (AtomStack->Pop())
    {
    }
    PaletteChanged = true;
    doRefresh();
}

void MIGLWidget::OnEditCopy()
{
    scene->renderer->setPickingEnabled(true);
    QApplication::clipboard()->setPixmap(static_cast<QGLWidget*>(viewport())->renderPixmap());
    scene->renderer->setPickingEnabled(false);
}

void MIGLWidget::OnAnimateRockandrollparameters()
{
    GenericDataDialog dlg(this);
    dlg.setWindowTitle("Rock and Roll Parameters");
    dlg.addDoubleField("Rock increment (0-10 deg)", RockIncrement);
    dlg.addDoubleField("Roll increment (0-10 deg)", RollIncrement);
    dlg.addDoubleField("Rock range (0-90 deg)", RockRange);

    if (dlg.exec() == QDialog::Accepted)
    {
        RockIncrement = dlg.value(0).toDouble();
        RollIncrement = dlg.value(1).toDouble();
        RockRange = dlg.value(2).toDouble();
        RockAngle = 0.0F;
        BlinkCounter = 0;
        PaletteChanged = true;
        QSettings settings;
        settings.setValue("View Parameters/RockIncrement", static_cast<int>(RockIncrement*100.0f));
        settings.setValue("View Parameters/RollIncrement", static_cast<int>(RollIncrement*100.0f));
        settings.setValue("View Parameters/RockRange", static_cast<int>(RockRange*100.0f));
    }
}

void MIGLWidget::OnUpdateFitResidue(QAction *action)
{
    action->setEnabled(!AtomStack->empty() && MIBusyManager::instance()->Busy() == false);
    action->setChecked(IsFitting() != false && SelectType == SINGLERESIDUE);
}

void MIGLWidget::OnUpdateFitResidues(QAction *action)
{
    action->setEnabled(!AtomStack->empty()  && MIBusyManager::instance()->Busy() == false);
    action->setChecked(IsFitting() != false && SelectType == RESIDUES);
}

void MIGLWidget::OnUpdateFitSingleatom(QAction *action)
{
    action->setEnabled(!AtomStack->empty()  && MIBusyManager::instance()->Busy() == false);
    action->setChecked(IsFitting() != false && SelectType == SINGLEATOM);
}

void MIGLWidget::OnUpdateFitAtoms(QAction *action)
{
    action->setEnabled(!AtomStack->empty()  && MIBusyManager::instance()->Busy() == false);
    action->setChecked(IsFitting() != false && SelectType == ATOMS);
}

void MIGLWidget::OnUpdateObjectShowresidue(QAction *action)
{
    action->setEnabled(!AtomStack->empty());
}

void MIGLWidget::OnObjectShowsidechain()
{
    Residue *res;
    Molecule *node;
    MIAtom *a;
    AtomStack->Pop(a, res, node);
    if (!a)
    {
        return;
    }
    std::string resshow;
    std::string at("*");
    if (node != NULL)
    {
        if (SaveColors)
        {
            node->Do();
        }
        node->Select(0, 0, 1, 0, resshow, at, res, res, 0, 0, 0, 0, 0);
    }
    PaletteChanged = true;
    doRefresh();
}

void MIGLWidget::OnUpdateObjectShowsidechain(QAction *action)
{
    action->setEnabled(!AtomStack->empty());
}

void MIGLWidget::OnGeomNeighbours()
{
    Residue *res;
    Molecule *node;
    MIAtom *a;
    float cutoff = 4.0F;
    AtomStack->Pop(a, res, node);
    if (!a)
        return;

    QString message = QString("Found %1 contacts <= %2")
            .arg(GetDisplaylist()->FindContacts(a, res, cutoff))
            .arg(cutoff);
    setMessage(message);
    PaletteChanged = true;
    Logger::log(message.toAscii().constData());
    doRefresh();
}

void MIGLWidget::OnUpdateGeomNeighbours(QAction *action)
{
    action->setEnabled(!AtomStack->empty());
}

void MIGLWidget::OnGeomClearneighbours()
{
    GetDisplaylist()->ClearContacts();
    ReDraw();
}

void MIGLWidget::OnGeomHbonds()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    Molecule *node = GetDisplaylist()->CurrentItem();
    if (!node)
    {
        return;
    }
    node->BuildHBonds();

    ReDraw();
}

void MIGLWidget::OnGeomClearhbonds()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    std::list<Molecule*>::iterator node = GetDisplaylist()->begin();
    std::list<Molecule*>::iterator end = GetDisplaylist()->end();
    while (node != end)
    {
        (*node)->ClearHbonds();
        node++;
    }

    ReDraw();
}

void MIGLWidget::OnSchematicSecondaryStructure()
{
    Molecule *node = GetDisplaylist()->CurrentItem();
    if (node != NULL)
    {
        int clear = false;
        clear = QMessageBox::question(this, "Show Schematic", "Do you want to hide residue atoms?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
        if (clear)
        {
            std::string resshow;
            std::string at("*");
            node->Select(1, 0, 0, 1, resshow, at, NULL, NULL, 0, Colors::YELLOW, Colors::COLORSECOND, 0, 0);
        }
        node->MakeSecondaryStructure(false, true);
        ReDraw();
    }
}

void MIGLWidget::OnRibbonSecondaryStructure()
{
    Molecule *node = GetDisplaylist()->CurrentItem();
    if (node != NULL)
    {
        int clear = false;
        clear = QMessageBox::question(this, "Show Ribbons", "Do you want to hide residue atoms?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
        if (clear)
        {
            std::string resshow;
            std::string at("*");
            node->Select(1, 0, 0, 1, resshow, at, NULL, NULL, 0, Colors::YELLOW, Colors::COLORSECOND, 0, 0);
        }
        node->MakeSecondaryStructure(true, false);
        ReDraw();
    }
}

void MIGLWidget::OnDeleteSecondaryStructure()
{
    Molecule *node = GetDisplaylist()->CurrentItem();
    if (node != NULL)
    {
        node->DeleteSecondaryStructure();
        ReDraw();
    }
}

void MIGLWidget::OnSecondaryStructureOptionsTube()
{
    promptSecondaryStructureOptions("tube");
}
void MIGLWidget::OnSecondaryStructureOptionsSheet()
{
    promptSecondaryStructureOptions("sheet");
}
void MIGLWidget::OnSecondaryStructureOptionsTurn()
{
    promptSecondaryStructureOptions("turn");
}
void MIGLWidget::OnSecondaryStructureOptionsRandom()
{
    promptSecondaryStructureOptions("random");
}
void MIGLWidget::OnSecondaryStructureOptionsHelix()
{
    promptSecondaryStructureOptions("helix");
}

void MIGLWidget::promptSecondaryStructureOptions(const std::string &type)
{
    std::string prefix("Secondary Structure/");
    prefix += type + "/";

    GenericDataDialog dlg(this);
    dlg.setWindowTitle("Secondary Structure Options");
    double width;
    double thickness;
    bool square;
    double radius;
    long red;
    long green;
    long blue;
    QSettings settings;
    if (type == "helix")
    {
        radius = settings.value((prefix + "radius").c_str(), 2.5).toDouble();
        dlg.addDoubleField("Helix radius:", radius);
    }
    else
    {
        double defaultWidth = 1.0;
        double defaultThickness = 1.0;
        bool defaultSquare = true;
        long defaultRed = 0;
        long defaultGreen = 0;
        long defaultBlue = 0;
        if (type == "tube")
        {
            defaultWidth = 0.8;
            defaultThickness = 0.8;
            defaultSquare = false;
            defaultRed = 0;
            defaultGreen = 0;
            defaultBlue = 255;
        }
        else if (type == "sheet")
        {
            defaultWidth = 2.0;
            defaultThickness = 1.0;
            defaultSquare = true;
            defaultRed = 255;
            defaultGreen = 255;
            defaultBlue = 0;
        }
        else if (type == "turn")
        {
            defaultWidth = 0.9;
            defaultThickness = 0.9;
            defaultSquare = false;
            defaultRed = 0;
            defaultGreen = 0;
            defaultBlue = 255;
        }
        else if (type == "random")
        {
            defaultWidth = 1.8;
            defaultThickness = 0.9;
            defaultSquare = false;
            defaultRed = 0;
            defaultGreen = 255;
            defaultBlue = 0;
        }
        width = settings.value((prefix + "width").c_str(), defaultWidth).toDouble();
        thickness = settings.value((prefix + "thickness").c_str(), defaultThickness).toDouble();
        square = settings.value((prefix + "square").c_str(), defaultSquare).toBool();
        red = settings.value((prefix + "red").c_str(), static_cast<int>(defaultRed)).toInt();
        green = settings.value((prefix + "green").c_str(), static_cast<int>(defaultGreen)).toInt();
        blue = settings.value((prefix + "blue").c_str(), static_cast<int>(defaultBlue)).toInt();
        dlg.addDoubleField("Width:", width);
        dlg.addDoubleField("Thickness:", thickness);
        dlg.addBoolField("Square:", square);
        dlg.addColorField("Color:", QColor(red, green, blue));
    }

    if (dlg.exec() == QDialog::Accepted)
    {
        if (type == "helix")
        {
            radius = dlg.value(0).toDouble();
            settings.setValue((prefix + "radius").c_str(), radius);
        }
        else
        {
            width = dlg.value(0).toDouble();
            thickness = dlg.value(1).toDouble();
            square = dlg.value(2).toBool();
            QColor color = dlg.value(3).value<QColor>();
            settings.setValue((prefix + "width").c_str(), width);
            settings.setValue((prefix + "thickness").c_str(), thickness);
            settings.setValue((prefix + "square").c_str(), square);
            settings.setValue((prefix + "red").c_str(), color.red());
            settings.setValue((prefix + "green").c_str(), color.green());
            settings.setValue((prefix + "blue").c_str(), color.blue());
        }
        Molecule *node = GetDisplaylist()->CurrentItem();
        if (node != NULL)
        {
            node->MakeSecondaryStructure(
                node->isSecondaryStructureRibbons(),
                node->isSecondaryStructureSchematic());
            ReDraw();
        }
    }
}

void MIGLWidget::OnObjectBackboneribbon()
{
    int do_single = QMessageBox::No;
    int clear = false;
    if (GetDisplaylist()->NumberItems() > 1)
    {
        do_single = QMessageBox::question(this, "Make Ribbons", "Do you want to build ribbons for all models?\n(No builds ribbons for just the current, active model)", QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (do_single == QMessageBox::Cancel)
        {
            return;
        }
    }
    clear = QMessageBox::question(this, "Make Ribbons", "Do you want to hide residue atoms?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
    std::string resshow;
    std::string at("*");
    if (do_single == QMessageBox::No)
    {
        Molecule *node = GetDisplaylist()->CurrentItem();
        if (node)
        {
            node->Do();
            if (clear)
            {
                node->Select(1, 0, 0, 1, resshow, at, NULL, NULL, 0, Colors::YELLOW, Colors::COLORSECOND, 0, 0);
            }
            node->BuildRibbons();
        }
    }
    else
    {
        std::list<Molecule*>::iterator node = GetDisplaylist()->begin();
        std::list<Molecule*>::iterator end = GetDisplaylist()->end();
        while (node != end)
        {
            (*node)->Do();
            if (clear)
            {
                (*node)->Select(1, 0, 0, 1, resshow, at, NULL, NULL, 0, Colors::YELLOW, Colors::COLORSECOND, 0, 0);
            }
            (*node)->BuildRibbons();
            node++;
        }
    }
    ReDraw();
}

void MIGLWidget::OnObjectClearribbon()
{
    std::list<Molecule*>::iterator node = GetDisplaylist()->begin();
    std::list<Molecule*>::iterator end = GetDisplaylist()->end();
    while (node != end)
    {
        (*node)->ClearRibbons();
        node++;
    }

    ReDraw();
}

void MIGLWidget::OnGeomAddsinglehbond()
{
    MIAtom *a1;
    MIAtom *a2;
    Molecule *mol1, *mol2;
    Residue *res1, *res2;
    AtomStack->Pop(a1, res1, mol1);
    AtomStack->Pop(a2, res2, mol2);
    if (!a2 || !res2 || !mol2 || !a1 || !res1 || !mol1)
    {
        Logger::message("First pick two atoms and then Add Bond");
        return;
    }
    if (mol1 != mol2)
    {
        Logger::message("Both ends of bonds must be in the same molecule");
        return;
    }
    mol1->AddHBond(a1, a2);
    PaletteChanged = true;
    ReDraw();
}

void MIGLWidget::OnUpdateGeomAddsinglehbond(QAction *action)
{
    action->setEnabled(!AtomStack->empty());
}

void MIGLWidget::OnViewContacts()
{
    ShowContacts = !ShowContacts;
    QSettings().setValue("View Parameters/ShowContacts", ShowContacts ? 1 : 0);
    scene->ShowContacts = ShowContacts;
    ReDraw();
}

void MIGLWidget::OnUpdateViewContacts(QAction *action)
{
    action->setChecked(ShowContacts != 0);
}

static Molecule *findMolecule(Displaylist *displaylist, const std::string &str)
{
    for (std::list<Molecule*>::iterator p = displaylist->begin(); p != displaylist->end(); ++p)
    {
        if ((*p)->compound == str)
            return *p;

        if ((*p)->pathname == str)
            return *p;
    }
    return 0;
}


void MIGLWidget::OnFitLsqsuperpose()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    if (GetDisplaylist()->NumberItems() == 0)
    {
        return;
    }

    static LSQFitDialog *dlg = new LSQFitDialog(this);
    dlg->setWindowTitle("Least-Squares Superposition");
    dlg->InitializeFromData();
    if (dlg->exec() != QDialog::Accepted)
        return;
    LSQFitDialog::Data data =  dlg->GetData();

    Molecule *source = findMolecule(GetDisplaylist(), data.sourceModel);
    Molecule *target = findMolecule(GetDisplaylist(), data.targetModel);
    if (!source || !target)
        return;

    ResidueListIterator res = source->residuesBegin();
    char m_chain = res->getChainId();
    float tx, ty, tz, x, y, z;
    bool chain_only = data.applyToChain;
    if (chain_only)
    {
        m_chain = data.chainId[0];
    }
    // save matrix for later
    LSQMatrix lsq_matrix;
    float r[3][3], v[3];
    r[0][0] = data.r00;
    r[0][1] = data.r01;
    r[0][2] = data.r02;

    r[1][0] = data.r10;
    r[1][1] = data.r11;
    r[1][2] = data.r12;

    r[2][0] = data.r20;
    r[2][1] = data.r21;
    r[2][2] = data.r22;

    v[0] = data.v0;
    v[1] = data.v1;
    v[2] = data.v2;


    lsq_matrix.SetMatrix(r, v);
    // Checkpoint the model
    std::string s = ::format("Before LSQ of %s onto %s", source->compound.c_str(), target->compound.c_str());
    if (!SaveModelFile(source, s.c_str()))
    {
        Logger::message("Warning: Unable to save model - will not be able to Undo");
    }
    for (; res != source->residuesEnd(); ++res)
    {
        if (chain_only && res->getChainId() != m_chain)
        {
            continue;
        }
        for (int i = 0; i < res->atomCount(); i++)
        {
            tx = res->atom(i)->x();
            ty = res->atom(i)->y();
            tz = res->atom(i)->z();
            x = lsq_matrix.Xvalue(tx, ty, tz);
            y = lsq_matrix.Yvalue(tx, ty, tz);
            z = lsq_matrix.Zvalue(tx, ty, tz);
            res->atom(i)->setPosition(x, y, z);
        }
    }
    Modify(true);
    source->SetCoordsChanged(true);
    ReDraw();
}

void MIGLWidget::OnFitFitmolecule()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    MIAtom *a;
    if (FitOK())
    {
        AtomStack->Pop(a, fitres, fitmol);
        if (a && fitres)
        {
            SelectType = MOLECULE;
            if (SetupFit(NULL, NULL, fitmol))
            {
                _modified = true;
                fitcenter.copyShallow(*a);
                fromcenter = a;
                ReDraw();
            }
        }
    }
}

void MIGLWidget::OnUpdateFitFitmolecule(QAction *action)
{
    action->setEnabled(!AtomStack->empty()  && MIBusyManager::instance()->Busy() == false);
    action->setChecked(IsFitting() != false && SelectType == MOLECULE);
}

void MIGLWidget::Purge(MIAtom *atom)
{
    if (atom && atom->type() & AtomType::SYMMATOM)
        return;
    savefits.Purge(atom);
    if (batonatom == atom)
    {
        batonatom = NULL;
    }
    if (fromcenter == atom)
    {
        fromcenter = NULL;
    }

    if (CurrentAtoms.size() > 0 && std::find(CurrentAtoms.begin(), CurrentAtoms.end(), atom) != CurrentAtoms.end())
    {
        cancelFitting();
    }
}

void MIGLWidget::Purge(Residue *res)
{
    // savefits.Purge(res); // doesn't exist, handled by atom purge

    size_t i;
    Residue *r;
    if (Monomer::isValid(focusres) && focusres == res)
    {
        setFocusResidue(focusres->next(), false, true);
    }
    // check to see if res in Pentamer
    if (Monomer::isValid(PentamerStart) && MIMoleculeBase::isValid(PentamerModel) && MIMoleculeBase::isValid(PentamerFrom))
    {
        r = PentamerStart;
        for (i = 0; i < 5; i++)
        {
            if (r == res)
            {
                clearPentamer();
                break;
            }
            if (Monomer::isValid(r))
                r = r->next();
        }
    }
    for (int j = 0; j < res->atomCount(); j++)
    {
        Purge(res->atom(j));
    }

    // code above may try to clear these, but this is a last-ditch effort
    // to ensure that they're cleared
    if (PentamerStart == res)
    {
        PentamerStart = NULL;
    }
    if (fitres == res)
    {
        fitres = NULL;
    }
    if (focusres == res)
    {
        focusres = NULL;
    }
}


void MIGLWidget::RemoveFileFromHistory(const std::string& /* pathname */)
{
#ifdef DO_ADD_TO_HISTORY
    wxFileHistory *history = MainFrame::GetMainFrame()->GetDocManager()->GetFileHistory();
    for (int i = history->GetCount()-1; i >= 0;  --i)
    {
        if (history->GetHistoryFile(i) == pathname)
        {
            history->RemoveFileFromHistory(i);
        }
    }
#endif
}

void MIGLWidget::OnInvertChiralCenter()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    Displaylist *Models = GetDisplaylist();

    MIAtom *a;
    Residue *res;
    Molecule *node;

    AtomStack->Pop(a, res, node);
    if (!a || !res || !node)
    {
        return;
    }

    Models->SetCurrent(node);

    std::string result;
    if (InvertChiralCenter(a, node->getBonds(), result))
    {
        Logger::log("Inverted chirality of atom %s\n", a->name());

        node->SetModified();
        Modify(true);
        ReDrawAll();
    }
    else
    {
        Logger::message(result);
    }
}

void MIGLWidget::OnUpdateInvertChiralCenter(QAction *action)
{
    action->setEnabled(!AtomStack->empty());
}

void MIGLWidget::OnViewChiralCenters()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    Displaylist *Models = GetDisplaylist();

    MIAtom *a;
    Residue *res;
    Molecule *node;

    AtomStack->Pop(a, res, node);
    if (!a || !res || !node)
    {
        return;
    }

    Models->SetCurrent(node);

    std::string result;
    GuessBondOrders(res, node->getBonds());
    result = FindChiralCenters(res, node->getBonds());
    result += "\n";
    Logger::message(result.c_str());

    node->SetModified();
    Modify(true);
    ReDrawAll();
}

void MIGLWidget::OnUpdateViewChiralCenters(QAction *action)
{
    action->setEnabled(!AtomStack->empty());
}

//#endif

void MIGLWidget::OnFitDeleteresidue()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    deleteResidueOnTopOfStack();
}

void MIGLWidget::deleteResidueOnTopOfStack()
{
    Displaylist *Models = GetDisplaylist();
    MIAtom *a;
    Residue *res;
    Molecule *node;
    std::string mess;
    AtomStack->Pop(a, res, node);
    if (!MIAtom::isValid(a) || !Residue::isValid(res) || !Molecule::isValid(node))
    {
        return;
    }
    if (!DontConfirmWater || !IsWater(res))
    {
        mess = "Are you sure you want to delete ";
        mess += resid(res).c_str();
        mess += "?";
        if (QMessageBox::question(this, "Confirm", mess.c_str(), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        {
            return;
        }
    }
    Models->SetCurrent(node);
    mess = ::format("Deleted residue %s", resid(res).c_str());
    SaveModelFile(node, mess.c_str());
    node->DeleteRes(res);
    Modify(true);
    node->SetModified();
    ReDraw();
}

void MIGLWidget::OnUpdateFitDeleteresidue(QAction *action)
{
    action->setEnabled(!AtomStack->empty() && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnFitInsertresidue()
{
    addResidueWithPrompt();
}

void MIGLWidget::addResidueWithPrompt()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }

    Residue *newres;
    Molecule *model = GetDisplaylist()->CurrentItem();
    if (!model)
    {
        Logger::message("There is no model - use Model/New Model...\nto start a new model and then try again");
        return;
    }

    MIAtom *a = NULL;
    Residue *atres = NULL;
    std::string chainStr;
    if (!AtomStack->empty() && model->residuesBegin() != model->residuesEnd())
    {
        AtomStack->Pop(a, atres, model);
        chainStr = std::string(1, (char)(atres->chain_id()&255));
    }

    AddResidueDialog addResidueDialog(MIFitDictionary()->GetDictResList());
    if (addResidueDialog.exec() != QDialog::Accepted)
        return;

    unsigned short chainId = static_cast<unsigned short>(addResidueDialog.chainId());
    unsigned int where = static_cast<unsigned int>(addResidueDialog.modelLocation());
    unsigned int m_phipsi = static_cast<unsigned int>(addResidueDialog.residueOffset());
    Residue *m_res = GetDictRes(addResidueDialog.residueType().toAscii().constData());

    if (m_res == NULL)
    {
        Logger::message("Error - nothing picked or residue not found in dictionary");
        return;
    }
    Residue *rtemp = MIFitDictionary()->GetBeta2();
    if (m_phipsi == 2)
    {
        rtemp = MIFitDictionary()->GetAlpha2();
    }
    // Checkpoint the model
    std::string s = ::format("Before inserting new residue %s", m_res->type().c_str());
    if (!SaveModelFile(model, s.c_str()))
    {
        Logger::message("Warning: Unable to save model - will not be able to Undo");
    }

    std::string newname;
    //newres = NULL;
    if (where == 3 || where == 2)
    {
        newres = model->InsertRes(NULL, m_res, where, chainId);
        newname = "1";
        m_phipsi = 0;
    }
    else if (atres != NULL)
    {
        if (where == 0)
        {
            newname = ftoa((int)(atof(atres->name().c_str())+1));
        }
        else
        {
            newname = ftoa((int)(atof(atres->name().c_str())-1));
        }
        newres = model->InsertRes(atres, m_res, where, chainId);
    }
    else
    {
        Logger::message("No selection - pick a residue to add after or choose another option");
        return;
    }
    if (newres == NULL)
    {
        Logger::message("Error: add residue failure - could be bad where option");
        return;
    }
    newres->setName(newname);

    if (m_phipsi == 0 || rtemp == NULL)
    {
        // translate the new residue to the center
        float cx = viewpoint->center()[0];
        float cy = viewpoint->center()[1];
        float cz = viewpoint->center()[2];
        float dx = 0, dy = 0, dz = 0;
        int i;
        for (i = 0; i < newres->atomCount(); i++)
        {
            dx += newres->atom(i)->x()-cx;
            dy += newres->atom(i)->y()-cy;
            dz += newres->atom(i)->z()-cz;
        }
        dx /= newres->atomCount();
        dy /= newres->atomCount();
        dz /= newres->atomCount();
        for (i = 0; i < newres->atomCount(); i++)
        {
            newres->atom(i)->translate(-dx, -dy, -dz);
        }
    }
    else
    {
        Residue *fixed_end, *target_end;
        bool fragmentResiduesSwapped = false;
        if (where == 0)
        {
            fixed_end = rtemp;
            target_end = fixed_end->next();
        }
        else
        {
            fixed_end = rtemp->next();
            target_end = rtemp;
            // temporarily swap pointers
            fragmentResiduesSwapped = true;
            fixed_end->setNext(target_end);
            fixed_end->setPrev(NULL);
            target_end->setPrev(fixed_end);
            target_end->setNext(NULL);
        }
        if (MoveOnto(fixed_end, atres, 2))
        {
            if (where == 0)
            {
                // fix O at end of atres
                MIAtom *O1 = atom_from_name("O", *fixed_end);
                MIAtom *O2 = atom_from_name("O", *atres);
                if (O1 && O2)
                {
                    O2->copyPosition(*O1);
                }
            }
            else
            {
                MIAtom *H1 = atom_from_name("HN", *fixed_end);
                MIAtom *H2 = atom_from_name("HN", *atres);
                if (H1 && H2)
                {
                    H2->copyPosition(*H1);
                }
            }
            if (MoveOnto(newres, target_end, 1))
            {
                // fix O at end of atres
                MIAtom *O1 = atom_from_name("O", *target_end);
                MIAtom *O2 = atom_from_name("O", *newres);
                if (O1 && O2)
                {
                    O2->copyPosition(*O1);
                }
                MIAtom *H1 = atom_from_name("HN", *target_end);
                MIAtom *H2 = atom_from_name("HN", *newres);
                if (H1 && H2)
                {
                    H2->copyPosition(*H1);
                }
                if (m_phipsi == 2)
                {
                    newres->setSecstr('H');
                }
                if (m_phipsi == 3)
                {
                    newres->setSecstr('S');
                }
            }
        }
        if (fragmentResiduesSwapped)
        {
            target_end->setNext(fixed_end);
            target_end->setPrev(NULL);
            fixed_end->setPrev(target_end);
            fixed_end->setNext(NULL);
        }
    }
    if (m_phipsi == 1)
    {
        // best fit
        model->Build();
        if (!SetupFit(newres, atres, model))
        {
            return;
        }
        vector<TORSION> torsions;
        TORSION tor;
        GeomRefiner *geomrefi = MIFitGeomRefiner();
        TORSION *chi1, *chi2, *chi3, *chi4, *chi5;
        chi1 = geomrefi->dict.getTORSION(newres, "CHI1", NULL);
        if (chi1)
        {
            torsions.push_back(*chi1);
        }
        chi2 = geomrefi->dict.getTORSION(newres, "CHI2", NULL);
        if (chi2)
        {
            torsions.push_back(*chi2);
        }
        chi3 = geomrefi->dict.getTORSION(newres, "CHI3", NULL);
        if (chi3)
        {
            torsions.push_back(*chi3);
        }
        chi4 = geomrefi->dict.getTORSION(newres, "CHI4", NULL);
        if (chi4)
        {
            torsions.push_back(*chi4);
        }
        chi5 = geomrefi->dict.getTORSION(newres, "CHI5", NULL);
        if (chi5)
        {
            torsions.push_back(*chi5);
        }
        MIAtom *Cnew = atom_from_name("C", *newres);
        MIAtom *CAnew = atom_from_name("CA", *newres);
        if (Cnew && CAnew)
        {
            if (where == 0)
            {
                tor.setAtom2(CAnew);
                tor.atom3 = Cnew;
            }
            else
            {
                tor.setAtom2(Cnew);
                tor.atom3 = CAnew;
            }
            torsions.push_back(tor);
        }
        if (where == 0)
        {
            MIAtom *Cat = atom_from_name("C", *atres);
            MIAtom *CAat = atom_from_name("CA", *atres);
            if (Cat && CAat)
            {
                tor.setAtom2(CAat);
                tor.atom3 = Cat;
                torsions.push_back(tor);
            }
            MIAtom *Nnew = atom_from_name("N", *newres);
            if (Nnew && CAnew)
            {
                tor.setAtom2(Nnew);
                tor.atom3 = CAnew;
                torsions.push_back(tor);
            }
        }
        else
        {
            MIAtom *Nat = atom_from_name("N", *atres);
            MIAtom *CAat = atom_from_name("CA", *atres);
            if (Nat && CAat)
            {
                tor.setAtom2(CAat);
                tor.atom3 = Nat;
                torsions.push_back(tor);
            }
        }

        EMap *currentmap = GetDisplaylist()->GetCurrentMap();
        if (currentmap)
        {
            geomrefi->TorsionOptimize(CurrentAtoms, model, currentmap, torsions);
        }

        if (chi1)
        {
            free(chi1);
        }
        if (chi2)
        {
            free(chi2);
        }
        if (chi3)
        {
            free(chi3);
        }
        if (chi4)
        {
            free(chi4);
        }
        if (chi5)
        {
            free(chi5);
        }
    }

    setFocusResidue(newres, false);
    focusnode = model;
    fitres = newres;

    int nlinks = model->getnlinks();
    model->Build();
    if (nlinks > 0)
    {
        model->BuildLinks();
    }
    // Mark the model as dirty
    Modify(true);
    model->SetModified();
    // the following makes it easier for the user to save the model
    GetDisplaylist()->SetCurrent(model);
    PaletteChanged = true;
    ReDrawAll();

}

void MIGLWidget::OnUpdateFitInsertresidue(QAction *action)
{
    action->setEnabled( /*!AtomStack->empty()  &&*/ MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnFitRenameresidue()
{
    MIAtom *a;
    Residue *res;
    Molecule *node;
    AtomStack->Pop(a, res, node);
    if (!a || !res || !node)
    {
        return;
    }

    QString newname = QInputDialog::getText(this, "Rename Residue", "Enter new residue name:");
    if (newname.isEmpty())
    {
        return;
    }
    res->setName(newname.toStdString());
    ReDrawAll();
}

void MIGLWidget::OnUpdateFitRenameresidue(QAction *action)
{
    action->setEnabled(!AtomStack->empty());
}

void MIGLWidget::OnAnimateBlink()
{
    Blink = Blink^1;
    if (!TimerOn)
    {
        m_timer = startTimer(ClockTick);
        TimerOn = true;
    }
/*
       if(Blink){
       BlinkCounter =0;
       Molecule * first = GetDisplaylist()->FirstItem();
       if(first){
       first->Show();
       Molecule *second = first->getnext();
       if(second)
        second->Hide();
       }
       ReDraw();
       }*/
}

void MIGLWidget::OnUpdateAnimateBlink(QAction *action)
{
    action->setEnabled(GetDisplaylist()->NumberItems() > 1);
    action->setChecked(Blink != 0);
}



void MIGLWidget::OnSolidSurfaceCommand(QAction *action)
{
    std::vector<Molecule*> mols;
    std::vector<unsigned int> selected;
    solidSurfaceCommand(action->data().toInt(), mols, selected);
}

void MIGLWidget::solidSurfaceCommand(int id, std::vector<Molecule*> &mols, std::vector<unsigned int> &selected)
{

    MISurfaceSetCurrentView(this);

    switch (id)
    {
    case ID_SOLIDSURFACE_QUALITY_0:
        MISetSurfaceQuality(0);
        break;
    case ID_SOLIDSURFACE_QUALITY_1:
        MISetSurfaceQuality(1);
        break;
    case ID_SOLIDSURFACE_QUALITY_2:
        MISetSurfaceQuality(2);
        break;
    case ID_SOLIDSURFACE_MOLECULAR:
        MISetSurfaceType(MISurfaceType::Molecular);
        break;
    case ID_SOLIDSURFACE_ACCESSIBLE:
        MISetSurfaceType(MISurfaceType::Accessible);
        break;
    case ID_SOLIDSURFACE_ATOMS_MODE:
        MISetSurfaceSelectionMode(MISurfaceSelectionMode::AtomsOnly);
        break;
    case ID_SOLIDSURFACE_RESIDUE_MODE:
        MISetSurfaceSelectionMode(MISurfaceSelectionMode::SingleResidue);
        break;
    case ID_SOLIDSURFACE_RESIDUES_MODE:
        MISetSurfaceSelectionMode(MISurfaceSelectionMode::Residues);
        break;
    case ID_SOLIDSURFACE_PEPTIDE_MODE:
        MISetSurfaceSelectionMode(MISurfaceSelectionMode::Peptide);
        break;
    case ID_SOLIDSURFACE_RESMOOTH:
    {
        bool ok;
        unsigned int val = static_cast<unsigned int>(QInputDialog::getInt(this, "Smoothing Steps", "Set the number of smoothing steps", MIGetSurfaceSmoothingSteps(), 0, 5, 1, &ok));
        if (ok)
        {
            MISetSurfaceSmoothingSteps(val);
        }
        break;
    }
    case ID_SOLIDSURFACE_MOLECULE_MODE:
        MISetSurfaceSelectionMode(MISurfaceSelectionMode::EntireMolecule);
        break;
    case ID_SOLIDSURFACE_CLEAR:
    {
        MIDeleteSurface(solidsurf_current_surface);
        if (solidsurf_current_surface >= MISurfaceCount())
        {
            solidsurf_current_surface = MISurfaceCount()-1;
        }
        break;
    }

    // these all use selection
    case ID_SOLIDSURFACE_BUILD:
    {
        if (selected.size()==0)
        {
            MISurfaceVectorsFromStack(mols, selected, MIGetSurfaceSelectionMode(), AtomStack);
        }
        if (selected.size())
        {
            solidsurf_current_surface = MIMakeSurface(mols, MIGetSurfaceType(), selected.size(), &selected[0]);
        }
        break;
    }
    case ID_SOLIDSURFACE_COLOR:
    {
        short c = MIColorPickerDlg::getColor(this, 0);
        if (c >= 0)
        {
            if (selected.size()==0)
            {
                MISurfaceVectorsFromStack(mols, selected, MIGetSurfaceSelectionMode(), AtomStack);
            }
            if (selected.size())
            {
                MIColorSurface(solidsurf_current_surface, c, mols, selected.size(), &selected[0]);
            }
        }
        break;
    }
    case ID_SOLIDSURFACE_COLOR_BY_ATOM:
    {
        if (selected.size()==0)
        {
            MISurfaceVectorsFromStack(mols, selected, MIGetSurfaceSelectionMode(), AtomStack);
        }
        if (selected.size())
        {
            MIAtomColorSurface(solidsurf_current_surface, mols, selected.size(), &selected[0]);
        }
        break;
    }

    case ID_SOLIDSURFACE_ALPHA:
    {
        bool ok;
        double percent = MIGetSurfaceAlpha();
        percent = QInputDialog::getDouble(this, "Transparency percentage", "Transparency factor 0.0 (invisible) - 1.0 (solid)", percent, 0.0, 1.0, 2, &ok);
        if (ok)
        {
            MISetSurfaceAlpha(percent);
        }
        break;
    }

    case ID_SOLIDSURFACE_MINVAL:
    {
        bool ok;
        double val = MIGetSurfaceMinValue();
        val = QInputDialog::getDouble(this, "Minimum gradient", "Enter value to be used as the miniumn value for gradient color interpolation.  Enter 0.0 for min and max to use automatically determined values.", val, FLT_MIN, FLT_MAX, 4, &ok);
        if (ok)
        {
            MISetSurfaceMinValue(val);
        }
        break;
    }

    case ID_SOLIDSURFACE_MAXVAL:
    {
        bool ok;
        double val = MIGetSurfaceMaxValue();
        val = QInputDialog::getDouble(this, "Maximum gradient", "Enter value to be used as the maxiumn value for gradient color interpolation.  Enter 0.0 for min and max to use automatically determined values.", val, FLT_MIN, FLT_MAX, 4, &ok);
        if (ok)
        {

            MISetSurfaceMaxValue(val);
        }
        break;
    }

    case ID_SOLIDSURFACE_MINCOLOR:
    {
        int c = MIColorPickerDlg::getColor(this, MIGetSurfaceMinColor(), "Minimum gradient color");
        if (c != -1)
        {
            MISetSurfaceMinColor(c);
        }
        break;
    }

    case ID_SOLIDSURFACE_MAXCOLOR:
    {
        int c = MIColorPickerDlg::getColor(this, MIGetSurfaceMaxColor(), "Maximum gradient color");
        if (c != -1)
        {
            MISetSurfaceMaxColor(c);
        }
        break;
    }

    case ID_SOLIDSURFACE_GRADCOLOR:
    {
        MISetSurfaceUseGradColor(!MIGetSurfaceUseGradColor());
        break;
    }

    case ID_SOLIDSURFACE_CALCDIST:
    {
        if (selected.size()==0)
        {
            MISurfaceVectorsFromStack(mols, selected, MIGetSurfaceSelectionMode(), AtomStack);
        }
        if (selected.size())
        {
            MISurfaceCalcDistance(solidsurf_current_surface, mols, selected.size(), &selected[0]);
        }
        break;
    }

    case ID_SOLIDSURFACE_USESURF_0:
    case ID_SOLIDSURFACE_USESURF_1:
    case ID_SOLIDSURFACE_USESURF_2:
    case ID_SOLIDSURFACE_USESURF_3:
    case ID_SOLIDSURFACE_USESURF_4:
    case ID_SOLIDSURFACE_USESURF_5:
    case ID_SOLIDSURFACE_USESURF_6:
    case ID_SOLIDSURFACE_USESURF_7:
    case ID_SOLIDSURFACE_USESURF_8:
    case ID_SOLIDSURFACE_USESURF_9:
        solidsurf_current_surface = id - ID_SOLIDSURFACE_USESURF_0;
        if (solidsurf_current_surface >= MISurfaceCount())
        {
            solidsurf_current_surface = MISurfaceCount()-1;
        }
        break;



    default:
        Logger::debug("Unknown entry in solidsurface handler!");
        break;
    }
    ReDraw();
}

void MIGLWidget::OnObjectSurfaceresidue()
{
    Residue *res;
    Molecule *node;
    MIAtom *a;
    AtomStack->Pop(a, res, node);
    if (!res)
    {
        return;
    }

    GenericDataDialog *dlg = new GenericDataDialog(this);
    dlg->setWindowTitle("Surface");
    dlg->addDoubleField("Dots/Angstrom (1-10):", 3.0f);
    dlg->addBoolField("Ignore Hidden Atoms:", true);
    if (dlg->exec() == QDialog::Accepted)
    {
        float dotsper = static_cast<float>(dlg->value(0).toDouble());
        bool ignoreHidden = dlg->value(1).toBool();
        if (dotsper >= 1.0f && dotsper <= 10.0f)
        {
            _modified = true;
            node->SurfaceResidue(res, (1.0f/dotsper), ignoreHidden);
            PaletteChanged = true;
            ReDraw();
        }
    }
}

void MIGLWidget::OnUpdateObjectSurfaceresidue(QAction *action)
{
    action->setEnabled(!AtomStack->empty());

}

void MIGLWidget::OnObjectSurfaceClearsurface()
{
    Displaylist *Models = GetDisplaylist();
    Molecule *node = Models->CurrentItem();
    if (node)
    {
        node->FreeDots();
        _modified = true;
        ReDraw();
    }
}

void MIGLWidget::OnObjectSurfaceresidues()
{
    Residue *res;
    Molecule *node;
    MIAtom *a;
    AtomStack->Pop(a, res, node);
    if (!a)
        return;

    static double dotsper = 3.0;
    static bool ignoreHidden = true;

    GenericDataDialog dlg(this);
    dlg.setWindowTitle("Surface");
    dlg.addDoubleField("Dots/Angstrom (1-10):", dotsper);
    dlg.addBoolField("Ignore Hidden Atoms:", ignoreHidden);
    if (dlg.exec() == QDialog::Accepted)
    {
        double newDotsper = dlg.value(0).toDouble();
        if (newDotsper >= 1.0f && newDotsper <= 10.0f)
        {
            dotsper = newDotsper;
            ignoreHidden = dlg.value(1).toBool();
            res->setFlags(res->flags()|128);
            while (!AtomStack->empty())
            {
                AtomStack->Pop(a, res, node);
                res->setFlags(res->flags()|128);
            }
            int numDots = node->SurfaceResidues(1.0f/dotsper,
                                                  ignoreHidden);
            _modified = true;
            QString message = QString("Number of dots: %1").arg(numDots);
            setMessage(message);
            Logger::log(message.toAscii().constData());
            PaletteChanged = true;
            ReDraw();
        }
    }
}

void MIGLWidget::OnUpdateObjectSurfaceresidues(QAction *action)
{
    action->setEnabled(!AtomStack->empty());
}

void MIGLWidget::OnUpdateObjectSurfaceatoms(QAction *action)
{
    action->setEnabled(!AtomStack->empty());
}

void MIGLWidget::OnObjectSurfaceatom()
{
    Residue *res;
    Molecule *node;
    MIAtom *a;
    AtomStack->Pop(a, res, node);
    if (!a)
        return;

    static double dotsper = 3.0;
    static bool ignoreHidden = true;

    GenericDataDialog dlg(this);
    dlg.setWindowTitle("Surface");
    dlg.addDoubleField("Dots/Angstrom (1-10):", dotsper);
    dlg.addBoolField("Ignore Hidden Atoms:", ignoreHidden);
    if (dlg.exec() == QDialog::Accepted)
    {
        double newDotsper = dlg.value(0).toDouble();
        if (newDotsper >= 1.0f && newDotsper <= 10.0f)
        {
            dotsper = newDotsper;
            ignoreHidden = dlg.value(1).toBool();
            int numDots = node->SurfaceAtom(a, 1.0f/dotsper, ignoreHidden);
            _modified = true;
            QString message = QString("Number of dots: %1").arg(numDots);
            setMessage(message);
            Logger::log(message.toAscii().constData());
            PaletteChanged = true;
            ReDraw();
        }
    }
}

void MIGLWidget::OnUpdateObjectSurfaceAtom(QAction *action)
{
    action->setEnabled(!AtomStack->empty());
}

void MIGLWidget::OnObjectSurfaceAtoms()
{
    Residue *res;
    Molecule *node;
    MIAtom *a;
    AtomStack->Pop(a, res, node);
    if (!a)
        return;

    static double dotsper = 3.0;
    static bool ignoreHidden = true;

    GenericDataDialog dlg(this);
    dlg.setWindowTitle("Surface");
    dlg.addDoubleField("Dots/Angstrom (1-10):", dotsper);
    dlg.addBoolField("Ignore Hidden Atoms:", ignoreHidden);
    if (dlg.exec() == QDialog::Accepted)
    {
        double newDotsper = dlg.value(0).toDouble();
        if (newDotsper >= 1.0f && newDotsper <= 10.0f)
        {
            dotsper = newDotsper;
            ignoreHidden = dlg.value(1).toBool();
            int numDots = node->SurfaceAtom(a, 1.0f/dotsper, ignoreHidden);
            while (!AtomStack->empty())
            {
                AtomStack->Pop(a, res, node);
                numDots += node->SurfaceAtom(a, 1.0f/dotsper, ignoreHidden);
            }
            _modified = true;
            QString message = QString("Number of dots: %1").arg(numDots);
            setMessage(message);
            Logger::log(message.toAscii().constData());
            PaletteChanged = true;
            ReDraw();
        }
    }
}

void MIGLWidget::OnEditSelectmodel()
{
    Displaylist *Models = GetDisplaylist();
    Molecule *lastnode = Models->CurrentItem();
    if (!lastnode)
    {
        return;
    }
}

void MIGLWidget::OnMapLoadfromphsfile()
{
    mapLoadfromphsfile();
}

void MIGLWidget::mapLoadfromphsfile()
{
    // no longer support .sca, and .ref: the readers are broken

    const std::string &s = MIFileSelector("Select Phase File", "./", "", "phs;fcf;mtz;cif;fin",
                                          "All phase files (*.mtz, *.phs, *.fcf, *.cif)|*.mtz;*.phs;*.fcf;*.cif|"
                                          "CCP4 MTZ (*.mtz)|*.mtz|Phase files (*.phs)|*.phs|SHELX (*.fcf)|*.fcf|mmCIF (*.cif)|*.cif|All files (*.*)|*.*", MI_OPEN_MODE);
    mapLoadfromphsfile(s);
}



static Molecule *FindModel(const std::string &mname, Displaylist *dl, bool or_current = false)
{
    if (strncasecmp(mname.c_str(), "Model:", 6)==0)
    {
        std::string name = std::string(&mname[6]);
        Displaylist::ModelList::iterator modelIter = dl->begin();
        for (; modelIter != dl->end(); ++modelIter)
        {
            Molecule *model = *modelIter;
            if (std::string(model->pathname.c_str())==name)
                return model;
        }
    }
    if (!or_current)
        return 0;
    return dl->GetCurrentModel();
}

static EMap *CreateMap(Displaylist *dl,
                       const std::string &file,
                       const std::string &maptypestr,
                       const std::string &fo, const std::string &fc,
                       const std::string &fom, const std::string &phi,
                       int gridlevel, float resmin, float resmax, bool /* map2 */)
{
    EMap *map = new EMap;
    unsigned int maptype = MapTypeForString(maptypestr);
    map->UseColumnLabels(fo, fc, fom, phi, "", "FreeR_flag"); // only really used for MTZ, but no harm

    if (!EMapBase::IsCCP4MTZFile(file.c_str()))
    {
        Molecule *model = FindModel(fc, dl, true);
        if (model)
        {
            *map->mapheader = model->GetMapHeader();
        }
        else
        {
            std::string data = CMapHeaderBase().Label();
            SelectCrystal::doSelectCrystal(data);
            CMapHeaderBase mh(data);
            map->mapheader->updateSymmetryAndCell(mh);
            map->RecalcResolution();
        }
    }

    if (map->LoadMapPhaseFile(file.c_str()))
    {
        if (maptypestr == std::string("Direct FFT"))
        {
            map->fColumnName = fo;
        }

        Molecule *model = FindModel(fc, dl, false);
        if (model)
        {
            map->SFCalc(model->residuesBegin());
        }

        if (map->FFTMap(maptype, gridlevel, resmin, resmax))
        {
            // set contour options to regular or diff map type as appropriate
            if (maptypestr == std::string("Direct FFT"))
            {
                if (fo == "FWT" || fo == "2FOFCWT")
                {
                    GetMapSettingsForStyle(REGULAR_MAP_SETTINGS, *map->GetSettings());
                }
                else if (fo == "DELFWT" || fo == "FOFCWT")
                {
                    GetMapSettingsForStyle(DIFFERENCE_MAP_SETTINGS, *map->GetSettings());
                }
                else
                {
                    GetMapSettingsForStyle(
                        map->isPredictedAsDifferenceMap() ?  DIFFERENCE_MAP_SETTINGS : REGULAR_MAP_SETTINGS,
                        *map->GetSettings());
                }
            }
            else
            {
                GetMapSettingsForStyle(
                    IsRegularMapType(maptype) ?  REGULAR_MAP_SETTINGS : DIFFERENCE_MAP_SETTINGS,
                    *map->GetSettings());
            }
            return map;
        }
    }

    delete map;
    return 0;
}


void MIGLWidget::mapLoadfromphsfile(const std::string &file)
{
    //build vector of model names
    std::vector<std::string> modelNames, cellList;
    Displaylist::ModelList::iterator modelIter = GetDisplaylist()->begin();
    for (; modelIter!=GetDisplaylist()->end(); ++modelIter)
    {
        Molecule *model = *modelIter;
        modelNames.push_back(model->pathname.c_str());
        // set cell information so resolution can be calculated
        CMapHeaderBase &mh = model->GetMapHeader();
        std::string cell = ::format("%f %f %f %f %f %f", mh.a, mh.b, mh.c,
                                    mh.alpha, mh.beta, mh.gamma);
        cellList.push_back(cell);
    }

    // pass control off to dialog
    PhaseFileLoadDialog dlg(this);
    dlg.setWindowTitle("Phase file import");
    if (!dlg.SetFile(file, modelNames, cellList))
    {
        return;
    }
    if (dlg.exec() != QDialog::Accepted)
    {
        return;
    }
    PhaseFileLoadDialog::Data data;
    dlg.GetData(data);

    int gridlevel = data.grid;
    float resmin = data.resmin;
    if (resmin==0.0)
        resmin = -1.0f;
    float resmax = data.resmax;
    if (resmax==0.0)
        resmax = -1.0f;

    if (data.enabled1)
    {
        EMap *map = CreateMap(GetDisplaylist(),
                              file, data.type1,
                              data.fo1.c_str(),  data.fc1.c_str(),
                              data.fom1.c_str(), data.phi1.c_str(),
                              gridlevel, resmin, resmax, true);
        if (map)
        {
            GetDisplaylist()->AddMap(map);
            doMapContour(map);
        }
    }

    if (data.enabled2)
    {
        EMap *map = CreateMap(GetDisplaylist(),
                              file, data.type2,
                              data.fo2.c_str(),  data.fc2.c_str(),
                              data.fom2.c_str(), data.phi2.c_str(),
                              gridlevel, resmin, resmax, true);
        if (map)
        {
            GetDisplaylist()->AddMap(map);
            doMapContour(map);
        }
    }
}


void MIGLWidget::OnMapLoadfromfile()
{
    mapLoadfromfile();
}

void MIGLWidget::mapLoadfromfile()
{
    const std::string &s = MIFileSelector("Select CCP4 or FSFOUR/XtalView Map File", "./", "", "map",
                                          "Map files (*.map)|*.map|All files (*.*)|*.*", MI_OPEN_MODE);
    mapLoadfromfile(s);
}

void MIGLWidget::mapLoadfromfile(const std::string &file)
{
    if (file.size() > 0)
    {
        EMap *map = new EMap;
        Molecule *model = GetDisplaylist()->GetCurrentModel();
        if (model != NULL)
        {
            *map->mapheader = model->GetMapHeader();
        }
        else
        {
            std::string data = CMapHeaderBase().Label();
            SelectCrystal::doSelectCrystal(data);
            CMapHeaderBase mh(data);
            map->mapheader->updateSymmetryAndCell(mh);
        }

        if (map->LoadMapFile(file.c_str()))
        {
            GetMapSettingsForStyle(REGULAR_MAP_SETTINGS, *map->GetSettings());
            GetDisplaylist()->AddMap(map);
            doMapContour(map);
        }
        else
        {
            delete map;
        }
    }
}


void MIGLWidget::OnMapContour()
{
    doMapContour(GetDisplaylist()->GetCurrentMap());
}

void MIGLWidget::mapVisibilityChanged(EMapBase *map)
{
    doMapContour(map);
}

void MIGLWidget::mapContourLevelsChanged(EMapBase *emap)
{
    doMapContour(emap);
}

void MIGLWidget::doMapContour(EMapBase *emap)
{
    if (emap == NULL)
    {
        Logger::log("Sorry there is no map to refine against - action canceled");
        return;
    }
    if (!emap->HasDensity())
    {
        Logger::log("Sorry there is no map density to refine against - action canceled");
        return;
    }
    if (emap->Visible())
    {
        float center[3];
        center[0] = viewpoint->center()[0];
        center[1] = viewpoint->center()[1];
        center[2] = viewpoint->center()[2];
        emap->Contour(center, &CurrentAtoms);
        _modified = true;
    }
    doRefresh();
}

void MIGLWidget::OnUpdateMapContour(QAction *action)
{
    action->setEnabled(GetDisplaylist()->GetCurrentMap() != NULL);
}

void MIGLWidget::OnSequenceEnter()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    Displaylist *Models = GetDisplaylist();
    if (Models->CurrentItem() == NULL)
    {
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Enter Sequence");
    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    layout->addWidget(new QLabel("Type in a sequence or paste it from another source", &dlg));
    QTextEdit *text = new QTextEdit(&dlg);
    layout->addWidget(text);
    layout->addWidget(new QDialogButtonBox(&dlg));
    if (dlg.exec() == QDialog::Accepted)
    {
        Models->CurrentItem()->SetSequence(text->toPlainText().toAscii().constData());
        PaletteChanged = true;
        ReDraw();
    }
}

void MIGLWidget::OnUpdateSequenceEnter(QAction *action)
{
    Displaylist *Models = GetDisplaylist();
    action->setEnabled(Models->CurrentItem() != NULL  && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnSequenceRead()
{
    readLowerSequenceWithPrompt();
}

void MIGLWidget::readLowerSequenceWithPrompt()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    Displaylist *Models = GetDisplaylist();
    if (!Models->CurrentItem())
        return;
    std::string s = MIFileSelector("Select .seq File", "", "", "seq",
                                   "Sequence files (*.seq)|*.seq| All files (*.*)|*.*", MI_OPEN_MODE);
    readLowerSequence(s);
}

void MIGLWidget::readLowerSequence(const std::string &file)
{
    if (file.size())
    {
        Displaylist *Models = GetDisplaylist();
        Models->CurrentItem()->ReadSequence(file.c_str(), SEQ_FORMAT_SIMPLE, 0);
        ReDrawAll();
    }
}

void MIGLWidget::OnUpdateSequenceRead(QAction *action)
{
    Displaylist *Models = GetDisplaylist();
    action->setEnabled(Models->CurrentItem() != NULL  && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnSequenceSave()
{
    Displaylist *Models = GetDisplaylist();
    if (!Models->CurrentItem())
        return;
    std::string s = MIFileSelector("Select .seq File", "", "", "seq",
                                   "Sequence files (*.seq)|*.seq| All files (*.*)|*.*", MI_SAVE_MODE);
    if (s.size())
    {
        Models->CurrentItem()->WriteSequence(s.c_str(), SEQ_FORMAT_SIMPLE);
    }
}

void MIGLWidget::OnUpdateSequenceSave(QAction *action)
{
    Displaylist *Models = GetDisplaylist();
    action->setEnabled(Models->CurrentItem() != NULL);
}

void MIGLWidget::OnSequenceInsertgap()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    Molecule *current = GetDisplaylist()->CurrentItem();
    if (!current)
        return;

    Residue *res;
    Molecule *node;
    MIAtom *a;
    AtomStack->Pop(a, res, node);
    if (!a)
    {
        return;
    }
    AtomStack->Push(a, res, node);
    if (node != current)
    {
        return;
    }
    node->InsertGap(res);
    QString message = QString("Sequence identities: %1")
            .arg(node->SequenceIdentities());
    setMessage(message);
    Logger::log(message.toAscii().constData());
    PaletteChanged = true;
    ReDraw();
}

void MIGLWidget::OnUpdateSequenceInsertgap(QAction *action)
{
    action->setEnabled(!AtomStack->empty() && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnSequenceDeletegap()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    Molecule *current = GetDisplaylist()->CurrentItem();
    if (!current)
        return;
    Residue *res;
    Molecule *node;
    MIAtom *a;
    AtomStack->Pop(a, res, node);
    if (!a)
    {
        return;
    }
    AtomStack->Push(a, res, node);
    if (node != current)
    {
        return;
    }
    node->DeleteGap(res);
    QString message = QString("Sequence identities: %1")
            .arg(node->SequenceIdentities());
    setMessage(message);
    Logger::log(message.toAscii().constData());
    PaletteChanged = true;
    ReDraw();
}

void MIGLWidget::OnUpdateSequenceDeletegap(QAction *action)
{
    action->setEnabled(!AtomStack->empty() && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnSequenceInsertlowergap()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    Molecule *current = GetDisplaylist()->CurrentItem();
    if (!current)
        return;
    Residue *res;
    Molecule *node;
    MIAtom *a;
    AtomStack->Pop(a, res, node);
    if (!a)
    {
        return;
    }
    AtomStack->Push(a, res, node);
    if (node != current)
    {
        return;
    }
    node->InsertLowerGap(res);
    QString message = QString("Sequence identities: %1")
            .arg(node->SequenceIdentities());
    setMessage(message);
    Logger::log(message.toAscii().constData());
    PaletteChanged = true;
    ReDraw();
}

void MIGLWidget::OnUpdateSequenceInsertlowergap(QAction *action)
{
    action->setEnabled(!AtomStack->empty() && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnSequenceDeletelowergap()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    Molecule *current = GetDisplaylist()->CurrentItem();
    if (!current)
        return;
    Residue *res;
    Molecule *node;
    MIAtom *a;
    AtomStack->Pop(a, res, node);
    if (!a)
    {
        return;
    }
    AtomStack->Push(a, res, node);
    if (node != current)
    {
        return;
    }
    node->DeleteLowerGap(res);
    QString message = QString("Sequence identities: %1")
            .arg(node->SequenceIdentities());
    Logger::log(message.toAscii().constData());
    PaletteChanged = true;
    ReDraw();
}

void MIGLWidget::OnUpdateSequenceDeletelowergap(QAction *action)
{
    action->setEnabled(!AtomStack->empty() && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnMapFFT()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    EMap *map = GetDisplaylist()->GetCurrentMap();
    if (!map)
    {
        return;
    }
    map->FFTMap();
    OnMapContour();
}

void MIGLWidget::OnUpdateMapFFT(QAction *action)
{
    EMap *map = GetDisplaylist()->GetCurrentMap();
    action->setEnabled(map != NULL && map->HasPhases() && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnMapSFCalc()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    EMap *map = GetDisplaylist()->GetCurrentMap();
    if (!map)
    {
        return;
    }
    Molecule *current = GetDisplaylist()->CurrentItem();
    if (current == NULL)
    {
        return;
    }
    Residue *res = current->residuesBegin();
    if (res == current->residuesEnd())
    {
        return;
    }
    if (map->SFCalc(res))
    {
        if (map->FFTMap())
        {
            OnMapContour();
        }
    }
    _modified = true;
    ReDraw();
}

void MIGLWidget::OnUpdateMapSFCalc(QAction *action)
{
    EMap *map = GetDisplaylist()->GetCurrentMap();
    bool HasPhases = false;
    bool hasResidue = false;
    if (map != NULL)
    {
        Molecule *current = GetDisplaylist()->CurrentItem();
        if (current != NULL)
        {
            hasResidue = current->residuesBegin() != current->residuesEnd();
        }
        HasPhases = map->HasPhases();
    }
    action->setEnabled(HasPhases && hasResidue && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnMapContourLevels()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    EMap *map = GetDisplaylist()->GetCurrentMap();
    if (!map)
    {
        return;
    }
    if (map->ContourLevels())
    {
        OnMapContour();
    }
}

void MIGLWidget::OnUpdateMapContourLevels(QAction *action)
{
    action->setEnabled(GetDisplaylist()->GetCurrentMap() != NULL && MIBusyManager::instance()->Busy() == false);
}

// this is now dead, as I've commented out the menu option for it
// use the tree instead
void MIGLWidget::OnMapSwitch()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    GetDisplaylist()->ChooseActiveMap();
}

void MIGLWidget::OnUpdateMapSwitch(QAction *action)
{
    action->setEnabled(GetDisplaylist()->MapCount() > 1 && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnRefiRegion()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    MIAtom *a;
    Residue *res, *res1, *res2, *r;
    Molecule *node;
    AtomStack->Pop(a, res, node);
    if (!a)
    {
        return;
    }
    if (res->next())
    {
        res2 = res->next();
    }
    else
    {
        res2 = res;
    }
    res1 = res;
    for (r = node->residuesBegin(); r != NULL; r = r->next())
    {
        if (r->next() == res)
        {
            res1 = r;
            break;
        }
    }

    if (!MIFitGeomRefiner()->SetRefiRes(res1, res2, node, GetDisplaylist()->GetCurrentMap()))
    {
        Logger::message("Set up Refinement failed - proabably because residue type  not in dictionary");
        return;
    }
    MIFitGeomRefiner()->Refine();
    UpdateCurrent();
    _modified = true;
    if (!AutoFit)
    {
        ReDraw();
    }
}

void MIGLWidget::OnRefiRange()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    refineRange();
}

void MIGLWidget::refineRange()
{
    MIAtom *a, *a2;
    Residue *res1, *res2;
    Molecule *node, *node2;
    AtomStack->Pop(a, res1, node);
    if (!a)
    {
        return;
    }
    AtomStack->Pop(a2, res2, node2);
    if (!a2 || node != node2)
    {
        return;
    }
    if (!MIFitGeomRefiner()->SetRefiRes(res1, res2, node, GetDisplaylist()->GetCurrentMap()))
    {
        Logger::message("Set up Refinement failed - proabably because residue type  not in dictionary");
        return;
    }
    MIFitGeomRefiner()->Refine();
    UpdateCurrent();
    _modified = true;
    if (!AutoFit)
    {
        ReDraw();
    }
}

void MIGLWidget::OnRefiUndo()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    MIFitGeomRefiner()->Undo();
    _modified = true;
    Modify(true);
    ReDraw();
}

void MIGLWidget::OnRefiOptions()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    GeomRefiner *g = MIFitGeomRefiner();

    RefinementOptionsDialog::Data data;

    data.bondWeight = g->GetBondWeightI();
    data.angleWeight = g->GetAngleWeightI();
    data.planeWeight = g->GetPlaneWeightI();
    data.torsionWeight = g->GetTorsionWeightI();
    data.bumpWeight = g->GetBumpWeightI();
    data.mapWeight = g->GetMapWeightI();

    data.constrainCA = g->dict.GetConstrainCA();
    data.constrainEnds = g->dict.GetConstrainEnds();
    data.verbose = g->GetVerbose();
    data.refineWhileFit = g->GetRefineWhileFit();

    data.sigmaBond = g->dict.GetSigmaBond();
    data.sigmaAngle = g->dict.GetSigmaAngle();
    data.sigmaPlane = g->dict.GetSigmaPlane();
    data.sigmaTorsion = g->dict.GetSigmaTorsion();
    data.sigmaBump = g->dict.GetSigmaBump();

    RefinementOptionsDialog dlg(data, this);
    dlg.setWindowTitle("Refinement Options");
    if (dlg.exec() != QDialog::Accepted)
    {
        return;
    }
    dlg.GetResults(data);

    g->SetBondWeightI(data.bondWeight);
    g->SetAngleWeightI(data.angleWeight);
    g->SetPlaneWeightI(data.planeWeight);
    g->SetTorsionWeightI(data.torsionWeight);
    g->SetBumpWeightI(data.bumpWeight);
    g->SetMapWeightI(data.mapWeight);

    g->dict.SetConstrainCA(data.constrainCA);
    g->dict.SetConstrainEnds(data.constrainEnds);
    g->SetVerbose(data.verbose);
    g->SetRefineWhileFit(data.refineWhileFit);

    g->dict.SetSigmaBond(data.sigmaBond);
    g->dict.SetSigmaAngle(data.sigmaAngle);
    g->dict.SetSigmaTorsion(data.sigmaTorsion);
    g->dict.SetSigmaPlane(data.sigmaPlane);
    g->dict.SetSigmaBump(data.sigmaBump);
}

void MIGLWidget::OnRefiMolecule()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    MIAtom *a;
    Residue *res, *res1, *res2 = NULL;
    Molecule *node;
    AtomStack->Pop(a, res1, node);
    if (!a)
    {
        return;
    }
    res = node->residuesBegin();
    res1 = res;
    while (res != NULL)
    {
        res2 = res;
        res = res->next();
    }
    if (!MIFitGeomRefiner()->SetRefiRes(res1, res2, node, GetDisplaylist()->GetCurrentMap()))
    {
        Logger::message("Set up Refinement failed - proabably because residue type  not in dictionary");
        return;
    }

    if (!MIFitGeomRefiner()->GetRefineWhileFit()) // if we are in refine while fit mode, UpdateCurrent will call refine for us
        MIFitGeomRefiner()->Refine();

    UpdateCurrent();
    _modified = true;
    ReDraw();
}

void MIGLWidget::OnUpdateRefiUndo(QAction *action)
{
    action->setEnabled(MIFitGeomRefiner()->IsRefining() != true && MIFitGeomRefiner()->CanUndo() && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnUpdateRefiRegion(QAction *action)
{
    action->setEnabled(!AtomStack->empty() && MIFitGeomRefiner()->IsRefining() != true && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnUpdateRefiRange(QAction *action)
{
    action->setEnabled(AtomStack->size() > 1 && MIFitGeomRefiner()->IsRefining() != true && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnUpdateRefiMolecule(QAction *action)
{
    action->setEnabled(!AtomStack->empty() && MIFitGeomRefiner()->IsRefining() != true && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnRefiAccept()
{
    acceptRefine();
}

void MIGLWidget::acceptRefine()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    MIFitGeomRefiner()->Accept();
    batonatom = NULL;
    std::vector<MAP_POINT>().swap(PList); // was PList.clear();

    // undo the lowered baton weight
    if (MIFitGeomRefiner()->GetMapWeightI() == 1)
    {
        MIFitGeomRefiner()->SetMapWeightI(10);
    }
    _modified = true;
    Modify(true);
    ReDraw();
}

void MIGLWidget::OnRefiCancel()
{
    cancelRefine();
}

void MIGLWidget::cancelRefine()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    MIFitGeomRefiner()->Cancel();
    _modified = true;
    ReDraw();
}

void MIGLWidget::OnRefiReset()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    MIFitGeomRefiner()->Reset();
    _modified = true;
    ReDraw();
}

void MIGLWidget::OnUpdateRefiAccept(QAction *action)
{
    action->setEnabled(MIFitGeomRefiner()->IsRefining() == true && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnUpdateRefiCancel(QAction *action)
{
    action->setEnabled(MIFitGeomRefiner()->IsRefining() == true && MIBusyManager::instance()->Busy() == false);

}

void MIGLWidget::OnUpdateRefiReset(QAction *action)
{
    action->setEnabled(MIFitGeomRefiner()->IsRefining() == true && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnRefiReDo()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    MIFitGeomRefiner()->Redo();
    _modified = true;
    Modify(true);
    ReDraw();
}

void MIGLWidget::OnUpdateRefiRedo(QAction *action)
{
    action->setEnabled(MIFitGeomRefiner()->IsRefining() != true && MIFitGeomRefiner()->CanRedo()  && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::Purge(Molecule *model)
{
    savefits.Purge(model);

    Residue *res = model->residuesBegin();
    while (res != NULL)
    {
        if (res==focusres)
        {
            doSetFocusResidue(NULL);
            break;
        }
        res = res->next();
    }

    res = model->residuesBegin();
    while (res != NULL)
    {
        Purge(res);
        res = res->next();
    }


    res = model->symmResiduesBegin();
    while (res != NULL)
    {
        if (res==focusres)
        {
            doSetFocusResidue(NULL);
            break;
        }
        res = res->next();
    }

    res = model->symmResiduesBegin();
    while (res != NULL)
    {
        Purge(res);
        res = res->next();
    }


    if (IsFitting() && fitmol == model)
    {
        cancelFitting();
    }
    if (fitmol == model)
    {
        fitmol = NULL;
        fitres = NULL;
    }
    if (focusnode == model)
    {
        focusnode = NULL;
    }
    if (PentamerFrom == model || PentamerModel == model)
    {
        clearPentamer();
    }

    std::vector<SaveModel>::iterator saveModelIter;
    for (saveModelIter = SaveModels.begin(); saveModelIter != SaveModels.end(); ++saveModelIter)
    {
        if (saveModelIter->model == model)
        {
            saveModelIter->model = NULL;
        }
    }
}

void MIGLWidget::OnFitUndo()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    if (FitToken != 0)
    {
        FitToken--;
        if (savefits.Restore(FitToken) == 0 && FitToken > 1)
        {
            FitToken--;
            savefits.Restore(FitToken);
        }
        std::string t = ::format("FitToken is now %d", (int)FitToken);
        Logger::log(t.c_str());
        _modified = true;
        Modify(true);
        ReDraw();
    }
}

void MIGLWidget::OnFitRedo()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    if ((int)FitToken < savefits.NumberSets()-1)
    {
        FitToken++;
        if (savefits.Restore(FitToken) && (int)FitToken < savefits.NumberSets()-1)
        {
            FitToken++;
            savefits.Restore(FitToken);
        }
        std::string t = ::format("FitToken is now %d", (int)FitToken);
        Logger::log(t.c_str());
        _modified = true;
        Modify(true);
        ReDraw();
    }
}

void MIGLWidget::OnUpdateFitUndo(QAction *action)
{
    action->setEnabled(!IsFitting() && savefits.NumberSets() > 1 && FitToken > 0 && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnUpdateFitRedo(QAction *action)
{
    action->setEnabled(!IsFitting() && (int)FitToken < savefits.NumberSets()-1 && savefits.NumberSets() > 1 && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnRefiRigidBody()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    EMap *currentmap = GetDisplaylist()->GetCurrentMap();
    if (currentmap)
    {
        MIFitGeomRefiner()->RigidOptimize(CurrentAtoms, fitmol, currentmap);
        _modified = true;
        Modify(true);
        UpdateCurrent();
        ReDraw();
    }
    else
    {
        Logger::log("Sorry, no map loaded to refine against - action canceled");
        return;
    }
}

void MIGLWidget::OnUpdateRefiRigidBody(QAction *action)
{
    action->setEnabled(CurrentAtoms.size() > 0 && GetDisplaylist()->GetCurrentMap() != NULL && MIBusyManager::instance()->Busy() == false);
}

float MIGLWidget::densityForCurrentAtoms()
{
    float value = 0.0f;
    Displaylist *displaylist = GetDisplaylist();
    EMap *emap = displaylist->GetCurrentMap();
    if (CurrentAtoms.size() > 0 && emap != NULL)
    {
        value = emap->RDensity(CurrentAtoms);
    }
    return value;
}

void MIGLWidget::UpdateCurrent()
{

    Displaylist *displaylist = GetDisplaylist();

#ifndef MIFIT_NO_RAMAPLOT
    if (RamaPlotMgr::instance()->IsShown())
        RamaPlotMgr::instance()->Update(fitres, SelectType);
#endif

    if (CurrentAtoms.size() == 0)
    {
        return;
    }
    EMap *emap = displaylist->GetCurrentMap();
    if (emap)
    {
        float r = emap->RDensity(CurrentAtoms);
        QString message = QString("Current Gdensity = %1")
                .arg(r, 0, 'f', 4);
        setMessage(message);
    }
    GeomRefiner *refi = MIFitGeomRefiner();
    if (refi->IsRefining() && refi->GetRefineWhileFit())
    {
        refi->Refine();
    }
    displaylist->UpdateContacts();
    static const unsigned int surfaceWhileFittingAtomLimit = 50;
    if (SurfaceCurrent != NO_SURFACE && CurrentAtoms.size() > surfaceWhileFittingAtomLimit)
    {
        Logger::log("Surfacing while fitting unavailable for more than %u atoms (currently %u atoms)",
                    surfaceWhileFittingAtomLimit, (unsigned int)CurrentAtoms.size());
        SurfaceCurrent = NO_SURFACE;
    }
    if (SurfaceCurrent == VDW_SURFACE || SurfaceCurrent == EXTENDED_SURFACE || SurfaceCurrent == PROBE_SURFACE)
    {
        float radius_mult = 1.0F;
        if (SurfaceCurrent == EXTENDED_SURFACE)
        {
            radius_mult = 2.0;
        }
        displaylist->SurfaceCurrent(CurrentAtoms, radius_mult);
        if (SurfaceCurrent == PROBE_SURFACE)
        {
            displaylist->ProbeSurface(fitmol->residuesBegin(), CurrentAtoms);
        }
    }
    else if (SurfaceCurrent == NO_SURFACE)
    {
        displaylist->ClearCurrentSurface();
    }
    if (IsTorsioning())
    {
        std::vector<PLINE>().swap(TorsionArrow); // was TorsionArrow.clear();
        MIAtom *a1 = fitmol->GetTorsionAtom1();
        MIAtom *a2 = fitmol->GetTorsionAtom2();
        if (a1 != NULL && a2 != NULL)
        {
            arrow2(TorsionArrow, a2->x() - a1->x(), a2->y() - a1->y(), a2->z() - a1->z(),
                   a1->x(), a1->y(), a1->z(), Colors::WHITE);
        }
    }
}

void MIGLWidget::InsertMRK(Residue *where, Molecule *model, bool before)
{
    Residue *m_res = GetDictRes("MRK");
    EMap *emap = GetDisplaylist()->GetCurrentMap();
    float ideal_dist = 3.8F;
    // values for before:
    // 0 - After atres
    // 1 - before atres
    if (m_res == NULL)
    {
        Logger::message("Error - nothing picked or residue not found in dictionary");
        return;
    }
    Residue *res = model->residuesBegin();
    if (res == NULL)
    {
        Logger::message("No model found - Use New Model command to start a model");
        return;
    }
    MIAtom *CAnext = NULL, *CAprev = NULL, *CA1 = NULL;
    MIAtom temp;
    CA1 = atom_from_name("CA", *where);
    if (!CA1)
    {
        return;
    }
    if (Monomer::isValid(where->next()))
    {
        CAnext = atom_from_name("CA", *where->next());
    }
    for (res = model->residuesBegin(); res != NULL; res = res->next())
    {
        if (res->next() == where)
        {
            CAprev = atom_from_name("CA", *res);
            break;
        }
    }
    temp.copyPosition(*CA1);
    temp.translate(0.0f, ideal_dist, 0.0f);
    if (before)
    {
        /* swap prev and next - tricky! */
        MIAtom *t = CAprev;
        CAprev = CAnext;
        CAnext = t;
    }
    if (CAprev == NULL)
    {
        CAprev = &temp;
    }

    std::string newname;
    if (!before)
    {
        newname = ftoa((int)(atof(where->name().c_str())+1));
    }
    else
    {
        newname = ftoa((int)(atof(where->name().c_str())-1));
    }

    // Checkpoint the model
    std::string s = ::format("Before inserting MRK %s", newname.c_str());
    SaveModelFile(model, s.c_str());

    Residue *newres = model->InsertRes(where, m_res, before);
    newres->setName(newname);
    newres->atom(0)->setPosition(CA1->x(), CA1->y()+ideal_dist, CA1->z());



    if (emap)
    {
        std::vector<MAP_POINT> &list = emap->PlaceCA(CAprev, CA1);
        std::vector<MAP_POINT>().swap(PList); // was PList.clear();

        for (size_t i = 0; i < list.size(); i++)
        {
            PList.push_back(list[i]);
        }

        // put the new atom at the top position in the map
        if (PList.size() > 0)
        {
            newres->atom(0)->setPosition(PList[0].x, PList[0].y, PList[0].z);
            batonposition = 0;
        }
    }
    AutoFit = true;
    // center on where residue
    viewpoint->setCenter(where->atom(0)->position());
    _modified = true;

    // rebuild the model bonds
    int nlinks = model->getnlinks();
    model->Build();
    if (nlinks > 0)
    {
        model->BuildLinks();
    }
    // the focus is now the new residue
    setFocusResidue(newres, true);
    focusnode = model;
    //int savecolor = newres->atom(0)->color;
    //set up baton mode
    GeomRefiner *geomrefi = MIFitGeomRefiner();
    geomrefi->Accept();
    geomrefi->SetRefiRes(where, newres, model, emap);
    geomrefi->dict.RemoveConstraints();
    geomrefi->dict.AddConstraint(CA1, "0.01");
    //geomrefi->Refine();
    geomrefi->SetRefineWhileFit(true);
    if (geomrefi->GetMapWeightI() != 0)
    {
        geomrefi->SetMapWeightI(1);
    }

    AtomStack->Push(newres->atom(0), newres, model);
    batonatom = newres->atom(0);
    OnFitSingleatom();

    AtomStack->Push(newres->atom(0), newres, model);
    AutoFit = false;
    // Mark the model as dirty
    Modify(true);
    model->SetCoordsChanged();
    // the following makes it easier for the user to save the model
    GetDisplaylist()->SetCurrent(model);
    PaletteChanged = true;
    ReDrawAll();
}

void MIGLWidget::OnNextBatonPosition()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    if (!batonatom || PList.size() <= 0)
    {
        return;
    }
    batonposition++;
    batonposition %= PList.size();
    batonatom->setPosition(PList[batonposition].x, PList[batonposition].y, PList[batonposition].z);
    _modified = true;
    ReDraw();
}

void MIGLWidget::refineConformer()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    EMap *currentmap = GetDisplaylist()->GetCurrentMap();
    if (!currentmap)
    {
        Logger::log("Sorry, no map loaded to refine against - action canceled");
        return;
    }
    vector<TORSION> torsions;
    GeomRefiner *geomrefi = MIFitGeomRefiner();
    TORSION *chi1, *chi2, *chi3, *chi4, *chi5;
    chi1 = geomrefi->dict.getTORSION(fitres, "CHI1", NULL);
    if (chi1)
    {
        torsions.push_back(*chi1);
    }
    chi2 = geomrefi->dict.getTORSION(fitres, "CHI2", NULL);
    if (chi2)
    {
        torsions.push_back(*chi2);
    }
    chi3 = geomrefi->dict.getTORSION(fitres, "CHI3", NULL);
    if (chi3)
    {
        torsions.push_back(*chi3);
    }
    chi4 = geomrefi->dict.getTORSION(fitres, "CHI4", NULL);
    if (chi4)
    {
        torsions.push_back(*chi4);
    }
    chi5 = geomrefi->dict.getTORSION(fitres, "CHI5", NULL);
    if (chi5)
    {
        torsions.push_back(*chi5);
    }

    geomrefi->TorsionOptimize(CurrentAtoms, fitmol, currentmap, torsions);
    _modified = true;
    Modify(true);
    ReDraw();

    if (chi1)
    {
        free(chi1);
    }
    if (chi2)
    {
        free(chi2);
    }
    if (chi3)
    {
        free(chi3);
    }
    if (chi4)
    {
        free(chi4);
    }
    if (chi5)
    {
        free(chi5);
    }
}

void MIGLWidget::generateSymmAtoms()
{
    Molecule *node = GetDisplaylist()->CurrentItem();
    if (!node)
        return;
    node->GetMapHeader().SetSymmOps();
    if (node->symmResiduesBegin())
    {
        clearSymmAtoms();
    }
    node->GenSymmAtoms(viewpoint);
    if (link_symm)
    {
        node->SymmLink();
    }
    ReDraw();
}

void MIGLWidget::clearSymmAtoms()
{
    Molecule *node = GetDisplaylist()->CurrentItem();
    if (!node)
        return;
    node->ClearSymmList();
    ReDraw();
}

void MIGLWidget::OnFitReplaceAndFit()
{
    replaceAndFitResidueWithPrompt();
}

void MIGLWidget::replaceAndFitResidueWithPrompt()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    Molecule *model;
    MIAtom *a;
    Residue *res;
    AtomStack->Peek(a, res, model);
    MI_ASSERT(a != NULL || res != NULL || model != NULL);
    if (a == NULL || res == NULL || model == NULL)
    {
        return;
    }
    std::string value = res->type();
    if (!promptForReplaceResidue(value))
    {
        return;
    }
    replaceAndFitResidue(value.c_str());
}

void MIGLWidget::replaceAndFitResidue(const char *residueType)
{
    Molecule *model;
    MIAtom *a;
    Residue *res;
    AtomStack->Peek(a, res, model);
    MI_ASSERT(a != NULL || res != NULL || model != NULL);
    if (a == NULL || res == NULL || model == NULL)
    {
        return;
    }
    replaceResidue(residueType);
    AtomStack->Push(atom_default(res), res, model);
    fitResidue();
    refineConformer();
}

void MIGLWidget::OnUpdateFitReplaceAndFit(QAction *action)
{
    action->setEnabled(IsFitting() == false && !AtomStack->empty() && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::saveSymmAtoms()
{
    Molecule *model;
    MIAtom *a;
    Residue *res;
    AtomStack->Pop(a, res, model);
    if (!MIAtom::isValid(a) || (a->type() & AtomType::SYMMATOM) == 0)
    {
        Logger::message("The atom on stack must be a symmetry atom.");
        return;
    }
    const std::string &s = MIFileSelector("Choose a name for the symmetry model file", "", "", "pdb",
                                          "PDB files (*.pdb)|*.pdb|All files (*.*)|*.*", MI_SAVE_MODE);
    if (s.size())
    {
        FILE *fp = fopen(s.c_str(), "w");
        model->SaveSymmMolecule(a, fp);
        fclose(fp);
    }
}

void MIGLWidget::OnSetRibbonColors()
{
    Displaylist *Models = GetDisplaylist();
    Molecule *m_node = Models->CurrentItem();
    if (!m_node)
        return;

    GenericDataDialog dlg(this);
    dlg.setWindowTitle("Set Ribbon Colors");
    dlg.addColorIndexField("Helix:", Colors::sec_colors[Colors::HELIX]);
    dlg.addColorIndexField("Sheet:", Colors::sec_colors[Colors::SHEET]);
    dlg.addColorIndexField("Coil:", Colors::sec_colors[Colors::COIL]);
    dlg.addBoolField("Use CA atom color:", m_node && (m_node->ribbon_coloring != 0));
    dlg.addBoolField("Save:", false);

    if (dlg.exec() != QDialog::Accepted)
    {
        return;
    }

    Colors::sec_colors[Colors::HELIX] = dlg.value(0).toInt();
    Colors::sec_colors[Colors::SHEET] = dlg.value(1).toInt();
    Colors::sec_colors[Colors::COIL] = dlg.value(2).toInt();
    if (m_node)
    {
        m_node->ribbon_coloring = dlg.value(3).toBool();
    }
    if (dlg.value(4).toBool())
    {
        Application::instance()->Write();
    }
    std::vector<Bond>::iterator iter = m_node->getBonds().begin();
    while (iter != m_node->getBonds().end())
    {
        Bond &bond = *iter;
        ++iter;
        if (bond.type == B_RIBBON)
        {
            short color = secstrcolor(bond.getAtom1()->occ());
            bond.getAtom1()->setColor(color);
            bond.getAtom2()->setColor(color);
        }
    }
    doRefresh();
}

void MIGLWidget::OnFindPentamer()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    Molecule *model;
    MIAtom *a;
    Residue *res;
    AtomStack->Pop(a, res, model);

    std::string pentdir = Application::instance()->PentDir.c_str();
    Residue *pentamer = model->MatchPentamer(pentdir, res);
    if (!pentamer)
    {
        Logger::message("Failed to find pentamer");
        return;
    }

    Molecule *model1 = new Molecule(pentamer, "Pentamer match", NULL, NULL, 0, MoleculeType::Pentamer);

    if (model1)
    {
        clearPentamer();
        PentamerModel = model1;
        PentamerFrom = model;
        PentamerStart = res;
        PaletteChanged = true;
        _modified = true;
        ReDrawAll();
    }
}

void MIGLWidget::OnUpdateFindPentamer(QAction *action)
{
    action->setEnabled(!AtomStack->empty() && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnClearPentamer()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    clearPentamer();
}

void MIGLWidget::clearPentamer()
{
    Residue *oldPentamerStart = PentamerStart;
    Molecule *oldPentamerModel = PentamerModel;
    // Clear pentamer model before delete to prevent loop in signaling
    PentamerModel = NULL;
    PentamerFrom = NULL;
    PentamerStart = NULL;
    if (oldPentamerStart != NULL)
    {
        Purge(oldPentamerStart);
    }
    if (oldPentamerModel != NULL)
    {
        Purge(oldPentamerModel);
        delete oldPentamerModel;
    }
    PaletteChanged = true;
    _modified = true;
    ReDrawAll();
}

void MIGLWidget::OnUpdateClearPentamer(QAction *action)
{
    action->setEnabled(PentamerModel != NULL && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnReplaceMiddle3()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    try
    {
        if (MIMoleculeBase::isValid(PentamerModel) && PentamerModel->residuesBegin())
        {
            FitToken = savefits.Save(PentamerStart->next(), 3, PentamerFrom);
            PentamerFrom->ReplaceMainChain(PentamerStart->next(), PentamerModel->residuesBegin()->next(), 3);
            PentamerFrom->SetCoordsChanged(true);
            Modify(true);
            clearPentamer();
        }
        else
        {
            Logger::message("Error replacing mainchain atoms");
        }
    }
    catch (...)
    {
        Logger::message("Error replacing mainchain atoms");
    }
}

void MIGLWidget::OnUpdateReplaceMiddle3(QAction *action)
{
    action->setEnabled(PentamerModel != NULL && PentamerFrom != NULL && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnReplaceFirst4()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    try
    {
        if (MIMoleculeBase::isValid(PentamerModel) && Monomer::isValid(PentamerModel->residuesBegin()))
        {
            FitToken = savefits.Save(PentamerStart, 4, PentamerFrom);
            PentamerFrom->ReplaceMainChain(PentamerStart, PentamerModel->residuesBegin(), 4);
            Modify(true);
            PentamerFrom->SetModified();
            clearPentamer();
        }
        else
        {
            Logger::message("Error replacing mainchain atoms");
        }
    }
    catch (...)
    {
        Logger::message("Error replacing mainchain atoms");
    }
}

void MIGLWidget::OnUpdateReplaceFirst4(QAction *action)
{
    action->setEnabled(PentamerModel != NULL && PentamerFrom != NULL && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnReplaceLast4()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    try
    {
        if (MIMoleculeBase::isValid(PentamerModel) && Monomer::isValid(PentamerModel->residuesBegin()))
        {
            FitToken = savefits.Save(PentamerStart->next(), 4, PentamerFrom);
            PentamerFrom->ReplaceMainChain(PentamerStart->next(), PentamerModel->residuesBegin()->next(), 4);
            PentamerFrom->SetModified();
            Modify(true);
            clearPentamer();
        }
        else
        {
            Logger::message("Error replacing mainchain atoms");
        }
    }
    catch (...)
    {
        Logger::message("Error replacing mainchain atoms");
    }
}

void MIGLWidget::OnUpdateReplaceLast4(QAction *action)
{
    action->setEnabled(PentamerModel != NULL && PentamerFrom != NULL && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnReplaceAll()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    try
    {
        if (MIMoleculeBase::isValid(PentamerModel) && Monomer::isValid(PentamerModel->residuesBegin()))
        {
            FitToken = savefits.Save(PentamerStart, 5, PentamerFrom);
            PentamerFrom->ReplaceMainChain(PentamerStart, PentamerModel->residuesBegin(), 5);
            PentamerFrom->SetModified();
            Modify(true);
            clearPentamer();
        }
        else
        {
            Logger::message("Error replacing mainchain atoms");
        }
    }
    catch (...)
    {
        Logger::message("Error replacing mainchain atoms");
    }
}

void MIGLWidget::OnUpdateReplaceAll(QAction *action)
{
    action->setEnabled(PentamerModel != NULL && PentamerFrom != NULL && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnMarkAlpha()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    MIAtom *a1, *a2;
    Residue *res1, *res2;
    Molecule *m1, *m2;
    AtomStack->Pop(a1, res1, m1);
    AtomStack->Pop(a2, res2, m2);
    if (m1 != m2)
    {
        Logger::message("Both residues must be in the same model");
        return;
    }
    m1->SetSecStr(res1, res2, 'H');
    ReDrawAll();
}

void MIGLWidget::OnMarkBeta()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    MIAtom *a1, *a2;
    Residue *res1, *res2;
    Molecule *m1, *m2;
    AtomStack->Pop(a1, res1, m1);
    AtomStack->Pop(a2, res2, m2);
    if (m1 != m2)
    {
        Logger::message("Both residues must be in the same model");
        return;
    }
    m1->SetSecStr(res1, res2, 'S');
    ReDrawAll();
}

void MIGLWidget::OnUpdateMarkAlpha(QAction *action)
{
    action->setEnabled(AtomStack->size() > 1 && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnUpdateMarkBeta(QAction *action)
{
    action->setEnabled(AtomStack->size() > 1 && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnFlipPeptide()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    MIAtom *a;
    Residue *res;
    Molecule *m;
    AtomStack->Pop(a, res, m);
    MIAtom *N = atom_from_name("N", *res);
    MIAtom *HN = atom_from_name("HN", *res);
    MIAtom *C = atom_from_name("C", *res);
    MIAtom *O = atom_from_name("O", *res);
    if (a != N && a != C && a != HN && a != O)
    {
        Logger::message("Can't find peptide plane you want to flip\nMake sure you have picked an atom in a peptide plane\n - i.e C, N, O, or HN");
    }
    Residue *res1, *res2;
    if (a == N || a == HN)
    {
        res2 = res;
        res1 = res->prev();
    }
    else
    {
        res1 = res;
        res2 = res->next();
    }
    if (!res1 || !res2)
    {
        return;
    }
    FitToken = savefits.Save(res1, 2, m);
    /*  initrotate sets up a matrix for the a 3-d rotation about
     *  the point x,y,z along vector v about angle alpha
     *  call rotate to apply rotation alpha in degrees
     *           initrotate(x,y,z,v1,v2,v3,alpha,matrix[4][3])
     *  apply with
     *       rotate(x,y,z,&xp,&yp,&zp,mat[4][3])
     */
    MIAtom *CA1 = atom_from_name("CA", *res1);
    MIAtom *CA2 = atom_from_name("CA", *res2);
    N = atom_from_name("N", *res2);
    HN = atom_from_name("HN", *res2);
    C = atom_from_name("C", *res1);
    O = atom_from_name("O", *res1);
    if (!CA1 || !CA2)
    {
        return;
    }
    float mat[4][3];
    initrotate(CA1->x(), CA1->y(), CA1->z(), CA2->x()-CA1->x(), CA2->y()-CA1->y(), CA2->z()-CA1->z(), 180.0, mat);
    float pos[3];
    if (N)
    {
        N->getPosition(pos);
        ::rotate(&pos[0], &pos[1], &pos[2], mat);
        N->setPosition(pos[0], pos[1], pos[2]);
    }
    if (HN)
    {
        HN->getPosition(pos);
        ::rotate(&pos[0], &pos[1], &pos[2], mat);
        HN->setPosition(pos[0], pos[1], pos[2]);
    }
    if (C)
    {
        C->getPosition(pos);
        ::rotate(&pos[0], &pos[1], &pos[2], mat);
        C->setPosition(pos[0], pos[1], pos[2]);
    }
    if (O)
    {
        O->getPosition(pos);
        ::rotate(&pos[0], &pos[1], &pos[2], mat);
        O->setPosition(pos[0], pos[1], pos[2]);
    }
    m->SetCoordsChanged();
    Modify(true);
    _modified = true;
    ReDraw();
}

void MIGLWidget::OnUpdateFlipPeptide(QAction *action)
{
    action->setEnabled(!AtomStack->empty() && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnHideModel()
{
    Displaylist *Models = GetDisplaylist();
    Molecule *node = Models->CurrentItem();
    if (node)
    {
        node->Do();
        node->HideAll();
        ReDraw();
    }
}

void MIGLWidget::OnFitSurfaceNone()
{
    SurfaceCurrent = NO_SURFACE;
    UpdateCurrent();
    ReDraw();
}

void MIGLWidget::OnFitSurfaceVdw()
{
    SurfaceCurrent = VDW_SURFACE;
    UpdateCurrent();
    ReDraw();
}

void MIGLWidget::OnFitSurfaceProbe()
{
    SurfaceCurrent = PROBE_SURFACE;
    UpdateCurrent();
    ReDraw();
}

void MIGLWidget::OnFitSurfaceExtended()
{
    SurfaceCurrent = EXTENDED_SURFACE;
    UpdateCurrent();
    ReDraw();
}

void MIGLWidget::OnUpdateFitSurfaceNone(QAction *action)
{
    action->setChecked(SurfaceCurrent == NO_SURFACE);
}

void MIGLWidget::OnUpdateFitSurfaceVdw(QAction *action)
{
    action->setChecked(SurfaceCurrent == VDW_SURFACE);
}

void MIGLWidget::OnUpdateFitSurfaceExtended(QAction *action)
{
    action->setChecked(SurfaceCurrent == EXTENDED_SURFACE);
}

void MIGLWidget::OnUpdateFitSurfaceProbe(QAction *action)
{
    action->setChecked(SurfaceCurrent == PROBE_SURFACE);
}

void MIGLWidget::OnRamachandranPlotShowAllowed()
{
    RamaPlotMgr::instance()->SetShowAllowed(!RamaPlotMgr::instance()->GetShowAllowed());
}

void MIGLWidget::OnUpdateRamachandranPlotShowAllowed(QAction *action)
{
    action->setChecked(RamaPlotMgr::instance()->GetShowAllowed());
}


void MIGLWidget::updateRamachandranPlot()
{
    RamaPlotMgr::instance()->SetView(this);
    Displaylist *models = GetDisplaylist();
    Molecule *model = models->CurrentItem();
    if (!model)
    {
        RamaPlotMgr::instance()->Update(0, 0, "");
        return;
    }
    std::string modelName;
    if (MIMoleculeBase::isValid(model))
    {
        modelName = model->compound.c_str();
    }
    RamaPlotMgr::instance()->Update(model, (model && model->Contains(focusres) ? focusres : 0), modelName);
}



void MIGLWidget::OnPolyAlaChain()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    polyAlaChain();
}

void MIGLWidget::polyAlaChain()
{
    Residue *res1, *res2, *res;
    MIAtom *a;
    Molecule *model;
    AtomStack->Pop(a, res, model);
    if (Monomer::isValid(res) && MIMoleculeBase::isValid(model))
    {
        getchain(res->chain_id(), model->residuesBegin(), res1, res2);
        if (res1 && res2)
        {
            AtomStack->Push(res2->atom(0), res2, model);
            AtomStack->Push(res1->atom(0), res1, model);
            polyAla();
        }
    }
}

void MIGLWidget::OnPolyAla()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    polyAla();
}

void MIGLWidget::polyAla()
{
    Residue *res1, *res2;
    MIAtom *a1, *a2;
    Molecule *model, *model2;
    AtomStack->Pop(a1, res1, model);
    AtomStack->Pop(a2, res2, model2);
    if (Monomer::isValid(res1) && Monomer::isValid(res2)
        && MIMoleculeBase::isValid(model) && MIMoleculeBase::isValid(model2))
    {
        AtomStack->Push(a1, res1, model);
        AtomStack->Push(a2, res2, model2);
        OnBuildCBRange();
        AtomStack->Push(a1, res1, model);
        AtomStack->Push(a2, res2, model2);
        OnBuildMainchainRange();
    }
}

void MIGLWidget::OnUpdatePolyAla(QAction *action)
{
    action->setEnabled(AtomStack->size() > 1 && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnUpdatePolyAlaChain(QAction *action)
{
    action->setEnabled(!AtomStack->empty() && MIBusyManager::instance()->Busy() == false);
}

static long save_id = 1;

bool MIGLWidget::SaveModelFile(Molecule *model, const char *description)
{
    std::string pathname;
    SaveModel s;
    std::string directory = Application::instance()->checkpointDirectory.c_str();
    if (directory.empty())
    {
        directory = QDir::current().path().toStdString();
    }
    pathname = ::format("%s/MIFit%ld_save%ld.pdb", directory.c_str(), Application::instance()->GetPid(), save_id);
    s.path = pathname;
    s.model = model;
    {
        time_t t = ::time(0);
        s.time = ctime(&t);
    }
    s.description = description;
    if (!model->SavePDBFile(pathname.c_str()))
    {
        return false;
    }
    SaveModels.push_back(s);
    std::string mess = ::format("Checkpointed model in %ld: %s - %s", save_id, description, pathname.c_str());
    Logger::log(mess);
    save_id++;
    return true;
}

void MIGLWidget::OnSequenceSaveModel()
{
    Displaylist *Models = GetDisplaylist();
    if (!Models->CurrentItem())
        return;
    const std::string &s = MIFileSelector("Select .seq File", "", "", "seq",
                                          "Sequence files (*.seq)|*.seq| All files (*.*)|*.*", MI_SAVE_MODE);
    if (s.size())
    {
        Models->CurrentItem()->WriteSequence(s.c_str(), SEQ_FORMAT_MODEL);
    }
}

void MIGLWidget::OnUpdateSequenceSaveModel(QAction *action)
{
    Displaylist *Models = GetDisplaylist();
    action->setEnabled(Models->CurrentItem() != NULL  && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnSequencePosition()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    Residue *start = NULL, *res1, *res2, *res, *end = NULL;
    MIAtom *a;
    Molecule *model, *model2;
    AtomStack->Pop(a, res1, model);
    AtomStack->Pop(a, res2, model2);
    if (model != model2)
    {
        Logger::message("Both ends must be in the same model");
        return;
    }
    std::string seq = model->SeqString();
    if (seq.length() == 0)
    {
        Logger::message("No sequence loaded - use Sequence menu to enter or read sequence");
        return;
    }
    size_t nres;
    // find the start and count
    start = res1;
    end = res2;
    nres = order_ends(start, end, model->residuesBegin());
    if (!start)
    {
        Logger::message("Can't find the start");
        return;
    }

    QStringList listItems;
    for (size_t i = 0; i < seq.length(); i++)
    {
        QString s;
        s.sprintf("%5ld %c", static_cast<long>(i+1), seq[i]);
        listItems += s;
    }

    QString selected = QInputDialog::getItem(this, "MIFit", "Set Sequence Position", listItems);
    int n = listItems.indexOf(selected);
    if (n > 0)
    {
        for (res = start; res != NULL; res = res->next())
        {
            res->setSeqpos(res->seqpos()+n);
            if (res == end)
            {
                break;
            }
        }
    }
    PaletteChanged = true;
    model->SetModified();
    Modify(true);
    ReDrawAll();
}

void MIGLWidget::OnSequencePositionChain()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    Residue *res1, *res2, *res;
    MIAtom *a;
    Molecule *model;
    AtomStack->Pop(a, res, model);
    getchain(res->chain_id(), model->residuesBegin(), res1, res2);
    if (res1 && res2)
    {
        AtomStack->Push(res2->atom(0), res2, model);
        AtomStack->Push(res1->atom(0), res1, model);
        OnSequencePosition();
    }
}

void MIGLWidget::OnUpdateSequencePosition(QAction *action)
{
    action->setEnabled(AtomStack->size() > 1 && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnUpdateSequencePositionChain(QAction *action)
{
    action->setEnabled(!AtomStack->empty() && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnReplaceSequence()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    modelReplaceWithSequence();
}

void MIGLWidget::modelReplaceWithSequence()
{
    Residue *start = NULL, *res1, *res2, *res, *end = NULL, *dictres = NULL;
    MIAtom *a;
    Molecule *model, *model2;
    AtomStack->Pop(a, res1, model);
    AtomStack->Pop(a, res2, model2);
    if (model != model2)
    {
        Logger::message("Both ends must be in the same model");
        return;
    }
    std::string seq = model->SeqString();
    if (seq.length() <= 0)
    {
        Logger::message("No sequence loaded - use Sequence menu to enter or read sequence");
        return;
    }
    if (!Monomer::isValid(res1) || !Monomer::isValid(res2))
    {
        Logger::message("ReplaceSequence: Error: bad residue pointers, cannot continue.");
        return;
    }
    size_t nres;
    // find the start and count
    start = res1;
    end = res2;
    nres = order_ends(start, end, model->residuesBegin());
    if (!start)
    {
        Logger::message("Can't find the start");
        return;
    }
    std::string s = ::format("About to replace the residues between %s and %s\n"
                             "with the lower sequence.  You can undo with Model/Revert Model.\nOK?",
                             resid(start).c_str(), resid(end).c_str());
    bool answer = QMessageBox::question(this, "Replace with Sequence", s.c_str(), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes;
    if (answer)
    {
        return;
    }
    // save model
    if (!SaveModelFile(model, "before change to sequence"))
    {
        Logger::message("Warning: Unable to save model - will not be able to Undo");
    }
    AutoFit = true;
    char seq_one;
    for (res = start; res != NULL; res = res->next())
    {
        if (strcmp(res->type().c_str(), "MRK") == 0 || strcmp(res->type().c_str(), "VEC") == 0 || res->name1() == '?')
        {
            Logger::message("Error: Poly-ala chain first or residue not amino acid");
            return;
        }
        seq_one = seq[res->seqpos()];
        dictres = GetDictRes(seq_one, 0);
        if (dictres)
        {
            MoveOnto(dictres, res);
            model->ReplaceRes(res, dictres);
            model->Build();
            a = atom_default(res);
            if (a)
            {
                AtomStack->Push(a, res, model);
                fitResidue();
                refineConformer();
                ApplyFit();
                acceptRefine();
            }
        }
        if (res == end)
        {
            break;
        }
    }
    PaletteChanged = true;
    _modified = true;
    model->Build();
    model->SetModified();
    Modify(true);
    AutoFit = false;
    ReDrawAll();
}

void MIGLWidget::OnUpdateReplaceSequence(QAction *action)
{
    action->setEnabled(AtomStack->size() > 1 && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnRevertModel()
{
    if (SaveModels.size() <= 0)
    {
        return;
    }
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    revertToSavedModelWithPrompt();
}

void MIGLWidget::revertToSavedModelWithPrompt()
{
    QStringList choices;
    for (unsigned int i = 0; i < SaveModels.size(); i++)
    {
        choices += QString("%1: %2 %3").arg(SaveModels[i].time.c_str(), SaveModels[i].description.c_str(), SaveModels[i].path.c_str());
    }
    QString selected = QInputDialog::getItem(this, "Revert Model", "Choose file to revert with", choices);
    int choice = choices.indexOf(selected);
    revertToSavedModel(choice);
}

void MIGLWidget::revertToSavedModel(int choice)
{
    if (choice >= 0 && choice < (int)SaveModels.size())
    {
        if (MIMoleculeBase::isValid(SaveModels[choice].model))
        {
            SaveModelFile(SaveModels[choice].model, "before revert model (to undo revert model)");
            SaveModels[choice].model->Revert(SaveModels[choice].path.c_str());
        }
        else
        {
            if (LoadPDBFile(SaveModels[choice].path.c_str()))
            {
                SaveModels[choice].model = GetDisplaylist()->CurrentItem();
                SaveModels[choice].model->compound = "Reverted model";
            }
        }
        if (MIMoleculeBase::isValid(SaveModels[choice].model))
        {
            PaletteChanged = true;
            _modified = true;
            SaveModels[choice].model->Build();
            SaveModels[choice].model->SetModified();
            SaveModels[choice].model->SetCoordsChanged(false);
            Modify(true);
        }
        ReDrawAll();
    }
}

void MIGLWidget::OnUpdateRevertModel(QAction *action)
{
    action->setEnabled(SaveModels.size() > 0 && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnCheckpointModel()
{
    Molecule *model = GetDisplaylist()->CurrentItem();
    if (!model)
        return;

    QString newname = QInputDialog::getText(this, "Checkpoint Description", "Enter a description of checkpoint:", QLineEdit::Normal, "User checkpoint");
    if (newname.isEmpty())
    {
        Logger::log("Action canceled.");
        return;
    }

    if (!SaveModelFile(model, newname.toAscii().constData()))
    {
        Logger::message("Warning: Unable to save model - will not be able to Undo");
    }
}

void MIGLWidget::OnUpdateCheckpointModel(QAction *action)
{
    action->setEnabled(GetDisplaylist() != NULL && GetDisplaylist()->CurrentItem() != NULL  && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnAutoCheckpointModel()
{
    AutoSave = !AutoSave;
}

void MIGLWidget::OnUpdateAutoCheckpointModel(QAction *action)
{
    action->setChecked(AutoSave);
}

void MIGLWidget::OnBuildCBRange()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    Residue *start = NULL, *res1, *res2, *res, *end = NULL;
    MIAtom *a;
    Molecule *model, *model2;
    AtomStack->Pop(a, res1, model);
    AtomStack->Pop(a, res2, model2);
    if (model != model2)
    {
        Logger::message("Both ends must be in the same model");
        return;
    }
    // find the start and count
    start = res1;
    end = res2;
    order_ends(start, end, model->residuesBegin());
    if (end->next() != NULL)
    {
        Residue *r = end->next();
        if (atom_from_name("CA", *r))
        {
            end = r;
        }
    }
    if (!start)
    {
        Logger::message("Can't find the start");
        return;
    }
    // save model
    if (!SaveModelFile(model, "before adding CB's"))
    {
        Logger::message("Warning: Unable to save model - will not be able to Undo");
    }
    res = start;
    while ((res != NULL) && res != end)
    {
        model->BuildCB(res);
        res = res->next();
    }
    model->Build();
    MIAtom *a1 = atom_from_name("CA", *start);
    MIAtom *a2 = atom_from_name("CA", *end);
    if (a1 && a2)
    {
        AtomStack->Push(a2, end, model);
        AtomStack->Push(a1, start, model);
        OnRefiRange();
        AtomStack->Push(a2, end, model);
        AtomStack->Push(a1, start, model);
        OnRefiRange();
        OnRefiAccept();
    }
    PaletteChanged = true;
    _modified = true;
    model->SetCoordsChanged();
    Modify(true);
    ReDrawAll();
}

void MIGLWidget::OnBuildMainchainRange()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    Residue *start = NULL, *res1, *res2, *res, *end = NULL;
    MIAtom *a;
    EMap *currentmap = GetDisplaylist()->GetCurrentMap();
    Molecule *model, *model2;
    AtomStack->Pop(a, res1, model);
    AtomStack->Pop(a, res2, model2);
    if (model != model2)
    {
        Logger::message("Both ends must be in the same model");
        return;
    }
    // find the start and count
    start = res1;
    end = res2;
    order_ends(start, end, model->residuesBegin());
    if (!start)
    {
        Logger::message("Can't find the start");
        return;
    }
    // save model
    if (!SaveModelFile(model, "before building mainchain"))
    {
        Logger::message("Warning: Unable to save model - will not be able to Undo");
    }
    res = start;
    while (res != NULL)
    {
        MIFitGeomRefiner()->BuildMainchain(res, model, currentmap, res->next() != end);
        if (res == end)
        {
            break;
        }
        res = res->next();
    }
    PaletteChanged = true;
    _modified = true;
    model->Build();
    model->SetModified();
    Modify(true);
    ReDrawAll();
}

void MIGLWidget::OnGotoNter()
{
    if (!focusres)
    {
        return;
    }
    Residue *res1, *res2, *res;
    Displaylist *Models = GetDisplaylist();
    Molecule *model = Models->CurrentItem();
    if (!model)
        return;
    getchain(focusres->chain_id(), model->residuesBegin(), res1, res2);
    res = res1;
    setFocusResidue(res);
}

void MIGLWidget::OnGotoCter()
{
    if (!focusres)
    {
        return;
    }
    Residue *res1, *res2, *res;
    Displaylist *Models = GetDisplaylist();
    Molecule *model = Models->CurrentItem();
    if (!model)
        return;
    getchain(focusres->chain_id(), model->residuesBegin(), res1, res2);
    res = res2;
    setFocusResidue(res);
}

void MIGLWidget::OnUpdateGotoNter(QAction *action)
{
    action->setEnabled(focusres != NULL && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnUpdateGotoCter(QAction *action)
{
    action->setEnabled(focusres != NULL && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnToolTip()
{
    MouseStillTime = 0;
    static int oldx = -1;
    static int oldy = -1;
    if (oldx != mouse.x() || oldy != mouse.y())
    {
        std::string s;
        prepareAtomPicking();
        vector<GLuint> ids = mousePicker->pick(mouse.x(), mouse.y(), frustum, atomPickingRenderable);
        MIAtom *atom = NULL;
        if (ids.size() > 0)
        {
            atom = atomPickingRenderable->getAtom(ids[0]);
        }
        if (atom != NULL)
        {
            Residue *res = NULL;
            Molecule *mol = NULL;
            findResidueAndMoleculeForAtom(atom, res, mol);

            if (atom->altloc() == ' ')
            {
                s = ::format("%s %s (%0.3f,%0.3f,%0.3f) Occ=%0.2f B=%0.1f",
                             resid(res).c_str(), atom->name(), atom->x(), atom->y(), atom->z(), atom->occ(), atom->BValue());
            }
            else
            {
                s = ::format("%s %s_%c (%0.3f,%0.3f,%0.3f) Occ=%0.2f B=%0.1f",
                             resid(res).c_str(), atom->name(), atom->altloc(), atom->x(), atom->y(), atom->z(), atom->occ(), atom->BValue());
            }
            oldx = mouse.x();
            oldy = mouse.y();
        }
        MIMainWindowMiddleFooter(s.c_str());
    }
}

void MIGLWidget::OnAddWater()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    Displaylist *Models = GetDisplaylist();
    Molecule *model = Models->CurrentItem();
    EMap *emap = Models->GetCurrentMap();
    if (!emap)
    {
        Logger::log("Sorry there is no map to sprinkle into - action canceled");
        return;
    }
    if (!model)
    {
        Logger::log("Sorry there is no model to add water to - action canceled");
        return;
    }
    static float dmin = 2.30F;
    static float dmax = 7.0F;
    static int maxadd = 100;
    static int level = 50;
    static float xmin = 0.0, xmax = 1.0F;
    static float ymin = 0.0, ymax = 1.0F;
    static float zmin = 0.0, zmax = 1.0F;
    static int oldspgpno = (-1);

    if (oldspgpno != emap->GetMapHeader()->spgpno)
    {
        emap->GetMapHeader()->AsuBounds(xmin, xmax, ymin, ymax, zmin, zmax);
        oldspgpno = emap->GetMapHeader()->spgpno;
    }

    GenericDataDialog dlg(this);
    dlg.setWindowTitle("Add Waters to Map");
    dlg.addDoubleField("Minimum Distance:", dmin);
    dlg.addDoubleField("Maximum Distance From Protein:", dmax);
    dlg.addIntField("Minimum map level (1 sigma = 50):", level);
    dlg.addIntField("Maximum Number of Waters to Add:", maxadd);
    dlg.addDoubleField("ASU Xmin:", xmin);
    dlg.addDoubleField("ASU Xmax:", xmax);
    dlg.addDoubleField("ASU Ymin:", ymin);
    dlg.addDoubleField("ASU Ymax:", ymax);
    dlg.addDoubleField("ASU Zmin:", zmin);
    dlg.addDoubleField("ASU Zmax:", zmax);


    if (dlg.exec() == QDialog::Accepted)
    {
        // Checkpoint the model
        if (!SaveModelFile(model, "Before adding waters"))
        {
            Logger::message("Warning: Unable to save model - will not be able to Undo");
        }
        dmin = dlg.value(0).toDouble();
        dmax = dlg.value(1).toDouble();
        level = dlg.value(2).toInt();
        maxadd = dlg.value(3).toInt();
        xmin = dlg.value(4).toDouble();
        xmax = dlg.value(5).toDouble();
        ymin = dlg.value(6).toDouble();
        ymax = dlg.value(7).toDouble();
        zmin = dlg.value(8).toDouble();
        zmax = dlg.value(9).toDouble();

        Residue *last;
        Residue *first = model->residuesBegin();
        if (!Monomer::isValid(first))
        {
            Logger::message("Waters can't be added to an empty model.");
            return;
        }
        emap->HydrateMap(level, maxadd, 1, model, dmin, dmax, xmin, xmax, ymin, ymax, zmin, zmax);
        // get the added waters
        first = first->next();
        if (first)
        {
            last = first;
            int n = 1;
            while (last->next() != NULL)
            {
                last = last->next();
                n++;
            }
            std::string s = ::format("Added %d waters from %s to %s", n, resid(first).c_str(), resid(last).c_str());
            Logger::log(s);
        }
        PaletteChanged = true;
        _modified = true;
        model->Build();
        model->SetCoordsChanged();
        Modify(true);
        ReDrawAll();
    }
}

void MIGLWidget::OnUpdateAddWater(QAction *action)
{
    action->setEnabled(GetDisplaylist()->CurrentItem() != NULL && GetDisplaylist()->GetCurrentMap() != NULL && MIBusyManager::instance()->Busy() == false);
}

int MIGLWidget::ScriptCommand(const char *ubuf)
{
    /* view script command handler */
    /* other script handlers are found in relevant class - See CMolwDoc::OnOpenDocument() */

    /* if successful returns 1
     * if error returns -1
     * if command not found returns 0
     */
    float a;
    int m;
    //char file[2000];
    char chain[20];
    extern int factor();

    std::string s(ubuf);
    s = MIToLower(s);
    const char *buf = s.c_str();

#define IFC(s) if (!strncasecmp(buf, s, strlen(s)))
#define cmdtoggle(p) return cmdtogglef(buf, p, NULL);
#define cmdtoggle2(p) return cmdtogglef(buf, NULL, &p);
#define cmdtoggle3(p, special) \
    {i = cmdtogglef(buf, p, NULL); special; return i;}
#define cmdtoggle4(p, special) \
    {i = cmdtogglef(buf, NULL, &p); special; return i;}

#define cmdchoice(p) return (cmdchoicef(buf, p));

#define cmdslider(panel, factor, special) \
    {i = cmdsliderf(buf, panel, NULL, factor, &m); special; return i;}
    /* keep rotx,y,z first to make as fast as possible
       when benchmarking graphics with multiple calls to roty */
    IFC("rotx")
    {
        if (sscanf(buf, "%*s%f", &a) == 1)
        {
            viewpoint->rotate(a, 0, 0);
            ReDraw();
            return 1;
        }
        else
        {
            return -1;
        }
    }
    IFC("roty")
    {
        if (sscanf(buf, "%*s%f", &a) == 1)
        {
            viewpoint->rotate(0, a, 0);
            ReDraw();
            return 1;
        }
        else
        {
            return -1;
        }
    }
    IFC("rotz")
    {
        if (sscanf(buf, "%*s%f", &a) == 1)
        {
            viewpoint->rotate(0, 0, a);
            ReDraw();
            return 1;
        }
        else
        {
            return -1;
        }
    }
    IFC("#") return 1;
    //IFC("arrows") cmdtoggle(lsqpop->arrows);
    IFC("atomlabel")
    {
        char resid[MAXNAME], atomid[MAXNAME];
        Residue *res;
        MIAtom *atom;
        Displaylist *models = GetDisplaylist();
        if (sscanf(buf, "%*s%d%s%s%s", &m, resid, atomid, chain) == 4)
        {
            if (m < 1 || m > models->NumberItems())
            {
                return -1;
            }
            if ((res = residue_from_name((*models)[m-1]->residuesBegin(), resid, chain[0]))
                != NULL)
            {
                if ((atom = atom_from_name(atomid, *res)) != NULL)
                {
                    /* create label */
                    (*models)[m-1]->labelAtom(atom, res);
                    return 1;
                }
            }
        }
        else
        {
            return -1;
        }
    }
    //IFC("autocont") cmdtoggle3(conturpop->autocontur,
    //AutoContourNotify(0,0,0));
    //IFC("autofft") cmdtoggle(sfpop->autofft);
    //IFC("autonum") cmdtoggle(model1pop->autonum);
    //IFC("autosave") return 1;
    //IFC("autosymmrad") cmdtoggle3(controlpop->autosymmradius,
    //AutoSymmRadiusNotify(0,0,0));
    //IFC("averageover") cmdtoggle(conturpop->ncrset);
    //IFC("blinking") cmdtoggle(hidepop->blinking);
    //IFC("blinkrate") cmdslider(controlpop->blinkrate,1,set_blinkrate(m));
    IFC("bond ")
    {
        char resid[MAXNAME], atomid[MAXNAME];
        char resid2[MAXNAME], atomid2[MAXNAME];
        char chain1[20], chain2[20];
        Residue *res, *res2;
        MIAtom *atom, *atom2;
        extern void BondAtoms();
        int m2;
        Displaylist *models = GetDisplaylist();
        if (sscanf(buf, "%*s%d%s%s%s%d%s%s%s", &m, resid, atomid, chain1, &m2, resid2, atomid2, chain2))
        {
            if (m < 1 || m > models->NumberItems())
            {
                return -1;
            }
            if (m2 < 1 || m2 > models->NumberItems())
            {
                return -1;
            }
            if (m != m2)
            {
                return -1;
            }
            if ((res = residue_from_name((*models)[m-1]->residuesBegin(), resid, chain1[0])) != NULL
                && (res2 = residue_from_name((*models)[m2-1]->residuesBegin(), resid2, chain2[0])) != NULL)
            {
                if ((atom = atom_from_name(atomid, *res)) != NULL && (atom2 = atom_from_name(atomid2, *res2)) != NULL)
                {
                    /* create bond */
                    (*models)[m-1]->AddBond(atom, atom2);
                    return 1;
                }
            }
        }
        else
        {
            return -1;
        }
    }
    /*
       IFC("buildvu"){
       sscanf(buf,"%*s%s",key);
       if( strcasecmp(key,"file")==0 &&
          sscanf(buf,"%*s%*s%d%s",&m,file)==2){
        xv_set(filepop->vufile,PANEL_VALUE,file,NULL);
        if(m>=0){
           xv_set(filepop->vunumber,PANEL_VALUE,m,NULL);
           xv_set(viewpop->viewvunumber,PANEL_VALUE,m,NULL);
        }
        LoadVuNotify();
        return 1;
       }
       sscanf(buf,"%*s%s%d%d%s%s%s%s",
        key,&m,&n,first,last,chain,filter);
       include = 0;
       for (i=0;i<((int)strlen(key));i++) key[i] = tolower(key[i]);
       if(strstr(key,"all")) {include = include| 1;
            include = include | 2;}
       if(strstr(key,"main")) include = include| 1;
       if(strstr(key,"side")) include = include| 2;
       if(strstr(key,"link")) include = include| 4;
       if(strstr(key,"plane")) include = include| 8;
       if(strstr(key,"other")) include = include| 16;
       if(strstr(key,"non-protein")) include = include| 16;
       if(strstr(key,"hbond")) include = include| 32;
       if(strstr(key,"vanderwaal")) include = include| 64;
       if(strstr(key,"ellips")) include = include| 512;
       if(strstr(key,"arrows")) include = include| 1024;
       if(include==0){
        sprintf(buf,"buildvu: unknown key: %s",key);
        Logger::log(buf);
        return -1;
       }
       if(m>0)
        xv_set(viewpop->viewmodel,PANEL_VALUE,m,NULL);
       if(n>0)
        xv_set(viewpop->viewvunumber,PANEL_VALUE,n,NULL);
       xv_set(viewpop->include,PANEL_VALUE,include,NULL);
       for(i=0;i<((int)strlen(filter));i++)
        if(filter[i]==',')filter[i]=' ';
       xv_set(viewpop->atomfilter,PANEL_VALUE,filter,NULL);
       xv_set(viewpop->chainid,PANEL_VALUE,chain,NULL);
       xv_set(viewpop->residue1,PANEL_VALUE,first,NULL);
       xv_set(viewpop->residue2,PANEL_VALUE,last,NULL);
       BuildVu(0, 0);
       return 1;
       }*/
    IFC("clear")
    {
        Displaylist *models = GetDisplaylist();
        while (models->NumberItems() > 0)
        {
            delete models->FirstItem();
        }
        while (models->MapCount() > 0)
        {
            Purge(models->GetMap(0));
        }
        models->ClearVus();
        return 1;
    }
    IFC("directory")
    {
        std::string file;
        file = MIAfterFirst(file, ' ');

        if (!file.empty())
        {
            QDir::setCurrent(file.c_str());
            return 1;
        }
    }
    IFC("update")
    {
        UpdateCurrent();
        ReDraw();
    }
    IFC("echo")
    {
        std::string mess(ubuf);
        mess = MIBeforeFirst(mess, '\n');
        mess = MIAfterFirst(mess, ' ');
        MIStringTrim(mess, false);
        MIStringTrim(mess, true);
        Logger::footer(mess.c_str());
        return 1;
    }
#ifdef DEFUNCT
    IFC("colormodel")
    {
        if (sscanf(buf, "%*s%d", &m) == 1)
        {
            extern void ColorModel();
            IFC("colormodelatom")
            xv_set(colorpop->colortype, PANEL_VALUE, 0, NULL);
            else
            {
                xv_set(colorpop->colortype, PANEL_VALUE, 1, NULL);
            }
            xv_set(colorpop->colormodelnum, PANEL_VALUE, m, NULL);
            ColorModel();
            return 1;
        }
        else
        {
            return -1;
        }
    }
    IFC("contactpair")
    {
        char resid1[MAXNAME], atomid1[MAXNAME];
        char resid2[MAXNAME], atomid2[MAXNAME];
        char chain1[20], chain2[20];
        RESIDUE *res1, *res2;
        MIAtom *atom1, *atom2;
        int m2, n;

        n = sscanf(buf, "%*s%d%s%s%s%d%s%s%s",
                   &m, resid1, atomid1, chain1,
                   &m2, resid2, atomid2, chain2);
        if (n < 6)
        {
            return -1;
        }
        if ((m  < 1) || (m  > NMODELS))
        {
            return -1;
        }
        if ((m2 < 1) || (m2 > NMODELS))
        {
            return -1;
        }
        res1 = residue_from_id(Res[m -1], resid1, chain1[0]);
        res2 = residue_from_id(Res[m2-1], resid2, chain2[0]);
        if ((res1 == NULL) || (res2 == NULL))
        {
            return -1;
        }
        atom1 = atombyname(atomid1, res1);
        atom2 = atombyname(atomid2, res2);
        if ((atom1 == NULL) || (atom2 == NULL))
        {
            return -1;
        }
        addcontact(atom1, atom2, (float)(-1.0));
        return 1;
    }
    IFC("contactradius") cmdslider(controlpop->contactradius, 10, );
    IFC("cycleize")
    {
        CYCLEIZE = 1;
        return 1;
    }
    IFC("depthcuep") cmdslider(controlpop->depthcuepercent, 1, );
    IFC("drawarrows") cmdtoggle(lsqpop->arrows);
    IFC("drawcontacts") cmdchoice(controlpop->drawcontacts);
    IFC("drawdist") cmdtoggle(controlpop->distancelabel);
    IFC("drawmode") cmdchoice(controlpop->drawmode);
    IFC("echomess") cmdtoggle(controlpop->echomessage);
    IFC("exclusive") cmdtoggle(model1pop->exclusive);
    IFC("exit") exit(0);
    IFC("fitresidues") cmdchoice(lsqpop->fit);
    IFC("orthogonalgridspacing")
    {
        if (sscanf(buf, "%*s%d", &m) == 1)
        {
            xv_set(conturpop->gridspacing, PANEL_VALUE, m, NULL);
        }
    }
    IFC("hidemap")
    {
        if (sscanf(buf, "%*s%d", &m) == 1)
        {
            m = m -1;
            if (m < 0 || m > NMAPS)
            {
                return -1;
            }
            xv_set(hidepop->maplist, PANEL_LIST_SELECT, m, false, NULL);
            HideMap(0, 0);
            return 1;
        }
        else
        {
            return -1;
        }
    }
    IFC("hidemodel")
    {
        if (sscanf(buf, "%*s%d", &m) == 1)
        {
            m = m -1;
            if (m < 0 || m >= NMODELS)
            {
                return -1;
            }
            /*
               if(nLines[m] > 0){
               nLines_save[m] = nLines[m];
               nLines[m] = 0;
               }
             */
            xv_set(hidepop->modellist, PANEL_LIST_SELECT, m, false, NULL);
            HideModel();
            return 1;
        }
        else
        {
            return -1;
        }
    }
    IFC("hidevu")
    {
        if (sscanf(buf, "%*s%d", &m) == 1)
        {
            m = m -1;
            if (m < 0 || m > number_of_vus)
            {
                return -1;
            }
            if (nvu[m] > 0)
            {
                nvu_save[m] = nvu[m];
                nvu[m] = 0;
            }
            VuList();
            return 1;
        }
        else
        {
            return -1;
        }
    }
    /* xfit extension to vu format- label.  other
     * vu software will treat this as a comment
     * format #LABEL x y z color label_string*/
    if (!strncasecmp(buf, "label ", 6) || !strncasecmp(buf, "label\t", 6) )
    {
        MIAtom *atom;
        char colorstring[BUFSIZ];
        int color;
        char name[BUFSIZ];

        if (nlabels > maxlabels)
        {
            Logger::log("LoadVuNotify: max labels exceeded- label skipped");
            return 1;
        }
        atom = new MIAtom();
        if (!atom)
        {
            Logger::log("LoadVuNotify: unable to allocate atom");
            return 1;
        }
        sscanf(buf, "%*s%f%f%f%s%[^\n]s", &atom->x, &atom->y, &atom->z, colorstring, name);
        /* special name to mark it as dummy atom for label purposes */
        strcpy(atom->name, ".lbl");
        /* truncate name if too long */
        name[MAXLABEL-1] = '\0';
        strcpy(labels[nlabels].name, name);
        labels[nlabels].atom = atom;
        if (isalpha(colorstring[0]))
        {
            color = xl_getcolor(colorstring);
        }
        else
        {
            sscanf(colorstring, "%d", &color);
            color = color%number_colors;
            color = (color/4)*4;
        }
        if (color > number_colors)
        {
            color = labelcolor;
        }
        if (color < 0)
        {
            color = labelcolor;
        }
        labels[nlabels].color = color;
        labels[nlabels].xo = atoi((char*)xv_get(labelpop->xoffset, PANEL_VALUE));
        labels[nlabels].yo = atoi((char*)xv_get(labelpop->yoffset, PANEL_VALUE));
        nlabels++;
#if defined (USEACCEL) || defined (USEAVS)
        redraw_labels_flag = 1;
#endif /*(USEACCEL||USEAVS) */

        return 1;
    }
    IFC("labelpick") cmdtoggle(controlpop->labelpicks);
    IFC("echo")
    {
        IFC("echocycle")
        {
            CycleFooter(buf+10);
            return 1;
        }
        Logger::log(buf+5);
        return 1;
    }
    IFC("labelpoint") cmdslider(printpop->labelpoints, 1, );
    IFC("labelstyle") cmdslider(labelpop->labelstyle, 1, );
    IFC("linedepthp") cmdslider(printpop->linedepthpercent, 1, );
    IFC("maskdone")
    {
        MaskDone();
        return 1;
    }
    IFC("cycledone")
    {
        CycleDone();
        return 1;
    }
    IFC("loadridge")
    {
        if (sscanf(buf, "%*s%s", file) == 1)
        {
            extern void RidgeLoad();
            xv_set(filepop->ridgefile, PANEL_VALUE, file, NULL);
            RidgeLoad(0, 0);
            return 1;
        }
        else
        {
            return -1;
        }
    }
    IFC("loadvu")
    {
        if (sscanf(buf, "%*s%d%s", &m, file) == 2)
        {
            xv_set(filepop->vufile, PANEL_VALUE, file, NULL);
            if (m > 0)
            {
                xv_set(filepop->vunumber, PANEL_VALUE, m, NULL);
                xv_set(viewpop->viewvunumber, PANEL_VALUE, m, NULL);
            }
            LoadVuNotify();
            return 1;
        }
        else
        {
            return -1;
        }
    }
    IFC("lsqarrowscale") cmdslider(lsqpop->lsqarrowscale, 1, );
    IFC("makeatomdraggable") cmdtoggle(refinepop->fitpicked);
    IFC("modellinew")
    {
        if (sscanf(buf, "%*s%f", &a) == 1)
        {
            MODELLINEWIDTH = a;
            sprintf(buf, "Model line width now %f\n", a);
            Logger::log(buf);
            return 1;
        }
        else
        {
            return -1;
        }
    }
    IFC("mousedamp") cmdslider(controlpop->mousecontrol, 30,
                               dragfactor = ((float)m) / 30.0);
    IFC("mousemode") cmdtoggle(controlpop->insightmousemode);
    IFC("ncrset") cmdtoggle(conturpop->ncrset);
    IFC("orientation") cmdchoice(printpop->orientation);
    IFC("picksearch") cmdchoice(controlpop->picktype);
    IFC("pop")
    {
        extern int poprefi1();
        extern int poprefi2();
        IFC("poprefi1")
        {
            poprefi1(0, 0);
            return 1;
        }
        else
        {
            IFC("poprefi2")
            {
                poprefi2();
                return 1;
            }
            else
            {
                return -1;
            }
        }
    }
    IFC("pushatom")
    {
        char resid[MAXNAME], atomid[MAXNAME];
        RESIDUE *res;
        MIAtom *atom;
        if (sscanf(buf, "%*s%d%s%s%s", &m, resid, atomid, chain) == 4)
        {
            if (m < 1 || m > NMODELS)
            {
                return -1;
            }
            if ((res = residue_from_id(Res[m-1], resid, chain[0]))
                != NULL)
            {
                if ((atom = atombyname(atomid, res)) != NULL)
                {
                    pushatom(atom);
                    return 1;
                }
            }
        }
        else
        {
            return -1;
        }
    }
    IFC("quit") exit(0);
    IFC("rdensityfunction") cmdchoice(refinepop->setting1);
    IFC("rdisplay") cmdtoggle(refinepop->rdisplay);
    IFC("refine")
    {
        IFC("refineagainst") cmdtoggle(refinepop->refinemap);
        IFC("refineangleweight") cmdslider(refinepop->angleweight, 10, );
        IFC("refinebondweight") cmdslider(refinepop->bondweight, 10, );
        IFC("refinemap ") cmdtoggle(refinepop->refinemap);
        IFC("refinemapweight ") cmdslider(refinepop->mapweight, 10, );
        IFC("refineplaneweight") cmdslider(refinepop->planeweight, 10, );
        IFC("refineverbose") cmdtoggle(refinepop->refineverbose);
        IFC("refinewhilefitting") cmdchoice(refinepop->refinecurrent);
    }
    IFC("residuelist") cmdtoggle(model1pop->exclusive);
    IFC("restrainend")
    {
        restrainends();
        return 1;
    }
    IFC("ridgeboxsize") cmdslider(ridgepop->ridgebox, 1, );
    IFC("ridgecolor") cmdchoice(ridgepop->ridgecolor);
    IFC("ridgelinelevel") cmdslider(ridgepop->levelslider, 1, );
    IFC("ridgelinewidth")
    {
        if (sscanf(buf, "%*s%f", &a) == 1)
        {
            RIDGELINEWIDTH = a;
            sprintf(buf, "Ridge line width now %f\n", a);
            Logger::log(buf);
            return 1;
        }
        else
        {
            return -1;
        }
    }
    IFC("ridgepickmode") cmdchoice(ridgepop->pickmode);
    IFC("ridgeshow") return 1;
    IFC("rocking") cmdtoggle(controlpop->rocking);
    IFC("rockrange") cmdslider(controlpop->rockrange, 1,
                               set_rockrange((float)m));
    IFC("rockrate") cmdslider(controlpop->rockrate, 10,
                              set_rockrate(m / 10.0));
    IFC("savevisible") cmdtoggle(filepop->pdbvisible);
    IFC("segid") cmdtoggle(filepop->usesegid);
    IFC("separation") cmdslider(viewpop->separation, 1, separation = m);
    IFC("setuprefi")
    {
        extern void SetUpRefi();
        SetUpRefi();
        return 1;
    }
    IFC("sfarrowscale") cmdslider(sfpop->arrowscale, 100, );
    IFC("shake") cmdtoggle(sfpop->shake);
    IFC("showmap")
    {
        if (sscanf(buf, "%*s%d", &m) == 1)
        {
            m = m -1;
            if (m < 0 || m > NMAPS)
            {
                return -1;
            }
            xv_set(hidepop->maplist, PANEL_LIST_SELECT, m, true, NULL);
            HideMap(0, 0);
            return 1;
        }
        else
        {
            return -1;
        }
    }
    IFC("showmodel")
    {
        if (sscanf(buf, "%*s%d", &m) == 1)
        {
            m = m -1;
            if (m < 0 || m >= NMODELS)
            {
                return -1;
            }
            /*
               if(nLines_save[m] > 0){
               nLines[m] = nLines_save[m];
               nLines_save[m] = 0;
               }
             */
            xv_set(hidepop->modellist, PANEL_LIST_SELECT, m, true, NULL);
            HideModel();
            return 1;
        }
        else
        {
            return -1;
        }
    }
    IFC("showvu")
    {
        if (sscanf(buf, "%*s%d", &m) == 1)
        {
            m = m -1;
            if (m < 0 || m > number_of_vus)
            {
                return -1;
            }
            if (nvu_save[m] > 0)
            {
                nvu[m] = nvu_save[m];
                nvu_save[m] = 0;
            }
            VuList();
            return 1;
        }
        else
        {
            return -1;
        }
    }
    IFC("stereo")
    {
        IFC("stereoangle") cmdslider(viewpop->stereo_angle, 10,
                                     stereo_angle = (float)m *MAXANGLE/100.0);
        if (strstr(buf, "on"))
        {
            stereo = true;
            xv_set(viewpop->stereo, PANEL_VALUE, stereo, NULL);
            ReDraw();
            return 1;
        }
        else if (strstr(buf, "off"))
        {
            stereo = false;
            ReDraw();
            xv_set(viewpop->stereo, PANEL_VALUE, stereo, NULL);
            return 1;
        }
        else if (strstr(buf, "right"))
        {
            stereo = 3;
            ReDraw();
            xv_set(viewpop->stereo, PANEL_VALUE, stereo, NULL);
            return 1;
        }
        else if (strstr(buf, "left"))
        {
            stereo = 2;
            ReDraw();
            xv_set(viewpop->stereo, PANEL_VALUE, stereo, NULL);
            return 1;
        }
        else
        {
            return -1;
        }
    }
    IFC("surfdensity") cmdslider(window1->currentsurfdensity, 1,
                                 Surfspacing = 1.0/((float)m));
    IFC("symmgen")
    {
        if (4 != sscanf(buf, "%*s%f%f%f%f", &c0, &c1, &c2, &c3))
        {
            return -1;
        }
        SymmCenter[0] = c0;
        SymmCenter[1] = c1;
        SymmCenter[2] = c2;
        SymmRadius = c3;
        SymmAtoms(2);
        return 1;
    }
    IFC("symmrad") cmdslider(controlpop->symmradius, 1, );
    IFC("titlefontsize") cmdslider(printpop->titlefontsize, 1, );
    IFC("transform")
    {
        ApplyView();
        return 1;
    }
    IFC("turnhydrogens") cmdtoggle4(j, HideHNot(0, !j, 0));
    IFC("verboseoutput") cmdtoggle(refinepop->refineverbose);
    IFC("vulinewidth")
    {
        if (sscanf(buf, "%*s%f", &a) == 1)
        {
            VULINEWIDTH = a;
            sprintf(buf, "Vu line width now %f\n", a);
            Logger::log(buf);
            return 1;
        }
        else
        {
            return -1;
        }
    }

#undef IFC
    /*  END */
#endif //IGNORE
    return 0;
}

static void GetScreenCenter(float *center, ViewPoint *vp)
{
    center[0] = vp->center()[0];
    center[1] = vp->center()[1];
    center[2] = vp->center()[2];
}

void MIGLWidget::OnRefiLigandFit()
{
    findLigandFit();
}

void MIGLWidget::findLigandFit()
{
    EMap *currentmap = GetDisplaylist()->GetCurrentMap();
    if (!fitres)
    {
        Logger::message("A residue must be in fitting mode.");
        Logger::log("A residue must be in fitting mode.");
        return;
    }
    if (!currentmap)
    {
        Logger::message("Sorry there is no map to refine against - action canceled");
        Logger::log("Sorry there is no map to refine against - action canceled");
        return;
    }
    if (!currentmap->HasDensity() )
    {
        Logger::message("Sorry there is no map density to refine against - action canceled");
        Logger::log("Sorry there is no map density to refine against - action canceled");
        return;
    }
    if (currentmap->mapheader->resmin > 2.5)
    {
        Logger::message("Map resolution is not high enough for the refiner - action canceled");
        Logger::log("Map resolution is not high enough for the refiner - action canceled");
        return;
    }

    if (fitres != 0 && fitres->atomCount() > 60)
    {
        Logger::message("Too many atoms for the refiner (more than 60)- action canceled");
        Logger::log("Too many atoms for the refiner (more than 60)- action canceled");
        return;
    }

    GeomRefiner *geomRefiner = MIFitGeomRefiner();
    GeomSaver entryConfs;
    const char *type = fitres->type().c_str();
    Residue *res = geomRefiner->dict.GetDictResidue(type, 0);
    if (res == NULL)
    {
        Logger::message("Unable to find dictionary entry for residue %s", type);
        Logger::log("Unable to find dictionary entry for residue %s", type);
        return;
    }
    GetConfs(entryConfs, res, &geomRefiner->dict, fitmol);
    Logger::debug("%d conformations before generation", entryConfs.NumberSets());
    if (entryConfs.NumberSets() <= 2)
    {
        int numberOfConformations = conflib::GenerateEnsemble(geomRefiner->dict.GetDictResidue(type, 0),
                                                              *(geomRefiner->dict.GetDictBonds(type, 0)), &geomRefiner->dict, true);
        Logger::log("Generated %d confirmations", numberOfConformations);
    }

    GeomSaver confs;
    if (fitres != 0
        && fitres->atomCount() == (int)CurrentAtoms.size()
        && AtomVectMatchesRes(CurrentAtoms, fitres)
        && GetConfs(confs, fitres, &geomRefiner->dict, fitmol))
    {
        Logger::log("Using %d conformations of residue %s to fit", confs.NumberSets(), fitres->type().c_str());
    }
    else
    {
        QMessageBox::information(this, "Info", "To use FitLigand, you must be fitting exactly one (whole) residue.");
    }

    if (IsFitting() && CurrentAtoms.size() > 0)
    {
        // refine into position
        if (!BoundingBox)
        {
            BoundingBox = new InterpBox(CurrentAtoms, currentmap);
        }
        fitmol->TranslateAtomsToCenter(CurrentAtoms, viewpoint);
        BoundingBox->ZeroModel(fitmol->residuesBegin());
        MyMIMolOptCheckPoint *ckpt = new MyMIMolOptCheckPoint(this, viewpoint);
        float screen_center[3];
        GetScreenCenter(screen_center, viewpoint);
        MIFitGeomRefiner()->LigandOptimize(
            CurrentAtoms, fitmol, currentmap,
            screen_center, *BoundingBox, Refine_Level::Thorough, confs, ckpt);
        delete ckpt;
        //    MIFitGeomRefiner()->FullOptimize(CurrentAtoms, fitmol, currentmap,
        //      viewpoint, this, *BoundingBox, Refine_Level::Thorough);
        //    MIFitGeomRefiner()->FullOptimize(CurrentAtoms, fitmol, currentmap,
        //      viewpoint, this, *BoundingBox, Refine_Level::Optimize);
        delete BoundingBox;
        BoundingBox = NULL;
        _modified = true;
        Modify(true);
        UpdateCurrent();
        ReDraw();
    }
}

void MIGLWidget::OnUpdateHideModel(QAction *action)
{
    action->setEnabled(GetDisplaylist()->CurrentItem() != NULL && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnRenderLinesmooth()
{
    scene->renderer->setAntialiasLines(!scene->renderer->isAntialiasLines());
    ReDraw();
}

void MIGLWidget::OnUpdateRenderLinesmooth(QAction *action)
{
    action->setChecked(scene->renderer->isAntialiasLines());
}

void MIGLWidget::OnViewSave()
{
    const std::string &pathname = MIFileSelector("Save Viewpoint (.vp) File", "", "", "vp",
                                                 "Vp files (*.vp)|*.vp|All files (*.*)|*.*", MI_SAVE_MODE);
    if (pathname.size())
    {
        XMLArchive ar(pathname.c_str(), CArchive::store);
        ar.BeginTag("ViewPoint");
        ViewPointIO::save(*viewpoint, ar);
        ar.EndTag();
    }
}

void MIGLWidget::OnViewLoad()
{
    const std::string &pathname = MIFileSelector("Load Viewpoint (.vp) File", "", "", "vp",
                                                 "Vp files (*.vp)|*.vp|All files (*.*)|*.*", MI_OPEN_MODE);
    if (pathname.size())
    {
        FILE *fp = fopen(pathname.c_str(), "r");
        ViewPointIO::load(*viewpoint, fp);
        _modified = true;
        ReDraw();
    }
}

Vector3<float> MIGLWidget::mapToCamera(const QPoint &point, int z)
{
    float scale = viewpoint->scale();
    Vector3<float> viewVector(point.x()/scale, -point.y()/scale, z/scale);
    camera->transformVector(viewVector);
    return viewVector;
}

Vector3<float> MIGLWidget::mapToView(const QPoint &point, int z)
{
    float scale = viewpoint->scale();
    Vector3<float> viewVector((point.x() - width()/2.0)/scale,
                              -(point.y() - height()/2.0)/scale,
                              z/scale);
    viewVector += Vector3<float>(viewpoint->center()[0],
                                 viewpoint->center()[1],
                                 viewpoint->center()[2]);
    return viewVector;
}


void MIGLWidget::OnAddWaterAtCursor()
{
    if (MIBusyManager::instance()->Busy())
        return;

    EMap *currentmap = GetDisplaylist()->GetCurrentMap();
    Molecule *model = GetDisplaylist()->CurrentItem();
    if (!model)
    {
        Logger::message("There is no model - use Model/New Model...\nto start a new model and then try again");
        return;
    }

    QPoint p = mapFromGlobal(QCursor::pos());
    Vector3<float> bPoint(mapToView(p));
    if (currentmap)
    {
        // search from -slab to + slab in z
        qreal fslab = viewpoint->frontClip() * viewpoint->scale();
        qreal bslab = viewpoint->backClip() * viewpoint->scale();
        float bdens = -std::numeric_limits<float>::max();
        float dens;
        // calculate .25 A increments in screen coordinates
        float interval = (viewpoint->frontClip()-viewpoint->backClip())/0.25F;
        interval = (fslab - bslab)/interval;
        if ((int)interval <= 0)
            interval = 1.0F;

        for (int is = bslab; is < fslab; is += interval)
        {
            Vector3<float> point = mapToView(p, is);
            if ((dens = currentmap->Rho(point)) > bdens)
            {
                bPoint = point;
                bdens = dens;
            }
        }
    }
    // save model
    if (!SaveModelFile(model, "before adding water"))
        Logger::message("Warning: Unable to save model - will not be able to Undo");

    model->AddWater(bPoint.getX(), bPoint.getY(), bPoint.getZ());
    int nlinks = model->getnlinks();
    model->Build();
    if (nlinks > 0)
        model->BuildLinks();

    // Mark the model as dirty
    Modify(true);
    model->SetModified();
    // the following makes it easier for the user to save the model
    GetDisplaylist()->SetCurrent(model);
    PaletteChanged = true;
    ReDrawAll();
}

void MIGLWidget::OnFindGeomErrors()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    Molecule *model = GetDisplaylist()->GetCurrentModel();
    float errorlevel = 6.0F;
    if (model)
    {
        bool ok = false;
        errorlevel = QInputDialog::getDouble(this, "Enter Error Level",
                             "Enter the error threshold (default 6.0).\n"
                             "Differences from ideality > error_threshold*tolerance\n"
                             "will be annotated\n"
                             "To set tolerances use Refine/Refine Options", errorlevel, 0.0, FLT_MAX, 2 &ok);
        if (!ok)
        {
            return;
        }
        Logger::footer("Clear old errors...");
        model->clearGeomAnnotations();
        Logger::footer("Find Geometry errors...");
        MIFitGeomRefiner()->FindGeomErrors(model, errorlevel);
        Modify(true);
        model->SetModified();
    }
}

void MIGLWidget::OnUpdateFindGeomErrors(QAction *action)
{
    action->setEnabled(GetDisplaylist()->CurrentItem() != NULL && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnClearGeomAnnotations()
{
    Molecule *model = GetDisplaylist()->CurrentItem();
    if (model != NULL)
    {
        model->clearGeomAnnotations();
    }
}

void MIGLWidget::OnLabelEveryNth()
{
    Molecule *model = GetDisplaylist()->CurrentItem();
    if (!model)
    {
        return;
    }

    bool ok = false;
    unsigned int nth = static_cast<unsigned int>(QInputDialog::getInt(this, "Label every nth", "Every nth residue will be labeled", 10, 0, INT_MAX, 1, &ok));
    if (!ok)
    {
        return;
    }
    model->labelEveryNthResidue(nth);
    Modify(true);
    PaletteChanged = true;
    ReDraw();
}

void MIGLWidget::OnUpdateLabelEveryNth(QAction *action)
{
    action->setEnabled(GetDisplaylist()->CurrentItem() != NULL);
}

void MIGLWidget::OnUpdateFindLigandDensity(QAction *action)
{
    action->setEnabled((GetDisplaylist()->GetCurrentMap() != NULL)
                  && (GetDisplaylist()->CurrentItem() != NULL)
                  && (MIBusyManager::instance()->Busy() == false));
}

void MIGLWidget::OnFindLigandDensity()
{
    float xmin, xmax, ymin, ymax, zmin, zmax;
    Displaylist *Models = GetDisplaylist();
    Molecule *model = Models->CurrentItem();
    EMap *emap = Models->GetCurrentMap();
    if (!Molecule::isValid(model))
    {
        return;
    }
    if (!emap)
    {
        return;
    }
    Residue *last;
    Residue *end = model->residuesBegin();
    if (!Monomer::isValid(end))
    {
        return;
    }
    Residue *first;
    ClusterList clusters;
    while (end->next() != NULL)
    {
        end = end->next();
    }
    emap->GetMapHeader()->AsuBounds(xmin, xmax, ymin, ymax, zmin, zmax);
    emap->HydrateMap(170, 10000, 0, model, 1.2F, 50.0F, xmin, xmax, ymin, ymax, zmin, zmax);
    first = end->next();
    if (first)
    {
        end->setNext(NULL);
        last = first;
        int n = 1;
        while (last->next() != NULL)
        {
            last = last->next();
            n++;
        }
        std::string s = ::format("Added %d atoms from %s to %s", n, resid(first).c_str(), resid(last).c_str());
        Logger::log(s.c_str());
        clusters.SetMaxDistance(2.3F);
        Residue *clusterResidues = clusters.BuildClusters(first, last, ClusterSize);
        //FreeResidueList(first);
        s = ::format("Found %d clusters (labeled as CLUST and put at the end of the model)", (int)clusters.size());
        Logger::log(s.c_str());
        Logger::footer(s.c_str());
        if (clusterResidues)
        {
            model->InsertResidues(end, clusterResidues, 3, 'x');
            PaletteChanged = true;
            _modified = true;
            model->Build();
            model->SetModified();
            Modify(true);
            //FIXME: as far as I can tell, clusterResidues is leaked here!
            //model->InsertResidues uses clusterResidues as the basis for a
            //copy, but does not incorporate the original back into the molecule
        }
    }
}

void MIGLWidget::OnFullScreen()
{
    IsFullScreen ^= 1;
    FullScreen(IsFullScreen);
}

void MIGLWidget::OnUpdateFullScreen(QAction *action)
{
    action->setChecked(IsFullScreen == true);
}

void MIGLWidget::OnRefiResidue()
{
    if (MIBusyManager::instance()->Busy())
    {
        return;
    }
    MIAtom *a;
    Residue *res;
    Molecule *node;
    AtomStack->Pop(a, res, node);
    if (!a)
    {
        return;
    }
    AtomStack->Push(a, res, node);
    AtomStack->Push(a, res, node);
    OnRefiRange();
}

void MIGLWidget::OnUpdateRefiResidue(QAction *action)
{
    action->setEnabled(!AtomStack->empty() && MIFitGeomRefiner()->IsRefining() != true && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::ColorModel(Molecule *node)
{
    if (!node)
    {
        return;
    }
    int color = 0;
    std::string at = ("*");
    bool incrementallyColor = false;
    if (Application::instance()->incrementallyColorModels)
    {
        color = 0;
        Displaylist *displaylist = GetDisplaylist();
        Displaylist::ModelList::iterator modelIter = displaylist->begin();
        while (modelIter != displaylist->end())
        {
            Molecule *model = *modelIter;
            ++modelIter;
            if (model == node)
            {
                break;
            }
            ++color;
            // Set incrementallyColor after first model in order to leave default
            // coloring for first model
            incrementallyColor = true;
        }
    }
    for (ResidueListIterator res = node->residuesBegin();
         res != node->residuesEnd(); ++res)
    {
        for (int i = 0; i < res->atomCount(); i++)
        {
            if (!incrementallyColor)
            {
                color = node->getcolor(res, res->atom(i), 1, WhenShownColor,
                                       WhenShownColorMethod, at);
            }
            res->atom(i)->setColor(sign(res->atom(i)->color())*color);
        }
    }
    PaletteChanged = true;
}

void MIGLWidget::OnFitSplitTorsion()
{
    if (!IsTorsioning() || MIBusyManager::instance()->Busy())
    {
        return;
    }
    if (!fitres || !fitmol)
    {
        return;
    }
    if (fitmol->SplitAtoms(CurrentAtoms, true))
    {
        fitmol->RotateTorsion(20.0F);
        PaletteChanged = true;
        _modified = true;
        fitmol->Build();
        fitmol->SetModified();
        Modify(true);
        ReDrawAll();
    }
}

void MIGLWidget::OnUpdateFitSplitTorsion(QAction *action)
{
    action->setEnabled(IsTorsioning() && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnFitSplitFit()
{
    if (!IsFitting() || MIBusyManager::instance()->Busy())
    {
        return;
    }
    if (!fitres || !fitmol)
    {
        return;
    }
    if (fitmol->SplitAtoms(CurrentAtoms, false))
    {
        fitmol->Translate(0.3F, 0, 0, &CurrentAtoms);
        PaletteChanged = true;
        _modified = true;
        fitmol->Build();
        fitmol->SetModified();
        Modify(true);
        ReDrawAll();
    }
}

void MIGLWidget::OnUpdateFitSplitFit(QAction *action)
{
    action->setEnabled(IsFitting() && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnMapReindex()
{
    EMap *map = GetDisplaylist()->GetCurrentMap();
    if (!map)
    {
        return;
    }
    int reindex_mat[3][3] = { {0, 1, 0}, {1, 0, 0}, {0, 0, -1} };
    map->Reindex(reindex_mat);
}

void MIGLWidget::OnUpdateMapReindex(QAction *action)
{
    EMap *map = GetDisplaylist()->GetCurrentMap();
    bool HasPhases = false;
    if (map != NULL)
    {
        HasPhases = map->HasPhases();
    }
    action->setEnabled(HasPhases && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::ClearAtomStack()
{
    if (AtomStack)
    {
        AtomStack->clear();
    }
}

void MIGLWidget::ReDrawAll()
{
    PaletteChanged = true;
    ReDraw();
}

// the other file got too big so MIGLWidget implemntation
//  is continued here

//  yech a global kludge variable. Seems to be to control
//  somthing with atom showing
static bool Wait = false;


void MIGLWidget::WriteStack(CArchive &ar)
{
    Molecule *node, *lastnode = NULL;
    Residue *res;
    MIAtom *a;
    int i;
    char buf2[512], aname[chemlib::MAXATOMNAME], rname[MAXNAME], chainid;
    while (!AtomStack->empty())
    {
        AtomStack->Pop(a, res, node);
        if (lastnode != node)
        {
            sprintf(buf2, "molecule %s\n", node->pathname.c_str());
            ar.Write(buf2, strlen(buf2));
        }
        strcpy(aname, a->name());
        strcpy(rname, res->name().c_str());
        chainid = (char)(res->chain_id()&255);
        if (chainid == ' ')
        {
            chainid = '*';
        }
        for (i = 0; i < (int)strlen(aname); i++)
        {
            if (aname[i] == ' ')
            {
                aname[i] = '_';
            }
        }
        for (i = 0; i < (int)strlen(rname); i++)
        {
            if (rname[i] == ' ')
            {
                rname[i] = '_';
            }
        }
        sprintf(buf2, "atom %3s %3s %c %3s %6.2f %6.2f\n", aname, rname, chainid,
                res->type().c_str(), (float)a->BValue(), (float)a->occ());
        ar.Write(buf2, strlen(buf2));
        lastnode = node;
    }
}


void MIGLWidget::ReadStack(const char *pathname, bool append)
{
    Displaylist *Models = GetDisplaylist();
    Molecule *node = NULL;
    Residue *res;
    //int color;
    FILE *f;
    if ((f = fopen(pathname, "r")) != NULL)
    {
        char buf[512], buf2[512];
        char rname[20], chainid, aname[20];
        MIAtom *a;
        res = NULL;
        if (append == false)
        {
            ClearAtomStack();
        }
        if (IsXML(f))
        {
            while (fgets(buf, sizeof buf, f) != NULL)
            {
                if (strstr(buf, "<AtomStack>"))
                {
                    break;
                }
            }
        }
        while (fgets(buf, sizeof buf, f) != NULL)
        {
            if (buf[0] == '#' || buf[0] == '!' || buf[0] == ';')
            {
                continue;
            }
            if (strstr(buf, "</AtomStack>") )
            {
                break;
            }
            if (strnicmp("molecule", buf, 8) == 0)
            {
                node = NULL;
                std::list<Molecule*>::iterator mlist = Models->begin();
                std::list<Molecule*>::iterator end = Models->end();
                sscanf(buf, "%*s %[^\r\n]", buf2);
                while (mlist != end)
                {
                    if (strcasecmp((*mlist)->pathname.c_str(), buf2) == 0)
                    {
                        node = *mlist;
                        break;
                    }
                    mlist++;
                }
                if (node == NULL)
                {
                    std::string s;
                    s = "Unable to find model: ";
                    s += buf2;
                    Logger::message(s);
                }
            }
            else if (strnicmp("atom", buf, 4) == 0)
            {
                if (node != NULL)
                {
                    sscanf(buf, "%*s%s%s%c", aname, rname, &chainid);
                    if ((res = residue_from_name(node->residuesBegin(), rname, chainid)) != NULL)
                    {
                        if ((a = atom_from_name(aname, *res)) != NULL)
                        {
                            AtomStack->Push(a, res, node);
                        }
                    }
                }

            }
        }
        fclose(f);
        ReDraw();
    }
    else
    {
        Logger::message("Error: MIGLWidget::ReadStack: Could not open file!");
        return;
    }
}

void MIGLWidget::OnObjectStackExpandtopallatomsinresidue()
{
    MIAtom *a;
    Molecule *m;
    Residue *r;
    AtomStack->Pop(a, r, m);

    if (a && a->type() & AtomType::SYMMATOM)
    {
        AtomStack->Push(a, r, m);
        Logger::message("Can't expand symmetry atom(s) on stack");
        return;
    }

    for (int i = 0; i < r->atomCount(); i++)
    {
        AtomStack->Push(r->atom(i), r, m);
    }

    ReDraw();
}

void MIGLWidget::OnUpdateObjectStackExpandtopallatomsinresidue(QAction *action)
{
    action->setEnabled(!AtomStack->empty() && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnUpdateObjectStackExpandtop2residues(QAction *action)
{
    action->setEnabled(AtomStack->size() > 1 && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnObjectStackExpandtop2residues()
{
    StackExpandTop2Range();
    ReDraw();
}

void MIGLWidget::OnObjectStackExpandtop2allatomsinrange()
{
    MIAtom *a1;
    MIAtom *a2;
    Molecule *m1, *m2;
    Residue *r1, *r2;
    AtomStack->Pop(a1, r1, m1);
    AtomStack->Pop(a2, r2, m2);
    if (m1 != m2)
    {
        Logger::message("Both atoms must be in same molecule");
        AtomStack->Push(a2, r2, m2);
        AtomStack->Push(a1, r1, m1);
        return;
    }

    if ((a1 && a1->type() & AtomType::SYMMATOM)
        || (a2 && a2->type() & AtomType::SYMMATOM))
    {
        Logger::message("Can't expand symmetry atom(s) on stack");
        AtomStack->Push(a2, r2, m2);
        AtomStack->Push(a1, r1, m1);
        return;
    }

    if (r1 == r2)
    {
        for (int i = 0; i < r2->atomCount(); i++)
        {
            AtomStack->Push(r2->atom(i), r2, m2);
        }
        return;
    }
    std::vector<ResidueListIterator> marks;
    ResidueListIterator res;
    for (res = m1->residuesBegin(); res != m1->residuesBegin(); ++res)
    {
        if (res == ResidueListIterator(r1)
            || res == ResidueListIterator(r2))
        {
            marks.push_back(res);
        }
    }
    if (marks.size() != 2)
    {
        Logger::message("Error expanding stack: residue not found in model");
        AtomStack->Push(a2, r2, m2);
        AtomStack->Push(a1, r1, m1);
    }

    ++marks[1];
    for (res = marks[0]; res != marks[1]; ++res)
    {
        for (int i = 0; i < res->atomCount(); i++)
        {
            AtomStack->Push(res->atom(i), res, m2);
        }
    }

    ReDraw();
}

void MIGLWidget::OnUpdateObjectStackExpandtop2allatomsinrange(QAction *action)
{
    action->setEnabled(AtomStack->size() > 1 && MIBusyManager::instance()->Busy() == false);
}

bool MIGLWidget::DoYouWantBallAndCylinder()
{
    if (viewpointSettings->GetBallandStick() != ViewPointSettings::BALLANDCYLINDER)
    {
        if (QMessageBox::question(this, "?", "In order to see the effect of this command, the render mode "
                         "needs to be Ball and Cylinder.\n Should I change it for you?",
                         QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        {
            viewpointSettings->SetBallandCylinder();
        }
        return true;
    }
    return false;
}

void MIGLWidget::OnObjectShowresiduerange()
{
    int stackstart = AtomStack->size();
    StackExpandTop2Range();
    SaveColors = true;
    DoingRange++;
    while (AtomStack->size() > stackstart-2 && !AtomStack->empty())
    {
        OnObjectShowresidue();
        SaveColors = false;
    }
    if (DoingRange)
    {
        DoingRange--;
    }
    SaveColors = true;
}

void MIGLWidget::OnUpdateObjectShowresiduerange(QAction *action)
{
    action->setEnabled(AtomStack->size() > 1 && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnUpdateObjectShowsidechainrange(QAction *action)
{
    action->setEnabled(AtomStack->size() > 1 && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnObjectShowsidechainrange()
{
    int stackstart = AtomStack->size();
    SaveColors = true;
    StackExpandTop2Range();
    DoingRange++;
    while (AtomStack->size() > stackstart-2 && !AtomStack->empty())
    {
        OnObjectShowsidechain();
        SaveColors = false;
    }
    if (DoingRange)
    {
        DoingRange--;
    }
    SaveColors = true;
}

void MIGLWidget::OnObjectWhenshowncolor()
{

    GenericDataDialog dlg(this);
    dlg.setWindowTitle("Choose Color");
    dlg.addColorIndexField("Color:", WhenShownColor);
    QStringList methods;
    methods << "Carbon Only" << "All Atoms" << "Secondary Structure" << "B-Value" << "Atom Type"
            << "Hydrophobicity" << "Shapley";
    dlg.addComboField("Method:", methods, WhenShownColorMethod);

    if (dlg.exec() != QDialog::Accepted)
    {
        PaletteChanged = false;
        return;
    }
    WhenShownColor = dlg.value(0).toInt();
    WhenShownColorMethod = dlg.value(1).toInt();
    PaletteChanged = true;
}

void MIGLWidget::OnGotoFitalltoscreen()
{
    Displaylist *Models = GetDisplaylist();
    std::list<Molecule*>::iterator node = Models->begin();
    std::list<Molecule*>::iterator end = Models->end();
    float xmin = std::numeric_limits<float>::max();
    float ymin = xmin;
    float zmin = xmin;
    float xmax = -std::numeric_limits<float>::max();
    float ymax = xmax;
    float zmax = xmax;
    float mxmin, mxmax, mymin, mymax, mzmin, mzmax;
    float mxc, myc, mzc;
    float xc = 0, yc = 0, zc = 0;
    int nmodels = 0;
    viewpoint->Do();
    while (node != end)
    {
        if ((*node)->VisibleBounds(camera, mxmin, mxmax, mymin, mymax, mzmin, mzmax))
        {
            (*node)->Center(mxc, myc, mzc);
            xc += mxc;
            yc += myc;
            zc += mzc;
            nmodels++;
            xmin = std::min(xmin, mxmin);
            ymin = std::min(ymin, mymin);
            zmin = std::min(zmin, mzmin);
            xmax = std::max(xmax, mxmax);
            ymax = std::max(ymax, mymax);
            zmax = std::max(zmax, mzmax);
        }
        node++;
    }
    if (nmodels > 0)
    {
        xc /= nmodels;
        yc /= nmodels;
        zc /= nmodels;
        float fzmin = zmin;
        float fzmax = zmax;
        fzmax = (fzmax - fzmin)/2.0F + 2.0F;
        viewpoint->setSlab(-fzmax, fzmax);
        qreal sw = 0.9*width()/(xmax - xmin + 2);
        qreal sh = 0.9*height()/(ymax - ymin + 2);
        ViewPointAnimation *animation = new ViewPointAnimation(viewpoint, this);
        animation->setCenterEnd(Vector3<float>(xc, yc, zc));
        animation->setScaleEnd(std::min(sw, sh));
        connect(animation, SIGNAL(finished()), this, SLOT(CheckCenter()));
        animation->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

void MIGLWidget::OnObjectRadiusisBall()
{
    WhenShownRadius = 2;
    ReDraw();
}

void MIGLWidget::OnObjectRadiusisCpk()
{
    WhenShownRadius = 3;
    ReDraw();
}

void MIGLWidget::OnObjectRadiusisCylinder()
{
    WhenShownRadius = 1;
    ReDraw();
}

void MIGLWidget::OnUpdateObjectRadiusisBall(QAction *action)
{
    action->setChecked(WhenShownRadius == 2);
}

void MIGLWidget::OnUpdateObjectRadiusisCpk(QAction *action)
{
    action->setChecked(WhenShownRadius == 3);
}

void MIGLWidget::OnUpdateObjectRadiusisCylinder(QAction *action)
{
    action->setChecked(WhenShownRadius == 1);
}

void MIGLWidget::OnUpdateObjectWhenshowncolor(QAction *action)
{
    action->setChecked(WhenShownColorMethod != Colors::COLORASIS && WhenShownColorMethod > 0);
}

void MIGLWidget::OnObjectResiduerangeColor()
{
    OnObjectWhenshowncolor();
    if (!PaletteChanged)
    {
        return;
    }
    int stackstart = AtomStack->size();
    StackExpandTop2Range();
    Residue *res;
    Molecule *node;
    MIAtom *a;
    bool save = true;
    std::string at = ("*");
    while (AtomStack->size() > stackstart-2 && !AtomStack->empty())
    {
        AtomStack->Pop(a, res, node);
        if (!a || !res || !node)
        {
            return;
        }
        if (save)
        {
            node->Do();
        }
        save = false;
        int color;
        for (int i = 0; i < res->atomCount(); i++)
        {
            color = node->getcolor(res, res->atom(i), 1, WhenShownColor,
                                   WhenShownColorMethod, at);
            res->atom(i)->setColor(sign(res->atom(i)->color())*color);
        }
    }
    ReDraw();
}

void MIGLWidget::OnUpdateObjectResiduerangeColor(QAction *action)
{
    action->setEnabled(AtomStack->size() > 1);
}

void MIGLWidget::OnObjectResiduerangeRadius()
{
    int stackstart = AtomStack->size();
    StackExpandTop2Range();
    Wait = true;
    SaveColors = true;
    DoingRange++;
    while (AtomStack->size() > stackstart-2 && !AtomStack->empty())
    {
        OnObjectResidueRadius();
        SaveColors = false;
    }
    if (DoingRange)
    {
        DoingRange--;
    }
    SaveColors = true;
    if (DoYouWantBallAndCylinder())
    {
        ReDraw();
    }
}

void MIGLWidget::OnUpdateObjectResiduerangeRadius(QAction *action)
{
    action->setEnabled(AtomStack->size() > 1);
}

void MIGLWidget::OnObjectResiduerangeTurnoff()
{
    int stackstart = AtomStack->size();
    StackExpandTop2Range();
    SaveColors = true;
    DoingRange++;
    while (AtomStack->size() > stackstart-2 && !AtomStack->empty())
    {
        OnObjectResidueTurnoff();
        SaveColors = false;
    }
    if (DoingRange)
    {
        DoingRange--;
    }
    SaveColors = true;
}

void MIGLWidget::OnUpdateObjectResiduerangeTurnoff(QAction *action)
{
    action->setEnabled(AtomStack->size() > 1);
}

void MIGLWidget::OnObjectResiduesColor()
{
    OnObjectWhenshowncolor();
    if (!PaletteChanged)
    {
        return;
    }
    Residue *res;
    Molecule *node;
    MIAtom *a;
    int color;
    std::string at = ("*");
    bool save = true;
    while (!AtomStack->empty())
    {
        AtomStack->Pop(a, res, node);
        if (!a || !res || !node)
        {
            return;
        }
        if (save)
        {
            node->Do();
        }
        save = false;
        for (int i = 0; i < res->atomCount(); i++)
        {
            color = node->getcolor(res, res->atom(i), 1, WhenShownColor,
                                   WhenShownColorMethod, at);
            res->atom(i)->setColor(sign(res->atom(i)->color())*color);
        }
    }
    ReDraw();
}

void MIGLWidget::OnUpdateObjectResiduesColor(QAction *action)
{
    action->setEnabled(!AtomStack->empty());
}

void MIGLWidget::OnObjectResiduesRadius()
{
    SaveColors = true;
    DoingRange++;
    while (!AtomStack->empty())
    {
        OnObjectResidueRadius();
        SaveColors = false;
    }
    if (DoingRange)
    {
        DoingRange--;
    }
    SaveColors = true;
}

void MIGLWidget::OnUpdateObjectResiduesRadius(QAction *action)
{
    action->setEnabled(!AtomStack->empty());
}

void MIGLWidget::OnObjectResiduesTurnoff()
{
    SaveColors = true;
    DoingRange++;
    while (!AtomStack->empty())
    {
        OnObjectResidueTurnoff();
        SaveColors = false;
    }
    if (DoingRange)
    {
        DoingRange--;
    }
    SaveColors = true;
}

void MIGLWidget::OnUpdateObjectResiduesTurnoff(QAction *action)
{
    action->setEnabled(!AtomStack->empty() );
}

void MIGLWidget::OnObjectAtomColor()
{
    OnObjectWhenshowncolor();
    if (!PaletteChanged)
    {
        return;
    }
    Residue *res;
    Molecule *node;
    MIAtom *a;
    AtomStack->Pop(a, res, node);
    if (!a || !node)
    {
        return;
    }
    if (SaveColors)
    {
        node->Do();
    }
    a->setColor(WhenShownColor);
    _modified = true;
    ReDraw();
}

void MIGLWidget::OnUpdateObjectAtomColor(QAction *action)
{
    action->setEnabled(!AtomStack->empty() );
}

void MIGLWidget::OnObjectAtomRadius()
{
    Residue *res;
    Molecule *node;
    MIAtom *a;
    AtomStack->Pop(a, res, node);
    if (!a || !node)
    {
        return;
    }
    if (SaveColors)
    {
        node->Do();
    }
    _modified = true;
    a->set_radius_type(WhenShownRadius);
    ReDraw();
}

void MIGLWidget::OnUpdateObjectAtomsColor(QAction *action)
{
    action->setEnabled(!AtomStack->empty() );
}

void MIGLWidget::OnUpdateObjectAtomRadius(QAction *action)
{
    action->setEnabled(!AtomStack->empty() );
}

void MIGLWidget::OnObjectAtomsColor()
{
    OnObjectWhenshowncolor();
    if (!PaletteChanged)
    {
        return;
    }
    Residue *res;
    Molecule *node;
    MIAtom *a;
    std::set<Molecule*> saved;
    while (!AtomStack->empty())
    {
        AtomStack->Pop(a, res, node);
        if (!a || !node)
        {
            return;
        }
        if (saved.find(node) != saved.end())
        {
            node->Do();
            saved.insert(node);
        }
        a->setColor(WhenShownColor);
    }
    _modified = true;
    ReDraw();
}

void MIGLWidget::OnObjectAtomsRadius()
{
    Wait = true;
    SaveColors = true;
    while (!AtomStack->empty())
    {
        OnObjectAtomRadius();
        SaveColors = false;
    }
    SaveColors = true;
    if (DoYouWantBallAndCylinder())
    {
        ReDraw();
    }
    Wait = false;
}

void MIGLWidget::OnUpdateObjectAtomsRadius(QAction *action)
{
    action->setEnabled(!AtomStack->empty());
}

void MIGLWidget::OnObjectResidueColor()
{
    OnObjectWhenshowncolor();
    if (!PaletteChanged)
    {
        return;
    }
    Residue *res;
    Molecule *node;
    MIAtom *a;
    AtomStack->Pop(a, res, node);
    if (!a || !res || !node)
    {
        return;
    }
    int color;
    std::string at = ("*");
    if (SaveColors)
    {
        node->Do();
    }
    for (int i = 0; i < res->atomCount(); i++)
    {
        color = node->getcolor(res, res->atom(i), 1, WhenShownColor,
                               WhenShownColorMethod, at);
        res->atom(i)->setColor(sign(res->atom(i)->color())*color);
    }
    ReDraw();
}

void MIGLWidget::OnShowColorallatoms()
{
    OnObjectWhenshowncolor();
    if (!PaletteChanged)
    {
        return;
    }
    Molecule *node = GetDisplaylist()->CurrentItem();
    if (!node)
    {
        return;
    }
    int color;
    std::string at = ("*");
    if (SaveColors)
    {
        node->Do();
    }
    for (ResidueListIterator res = node->residuesBegin();
         res != node->residuesEnd(); ++res)
    {
        for (int i = 0; i < res->atomCount(); i++)
        {
            color = node->getcolor(res, res->atom(i), 1, WhenShownColor,
                                   WhenShownColorMethod, at);
            res->atom(i)->setColor(sign(res->atom(i)->color())*color);
        }
    }
    PaletteChanged = true;
    ReDraw();
}

void MIGLWidget::OnUpdateObjectResidueColor(QAction *action)
{
    action->setEnabled(!AtomStack->empty() );
}

void MIGLWidget::OnObjectResidueRadius()
{
    Residue *res;
    Molecule *node;
    MIAtom *a;
    AtomStack->Pop(a, res, node);
    if (!a || !node || !res)
    {
        return;
    }
    if (SaveColors)
    {
        node->Do();
    }
    for (int i = 0; i < res->atomCount(); i++)
    {
        res->atom(i)->set_radius_type(WhenShownRadius);
    }
    if (WhenShownRadius > 0 && !Wait)
    {
        DoYouWantBallAndCylinder();
    }
    if (!DoingRange)
    {
        ReDraw();
    }
}

void MIGLWidget::OnUpdateObjectResidueRadius(QAction *action)
{
    action->setEnabled(!AtomStack->empty() );
}

void MIGLWidget::OnObjectResidueTurnoff()
{
    Residue *res;
    Molecule *node;
    MIAtom *a;
    int i, non = 0;
    AtomStack->Pop(a, res, node);
    if (!a || !node || !res)
    {
        return;
    }
    if (SaveColors)
    {
        node->Do();
    }
    for (i = 0; i < res->atomCount(); i++)
    {
        if (res->atom(i)->color() >= 0)
        {
            non++;
        }
    }
    if (non == 0)
    {
        return;
    }
    for (i = 0; i < res->atomCount(); i++)
    {
        a = res->atom(i);
        if (non > 1 && !strcmp(a->name(), "CA"))
        {
            if (node->linked(a))
            {
                continue;
            }
        }
        a->setColor(-abs(a->color()));
    }
    if (!DoingRange)
    {
        ReDraw();
    }
}

void MIGLWidget::OnUpdateObjectResidueTurnoff(QAction *action)
{
    action->setEnabled(!AtomStack->empty() );
}

void MIGLWidget::OnShowUndocolorradius()
{
    Displaylist *Models = GetDisplaylist();
    Molecule *node = Models->CurrentItem();
    if (!node)
    {
        return;
    }
    node->UnDo();
    ReDraw();
}

void MIGLWidget::OnUpdateShowUndocolorradius(QAction *action)
{
    Displaylist *Models = GetDisplaylist();
    Molecule *node =  Models->CurrentItem();
    if (node)
    {
        action->setEnabled(node->UnDoable(node) != false);
    }
}

void MIGLWidget::OnObjectShowresidue()
{
    Residue *res;
    Molecule *node;
    MIAtom *a;
    AtomStack->Pop(a, res, node);
    if (!a)
    {
        return;
    }
    std::string resshow;
    std::string at("*");
    if (node != NULL)
    {
        if (SaveColors)
        {
            node->Do();
        }
        node->Select(0, 1, 1, 1, resshow, at, res, res, 0,
                     0, 0, 0, 1, 0, 0);
    }
    if (!DoingRange)
    {
        ReDraw();
    }
}

void MIGLWidget::OnObjectStackDeletetopitem()
{
    AtomStack->Pop();
    ReDraw();
}

void MIGLWidget::OnViewUndo()
{
    viewpoint->UnDo();
    ReDraw();
}

void MIGLWidget::OnUpdateViewUndo(QAction *action)
{
    action->setEnabled(viewpoint->UnDoable() != false);
}

void MIGLWidget::OnShowPickedatomTurnon()
{
    Residue *res;
    Molecule *node;
    MIAtom *a;
    AtomStack->Pop(a, res, node);
    if (!a || !node)
    {
        return;
    }
    if (SaveColors)
    {
        node->Do();
    }
    a->setColor(abs(a->color()));
    _modified = true;
    ReDraw();
}

void MIGLWidget::OnUpdateShowPickedatomTurnon(QAction *action)
{
    action->setEnabled(!AtomStack->empty() );
}

void MIGLWidget::OnShowPickedatomTurnoff()
{
    Residue *res;
    Molecule *node;
    MIAtom *a;
    AtomStack->Pop(a, res, node);
    if (!a || !node)
    {
        return;
    }
    if (SaveColors)
    {
        node->Do();
    }
    a->setColor(-abs(a->color()));
    _modified = true;
    ReDraw();
}

void MIGLWidget::OnUpdateShowPickedatomTurnoff(QAction *action)
{
    action->setEnabled(!AtomStack->empty() );
}

void MIGLWidget::OnShowAllpickedatomsTurnoff()
{
    SaveColors = true;
    while (!AtomStack->empty())
    {
        OnShowPickedatomTurnoff();
        SaveColors = false;
    }
    SaveColors = true;
}

void MIGLWidget::OnUpdateShowAllpickedatomsTurnoff(QAction *action)
{
    action->setEnabled(!AtomStack->empty() );
}

void MIGLWidget::OnShowAllpickedatomsTurnon()
{
    SaveColors = true;
    while (!AtomStack->empty())
    {
        OnShowPickedatomTurnon();
        SaveColors = false;
    }
    SaveColors = true;
}

void MIGLWidget::OnShowRadiusmodel()
{
    Molecule *node = GetDisplaylist()->CurrentItem();
    if (!node)
    {
        return;
    }
    //int color;
    //std::string at= ("*");
    if (SaveColors)
    {
        node->Do();
    }
    for (ResidueListIterator res = node->residuesBegin();
         res != node->residuesEnd(); ++res)
    {
        for (int i = 0; i < res->atomCount(); i++)
        {
            res->atom(i)->set_radius_type(WhenShownRadius);
        }
    }
    DoYouWantBallAndCylinder();
    ReDraw();
}

void MIGLWidget::OnObjectShowresidues()
{
    SaveColors = true;
    DoingRange++;
    while (!AtomStack->empty())
    {
        OnObjectShowresidue();
        SaveColors = false;
    }
    if (DoingRange)
    {
        DoingRange--;
    }
    SaveColors = true;
}

void MIGLWidget::OnUpdateObjectShowresidues(QAction *action)
{
    action->setEnabled(!AtomStack->empty() );
}

void MIGLWidget::OnObjectShowsidechains()
{
    SaveColors = true;
    while (!AtomStack->empty())
    {
        OnObjectShowsidechain();
        SaveColors = false;
    }
    SaveColors = true;
}

void MIGLWidget::OnUpdateObjectShowsidechains(QAction *action)
{
    action->setEnabled(!AtomStack->empty() );
}

void MIGLWidget::OnUpdateShowRadiusmodel(QAction *action)
{
    action->setEnabled(GetDisplaylist()->CurrentItem() != NULL);
}

void MIGLWidget::OnUpdateShowColorallatoms(QAction *action)
{
    action->setEnabled(GetDisplaylist()->CurrentItem() != NULL);
}

void MIGLWidget::OnUpdateShowAllpickedatomsTurnon(QAction *action)
{
    action->setEnabled(!AtomStack->empty() );
}

void MIGLWidget::OnUpdateSurfaceSolvent(QAction *action)
{
    action->setEnabled(GetDisplaylist()->CurrentItem() != NULL);
}

void MIGLWidget::OnShowHidehydrogens()
{
    Displaylist *Models = GetDisplaylist();
    Molecule *node = Models->CurrentItem();
    if (node)
    {
        node->ToggleHydrogens();
    }
    ReDraw();
}

void MIGLWidget::OnUpdateShowHidehydrogens(QAction *action)
{
    Displaylist *Models = GetDisplaylist();
    Molecule *node = Models->CurrentItem();
    if (node)
    {
        action->setChecked(node->HVisible == false);
    }
}

void MIGLWidget::OnObjectSurfaceSpherearoundatom()
{
    Residue *res;
    Molecule *node;
    MIAtom *a;
    AtomStack->Pop(a, res, node);
    if (!a)
    {
        return;
    }
    static float radius = 10.0F;
    static float dotsper = 0.5F;

    bool ok;
    radius = QInputDialog::getDouble(this, "Sphere around atom", "Enter the radius of the sphere:", radius, 0.0, FLT_MAX, 2, &ok);
    if (!ok)
    {
        return;
    }
    node->SurfaceAroundAtom(a, dotsper, radius);
    _modified = true;
    PaletteChanged = true;
    ReDraw();
}

void MIGLWidget::OnUpdateObjectSurfaceSpherearoundatom(QAction *action)
{
    action->setEnabled(!AtomStack->empty());
}

void MIGLWidget::OnShowShowwithinsphere()
{
    static float r = 10.0;
    Displaylist *Models = GetDisplaylist();
    Molecule *node = Models->CurrentItem();
    if (node == NULL)
    {
        return;
    }

    bool ok;
    r = QInputDialog::getDouble(this, "Show Within Sphere", "Enter the radius of the sphere:", r, 0.0, FLT_MAX, 2, &ok);
    if (!ok)
    {
        return;
    }

    node->Do();
    MIAtom *atom;
    int within, i;
    double dx, dy, dz;
    double cx, cy, cz;
    double r2 = (double)r * r;
    cx = (double)viewpoint->center()[0];
    cy = (double)viewpoint->center()[1];
    cz = (double)viewpoint->center()[2];
    for (ResidueListIterator res = node->residuesBegin();
         res != node->residuesEnd(); ++res)
    {
        within = 0;
        for (i = 0; i < res->atomCount(); i++)
        {
            atom = res->atom(i);
            dx = (double)atom->x() - cx;
            dy = (double)atom->y() - cy;
            dz = (double)atom->z() - cz;
            if ((dx*dx + dy*dy + dz*dz) < r2)
            {
                within++;
            }
        }
        for (i = 0; i < res->atomCount(); i++)
        {
            atom = res->atom(i);
            if (within)
            {
                atom->setColor(abs(atom->color()));
            }
            else
            {
                atom->setColor(-abs(atom->color()));
            }
        }
    }
    _modified = true;
    PaletteChanged = true;
    ReDraw();
}

void MIGLWidget::FullScreen(bool full)
{
    if (full)
    {
        QMdiArea *mdi = MIMainWindow::instance()->getMdiArea();
        oldParent = mdi->currentSubWindow();
        containerWidget = oldParent->widget();
        mdi->removeSubWindow(oldParent);
        oldParent->setWidget(NULL);
        containerWidget->setParent(fullScreenWidget);
        fullScreenLayout->addWidget(containerWidget, 1);
        fullScreenWidget->showFullScreen();
    }
    else
    {
        fullScreenWidget->layout()->removeWidget(containerWidget);
        containerWidget->setParent(oldParent);
        oldParent->setWidget(containerWidget);
        QMdiArea *mdi = MIMainWindow::instance()->getMdiArea();
        mdi->addSubWindow(oldParent);
        oldParent->show();
        containerWidget = NULL;
        oldParent = NULL;
        fullScreenWidget->hide();
    }
    IsFullScreen = full;
}

void MIGLWidget::OnShowBackboneAsCATrace()
{
    Molecule *molecule = GetDisplaylist()->CurrentItem();
    if (molecule == NULL)
    {
        return;
    }
    std::string resshow;
    std::string at("*");
    molecule->Do();
    molecule->Select(0, 1, 0, 0, resshow, at, NULL, NULL, true, 0, Colors::COLOROFF, 0, 0);
    molecule->Select(0, 0, 0, 0, resshow, at, NULL, NULL, true, 0, Colors::COLORON, 1, 0);
    ReDraw();
}

void MIGLWidget::OnShowBackboneAsAtoms()
{
    Molecule *molecule = GetDisplaylist()->CurrentItem();
    if (molecule == NULL)
    {
        return;
    }
    std::string resshow;
    std::string at("*");
    molecule->Do();
    molecule->ClearLinks();
    molecule->Select(0, 1, 0, 0, resshow, at, NULL, NULL, true, 0, Colors::COLORON, 0, 0);
    ReDraw();
}

void MIGLWidget::OnShowHideBackbone()
{
    Molecule *node = GetDisplaylist()->CurrentItem();
    if (node == NULL)
    {
        return;
    }
    std::string resshow;
    std::string at("*");
    node->Do();
    node->ClearLinks();
    node->Select(0, 1, 0, 0, resshow, at, NULL, NULL, true, 0, Colors::COLOROFF, 0, 0);
    ReDraw();
}

void MIGLWidget::OnShowSymmAtomsAsAtoms()
{
    GetDisplaylist()->CurrentItem()->setSymmAtomsVisible(true);
    link_symm = false;
    generateSymmAtoms();
}

void MIGLWidget::OnShowSymmAtomsAsCATrace()
{
    GetDisplaylist()->CurrentItem()->setSymmAtomsVisible(true);
    link_symm = true;
    generateSymmAtoms();
}

void MIGLWidget::OnShowHideSymmAtoms()
{
    GetDisplaylist()->CurrentItem()->setSymmAtomsVisible(false);
    clearSymmAtoms();
}

void MIGLWidget::OnShowSaveSymmAtoms()
{
    saveSymmAtoms();
}

void MIGLWidget::OnUpdateShowSaveSymmAtoms(QAction *action)
{
    bool enabled = false;
    MIAtom *a;
    Residue *res;
    Molecule *model;
    AtomStack->Peek(a, res, model);
    if (MIAtom::isValid(a) && (a->type() & AtomType::SYMMATOM) != 0)
    {
        enabled = true;
    }
    action->setEnabled(enabled);
}

void MIGLWidget::OnShowSidechainAtoms()
{
    Molecule *molecule = GetDisplaylist()->CurrentItem();
    if (molecule == NULL)
    {
        return;
    }
    std::string resshow;
    std::string at("*");
    molecule->Do();
    molecule->Select(0, 0, 1, 0, resshow, at, NULL, NULL, true, 0, Colors::COLORON, 0, 0);
    ReDraw();
}

void MIGLWidget::OnHideSidechainAtoms()
{
    Molecule *molecule = GetDisplaylist()->CurrentItem();
    if (molecule == NULL)
    {
        return;
    }
    std::string resshow;
    std::string at("*");
    molecule->Do();
    molecule->Select(0, 0, 1, 0, resshow, at, NULL, NULL, true, 0, Colors::COLOROFF, 0, 0);
    ReDraw();
}

void MIGLWidget::keyPressEvent(QKeyEvent *e)
{
    unsigned int nChar = e->key();
    bool shift = e->modifiers() & Qt::ShiftModifier;
    if (nChar <= 256 && isalpha(nChar))
    {
        // key() doesn't distinguish between upper and lower case keys, so we do it here
        QString s = e->text();
        if (s.size()==0)
        {
            return;
        }
        nChar = s[0].toAscii();
    }

    if (nChar <= 256 && isupper(nChar) && !shift)
    {
        nChar = tolower(nChar);
    }
    if (nChar == 186 && !shift)
    {
        nChar = ';';
    }
    else if (nChar == 186 && shift)
    {
        nChar = ':';
    }
    else if (nChar == 188 && !shift)
    {
        nChar = ',';
    }
    else if (nChar == 188 && shift)
    {
        nChar = '<';
    }
    else if (nChar == 190 && !shift)
    {
        nChar = '.';
    }
    else if (nChar == 340 && !shift)
    {
        nChar = '.';
    }
    else if (nChar == 46 && shift)
    {
        nChar = '>';
    }
    else if (nChar == 190 && shift)
    {
        nChar = '>';
    }
    else if (nChar == 340 && shift)
    {
        nChar = '>';
    }
    else if (nChar == 219 && !shift)
    {
        nChar = '[';
    }
    else if (nChar == 219 && shift)
    {
        nChar = '{';
    }
    else if (nChar == 220 && shift)
    {
        nChar = '|';
    }
    else if (nChar == 221 && !shift)
    {
        nChar = ']';
    }
    else if (nChar == 221 && shift)
    {
        nChar = '}';
    }
    if (!OnKeyDown(nChar, e->modifiers()))
    {
        QDeclarativeView::keyPressEvent(e); // chain to parent
    }
    ReDraw();
    MIMainWindow::instance()->updateToolBar();
}

void MIGLWidget::handleMousePress(QGraphicsSceneMouseEvent *e)
{
    QPoint pos = mapFromScene(e->scenePos());

    OnMousePress(pos, e->button() | e->buttons(), e->modifiers());
    e->accept();
}

void MIGLWidget::OnMousePress(QPoint pos, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(buttons)
    Q_UNUSED(modifiers)
    mouse = pos;
    mousestart = mouse;

    if (!MouseCaptured)
    {
        MouseCaptured = true;
    }

    if (pos.y() > 30)
    {
        SetCursor(imhCross);
    }
    else
    {
        SetCursor(imhZCursor);
    }
}

void MIGLWidget::handleMouseRelease(QGraphicsSceneMouseEvent *e)
{
    QPoint pos = mapFromScene(e->scenePos());

    OnMouseRelease(pos, e->button() | e->buttons(), e->modifiers());
}


void MIGLWidget::OnMouseRelease(QPoint pos, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)
    if (buttons & Qt::LeftButton)
    {
        Displaylist *models = GetDisplaylist();
        Bond bestbond;
        // if the mouse is released without moving then pick the closest atom
        DraggingSlab = false;
        DraggingRotate = false;

        if (pos.x() == mousestart.x() && pos.y() == mousestart.y() && !doubleclicked)
        {
            mouse = QPoint(pos.x(), pos.y());
            if (!TopView)
            {
                annotationPickingRenderable->setModels(models);

                vector<GLuint> ids = mousePicker->pick(pos.x(), pos.y(), frustum, annotationPickingRenderable);

                Annotation *annot = NULL;
                if (ids.size() > 0)
                {
                    annot = annotationPickingRenderable->getAnnotation(ids[0]);
                }
                if (annot)
                {
                    CurrentAnnotation = annot;
                    Logger::log("Picked Annotation:");
                    Logger::log(annot->GetText());
                }
                else
                {
                    prepareAtomPicking();
                    ids = mousePicker->pick(pos.x(), pos.y(), frustum, atomPickingRenderable);
                    MIAtom *atom = NULL;
                    if (ids.size() > 0)
                    {
                        atom = atomPickingRenderable->getAtom(ids[0]);
                    }
                    if (IsFitting())
                    {
                        prepareBondPicking();
                        ids = mousePicker->pick(pos.x(), pos.y(), frustum, bondPickingRenderable);
                        Bond *bond = NULL;
                        if (ids.size() > 0)
                        {
                            bond = bondPickingRenderable->getBond(ids[0]);
                        }
                        if (bond != NULL)
                        {
                            MIAtom *atom1 = bond->getAtom1();
                            MIAtom *atom2 = bond->getAtom2();
                            if (ids.size() > 1 && ids[1] == 2)
                            {
                                MIAtom *a = atom1;
                                atom1 = atom2;
                                atom2 = a;
                            }
                            {
                                Residue *res1 = residue_from_atom(fitmol->residuesBegin(), atom1);
                                Residue *res2 = residue_from_atom(fitmol->residuesBegin(), atom2);
                                AtomStack->Push(atom2, res2, fitmol);
                                AtomStack->Push(atom1, res1, fitmol);
                            }
                            OnFitSetuptorsion();
                        }
                    }
                    if (atom != NULL)
                    {

                        Residue *res = NULL;
                        Molecule *mol = NULL;
                        findResidueAndMoleculeForAtom(atom, res, mol);
                        select(mol, res, atom);
                        if (res != NULL && mol != NULL)
                        {
                            AtomStack->Push(atom, res, mol);
                        }
                    }

                    PaletteChanged = true;
                    doRefresh();
                }
            }
        }
        else
        {
            ReDraw();
        }
        MIMainWindow::instance()->updateNavigator();
        doubleclicked = false;
        if (MouseCaptured)
        {
            MouseCaptured = false;
        }
        DragStart = false;

        if (pos.y() > 30)
        {
            SetCursor(imhCross);
        }
        else
        {
            SetCursor(imhZCursor);
        }
    }

    if (buttons & (Qt::RightButton | Qt::MidButton))
    {
        // we've been showing drags as ball-and-stick and now we need
        // to redraw

        if (pos.x() != mousestart.x() || pos.y() != mousestart.y())
        {
            if (viewpointSettings->GetBallandStick() > ViewPointSettings::BALLANDSTICK)
            {
                ReDraw();
            }
        }
        else
        {
            popup_menu->popup(mapToGlobal(pos));
        }

        if (pos.y() > 30)
        {
            SetCursor(imhCross);
        }
        else
        {
            SetCursor(imhZCursor);
        }

        if (MouseCaptured)
        {
            MouseCaptured = false;
        }
        DragStart = false;

        MIMainWindow::instance()->updateNavigator();
    }

}

void MIGLWidget::handleMouseMove(QGraphicsSceneMouseEvent *e)
{
    QPoint pos = mapFromScene(e->scenePos());

    if (e->buttons() == Qt::NoButton)
    {
        mouse = pos;
    }
    OnMouseMove(pos, e->button() | e->buttons(), e->modifiers());
}

void MIGLWidget::handleMouseDoubleClick(QGraphicsSceneMouseEvent *e)
{
//FIXME: implement double click support.  Note that the widget gets a
//mousePressEvent() and a mouseReleaseEvent() before the
//mouseDoubleClickEvent().

    if (e->button() == Qt::LeftButton)
    {
        OnLButtonDblClk();
    }
}


void MIGLWidget::enterEvent(QEvent*)
{
    MouseInWindow = true;
    if (!TimerOn)
    {
        m_timer = startTimer(ClockTick);
        TimerOn = true;
    }
}

void MIGLWidget::leaveEvent(QEvent*)
{
    MouseInWindow = false;
}


void MIGLWidget::closeEvent(QCloseEvent *event)
{
    if (!OnSaveModified())
    {
        event->ignore();
        return;
    }

    event->accept();

    ViewController::instance()->viewDeactivated(this);

}

void MIGLWidget::prepareAtomPicking()
{
    RenderStyle style = RenderStyle::getBallAndLine();
    style.setBondLine(false);
    style.setBallPercent(viewpointSettings->GetBallSize() / 100.0f);
    atomPickingRenderable->setRenderStyle(style);
    atomPickingRenderable->setModels(GetDisplaylist());
}

void MIGLWidget::prepareBondPicking()
{
    RenderStyle style;
    if (viewpointSettings->GetBallandStick() == ViewPointSettings::STICKS)
    {
        style.set(RenderStyle::getLine());
        style.setBondLineWidth(viewpointSettings->GetLineThickness());
    }
    else if (viewpointSettings->GetBallandStick() == ViewPointSettings::BALLANDSTICK)
    {
        style.set(RenderStyle::getBallAndLine());
        style.setBondLineWidth(viewpointSettings->GetLineThickness());
    }
    else
    {
        style.set(RenderStyle::getBallAndStick());
        style.setStickPercent((viewpointSettings->GetBallSize() / 100.0f) * (viewpointSettings->GetCylinderSize() / 100.0f));
    }
    style.setAtomBall(false);
    bondPickingRenderable->setRenderStyle(style);
    bondPickingRenderable->setMolecule(fitmol);
    //  bondPickingRenderable->setMolecule(GetDisplaylist()->GetCurrentModel());
}

void MIGLWidget::doSlabDrag(int x, int y, int /* dx */, int dy)
{
    static int draggingFront = false;
    if (!DraggingSlab)
    {
        vector<GLuint> ids = mousePicker->pick(x, y, frustum, slabPickingRenderable);
        if (ids.size() > 0)
        {
            draggingFront = (ids[0] == 1);
            SetCursor(imhSlab);
            DraggingSlab = true;
        }
    }
    if (DraggingSlab)
    {
        float dc = (float) dy / (float) viewpoint->scale();
        if (draggingFront)
        {
            viewpoint->setFrontClip(viewpoint->frontClip()+dc);
        }
        else
        {
            viewpoint->setBackClip(viewpoint->backClip()+dc);
        }
    }
}

void MIGLWidget::findResidueAndMoleculeForAtom(MIAtom *atom, Residue* &res, Molecule* &mol)
{
    Displaylist *models = GetDisplaylist();
    std::list<Molecule*>::iterator molIter = models->begin();
    while (molIter != models->end())
    {
        res = residue_from_atom((*molIter)->residuesBegin(), atom);
        if (res != NULL)
        {
            mol = *molIter;
            break;
        }
        res = residue_from_atom((*molIter)->symmResiduesBegin(), atom);
        if (res != NULL)
        {
            mol = *molIter;
            break;
        }
        ++molIter;
    }
}

void MIGLWidget::OnExportImage()
{
    const QString filter = "JPEG Image format (*.jpg);;"
                           "PNG Image format (*.png);;"
                           "TIFF Image format (*.tif)";

    const QString ExportImageDirKey = "exportImageDir";
    const QString ExportImageSelectedFilterKey = "exportImageSelectedFilter";
    QSettings settings;
    const QString exportImageDir = settings.value(ExportImageDirKey, QDir::currentPath()).toString();
    QString selectedFilter = settings.value(ExportImageSelectedFilterKey, "JPEG Image format (*.jpg)").toString();

    QString path = QFileDialog::getSaveFileName(this, "Export Image", exportImageDir, filter, &selectedFilter);
    if (path.isEmpty())
        return;

    QString defaultExt;
    if (selectedFilter.startsWith("JPEG"))
        defaultExt = ".jpg";
    else if (selectedFilter.startsWith("PNG"))
        defaultExt = ".png";
    else if (selectedFilter.startsWith("TIFF"))
        defaultExt = ".tif";

    std::string ext(file_extension(path.toAscii().constData()));
    if (ext.empty())
    {
        path += defaultExt;
    }

    scene->renderer->setPickingEnabled(true);
    QPixmap image = static_cast<QGLWidget*>(viewport())->renderPixmap(); //TODO: could alter canvas size here
    image.save(path);
    scene->renderer->setPickingEnabled(false);

    QFileInfo imagePath(path);
    settings.setValue(ExportImageDirKey, imagePath.absoluteDir().canonicalPath());
    settings.setValue(ExportImageSelectedFilterKey, selectedFilter);
}



void MIGLWidget::OnDeleteAtom()
{
    deleteAtom();
}

void MIGLWidget::OnUpdateDeleteAtom(QAction *action)
{
    action->setEnabled(!AtomStack->empty());
}

void MIGLWidget::deleteAtom()
{
    Displaylist *models = GetDisplaylist();
    if (!Displaylist::isValid(models))
    {
        Logger::debug("MIGLWidget::deleteAtom: invalid displaylist");
        return;
    }
    MIAtom *atom =  models->GetPickedAtom();
    Residue *res = models->GetPickedResidue();
    Molecule *model = models->GetPickedMolecule();
    if (!MIAtom::isValid(atom) || !Monomer::isValid(res) || !MIMoleculeBase::isValid(model))
    {
        Logger::debug("MIGLWidget::deleteAtom: invalid atom/res/mol");
        return;
    }

    std::string mess = ::format("Deleted atom %s in %s", MIAtom::liststring(atom).c_str(), resid(res).c_str());
    SaveModelFile(model, mess.c_str());
    model->DeleteAtom(atom);
    model->SetModified();
}

void MIGLWidget::OnContourListFile()
{
    const std::string &s = MIFileSelector("Choose contour list file", "", "", "txt",
                                          "Text file (*.txt)|*.txt|All files (*.*)|*.*", MI_SAVE_MODE);
    if (s.size())
    {
        FILE *listFile = fopen(s.c_str(), "w");
        Displaylist *displaylist = GetDisplaylist();
        std::vector<EMap*> &maps = displaylist->getMaps();
        std::vector<EMap*>::iterator map = maps.begin();
        int mapNumber = 1;
        while (map != maps.end())
        {
            EMap *emap = *map;
            ++map;
            if (emap->Visible())
            {
                std::vector<PLINE> &lines = emap->edges;
                Logger::debug("map %d: Writing %d lines", mapNumber, lines.size());
                std::vector<PLINE>::iterator line = lines.begin();
                while (line != lines.end())
                {
                    PLINE &pline = *line;
                    ++line;
                    APOINT p1 = pline.p1;
                    APOINT p2 = pline.p2;
                    fprintf(listFile, "%d %0.3f %0.3f %0.3f\n", 0, p1.x, p1.y, p1.z);
                    fprintf(listFile, "%d %0.3f %0.3f %0.3f\n", 1, p2.x, p2.y, p2.z);
                }
            }
            ++mapNumber;
        }
        fclose(listFile);
    }
}

const Residue*MIGLWidget::getFocusResidue()
{
    if (focusres == NULL)
    {
        focusres = NULL;
    }
    return focusres;
}

void MIGLWidget::setFocusResidue(Residue *res, bool recenter, bool deleted)
{
    focusresDeleted = deleted;
    doSetFocusResidue(res);
    if ((focusres != NULL))
    {
        select(NULL, focusres, NULL, false);
        Displaylist *models = GetDisplaylist();
        Molecule *mol = models->GetPickedMolecule();
        MIAtom *atom = models->GetPickedAtom();
        if (!MIAtom::isValid(atom) || !Monomer::isValid(focusres) || !MIMoleculeBase::isValid(mol))
        {
            Logger::debug("MIGLWidget::setFocusResidue: invalid atom/res/mol");
            return;
        }
        AtomStack->Push(models->GetPickedAtom(), focusres, mol);
        if (recenter)
        {
            CenterAtResidue(focusres);
            EMap *map = GetDisplaylist()->GetCurrentMap();
            if (map != NULL)
                doMapContour(map);

            doRefresh();
        }
    }
}

void MIGLWidget::doSetFocusResidue(Residue *res)
{
    if (focusres == res)
        return;

    focusres = res;
    focusResidueChanged(focusres);
}

void MIGLWidget::PurgeChain(Residue *chain)
{
    Residue *res = chain;
    short chain_id = chain->chain_id();
    while ((res != NULL) && res->chain_id() == chain_id)
    {
        Residue *nextres = res->next();
        Purge(res);
        res = nextres;
    }
}

void MIGLWidget::select(Molecule *model, Residue *residue, MIAtom *atom, bool label)
{
    Displaylist *models = GetDisplaylist();
    if (!MIMoleculeBase::isValid(model))
        model = models->GetPickedMolecule();

    if (!MIMoleculeBase::isValid(model))
        model = models->CurrentItem();

    if (!Monomer::isValid(residue))
        residue = models->GetPickedResidue();

    if (!MIAtom::isValid(atom))
        atom = atom_from_name("CA", *residue);

    if (!MIAtom::isValid(atom) && Monomer::isValid(residue) && residue->atomCount() > 0)
        atom = residue->atom(0);


    if (!MIAtom::isValid(atom) || !Monomer::isValid(residue) || !MIMoleculeBase::isValid(model))
    {
        Logger::debug("MIGLWidget::select: invalid atom/res/mol");
        return;
    }
    models->SetPicked(model, residue, atom);

    doSetFocusResidue(models->GetPickedResidue());
    focusnode = models->GetPickedMolecule();
    if (label && Application::instance()->LabelPicks)
        models->LabelPick(Application::instance()->LabelToggle);

    doRefresh();
}

void MIGLWidget::OnRenderTargetSize()
{

    bool ok;
    float size = scene->getTargetSize();
    size = QInputDialog::getDouble(this, "Edit view target size", "Edit view target size", size, 0.0, FLT_MAX, 2, &ok);
    if (!ok)
        return;

    scene->setTargetSize(size);
}


void MIGLWidget::OnUpdateRefiLigandFit(QAction *action)
{
    action->setEnabled(IsFitting() && fitres != NULL);
}

void MIGLWidget::handleKey_space(bool spaceKeyDown)
{
    if (MIFitGeomRefiner()->IsRefining())
    {
        if (spaceKeyDown) // make sure key is still down, and not a buffered keypress
        {
            OnNextBatonPosition();
            MIFitGeomRefiner()->Refine();
            UpdateCurrent();
            _modified = true;
            ReDraw();
        }
    }
    else
    {
        if (focusres != NULL)
        {
            if (focusresDeleted)
                setFocusResidue(focusres);

            else if (focusres->next() != NULL)
                setFocusResidue(focusres->next());
        }
    }
}

bool MIGLWidget::OnOpenDocument(const std::string &path)
{
    ViewPoint *viewpoint = GetViewPoint();
    Molecule *node;
    const char *pathname = (const char*)path.c_str();
    char buf[2000];
    char fileBuf[2000];
    std::auto_ptr<io> ioObj(io::defaultIo());
    io &file = *ioObj;
    int nmap;
    std::string initPath = QDir::currentPath().toStdString();
    QDir::setCurrent(QFileInfo(pathname).path());
    try
    {
        if (IsXMLDocument(pathname))
        {
            LoadPDBFile(pathname);
            if (!file.open(pathname, "r"))
            {
                std::string s("MIGLWidget::OnOpenDocument:Can't open file: ");
                s += pathname;
                Logger::message(s);
                QDir::setCurrent(initPath.c_str());
                return false;
            }
            viewpoint->Do();
            ViewPointIO::load(*viewpoint, file.fp());
            file.rewind();
            // scan for maps
            std::string col_names("");
            while (file.gets(buf, sizeof buf) != NULL)
            {
                if (strncasecmp(buf, "mapcolumns", 10) == 0)
                {
                    char labelsBuf[4096];
                    sscanf(buf, "%*s %[^\r\n]", labelsBuf);
                    col_names = std::string(labelsBuf);
                    continue;
                }
                if (strncasecmp(buf, "loadmap", 7) == 0)
                {
                    LoadMap(buf, pathname, col_names);
                    col_names = "";
                }
            }
            file.close();

            ReadStack(pathname, false);
            SetDocumentSaved(true);
        }
        else if (strcmp(&pathname[strlen(pathname)-3], ".VP")
                 && strcmp(&pathname[strlen(pathname)-3], ".vp")
                 && strcmp(&pathname[strlen(pathname)-4], ".mlw")
                 && strcmp(&pathname[strlen(pathname)-4], ".MLW"))
        {
            // read in pdb file here
            LoadPDBFile(pathname);
        }
        else
        {
            if (!file.open(pathname, "r"))
            {
                std::string s("MIGLWidget::OnOpenDocument:Can't open file: ");
                s += pathname;
                Logger::message(s);
                QDir::setCurrent(initPath.c_str());
                return false;
            }
            // Scan for silentmode
            while (file.gets(buf, sizeof buf) != NULL)
            {
                if (strncasecmp(buf, "silent", 6) == 0)
                {
                    Application::instance()->SetSilentMode(true);
                    //break;
                }
                if (strncasecmp(buf, "directory", 9) == 0)
                {
                    std::string mess(buf);
                    mess = MIBeforeFirst(mess, '\n');
                    mess = MIAfterFirst(mess, ' ');
                    MIStringTrim(mess, false);
                    MIStringTrim(mess, true);
                    if (QDir::setCurrent(mess.c_str()) == false)
                    {
                        Logger::log("Directory command failed:");
                        Logger::log(mess);
                    }
                }
            }
            file.rewind();
            // Scan for LoadPDB's
            while (file.gets(buf, sizeof buf) != NULL)
            {
                if (strncasecmp(buf, "loadpdb", 7) == 0)
                {
                    std::string oldpath = QDir::currentPath().toStdString();
                    QDir::setCurrent(path.c_str());
                    int nmodel;
                    sscanf(buf, "%*s%d %[^\r\n]", &nmodel, fileBuf);
                    LoadPDBFile(fileBuf);
                    QDir::setCurrent(oldpath.c_str());
                }
            }
            std::list<Molecule*>::iterator node = Models->begin();
            while (node != Models->end())
            {
                file.rewind();
                (*node)->Load(file.fp());
                node++;
            }

            file.rewind();
            viewpoint->Do();
            ViewPointIO::load(*viewpoint, file.fp());
            file.rewind();
            // scan for maps

            std::string col_names("");
            while (file.gets(buf, sizeof buf) != NULL)
            {
                if (strncasecmp(buf, "mapcolumns", 10) == 0)
                {
                    char labelsBuf[4096];
                    sscanf(buf, "%*s %[^\r\n]", labelsBuf);
                    col_names = std::string(labelsBuf);
                    continue;
                }
                if (strncasecmp(buf, "loadmap", 7) == 0)
                {

                    if (sscanf(buf, "%*s%d %[^\r\n]", &nmap, fileBuf) != 2)
                    {
                        continue;
                    }
                    EMap *map = new EMap;
                    if (checkmappath(fileBuf))
                    {
                        if (strncasecmp(buf, "loadmapphase", 12) == 0)
                        {
                            if (col_names.size())
                            {
                                map->InterpretColumnLabelString(col_names.c_str());
                                col_names = "";
                            }
                            if (map->LoadMapPhaseFile(fileBuf))
                            {
                                map->SetMapNumber(nmap);
                                GetDisplaylist()->AddMap(map);
                                map->Read(pathname);
                                if (map->FFTMap())
                                    doMapContour(map);
                            }
                            else
                                delete map;
                        }
                        else
                        {
                            if (map->LoadMapFile(fileBuf))
                            {
                                map->SetMapNumber(nmap);
                                GetDisplaylist()->AddMap(map);
                                map->Read(pathname);
                                doMapContour(map);
                            }
                            else
                                delete map;
                        }
                    }
                    else
                        delete map;
                }
            }

            // Read the script for general stuff
            file.rewind();
            viewpoint->Do();
            ViewPointIO::load(*viewpoint, file.fp());
            file.rewind();
            while (file.gets(buf, sizeof buf) != NULL)
                ScriptCommand(buf);

            Application::instance()->SetSilentMode(false);
            file.close();
            ReadStack(pathname, false);
            SetDocumentSaved(true);
        }
        node = Models->CurrentItem();
        QDir::setCurrent(QFileInfo(pathname).path());
        Modify(false);
        std::string filename = path;
        // check the extension - should be mlw
        std::string ext = MIAfterLast(filename, '.');
        if (strcasecmp(ext.c_str(), "mlw") != 0)
        {
            std::string newpath(filename);
            newpath.erase(newpath.size()-ext.size());
            newpath += "mlw";
            filename = newpath;
            Modify(true);
        }
        SetTitle(filename);
        SetFilename(filename);
        QDir::setCurrent(initPath.c_str());
        return true;
    }
    catch (...)
    {
        Logger::message("Program has detected an unresolvable error opening the document\n"
                        "Possible causes: document corrupted, document read-locked, files have been moved");
        QDir::setCurrent(initPath.c_str());
        return false;
    }
    QDir::setCurrent(initPath.c_str());
    return true;
}

bool MIGLWidget::LoadPDBFile(const char *pathname)
{
    bool needscoloring = true;
    Residue *reslist;
    //std::deque<MIAtom> *atoms= new std::deque<MIAtom>;
    std::vector<Bond> connects;
    std::string compound;
    std::auto_ptr<io> ioObj(io::defaultIo());
    io &file = *ioObj;
    if (!file.open(pathname, "r"))
    {
        std::string s("MIGLWidget::LoadPDBFile:Can't open file: ");
        s += pathname;
        Logger::message(s);
        return false;
    }
    compound = pathname;
    if (IsXMLMolecule(file))
    {
        needscoloring = false;
        if (LoadXMLMolecule(pathname, 0) == false)
        {
            file.close();
            return false;
        }
    }
    else
    {
        if ((reslist = LoadPDB(file.fp(), &connects)) == NULL)
        {
            file.close();
            return false;
        }
        Models->AddItem(reslist, compound, file.fp(), &connects, MoleculeType::PDB);
    }

    if (needscoloring)
    {
        ColorModel(GetDisplaylist()->CurrentItem() );
    }
    newfile = 1;
    SetTitle(pathname);
    Modify(true);
    file.close();
    return true;
}

bool MIGLWidget::OnNewDocument()
{
    std::string path = GetFilename();
    path += ".mlw";
    SetFilename(path);
    SetTitle(path);
    return true;
}

bool MIGLWidget::SaveDocument(const std::string &pathname)
{
    XMLArchive ar(pathname.c_str(), CArchive::store);
    // check to see if file was opened
    if (!ar.IsOpened())
    {
        return false;
    }
    Serialize(ar);
    std::string ext = file_extension(pathname.c_str());
    if (strcasecmp("mlw", ext.c_str()) == 0)
    {
        Modify(false);
        SetFilename(pathname);
        SetDocumentSaved(true);
    }
    std::string s = "Saved ";
    s += pathname;
    Logger::log(s);
    return true;
}

bool MIGLWidget::OnSaveDocument(const std::string &pathname)
{
    std::string filename = pathname;
    // check the extension - should be mlw
    std::string ext = MIAfterLast(pathname, '.');
    if (strcasecmp(ext.c_str(), "mlw") != 0)
    {
        std::string newpath(pathname);
        newpath.erase(newpath.size()-ext.size());
        newpath += "mlw";
        std::string mess("The file does not have the extension 'mlw'\n"
                         "Would this name be OK? (Cancel to use old name)");
        QString path = QInputDialog::getText(this, "Suggested Extension Change", mess.c_str(), QLineEdit::Normal, newpath.c_str());
        if (!path.isEmpty())
        {
            filename = path.toStdString();
        }
    }
    return SaveDocument(filename);
}

bool MIGLWidget::OnSaveModified()
{
    if (_modified && Application::instance()->onCloseSaveActiveModelToPdb)
    {
        exportCurrentModel();
    }

    if (_modified)
    {
        std::string title = GetTitle();
        if (endsWith(title, "[*]"))
        {
            title = title.substr(0, title.size()-3);
        }
        std::string msgTitle = "MIFit: Warning";
        std::string prompt = ::format("Do you want to save changes to document %s?", title.c_str());

        int ret = QMessageBox::question(this, msgTitle.c_str(), prompt.c_str(),
                                        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        switch (ret)
        {
        case QMessageBox::No:
            Modify(false);
            return true;
            break;
        case QMessageBox::Yes:
            if (_filename.size())
                return SaveDocument(_filename);
            else
                return SaveAs();
            break;
        case QMessageBox::Cancel:
            return false;
            break;
        }
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////
// MIGLWidget serialization

void MIGLWidget::Serialize(XMLArchive &ar)
{
    ViewPoint *viewpoint = GetViewPoint();
    std::string date;
    {
        time_t t = ::time(0);
        date = ctime(&t);
    }
    std::list<Molecule*>::iterator node = Models->begin();
    std::list<Molecule*>::iterator end = Models->end();
    int i;

    std::string s;

    if (ar.IsStoring())
    {
        s = ::format("CreationDate=\"%s\"", date.c_str());
        s += " version=\"1.1\"";
        ar.BeginTag("wxFitDocument", (const char*)s.c_str());
        while (node != Models->end())
        {
            (*node)->Save(ar);
            node++;
        }
        for (i = 0; i < Models->MapCount(); i++)
        {
            ar.BeginTag("EMap");
            ar.WriteMapHeader(*Models->GetMap(i)->mapheader);
            Models->GetMap(i)->Save(ar, i+1);
            ar.EndTag();   // EMap
        }

        ar.BeginTag("AtomStack");
        WriteStack(ar);
        ar.EndTag();  //AtomStack

        ar.BeginTag("ViewPoint");
        ViewPointIO::save(*viewpoint, ar);
        ar.EndTag();  //ViewPoint

        ar.EndTag();  //wxFitDocument
    }
}

/////////////////////////////////////////////////////////////////////////////
// MIGLWidget commands
void MIGLWidget::OnEditClearlabels()
{
    Models->ClearLabels();
    Modify(true);
    ReDraw();
}

void MIGLWidget::OnEditLabels()
{
    GenericDataDialog dlg(this);
    dlg.setWindowTitle("Label Options");
    QStringList styles;
    styles << "GLU 100 A CA" << "GLU 100 A" << "GLU 100" << "GLU" << "100"
           << "CA" << "E 100 A" << "E 100" << "E";
    dlg.addComboField("Label Style:", styles, ATOMLABEL::defaultStyle());
    dlg.addIntField("Label Size:", ATOMLABEL::defaultSize());
    dlg.addColorField("Label Color:", QColor(ATOMLABEL::defaultRed(), ATOMLABEL::defaultGreen(), ATOMLABEL::defaultBlue()));
    dlg.addBoolField("Label picked atoms:", Application::instance()->LabelPicks);
    dlg.addBoolField("Toggle labels when picked again:", Application::instance()->LabelToggle);
    dlg.addBoolField("Save these options as my defaults:", false);

    if (dlg.exec() != QDialog::Accepted)
    {
        return;
    }

    unsigned int style = dlg.value(0).toInt();
    bool doAtomLabelUpdate = false;
    if (style != (unsigned int)ATOMLABEL::defaultStyle())
    {
        ATOMLABEL::defaultStyle(style);
        doAtomLabelUpdate = true;
    }
    ATOMLABEL::defaultSize(dlg.value(1).toInt());
    QColor color = dlg.value(2).value<QColor>();
    ATOMLABEL::defaultColor(color.red(), color.green(), color.blue());
    Application::instance()->LabelPicks = dlg.value(3).toBool();
    Application::instance()->LabelToggle = dlg.value(4).toBool();
    if (dlg.value(5).toBool())
    {
        QSettings settings;
        settings.setValue("View Parameters/LabelStyle", ATOMLABEL::defaultStyle());
        settings.setValue("View Parameters/LabelSize", ATOMLABEL::defaultSize());
        settings.setValue("View Parameters/LabelColorRed", ATOMLABEL::defaultRed());
        settings.setValue("View Parameters/LabelColorGreen", ATOMLABEL::defaultGreen());
        settings.setValue("View Parameters/LabelColorBlue", ATOMLABEL::defaultBlue());
        settings.setValue("View Parameters/LabelPicks", Application::instance()->LabelPicks);
        settings.setValue("View Parameters/LabelToggle", Application::instance()->LabelToggle);
    }
    if (doAtomLabelUpdate)
    {
        Molecule *lastnode = Models->CurrentItem();
        if (lastnode)
        {
            lastnode->updateAtomLabels();
        }
    }
    ReDraw();
}


void MIGLWidget::OnFileSave()
{
    OnSaveDocument(_filename);
}

void MIGLWidget::OnFileSaveAs()
{
    SaveAs();
}

void MIGLWidget::OnAnnotation()
{
    Molecule *node = Models->CurrentItem();
    if (!node)
    {
        return;
    }

    QString str = QInputDialog::getText(this, "Annotation", "Type in the annotation:");
    if (str.isEmpty())
    {
        return;
    }

    ViewPoint *viewpoint = GetViewPoint();
    node->addAnnotation(str.toAscii().constData(), viewpoint->center()[0], viewpoint->center()[1], viewpoint->center()[2]);
    // make most recently added annotation the CurrentAnnotation
    CurrentAnnotation = node->getAnnotations().back();
}

void MIGLWidget::Purge(EMap *emap)
{
    MIFitGeomRefiner()->Purge(emap);
    GetDisplaylist()->DeleteMap(emap);
}


void MIGLWidget::OnNewModel()
{
    QString compound = QInputDialog::getText(this, "Input model name", "What do you want to call this model?");
    if (compound.isEmpty())
    {
        return;
    }

    vector<Bond> connects;
    Models->AddItem(NULL, compound.toAscii().constData(), NULL, &connects, MoleculeType::New);
    //OnFitInsertresidue();
    Modify(true);
    ReDrawAll();
}

void MIGLWidget::OnUpdateAnnotation(QAction *action)
{
    action->setEnabled(Models != NULL && Models->CurrentItem() != NULL && MIBusyManager::instance()->Busy() == false);
}

Residue*MIGLWidget::GetDictRes(const char *key, int confomer)
{
    return MIFitDictionary()->GetDictResidue(key, confomer);
}

Residue*MIGLWidget::GetDictRes(const char key_single, int confomer)
{
    return MIFitDictionary()->GetDictResidue(key_single, confomer);
}

vector<std::string> MIGLWidget::GetDictResList()
{
    std::vector<std::string> reslist;
    std::vector<std::string> result;
    reslist = MIFitDictionary()->GetDictResList();
    for (unsigned int i = 0; i < reslist.size(); ++i)
    {
        result.push_back(reslist[i].c_str());
    }
    return result;
}

bool MIGLWidget::LoadXMLMolecule(const char *pathname, int nmodel)
{
    nmodel = 1;

    inputMapHeaders.clear();
    MoleculeXmlHandler molHandler;
    QXmlSimpleReader reader;
    reader.setContentHandler(&molHandler);
    reader.setErrorHandler(&molHandler);

    QFile file(pathname);
    if (!file.open(QFile::ReadOnly | QFile::Text))
    {
        QMessageBox::warning(this, tr("SAX Bookmarks"),
                             tr("Cannot read file %1:\n%2.")
                             .arg(pathname)
                             .arg(file.errorString()));
        return false;
    }

    QXmlInputSource xmlInputSource(&file);
    if (!reader.parse(xmlInputSource))
    {
        QMessageBox::warning(this, tr("MIFit"),
                             tr("Error parsing file %1:\n%2.")
                             .arg(pathname)
                             .arg(molHandler.errorString()));
        return false;
    }
    inputMapHeaders = molHandler.mapHeaders;

    vector<Molecule*>::iterator mi;
    for (mi = molHandler.molecules.begin(); mi != molHandler.molecules.end(); ++mi)
    {
        // add molecule to displaylist
        if ((*mi)->residuesBegin() != (*mi)->residuesEnd())
        {
            Models->AddItem((*mi));
            (*mi)->modelnumber = nmodel;
            nmodel++;
        }
    }

    return true;
}

bool MIGLWidget::IsXMLMolecule(io &file)
{
    char buf[2048];
    bool found_xmltag = false;
    bool found_moleculetag = false;
    file.rewind();
    while (file.gets(buf, sizeof(buf)) != NULL)
    {
        if (strstr(buf, "<?xml"))
        {
            found_xmltag = true;
        }
        if (found_xmltag && strstr(buf, "<Molecule"))
        {
            found_moleculetag = true;
        }
    }
    file.rewind();
    return (found_xmltag && found_moleculetag);
}

bool MIGLWidget::IsXMLDocument(const char *pathname)
{
    std::auto_ptr<io> ioObj(io::defaultIo());
    io &file = *ioObj;
    file.open(pathname, "r");
    if (!file.isOpen())
    {
        return false;
    }
    char buf[2048];
    bool found_xmltag = false;
    bool found_doctag = false;
    int count = 0;
    while (file.gets(buf, sizeof(buf)) != NULL && count < 100)
    {
        if (strstr(buf, "<?xml"))
        {
            found_xmltag = true;
        }
        if (found_xmltag && strstr(buf, "<wxFitDocument"))
        {
            found_doctag = true;
        }
        count++;
    }
    file.close();
    return (found_xmltag && found_doctag);
}

void MIGLWidget::OnExportModel()
{
    exportCurrentModel();
}

void MIGLWidget::exportCurrentModel()
{
    Molecule *model = Models->CurrentItem();
    if (model)
    {
        const std::string &s = MIFileSelector("Save active model to PDB file", "", "", "pdb",
                                              "PDB files (*.pdb)|*.pdb|All files (*.*)|*.*", MI_SAVE_MODE);
        if (s.size())
        {
            model->SavePDBFile(s.c_str());
        }
    }
}

void MIGLWidget::OnUpdateExportModel(QAction *action)
{
    action->setEnabled(Models->CurrentItem() != NULL && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnMoveAnnotationAtom()
{
    MIAtom *a = AtomStack->Pop();
    CurrentAnnotation->SetPosition(a->x(), a->y(),
                                   a->z());
    Modify(true);
    _modified = true;
    ReDraw();
}

void MIGLWidget::OnUpdateMoveAnnotationAtom(QAction *action)
{
    action->setEnabled(CurrentAnnotation != NULL && !AtomStack->empty() && MIBusyManager::instance()->Busy() == false);
}

void MIGLWidget::OnMoveAnnotationCenter()
{
    CurrentAnnotation->SetPosition(viewpoint->center()[0], viewpoint->center()[1],
                                   viewpoint->center()[2]);
    Modify(true);
    _modified = true;
    ReDraw();
}

void MIGLWidget::OnUpdateMoveAnnotationCenter(QAction *action)
{
    action->setEnabled(CurrentAnnotation != NULL && MIBusyManager::instance()->Busy() == false);
}

bool MIGLWidget::LoadMap(const char *buf, const char *pathname, const std::string &col_names)
{
    int nmap;
    char file[1024];

    //char pathname [1024];
    if (strncasecmp(buf, "loadmap", 7) == 0)
    {
        EMap *map = new EMap;
        //map->SetCrystal(Application::instance()->Crystal);
        if (sscanf(buf, "%*s%d %[^\r\n]", &nmap, file) != 2)
        {
            return false;
        }

        // check if file valid
        if (!QFileInfo(file).exists())
        {
            std::string path, name, ext;
            std::string origPath = file;
            std::string strPath(pathname);
            SplitPath(origPath, &path, &name, &ext);
            QFileInfo newFile(QFileInfo(strPath.c_str()).path(), QString((ext.size() ? name + '.' + ext : name).c_str()));
            sprintf(file, "%s", newFile.absoluteFilePath().toAscii().constData());
        }
        if (checkmappath(file))
        {
            map->SetMapNumber(nmap);
            //if(pathname) map->ReadCrystal(pathname);
            if (nmap-1 >= 0 && static_cast<size_t>(nmap-1) < inputMapHeaders.size())
            {
                *map->mapheader = inputMapHeaders[nmap-1];
            }
            if (strncasecmp(buf, "loadmapphase", 12) == 0)
            {

                if (col_names.size())
                {
                    map->InterpretColumnLabelString(col_names.c_str());
                }
                if (map->LoadMapPhaseFile(file))
                {
                    GetDisplaylist()->AddMap(map);
                    if (pathname)
                    {
                        map->Read(pathname);
                        if (map->FFTCalc())
                        {
                            doMapContour(map);
                        }
                    }
                    else
                    {
                        map->FFTMap();
                        doMapContour(map);
                    }
                }
                else
                {
                    delete map;
                    return false;
                }
            }
            else
            {
                if (map->LoadMapFile(file))
                {
                    map->SetMapNumber(nmap);
                    GetDisplaylist()->AddMap(map);
                    map->Read(pathname);
                    doMapContour(map);
                }
                else
                {
                    delete map;
                    return false;
                }
            }
        }
        else
        {
            delete map;
            return false;
        }
    }
    else
    {
        return false;
    }
    return true;
}


void MIGLWidget::Modify(bool modify)
{
    _modified = modify;
    setWindowModified(_modified);
    Displaylist *displaylist = GetDisplaylist();
    if (displaylist != NULL)
    {
        displaylist->SetModified(modify);
    }
}

bool MIGLWidget::IsModified()
{
    bool models_changed = false;
    Displaylist *displaylist = GetDisplaylist();
    if (displaylist != NULL)
    {
        models_changed = displaylist->IsModified();
    }
    return models_changed || _modified;
}

bool MIGLWidget::Close()
{
    if (MIBusyManager::instance()->Busy())
    {
        int answer = QMessageBox::question(this,
            "Force Background Operation to Abort?",
            "You have background processes running - quitting now may cause a fault\n"
            "(although it will still quit!).To end gracefully, choose Cancel and then\n"
            "use the Stop button on the toolbar to abort the operation or wait for it to end.\n"
            "\n"
            "Yes to force the background process to abort\n"
            "No to continue and perhaps crash, but end the program.\n"
            "Cancel to go back to what you were doing.",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (answer == QMessageBox::Yes)
        {
            MIBusyManager::instance()->ForceAbort();
            return false;
        }
        else if (answer == QMessageBox::Cancel)
        {
            return false;
        }
    }
    if (OnSaveModified())
    {
        //delete display list, (and therefore molecules) and prevent them
        // from forwarding signals to dead or dying windows
        delete Models;
        Models = 0;
        return true;
    }
    return false;
}

void MIGLWidget::OnMapAddFree()
{
    EMap *emap = GetDisplaylist()->GetCurrentMap();
    if (!emap)
    {
        return;
    }
    if (!emap->HasPhases())
    {
        return;
    }
    if (emap->HasFreeRFlag())
    {
        std::string s = ::format("%s\nThis reflections set already has an R-Free flag\n"
                                 "Are you sure you want to continue?"
                                 "Doing so could invalidate your R-Free calculations", emap->MapID().c_str());
        if (QMessageBox::question(this, "R-Free Flag Already Set", s.c_str(), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        {
            return;
        }
    }
    static unsigned int f = 5;
    bool ok;
    f = static_cast<unsigned int>(QInputDialog::getInt(this, "Add R-Free Flag Options", "Enter percentage (1-100):", f, 1, 100, 1, &ok));
    if (!ok)
    {
        return;
    }
    //FIXME: enable when use shells is enabled in emap
    static bool use_shells = false;
    //use_shells = data["use_shells"].b;
    long nadded = emap->AddFreeRFlag(f, use_shells);
    Logger::log("Added %ld R-Free flags", nadded);
}

bool MIGLWidget::LoadScript(const char *path)
{
    return OnOpenDocument(std::string(path));
}




bool MIGLWidget::SaveAs()
{
    std::string tmp = MIFileSelector("Save as",
                                     ".",
                                     GetFilename(),
                                     "mlw",
                                     "Model Fitting Session (*.mlw)|*.mlw|All files (*.*)|*.*",
                                     MI_SAVE_MODE,
                                     0);

    if (tmp.size() == 0)
    {
        return false;
    }

    std::string fileName(tmp.c_str());
    std::string path, name, ext;
    SplitPath(fileName, &path, &name, &ext);

    if (ext.empty())
    {
        fileName += std::string(".mlw");
    }

    SetFilename(fileName);
    SetTitle(fileName);


    // Files that were not saved correctly are not added to the FileHistory.
    if (!OnSaveDocument(_filename))
    {
        return false;
    }

    // this will add it to the history
    MIMainWindow::instance()->setCurrentFile(fileName);
    return true;
}

void MIGLWidget::SetCursor(unsigned int id)
{
    MIMainWindow::instance()->SetCursor(id, this);
}

void MIGLWidget::SetTitle(const std::string &title)
{
    static unsigned int unknownCnt = 0;
    if (_title.size() && strncmp("Untitled", _title.c_str(), 8)!=0) // already set, don't change
        return;

    if (!title.size())
    {
        _title = ::format("Untitled %d[*]", ++unknownCnt);
        return;
    }

    QFileInfo fi(title.c_str());
    if (fi.exists())
    {
        _title = fi.baseName().toStdString() + ".mlw[*]";
    }
    else
    {
        _title = title + "[*]";
    }
    setWindowTitle(_title.c_str());
}


// do extension-based recognition and farm out file opens to handlers
void MIGLWidget::OpenAnyFile(const std::string &fname)
{
    const char *arg = fname.c_str();
    const char *ext = file_extension(fname.c_str());

    if (strcmp(ext, ".mlw") == 0)
    {
        if (OnOpenDocument(arg))
            SetTitle(fname);
        return;
    }

    if (strcmp(ext, ".pdb") == 0)
    {
        if (LoadPDBFile(arg))
        {
            if (_filename.empty())
            {
                _filename = fname;
                _filename.erase(_filename.size() - 4); // remove 4 chars in ".pdb"
                _filename += ".mlw";
                SetTitle(_filename);
            }
        }
        return;
    }

    if ((strcmp(ext, ".mtz") == 0)
        || (strcmp(ext, ".phs") == 0)
        || (strcmp(ext, ".fcf") == 0)
        || // we no longer support these extensions b/c their readers are broken
           // || (strcmp(ext, ".sca") == 0)
           // || (strcmp(ext, ".ref") == 0)
        (strcmp(ext, ".cif") == 0))
    {
        mapLoadfromphsfile(arg);
        return;
    }

    if (strcmp(ext, ".map") == 0)
    {
        mapLoadfromfile(arg);
        return;
    }

    MIMainWindowLog(::format("Unrecognized file extension %s loading file %s\n", ext, arg));
}

void MIGLWidget::updatePopupMenu()
{
    bool notBusy = !MIBusyManager::instance()->Busy();
    bool notEmptyStack = !AtomStack->empty();
    bool isFitting = IsFitting();
    bool isRefining = MIFitGeomRefiner()->IsRefining();

    fitResidueAction->setEnabled(notEmptyStack && notBusy);
    fitResidueAction->setChecked(isFitting && SelectType == SINGLERESIDUE);

    rotateAction->setEnabled(isFitting && notBusy);
    rotateAction->setChecked(RightMouseMode == ROTATE);
    translateAction->setEnabled(isFitting && notBusy);
    translateAction->setChecked(RightMouseMode == TRANSLATE);
    setTorsionAction->setEnabled(isFitting && notBusy && AtomStack->size() >= 2);
    acceptFitAction->setEnabled(isFitting && notBusy);
    cancelFitAction->setEnabled(isFitting && notBusy);

    replaceAndFitAction->setEnabled(isFitting && notBusy && notEmptyStack);
    deleteResidueAction->setEnabled(notEmptyStack && notBusy);
    deleteAtomAction->setEnabled(notEmptyStack);

    refineRegionAction->setEnabled(notEmptyStack && !isRefining && notBusy);
    acceptRefineAction->setEnabled(isRefining && notBusy);
    cancelRefineAction->setEnabled(isRefining && notBusy);
    saveAction->setEnabled(true);
    fullscreenAction->setEnabled(true);
    fullscreenAction->setChecked(IsFullScreen);
    clearLabelsAction->setEnabled(true);
}

void MIGLWidget::updateDotSurfMenu()
{
    bool notEmptyStack = !AtomStack->empty();

    dotSurfVdwAction->setEnabled(true);
    dotSurfSolventExposedAction->setEnabled(GetDisplaylist()->CurrentItem() != NULL);
    dotSurfAtomSphereAction->setEnabled(notEmptyStack);
    dotSurfResidueAction->setEnabled(notEmptyStack);
    dotSurfResiduesAction->setEnabled(notEmptyStack);
    dotSurfAtomAction->setEnabled(notEmptyStack);
    dotSurfAtomsAction->setEnabled(notEmptyStack);
    dotSurfClearAction->setEnabled(true);
}

void MIGLWidget::solidSurfaceActionTriggered(QAction *action)
{
    std::vector<Molecule*> mols;
    std::vector<unsigned int> selected;
    solidSurfaceCommand(action->data().toInt(), mols, selected);
}

QMenu*MIGLWidget::newSolidsurfMenu(bool include_selection_items)
{
    QMenu *menu = new QMenu;

    solidsurfActionGroup = new QActionGroup(this);
    solidsurfActionGroup->setExclusive(false);
    connect(solidsurfActionGroup, SIGNAL(triggered(QAction*)), this, SLOT(solidSurfaceActionTriggered(QAction*)));

    solidsurfCommonActionGroup = new QActionGroup(this);
    solidsurfCommonActionGroup->setExclusive(false);
    connect(solidsurfCommonActionGroup, SIGNAL(triggered(QAction*)), this, SLOT(solidSurfaceActionTriggered(QAction*)));

    QActionGroup *actionGroup;
    actionGroup = solidsurfActionGroup;

    solidsurf_menu_action(menu, actionGroup, ID_SOLIDSURFACE_BUILD, "Build surface");
    solidsurf_menu_action(menu, actionGroup, ID_SOLIDSURFACE_COLOR, "Color surface");
    solidsurf_menu_action(menu, actionGroup, ID_SOLIDSURFACE_COLOR_BY_ATOM, "Color surface by atom type");

    solidsurf_menu_action(menu, solidsurfCommonActionGroup, ID_SOLIDSURFACE_CLEAR, "Clear Surface");
    QAction *sep = menu->addSeparator();
    solidsurfCommonActionGroup->addAction(sep);

    if (include_selection_items)
    {
        actionGroup = solidsurfActionGroup;
        solidsurf_menu_action(menu, actionGroup, ID_SOLIDSURFACE_ATOMS_MODE,    "Atom selection")
        ->setCheckable(true);
        solidsurf_menu_action(menu, actionGroup, ID_SOLIDSURFACE_RESIDUE_MODE,  "Single residue selection")
        ->setCheckable(true);
        solidsurf_menu_action(menu, actionGroup, ID_SOLIDSURFACE_RESIDUES_MODE, "Multiple residue selection")
        ->setCheckable(true);
        solidsurf_menu_action(menu, actionGroup, ID_SOLIDSURFACE_PEPTIDE_MODE,  "Peptide selection")
        ->setCheckable(true);
        solidsurf_menu_action(menu, actionGroup, ID_SOLIDSURFACE_MOLECULE_MODE, "Molecule selection")
        ->setCheckable(true);
        sep = menu->addSeparator();
        actionGroup->addAction(sep);
    }

    actionGroup = solidsurfCommonActionGroup;

    solidsurf_menu_action(menu, actionGroup, ID_SOLIDSURFACE_MOLECULAR, "Molecular surface mode")
    ->setCheckable(true);
    solidsurf_menu_action(menu, actionGroup, ID_SOLIDSURFACE_ACCESSIBLE, "Accessible surface mode")
    ->setCheckable(true);
    sep = menu->addSeparator();
    actionGroup->addAction(sep);

    solidsurf_menu_action(menu, actionGroup, ID_SOLIDSURFACE_RESMOOTH, "Set smooth level");
    solidsurf_menu_action(menu, actionGroup, ID_SOLIDSURFACE_ALPHA, "Set transparency level");
    sep = menu->addSeparator();
    actionGroup->addAction(sep);

    solidsurf_menu_action(menu, actionGroup, ID_SOLIDSURFACE_GRADCOLOR, "Color with gradient")
    ->setCheckable(true);
    solidsurf_menu_action(menu, actionGroup, ID_SOLIDSURFACE_MINCOLOR, "Set min gradient color");
    solidsurf_menu_action(menu, actionGroup, ID_SOLIDSURFACE_MAXCOLOR, "Set max gradient color");
    solidsurf_menu_action(menu, actionGroup, ID_SOLIDSURFACE_MINVAL, "Set min gradient value");
    solidsurf_menu_action(menu, actionGroup, ID_SOLIDSURFACE_MAXVAL, "Set max gradient value");
    solidsurf_menu_action(menu, solidsurfActionGroup, ID_SOLIDSURFACE_CALCDIST, "Calculate distance");
    sep = menu->addSeparator();
    actionGroup->addAction(sep);

    solidsurf_menu_action(menu, actionGroup, ID_SOLIDSURFACE_QUALITY_0,    "Standard quality")
    ->setCheckable(true);
    solidsurf_menu_action(menu, actionGroup, ID_SOLIDSURFACE_QUALITY_1,    "High quality")
    ->setCheckable(true);
    solidsurf_menu_action(menu, actionGroup, ID_SOLIDSURFACE_QUALITY_2,    "Ultra quality")
    ->setCheckable(true);
    // leave this here, it's needed when this menu is dynamically added to
    sep = menu->addSeparator();
    actionGroup->addAction(sep);

    for (unsigned int i = 0; i < 10; ++i)
    {
        char buf[64];
        sprintf(buf, "Use surface %d", i+1);
        unsigned int id = ID_SOLIDSURFACE_USESURF_0 + i;
        QAction *a = solidsurf_menu_action(menu, actionGroup, id, buf);
        a->setCheckable(true);
        a->setChecked(i == solidsurf_current_surface);
        solidsurfActions[i] = a;
    }
    updateSolidsurfMenu();
    return menu;
}

void MIGLWidget::updateSolidsurfMenu()
{

    MISurfaceSetCurrentView(this);

    unsigned int scnt = MISurfaceCount();
    for (unsigned int i = 0; i < 10; ++i)
    {
        QAction *act = solidsurfActions[i];
        act->setVisible(i < scnt);
        act->setChecked(i == solidsurf_current_surface);
    }

    QList<QAction*> actions(solidsurfActionGroup->actions());
    actions += solidsurfCommonActionGroup->actions();
    foreach (QAction* action, actions)
    {
        switch (action->data().toInt())
        {
        case ID_SOLIDSURFACE_QUALITY_0:
            action->setChecked(MIGetSurfaceQuality() == 0);
            break;
        case ID_SOLIDSURFACE_QUALITY_1:
            action->setChecked(MIGetSurfaceQuality() == 1);
            break;
        case ID_SOLIDSURFACE_QUALITY_2:
            action->setChecked(MIGetSurfaceQuality() == 2);
            break;

        case ID_SOLIDSURFACE_MOLECULAR:
            action->setChecked(MIGetSurfaceType() == MISurfaceType::Molecular);
            break;
        case ID_SOLIDSURFACE_ACCESSIBLE:
            action->setChecked(MIGetSurfaceType() == MISurfaceType::Accessible);
            break;
        case ID_SOLIDSURFACE_CLEAR:
            action->setEnabled(MISurfaceCount() != 0);
            break;

        case ID_SOLIDSURFACE_ATOMS_MODE:
            action->setChecked(MIGetSurfaceSelectionMode() == MISurfaceSelectionMode::AtomsOnly);
            break;
        case ID_SOLIDSURFACE_RESIDUE_MODE:
            action->setChecked(MIGetSurfaceSelectionMode() == MISurfaceSelectionMode::SingleResidue);
            break;
        case ID_SOLIDSURFACE_RESIDUES_MODE:
            action->setChecked(MIGetSurfaceSelectionMode() == MISurfaceSelectionMode::Residues);
            break;
        case ID_SOLIDSURFACE_PEPTIDE_MODE:
            action->setChecked(MIGetSurfaceSelectionMode() == MISurfaceSelectionMode::Peptide);
            break;
        case ID_SOLIDSURFACE_MOLECULE_MODE:
            action->setChecked(MIGetSurfaceSelectionMode() == MISurfaceSelectionMode::EntireMolecule);
            break;

        case ID_SOLIDSURFACE_COLOR:
        case ID_SOLIDSURFACE_COLOR_BY_ATOM:
            action->setEnabled(MISurfaceCount() != 0 && !AtomStack->empty());
            break;

        case ID_SOLIDSURFACE_BUILD:
        case ID_SOLIDSURFACE_CALCDIST:
            action->setEnabled(!AtomStack->empty());
            break;

        case ID_SOLIDSURFACE_GRADCOLOR:
            action->setChecked(MIGetSurfaceUseGradColor());
            break;

        default:
            break;
        }
    }
}

QAction*MIGLWidget::solidSurfMenuAction() const
{
    return solidsurf_popup_menu->menuAction();
}

ViewController*ViewController::instance()
{
    static ViewController theInstance;
    return &theInstance;
}

void MIGLWidget::StackExpandTop2Range()
{
    MIAtom *a1, *a2, *a;
    Molecule *m1, *m2;
    Residue *r1, *r2;
    AtomStack->Pop(a1, r1, m1);
    AtomStack->Pop(a2, r2, m2);
    if (m1 != m2)
    {
        Logger::message("Both atoms must be in same molecule");
        AtomStack->Push(a2, r2, m2);
        AtomStack->Push(a1, r1, m1);
        return;
    }

    if ((a1 && a1->type() & AtomType::SYMMATOM)
        || (a2 && a2->type() & AtomType::SYMMATOM))
    {
        Logger::message("Can't expand symmetry atom(s) on stack");
        AtomStack->Push(a2, r2, m2);
        AtomStack->Push(a1, r1, m1);
        return;
    }

    if (r1 == r2)
    {
        if ((a = atom_from_name("CA", *r2)) == NULL)
        {
            AtomStack->Push(a, r2, m2);
        }
        else
        {
            AtomStack->Push(r2->atom(0), r2, m2);
        }
        return;
    }

    ResidueListIterator res;
    std::vector<ResidueListIterator> marks;
    for (res = m1->residuesBegin(); res != m1->residuesEnd(); ++res)
    {
        if (res == ResidueListIterator(r1) || res == ResidueListIterator(r2))
            marks.push_back(res);
        if (marks.size() == 2)
            break;
    }
    if (marks.size() != 2)
    {
        Logger::message("Error expanding stack: residue not found in model");
        AtomStack->Push(a2, r2, m2);
        AtomStack->Push(a1, r1, m1);
        return;
    }

    ++marks[1];
    for (res = marks[0]; res != marks[1]; ++res)
    {
        if ((a = atom_from_name("CA", *res)) != NULL)
        {
            AtomStack->Push(a, res, m2);
        }
        else
        {
            AtomStack->Push(res->atom(0), res, m2);
        }
    }
}

void MIGLWidget::setMessage(const QString &message)
{
    rootContext()->setContextProperty("messageVisible", !message.isEmpty());
    if (!message.isEmpty())
        rootContext()->setContextProperty("message", message);
}

const mi::opengl::Camera *MIGLWidget::getCamera() const
{
    return camera;
}
