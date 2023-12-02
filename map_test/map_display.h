#pragma once
//
//  map_display.h
// 
//  Higher level control of the map
//  Use offscreen offscreen graphics buffer to refresh the screen
//  Map tilts are saved and managed with a double-layered cache
//

#include "map_process.h"
#include "area_display.h"

namespace map {

    class Fl_Map : public Map, public Fl_Group {
        // Number of colums and rows of tilt images
        int cols, rows;
        // Images buffer for posision-only changes
        std::map<tilts::TiltId, Fl_Image*> redraw_buffer;
        std::list<tilts::TiltId> redraw_list;
        size_t max_cache_size;
        // Source of tilts and another buffer
        tilts::TiltsSource src;

        Fl_Offscreen oscr;
        int mouse_x = 0, mouse_y = 0;
        bool dragging = false;

    public:
        area::Fl_Area* areas;
        bool redraw_flag = 0, resize_flag = 0;

        // Offscreen drawing function for displaying map and areas
        void draw_map(bool resize = true) {
            redraw_flag = false;
            resize_flag = false;
#if NO_MAP
            fl_rectf(0, 0, Map::w, Map::h, fl_rgb_color(252, 249, 242));
            areas->sync_with(*this);
            areas->draw_areas(resize);
            return;
#endif // NO_MAP

            // Index for top left tilt
            auto tilt0 = mercator_to_tilt_id(lng, lat, z);

            // Screen coordinate for top left corner of top left tilt
            double xz = lng * tilts_per_side, yz = lat * tilts_per_side;
            int pixels_per_tilt = static_cast<int>(tilts::TILT_SIZE * k);
            int x0 = static_cast<int>((int(xz) - xz) * pixels_per_tilt);
            int y0 = static_cast<int>((int(yz) - yz) * pixels_per_tilt);

            // Displaying tilts
            for (int i = 0; i < rows; i++) {
                for (int j = 0; j < cols; j++) {
                    // Index for tilt[i, j]
                    auto ti = tilt0.offset(i, j);
                    // Index correction
                    if (ti.x >= static_cast<int>(tilts_per_side)) {
                        ti.x -= static_cast<int>(tilts_per_side);
                    } else if (ti.x < 0) {
                        ti.x += static_cast<int>(tilts_per_side);
                    }
                    // Resized tilt image
                    Fl_Image* pngResized;
                    if (redraw_buffer.find(ti) != redraw_buffer.end()) {
                        // Cache hit
                        pngResized = redraw_buffer[ti];
                    } else {
                        // Get tilt from source
                        auto td = src.get(ti);
                        if (td.size == 0) {
                            // Not successfully requested yet
                            fl_rectf(x0 + i * pixels_per_tilt, y0 + j * pixels_per_tilt,
                                pixels_per_tilt, pixels_per_tilt, fl_rgb_color(252, 249, 242));
                            continue;
                        }
                        auto png = Fl_PNG_Image(nullptr, td.buf, td.size);
                        pngResized = png.copy(pixels_per_tilt, pixels_per_tilt);
                        // Storing resized image into buffer
                        redraw_list.push_back(ti);
                        redraw_buffer[ti] = pngResized;
                    }
                    pngResized->draw(x0 + i * pixels_per_tilt, y0 + j * pixels_per_tilt);
                    /*if (DEBUG) {
                        std::cout << "Putting tilt " << ti << " at ("
                            << x0 + i * pixels_per_tilt << ", "
                            << y0 + j * pixels_per_tilt << ")" << std::endl;
                    }*/
                }
            }
            // Synchronising coordinate and zoom factors
            areas->sync_with(*this);
            areas->draw_areas(resize);
        }

        void draw_normal() {
            // Clear up buffer if exceeds the limit
            while (redraw_list.size() > max_cache_size) {
                delete redraw_buffer[redraw_list.front()];
                redraw_buffer.erase(redraw_list.front());
                redraw_list.pop_front();
            }

            fl_begin_offscreen(oscr);
            draw_map(false);
            fl_end_offscreen();
            fl_copy_offscreen(0, 0, Map::w, Map::h, oscr, 0, 0);
        }

