#include "EventBus.h"

EventBus* EventBus::instance() {
    static EventBus s_instance;
    return &s_instance;
}
