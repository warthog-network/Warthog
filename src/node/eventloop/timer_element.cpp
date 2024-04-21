#include "timer_element.hpp"
#include "eventloop/eventloop.hpp"
#include "global/globals.hpp"

TimerElement::~TimerElement(){
    if (key) 
        global().core->cancel_timer(*key);
}
