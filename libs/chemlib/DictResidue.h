#ifndef mifit_model_DictResidue_h
#define mifit_model_DictResidue_h

#include <vector>

#include "model.h"

namespace chemlib
{
    class Bond;
    class ANGLE;

/**
 * A single residue in the geometry dictionary.
 */
    class DictResidue
    {
    private:
        Residue *_residue; // reference only, not owned by this class
        std::vector<Bond> bonds;
        std::vector<ANGLE> angles;

    public:
        DictResidue(Residue *r); // a reference to the input residue is kept, it is not copied
        ~DictResidue();

        bool Build();
        void BuildAngles();
        void BuildBonds();
        Residue *residue();
        const Residue *residue() const;
        std::vector<Bond> *Bonds();
        std::vector<ANGLE> *Angles();
    };

}

#endif // ifndef mifit_model_DictResidue_h
