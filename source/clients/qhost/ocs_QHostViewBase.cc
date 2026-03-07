//
// Created by ebablick on 03.03.26.
//

#include "ocs_QHostViewBase.h"

ocs::QHostViewBase::QHostViewBase(const QHostParameter &parameter) {
   full_listing_ = parameter.get_show();
}
