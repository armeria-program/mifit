#ifndef MI_DIALOG_H
#define MI_DIALOG_H

#include "core/corelib.h"
#include <climits>
#include <vector>
#include <string>

class QWidget;
class QDialog;

class MIDialog {
  public:
  MIDialog(QWidget *parent, const std::string &name);
  virtual ~MIDialog();

  virtual bool GetResults(MIData& data);
  virtual bool RandomResults(MIData& data);

  static void SetRandom(bool state);

 protected:
    QWidget *_qparent;
    std::string _name;
    static bool IsRandom;

private:
  virtual bool PromptForResults(MIData& data) = 0;
};


class MIGenericDialog : public MIDialog {
public:
  MIGenericDialog(QWidget* parent, const std::string& name);
  ~MIGenericDialog();

  void order(const std::string& key);
  void label(const std::string& key, const std::string& label);

private:
  bool PromptForResults(MIData& data);
   QDialog *_qdlg;
};

//NOTE: wraps MIGenericDialog with a radio data item with labels for the choices
int MIGetSingleChoiceIndex(const std::string& message,
                           const std::string& caption,
                           const std::vector<std::string>& choices,
                           QWidget *parent=0);


class MIMessageDialog : public MIDialog {
  public:
    MIMessageDialog(QWidget* parent,
                    const std::string& name,
                    const std::string& messsage,
                    int style);

  private:
    void init(const std::string& messsage, int style);


    bool PromptForResults(MIData& data);
    std::string _message;
    int _style;
    //variables: val.i
};

// wrappers for MIMessageDialog
const int MI_YES = 1;
const int MI_NO = 0;
const int MI_CANCEL = -1;

#define MIDIALOG_YES_NO                0x00000001
#define MIDIALOG_CANCEL                0x00000002
#define MIDIALOG_NO_DEFAULT            0x00000004

#define MIDIALOG_ICON_WARNING          0x00000100
#define MIDIALOG_ICON_ERROR            0x00000200
//#define MIDIALOG_ICON_QUESTION       0x00000400 // unused, assumed w/ YES_NO
#define MIDIALOG_ICON_INFORMATION      0x00000800

int MIMessageBox(const std::string& message, const std::string& caption = "", int style = MIDIALOG_ICON_INFORMATION, QWidget* parent = 0);
bool MIYesNo(const std::string &message, const std::string& caption = "");



class MIGetIntegerDialog : public MIDialog {
public:
  MIGetIntegerDialog(QWidget* parent,
                     const std::string& name,
                     const std::string& messsage);

  bool GetValue(int deft, int& result, int min = INT_MIN, int max = INT_MAX);


private:
  bool PromptForResults(MIData& data);
  std::string _message;
  int _min;
  int _max;
  //variables: val.i
};


class MIGetUnsignedIntegerDialog : public MIDialog {
public:
  MIGetUnsignedIntegerDialog(QWidget* parent,
                             const std::string& name,
                             const std::string& messsage);

  bool GetValue(unsigned int deft, unsigned int& result, unsigned int min = 0, unsigned int max = UINT_MAX);


private:
  bool PromptForResults(MIData& data);
  std::string _message;
  unsigned int _min;
  unsigned int _max;
  //variables: val.u
};

class MIGetFloatDialog : public MIDialog {
public:
  MIGetFloatDialog(QWidget* parent,
                   const std::string& name,
                   const std::string& messsage);
  bool GetValue(float deft, float& result, float min = FLT_MIN, float max = FLT_MAX);

private:
  bool PromptForResults(MIData& data);
  std::string _message;
  float _min, _max;
  //variables: val.f
};

class MIGetStringDialog : public MIDialog {
public:
  MIGetStringDialog(QWidget* parent,
                    const std::string& name,
                    const std::string& messsage, bool multiline=false);
  bool GetValue(const std::string& deft, std::string& result);

private:
  bool PromptForResults(MIData& data);
  std::string _message;
  bool _multiline;
  //variables: val.str
};

