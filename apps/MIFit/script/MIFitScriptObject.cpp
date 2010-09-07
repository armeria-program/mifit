#include "MIFitScriptObject.h"
#include <core/Version.h>
#include <jobs/BatchJob.h>
#include <jobs/BatchJobManager.h>
#include <QFileInfo>
#include <QScriptEngine>
#include <ui/uilib.h>

MIFitScriptObject::MIFitScriptObject(QScriptEngine *engine, QObject *parent)
    : QObject(parent),
      engine(engine),
      jobMenu(NULL)
{
}

void MIFitScriptObject::setJobMenu(QMenu *jobMenu)
{
    this->jobMenu = jobMenu;
}

QString MIFitScriptObject::version()
{
    return MIFit_version;
}

QString MIFitScriptObject::directory()
{
    return Application::instance()->GetMolimageHome().c_str();
}

bool MIFitScriptObject::writeCurrentModel(const QString &file)
{
    if (file == "test.pdb")
        engine->currentContext()->throwError("invalid file");

    MIGLWidget *doc = MIMainWindow::instance()->currentMIGLWidget();
    if (!doc)
    {
        engine->currentContext()->throwError("no current document");
        return false;
    }
    Molecule *model = doc->GetDisplaylist()->GetCurrentModel();
    if (!model)
    {
        engine->currentContext()->throwError("no current model");
        return false;
    }
    QFileInfo fileInfo(file);
    return model->SavePDBFile(fileInfo.absoluteFilePath().toAscii().constData());
}

QStringList MIFitScriptObject::dictionaryResidueList()
{
    QStringList result;
    std::vector<std::string> choices = MIFitDictionary()->GetDictResList();
    for (size_t i = 0; i < choices.size(); ++i)
    {
        result << choices[i].c_str();
    }
    return result;
}

QStringList MIFitScriptObject::spacegroupList()
{
    std::vector<std::string> sg;
    MIGetSpacegroups(sg);
    QStringList result;
    for (size_t i = 0; i < sg.size(); ++i)
    {
        result << sg[i].c_str();
    }
    return result;
}

void MIFitScriptObject::addJob(const QString &menuName, const QString &jobName, const QString &executable, const QStringList &arguments, const QString &workingDirectory)
{
    MIMainWindow::instance()->addJob(menuName, jobName, executable, arguments, workingDirectory);
}

void MIFitScriptObject::setJobWorkDir(const QString &jobId, const QString &workDir)
{
    bool ok;
    unsigned long jobIdNum = jobId.toULong(&ok);
    if (ok)
    {
        std::vector<BatchJob*> &jobs = *MIMainWindow::instance()->GetJobManager()->GetJobList();
        foreach (BatchJob *job, jobs)
        {
            if (jobIdNum == job->jobId())
            {
                job->setWorkingDirectory(workDir);
                break;
            }
        }
    }

}
