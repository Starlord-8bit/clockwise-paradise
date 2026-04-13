#include "HomeAssistant.h"

#include <CWMqtt.h>

void startHomeAssistantIntegration() {
  CWMqtt::getInstance()->begin();
}