class MIColorPromptDialog : public MIDialog {
public:
  MIColorPromptDialog(QWidget* parent, const std::string& name);
private:
  bool PromptForResults(MIData& data);
  //variables red.u green.u blue.u
};

// NOTE: wraps MIColorPromptDialog
PaletteColor MIGetColorFromUser(QWidget* parent, const PaletteColor& deft);

class MIColorPaletteDialog : public MIDialog {
public:
  MIColorPaletteDialog(QWidget* parent, const std::string& name);
private:
  bool PromptForResults(MIData& data);
  //variables: color.radio
};

//NOTE: wraps MIColorPaletteDialog
int MIColorChooser(int start_color, const std::string name="Choose color");

class MIFontDialog : public MIDialog {
public:
  MIFontDialog(QWidget* parent, const std::string& name);
private:
  bool PromptForResults(MIData& data);
  //variables: fontDesc.str
};

//NOTE: wrapper around MIFontDialog
bool MIGetFontFromUser(std::string &fontDesc);


class MISelectDirectoryDialog : public MIDialog {
public:
  MISelectDirectoryDialog(QWidget* parent, const std::string& name,
                          const std::string& deft = ".");
private:
  bool PromptForResults(MIData& data);
  std::string _deft;
  //variables: dir.str
};

const unsigned int MI_OPEN_MODE=0;
const unsigned int MI_MULTI_OPEN_MODE=1;
const unsigned int MI_SAVE_MODE=2;

class MIFileDialog : public MIDialog {
public:
  MIFileDialog(QWidget* parent, const std::string& message,
               const std::string& deftDir = "",
               const std::string& deftFile = "",
               const std::string& filter = "",
               unsigned int mode = MI_OPEN_MODE);
    bool RandomResults(MIData& data);

private:
  bool PromptForResults(MIData& data);
  std::string _deftDir, _deftFile, _filter;
  unsigned int _mode;
  //variables: filterIndex.i path.str pathList.strlist
};

std::string MIFileSelector(const std::string& message, const std::string& default_path = "",
                           const std::string& default_filename = "", const std::string& default_extension = "",
                           const std::string& wildcard = "*.*", unsigned int mode = MI_OPEN_MODE, QWidget* parent = NULL);



class MISelectCrystalDialog : public MIDialog {
public:
  MISelectCrystalDialog(QWidget* parent, const std::string& name);
private:
  bool PromptForResults(MIData& data);
  //variables: info.str (same format as CMapHeaderBase.Label() generates
};

class MISmilesImportDialog : public MIDialog {
public:
  MISmilesImportDialog(QWidget* parent, const std::string& name);
private:
  bool PromptForResults(MIData& data);
  //variables: mode.i filename.str code.str smiles.str dbquery.str
};

class MIContourDialog : public MIDialog {
public:
  MIContourDialog(QWidget* parent, const std::string& name);
private:
  bool PromptForResults(MIData& data);
  //variables: radiusMax.i radius.f  mapmin.f mapmax.f
  //           maplevel0.f maplevel1.f maplevel2.f maplevel3.f maplevel4.f
  //           maplevelColor0.i maplevelColor1.i maplevelColor2.i maplevelColor3.i maplevelColor4.i
  //           maplevelOn0.b maplevelOn1.b maplevelOn2.b maplevelOn3.b maplevelOn4.b
  //           contourMethod.radio blobRadius.f lineWidth.f
};


class MIRefinementOptionsDialog : public MIDialog {
public:
  MIRefinementOptionsDialog(QWidget* parent, const std::string& name);
private:
  bool PromptForResults(MIData& data);
  //variables: BondWeight.i AngleWeight.i PlaneWeight.i TorsionWeight.i
  //           BumpWeight.i MapWeight.i ConstrainCA.b ConstrainEnds.b
  //           Verbose.b RefineWhileFit.b SigmaBond.f SigmaAngle.f
  //           SigmaTorsion.f SigmaPlane.f SigmaBump.f
};


