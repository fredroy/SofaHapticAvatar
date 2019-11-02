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

#include <SofaHapticAvatar/HapticAvatarDefines.h>
#include <SofaHapticAvatar/PortalManager.h>
#include <sofa/core/ObjectFactory.h>
#include <sofa/helper/system/FileRepository.h>
#include <tinyxml.h>

#include <iostream>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>


namespace sofa
{

namespace component
{

namespace controller
{

int PortalManagerClass = core::RegisterObject("TODO detail")
    .add< PortalManager >()
    ;


PortalManager::PortalManager()
    : m_configFilename(initData(&m_configFilename, "configFilename", "Config Filename of the object"))
    , m_portalPosition0(initData(&m_portalPosition0, "portalPosition0", "portal rigid position test"))
    , m_portalPosition1(initData(&m_portalPosition1, "portalPosition1", "portal rigid position test"))
    , m_portalPosition2(initData(&m_portalPosition2, "portalPosition2", "portal rigid position test"))
    , m_portalPosition3(initData(&m_portalPosition3, "portalPosition3", "portal rigid position test"))
    , m_portalPosition4(initData(&m_portalPosition4, "portalPosition4", "portal rigid position test"))
{    
}


void PortalManager::init()
{
    msg_info() << "PortalManager::init()";
    parseConfigFile();
    portalsSetup();
  /*  PortalManager::VecCoord & pos = *m_portalsPosition.beginEdit();
    pos.resize(m_portals.size());
    m_portalsPosition.endEdit();*/
    printInfo();
}

void PortalManager::portalsSetup()
{
    int cpt = 0;
    for (auto pController : m_portals)
    {
        pController->portalSetup();
        // TODO need to find a better way of doing this
        if (cpt == 0)
            m_portalPosition0 = pController->getPortalPosition();
        else if (cpt == 1)
            m_portalPosition1 = pController->getPortalPosition();
        else if (cpt == 2)
            m_portalPosition2 = pController->getPortalPosition();
        else if (cpt == 3)
            m_portalPosition3 = pController->getPortalPosition();
        else if (cpt == 4)
            m_portalPosition4 = pController->getPortalPosition();
        
        std::cout << "pController->getPortalPosition(): " << pController->getPortalPosition() << std::endl;
        cpt++;
    }
}

void PortalManager::reinit()
{
    msg_info() << "PortalManager::reinit()";
}

void PortalManager::handleEvent(core::objectmodel::Event *)
{
    //msg_info() << "PortalManager::handleEvent()";
}

void PortalManager::draw(const sofa::core::visual::VisualParams* vparams)
{

}


void PortalManager::setFilename(std::string f)
{
    m_configFilename.setValue(f);
}

const std::string& PortalManager::getFilename()
{
    return m_configFilename.getValue();
}


bool PortalManager::getIntAttribute(const TiXmlElement* elem, const char* attributeN, int* value)
{
    int res = elem->QueryIntAttribute(attributeN, value);
    if (res == TIXML_WRONG_TYPE)
    {
        msg_error() << "Wrong XML format attribute, waiting for Int for attribute: " << attributeN;
        return false;
    }
    else if (res == TIXML_NO_ATTRIBUTE)
    {
        msg_error() << "Wrong XML attribute, attribute not found: " << attributeN;
        return false;
    }

    return true;
}

bool PortalManager::getFloatAttribute(const TiXmlElement* elem, const char* attributeN, float* value)
{
    int res = elem->QueryFloatAttribute(attributeN, value);
    if (res == TIXML_WRONG_TYPE)
    {
        msg_error() << "Wrong XML format attribute, waiting for Float for attribute: " << attributeN;
        return false;
    }
    else if (res == TIXML_NO_ATTRIBUTE)
    {
        msg_error() << "Wrong XML attribute, attribute not found: " << attributeN;
        return false;
    }

    return true;
}


bool PortalManager::parseConfigFile()
{
    m_componentstate = sofa::core::objectmodel::ComponentState::Invalid;

    // -- Check filename field:
    if (m_configFilename.getValue() == "")
    {
        msg_error() << "No file to load. Data configFilename is not set.";
        return false;
    }

    // -- Check if file exist:
    const char* filename = m_configFilename.getFullPath().c_str();
    std::string sfilename(filename);

    if (!sofa::helper::system::DataRepository.findFile(sfilename))
    {
        msg_error() << "File: '" << m_configFilename << "' not found. ";
        return false;
    }

    TiXmlDocument doc(filename); // the resulting document tree
    if (!(doc.LoadFile()))
    {
        msg_error() << "Failed to open " << filename << "\n" << doc.ErrorDesc() << " at line " << doc.ErrorRow() << " row " << doc.ErrorCol();
        return false;
    }

    const TiXmlElement* hRoot = doc.RootElement();
    if (hRoot == nullptr)
    {
        msg_error() << " Empty document: " << filename;
        return false;
    }

    // start reading the xml config file:
    std::string resHeader = hRoot->ValueStr();
    if (resHeader != "Procedure")
    {
        msg_error() << " File format error, searching for Procedure, get: "<< resHeader;
        return false;
    }

    const char* procedureName = hRoot->Attribute("Name");
    std::cout << "procedureName: " << procedureName << std::endl;

    // get portals child node:
    const TiXmlNode* pChild = hRoot->FirstChild("Portals");
    if (pChild == nullptr)
    {
        msg_error() << " File format error2: " << filename;
        return false;
    }
    
    for (const TiXmlElement* portalNode = pChild->FirstChildElement("Portal"); portalNode != 0; portalNode = portalNode->NextSiblingElement("Portal"))
    {
        if (portalNode == nullptr)
            continue;
        const TiXmlElement* portalSettings = portalNode->FirstChildElement("PortalSettings");
        if (portalSettings == nullptr)
            continue;

        int idP, rail;
        bool res = getIntAttribute(portalNode, "Number", &idP);

        getIntAttribute(portalSettings, "Rail", &rail);

        float railPos, flipAngle, tiltAngle;
        getFloatAttribute(portalSettings, "RailPos", &railPos);
        getFloatAttribute(portalSettings, "FlipAngle", &flipAngle);
        getFloatAttribute(portalSettings, "TiltAngle", &tiltAngle);

        PortalController* pController = new PortalController(idP, rail, railPos, flipAngle, tiltAngle, std::string(portalSettings->Attribute("ComPort")));        
        m_portals.push_back(pController);
    }
    
    m_componentstate = sofa::core::objectmodel::ComponentState::Valid;
    return true;
}


void PortalManager::printInfo()
{
    for (auto pController : m_portals)
    {
        pController->printInfo();
    }
}

} // namespace controller

} // namespace component

} // namespace sofa