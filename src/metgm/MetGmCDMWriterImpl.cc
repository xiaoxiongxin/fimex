/*
 * Fimex
 *
 * (C) Copyright 2011, met.no
 *
 * Project Info:  https://wiki.met.no/fimex/start
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include "MetGmCDMWriterImpl.h"

// private/implementation code
//
#include "MetGmTags.h"
#include "MetGmUtils.h"
#include "MetGmVersion.h"
#include "MetGmHandlePtr.h"
#include "MetGmGroup1Ptr.h"
#include "MetGmGroup2Ptr.h"
#include "MetGmGroup3Ptr.h"
#include "MetGmGroup5Ptr.h"
#include "MetGmFileHandlePtr.h"
#include "MetGmConfigurationMappings.h"

// METGM C Lib
#include "metgm.h"

// fimex
//
#include "fimex/CDM.h"
#include "fimex/Data.h"
#include "fimex/SharedArray.h"
#include "fimex/String2Type.h"
#include "fimex/Units.h"
#include "fimex/interpolation.h"

// udunits
#include <udunits2.h>

// libxml2
#include <libxml/tree.h>
#include <libxml/xpath.h>

// standard
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <deque>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <set>

namespace MetNoFimex {

    #define FREE_TEXT "metgm_free_text"
    #define VERSION   "metgm_version"
    #define ANALYSIS_DATE_TIME "metgm_analysis_date_time"
    #define START_DATE_TIME "metgm_start_date_time"
    #define DATA_TYPE "metgm_data_type"
    #define MODEL_TYPE "metgm_model_type"
    #define PRODUCTION_NATION "metgm_production_nation"

typedef std::shared_ptr<MetGmTags> MetGmTagsPtr;

void MetGmCDMWriterImpl::configure(const std::unique_ptr<XMLDoc>& doc)
{
    if (!doc.get())
        throw CDMException("Please supply xml config file the MetGmReader has to be informed how are pids mapped to actual CDM variables");

    const CDM& cdmRef = cdmReader->getCDM();

    // start metgm_parameter
    xmlXPathObject_p xpathObj = doc->getXPathObject("/metgm_config/writer/metgm_parameter");
    xmlNodeSetPtr nodes = xpathObj->nodesetval;
    size_t size = (nodes) ? nodes->nodeNr : 0;
    for (size_t i = 0; i < size; ++i) {
        xmlNodePtr node = nodes->nodeTab[i];

        std::string metgmName = getXmlProp(node, "name");
        if (metgmName.empty()) {
            continue;
        }

        xmlXPathObject_p xpathObj = doc->getXPathObject("/metgm_config/writer/metgm_parameter[@name=\"" + metgmName + "\"]/attribute[@name=\"metgm_p_id\"]");
        std::string str_p_id;
        short p_id = 0;
        if (xpathObj->nodesetval && xpathObj->nodesetval->nodeNr > 0) {
            str_p_id = getXmlProp(xpathObj->nodesetval->nodeTab[0], "value");
            if (str_p_id.empty()) {
                continue;
            }
            p_id = string2type<size_t>(str_p_id);
        } else {
            continue;
        }

        xpathObj = doc->getXPathObject("/metgm_config/writer/metgm_parameter[@name=\"" + metgmName + "\"]/attribute[@name=\"_FillValue\"]");
        std::string str_FillValue;
        if (xpathObj->nodesetval && xpathObj->nodesetval->nodeNr > 0) {
            str_FillValue = getXmlProp(xpathObj->nodesetval->nodeTab[0], "value");
        }

        xpathObj = doc->getXPathObject("/metgm_config/writer/metgm_parameter[@name=\"" + metgmName + "\"]/attribute[@name=\"units\"]");
        std::string str_units;
        if (xpathObj->nodesetval && xpathObj->nodesetval->nodeNr > 0) {
            str_units = getXmlProp(xpathObj->nodesetval->nodeTab[0], "value");
        }

        xpathObj = doc->getXPathObject("/metgm_config/writer/metgm_parameter[@name=\"" + metgmName + "\"]/attribute[@name=\"standard_name\"]");
        std::string str_standard_name;
        if (xpathObj->nodesetval && xpathObj->nodesetval->nodeNr > 0) {
            str_standard_name = getXmlProp(xpathObj->nodesetval->nodeTab[0], "value");
            if (str_standard_name.empty()) {
                continue;
            }
            // find all variables with given standard_name value
            std::vector<std::string> varNames = cdmRef.findVariables("standard_name", str_standard_name);
            for (size_t index = 0; index < varNames.size(); ++index) {
                std::string varName = varNames[index];
                if (cdmRef.hasDimension(varName)) {
                    continue;
                }

                const CDMVariable* pVar = &cdmRef.getVariable(varName);

                if (!str_units.empty() && !cdmRef.getUnits(varName).empty()) {
                    /* check if dimensions convertible */
                    Units checker;
                    if (!checker.areConvertible(str_units, cdmRef.getUnits(varName))) {
                        continue;
                    }
                }

                MetGmConfigurationMappings cfgEntry(p_id, pVar->getName());
                cfgEntry.units_ = str_units.empty() ? std::string() : str_units;

                if (!str_FillValue.empty())
                    cfgEntry.fillValue_ = str_FillValue;

                xmlConfiguration_.insert(cfgEntry);
            }

        } else {
            continue;
        }
    }
    // end metgm_parameter

    xpathObj = doc->getXPathObject("/metgm_config/writer/variable");
    nodes = xpathObj->nodesetval;
    size = (nodes) ? nodes->nodeNr : 0;
    for (size_t i = 0; i < size; ++i) {

        xmlNodePtr node = nodes->nodeTab[i];

        std::string kildeName = getXmlProp(node, "name");
        if (kildeName.empty()) {
            continue;
        }

        if (!cdmRef.hasVariable(kildeName)) {
            continue;
        }

        xmlXPathObject_p xpathObj = doc->getXPathObject("/metgm_config/writer/variable[@name=\"" + kildeName + "\"]/attribute[@name=\"metgm_p_id\"]");
        std::string str_p_id;
        short p_id = 0;
        if (xpathObj->nodesetval && xpathObj->nodesetval->nodeNr > 0) {
            str_p_id = getXmlProp(xpathObj->nodesetval->nodeTab[0], "value");
            if (str_p_id.empty()) {
                continue;
            }
            p_id = string2type<size_t>(str_p_id);
        } else {
            continue;
        }

        xpathObj = doc->getXPathObject("/metgm_config/writer/metgm_parameter[@name=\"" + kildeName + "\"]/attribute[@name=\"units\"]");
        std::string str_units;
        if (xpathObj->nodesetval && xpathObj->nodesetval->nodeNr > 0) {
            str_units = getXmlProp(xpathObj->nodesetval->nodeTab[0], "value");
        }

        const CDMVariable* pVar = &cdmRef.getVariable(kildeName);

        if (!str_units.empty() && !cdmRef.getUnits(kildeName).empty()) {
            /* check if dimensions convertible */
            Units checker;
            if (!checker.areConvertible(str_units, cdmRef.getUnits(kildeName))) {
                continue;
            }
        }

        MetGmConfigurationMappings cfgEntry(p_id, pVar->getName());

        xpathObj = doc->getXPathObject("/metgm_config/writer/variable[@name=\"" + kildeName + "\"]/attribute[@name=\"_FillValue\"]");
        if (xpathObj->nodesetval && xpathObj->nodesetval->nodeNr > 0) {
            cfgEntry.fillValue_ = getXmlProp(xpathObj->nodesetval->nodeTab[0], "value");
        }

        xpathObj = doc->getXPathObject("/metgm_config/writer/variable[@name=\"" + kildeName + "\"]/attribute[@name=\"add_offset\"]");
        if (xpathObj->nodesetval && xpathObj->nodesetval->nodeNr > 0) {
            cfgEntry.addOffset_ = getXmlProp(xpathObj->nodesetval->nodeTab[0], "value");
        }

        xpathObj = doc->getXPathObject("/metgm_config/writer/variable[@name=\"" + kildeName + "\"]/attribute[@name=\"scale_factor\"]");
        if (xpathObj->nodesetval && xpathObj->nodesetval->nodeNr > 0) {
            cfgEntry.scaleFactor_ = getXmlProp(xpathObj->nodesetval->nodeTab[0], "value");
        }

        xmlConfiguration_.insert(cfgEntry);
    }
    }


    void MetGmCDMWriterImpl::writeGroup0Data()
    {
        MGM_THROW_ON_ERROR(mgm_set_version(*metgmHandle_, *metgmVersion_));
    }

    void MetGmCDMWriterImpl::writeGroup1Data()
    {
        std::shared_ptr<MetGmGroup1Ptr> pg1 = MetGmGroup1Ptr::createMetGmGroup1PtrForWriting(cdmReader);

        MGM_THROW_ON_ERROR(mgm_set_analysis_date_time(*metgmHandle_, pg1->analysisTime()))

        MGM_THROW_ON_ERROR(mgm_set_start_date_time(*metgmHandle_, pg1->startTime()))

        MGM_THROW_ON_ERROR(mgm_set_free_text(*metgmHandle_, pg1->freeText().c_str()))

        MGM_THROW_ON_ERROR(mgm_set_data_type(*metgmHandle_, pg1->dataType()))

        MGM_THROW_ON_ERROR(mgm_set_model_type(*metgmHandle_, pg1->modelType().c_str()))

        MGM_THROW_ON_ERROR(mgm_set_production_nation(*metgmHandle_, pg1->productNation().c_str()))

    }

    void MetGmCDMWriterImpl::writeGroup2Data()
    {
        const short np = cdmConfiguration_.size();

        MGM_THROW_ON_ERROR(mgm_set_number_of_params(*metgmHandle_, np))

        if(*metgmVersion_ == MGM_Edition2) {

            std::set<short> uniquePid;
            for (const MetGmCDMVariableProfile& profile : cdmConfiguration_)
                uniquePid.insert(profile.p_id_);

            const short ndp = uniquePid.size();

            MGM_THROW_ON_ERROR(mgm_set_number_of_dist_params(*metgmHandle_, ndp))

            size_t index = 0;
            for (const short p_id : uniquePid) {

                ++index;

                const MetGmCDMVariableProfileEqPId byPId(p_id);
                cdm_configuration::const_iterator pIt = std::find_if(cdmConfiguration_.begin(), cdmConfiguration_.end(), byPId), nIt = pIt;
                size_t ndpr = 1;
                while ((nIt = std::find_if(++nIt, cdmConfiguration_.end(), byPId)) != cdmConfiguration_.end())
                    ndpr += 1;

                MGM_THROW_ON_ERROR(mgm_set_param_id(*metgmHandle_, index, p_id))
                MGM_THROW_ON_ERROR(mgm_set_ndpr(*metgmHandle_, index, ndpr))

                /**
                  * TODO: should HD be treated as CDMAttribute?
                  *
                  * the HD value should be highest for given parameter
                  */
                MGM_THROW_ON_ERROR(mgm_set_hd(*metgmHandle_, index, pIt->hd()))
            }
        }
    }

    void MetGmCDMWriterImpl::writeHeader()
    {
        MGM_THROW_ON_ERROR(mgm_write_header(*metgmFileHandle_, *metgmHandle_))
    }

    void MetGmCDMWriterImpl::writeGroup3TimeAxis(const CDMVariable* pVar)
    {
        const MetGmTagsPtr& tags = std::find_if(cdmConfiguration_.begin(), cdmConfiguration_.end(), MetGmCDMVariableProfileEqName(pVar->getName()))->pTags_;

        if(tags->tTag().get()) {
            MGM_THROW_ON_ERROR(tags->set_nt(tags->tTag()->nT()))
            MGM_THROW_ON_ERROR(tags->set_dt(tags->tTag()->dT()))
        } else {
            MGM_THROW_ON_ERROR(tags->set_nt(1))
            MGM_THROW_ON_ERROR(tags->set_dt(metgmTimeTag_->dT() * metgmTimeTag_->nT()))
        }
    }

    void MetGmCDMWriterImpl::writeGroup3HorizontalAxis(const CDMVariable* pVar)
    {
        const MetGmTagsPtr& tags = std::find_if(cdmConfiguration_.begin(), cdmConfiguration_.end(), MetGmCDMVariableProfileEqName(pVar->getName()))->pTags_;

        // x
        MGM_THROW_ON_ERROR(tags->set_dx(tags->xTag()->dx()));
        MGM_THROW_ON_ERROR(tags->set_nx(tags->xTag()->nx()));
        MGM_THROW_ON_ERROR(tags->set_cx(tags->xTag()->cx()));
        // y
        MGM_THROW_ON_ERROR(tags->set_dy(tags->yTag()->dy()));
        MGM_THROW_ON_ERROR(tags->set_ny(tags->yTag()->ny()));
        MGM_THROW_ON_ERROR(tags->set_cy(tags->yTag()->cy()));
    }

    void MetGmCDMWriterImpl::writeGroup3VerticalAxis(const CDMVariable* pVar)
    {
        const MetGmTagsPtr& tags = std::find_if(cdmConfiguration_.begin(), cdmConfiguration_.end(), MetGmCDMVariableProfileEqName(pVar->getName()))->pTags_;

        if(tags->zTag().get()) {
            MGM_THROW_ON_ERROR(tags->set_nz(tags->zTag()->nz()));
            MGM_THROW_ON_ERROR(tags->set_pr(tags->zTag()->pr()));
            MGM_THROW_ON_ERROR(tags->set_pz(tags->zTag()->pz()));
        } else {
            MGM_THROW_ON_ERROR(tags->set_nz(1));
            MGM_THROW_ON_ERROR(tags->set_pr(0));
            MGM_THROW_ON_ERROR(tags->set_pz(1));
        }
    }

    void MetGmCDMWriterImpl::writeGroup3Data(const CDMVariable* pVar)
    {
        writeGroup3TimeAxis(pVar);

        writeGroup3HorizontalAxis(pVar);

        writeGroup3VerticalAxis(pVar);

        cdm_configuration::iterator it = std::find_if(cdmConfiguration_.begin(), cdmConfiguration_.end(), MetGmCDMVariableProfileEqName(pVar->getName()));
        MGM_THROW_ON_ERROR(mgm_write_group3(*metgmFileHandle_, *metgmHandle_, *(it)->pTags_->gp3()));
    }

    void MetGmCDMWriterImpl::writeGroup4Data(const CDMVariable* pVar)
    {
        const MetGmTagsPtr& tags = std::find_if(cdmConfiguration_.begin(), cdmConfiguration_.end(), MetGmCDMVariableProfileEqName(pVar->getName()))->pTags_;
        if (tags->zTag()) {
            MGM_THROW_ON_ERROR(mgm_write_group4(*metgmFileHandle_, *metgmHandle_, tags->zTag()->points().get()));
        } else {
            /* no z profile for variable */
            float f = 0;
            MGM_THROW_ON_ERROR(mgm_write_group4 (*metgmFileHandle_, *metgmHandle_, &f));
        }

    }

    void MetGmCDMWriterImpl::writeGroup5Data(const CDMVariable* pVar)
    {
        const MetGmCDMVariableProfile& profile =
            *std::find_if(cdmConfiguration_.begin(), cdmConfiguration_.end(), MetGmCDMVariableProfileEqName(pVar->getName()));

        MGM_THROW_ON_ERROR(mgm_write_group5 (*metgmFileHandle_, *metgmHandle_, profile.pTags_->data().get()));
    }

    void MetGmCDMWriterImpl::init()
    {
        for (xml_configuration::const_iterator pIt : sorted_by_pid(xmlConfiguration_)) {
            const MetGmConfigurationMappings& entry = *pIt;

            MetGmTagsPtr tags;

            if (std::find_if(cdmConfiguration_.begin(), cdmConfiguration_.end(), MetGmCDMVariableProfileEqName(entry.cdmName_)) != cdmConfiguration_.end()) {
                throw CDMException("hmmm... the variable should not be found in cdm profile map");
            }

            const CDMVariable* pVariable = &cdmReader->getCDM().getVariable(entry.cdmName_);

            tags = MetGmTags::createMetGmTagsForWriting(cdmReader, pVariable, metgmHandle_,
                                                        entry.p_id_, entry.fillValue_, entry.addOffset_, entry.scaleFactor_);

            assert(tags.get());

            if(tags->data().get() == 0) {
                continue;
            }

            MetGmCDMVariableProfile profile(entry.p_id_, entry.cdmName_, tags);
            cdmConfiguration_.insert(profile);
        }

        writeGroup0Data();
        writeGroup1Data();
        writeGroup2Data();

        writeHeader();

        const CDM& cdmRef = cdmReader->getCDM();
        cdm_configuration::const_iterator varIt;
        for(varIt = cdmConfiguration_.begin(); varIt != cdmConfiguration_.end(); ++varIt) {
            const CDMVariable* pVariable = &cdmRef.getVariable(varIt->cdmName_);
            writeGroup3Data(pVariable);
            writeGroup4Data(pVariable);
            writeGroup5Data(pVariable);
        }
    }

    MetGmCDMWriterImpl::MetGmCDMWriterImpl
            (
                    CDMReader_p cdmReader,
                    const std::string& outputFile,
                    const std::string& configFile
                    )
                        : CDMWriter(cdmReader, outputFile), configFileName_(configFile)
    {
        std::unique_ptr<XMLDoc> xmlDoc;
        if (!configFileName_.empty()) {
            xmlDoc.reset(new XMLDoc(configFileName_));
        }

        metgmTimeTag_ = MetGmTimeTag::createMetGmTimeTagGlobal(cdmReader);
        metgmVersion_ = MetGmVersion::createMetGmVersion(xmlDoc);
        metgmFileHandle_ = MetGmFileHandlePtr::createMetGmFileHandlePtrForWriting(outputFile);
        metgmHandle_ = MetGmHandlePtr::createMetGmHandleForWriting(metgmFileHandle_, metgmVersion_);

        configure(xmlDoc);

        init();
    }

    MetGmCDMWriterImpl::MetGmCDMWriterImpl(CDMReader_p cdmReader, const std::string& outputFile)
        : CDMWriter(cdmReader, outputFile)
    { }

    MetGmCDMWriterImpl::~MetGmCDMWriterImpl() { }

} // end namespace
