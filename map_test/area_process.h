#pragma once
//
//  area_process.h
// 
//  Handles single polygon areas
//  Some interesting techniques were used to draw the inner regions of the polygons 
//

#include "polygon.h"
#include "spherical.h"

namespace area {

    // Class of polygonal areas on map
    class Area {
    protected:
        // Stored data [R,G,B,A] and size of the image to be drawn
        uchar* img_data;
        size_t data_size;
        Fl_Image* image;
        size_t display_w, display_h, img_w, img_h;

        // Points of the border
        std::vector<Vec2d> polygon;
        // Bounding box
        Vec2d bbox1, bbox2;
        // Stored color
        uchar cR, cG, cB, cA;
        // Anchor dropped when image is ready to reuse
        Vec2d anchor;
        bool display = true;

        long double area_size = 0;
        Vec2d temp_point;
        std::string tag;

    public:
        void push(double x, double y) {
            if (polygon.empty()) {
                // Initialization
                bbox1 = { x,y };
                bbox2 = { x,y };
                temp_point = { x,y };
            } else {
                // Update bounding box
                bbox1 = { std::min(bbox1.x, x), std::min(bbox1.y, y) };
                bbox2 = { std::max(bbox2.x, x), std::max(bbox2.y, y) };
            }
            // Push back to point set
            polygon.push_back({ x,y });

            if (polygon.size() > 2) {
                // Update area's size
                auto& a = polygon.front(), & b = *(polygon.rbegin() + 1), & c = *polygon.rbegin();
                auto [xa, ya] = map::Map::mercator_to_sphere(a.x, a.y);
                auto [xb, yb] = map::Map::mercator_to_sphere(b.x, b.y);
                auto [xc, yc] = map::Map::mercator_to_sphere(c.x, c.y);
                area_size += sphere::spherical_triangle(ya, xa, yb, xb, yc, xc);
#if DEBUG
                std::cout << "Size added : " << sphere::spherical_triangle(ya, xa, yb, xb, yc, xc) << " now : " << area_size << std::endl;
#endif // DEBUG
            }
        }

        // Return if area's bounding box is out of screen
        bool is_clipped(double x1, double y1, double x2, double y2) const {
            return (x1 > bbox2.x || y1 > bbox2.y || x2 < bbox1.x || y2 < bbox1.y);
        }
        // Return if area's bounding box is smaller than the screen
        bool is_fit(double dx, double dy) const {
            return (dx > bbox2.x - bbox1.x && dy > bbox2.y - bbox1.y);
        }

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
        // Determine if the point is inside the polygon, then give the set of intersections of the ray with the polygon
        std::tuple<bool, std::vector<double>> trace_ray(double x, double y, bool has_temp = false) const {
            std::vector<double> intersects;
            Vec2d ray_start(x - 1e-15, y - 1e-15);

            // Iterate over each side of the polygon
            for (size_t i = 0; i < polygon.size() - 1; i++) {
                auto& c1 = polygon[i];
                auto& c2 = polygon[i + 1];

                if (auto c = ray_intersect(c1, c2, ray_start); c) {
                    intersects.push_back(c.value());
                }
            }
            // Is an area under construction
            if (has_temp) {
                auto& c1 = polygon.back();
                auto& c2 = polygon.front();
                if (auto c = ray_intersect(c1, temp_point, ray_start); c) {
                    intersects.push_back(c.value());
                }
                if (auto c = ray_intersect(temp_point, c2, ray_start); c) {
                    intersects.push_back(c.value());
                }
            }

            return { intersects.size() % 2, intersects };
        }

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
            polygon(std::forward<std::vector<Vec2d>&&>(other.polygon)), bbox1(other.bbox1), bbox2(other.bbox2),
            cR(other.cR), cG(other.cG), cB(other.cB), cA(other.cA), display(other.display),
            temp_point(other.temp_point), area_size(other.area_size), tag(other.tag) {
            other.img_data = nullptr;
            other.image = nullptr;
        }
        ~Area() {
            delete[] img_data;
            delete image;
        }

