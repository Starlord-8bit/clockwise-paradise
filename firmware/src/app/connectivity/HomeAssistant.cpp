#include "HomeAssistant.h"

#include <connectivity/CWMqtt.h>

void startHomeAssistantIntegration() {
  CWMqtt::getInstance()->begin();
}
