#pragma once
//
//  area_display.h
// 
//  Manage multiple areas and use FLTK to display them
//

#include "map_process.h"
#include "area_process.h"

namespace area {
    
    class Fl_Area : public map::Map, public Fl_Box {
	public:
        std::list<Area> areas;
        bool fill_areas = true;
        Area* temp;

        Fl_Area(int u, int v, int w, int h) :
            Fl_Box(u, v, w, h), Map(w, h, 1, 15) {
            temp = new Area(w, h, 255, 0, 0, 32);
            temp->set_name("SJTU");
            temp->push(0.83735, 0.409238);
            temp->push(0.837286, 0.409262);
            temp->push(0.837299, 0.409298);
            temp->push(0.837364, 0.409274);
		}

		~Fl_Area() {
            delete temp;
		}

        void draw_area(Area* a, double x1, double y1, bool resize = true, bool has_temp = false, bool fill = true) {
            if (!a->visible()) {
                return;
            }
            if (a->is_clipped(lng, lat, x1, y1) && !has_temp) {
                if (a->points_count()) {
                    auto [cx, cy] = cursor_mercator(Map::w / 2, Map::h / 2);
                    a->indicator(cx, cy, Map::w, Map::h);
                }
                if (resize) {
                    a->reset_anchor();
                }
                return;
            }
            if (fill_areas && fill) {
                a->fill(lng, lat, x1, y1, resize, has_temp);
            } else if (resize) {
                a->reset_anchor();
            }
            a->outline(lng, lat, pixels_per_side, has_temp);
        }

		void draw_areas(bool resize = true) {
            auto [x1, y1] = cursor_mercator(Map::w, Map::h);
            if (temp) {
                draw_area(temp, x1, y1, true, true);
                for (auto& a : areas) {
                    draw_area(&a, x1, y1, resize, false, false);
                }
            } else {
                for (auto& a : areas) {
                    draw_area(&a, x1, y1, resize);
                }
            }
		}

        void draw() { draw_areas(); }

        bool finish() {
            bool ret = false;
            if (temp->points_count() > 2 && temp->legal()) {
                temp->finish();
                areas.push_back(std::move(*temp));
                ret = true;
            }
            delete temp;
            temp = nullptr;
            return ret;
        }

        std::vector<std::pair<Fl_Color, std::string>> get_info() {
            std::vector<std::pair<Fl_Color, std::string>> ret;
            for (auto& a : areas) {
                ret.push_back(std::pair(a.color(), a.name()));
            }
            return ret;
        }
	};
} // namespace area