        Vec2d center() const { return Vec2d((bbox1.x + bbox2.x) / 2, (bbox1.y + bbox2.y) / 2); }

        bool visible() const { return display; }
        void flip_visible() {
            display = !display;
            reset_anchor();
        }

        Fl_Color color() const { return cR << 24 | cG << 16 | cB << 8; }
        void color(uchar R, uchar G, uchar B, uchar A) {
            cR = R, cG = G, cB = B, cA = A;
        }

        void set_temp(double x, double y) { temp_point.x = x, temp_point.y = y; }
        void reset_temp() { 
            if (!polygon.empty()) {
                temp_point = polygon.front();
            }
        }

        double size() const { return abs(area_size); }
        double temp_size() const {
            if (polygon.size() < 2) {
                return 0;
            }
            auto& a = polygon.front(), & b = *polygon.rbegin();
            auto [xa, ya] = map::Map::mercator_to_sphere(a.x, a.y);
            auto [xb, yb] = map::Map::mercator_to_sphere(b.x, b.y);
            auto [xc, yc] = map::Map::mercator_to_sphere(temp_point.x, temp_point.y);
            /*if (DEBUG) {
                std::cout << "Temp size : " << area_size + sphere::spherical_triangle(ya, xa, yb, xb, yc, xc) << std::endl;
            }*/
            return abs(area_size + sphere::spherical_triangle(ya, xa, yb, xb, yc, xc));
        }
        // Recalculate area size and bounding box, only call before polygon finished
        void recalculate() {
            area_size = 0;
            if (polygon.empty()) {
                return;
            }
            bbox1 = { polygon[0].x, polygon[0].y };
            bbox2 = { polygon[0].x, polygon[0].y };
            for (size_t i = 1; i < polygon.size(); i++) {
                bbox1 = { std::min(bbox1.x, polygon[i].x), std::min(bbox1.y, polygon[i].y) };
                bbox2 = { std::max(bbox2.x, polygon[i].x), std::max(bbox2.y, polygon[i].y) };
            }
            if (polygon.size() < 3) {
                return;
            }
            auto [p0x, p0y] = map::Map::mercator_to_sphere(polygon[0].x, polygon[0].y);
            auto [p1x, p1y] = map::Map::mercator_to_sphere(polygon[1].x, polygon[1].y);
            for (size_t i = 2; i < polygon.size(); i++) {
                auto [p2x, p2y] = map::Map::mercator_to_sphere(polygon[i].x, polygon[i].y);
                area_size += sphere::spherical_triangle(p0y, p0x, p1y, p1x, p2y, p2x);
                p1x = p2x, p1y = p2y;
            }
        }

        size_t points_count() const { return polygon.size(); }
        void reset_anchor() { anchor = { 0,0 }; }

        // Check if area after adding temp_point is legal 
        bool legal() const {
            if (polygon.size() < 3) {
                return true;
            }
            for (size_t i = 0; i < polygon.size() - 2; i++) {
                auto& c1 = polygon[i];
                auto& c2 = polygon[i + 1];
                if (is_intersect(c1, c2, polygon.back(), temp_point)) {
                    return false;
                }
            }
            return true;
        }

        // Check if area size is calculable after adding temp_point
        bool size_legal() const {
            if (polygon.size() < 3) {
                return true;
            }
            if (!legal()) {
                return false;
            }
            for (size_t i = 1; i < polygon.size() - 1; i++) {
                auto& c1 = polygon[i];
                auto& c2 = polygon[i + 1];
                if (is_intersect(c1, c2, polygon.front(), temp_point)) {
                    return false;
                }
            }
            return true;
        }

        void confirm_temp() { push(temp_point.x, temp_point.y); }
        void finish() {
            if (polygon.size() > 2) {
                polygon.push_back(polygon.front());
            }
        }
        void undo_temp() {
            if (!polygon.empty()) {
                polygon.pop_back();
                recalculate();
            }
        }

        std::string name() const { return tag; }
        void set_name(std::string n) { tag = n; }
    };
} // namespace area