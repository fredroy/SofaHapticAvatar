/******************************************************************************
* License version                                                             *
*                                                                             *
* Authors:                                                                    *
* Contact information:                                                        *
******************************************************************************/

#include <SofaHapticAvatar/HapticAvatar_GrasperDeviceController.h>

#include <sofa/core/ObjectFactory.h>

#include <sofa/simulation/Node.h>
#include <sofa/simulation/AnimateBeginEvent.h>
#include <sofa/simulation/AnimateEndEvent.h>
#include <sofa/simulation/CollisionEndEvent.h>
#include <sofa/core/collision/DetectionOutput.h>

#include <sofa/core/visual/VisualParams.h>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <fstream>

namespace sofa::HapticAvatar
{

using namespace sofa::helper::system::thread;

int HapticAvatar_GrasperDeviceControllerClass = core::RegisterObject("Driver allowing interfacing with Haptic Avatar device.")
    .add< HapticAvatar_GrasperDeviceController >()
    ;


//constructeur
HapticAvatar_GrasperDeviceController::HapticAvatar_GrasperDeviceController()
    : HapticAvatar_ArticulatedDeviceController()
    , d_MaxOpeningAngle(initData(&d_MaxOpeningAngle, SReal(60.0f), "MaxOpeningAngle", "Max jaws opening angle"))
    , l_iboxCtrl(initLink("iboxController", "link to IBoxController"))
    , m_iboxCtrl(nullptr)
{
    this->f_listening.setValue(true);
    
    d_hapticIdentity.setReadOnly(true);

    m_toolRot.identity();

    VecCoord & articulations = *d_toolPosition.beginEdit();
    articulations.resize(6);
    articulations[0] = 0;
    articulations[1] = 0;
    articulations[2] = 0;
    articulations[3] = 0;

    articulations[4] = 0;
    articulations[5] = 0;
    d_toolPosition.endEdit();
}



//executed once at the start of Sofa, initialization of all variables excepts haptics-related ones
void HapticAvatar_GrasperDeviceController::initImpl()
{
    // get ibox if one
    if (!l_iboxCtrl.empty())
    {
        m_iboxCtrl = l_iboxCtrl.get();
        if (m_iboxCtrl != nullptr)
        {
            msg_info() << "Device " << d_hapticIdentity.getValue() << " connected with IBox: " << m_iboxCtrl->d_hapticIdentity.getValue();
        }
    }


    simulation::Node *context = dynamic_cast<simulation::Node *>(this->getContext()); // access to current node
    m_forceFeedback = context->get<LCPForceFeedback>(this->getTags(), sofa::core::objectmodel::BaseContext::SearchRoot);

    m_HA_driver->setDeadBandPWMWidth(100, 0, 0, 0);
    if (m_forceFeedback == nullptr)
    {
        msg_warning() << "ForceFeedback not found";
    }
}


bool HapticAvatar_GrasperDeviceController::createHapticThreads()
{   
    m_terminate = false;
    haptic_thread = std::thread(Haptics, std::ref(this->m_terminate), this, m_HA_driver);
    copy_thread = std::thread(CopyData, std::ref(this->m_terminate), this);

    return true;
}


void HapticAvatar_GrasperDeviceController::Haptics(std::atomic<bool>& terminate, void * p_this, void * p_driver)
{ 
    std::cout << "Haptics thread" << std::endl;

    HapticAvatar_GrasperDeviceController* _deviceCtrl = static_cast<HapticAvatar_GrasperDeviceController*>(p_this);
    HapticAvatar_DriverPort* _driver = static_cast<HapticAvatar_DriverPort*>(p_driver);

    if (_deviceCtrl == nullptr)
    {
        msg_error("Haptics Thread: HapticAvatar_GrasperDeviceController cast failed");
        return;
    }

    if (_driver == nullptr)
    {
        msg_error("Haptics Thread: HapticAvatar_Driver cast failed");
        return;
    }

    /// Pointer to the IBoxController component
    HapticAvatar_IBoxController * _iboxCtrl = _deviceCtrl->m_iboxCtrl;

    // Loop Timer
    long targetSpeedLoop = 1; // Target loop speed: 1ms

    // Use computer tick for timer
    ctime_t refTicksPerMs = CTime::getRefTicksPerSec() / 1000;
    ctime_t targetTicksPerLoop = targetSpeedLoop * refTicksPerMs;

	auto startSimulationTime = std::chrono::high_resolution_clock::now();

    VecDeriv resForces;
    resForces.resize(6);
    int cptLoop = 0;
    ctime_t startTimePrev = CTime::getRefTime();
    ctime_t summedLoopDuration = 0;
	
	//log file
	//std::ofstream logFile("HapticAvatarLog.txt");
	bool contact = false;
    while (!terminate)
    {
        //auto t1 = std::chrono::high_resolution_clock::now();
        
        ctime_t startTime = CTime::getRefTime();
        summedLoopDuration += (startTime - startTimePrev);
        startTimePrev = startTime;

        // Get all info from devices
        _deviceCtrl->m_hapticData.anglesAndLength = _driver->getAnglesAndLength();
        _deviceCtrl->m_hapticData.toolId = _driver->getToolID();
        //_deviceCtrl->m_hapticData.motorValues = _driver->getLastPWM();

         if (_iboxCtrl)
        {
            float angle = _iboxCtrl->getJawOpeningAngle(_deviceCtrl->m_hapticData.toolId);
            _deviceCtrl->m_hapticData.jawOpening = angle;
        }

        // Force feedback computation
        if (_deviceCtrl->m_simulationStarted && _deviceCtrl->m_forceFeedback)
        {
            const HapticAvatar_GrasperDeviceController::VecCoord& articulations = _deviceCtrl->d_toolPosition.getValue();
            _deviceCtrl->m_forceFeedback->computeForce(articulations, resForces);

            /// ** resForces: **             
            /// articulations[0] => dofV[Dof::YAW];
            /// articulations[1] => -dofV[Dof::PITCH];
            /// articulations[2] => dofV[Dof::ROT];
            /// articulations[3] => dofV[Dof::Z];

            /// articulations[4] => Grasper up
            /// articulations[5] => Grasper Down 

            //_driver->setManualPWM(float(-resForces[2][0]), float(-resForces[1][0]), float(resForces[3][0]), float(-resForces[0][0]));
            _driver->setMotorForceAndTorques(-resForces[2][0], resForces[1][0], resForces[3][0], -resForces[0][0]);

            if (_iboxCtrl)
            {
                // TODO: implement Grasper ForceFeedback here, should be a conversion from 1D angular constraint into force
                float jaw_momentum_arm = 25.0f * sin(0.38f + _deviceCtrl->m_hapticData.jawOpening);
                float handleForce = (resForces[4][0] - resForces[5][0]) / jaw_momentum_arm; // in Newtons

                _iboxCtrl->setHandleForce(_deviceCtrl->m_hapticData.toolId, handleForce*3);
                //if (cptLoop == 50)
                //{
                //    std::cout << "handleForce (toolId=" << _deviceCtrl->m_hapticData.toolId << ") " << handleForce << std::endl;
                //    cptLoop = 0;
                //}
            }

            //if (cptLoop == 50)
            //{
            //    std::cout << "resForces: " << resForces << std::endl;
            //    cptLoop = 0;
            //}

            cptLoop++;
            if (cptLoop % 1000 == 0) {
                float updateFreq = 1000*1000 / ((float)summedLoopDuration / (float) refTicksPerMs); // in Hz
                std::cout << "Average haptic loop frequency " << std::to_string(int(updateFreq)) << std::endl;
                summedLoopDuration = 0;
            }
            if (cptLoop % 30000 == 0) {
                _driver->printStatus();
            }

        }
        else
            _driver->releaseForce();

        _driver->update();
        _iboxCtrl->update();

        ctime_t endTime = CTime::getRefTime();
        ctime_t duration = endTime - startTime;
		
		//contact = false;
		//for (int i = 0; i < 6; i++)
		//{
		//	if (resForces[i][0] != 0.0)
		//	{
		//		contact = true;
		//		break;
		//	}
		//}
		//if (contact)
		//{
		//	auto currentTime = std::chrono::high_resolution_clock::now();
		//	auto timeSinceStart = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - startSimulationTime);

		//	if (logFile.is_open())
		//	{
		//		logFile << timeSinceStart.count() << " ";
		//		logFile << contact << " "; 
		//		logFile << duration * 1000 / refTicksPerMs << " ";
		//		logFile << resForces << std::endl;
		//	}
		//}

		
        // If loop is quicker than the target loop speed. Wait here.
        //duration = 0;
        while (duration < targetTicksPerLoop)
        {
            endTime = CTime::getRefTime();
            duration = endTime - startTime;
        }
    }
	//logFile.close();

    // ensure no force
    _driver->releaseForce();
    std::cout << "Haptics thread END!!" << std::endl;
}


void HapticAvatar_GrasperDeviceController::CopyData(std::atomic<bool>& terminate, void * p_this)
{
    HapticAvatar_GrasperDeviceController* _deviceCtrl = static_cast<HapticAvatar_GrasperDeviceController*>(p_this);
    
    // Use computer tick for timer
    ctime_t targetSpeedLoop = 1/2; // Target loop speed: 0.5ms
    ctime_t refTicksPerMs = CTime::getRefTicksPerSec() / 1000;
    ctime_t targetTicksPerLoop = targetSpeedLoop * refTicksPerMs;
    double speedTimerMs = 1000 / double(CTime::getRefTicksPerSec());

    ctime_t lastTime = CTime::getRefTime();
    std::cout << "refTicksPerMs: " << refTicksPerMs << " targetTicksPerLoop: " << targetTicksPerLoop << std::endl;
    int cptLoop = 0;
    // Haptics Loop
    while (!terminate)
    {
        ctime_t startTime = CTime::getRefTime();
        _deviceCtrl->m_simuData = _deviceCtrl->m_hapticData;

        ctime_t endTime = CTime::getRefTime();
        ctime_t duration = endTime - startTime;

        // If loop is quicker than the target loop speed. Wait here.
        while (duration < targetTicksPerLoop)
        {
            endTime = CTime::getRefTime();
            duration = endTime - startTime;
        }
    }
}

void HapticAvatar_GrasperDeviceController::updatePositionImpl()
{
    if (!m_HA_driver)
        return;

    //std::cout << "updatePositionImpl" << std::endl;

    // get info from simuData
    sofa::type::fixed_array<float, 4> dofV = m_simuData.anglesAndLength;

    VecCoord & articulations = *d_toolPosition.beginEdit();
    //std::cout << "YAW: " << dofV[Dof::YAW] << " | PITCH: " << dofV[Dof::PITCH] << " | ROT: " << dofV[Dof::ROT] << " | Z: " << dofV[Dof::Z] << std::endl;
    articulations[0] = dofV[Dof::YAW];
    articulations[1] = -dofV[Dof::PITCH];
    articulations[2] = dofV[Dof::ROT];
    articulations[3] = dofV[Dof::Z];

    float _OpeningAngle = m_simuData.jawOpening * d_MaxOpeningAngle.getValue() * 0.01f;
    articulations[4] = _OpeningAngle;
    articulations[5] = -_OpeningAngle;

    d_toolPosition.endEdit();
}


void HapticAvatar_GrasperDeviceController::handleEvent(core::objectmodel::Event *event)
{
    if (!m_deviceReady)
        return;

    if (dynamic_cast<sofa::simulation::AnimateBeginEvent *>(event))
    {
        m_simulationStarted = true;
        updatePositionImpl();
    }
}

} // namespace sofa::HapticAvatar