class MIPhaseFileLoadDialog : public MIDialog {
public:
  MIPhaseFileLoadDialog(QWidget* parent, const std::string& name);
private:
  bool PromptForResults(MIData& data);
  //variables:
  // required input: filename.str models.strList, cells.strList
  // output: grid.radio, resmin.f resmax.f
  //         enabled1.b, type1.str fo1.str, fc1.str, fom1.str, phi1.str
  //         enabled2.b, type2.str fo2.str, fc2.str, fom2.str, phi2.str
};


class MIBValueColorsDialog : public MIDialog {
public:
  MIBValueColorsDialog(QWidget* parent, const std::string& name);
private:
  bool PromptForResults(MIData& data);
  //variables: level1.f .. level10.i,  color1.i .. color10.i, save.b
};

class MIAtomColorsDialog : public MIDialog {
public:
  MIAtomColorsDialog(QWidget* parent, const std::string& name);
private:
  bool PromptForResults(MIData& data);
  //variables:
  //  input/output: atomNames.strList, atomColors.strList
};

class MILSQFitDialog : public MIDialog {
public:
  MILSQFitDialog(QWidget* parent, const std::string& name);
private:
  bool PromptForResults(MIData& data);
  //variables:
  //  output: sourceModel.str
  //          targetModel.str
  //          matrix.str (16 floats)
  //          applyToChain.b
  //          chainId.str
};

class MIBindNGrindDialog : public MIDialog {
public:
  MIBindNGrindDialog(QWidget* parent, const std::string& name);
  void GetInitialData(MIData &dat);

private:
  bool PromptForResults(MIData& data);
  //variables:
  //    hklin.strList, pdbin.str, process_engine.str, arpwarpmap.b,
  //    detector_constants.str, spacegroup_no.radio, reference_mtz.str,
  //    multi_search.b, libfile.str, viewpoint_method.str, bngsummary.str
  //    place_ligand.b, ligand_name.str, ligand_from_dictionary.b

};

class MIIntegrateDialog : public MIDialog {
public:
  MIIntegrateDialog(QWidget* parent, const std::string& name);
  void GetInitialData(MIData &dat);

private:
  bool PromptForResults(MIData& data);
  //variables:
  //  template_image.strList, detector_constants.str,
  //  spacegroup_no.radio, first_image.u, last_image.u,
  //  integrate_resolution.str
};

class MISadPhasingDialog : public MIDialog {
public:
  MISadPhasingDialog(QWidget* parent, const std::string& name);
  void GetInitialData(MIData &dat);

private:
  bool PromptForResults(MIData& data);
  //variables:
  //  template_image.strList, detector_constants.str,
  //  spacegroup_no.radio, first_image.u, last_image.u,
  //  integrate_resolution.str
};


class MINCSModelingDialog : public MIDialog {
public:
  MINCSModelingDialog(QWidget* parent, const std::string& name);
  void GetInitialData(MIData &dat);

private:
  bool PromptForResults(MIData& data);
};

class MICocrystalSuperpositionDialog : public MIDialog {
public:
  MICocrystalSuperpositionDialog(QWidget* parent, const std::string& name);
  void GetInitialData(MIData &dat);

private:
  bool PromptForResults(MIData& data);
};

class MIMolRepDialog : public MIDialog {
public:
  MIMolRepDialog(QWidget* parent, const std::string& name);
  void GetInitialData(MIData &dat);

private:
  bool PromptForResults(MIData& data);
};

class MICustomJobDialog : public MIDialog {
public:
  MICustomJobDialog(QWidget* parent, const std::string& name);
  void GetInitialData(MIData &dat);

private:
  bool PromptForResults(MIData& data);
};


class MIRefinementDialog : public MIDialog {
public:
  MIRefinementDialog(QWidget* parent, const std::string& name);
  void GetInitialData(MIData &dat);

private:
  bool PromptForResults(MIData& data);
};

class MIJobReportDialog : public MIDialog {
public:
  MIJobReportDialog(QWidget* parent, const std::string& name);
  void GetInitialData(MIData &dat);

private:
  bool PromptForResults(MIData& data);
};


#endif
