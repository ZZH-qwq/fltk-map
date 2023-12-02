#include "httplib.h"
#include <FL/Fl.H>
#include <FL/Enumerations.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Shared_Image.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Pack.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <FL/fl_draw.H>

#include <cmath>
#include <algorithm>
#include <tuple>
#include <thread>
#include <optional>
#include <variant>
#include <random>
#include <string>
#include <map>
#include <list>
#include <sstream>
#include <iostream>
#include <iomanip>

#define DEBUG true
#define NO_MAP false

#include "control.h"

bool poll_future_triggered = false;
void poll_future_handler(void*) {
    poll_future_triggered = false;
#if DEBUG
        std::cout << "Timeout retriggered" << std::endl;
#endif // DEBUG
    //control::m->draw();
}

int main() {
    Fl::visual(FL_DOUBLE | FL_RGB);
    fl_register_images();
    Fl::scheme("gtk+");

    control::win = new Fl_Double_Window(1280, 800, "Map");
    control::win->color(FL_LIGHT3);
    control::area_list = new control::Fl_Area_List(1020, 20, 255, 700);
    control::new_area_control = new control::Fl_New_Area_Control(1020, 590, 240, 190);
    control::m = new map::Fl_Map(0, 0, 1000, 800);
    control::areas = control::m->areas;
    control::new_area_control->link();
    control::new_area_control->take_focus();
    control::win->end();
    control::win->show();

    while (true) {
        auto [poll, updated] = control::m->poll_futures();
        if (control::m->redraw_flag || updated) {
            control::win->redraw();
            //control::m->draw();
            if (control::areas->temp) {
                control::new_area_control->update_size();
            }
        }
        if (poll) {
            if (!poll_future_triggered) {
                poll_future_triggered = true;
                Fl::add_timeout(0.2, poll_future_handler);
            }
        }
        auto nWin = Fl::wait();
        if (nWin == 0) {
            break;
        }
    }
    delete control::m;
    delete control::area_list;
    delete control::new_area_control;
    delete control::win;
    return 0;
}
