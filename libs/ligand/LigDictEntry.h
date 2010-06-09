#ifndef mifit_ligand_LigDictEntry_h
#define mifit_ligand_LigDictEntry_h

#include <vector>
#include <chemlib/chemlib.h>

class LigDictEntry
{
public:
    LigDictEntry(chemlib::Residue *r); // note takes ownership of r!

    ~LigDictEntry();

    chemlib::Residue *res;
    std::vector<chemlib::Bond> bonds;
    std::vector<chemlib::ANGLE> angles;
    std::vector<chemlib::TORSDICT> torsions;
    std::vector<chemlib::PLANEDICT> planes;
    std::vector<chemlib::CHIRALDICT> chirals;

};
#endif // ifndef mifit_ligand_LigDictEntry_h
