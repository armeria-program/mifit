#ifndef __LSQFitDialog_H__
#define __LSQFitDialog_H__

#include <vector>

#include <chemlib/chemlib.h>
#include "core/corelib.h"

class Molecule;
class Displaylist;

#include "ui_LSQFitDialog.h"

//@{
// Struct for cantaining a match between two chains.
//  Used by LSQFit dialog.
//@}
class MATCH
{
public:
    chemlib::Residue *target;
    chemlib::Residue *source;
    int length;
    std::string atomtype;
};

//@{
// Dialog box for least-squares overlapping of two chains.
// Allows for multiple segementsw to be matched.
// Segments can either be in the same molecule (For example
// A and B chains, or in two different realated molecules.
//  Someday we should make this able to automatically find the overlaps
// say with A GA optimization of a distance matrix...
//@}
class LSQFitDialog : public QDialog, public Ui::LSQFitDialog
{
    Q_OBJECT

public:
    struct Data
    {
        std::string sourceModel;
        std::string targetModel;
        bool applyToChain;
        std::string chainId;

        float r00;
        float r01;
        float r02;

        float r10;
        float r11;
        float r12;

        float r20;
        float r21;
        float r22;

        float v0;
        float v1;
        float v2;

    };

    LSQFitDialog(QWidget *parent);

    void InitializeFromData();
    Data GetData();

    void SetAtomType(const char*);
    void ListTargetChoices();
    void ListSourceChoices();
    Displaylist *displaylist;
    Molecule *m_source;
    Molecule *m_target;
    bool success;
    double r[3][3];
    double v[3];
    double rms;
    void ListTarget();
    void ListSource();
    void ListMatches();
    bool MatchOK(MATCH*);
    chemlib::Residue *targetres, *sourceres;

    std::vector<MATCH> Matches;

private slots:
    void on_targetComboBox_currentIndexChanged(const QString&);
    void on_sourceComboBox_currentIndexChanged(const QString&);
    void on_sourceListWidget_currentTextChanged(const QString &str);
    void on_targetListWidget_currentTextChanged(const QString &str);
    void on_calcButton_clicked();
    void on_removeButton_clicked();
    void on_addButton_clicked();
    void on_exportButton_clicked();
    void on_importButton_clicked();

private:
    void updateButtons();
    void updateMatrix();
    void setSuccess(bool value);
    Molecule *findMolecule(const std::string&);
};

#endif // ifndef __LSQFitDialog_H__
