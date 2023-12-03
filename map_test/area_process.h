#pragma once
//
//  area_process.h
// 
//  Handles single polygon areas
//  Some interesting techniques were used to draw the inner regions of the polygons 
//

#include "polygon.h"

namespace area {

    // Class of polygonal areas on map
    class Area : public Polygon {
    protected:
        // Stored data [R,G,B,A] and size of the image to be drawn
        uchar* img_data;
        size_t data_size;
        Fl_Image* image;
        size_t display_w, display_h, img_w, img_h;

        // Stored color
        uchar cR, cG, cB, cA;
        // Anchor dropped when image is ready to reuse
        Vec2d anchor;

        bool display = true;
        std::string tag;

    public:
        // Fill area with its color
        void fill(double x1, double y1, double x2, double y2, bool resize = true, bool has_temp = false) {
            if (polygon.size() < 2 || polygon.size() < 3 && !has_temp) {
                return;
            }
            double dx = x2 - x1, dy = y2 - y1;
            if (resize) {
                // Reset the anchor
                anchor = { 0,0 };
            } else if (anchor.y > EPSILON) {
                // Have anchor - draw image in relative posision
                image->draw(static_cast<int>((anchor.x - x1) / dx * display_w), static_cast<int>((anchor.y - y1) / dy * display_h));
                return;
            }
            if (!has_temp && is_fit(dx, dy)) {
                generate_img(bbox1.x, bbox1.y, dx, dy, has_temp);
                image->draw(static_cast<int>((bbox1.x - x1) / dx * display_w), static_cast<int>((bbox1.y - y1) / dy * display_h));
#if DEBUG
                std::cout << "Anchor dropped" << std::endl;
#endif // DEBUG
                // Drop the anchor
                anchor = { bbox1.x,bbox1.y };
                return;
            }
            generate_img(x1, y1, dx, dy, has_temp);
            image->draw(0, 0);
        }

        // Trace the outline of the area
        void outline(double x, double y, double scale, bool has_temp = false) const {
            if (polygon.empty()) {
                return;
            }
            fl_color(cR, cG, cB);
            if (polygon.size() < 3 || !has_temp || has_temp && legal()) {
                fl_begin_line();
                fl_line_style(FL_SOLID, 3);
                for (auto& i : polygon) {
                    fl_vertex((i.x - x) * scale, (i.y - y) * scale);
                }
                fl_end_line();
                if (has_temp) {
                    fl_begin_line();
                    fl_line_style(FL_DASHDOT, 3);
                    fl_vertex((polygon.back().x - x) * scale, (polygon.back().y - y) * scale);
                    fl_vertex((temp_point.x - x) * scale, (temp_point.y - y) * scale);
                    fl_end_line();
                    fl_begin_line();
                    fl_line_style(FL_DOT, 2);
                    fl_vertex((temp_point.x - x) * scale, (temp_point.y - y) * scale);
                    fl_vertex((polygon.front().x - x) * scale, (polygon.front().y - y) * scale);
                    fl_end_line();
                }
            } else {
                fl_begin_loop();
                fl_line_style(FL_DOT, 3);
                for (auto& i : polygon) {
                    fl_vertex((i.x - x) * scale, (i.y - y) * scale);
                }
                fl_vertex((temp_point.x - x)* scale, (temp_point.y - y)* scale);
                fl_end_loop();
            }
        }