        void draw_resize(bool disable_offscreen = false) {
            // Recreate buffer
            for (auto& p : redraw_buffer) {
                delete p.second;
            }
            redraw_buffer.clear();
            redraw_list.clear();
#if DEBUG
            std::cout << "Resized! Cache Cleared." << std::endl;
#endif // DEBUG

            if (disable_offscreen) {
                draw_map();
                return;
            }
            fl_begin_offscreen(oscr);
            draw_map();
            fl_end_offscreen();
            fl_copy_offscreen(0, 0, Map::w, Map::h, oscr, 0, 0);
        }

        void draw() override {
            if (resize_flag) {
                draw_resize();
            } else {
                draw_normal();
            }
        }

        void drag_screen_by(int dx, int dy) {
            translate(dx, dy);
            redraw_flag = true;
#if DEBUG
            std::cout << "Dragged by dx = " << dx << ", dy = " << dy
                << ", now lng = " << lng << ", lat = " << lat << std::endl;
#endif // DEBUG
        }

        void scroll_by(int dy, int mx, int my) {
            // Scaling speed limit, or the program will crash :( 
            if (dy > 3) {
                dy = 3;
            } else if (dy < -3) {
                dy = -3;
            }
            auto z1 = z;

            if(scale(mx, my, powl(1.05, -dy))){
#if DEBUG
                std::cout << "Scrolled by dy = " << dy
                    << ", now z = " << z << ", k = " << k
                    << std::endl;
#endif // DEBUG
                //draw_resize(z1 != z);
                redraw_flag = true;
                resize_flag = true;
            }
        }

        int handle(int event) override {
            switch (event) {
            case FL_ENTER: {
                return 1;
            }
            case FL_MOVE: {
                if (areas->temp) {
                    auto [x, y] = cursor_mercator(Fl::event_x(), Fl::event_y());
                    areas->temp->set_temp(x, y);
                    redraw_flag = true;
                }
                return 1;
            }
            case FL_LEAVE: {
                if (areas->temp) {
                    areas->temp->reset_temp();
                    redraw_flag = true;
                }
                return 1;
            }
            case FL_FOCUS: {
                return 1;
            }
            case FL_PUSH: {
                mouse_x = Fl::event_x();
                mouse_y = Fl::event_y();
#if DEBUG
                auto [x, y] = cursor_mercator(mouse_x, mouse_y);
                std::cout << "Crusor x = " << x << ", y = " << y << std::endl;
#endif // DEBUG
                return 1;
            }
            case FL_DRAG: {
                dragging = true;
                int dx = Fl::event_x() - mouse_x;
                int dy = Fl::event_y() - mouse_y;
                mouse_x = Fl::event_x();
                mouse_y = Fl::event_y();
                drag_screen_by(dx, dy);
                return 1;
            }
            case FL_RELEASE: {
                if (Fl::event_is_click() && areas->temp && areas->temp->legal()) {
                    areas->temp->confirm_temp();
                }
                dragging = false;
                return 1;
            }
            case FL_MOUSEWHEEL:
                scroll_by(Fl::event_dy(), Fl::event_x(), Fl::event_y());
            }
            return Fl_Widget::handle(event);
        }

        Fl_Map(int u, int v, size_t w, size_t h)
            : Fl_Group(u, v, w, h), Map(w, h, 1, 15), rows(int(w / tilts::TILT_SIZE) + 2),
            cols(int(h / tilts::TILT_SIZE) + 2), src(cols* rows * 30),
            max_cache_size(cols* rows * 5) {
#if DEBUG
            std::cout << "Initializing map with rows = " << rows
                << ", cols = " << cols << std::endl;
#endif // DEBUG

            areas = new area::Fl_Area(0, 0, w, h);
            oscr = fl_create_offscreen(w, h);
        }

        ~Fl_Map() {
            for (auto& p : redraw_buffer) {
                delete p.second;
            }
            fl_delete_offscreen(oscr);
            delete areas;
        }

        std::tuple<bool, bool> poll_futures() { return src.pollFutures(); }
    };
} // namespace map