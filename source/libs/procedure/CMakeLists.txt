#___INFO__MARK_BEGIN_NEW__
###########################################################################
#
#  Copyright 2026 HPC-Gridware GmbH
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
###########################################################################
#___INFO__MARK_END_NEW__

# source/libs/uti
set(LIBRARY_NAME procedure)
set(LIBRARY_SOURCES
        qhost/ocs_QHostContoller.cc
        qhost/ocs_QHostParameter.cc
        qhost/ocs_QHostModel.cc
        qhost/ocs_QHostViewBase.cc
        qhost/ocs_QHostViewPlain.cc
        qhost/ocs_QHostViewXML.cc

        qstat/ocs_QStatGenericModel.cc
        qstat/ocs_QStatParameter.cc
        qstat/ocs_QStatDefaultController.cc
        qstat/ocs_QStatDefaultViewBase.cc
        qstat/ocs_QStatDefaultViewPlain.cc
        qstat/ocs_QStatDefaultViewXML.cc
        qstat/ocs_QStatGroupController.cc
        qstat/ocs_QStatGroupViewBase.cc
        qstat/ocs_QStatGroupViewPlain.cc
        qstat/ocs_QStatGroupViewXML.cc
        qstat/ocs_QStatSelectController.cc
        qstat/ocs_QStatSelectViewBase.cc
        qstat/ocs_QStatSelectViewPlain.cc
        qstat/ocs_QStatSelectViewXML.cc
        qstat/ocs_QStatModelBase.cc
        qstat/ocs_QStatJobModel.cc
        qstat/ocs_QStatJobViewBase.cc
        qstat/ocs_QStatJobViewPlain.cc
        qstat/ocs_QStatJobViewXML.cc
        qstat/ocs_QStatJobController.cc

        qrstat/ocs_QRStatParameter.cc
        qrstat/ocs_QRStatModel.cc
        qrstat/ocs_QRStatViewBase.cc
        qrstat/ocs_QRStatViewPlain.cc
        qrstat/ocs_QRStatViewXML.cc
        qrstat/ocs_QRStatController.cc

        qquota/ocs_QQuotaParameter.cc
        qquota/ocs_QQuotaModel.cc
        qquota/ocs_QQuotaViewBase.cc
        qquota/ocs_QQuotaViewPlain.cc
        qquota/ocs_QQuotaViewXML.cc
        qquota/ocs_QQuotaController.cc

)

set(LIBRARY_INCLUDES "./" ${SGE_INCLUDES})
add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${LIBRARY_INCLUDES})

