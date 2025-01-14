/******************************************************************************
* License version                                                             *
*                                                                             *
* Authors:                                                                    *
* Contact information:                                                        *
******************************************************************************/
#pragma once

#include <SofaHapticAvatar/HapticAvatar_BaseDeviceController.h>

namespace sofa::HapticAvatar
{

using namespace sofa::defaulttype;

/**
* Haptic Avatar driver
*/
class SOFA_HAPTICAVATAR_API HapticAvatar_ArticulatedDeviceController : public HapticAvatar_BaseDeviceController
{
public:
    SOFA_CLASS(HapticAvatar_ArticulatedDeviceController, HapticAvatar_BaseDeviceController);
    typedef Vec1Types::Coord Coord;
    typedef Vec1Types::VecCoord VecCoord;
    typedef Vec1Types::VecDeriv VecDeriv;
    typedef sofa::component::controller::LCPForceFeedback<sofa::defaulttype::Vec1dTypes> LCPForceFeedback;

    /// Default constructor
    HapticAvatar_ArticulatedDeviceController();

public:
    /// output data position of the tool
    Data<VecCoord> d_toolPosition;

    /// Pointer to the ForceFeedback component
    LCPForceFeedback::SPtr m_forceFeedback;

};

} // namespace sofa::HapticAvatar
