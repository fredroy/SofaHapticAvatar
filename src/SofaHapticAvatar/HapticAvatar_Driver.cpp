/******************************************************************************
*       SOFA, Simulation Open-Framework Architecture, development version     *
*                (c) 2006-2019 INRIA, USTL, UJF, CNRS, MGH                    *
*                                                                             *
* This program is free software; you can redistribute it and/or modify it     *
* under the terms of the GNU Lesser General Public License as published by    *
* the Free Software Foundation; either version 2.1 of the License, or (at     *
* your option) any later version.                                             *
*                                                                             *
* This program is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       *
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License *
* for more details.                                                           *
*                                                                             *
* You should have received a copy of the GNU Lesser General Public License    *
* along with this program. If not, see <http://www.gnu.org/licenses/>.        *
*******************************************************************************
* Authors: The SOFA Team and external contributors (see Authors.txt)          *
*                                                                             *
* Contact information: contact@sofa-framework.org                             *
******************************************************************************/

#include <SofaHapticAvatar/HapticAvatar_Driver.h>
#include <sofa/helper/logging/Messaging.h>

namespace sofa::HapticAvatar
{

using namespace HapticAvatar;

HapticAvatar_BaseDriver::HapticAvatar_BaseDriver(const std::string& portName)
    : m_connected(false)
    , m_portName(portName)
{
    // First try to connect to device
    connectDevice();

    if (!m_connected)
    {
        msg_error("HapticAvatar_BaseDriver") << "## Device Not Connected at port: " << m_portName;
    }
}


HapticAvatar_BaseDriver::~HapticAvatar_BaseDriver()
{
    //Check if we are connected before trying to disconnect
    if (m_connected)
    {
        //We're no longer connected
        m_connected = false;
        //Close the serial handler
        CloseHandle(m_hSerial);
    }
}



///////////////////////////////////////////////////////////////
/////      Public API methods for any communication       /////
///////////////////////////////////////////////////////////////

int HapticAvatar_BaseDriver::resetDevice(int mode)
{
    char incomingData[INCOMING_DATA_LEN];
    std::string arguments = std::to_string(mode);
    if (sendCommandToDevice(getResetCommandId(), arguments, incomingData) == false) {
        return -1;
    }

    int res = std::atoi(incomingData);
    return res;
}



std::string HapticAvatar_BaseDriver::getIdentity()
{
    char incomingData[INCOMING_DATA_LEN];
    if (sendCommandToDevice(getIdentityCommandId(), "", incomingData) == false) {
        return "Unknown";
    }

    std::string iden = convertSingleData(incomingData);
    return iden;
}


int HapticAvatar_BaseDriver::getToolID()
{
    char incomingData[INCOMING_DATA_LEN];
    if (sendCommandToDevice(getToolIDCommandId(), "", incomingData) == false) {
        return -1;
    }

    int res = std::atoi(incomingData);
    std::cout << "getToolID: " << res << std::endl;
    return res;
}


int HapticAvatar_BaseDriver::getDeviceStatus()
{
    char incomingData[INCOMING_DATA_LEN];
    if (sendCommandToDevice(getStatusCommandId(), "", incomingData) == false) {
        return -1;
    }

    int res = std::atoi(incomingData);
    std::cout << "getDeviceStatus: " << res << std::endl;
    return res;
}


bool HapticAvatar_BaseDriver::sendCommandToDevice(int commandId, const std::string& arguments, char *result)
{
    std::string fullCommand = std::to_string(commandId) + " " + arguments + " \n";
    //std::cout << "fullCommand: '" << fullCommand << "'" << std::endl;
    char outgoingData[OUTGOING_DATA_LEN];
    strcpy(outgoingData, fullCommand.c_str());

    unsigned int outlen = (unsigned int)(strlen(outgoingData));
    bool write_success = WriteDataImpl(outgoingData, outlen);
    if (!write_success) {
        return false;
    }

    // request data
    if (result != nullptr)
        getDataImpl(result, false);

    return true;
}


std::string HapticAvatar_BaseDriver::convertSingleData(char *buffer, bool forceRemoveEoL)
{
    std::string res = std::string(buffer);
    if (forceRemoveEoL)
        res.pop_back();
    else if (res.back() == '\n')
        res.pop_back();

    while (res.back() == ' ' || res.back() == '\n')
    {
        res.pop_back();
    }

    return res;
}




///////////////////////////////////////////////////////////////
/////      Internal Methods for device communication      /////
///////////////////////////////////////////////////////////////

void HapticAvatar_BaseDriver::connectDevice()
{
    //Try to connect to the given port throuh CreateFile
    m_hSerial = CreateFileA(m_portName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    //Check if the connection was successfull
    if (m_hSerial == INVALID_HANDLE_VALUE)
    {
        //If not success full display an Error
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {

            //Print Error if neccessary
            msg_error("HapticAvatar_BaseDriver") << "Handle was not attached. Reason: " << m_portName << " not available.";
        }
        else
        {
            msg_error("HapticAvatar_BaseDriver") << "Unknown error occured!";
        }
    }
    else
    {
        //If connected we try to set the comm parameters
        DCB dcbSerialParams = { 0 };

        //Try to get the current
        if (!GetCommState(m_hSerial, &dcbSerialParams))
        {
            //If impossible, show an error
            msg_warning("HapticAvatar_BaseDriver") << "Failed to get current serial parameters!";
        }
        else
        {
            //Define serial connection parameters for the arduino board
            dcbSerialParams.BaudRate = CBR_9600;
            dcbSerialParams.ByteSize = 8;
            dcbSerialParams.StopBits = ONESTOPBIT;
            dcbSerialParams.Parity = NOPARITY;
            //Setting the DTR to Control_Enable ensures that the Arduino is properly
            //reset upon establishing a connection
            dcbSerialParams.fDtrControl = DTR_CONTROL_ENABLE;

            //Set the parameters and check for their proper application
            if (!SetCommState(m_hSerial, &dcbSerialParams))
            {
                msg_warning("HapticAvatar_BaseDriver") << "ALERT: Could not set Serial Port parameters";
            }
            else
            {
                //If everything went fine we're connected
                m_connected = true;
                //Flush any remaining characters in the buffers
                PurgeComm(m_hSerial, PURGE_RXCLEAR | PURGE_TXCLEAR);
                //We wait 2s as the arduino board will be reseting
                Sleep(ARDUINO_WAIT_TIME);
            }
        }
    }
}


int HapticAvatar_BaseDriver::getDataImpl(char *buffer, bool do_flush)
{
    bool response = false;
    int cptSecu = 0;
    int n = 0;
    int que = 0;
    char * pch;
    int num_cr = 0;
    while (!response && cptSecu < 10000)
    {
        n = ReadDataImpl(buffer, INCOMING_DATA_LEN, &que, do_flush);
        if (n > 0)
        {
            // count the number of \n in the return string
            pch = strchr(buffer, '\n');
            while (pch != NULL)
            {
                num_cr++;
                pch = strchr(pch + 1, '\n');
            }
            response = true;
        }

        cptSecu++;
    }

    if (!response) // secu loop reach end
    {
        std::cerr << "## Error getData no message returned. Reach security loop limit: " << cptSecu << std::endl;
        return -1;
    }

    return num_cr;
}


int HapticAvatar_BaseDriver::ReadDataImpl(char *buffer, unsigned int nbChar, int *queue, bool do_flush)
{
    //Number of bytes we'll have read
    DWORD bytesRead = 0;
    DWORD junkBytesRead = 0;

    //Use the ClearCommError function to get status info on the Serial port
    ClearCommError(m_hSerial, &m_errors, &m_status);

    *queue = (int)m_status.cbInQue;


    if (do_flush) {
        if (*queue > 0)
            // read away as much as possible, max 512
            ReadFile(m_hSerial, buffer, std::min(unsigned int(*queue), nbChar), &bytesRead, NULL);
    }
    else {
        // regardless of what the queue is, read nbChar bytes, in an attempt to suspend the thread
        ReadFile(m_hSerial, buffer, *queue, &bytesRead, NULL);
        //ClearCommError(this->hSerial, &this->errors, &this->status);
        //*queue = (int)status.cbInQue;
        //if (*queue > 0) {
        //	char junkbuffer[32];
        //	// clear the buffer from any remaining bytes
        //	ReadFile(this->hSerial, junkbuffer, min(*queue, 32), &junkBytesRead, NULL);
        //}

    }
    return bytesRead;
}


bool HapticAvatar_BaseDriver::WriteDataImpl(char *buffer, unsigned int nbChar)
{
    DWORD bytesSend;

    //Try to write the buffer on the Serial port
    if (!WriteFile(m_hSerial, (void *)buffer, nbChar, &bytesSend, 0))
    {
        //In case it don't work get comm error and return false
        ClearCommError(m_hSerial, &m_errors, &m_status);

        std::cerr << "Error failed to send command: '" << buffer << "'. Error returned: " << m_errors << std::endl;
        return false;
    }
    else
        return true;
}




///////////////////////////////////////////////////////////////
/////      Methods for specific device communication      /////
///////////////////////////////////////////////////////////////

HapticAvatar_Driver::HapticAvatar_Driver(const std::string& portName)
    : HapticAvatar_BaseDriver(portName)
{

}


sofa::type::fixed_array<float, 4> HapticAvatar_Driver::getAngles_AndLength()
{
    sofa::type::fixed_array<float, 4> results;
    char incomingData[INCOMING_DATA_LEN];
    if (sendCommandToDevice(CmdTool::GET_ANGLES_AND_LENGTH, "", incomingData) == false) {
        return results;
    }

    char* pEnd;
    results[0] = std::strtof(incomingData, &pEnd) * 0.0001f;
    for (unsigned int i = 1; i < 4; ++i)
    {
        results[i] = std::strtof(pEnd, &pEnd) * 0.0001f;
    }

    return results;
}


float HapticAvatar_Driver::getJawTorque()
{
    char incomingData[INCOMING_DATA_LEN];
    if (sendCommandToDevice(CmdTool::GET_TOOL_JAW_TORQUE, "", incomingData) == false) {
        return 0.0f;
    }

    char* pEnd;
    float res = std::strtof(incomingData, &pEnd) * 0.0001f;

    return res;
}


float HapticAvatar_Driver::getJawOpeningAngle()
{
    char incomingData[INCOMING_DATA_LEN];
    std::cout << "getJawOpeningAngle" << std::endl;
    if (sendCommandToDevice(CmdTool::SET_TOOL_JAW_OPENING_ANGLE, "", incomingData) == false) {
        return 0.0f;
    }
    std::cout << "getJawOpeningAngle out" << std::endl;
    char* pEnd;
    float res = std::strtof(incomingData, &pEnd) * 0.0001f;

    return res;
}




sofa::type::fixed_array<float, 4> HapticAvatar_Driver::getLastPWM()
{
    sofa::type::fixed_array<float, 4> results;
    char incomingData[INCOMING_DATA_LEN];
    if (sendCommandToDevice(CmdTool::GET_LAST_PWM, "", incomingData) == false) {
        return results;
    }

    char* pEnd;
    results[0] = std::strtof(incomingData, &pEnd);
    for (unsigned int i = 1; i < 4; ++i)
    {
        results[i] = std::strtof(pEnd, &pEnd);
    }

    return results;
}


sofa::type::fixed_array<float, 4> HapticAvatar_Driver::getMotorScalingValues()
{
    sofa::type::fixed_array<float, 4> results;
    char incomingData[INCOMING_DATA_LEN];
    if (sendCommandToDevice(CmdTool::GET_MOTOR_SCALING_VALUES, "", incomingData) == false) {
        return results;
    }

    char* pEnd;
    results[0] = std::strtof(incomingData, &pEnd) * 0.0001f;
    for (unsigned int i = 1; i < 4; ++i)
    {
        results[i] = std::strtof(pEnd, &pEnd) * 0.0001f;
    }

    return results;
}


sofa::type::fixed_array<float, 3> HapticAvatar_Driver::getLastCollisionForce()
{
    sofa::type::fixed_array<float, 3> results;
    char incomingData[INCOMING_DATA_LEN];
    if (sendCommandToDevice(CmdTool::GET_LAST_COLLISION_FORCE, "", incomingData) == false) {
        return results;
    }

    char* pEnd;
    results[0] = std::strtof(incomingData, &pEnd) * 0.0001f;
    for (unsigned int i = 1; i < 3; ++i)
    {
        results[i] = std::strtof(pEnd, &pEnd) * 0.0001f;
    }

    return results;
}


void HapticAvatar_Driver::setMotorForce_AndTorques(sofa::type::fixed_array<float, 4> values)
{
    std::string arguments;
    for (unsigned int i = 0; i < values.size(); ++i)
    {
        int value = int(values[i] * 10000);
        arguments = arguments + std::to_string(value) + " ";
    }
    arguments.pop_back(); // remove last space, too avoid any synthax problem.

    sendCommandToDevice(CmdTool::SET_MOTOR_FORCE_AND_TORQUES, arguments, nullptr);
    return;
}

void HapticAvatar_Driver::setTipForce_AndRotTorque(sofa::type::Vector3 force, float RotTorque)
{
    std::string arguments;
    for (unsigned int i = 0; i < force.size(); ++i)
    {
        int value = int(force[i] * 10000);
        arguments = arguments + std::to_string(value) + " ";
    }
    int rotValue = int(RotTorque * 10000);
    arguments += std::to_string(rotValue);

    sendCommandToDevice(CmdTool::SET_MOTOR_FORCE_AND_TORQUES, arguments, nullptr);
    return;
}


int cptF = 0;

void HapticAvatar_Driver::setManualForceVector(sofa::type::Vector3 force, bool useManualPWM)
{
    // ./sofa-build/bin/Release/runSofa.exe sofa_plugins/SofaHapticAvatar/examples/HapticAvatar_collision_cube.scn

    // SET_MANUAL_PWM RotPWM PitchPWM ZPWM YawPWM
    // SET_MANUAL_PWM is command number 35.

    // RotPWM = int(-17.56 * Rot_torque)
    // PitchPWM = int(-2.34 * Pitch_torque)
    // ZPWM = int(-82.93*Z_force)
    // YawPWM = int(3.41*Yaw_torque)

    //std::cout << "setTranslationForce: " << force << std::endl;
    //double res = force.norm();

    sofa::type::Vector3 toolDir = sofa::type::Vector3(0, 1, 0);
    sofa::type::Vector3 yawDir = sofa::type::Vector3(0, 0, 1);
    sofa::type::Vector3 pitchDir = sofa::type::Vector3(-1, 0, 0);

    float rotTorque = 0.0f;
    float pitchTorque = float(pitchDir * force * 10);
    float zForce = float(toolDir * force);
    float yawTorque = float(yawDir * force * 10);

    /*if (res > 20)
        zForce = 20;
    else
        zForce = res;
        */
    if (useManualPWM)
        setManual_PWM(rotTorque, pitchTorque, zForce, yawTorque);
    else
        setManual_Force_and_Torques(rotTorque, pitchTorque, zForce, yawTorque);
    //SET_TIP_FORCE_AND_ROT_TORQUE
}


void HapticAvatar_Driver::setTipForceVector(sofa::type::Vector3 force)
{
    sofa::type::fixed_array<int, 4> values;
    values[0] = int(force[0] * 100);
    values[1] = int(force[2] * 100);
    values[2] = -int(force[1] * 100);
    values[3] = 0;

    std::string args;
    args = std::to_string(values[0])
        + " " + std::to_string(values[1])
        + " " + std::to_string(values[2])
        + " " + std::to_string(values[3]);

    bool resB = sendCommandToDevice(CmdTool::SET_TIP_FORCE_AND_ROT_TORQUE, args, nullptr);
    std::cout << "args: '" << args << "'" << std::endl;
    if (resB == false)
    {
        std::string fullCommand = std::to_string(CmdTool::SET_TIP_FORCE_AND_ROT_TORQUE) + " " + args;
        std::cerr << "Error failed to send command: '" << fullCommand << "'" << std::endl;
    }

}


void HapticAvatar_Driver::releaseForce()
{
    sendCommandToDevice(CmdTool::SET_MANUAL_PWM, "0 0 0 0", nullptr);
}


void HapticAvatar_Driver::setManual_PWM(float rotTorque, float pitchTorque, float zforce, float yawTorque)
{
    //bool sendForce = false;
    //if (pitchTorque != 0.0f || zforce != 0.0f || yawTorque != 0.0f)
    //{
    //    std::cout << "Force: rotTorque: " << rotTorque
    //        << " | pitchTorque: " << pitchTorque
    //        << " | zforce: " << zforce
    //        << " | yawTorque: " << yawTorque
    //        << std::endl;
    //    //sendForce = true;
    //}

    sofa::type::fixed_array<int, 4> values;     
    values[0] = int(-17.56 * rotTorque); // RotPWM
    values[1] = int(2.34 * pitchTorque); // PitchPWM
    values[2] = int(-82.93 * zforce); // ZPWM
    values[3] = int(3.41 * yawTorque); // YawPWM

    if (cptF == 100)
    {
        std::cout << "zForce: " << values[2]
            << " | pitchTorque: " << values[1]
            << " | yawTorque: " << values[3]
            << " | toolTorque: " << values[0]
            << std::endl;

        cptF = 0;
    }
    // cptF++;

    int maxPWM = 1000;
    for (int i = 0; i < 4; i++)
    {
        if (values[i] < -maxPWM)
            values[i] = -maxPWM;
        else if (values[i] > maxPWM)
            values[i] = maxPWM;
    }

    std::string args;
    args = std::to_string(values[0])
        + " " + std::to_string(values[1])
        + " " + std::to_string(values[2])
        + " " + std::to_string(values[3]);

    bool resB = sendCommandToDevice(CmdTool::SET_MANUAL_PWM, args, nullptr);
    if (resB == false)
    {
        std::string fullCommand = std::to_string(CmdTool::SET_MANUAL_PWM) + " " + args;
        std::cerr << "Error failed to send command: '" << fullCommand << "'" << std::endl;
    }
    //std::cout << "force resB: " << resB << std::endl;

    //char incomingData[INCOMING_DATA_LEN];
    //int resMsg = getDataImpl(incomingData, false);
    //if (resMsg == 1)
    //    std::cout << "force return msg: " << incomingData << std::endl;
}


void HapticAvatar_Driver::setManual_Force_and_Torques(float rotTorque, float pitchTorque, float zforce, float yawTorque)
{
    sofa::type::fixed_array<int, 4> values;
    values[0] = int(rotTorque * 10000);
    values[1] = int(pitchTorque * 10000);
    values[2] = int(zforce * 10000);
    values[3] = int(yawTorque * 10000);

    std::string args;
    args = std::to_string(values[0])
        + " " + std::to_string(values[1])
        + " " + std::to_string(values[2])
        + " " + std::to_string(values[3]);

    bool resB = sendCommandToDevice(CmdTool::SET_MOTOR_FORCE_AND_TORQUES, args, nullptr);
    if (resB == false)
    {
        std::string fullCommand = std::to_string(CmdTool::SET_MOTOR_FORCE_AND_TORQUES) + " " + args;
        std::cerr << "Error failed to send command: '" << fullCommand << "'" << std::endl;
    }
    //std::cout << "force resB: " << resB << std::endl;
}



///////////////////////////////////////////////////////////////
/////       Methods for specific IBOX communication       /////
///////////////////////////////////////////////////////////////

HapticAvatar_IboxDriver::HapticAvatar_IboxDriver(const std::string& portName)
    : HapticAvatar_BaseDriver(portName)
{

}


sofa::type::fixed_array<float, 4> HapticAvatar_IboxDriver::getOpeningValues()
{
    sofa::type::fixed_array<float, 4> results;
    char incomingData[INCOMING_DATA_LEN];
    if (sendCommandToDevice(CmdIBox::GET_IBOX_OPENING_VALUES, "", incomingData) == false) {
        return results;
    }

    char* pEnd;
    results[0] = std::strtof(incomingData, &pEnd) * 0.0001f;
    for (unsigned int i = 1; i < 4; ++i)
    {
        results[i] = std::strtof(pEnd, &pEnd) * 0.0001f;
    }

    return results;
}


void HapticAvatar_IboxDriver::setHandleForces(float upperJawForce, float lowerJawForce)
{
    // TODO convert force into string command

    // add value threashold check if needed
    //float maxPWM = 1000;
    //if (upperJawForce > maxPWM)
    //    upperJawForce = maxPWM;

    //if (lowerJawForce > maxPWM)
    //    lowerJawForce = maxPWM;

	int force = (int)(10000.0f * (upperJawForce - lowerJawForce));	
    std::string args;
    args = std::to_string(force)
        + " " + std::to_string(0)
		+ " " + std::to_string(0)
		+ " " + std::to_string(0)
		+ " " + std::to_string(0)
		+ " " + std::to_string(0);
    //args = std::to_string(upperJawForce)
    //    +" " + std::to_string(lowerJawForce);

    //std::cout << "setHandleForces command args: " << args << std::endl;

    bool resB = sendCommandToDevice(CmdIBox::SET_IBOX_ALL_FORCES, args, nullptr);
    if (resB == false)
    {
        std::string fullCommand = std::to_string(CmdIBox::SET_IBOX_ALL_FORCES) + " " + args;
        std::cerr << "Error failed to send command: '" << fullCommand << "'" << std::endl;
    }
}

void HapticAvatar_IboxDriver::setLoopGain(int loopGain)
{
	std::string args;
	args = std::to_string(loopGain);
	for (int chan = 0; chan < 6; chan++)
	{
		args = std::to_string(chan) + " " + std::to_string(loopGain) + " " + std::to_string(0);

		std::string fullCommand = std::to_string(CmdIBox::SET_IBOX_LOOP_GAIN) + " " + args;
		std::cout << "LoopGain command args: " << fullCommand << std::endl;

		bool resB = sendCommandToDevice(CmdIBox::SET_IBOX_LOOP_GAIN, args, nullptr);
		if (resB == false)
		{
			std::cerr << "Error failed to send command: '" << fullCommand << "'" << std::endl;
		}
	}
}


} // namespace sofa::HapticAvatar