        // Show an indicator pointing at area's center
        void indicator(double cx, double cy, size_t w, size_t h) {
            fl_color(cR, cG, cB);
            const double phi = M_PI * 5 / 18;
            auto c = center();
            double distance = sphere::distance(cx, cy, c.x, c.y);
            double theta = sphere::initial_bearing(cx, cy, c.x, c.y);
            double wx = (abs(theta) > M_PI / 2 ? -0.45 : 0.45) * w, wy = wx * tan(theta);
            double hy = (theta > 0 ? 0.45 : -0.45) * h, hx = hy / tan(theta);
            double ix, iy;
            if (abs(hx) > abs(wx) || abs(hy) > abs(wy)) {
                ix = wx, iy = wy;
            } else {
                ix = hx, iy = hy;
            }
            double angle1 = theta + phi - M_PI, angle2 = theta - phi - M_PI;
            double lmax = std::min(w, h) / 12.0, lmin = std::min(w, h) / 30.0;
            double length = lmax * pow(1.05, -distance / 3) + lmin;
            fl_line_style(FL_SOLID, static_cast<int>(length / 25 + 2));
            double sx = ix + length * cos(angle1), sy = iy + length * sin(angle1);
            double ex = ix + length * cos(angle2), ey = iy + length * sin(angle2);
            auto a = w / 2.0 + ix, b = h / 2.0 + iy;
            fl_begin_line();
            fl_vertex(w / 2.0 + sx, h / 2.0 + sy);
            fl_vertex(w / 2.0 + ix, h / 2.0 + iy);
            fl_vertex(w / 2.0 + ex, h / 2.0 + ey);
            fl_end_line();
        }

    protected:
        void generate_img(double x1, double y1, double dx, double dy, bool has_temp = false) {
            std::fill(img_data, img_data + data_size, 0);
            for (size_t j = 0; j < img_h; j++) {
                double y = dy * j / img_h + y1;
                // Begin status of start point and the set of intersections along the ray
                auto [st, iset] = trace_ray(x1 + dx / display_w, y + dy / display_h, has_temp);
                if (iset.empty()) {
                    // No intersection
                    continue;
                }
                std::sort(iset.begin(), iset.end());
                // Starting at the first filled point
                size_t i = static_cast<size_t>(st ? 0 : std::max((iset[0] - x1) * img_w / dx, 0.0));
                // Refers to the index of filled period's end
                size_t idx = st ? 0 : 1;
                for (; idx <= iset.size() && i < img_w; idx += 2) {
                    size_t i_end = static_cast<size_t>(idx < iset.size() ? (iset[idx] - x1) * img_w / dx : img_w);
                    for (; i < i_end && i < img_w; i++) {
                        // Fill pixels in filled period
                        size_t target = (j * img_w + i) * 4;
                        img_data[target] = cR;
                        img_data[target + 1] = cG;
                        img_data[target + 2] = cB;
                        img_data[target + 3] = cA;
                    }
                    if (idx >= iset.size() - 1 || i >= img_w) {
                        break;
                    }
                    i = static_cast<size_t>((iset[idx + 1] - x1) * img_w / dx);
                }
            }
            delete image;
            Fl_RGB_Image img(img_data, static_cast<int>(img_w), static_cast<int>(img_h), 4);
            image = img.copy(static_cast<int>(display_w), static_cast<int>(display_h));
        }

    public:
        Area(size_t _w, size_t _h, uchar R, uchar G, uchar B, uchar A)  noexcept :display_w(_w), display_h(_h),
            img_w(_w / 3), img_h(_h / 3), cR(R), cG(G), cB(B), cA(A) {
            data_size = img_w * img_h * 4;
            img_data = new uchar[data_size];
            image = nullptr;
        }
        Area(Area&& other) noexcept : img_data(other.img_data), data_size(other.data_size), image(other.image),
            display_h(other.display_h), display_w(other.display_w), img_w(other.img_w), img_h(other.img_h),
            cR(other.cR), cG(other.cG), cB(other.cB), cA(other.cA), display(other.display), tag(other.tag),
            Polygon(std::forward<Polygon&&>(other)) {
            other.img_data = nullptr;
            other.image = nullptr;
        }
        ~Area() {
            delete[] img_data;
            delete image;
        }

        bool visible() const { return display; }
        void flip_visible() {
            display = !display;
            reset_anchor();
        }

        Fl_Color color() const { return cR << 24 | cG << 16 | cB << 8; }
        void color(uchar R, uchar G, uchar B, uchar A) {
            cR = R, cG = G, cB = B, cA = A;
        }

        void reset_anchor() { anchor = { 0,0 }; }

        std::string name() const { return tag; }
        void set_name(std::string n) { tag = n; }
    };
} // namespace area