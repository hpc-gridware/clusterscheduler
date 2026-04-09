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
        ocs_ProcedureParameter.cc
        ocs_ProcedureModel.cc
        ocs_ProcedureView.cc
        ocs_ProcedureController.cc

        qhost/ocs_QHostContoller.cc
        qhost/ocs_QHostParameter.cc
        qhost/ocs_QHostParameterClient.cc
        qhost/ocs_QHostModelBase.cc
        qhost/ocs_QHostModelClient.cc
        qhost/ocs_QHostModelServer.cc
        qhost/ocs_QHostViewBase.cc
        qhost/ocs_QHostViewPlain.cc
        qhost/ocs_QHostViewXML.cc
        qhost/ocs_QHostViewJSON.cc
        qhost/ocs_QHostModelServer.cc

        qrstat/ocs_QRStatController.cc
        qrstat/ocs_QRStatParameter.cc
        qrstat/ocs_QRStatParameterClient.cc
        qrstat/ocs_QRStatModelBase.cc
        qrstat/ocs_QRStatModelClient.cc
        qrstat/ocs_QRStatModelServer.cc
        qrstat/ocs_QRStatViewBase.cc
        qrstat/ocs_QRStatViewPlain.cc
        qrstat/ocs_QRStatViewJSON.cc
        qrstat/ocs_QRStatViewXML.cc

        qquota/ocs_QQuotaParameter.cc
        qquota/ocs_QQuotaParameterClient.cc
        qquota/ocs_QQuotaModelBase.cc
        qquota/ocs_QQuotaModelClient.cc
        qquota/ocs_QQuotaModelServer.cc
        qquota/ocs_QQuotaViewBase.cc
        qquota/ocs_QQuotaViewPlain.cc
        qquota/ocs_QQuotaViewXML.cc
        qquota/ocs_QQuotaViewJSON.cc
        qquota/ocs_QQuotaController.cc

        qstat/default/ocs_QStatDefaultController.cc
        qstat/default/ocs_QStatDefaultViewBase.cc
        qstat/default/ocs_QStatDefaultViewPlain.cc
        qstat/default/ocs_QStatDefaultViewXML.cc

        qstat/group/ocs_QStatGroupController.cc
        qstat/group/ocs_QStatGroupViewBase.cc
        qstat/group/ocs_QStatGroupViewPlain.cc
        qstat/group/ocs_QStatGroupViewXML.cc

        qstat/select/ocs_QStatSelectController.cc
        qstat/select/ocs_QStatSelectViewBase.cc
        qstat/select/ocs_QStatSelectViewPlain.cc
        qstat/select/ocs_QStatSelectViewXML.cc

        qstat/job/ocs_QStatJobModel.cc
        qstat/job/ocs_QStatJobViewBase.cc
        qstat/job/ocs_QStatJobViewPlain.cc
        qstat/job/ocs_QStatJobViewXML.cc
        qstat/job/ocs_QStatJobController.cc

        qstat/ocs_QStatModelBase.cc
        qstat/ocs_QStatModelClient.cc
        qstat/ocs_QStatModelServer.cc
        qstat/ocs_QStatParameter.cc
        qstat/ocs_QStatParameterClient.cc
)

set(LIBRARY_INCLUDES "./" ${SGE_INCLUDES})
add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${LIBRARY_INCLUDES})

