/******************************************************************************
* License version                                                             *
*                                                                             *
* Authors:                                                                    *
* Contact information:                                                        *
******************************************************************************/

#include <SofaHapticAvatar/HapticAvatar_IBoxController.h>

#include <sofa/core/ObjectFactory.h>


namespace sofa::HapticAvatar
{

using namespace HapticAvatar;

int HapticAvatar_IBoxControllerClass = core::RegisterObject("Driver allowing interfacing with Haptic Avatar device.")
    .add< HapticAvatar_IBoxController >()
    ;


//constructeur
HapticAvatar_IBoxController::HapticAvatar_IBoxController()
    : d_portName(initData(&d_portName, std::string("//./COM5"), "portName", "position of the base of the part of the device"))
    , d_hapticIdentity(initData(&d_hapticIdentity, "hapticIdentity", "position of the base of the part of the device"))
    , m_HA_driver(nullptr)
    , m_deviceReady(false)    
{
    this->f_listening.setValue(true);
    
    
}


HapticAvatar_IBoxController::~HapticAvatar_IBoxController()
{
    clearDevice();
    if (m_HA_driver)
    {
        delete m_HA_driver;
        m_HA_driver = nullptr;
    }
}


//executed once at the start of Sofa, initialization of all variables excepts haptics-related ones
void HapticAvatar_IBoxController::init()
{
    msg_info() << "HapticAvatar_IBoxController::init()";
    m_HA_driver = new HapticAvatar_DriverIbox(d_portName.getValue());

    if (!m_HA_driver->IsConnected()) {
        msg_error() << "HapticAvatar_IBoxController driver creation failed";
        return;
    }

    // get identity
    std::string identity = m_HA_driver->getDeviceType();
    d_hapticIdentity.setValue(identity);
    std::cout << "HapticAvatar_IBoxController identity: '" << identity << "'" << std::endl;

    for (int i = 0; i < IBOX_NUM_CHANNELS; i++) {
        setLoopGain(i, 2.5f, 0);
    }

    return;
}

float HapticAvatar_IBoxController::getJawOpeningAngle(int toolId)
{
    return m_HA_driver->getOpeningValue(toolId);
}


void HapticAvatar_IBoxController::setHandleForce(int toolId, float force)
{
    return m_HA_driver->setForce(toolId, force);
}

void HapticAvatar_IBoxController::setLoopGain(int chan, float loopGainP, float loopGainD)
{
    m_HA_driver->setLoopGain(chan, loopGainP, loopGainD);
}

void HapticAvatar_IBoxController::update()
{
    m_HA_driver->update();
}

void HapticAvatar_IBoxController::clearDevice()
{
    msg_info() << "HapticAvatar_IBoxController::clearDevice()";
   
}


} // namespace sofa::HapticAvatar
