#pragma once
//
//  polygon.h
// 
//  The base class for a polygon.
//  Provides tools to determine the relationship of points, rays and line segments
//

#include "spherical.h"

namespace area {
#define EPSILON 1e-16

    // 2D vec of double
    struct Vec2d {
        double x, y;
        Vec2d() { x = 0.0, y = 0.0; }
        Vec2d(double dx, double dy) { x = dx, y = dy; }
        void Set(double dx, double dy) { x = dx, y = dy; }
    };

    class Polygon {
    protected:
        // Points of the border
        std::vector<Vec2d> polygon;
        // Bounding box
        Vec2d bbox1, bbox2;

        long double area_size = 0;
        Vec2d temp_point;

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
        
        // Determine if a point is on a line segment
        static bool is_point_on_line(const Vec2d q, const Vec2d p1, const Vec2d p2) {
            double d1 = (p1.x - q.x) * (p2.y - q.y) - (p2.x - q.x) * (p1.y - q.y);
            return ((abs(d1) < EPSILON) && ((q.x - p1.x) * (q.x - p2.x) <= 0) && ((q.y - p1.y) * (q.y - p2.y) <= 0));
        }

        // Determine whether a line segments and a x-aligned ray intersect
        // Assert the ray, start at q1, is parallel to the x-axis and towards the direction of x+
        // If so, give out the point of intersection
        static std::optional<double> ray_intersect(const Vec2d& p1, const Vec2d& p2, const Vec2d& q1) {
            if (std::max(p1.x, p2.x) < q1.x || std::max(p1.y, p2.y) < q1.y || q1.y < std::min(p1.y, p2.y)) {
                return std::nullopt;
            }
            if (is_point_on_line(q1, p1, p2) || abs(p2.y - p1.y) < EPSILON) {
                return std::nullopt;
            }
            // Only the upper endpoint counts
            if (abs(p1.y - q1.y) <= EPSILON) {
                if (p1.x > q1.x && p1.y > p2.y) {
                    return p1.x;
                } else {
                    return std::nullopt;
                }
            }
            if (abs(p2.y - q1.y) <= EPSILON) {
                if (p2.x > q1.x && p2.y > p1.y) {
                    return p2.x;
                } else {
                    return std::nullopt;
                }
            }

            double p1q = p1.y - q1.y;
            double qp2 = q1.y - p2.y;
            double x = p1.x + (p2.x - p1.x) * (p1q / (p1q + qp2));
            if (x < q1.x) {
                return std::nullopt;
            } else {
                return x;
            }
        }

        // Determine whether two line segments intersect
        static bool is_intersect(const Vec2d& p1, const Vec2d& p2, const Vec2d& q1, const Vec2d& q2) {
            if (std::max(p1.x, p2.x) < std::min(q1.x, q2.x) || std::max(p1.y, p2.y) < std::min(q1.y, q2.y) ||
                std::max(q1.x, q2.x) < std::min(p1.x, p2.x) || std::max(q1.y, q2.y) < std::min(p1.y, p2.y)) {
                return false;
            }
            if (((q1.x - p1.x) * (p2.y - p1.y) - (p2.x - p1.x) * (q1.y - p1.y)) *
                ((q2.x - p1.x) * (p2.y - p1.y) - (p2.x - p1.x) * (q2.y - p1.y)) >= 0 ||
                ((p1.x - q1.x) * (q2.y - q1.y) - (q2.x - q1.x) * (p1.y - q1.y)) *
                ((p2.x - q1.x) * (q2.y - q1.y) - (q2.x - q1.x) * (p2.y - q1.y)) >= 0) {
                return false;
            }
            return true;
        }

    public:
        Polygon() = default;
        Polygon(Polygon&& other) noexcept : polygon(std::forward<std::vector<Vec2d>&&>(other.polygon)),
            bbox1(other.bbox1), bbox2(other.bbox2), temp_point(other.temp_point), area_size(other.area_size) {}

        Vec2d center() const { return Vec2d((bbox1.x + bbox2.x) / 2, (bbox1.y + bbox2.y) / 2); }

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

        static uchar hsl_val(double n1, double n2, double hue) {
            if (hue > 360) {
                hue -= 360;
            } else if (hue < 0) {
                hue += 360;
            }
            if (hue < 60) {
                return static_cast<uchar>((n1 + (n2 - n1) * hue / 60) * 225);
            } else if (hue < 180) {
                return static_cast<uchar>(n2 * 255);
            } else if (hue < 240) {
                return static_cast<uchar>((n1 + (n2 - n1) * (240 - hue) / 60) * 255);
            } else {
                return static_cast<uchar>(n1 * 255);
            }
        }

        static std::tuple<uchar, uchar, uchar> hsl_to_rgb(double h, double s, double l) {
            double cmax = l <= 0.5 ? l * (1 + s) : l * (1 - s) + s, cmin = 2 * l - cmax;
            return { hsl_val(cmin,cmax,h + 120) ,hsl_val(cmin,cmax,h) ,hsl_val(cmin,cmax,h - 120) };
        }

        static std::tuple<uchar, uchar, uchar> color_generator() {
            static std::default_random_engine e;
            static int hue = 0;
            std::uniform_real_distribution<double> h(180, 300), l(0.45, 0.75), s(0.75, 1);
            hue = int(hue + h(e)) % 360;
            return hsl_to_rgb(hue, s(e), l(e));
        }
    };
}