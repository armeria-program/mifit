#include <opengl/Axes.h>

#include <math/Matrix4.h>
#include <math/Quaternion.h>
#include <opengl/OpenGL.h>
#include <opengl/Text.h>
#include <QtCore/QString>

using namespace mi::math;

namespace mi
{
namespace opengl
{

int Axes::labelsFontSize = 16;

Axes::Axes(std::auto_ptr<Text> font)
    : labelsFont(font)
{

}

void Axes::setFontSize(int fontSize)
{
    this->fontSize = fontSize;
}

void Axes::setLength(int length)
{
    this->length = length;
}

void Axes::setGlUnitsPerPixel(float glUnitsPerPixel)
{
    this->glUnitsPerPixel = glUnitsPerPixel;
}

static void getInverseRotation(float inverseRotation[16])
{
    static GLfloat modelview[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
    Matrix4<float> matrix(modelview);
    Quaternion<float> quat;
    quat.set(matrix);
    quat.negate();
    matrix.set(quat);
    matrix.getInColumnMajorOrder(inverseRotation);
}

void Axes::drawText(const char *text, float x, float y, float z, float offset, float scale, float inverseRotation[16])
{

    glPushMatrix();
    // Position at coordintes.
    glTranslatef(x, y, z);

    // Keep oriented toward camera.
    glMultMatrixf(inverseRotation);

    // Offset position.
    glTranslatef(offset, offset, offset);
    // Scale size.
    glScalef(scale, scale, 1.0f);

    glColor3f(1.0f, 1.0f, 1.0f);
    labelsFont->renderText(0, 0, QString(text));

    glPopMatrix();
}

void Axes::render()
{
    double textHeight = (double) fontSize * glUnitsPerPixel;
    float labelsTextScale = (float) (14.0f / (float) labelsFontSize * 0.1f * textHeight);
    float renderedLength = length * glUnitsPerPixel;

    glPushAttrib(GL_LINE_BIT);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(3.0f);
    glBegin(GL_LINES);
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(renderedLength, 0.0f, 0.0f);
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, renderedLength, 0.0f);
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, renderedLength);
    glEnd();
    glPopAttrib();

    float inverseRotation[16];
    getInverseRotation(inverseRotation);
    drawText(" X ", renderedLength, 0.0f, 0.0f, 0.0f, labelsTextScale, inverseRotation);
    drawText(" Y ", 0.0f, renderedLength, 0.0f, 0.0f, labelsTextScale, inverseRotation);
    drawText(" Z ", 0.0f, 0.0f, renderedLength, 0.0f, labelsTextScale, inverseRotation);
}

}
}
